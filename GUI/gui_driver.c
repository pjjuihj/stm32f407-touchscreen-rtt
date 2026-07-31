/**
 * @file gui_driver.c
 * @brief LVGL驱动适配层实现
 */

#include "gui_driver.h"
#include "lcd.h"
#include "touch.h"
#include "xpt2046.h"
#include "usart.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

/*===========================================================================
 * 环形缓冲区结构
 *===========================================================================*/

/**
 * @brief 通用环形缓冲区结构
 */
typedef struct {
    volatile uint16_t head;     // 写入位置
    volatile uint16_t tail;     // 读取位置
    volatile uint16_t count;    // 当前数据量
    uint16_t size;              // 缓冲区总大小
    char *buf;                  // 数据存储区
} ring_buf_t;

/**
 * @brief 写入单个字符到环形缓冲区（中断安全）
 */
static void ring_buf_put(ring_buf_t *ring, char c)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(ring->count < ring->size) {
        ring->buf[ring->head] = c;
        ring->head = (ring->head + 1) % ring->size;
        ring->count++;
    }
    // 缓冲区满时丢弃字符

    __set_PRIMASK(primask);
}

/**
 * @brief 从环形缓冲区读取单个字符（中断安全）
 * @return 读取的字符，缓冲区空返回 -1
 */
static int ring_buf_get(ring_buf_t *ring)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if(ring->count == 0) {
        __set_PRIMASK(primask);
        return -1;
    }

    char c = ring->buf[ring->tail];
    ring->tail = (ring->tail + 1) % ring->size;
    ring->count--;

    __set_PRIMASK(primask);
    return c;
}

/**
 * @brief 获取环形缓冲区中的数据量
 */
static uint16_t ring_buf_count(ring_buf_t *ring)
{
    return ring->count;
}

/*===========================================================================
 * DMA 调度器
 *===========================================================================*/

/**
 * @brief DMA 发送状态
 */
typedef enum {
    DMA_IDLE = 0,               // 空闲，可启动新传输
    DMA_TX_LVGL,                // 正在发送 LVGL 日志
    DMA_TX_CUSTOM,              // 正在发送自定义日志
} dma_state_t;

/**
 * @brief DMA 调度器上下文
 */
typedef struct {
    dma_state_t state;          // 当前状态
    uint8_t tx_buf[256];        // DMA 发送缓冲区
    uint16_t tx_len;            // 当前发送长度
} dma_scheduler_t;

// LVGL 日志缓冲区
static char lvgl_log_buf[1024];
static ring_buf_t lvgl_ring = {
    .head = 0, .tail = 0, .count = 0,
    .size = 1024, .buf = lvgl_log_buf
};

// 自定义日志缓冲区
static char custom_log_buf[1024];
static ring_buf_t custom_ring = {
    .head = 0, .tail = 0, .count = 0,
    .size = 1024, .buf = custom_log_buf
};

// DMA 调度器
static dma_scheduler_t dma_scheduler = {
    .state = DMA_IDLE,
    .tx_len = 0
};

/**
 * @brief 从环形缓冲区填充 DMA 发送缓冲区
 * @return 实际填充的字节数
 */
static uint16_t fill_dma_buf(ring_buf_t *ring, uint16_t max_len)
{
    uint16_t len = 0;
    int c;

    while(len < max_len && (c = ring_buf_get(ring)) != -1) {
        dma_scheduler.tx_buf[len++] = (uint8_t)c;
    }

    return len;
}

/**
 * @brief DMA 调度器 - 主循环调用
 */
static void dma_scheduler_run(void)
{
    // 如果 DMA 正忙，直接返回
    if(dma_scheduler.state != DMA_IDLE) return;

    // 优先检查 LVGL 缓冲区
    if(ring_buf_count(&lvgl_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&lvgl_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_LVGL;
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }

    // LVGL 无数据，检查自定义缓冲区
    if(ring_buf_count(&custom_ring) > 0) {
        dma_scheduler.tx_len = fill_dma_buf(&custom_ring, sizeof(dma_scheduler.tx_buf));
        dma_scheduler.state = DMA_TX_CUSTOM;
        HAL_UART_Transmit_DMA(&huart1, dma_scheduler.tx_buf, dma_scheduler.tx_len);
        return;
    }
}

/*===========================================================================
 * 显示驱动配置
 *===========================================================================*/

/* LCD分辨率 */
#define MY_DISP_HOR_RES  240
#define MY_DISP_VER_RES  320

/* 缓冲区行数 */
#define BUF_LINES        10

/* 双缓冲区 */
static lv_color_t buf1[MY_DISP_HOR_RES * BUF_LINES];
static lv_color_t buf2[MY_DISP_HOR_RES * BUF_LINES];

/**
 * @brief LVGL刷新回调函数
 */
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t size = width * height;

    /* 设置显示窗口 */
    LCD_Open_Window(area->x1, area->y1, width, height);
    LCD_WriteGRAM();

    /* 发送颜色数据 - 简化版本 */
    uint16_t *color_p16 = (uint16_t *)px_map;
    uint32_t i;
    for(i = 0; i < size; i++) {
        LCD_DATA = color_p16[i];
    }

    /* 通知LVGL刷新完成 */
    lv_display_flush_ready(disp);
}

/**
 * @brief 初始化显示驱动
 */
void gui_disp_init(void)
{
    /* 创建显示设备 */
    lv_display_t *disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

    if(disp == NULL) {
        return;
    }

    /* 设置刷新回调 */
    lv_display_set_flush_cb(disp, disp_flush_cb);

    /* 设置缓冲区 */
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/*===========================================================================
 * 触摸驱动配置
 *===========================================================================*/

/**
 * @brief 触摸读取回调
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0, last_y = 0;

    /* 扫描触摸 */
    XPT2046_Scan(0);

    /* 检查触摸状态 */
    if(Xdown != 0xffff && Ydown != 0xffff) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = Xdown;
        data->point.y = Ydown;
        last_x = data->point.x;
        last_y = data->point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = last_x;
        data->point.y = last_y;
    }
}

/**
 * @brief 初始化触摸驱动
 */
void gui_touch_init(void)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    /* 设置触摸关联的显示设备 */
    lv_display_t *disp = lv_display_get_default();
    if(disp != NULL) {
        lv_indev_set_display(indev, disp);
    }
}

/*===========================================================================
 * 时钟驱动配置
 *===========================================================================*/

/**
 * @brief 官方时钟回调函数
 */
static uint32_t gui_tick_get(void)
{
    return HAL_GetTick();
}

/**
 * @brief 初始化时钟驱动
 */
void gui_tick_init(void)
{
    lv_tick_set_cb(gui_tick_get);
}

/*===========================================================================
 * LVGL 日志回调
 *===========================================================================*/

/**
 * @brief LVGL 9.5 日志回调 - 仅写入 LVGL 缓冲区
 *
 * 使用环形缓冲区代替直接调用 USART1_SendString，
 * 避免 LVGL 在临界区/中断上下文中调用时导致死锁或 HardFault。
 * 缓冲区数据由 DMA 调度器在主循环中异步发送。
 */
static void lv_log_print_g_cb(lv_log_level_t level, const char *buf)
{
    // 递归保护
    static volatile bool in_log_cb = false;
    if(in_log_cb) return;
    in_log_cb = true;

    // 日志级别前缀
    const char *prefix;
    switch(level) {
        case LV_LOG_LEVEL_TRACE: prefix = "[T] "; break;
        case LV_LOG_LEVEL_INFO:  prefix = "[I] "; break;
        case LV_LOG_LEVEL_WARN:  prefix = "[W] "; break;
        case LV_LOG_LEVEL_ERROR: prefix = "[E] "; break;
        case LV_LOG_LEVEL_USER:  prefix = "[U] "; break;
        default:                 prefix = "[?] "; break;
    }

    // 写入前缀
    for(const char *p = prefix; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }

    // 写入日志内容
    for(const char *p = buf; *p; p++) {
        ring_buf_put(&lvgl_ring, *p);
    }

    // 换行符
    ring_buf_put(&lvgl_ring, '\r');
    ring_buf_put(&lvgl_ring, '\n');

    in_log_cb = false;
}

/*===========================================================================
 * 公共接口
 *===========================================================================*/

/**
 * @brief 初始化日志系统（必须在 lv_init 之前调用）
 */
void gui_log_init(void)
{
    // 清空 LVGL 环形缓冲区（仅重置操作字段，保留 size 和 buf）
    lvgl_ring.head = 0;
    lvgl_ring.tail = 0;
    lvgl_ring.count = 0;

    // 清空自定义环形缓冲区
    custom_ring.head = 0;
    custom_ring.tail = 0;
    custom_ring.count = 0;

    // 清空 DMA 调度器
    memset(&dma_scheduler, 0, sizeof(dma_scheduler));

    // 注册 LVGL 日志回调
#if LV_USE_LOG
    lv_log_register_print_cb(lv_log_print_g_cb);
#endif
}

/**
 * @brief 主循环调用 - 调度 DMA 发送
 */
void gui_log_flush(void)
{
    dma_scheduler_run();
}

/**
 * @brief 写入自定义日志
 */
void gui_log_write(const char *str)
{
    if(!str) return;
    for(const char *p = str; *p; p++) {
        ring_buf_put(&custom_ring, *p);
    }
}

/*===========================================================================
 * DMA 回调
 *===========================================================================*/

/**
 * @brief UART DMA 发送完成回调
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;

    dma_scheduler.state = DMA_IDLE;
    dma_scheduler.tx_len = 0;

    // 立即检查是否有更多数据要发送
    dma_scheduler_run();
}

/**
 * @brief UART 错误回调
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;

    // 清除错误标志
    __HAL_UART_CLEAR_PEFLAG(huart);

    // 重置 DMA 状态
    dma_scheduler.state = DMA_IDLE;
    dma_scheduler.tx_len = 0;
}
