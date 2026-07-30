/**
 * @file lv_conf.h
 * @brief LVGL配置文件 - STM32F407ZG + ILI9341
 *
 * @version 1.0
 * @date 2026-07-28
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*===========================================================================
 * 颜色配置
 *===========================================================================*/
#define LV_COLOR_DEPTH          16    /* RGB565 - ILI9341 */
#define LV_COLOR_16_SWAP        0     /* 不交换字节顺序 */
#define LV_COLOR_CHROMA_KEY lv_color_hex(0x00ff00)

/*===========================================================================
 * 内存配置
 *===========================================================================*/
#define LV_MEM_CUSTOM          0
#define LV_MEM_SIZE            (48U * 1024U)    /* 48KB堆 */

/*===========================================================================
 * HAL配置
 *===========================================================================*/
#define LV_TICK_CUSTOM          0
#define LV_DEF_REFR_PERIOD     30    /* 30ms刷新周期 */
#define LV_DEF_DRAW_BUF_SIZE   2400  /* 240*10 */
#define LV_DRAW_COMPLEX        1
#define LV_DRAW_TIMEOUT_MS     50

/*===========================================================================
 * 日志配置
 *===========================================================================*/
#define LV_USE_LOG             0
#if LV_USE_LOG
    #define LV_LOG_LEVEL        LV_LOG_LEVEL_ERROR
    #define LV_LOG_PRINTF       0
#endif

/*===========================================================================
 * 断言配置
 *===========================================================================*/
#define LV_USE_ASSERT_NULL          0
#define LV_USE_ASSERT_MALLOC        0
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/*===========================================================================
 * 显示配置
 *===========================================================================*/
#define LV_USE_DISPLAY              1
#define LV_USE_DRAW_ROTATE          0
#define LV_USE_DRAW_DOUBLE_BUFFER   1

/*===========================================================================
 * ILI9341驱动配置
 *===========================================================================*/
#define LV_USE_ILI9341              1
#define LV_USE_GENERIC_MIPI         1

/*===========================================================================
 * GPU配置
 *===========================================================================*/
#define LV_USE_GPU_STM32_DMA2D      0

/*===========================================================================
 * 输入设备配置
 *===========================================================================*/
#define LV_USE_GROUP            1
#define LV_USE_KEYBOARD         0
#define LV_USE_MOUSE            1
#define LV_USE_MOUSEWHEEL       0
#define LV_USE_ENCODER          0
#define LV_USE_BUTTON           1

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

/* 高级控件 */
#define LV_USE_CHART            1
#define LV_USE_LED              1
#define LV_USE_LIST             1
#define LV_USE_MSGBOX           1
#define LV_USE_SPINNER          1
#define LV_USE_TABVIEW          1
#define LV_USE_TABLE            1

/*===========================================================================
 * 主题配置
 *===========================================================================*/
#define LV_USE_THEME_DEFAULT    1
#define LV_THEME_DEFAULT_DARK   0

/*===========================================================================
 * 字体配置
 *===========================================================================*/
#define LV_FONT_MONTSERRAT_12   1
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_FMT_TXT_LARGE  0
#define LV_USE_FONT_PLACEHOLDER 1

/*===========================================================================
 * 布局配置
 *===========================================================================*/
#define LV_USE_FLEX              1
#define LV_USE_GRID              1

/*===========================================================================
 * 其他配置
 *===========================================================================*/
#define LV_USE_PERF_MONITOR    0
#define LV_USE_MEM_MONITOR     0
#define LV_USE_SNAPSHOT        0
#define LV_USE_ANIM            1
#define LV_USE_IMG_DECODER    1
#define LV_USE_FREETYPE        0
#define LV_USE_RLOTTIE         0
#define LV_USE_FFMPEG          0
#define LV_USE_GSTREAMER       0
#define LV_USE_LIBJPEG_TURBO   0
#define LV_USE_LIBPNG          0
#define LV_USE_LIBWEBP         0
#define LV_USE_SDL             0
#define LV_USE_WINDOWS         0
#define LV_USE_WAYLAND         0
#define LV_USE_X11             0
#define LV_USE_QNX             0
#define LV_USE_DRM             0
#define LV_USE_LINUX_FB        0
#define LV_USE_EVDEV           0
#define LV_USE_LIBINPUT        0
#define LV_USE_NUTTX           0
#define LV_USE_QRCODE          0
#define LV_USE_BARCODE          0
#define LV_USE_BMP             0
#define LV_USE_GIF             0
#define LV_USE_LODEPNG         0
#define LV_USE_LIBPNG          0
#define LV_USE_TINY_TTF        0
#define LV_USE_TJPGD           0
#define LV_USE_SVG             0
#define LV_USE_LZ4             0
#define LV_USE_FROGFS          0
#define LV_USE_VG_LITE_DRIVER  0
#define LV_USE_NANOVG          0
#define LV_USE_3DTEXTURE       0
#define LV_USE_LOTTIE          0
#define LV_USE_FONT_MANAGER    0
#define LV_USE_IMG_FONT        0
#define LV_USE_FILE_EXPLORER   0
#define LV_USE_TRANSLATION     0
#define LV_USE_FRAGMENT        0

#endif /* LV_CONF_H */
