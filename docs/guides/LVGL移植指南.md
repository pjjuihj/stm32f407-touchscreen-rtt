# LVGL v9.5.0 移植指南 (STM32F407ZG + ILI9341)

## 重要提示

**基于LVGL v9.5.0官方源码，使用官方ILI9341驱动！**

---

## 目录

1. [硬件信息](#1-硬件信息)
2. [LVGL v9.x API变化](#2-lvgl-v9x-api变化)
3. [官方驱动说明](#3-官方驱动说明)
4. [文件结构](#4-文件结构)
5. [移植步骤](#5-移植步骤)
6. [驱动适配](#6-驱动适配)
7. [编译配置](#7-编译配置)
8. [测试验证](#8-测试验证)
9. [常见问题](#9-常见问题)

---

## 1. 硬件信息

### 1.1 MCU规格

| 参数 | 规格 |
|------|------|
| **型号** | STM32F407ZGT6 |
| **内核** | ARM Cortex-M4 |
| **主频** | 168MHz |
| **Flash** | 1MB |
| **SRAM** | 192KB (128KB + 64KB CCM) |

### 1.2 LCD显示模块

| 参数 | 规格 |
|------|------|
| **驱动芯片** | ILI9341 |
| **分辨率** | 240 × 320 像素 |
| **颜色深度** | 16位 RGB565 |
| **接口** | 8080并口 (FSMC控制) |
| **背光** | PF10 (GPIO控制) |

### 1.3 LCD引脚连接 (FSMC Bank4)

| LCD引脚 | STM32引脚 | 功能 |
|---------|-----------|------|
| D0~D15 | PD14,PD15,PD0,PD1,PE7~PE15,PD8~PD10 | 16位数据总线 |
| CS | PG12 | 片选 (FSMC_NE4) |
| RS/DC | PF12 | 数据/命令选择 |
| WR | PD5 | 写使能 (FSMC_NWE) |
| RD | PD4 | 读使能 (FSMC_NOE) |
| BL | PF10 | 背光控制 |

### 1.4 触摸屏模块

| 参数 | 规格 |
|------|------|
| **触摸芯片** | XPT2046 |
| **类型** | 电阻触摸屏 |
| **接口** | SPI (软件模拟) |
| **分辨率** | 4096 × 4096 (12位ADC) |

### 1.5 触摸校准参数

```c
// 已在 USER/TOUCH/xpt2046.c 中定义
extern float xFactor;   // 0.06671114
extern float yFactor;   // 0.09117551
extern short xOffset;   // -11
extern short yOffset;   // -18
```

---

## 2. LVGL v9.x API变化

**重要：LVGL v9.x 相比 v8.x 有重大API变化！**

### 2.1 显示驱动API变化

| v8.x | v9.x |
|------|------|
| `lv_disp_drv_t` 结构体 | `lv_display_t *` 指针 |
| `lv_disp_drv_init(&drv)` | `lv_display_create(hor, ver)` |
| `lv_disp_drv_register(&drv)` | `lv_display_set_flush_cb(disp, cb)` |
| `lv_disp_buf_init()` | `lv_display_set_buffers()` |
| `lv_disp_flush_ready(disp)` | `lv_display_flush_ready(disp)` |

**v9.x 显示驱动创建流程:**

```c
// 1. 创建显示设备
lv_display_t *disp = lv_display_create(240, 320);

// 2. 设置刷新回调
lv_display_set_flush_cb(disp, my_flush_cb);

// 3. 设置缓冲区
lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

// 4. 设置颜色格式 (可选)
lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
```

### 2.2 输入设备API变化

| v8.x | v9.x |
|------|------|
| `lv_indev_drv_t` 结构体 | `lv_indev_t *` 指针 |
| `lv_indev_drv_init(&drv)` | `lv_indev_create()` |
| `lv_indev_drv_register(&drv)` | `lv_indev_set_type(indev, type)` |
| - | `lv_indev_set_read_cb(indev, cb)` |

**v9.x 输入设备创建流程:**

```c
// 1. 创建输入设备
lv_indev_t *indev = lv_indev_create();

// 2. 设置类型
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);

// 3. 设置读取回调
lv_indev_set_read_cb(indev, my_read_cb);
```

### 2.3 Tick API

```c
// 方法1: 定时器中断调用
lv_tick_inc(1);  // 每1ms调用一次

// 方法2: 设置自定义回调 (推荐)
lv_tick_set_cb(my_tick_get);  // my_tick_get 返回毫秒数

// 方法3: 使用lv_delay_ms()
lv_delay_ms(100);  // 延时100ms
```

### 2.4 主循环

```c
while(1)
{
    lv_task_handler();  // LVGL任务处理
    lv_tick_inc(5);     // 如果使用lv_tick_inc()，需要定期调用
    delay_ms(5);
}
```

---

## 3. 官方驱动说明

### 3.1 内置驱动文件

```
LVGL/src/drivers/display/
├── ili9341/
│   ├── lv_ili9341.h      # ILI9341驱动头文件
│   └── lv_ili9341.c      # ILI9341驱动实现
└── lcd/
    ├── lv_lcd_generic_mipi.h  # 通用MIPI LCD驱动
    └── lv_lcd_generic_mipi.c
```

### 3.2 官方ILI9341驱动API

**创建ILI9341显示设备:**

```c
/**
 * 创建ILI9341显示设备
 * @param hor_res       水平分辨率 (240)
 * @param ver_res       垂直分辨率 (320)
 * @param flags         配置标志 (镜像、RGB顺序等)
 * @param send_cmd_cb   发送命令回调函数
 * @param send_color_cb 发送颜色数据回调函数
 * @return              显示设备指针
 */
lv_display_t * lv_ili9341_create(uint32_t hor_res, uint32_t ver_res, lv_lcd_flag_t flags,
                                 lv_ili9341_send_cmd_cb_t send_cmd_cb,
                                 lv_ili9341_send_color_cb_t send_color_cb);
```

**回调函数类型:**

```c
// 发送命令回调
typedef void (*lv_ili9341_send_cmd_cb_t)(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size,
                                         const uint8_t * param, size_t param_size);

// 发送颜色数据回调
typedef void (*lv_ili9341_send_color_cb_t)(lv_display_t * disp, const lv_area_t * area,
                                           uint8_t * color_p, uint32_t color_p_size);
```

**配置标志 (lv_lcd_flag_t):**

```c
#define LV_LCD_FLAG_FLIP_VERTICAL    (1 << 0)  // 垂直翻转
#define LV_LCD_FLAG_FLIP_HORIZONTAL  (1 << 1)  // 水平翻转
#define LV_LCD_FLAG_RGB_ORDER        (1 << 3)  // RGB/BGR顺序
#define LV_LCD_FLAG_LINE_ADDR_ORDER  (1 << 4)  // 行地址顺序
#define LV_LCD_FLAG_COL_ADDR_ORDER   (1 << 6)  // 列地址顺序
#define LV_LCD_FLAG_PAGE_ADDR_ORDER  (1 << 7)  // 页地址顺序
```

---

## 4. 文件结构

### 4.1 移植前项目结构

```
触摸屏_usart/
├── Main/
│   ├── main.c              # 主函数
│   └── main.h
├── USER/
│   ├── LCD/                # LCD驱动 (保持不变)
│   ├── TOUCH/              # 触摸驱动 (保持不变)
│   ├── LED/                # LED驱动
│   ├── KEY/                # 按键驱动
│   └── BEEP/               # 蜂鸣器驱动
├── Common/                 # 公共函数
├── STM32F4xx_HAL_Driver/   # HAL库
├── Startup_config/         # 启动文件
└── Project/                # Keil工程文件
```

### 4.2 移植后项目结构

```
触摸屏_usart/
├── Main/
│   ├── main.c              # 主函数 (修改)
│   └── main.h
├── LVGL/                   # 新增: LVGL库
│   ├── lvgl.h              # LVGL主头文件
│   ├── lv_conf.h           # 新增: 配置文件
│   └── src/                # LVGL源码
│       ├── core/           # 核心
│       ├── display/        # 显示API
│       ├── indev/          # 输入设备API
│       ├── tick/           # Tick API
│       ├── draw/           # 绘图引擎
│       ├── font/           # 字体
│       ├── drivers/        # 官方驱动 (包含ILI9341)
│       └── widgets/        # 控件
├── GUI/                    # 新增: 驱动适配层
│   ├── gui_driver.h        # 驱动接口声明
│   └── gui_driver.c        # 驱动适配实现
├── USER/                   # 现有驱动 (保持不变)
├── Common/
├── STM32F4xx_HAL_Driver/
├── Startup_config/
└── Project/
```

---

## 5. 移植步骤

### 步骤1: 创建 lv_conf.h

```bash
cp LVGL/lv_conf_template.h LVGL/lv_conf.h
```

**编辑 LVGL/lv_conf.h，关键配置:**

```c
/**
 * lv_conf.h - LVGL配置文件
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*===========================================================================
 * 显示配置
 *===========================================================================*/
/* 启用ILI9341驱动 */
#define LV_USE_ILI9341          1

/* 启用通用MIPI LCD驱动 */
#define LV_USE_GENERIC_MIPI     1

/* 显示缓冲区大小 */
#define LV_DEF_DRAW_BUF_SIZE   2400  /* 240*10 */

/* 颜色格式 */
#define LV_COLOR_DEPTH          16   /* RGB565 */

/*===========================================================================
 * 输入设备配置
 *===========================================================================*/
#define LV_USE_GROUP            1
#define LV_USE_MOUSE            1

/*===========================================================================
 * 控件配置
 *===========================================================================*/
#define LV_USE_ARC              1
#define LV_USE_BAR              1
#define LV_USE_BTN              1
#define LV_USE_BTNMATRIX        1
#define LV_USE_CANVAS           1
#define LV_USE_CHECKBOX         1
#define LV_USE_DROPDOWN         1
#define LV_USE_IMG              1
#define LV_USE_LABEL            1
#define LV_USE_LINE             1
#define LV_USE_ROLLER           1
#define LV_USE_SLIDER           1
#define LV_USE_SWITCH           1
#define LV_USE_TEXTAREA         1
#define LV_USE_CHART            1
#define LV_USE_LIST             1
#define LV_USE_MSGBOX           1
#define LV_USE_SPINNER          1
#define LV_USE_TABVIEW          1
#define LV_USE_TABLE            1

/*===========================================================================
 * 字体配置
 *===========================================================================*/
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1

/*===========================================================================
 * 主题配置
 *===========================================================================*/
#define LV_USE_THEME_DEFAULT    1

#endif /* LV_CONF_H */
```

### 步骤2: 创建 GUI 目录

```bash
mkdir GUI
```

### 步骤3: 创建驱动适配文件

**创建 GUI/gui_driver.h:**

```c
/**
 * gui_driver.h - LVGL驱动适配层接口
 */

#ifndef __GUI_DRIVER_H
#define __GUI_DRIVER_H

#include "lvgl.h"

/**
 * @brief 初始化显示驱动
 */
void gui_disp_init(void);

/**
 * @brief 初始化触摸驱动
 */
void gui_touch_init(void);

/**
 * @brief 初始化tick (在定时器中断中调用)
 */
void gui_tick_init(void);

/**
 * @brief LVGL任务处理 (在主循环中调用)
 */
void gui_task_handler(void);

#endif /* __GUI_DRIVER_H */
```

**创建 GUI/gui_driver.c:**

```c
/**
 * gui_driver.c - LVGL驱动适配层实现
 * 使用LVGL v9.5.0官方ILI9341驱动
 */

#include "gui_driver.h"
#include "lcd.h"
#include "touch.h"

/* 包含ILI9341驱动头文件 */
#include "src/drivers/display/ili9341/lv_ili9341.h"

/*===========================================================================
 * 显示驱动配置
 *===========================================================================*/

/* LCD分辨率 */
#define MY_DISP_HOR_RES  240
#define MY_DISP_VER_RES  320

/* 缓冲区行数 */
#define BUF_LINES        10

/* 双缓冲区: 240 * 10 * 2 bytes(RGB565) = 4800 bytes each */
static lv_color_t buf1[MY_DISP_HOR_RES * BUF_LINES];
static lv_color_t buf2[MY_DISP_HOR_RES * BUF_LINES];

/* 显示设备指针 */
static lv_display_t * disp_handle = NULL;

/**
 * @brief 发送命令回调函数
 * 向ILI9341发送命令和参数
 */
static void lcd_send_cmd_cb(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size,
                            const uint8_t * param, size_t param_size)
{
    /* 发送命令 */
    for(size_t i = 0; i < cmd_size; i++)
    {
        LCD_CMD = cmd[i];
    }

    /* 发送参数 */
    if(param != NULL && param_size > 0)
    {
        for(size_t i = 0; i < param_size; i++)
        {
            LCD_DATA = param[i];
        }
    }
}

/**
 * @brief 发送颜色数据回调函数
 * 向ILI9341发送像素数据
 */
static void lcd_send_color_cb(lv_display_t * disp, const lv_area_t * area,
                              uint8_t * color_p, uint32_t color_p_size)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t size = width * height;

    /* 设置LCD窗口区域 */
    LCD_Open_Window(area->x1, area->y1, width, height);

    /* 准备写入GRAM */
    LCD_WriteGRAM();

    /* 批量写入像素数据 */
    uint16_t * color_p16 = (uint16_t *)color_p;
    for(uint32_t i = 0; i < size; i++)
    {
        LCD_DATA = color_p16[i];
    }
}

/**
 * @brief 初始化显示驱动
 * 使用LVGL v9.5.0官方ILI9341驱动
 */
void gui_disp_init(void)
{
    /* 配置标志: 根据实际情况调整 */
    lv_lcd_flag_t flags = 0;

    /* 如果需要RGB顺序，取消注释 */
    // flags |= LV_LCD_FLAG_RGB_ORDER;

    /* 如果需要翻转，取消注释 */
    // flags |= LV_LCD_FLAG_FLIP_VERTICAL;
    // flags |= LV_LCD_FLAG_FLIP_HORIZONTAL;

    /* 创建ILI9341显示设备 */
    disp_handle = lv_ili9341_create(MY_DISP_HOR_RES, MY_DISP_VER_RES,
                                    flags, lcd_send_cmd_cb, lcd_send_color_cb);

    /* 设置显示缓冲区 */
    lv_display_set_buffers(disp_handle, buf1, buf2,
                           MY_DISP_HOR_RES * BUF_LINES * sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

/*===========================================================================
 * 触摸驱动配置
 *===========================================================================*/

/**
 * @brief 触摸读取回调函数
 * 读取XPT2046触摸坐标
 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0, last_y = 0;

    /* 调用现有的XPT2046扫描函数 */
    XPT2046_Scan(0);

    /* 检测触摸状态 */
    if(Xdown != 0 && Ydown != 0)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = Xdown;
        data->point.y = Ydown;

        /* 保存最后有效坐标 */
        last_x = data->point.x;
        last_y = data->point.y;
    }
    else
    {
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
    lv_indev_t *indev;

    /* 创建输入设备 */
    indev = lv_indev_create();

    /* 设置输入设备类型 */
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);

    /* 设置读取回调 */
    lv_indev_set_read_cb(indev, touch_read_cb);
}

/*===========================================================================
 * Tick配置
 *===========================================================================*/

/**
 * @brief 获取系统tick (毫秒)
 * 使用HAL_GetTick()作为时钟源
 */
static uint32_t gui_tick_get(void)
{
    return HAL_GetTick();
}

/**
 * @brief 初始化tick
 * 设置LVGL的tick回调
 */
void gui_tick_init(void)
{
    /* 设置自定义tick回调 */
    lv_tick_set_cb(gui_tick_get);
}

/*===========================================================================
 * 任务处理
 *===========================================================================*/

/**
 * @brief LVGL任务处理
 * 在主循环中调用
 */
void gui_task_handler(void)
{
    lv_task_handler();
}
```

### 步骤4: 修改主函数

**修改 Main/main.c:**

```c
/**
 * @file    main.c
 * @brief   主函数 - LVGL移植版本
 * @note    基于STM32F407ZG + ILI9341 + XPT2046
 *          使用LVGL v9.5.0官方ILI9341驱动
 */

#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "gui_driver.h"     /* LVGL驱动接口 */
#include "lvgl.h"           /* LVGL头文件 */

/**
 * @brief 主函数
 */
int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336, 8, 2, 7);   /* 168MHz */
    delay_init();

    /* 硬件初始化 */
    LED_Init();
    BEEP_Init();
    KEY_Init();
    LCD_Init();                        /* 初始化LCD + FSMC */
    Touch_Init();                      /* 初始化XPT2046触摸 */

    /* LVGL初始化 */
    lv_init();                         /* 初始化LVGL核心 */
    gui_tick_init();                   /* 初始化tick (必须在disp_init之前) */
    gui_disp_init();                   /* 初始化显示驱动 (使用官方ILI9341驱动) */
    gui_touch_init();                  /* 初始化触摸驱动 */

    /* 测试: 显示一个按钮 */
    lv_obj_t *btn = lv_btn_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_center(btn);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "LVGL OK!");
    lv_obj_center(label);

    /* 主循环 */
    while(1)
    {
        gui_task_handler();            /* LVGL任务处理 */
        delay_ms(5);                   /* 约200Hz刷新率 */

        /* LED心跳指示 */
        static uint32_t led_tick = 0;
        if(HAL_GetTick() - led_tick >= 500)
        {
            LED0 = !LED0;
            led_tick = HAL_GetTick();
        }
    }
}
```

---

## 6. 驱动适配

### 6.1 官方驱动工作原理

```
LVGL渲染 → 官方ILI9341驱动 → lcd_send_cmd_cb() → LCD_CMD/LCD_DATA → FSMC写入
                           → lcd_send_color_cb() → LCD_Open_Window() → LCD_DATA
```

### 6.2 你只需实现两个回调函数

| 回调函数 | 功能 | 说明 |
|----------|------|------|
| `lcd_send_cmd_cb()` | 发送命令和参数 | 调用现有的 LCD_CMD 和 LCD_DATA |
| `lcd_send_color_cb()` | 发送像素数据 | 调用现有的 LCD_Open_Window() 和 LCD_WriteGRAM() |

### 6.3 关键函数说明

| 函数 | 功能 | 所在文件 |
|------|------|----------|
| `LCD_Open_Window()` | 设置LCD显示窗口 | USER/LCD/lcd.c |
| `LCD_WriteGRAM()` | 准备写入GRAM | USER/LCD/lcd.c |
| `LCD_CMD` | 写命令寄存器 | USER/LCD/lcd.h |
| `LCD_DATA` | 写数据寄存器 | USER/LCD/lcd.h |
| `XPT2046_Scan()` | 扫描触摸状态 | USER/TOUCH/xpt2046.c |
| `Xdown, Ydown` | 触摸坐标变量 | USER/TOUCH/xpt2046.c |

---

## 7. 编译配置

### 7.1 Keil工程配置

**1. 添加LVGL头文件路径:**

```
Project → Options for Target → C/C++ → Include Paths
```

添加:
- `..\LVGL`
- `..\LVGL\src`
- `..\LVGL\src\drivers\display\ili9341`
- `..\GUI`

**2. 添加LVGL源文件:**

```
Project → Manage Project Items
```

创建分组 `LVGL`，添加以下文件:
- `LVGL/src/*.c` (所有.c文件)

创建分组 `GUI`，添加:
- `GUI/gui_driver.c`

**3. 定义预处理宏:**

```
Project → Options for Target → C/C++ → Preprocessor Symbols → Define
```

添加:
```
USE_HAL_DRIVER,STM32F407xx,LV_CONF_INCLUDE_SIMPLE
```

**4. 优化设置:**

```
Project → Options for Target → C/C++ → Optimization
```

建议: Level 1 (-O1) 或 Level 2 (-O2)

### 7.2 内存配置

```
Project → Options for Target → Target
```

- IROM1: 0x08000000, Size: 0x100000 (1MB)
- IRAM1: 0x20000000, Size: 0x20000 (128KB)
- IRAM2: 0x10000000, Size: 0x10000 (64KB CCM)

---

## 8. 测试验证

### 8.1 编译测试

```bash
# 编译项目，确保无错误
# 常见错误:
# - 头文件路径错误
# - 源文件未添加
# - lv_conf.h配置错误
# - LV_USE_ILI9341 未定义为1
# - LV_USE_GENERIC_MIPI 未定义为1
```

### 8.2 显示测试

```c
/* 测试1: 清屏 */
lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0000FF), 0);
lv_refr_now(NULL);

/* 测试2: 显示文字 */
lv_obj_t *label = lv_label_create(lv_screen_active());
lv_label_set_text(label, "Hello LVGL!");
lv_obj_center(label);

/* 测试3: 显示按钮 */
lv_obj_t *btn = lv_btn_create(lv_screen_active());
lv_obj_set_size(btn, 120, 50);
lv_obj_center(btn);
```

### 8.3 触摸测试

```c
/* 测试触摸事件 */
static void btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED)
    {
        LV_LOG_USER("Button clicked!");
    }
}

lv_obj_t *btn = lv_btn_create(lv_screen_active());
lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);
```

### 8.4 性能测试

```c
/* 启用性能监控 */
#define LV_USE_PERF_MONITOR    1

/* 或手动测量 */
uint32_t start = HAL_GetTick();
lv_refr_now(NULL);
uint32_t end = HAL_GetTick();
printf("Refresh time: %d ms\n", end - start);
```

---

## 9. 常见问题

### Q1: 编译报错 "lv_conf.h not found"

**解决:** 确保:
1. `LVGL/lv_conf.h` 文件存在
2. 预处理宏定义了 `LV_CONF_INCLUDE_SIMPLE`
3. 头文件路径包含 `..\LVGL`

### Q2: LCD显示花屏

**解决:**
1. 检查 `lv_lcd_flag_t flags` 配置
2. 尝试添加 `LV_LCD_FLAG_RGB_ORDER` 或 `LV_LCD_FLAG_FLIP_VERTICAL`
3. 检查FSMC时序配置

### Q3: 触摸无响应

**解决:**
1. 确认XPT2046驱动正常工作
2. 检查触摸回调函数是否被调用
3. 验证坐标映射是否正确

### Q4: 内存不足

**解决:**
1. 减小 `LV_MEM_SIZE` (lv_conf.h)
2. 减小缓冲区行数 `BUF_LINES`
3. 关闭不需要的控件

### Q5: 动画卡顿

**解决:**
1. 增加 `LV_DEF_REFR_PERIOD`
2. 使用部分刷新模式
3. 减少同时动画的对象数量

### Q6: 编译报错 "undefined reference to lv_ili9341_create"

**解决:**
1. 确保 `LV_USE_ILI9341` 定义为1
2. 确保 `LV_USE_GENERIC_MIPI` 定义为1
3. 确保添加了 `LVGL/src/drivers/display/ili9341/lv_ili9341.c` 到工程

---

## 附录

### A. 官方驱动文件位置

```
LVGL/src/drivers/display/
├── ili9341/
│   ├── lv_ili9341.h      # ILI9341驱动
│   └── lv_ili9341.c
├── lcd/
│   ├── lv_lcd_generic_mipi.h  # 通用MIPI驱动
│   └── lv_lcd_generic_mipi.c
├── st7735/
├── st7789/
├── st7796/
└── ...
```

### B. 参考资源

- [LVGL官方文档](https://docs.lvgl.io/9.5/)
- [LVGL GitHub](https://github.com/lvgl/lvgl)
- [LVGL Porting指南](https://docs.lvgl.io/9.5/porting/index.html)

### C. 文件清单

| 文件 | 状态 | 说明 |
|------|------|------|
| `LVGL/lvgl.h` | 新增 | LVGL主头文件 |
| `LVGL/lv_conf.h` | 新增 | LVGL配置文件 |
| `LVGL/src/*` | 新增 | LVGL源码 (包含官方ILI9341驱动) |
| `GUI/gui_driver.h` | 新增 | 驱动接口声明 |
| `GUI/gui_driver.c` | 新增 | 驱动适配实现 |
| `Main/main.c` | 修改 | 添加LVGL初始化 |

---

**文档版本:** v2.0
**更新日期:** 2026-07-28
**LVGL版本:** v9.5.0
**硬件平台:** STM32F407ZG + ILI9341 + XPT2046
**重要更新:** 基于LVGL v9.x官方API，使用官方ILI9341驱动
