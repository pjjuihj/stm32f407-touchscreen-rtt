# LVGL PC 模拟环境设计

## 概述

搭建 LVGL 的 PC 模拟环境，使用 SDL2 作为显示后端，实现 UI 代码的跨平台共享。PC 上快速迭代 UI 设计，无需每次烧录硬件。

## 目标

- 加速 UI 开发：PC 上快速迭代，无需烧录
- 代码共享：UI 代码只写一次，PC 和 STM32 共享
- 验证优先：先在 PC 上验证 UI 布局，确认后同步到硬件

## 非目标

- 不替换 Keil 构建系统，Keil 继续用于 STM32 开发
- 不支持 SDL3（LVGL v9.5.0 不支持）
- 不实现完整的硬件仿真（如 ADC、DAC 等）

## 目录结构

```
触摸屏_usart/
├── LVGL/                  ← 共享，不动
├── lv_conf.h              ← 共享，不动
├── GUI/                   ← 硬件驱动层（STM32 专用）
│   └── gui_driver.c       ← 原有，不动
├── ui/                    ← 新建，共享 UI 代码
│   ├── ui.h               ← UI 初始化接口
│   └── ui_main.c          ← 测试按钮等 UI 逻辑（从 main.c 抽取）
├── pc_sim/                ← 新建，PC 模拟专用
│   ├── CMakeLists.txt     ← PC 构建配置
│   ├── main.c             ← SDL2 事件循环
│   ├── sdl_driver.h       ← SDL2 驱动接口
│   └── sdl_driver.c       ← LVGL 的 SDL2 驱动初始化
└── ...（原有 STM32 文件不变）
```

## HAL 抽象层

### ui/ui.h

```c
#ifndef UI_H
#define UI_H

#include "lvgl.h"

// 平台无关的 UI 初始化
void ui_init(void);

#endif
```

### ui/ui_main.c

```c
#include "ui.h"

static void btn_event_cb(lv_event_t * e);

void ui_init(void)
{
    // 获取默认屏幕
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_white(), 0);

    // 创建测试按钮（从 main.c 抽取）
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_center(btn);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t * label = lv_label_create(btn);
    lv_label_set_text(label, "Click Me!");
    lv_obj_center(label);
}

static void btn_event_cb(lv_event_t * e)
{
    LV_LOG_USER("Button clicked!");
    // STM32 上会调用 LED0_Toggle()，PC 上只打印日志
}
```

**设计决策**：
- UI 代码只依赖 LVGL API，不依赖任何硬件
- 硬件操作（如 LED 控制）通过回调或事件处理，不在 UI 层直接调用
- PC 和 STM32 共享同一份 `ui_main.c`

## SDL2 驱动层

### pc_sim/sdl_driver.h

```c
#ifndef SDL_DRIVER_H
#define SDL_DRIVER_H

// 初始化 SDL2 驱动和 LVGL 输入设备
void sdl_driver_init(void);

#endif
```

### pc_sim/sdl_driver.c

```c
#include "lvgl.h"

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

static lv_display_t * disp;
static lv_indev_t * mouse;
static lv_indev_t * mousewheel;
static lv_indev_t * keyboard;

void sdl_driver_init(void)
{
    // 初始化 SDL
    SDL_Init(SDL_INIT_VIDEO);

    // 创建 LVGL 显示（使用 LVGL 内置 SDL 驱动）
    disp = lv_sdl_window_create(240, 320);  // 匹配硬件分辨率
    lv_sdl_window_set_title(disp, "LVGL PC Sim");

    // 创建输入设备
    mouse = lv_sdl_mouse_create();
    mousewheel = lv_sdl_mousewheel_create();
    keyboard = lv_sdl_keyboard_create();
}
```

### pc_sim/main.c

```c
#define SDL_MAIN_HANDLED
#include "lvgl.h"
#include "sdl_driver.h"
#include "ui.h"

int main(void)
{
    // 初始化 LVGL
    lv_init();

    // 初始化 SDL2 驱动
    sdl_driver_init();

    // 初始化 UI（共享代码）
    ui_init();

    // 主循环
    while (1) {
        lv_timer_handler();
        SDL_Delay(5);  // 5ms，匹配 STM32 的 delay_ms(5)
    }

    return 0;
}
```

**设计决策**：
- 使用 LVGL 内置的 SDL 驱动（`lv_sdl_window_create` 等）
- 分辨率设为 240x320，匹配硬件
- 主循环结构与 STM32 版本一致

## CMake 构建配置

### pc_sim/CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(lvgl_pc_sim C)

set(CMAKE_C_STANDARD 11)

# 查找 SDL2
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# 包含 LVGL 源码（复用父目录的 LVGL）
set(LVGL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL)
add_subdirectory(${LVGL_DIR} ${CMAKE_BINARY_DIR}/lvgl)

# 禁用不需要的 LVGL 功能
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../lv_conf.h CACHE STRING "" FORCE)
set(LV_USE_DEMOWidgets OFF CACHE BOOL "" FORCE)
set(LV_USE_DEMOBENCHMARKS OFF CACHE BOOL "" FORCE)

# PC 模拟源文件
add_executable(lvgl_pc_sim
    main.c
    sdl_driver.c
)

# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
)

# 包含目录
target_include_directories(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../LVGL
    ${CMAKE_CURRENT_SOURCE_DIR}/../
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui
    ${SDL2_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(lvgl_pc_sim
    lvgl
    ${SDL2_LIBRARIES}
)

# 编译选项
target_compile_options(lvgl_pc_sim PRIVATE ${SDL2_CFLAGS_OTHER})
```

### 构建命令

```bash
cd pc_sim
mkdir build && cd build
cmake ..
cmake --build .
```

**设计决策**：
- 复用父目录的 `LVGL/` 和 `lv_conf.h`
- 通过 PkgConfig 查找 SDL2
- 共享 UI 代码通过相对路径引用

## STM32 侧的调整

修改 STM32 的 `main.c`，将 UI 创建逻辑委托给共享的 `ui_init()`：

```c
#include "ui.h"  // 新增

int main(void)
{
    // ... 原有初始化代码 ...
    
    // LVGL 初始化
    lv_init();
    gui_log_init();
    gui_tick_init();
    gui_disp_init();
    gui_touch_init();

    // 使用共享 UI 代码（替换原来的测试按钮代码）
    ui_init();

    lv_refr_now(NULL);

    while (1) {
        lv_task_handler();
        delay_ms(5);
    }
}
```

**设计决策**：
- STM32 的 `main.c` 不再直接创建 UI 控件
- UI 逻辑完全由 `ui/ui_main.c` 提供
- 硬件初始化（LCD、触摸）仍由 STM32 的 `gui_driver.c` 负责

## 依赖关系

### PC 模拟依赖

- SDL2（通过 vcpkg 或系统包管理器安装）
- CMake 3.10+
- GCC 或 MSVC

### STM32 依赖

- 无新增依赖，使用现有 Keil 或 CMake 构建

## 测试与验证

### 验证步骤

1. **PC 模拟验证**：
   ```bash
   cd pc_sim/build
   ./lvgl_pc_sim
   ```
   - 应看到 240x320 的窗口
   - 白色背景，居中的 "Click Me!" 按钮
   - 点击按钮应在控制台输出日志

2. **STM32 硬件验证**：
   - 编译并烧录到 STM32
   - 确认 LCD 显示相同的按钮
   - 确认触摸按钮能触发 LED0 闪烁

3. **一致性检查**：
   - PC 和 STM32 上的 UI 外观应完全一致
   - 按钮位置、大小、颜色应相同

### 通过标准

- PC 上能正常显示和交互
- STM32 上功能与 PC 一致
- UI 代码无平台特定的 #ifdef

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| SDL2 安装复杂 | 提供 vcpkg 和手动安装两种方式 |
| lv_conf.h 配置冲突 | 保持一份配置，PC 和 STM32 共用 |
| UI 代码意外依赖硬件 | 代码审查，确保只使用 LVGL API |
| 分辨率不匹配 | PC 和 STM32 都使用 240x320 |

## 未来扩展

- 添加更多 UI 页面（主界面、设置等）
- 支持触摸模拟（鼠标拖拽）
- 添加 LVGL demo 作为参考
- 支持热重载（修改 UI 代码后自动刷新）
