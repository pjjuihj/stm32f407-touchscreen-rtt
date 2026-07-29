# LVGL移植设计文档

## 1. 项目概述

### 1.1 项目目标

将LVGL v9.5.0图形库移植到STM32F407ZG开发板，支持ILI9341 (240×320) LCD显示屏和XPT2046电阻触摸屏，实现完整的GUI应用。

### 1.2 项目范围

| 项目 | 内容 |
|------|------|
| **硬件平台** | STM32F407ZG + ILI9341 (240×320) + XPT2046触摸 |
| **软件版本** | LVGL v9.5.0 |
| **功能需求** | 中文字体(3000字)、图片显示、动画效果 |
| **开发周期** | 1周 |
| **代码规范** | MISRA-C标准、模块化、可复用 |
| **文档要求** | 设计文档(Markdown)、API文档(Doxygen)、用户手册(PDF) |

### 1.3 硬件信息

**MCU规格:**
| 参数 | 规格 |
|------|------|
| **型号** | STM32F407ZGT6 |
| **内核** | ARM Cortex-M4 |
| **主频** | 168MHz |
| **Flash** | 1MB |
| **SRAM** | 192KB (128KB + 64KB CCM) |

**LCD显示模块:**
| 参数 | 规格 |
|------|------|
| **驱动芯片** | ILI9341 |
| **分辨率** | 240 × 320 像素 |
| **颜色深度** | 16位 RGB565 |
| **接口** | 8080并口 (FSMC控制) |
| **背光** | PF10 (GPIO控制) |

**触摸屏模块:**
| 参数 | 规格 |
|------|------|
| **触摸芯片** | XPT2046 |
| **类型** | 电阻触摸屏 |
| **接口** | SPI (软件模拟) |
| **分辨率** | 4096 × 4096 (12位ADC) |

**LCD引脚连接 (FSMC Bank4):**
| LCD引脚 | STM32引脚 | 功能 |
|---------|-----------|------|
| D0~D15 | PD14,PD15,PD0,PD1,PE7~PE15,PD8~PD10 | 16位数据总线 |
| CS | PG12 | 片选 (FSMC_NE4) |
| RS/DC | PF12 | 数据/命令选择 |
| WR | PD5 | 写使能 (FSMC_NWE) |
| RD | PD4 | 读使能 (FSMC_NOE) |
| BL | PF10 | 背光控制 |

**触摸校准参数:**
```c
// 已在 USER/TOUCH/xpt2046.c 中定义
extern float xFactor;   // 0.06671114
extern float yFactor;   // 0.09117551
extern short xOffset;   // -11
extern short yOffset;   // -18
```

## 2. 验收标准

### 2.1 功能验收

**显示测试:**
| 测试项 | 标准 |
|--------|------|
| **测试内容** | 渐变、文字、图形、动画 |
| **测试时间** | 10分钟 |
| **稳定性** | 无闪烁、无花屏、无死机 |

**触摸测试:**
| 测试项 | 标准 |
|--------|------|
| **测试操作** | 滑动 |
| **响应时间** | <50ms |
| **精度** | <5像素 |

**性能测试:**
| 测试项 | 标准 |
|--------|------|
| **帧率** | 60fps |
| **刷新延迟** | <100ms |

### 2.2 稳定性验收

| 测试项 | 标准 |
|--------|------|
| **连续运行时间** | 24小时 |
| **测试内容** | 显示+触摸+动画循环 |
| **稳定性** | 无崩溃、无内存泄漏 |

### 2.3 资源限制

| 资源 | 限制 |
|------|------|
| **RAM使用** | <100KB |
| **CPU使用** | <50% |
| **Flash使用** | <500KB |

### 2.4 代码验收

| 验收项 | 标准 |
|--------|------|
| **代码规范** | MISRA-C标准 |
| **静态分析** | 通过cppcheck检查 |
| **代码审查** | 无严重问题 |
| **模块化** | 职责清晰、依赖单向 |

### 2.5 文档验收

| 文档 | 格式 | 内容 |
|------|------|------|
| **设计文档** | Markdown | 架构图、模块说明、接口定义、数据流 |
| **API文档** | Doxygen | 函数说明、参数说明、返回值、示例代码 |
| **用户手册** | PDF | 使用说明、配置说明、故障排除 |

## 3. 方案对比

### 3.1 显示缓冲方案对比

| 方案 | 描述 | RAM占用 | 优点 | 缺点 | 推荐度 |
|------|------|---------|------|------|--------|
| **方案A: 标准双缓冲** | 240×10×2×2 = 9.6KB | 9.6KB | 官方推荐，动画流畅，无闪烁 | RAM占用较大 | ⭐⭐⭐⭐⭐ |
| **方案B: 单缓冲** | 240×10×2 = 4.8KB | 4.8KB | RAM占用小 | 高级动画可能闪烁 | ⭐⭐⭐ |
| **方案C: 全屏缓冲** | 240×320×2 = 150KB | 150KB | 性能最好 | RAM不足(192KB) | ❌ |
| **方案D: 外部SRAM** | 外部512KB SRAM | 0KB内部 | RAM充足 | 需要硬件修改，FSMC冲突 | ❌ |

**推荐：方案A (标准双缓冲)**
- STM32F407ZG有192KB RAM，9.6KB足够
- 动画流畅，无闪烁
- 官方推荐，文档丰富

### 3.2 模块化架构对比

| 方案 | 描述 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **方案A: 分层+功能** | UI层→业务层→驱动层→HAL层 | 职责清晰，依赖单向，易维护 | 文件较多 | ⭐⭐⭐⭐⭐ |
| **方案B: 纯功能划分** | 按显示、触摸、字体等划分 | 结构简单 | 依赖混乱，难维护 | ⭐⭐ |
| **方案C: 纯文件划分** | 每个功能一个文件 | 文件少 | 职责不清，难扩展 | ⭐ |

**推荐：方案A (分层+功能划分)**
- 职责清晰：每层只做一件事
- 依赖单向：上层调用下层，下层不依赖上层
- 易于测试：每层可以独立测试
- 易于替换：更换硬件时只需修改驱动层
- 代码复用：业务层和UI层可以在不同项目复用

### 3.3 驱动适配方案对比

| 方案 | 描述 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **方案A: 官方ILI9341驱动** | 使用LVGL内置驱动 | 官方维护，稳定性好，只需2个回调 | 需要理解官方API | ⭐⭐⭐⭐⭐ |
| **方案B: 自定义驱动** | 自己实现完整的LCD驱动 | 完全控制，灵活 | 工作量大，易出错 | ⭐⭐ |
| **方案C: 通用LCD驱动** | 使用LVGL通用LCD驱动 | 通用性强 | 性能可能不如专用驱动 | ⭐⭐⭐ |

**推荐：方案A (官方ILI9341驱动)**
- LVGL v9.5.0内置ILI9341官方驱动
- 只需实现2个回调函数：`lcd_send_cmd_cb` 和 `lcd_send_color_cb`
- 官方维护，稳定性好
- 自动处理ILI9341初始化序列

### 3.4 Tick源方案对比

| 方案 | 描述 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **方案A: HAL_GetTick()回调** | 使用HAL库的tick | 简单，无需额外配置 | 精度可能不够 | ⭐⭐⭐⭐ |
| **方案B: 定时器中断** | 使用TIM2产生1ms中断 | 精度高，稳定 | 需要配置定时器 | ⭐⭐⭐⭐⭐ |
| **方案C: SysTick中断** | 使用SysTick产生中断 | 精度高 | 可能与HAL库冲突 | ⭐⭐⭐ |

**推荐：方案B (定时器中断)**
- 使用TIM2产生1ms中断
- 精度高，稳定
- 不会与HAL库冲突
- 可以精确控制LVGL时基

### 3.5 中文字体方案对比

| 方案 | 描述 | Flash占用 | 优点 | 缺点 | 推荐度 |
|------|------|-----------|------|------|--------|
| **方案A: lv_font_conv生成** | 使用官方工具生成 | 200-250KB | 官方支持，兼容性好 | 需要生成工具 | ⭐⭐⭐⭐⭐ |
| **方案B: 外部字库芯片** | 使用外部Flash存储字库 | 0KB内部 | Flash空间充足 | 需要硬件修改 | ⭐⭐⭐ |
| **方案C: 完整GB2312字库** | 包含所有汉字 | 1MB+ | 覆盖全面 | Flash不足 | ❌ |

**推荐：方案A (lv_font_conv生成)**
- 使用LVGL官方字体转换工具
- 生成常用3000字，约200-250KB Flash
- STM32F407ZG有1MB Flash，足够
- 官方支持，兼容性好

### 3.6 图片显示方案对比

| 方案 | 描述 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **方案A: LVGL内置解码器** | 使用LVGL内置图片解码器 | 支持PNG/JPEG/BMP，官方维护 | 需要额外Flash空间 | ⭐⭐⭐⭐⭐ |
| **方案B: 自定义解码器** | 自己实现图片解码 | 完全控制，灵活 | 工作量大，易出错 | ⭐⭐ |
| **方案C: 预处理图片** | 将图片转换为C数组 | 解码速度快，简单 | 图片格式固定，不易修改 | ⭐⭐⭐ |

**推荐：方案A (LVGL内置解码器)**
- LVGL v9.x内置图片解码器
- 支持PNG、JPEG、BMP等格式
- 官方维护，稳定性好
- 可以动态加载图片

### 3.7 动画方案对比

| 方案 | 描述 | 优点 | 缺点 | 推荐度 |
|------|------|------|------|--------|
| **方案A: LVGL内置动画** | 使用LVGL动画系统 | 官方支持，功能强大，易于使用 | 需要学习API | ⭐⭐⭐⭐⭐ |
| **方案B: 自定义动画** | 自己实现动画逻辑 | 完全控制，灵活 | 工作量大，易出错 | ⭐⭐ |
| **方案C: 第三方动画库** | 使用其他动画库 | 功能丰富 | 增加依赖，兼容性问题 | ⭐⭐ |

**推荐：方案A (LVGL内置动画)**
- LVGL v9.x内置强大的动画系统
- 支持属性动画、路径动画、时间线等
- 官方支持，文档丰富
- 可以轻松实现各种动画效果

### 3.8 方案总结

| 模块 | 推荐方案 | 理由 |
|------|----------|------|
| **显示缓冲** | 标准双缓冲 | 官方推荐，动画流畅 |
| **模块化架构** | 分层+功能划分 | 职责清晰，易维护 |
| **驱动适配** | 官方ILI9341驱动 | 官方维护，只需2个回调 |
| **Tick源** | 定时器中断 | 精度高，稳定 |
| **中文字体** | lv_font_conv生成 | 官方支持，兼容性好 |
| **图片显示** | LVGL内置解码器 | 官方维护，功能强大 |
| **动画** | LVGL内置动画 | 官方支持，功能强大 |

**最终推荐：全部使用官方方案**
- 使用LVGL v9.5.0内置的ILI9341驱动
- 使用LVGL内置的动画系统
- 使用LVGL内置的图片解码器
- 使用lv_font_conv生成中文字库

**理由：**
1. 官方维护，稳定性好
2. 文档丰富，易于学习
3. 兼容性好，不易出错
4. 功能强大，满足需求

## 4. 架构设计

### 3.1 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                     应用层 (UI层)                         │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │  ui_main.c  │  │ ui_data.c   │  │ui_settings.c│     │
│  │  主界面     │  │  数据界面   │  │  设置界面   │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
├─────────────────────────────────────────────────────────┤
│                     业务层                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │ gui_app.c   │  │ gui_fonts.c │  │ gui_images.c│     │
│  │ 应用逻辑   │  │  字体管理   │  │  图片管理   │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
├─────────────────────────────────────────────────────────┤
│                     驱动层                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │gui_display.c│  │gui_touch.c  │  │ gui_tick.c  │     │
│  │  显示驱动   │  │  触摸驱动   │  │  时钟驱动   │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
├─────────────────────────────────────────────────────────┤
│                     硬件抽象层 (HAL)                     │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │  lcd.c      │  │ xpt2046.c   │  │  delay.c    │     │
│  │  LCD驱动    │  │  触摸芯片   │  │  延时函数   │     │
│  └─────────────┘  └─────────────┘  └─────────────┘     │
└─────────────────────────────────────────────────────────┘
```

### 3.2 模块职责

**应用层 (UI层):**
- `ui_main.c` - 主界面，显示系统信息、导航菜单
- `ui_data.c` - 数据界面，显示传感器数据、图表
- `ui_settings.c` - 设置界面，系统参数配置

**业务层:**
- `gui_app.c` - 应用逻辑，页面切换、事件处理
- `gui_fonts.c` - 字体管理，中文字库加载、字体切换
- `gui_images.c` - 图片管理，图片解码、缓存管理

**驱动层:**
- `gui_display.c` - 显示驱动，LCD刷新回调、缓冲区管理
- `gui_touch.c` - 触摸驱动，触摸读取回调、坐标转换
- `gui_tick.c` - 时钟驱动，tick回调、延时管理

**硬件抽象层 (HAL):**
- `lcd.c` - LCD驱动，FSMC接口、ILI9341初始化
- `xpt2046.c` - 触摸芯片驱动，SPI接口、触摸读取
- `delay.c` - 延时函数，us/ms延时

### 3.3 依赖关系

```
UI层 → 业务层 → 驱动层 → 硬件抽象层
```

**依赖规则:**
1. 上层可以调用下层接口
2. 下层不能调用上层接口
3. 同层模块之间尽量避免直接调用
4. 通过接口（头文件）进行通信

### 3.4 接口设计

**驱动层接口:**
```c
/* 显示驱动接口 */
typedef void (*gui_flush_cb_t)(const lv_area_t *area, uint8_t *color_p);
void gui_disp_init(void);

/* 触摸驱动接口 */
typedef void (*gui_read_cb_t)(lv_indev_data_t *data);
void gui_touch_init(void);

/* 时钟驱动接口 */
uint32_t gui_tick_get(void);
void gui_tick_init(void);
```

**业务层接口:**
```c
/* 应用逻辑接口 */
void gui_app_init(void);
void gui_app_task_handler(void);

/* 字体管理接口 */
void gui_fonts_init(void);
const lv_font_t* gui_fonts_get(uint8_t size);

/* 图片管理接口 */
void gui_images_init(void);
const void* gui_images_get(const char *name);
```

**UI层接口:**
```c
/* 主界面接口 */
void ui_main_create(void);

/* 数据界面接口 */
void ui_data_create(void);

/* 设置界面接口 */
void ui_settings_create(void);
```

## 4. 文件结构

### 4.1 目录结构

```
触摸屏_usart/
├── Main/
│   └── main.c              # 主函数
├── LVGL/                   # LVGL库
│   ├── lvgl.h
│   ├── lv_conf.h           # 配置文件
│   └── src/                # 源码
├── GUI/                    # 驱动适配层
│   ├── gui_driver.h        # 驱动接口
│   ├── gui_driver.c        # 驱动实现
│   ├── gui_app.h           # 应用逻辑接口
│   ├── gui_app.c           # 应用逻辑实现
│   ├── gui_fonts.h         # 字体管理接口
│   ├── gui_fonts.c         # 字体管理实现
│   ├── gui_images.h        # 图片管理接口
│   ├── gui_images.c        # 图片管理实现
│   └── docs/               # 文档
│       ├── design.md       # 设计文档
│       ├── api.md          # API文档
│       └── user_manual.pdf # 用户手册
├── UI/                     # 界面层
│   ├── ui_main.h           # 主界面接口
│   ├── ui_main.c           # 主界面实现
│   ├── ui_data.h           # 数据界面接口
│   ├── ui_data.c           # 数据界面实现
│   ├── ui_settings.h       # 设置界面接口
│   └── ui_settings.c       # 设置界面实现
├── USER/                   # 现有驱动
│   ├── LCD/
│   └── TOUCH/
├── Common/
├── STM32F4xx_HAL_Driver/
├── Startup_config/
└── Project/
```

### 4.2 文件清单

| 文件 | 状态 | 说明 |
|------|------|------|
| `LVGL/lv_conf.h` | 新增 | LVGL配置文件 |
| `GUI/gui_driver.h` | 新增 | 驱动接口声明 |
| `GUI/gui_driver.c` | 新增 | 驱动适配实现 |
| `GUI/gui_app.h` | 新增 | 应用逻辑接口 |
| `GUI/gui_app.c` | 新增 | 应用逻辑实现 |
| `GUI/gui_fonts.h` | 新增 | 字体管理接口 |
| `GUI/gui_fonts.c` | 新增 | 字体管理实现 |
| `GUI/gui_images.h` | 新增 | 图片管理接口 |
| `GUI/gui_images.c` | 新增 | 图片管理实现 |
| `GUI/gui_log.h` | 新增 | 日志功能接口 |
| `GUI/gui_log.c` | 新增 | 日志功能实现 |
| `GUI/gui_stability.h` | 新增 | 稳定性监控接口 |
| `GUI/gui_stability.c` | 新增 | 稳定性监控实现 |
| `UI/ui_main.h` | 新增 | 主界面接口 |
| `UI/ui_main.c` | 新增 | 主界面实现 |
| `UI/ui_data.h` | 新增 | 数据界面接口 |
| `UI/ui_data.c` | 新增 | 数据界面实现 |
| `UI/ui_settings.h` | 新增 | 设置界面接口 |
| `UI/ui_settings.c` | 新增 | 设置界面实现 |
| `Main/main.c` | 修改 | 添加LVGL初始化 |

## 5. 核心代码设计

### 5.1 显示驱动 (gui_driver.c)

```c
/* 发送命令回调 */
static void lcd_send_cmd_cb(lv_display_t *disp, const uint8_t *cmd,
                            size_t cmd_size, const uint8_t *param, size_t param_size)
{
    for(size_t i = 0; i < cmd_size; i++)
    {
        LCD_CMD = cmd[i];
    }
    if(param != NULL && param_size > 0)
    {
        for(size_t i = 0; i < param_size; i++)
        {
            LCD_DATA = param[i];
        }
    }
}

/* 发送颜色回调 */
static void lcd_send_color_cb(lv_display_t *disp, const lv_area_t *area,
                              uint8_t *color_p, uint32_t color_p_size)
{
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t size = width * height;

    LCD_Open_Window(area->x1, area->y1, width, height);
    LCD_WriteGRAM();

    uint16_t *color_p16 = (uint16_t *)color_p;
    for(uint32_t i = 0; i < size; i++)
    {
        LCD_DATA = color_p16[i];
    }
}

/* 初始化显示驱动 */
void gui_disp_init(void)
{
    lv_lcd_flag_t flags = 0;
    lv_display_t *disp = lv_ili9341_create(240, 320, flags,
                                            lcd_send_cmd_cb,
                                            lcd_send_color_cb);

    static lv_color_t buf1[240 * 10];
    static lv_color_t buf2[240 * 10];
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}
```

### 5.2 触摸驱动 (gui_driver.c)

```c
/* 触摸读取回调 */
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0, last_y = 0;

    XPT2046_Scan(0);

    if(Xdown != 0 && Ydown != 0)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = Xdown;
        data->point.y = Ydown;
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

/* 初始化触摸驱动 */
void gui_touch_init(void)
{
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}
```

### 5.3 时钟驱动 (gui_driver.c)

```c
/* 获取系统tick */
static uint32_t gui_tick_get(void)
{
    return HAL_GetTick();
}

/* 初始化tick */
void gui_tick_init(void)
{
    lv_tick_set_cb(gui_tick_get);
}
```

### 5.4 主函数 (main.c)

```c
int main(void)
{
    HAL_Init();
    Stm32_Clock_Init(336, 8, 2, 7);
    delay_init();

    /* 硬件初始化 */
    LED_Init();
    BEEP_Init();
    KEY_Init();
    LCD_Init();
    Touch_Init();

    /* LVGL初始化 */
    lv_init();
    gui_tick_init();
    gui_disp_init();
    gui_touch_init();

    /* 创建测试UI */
    ui_main_create();

    /* 主循环 */
    while(1)
    {
        gui_app_task_handler();
        delay_ms(5);
    }
}
```

## 6. LVGL配置

### 6.1 关键配置项

```c
/* 启用ILI9341驱动 */
#define LV_USE_ILI9341          1

/* 启用通用MIPI LCD驱动 */
#define LV_USE_GENERIC_MIPI     1

/* 启用触摸输入 */
#define LV_USE_MOUSE            1

/* 显示缓冲区大小 */
#define LV_DEF_DRAW_BUF_SIZE   2400  /* 240*10 */

/* 启用中文字体支持 */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1

/* 启用动画 */
#define LV_USE_ANIM            1

/* 启用图片解码器 */
#define LV_USE_IMG_DECODER    1

/* 启用Flex布局 */
#define LV_USE_FLEX              1

/* 启用Grid布局 */
#define LV_USE_GRID              1
```

### 6.2 内存配置

```c
/* 堆大小 */
#define LV_MEM_SIZE            (48U * 1024U)    /* 48KB */

/* 显示缓冲区 */
#define LV_DEF_DRAW_BUF_SIZE   2400  /* 240*10 */
```

### 6.3 日志配置

```c
/*===========================================================================
 * 日志配置
 *===========================================================================*/

/**
 * @brief 启用日志输出
 *        0: 禁用 (节省代码空间)
 *        1: 启用 (调试时推荐)
 */
#define LV_USE_LOG             1

#if LV_USE_LOG
    /**
     * @brief 日志级别
     *        LV_LOG_LEVEL_TRACE: 最详细 (0)
     *        LV_LOG_LEVEL_INFO:  信息 (1)
     *        LV_LOG_LEVEL_WARN:  警告 (2)
     *        LV_LOG_LEVEL_ERROR: 错误 (3)
     *        LV_LOG_LEVEL_USER:  用户自定义 (4)
     *        LV_LOG_LEVEL_NONE:  禁用 (5)
     */
    #define LV_LOG_LEVEL        LV_LOG_LEVEL_TRACE

    /**
     * @brief 日志打印函数
     *        0: 使用LVGL内置打印
     *        1: 使用printf
     */
    #define LV_LOG_PRINTF       1

    /**
     * @brief 启用各模块日志
     */
    #define LV_LOG_TRACE_MEM        1   /* 内存分配 */
    #define LV_LOG_TRACE_TIMER      1   /* 定时器 */
    #define LV_LOG_TRACE_INDEV      1   /* 输入设备 */
    #define LV_LOG_TRACE_REFR       1   /* 刷新 */
    #define LV_LOG_TRACE_EVENT      1   /* 事件 */
    #define LV_LOG_TRACE_OBJ_CREATE 1   /* 对象创建 */
    #define LV_LOG_TRACE_LAYOUT     1   /* 布局 */
    #define LV_LOG_TRACE_ANIM       1   /* 动画 */
#endif
```

### 6.4 稳定性监控配置

```c
/*===========================================================================
 * 稳定性监控配置
 *===========================================================================*/

/**
 * @brief 启用栈溢出检测
 *        0: 禁用
 *        1: 启用
 */
#define GUI_USE_STACK_OVERFLOW_CHECK  1

/**
 * @brief 启用内存泄漏检测
 *        0: 禁用
 *        1: 启用
 */
#define GUI_USE_MEMORY_LEAK_CHECK    1

/**
 * @brief 栈溢出检测阈值 (字节)
 */
#define GUI_STACK_OVERFLOW_THRESHOLD  256

/**
 * @brief 内存使用警告阈值 (百分比)
 */
#define GUI_MEMORY_WARNING_THRESHOLD  0.8f

/**
 * @brief 内存使用危险阈值 (百分比)
 */
#define GUI_MEMORY_CRITICAL_THRESHOLD 0.9f

/**
 * @brief 栈溢出检测周期 (ms)
 */
#define GUI_STACK_CHECK_PERIOD       100

/**
 * @brief 内存检测周期 (ms)
 */
#define GUI_MEMORY_CHECK_PERIOD      1000
```

## 7. Keil工程配置

### 7.1 预处理宏

```
USE_HAL_DRIVER,STM32F407xx,LV_CONF_INCLUDE_SIMPLE
```

### 7.2 头文件路径

```
..\Common
..\Main
..\Startup_config
..\STM32F4xx_HAL_Driver\inc
..\USER\LED
..\USER\LCD
..\USER\BEEP
..\USER\KEY
..\USER\TOUCH
..\LVGL
..\LVGL\src
..\LVGL\src\drivers\display\ili9341
..\LVGL\src\drivers\display\lcd
..\GUI
..\UI
```

### 7.3 工程分组

```
TOUCH (Target 1)
├── User          # 用户代码 (main.c等)
├── StdPeriph     # HAL库
├── Startup       # 启动文件
├── LVGL          # LVGL库
├── GUI           # 驱动适配层
└── UI            # 界面层
```

### 7.4 编译选项

| 选项 | 设置 |
|------|------|
| **优化级别** | -O1 (Level 1) |
| **C99模式** | 启用 |
| **警告级别** | 默认 |

### 7.5 内存配置

| 区域 | 起始地址 | 大小 |
|------|----------|------|
| **IROM1** | 0x08000000 | 0x100000 (1MB) |
| **IRAM1** | 0x20000000 | 0x20000 (128KB) |
| **IRAM2** | 0x10000000 | 0x10000 (64KB) |

## 8. 测试用例

### 8.1 显示测试

| 测试项 | 测试内容 | 预期结果 |
|--------|----------|----------|
| 渐变测试 | 显示RGB渐变色 | 颜色过渡平滑 |
| 文字测试 | 显示中英文 | 文字清晰可读 |
| 图形测试 | 显示按钮、进度条 | 图形显示正确 |
| 动画测试 | 页面切换动画 | 动画流畅无卡顿 |

### 8.2 触摸测试

| 测试项 | 测试内容 | 预期结果 |
|--------|----------|----------|
| 滑动测试 | 左右滑动切换页面 | 响应<50ms，精度<5像素 |
| 点击测试 | 点击按钮 | 响应<50ms |
| 拖拽测试 | 拖拽滑块 | 跟手流畅 |

### 8.3 性能测试

| 测试项 | 测试内容 | 预期结果 |
|--------|----------|----------|
| 帧率测试 | 测量刷新帧率 | 60fps |
| 内存测试 | 监控内存使用 | <100KB |
| CPU测试 | 监控CPU使用 | <50% |

### 8.4 稳定性测试

| 测试项 | 测试内容 | 预期结果 |
|--------|----------|----------|
| 24小时测试 | 显示+触摸+动画循环 | 无崩溃、无内存泄漏 |

## 9. 风险和注意事项

### 9.1 技术风险

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| LVGL v9.x API变化 | 需要学习新API | 参考官方文档和示例 |
| FSMC时序问题 | LCD显示花屏 | 调整时序参数 |
| 触摸精度问题 | 触摸不准确 | 重新校准 |
| 内存不足 | 系统崩溃 | 优化内存使用 |
| 内存泄漏 | 系统不稳定 | 添加内存泄漏检测 |
| 栈溢出 | 系统崩溃 | 添加栈溢出检测 |
| 空指针访问 | HardFault | 检查所有指针 |
| 数组越界 | 内存损坏 | 检查数组边界 |
| 中断冲突 | 系统卡死 | 使用临界区保护 |
| FSMC时序错误 | 死机 | 使用保守时序 |

### 9.2 注意事项

1. **Flash空间** - 中文字库较大，需确保Flash空间足够
2. **RAM使用** - 显示缓冲占用约10KB，需监控堆栈使用
3. **触摸精度** - 电阻触摸屏精度有限，可能需要校准
4. **动画性能** - 高级动画需要足够的CPU和RAM资源

### 9.3 死机预防措施

**9.3.1 内存安全**

```c
/* 1. 检查内存分配 */
lv_obj_t *obj = lv_obj_create(parent);
if(obj == NULL) {
    printf("ERROR: Memory allocation failed!\n");
    return NULL;
}

/* 2. 监控内存使用 */
lv_mem_monitor_t mon;
lv_mem_monitor(&mon);
if(mon.used_pct > 80) {
    printf("WARNING: Memory usage high: %d%%\n", mon.used_pct);
}

/* 3. 使用内存池 */
static LV_MEM_DEFINE(my_mem, 48 * 1024);  /* 48KB内存池 */
```

**9.3.2 栈安全**

```c
/* 1. 增加栈大小 */
// Keil: Options → Linker → Stack Size = 0x1000 (4KB)

/* 2. 栈溢出检测 */
uint32_t get_stack_usage(void) {
    uint32_t sp;
    __asm volatile ("MRS %0, MSP" : "=r" (sp));
    extern uint32_t __initial_sp;
    return (uint32_t)&__initial_sp - sp;
}

/* 3. 避免大栈变量 */
/* 错误: uint8_t buffer[2048]; */
/* 正确: static uint8_t buffer[2048]; */
```

**9.3.3 指针安全**

```c
/* 1. 检查指针 */
void safe_function(void *ptr) {
    if(ptr == NULL) {
        printf("ERROR: NULL pointer!\n");
        return;
    }
    /* 使用ptr */
}

/* 2. 使用LVGL断言 */
LV_ASSERT_NULL(ptr);
LV_ASSERT(obj != NULL);
```

**9.3.4 中断安全**

```c
/* 1. 中断中只设置标志 */
volatile uint8_t flag = 0;

void EXTI0_IRQHandler(void) {
    flag = 1;  /* 只设置标志 */
}

void main_loop(void) {
    if(flag) {
        __disable_irq();
        flag = 0;
        __enable_irq();
        /* 处理事件 */
    }
}

/* 2. 使用临界区 */
void critical_section(void) {
    __disable_irq();
    /* 访问共享资源 */
    __enable_irq();
}
```

**9.3.5 时序安全**

```c
/* 1. 使用保守时序 */
void FSMC_Config_Safe(void) {
    /* 地址建立时间: 15个HCLK */
    /* 数据建立时间: 60个HCLK */
}

/* 2. 避免回调中延时 */
/* 错误: delay_ms(10); */
/* 正确: 不延时，立即返回 */
```

### 9.4 死机调试方法

**9.4.1 硬件调试**

```c
/* 1. LED指示 */
void error_handler(void) {
    LED0 = 0;  /* 点亮LED */
    while(1);  /* 停止 */
}

/* 2. 串口输出 */
void error_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
```

**9.4.2 软件调试**

```c
/* 1. 断点调试 */
// 在HardFault_Handler中设置断点

/* 2. 堆栈回溯 */
// 使用Keil的堆栈回溯功能

/* 3. 内存查看 */
// 使用Keil的内存查看器
```

**9.4.3 日志调试**

```c
/* 1. 启用LVGL日志 */
#define LV_USE_LOG  1
#define LV_LOG_LEVEL  LV_LOG_LEVEL_TRACE

/* 2. 自定义日志 */
void my_log(const char *fmt, ...) {
    printf("[LOG] %s\n", fmt);
}
```

### 9.5 死机预防检查清单

| 检查项 | 状态 | 说明 |
|--------|------|------|
| ✅ | 内存分配检查 | 检查所有lv_obj_create返回值 |
| ✅ | 栈溢出检测 | 增加栈大小，监控栈使用 |
| ✅ | 空指针检查 | 检查所有指针 |
| ✅ | 数组边界检查 | 检查数组索引 |
| ✅ | 中断安全 | 中断中只设置标志 |
| ✅ | 时序安全 | 使用保守时序 |
| ✅ | 回调安全 | 回调中不延时 |

### 9.6 关键代码模板

```c
/* 1. 安全对象创建 */
lv_obj_t* safe_lv_obj_create(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    if(obj == NULL) {
        printf("ERROR: Object creation failed!\n");
        return NULL;
    }
    return obj;
}

/* 2. 安全内存监控 */
void safe_memory_monitor(void) {
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    if(mon.used_pct > 80) {
        printf("WARNING: Memory usage high: %d%%\n", mon.used_pct);
    }
}

/* 3. 安全栈监控 */
void safe_stack_monitor(void) {
    uint32_t usage = get_stack_usage();
    if(usage > 768) {
        printf("WARNING: Stack usage high: %lu bytes\n", usage);
    }
}
```

## 10. 附录

### 10.1 参考资源

- [LVGL官方文档](https://docs.lvgl.io/9.5/)
- [LVGL GitHub](https://github.com/lvgl/lvgl)
- [LVGL Porting指南](https://docs.lvgl.io/9.5/porting/index.html)

### 10.2 LVGL版本

- LVGL版本: v9.5.0
- 内置驱动: ILI9341官方驱动

---

**文档版本:** v1.0
**创建日期:** 2026-07-28
**作者:** Claude
