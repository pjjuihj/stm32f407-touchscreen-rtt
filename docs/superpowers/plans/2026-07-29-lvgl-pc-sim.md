# LVGL PC 模拟环境实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 搭建 LVGL 的 PC 模拟环境，使用 SDL2 作为显示后端，实现 UI 代码的跨平台共享。

**Architecture:** 创建共享 UI 层 (`ui/`) 包含平台无关的 UI 逻辑，PC 专用层 (`pc_sim/`) 包含 SDL2 驱动和主循环，STM32 侧修改 `main.c` 调用共享 UI。LVGL 源码和配置文件保持共享。

**Tech Stack:** LVGL v9.5.0, SDL2, CMake, GCC (PC) / arm-none-eabi-gcc (STM32)

## Global Constraints

- LVGL 版本: v9.5.0 (已存在于 `LVGL/` 目录)
- 配置文件: `lv_conf.h` 保持一份，PC 和 STM32 共用
- 分辨率: 240x320 (匹配 ILI9341 硬件)
- UI 代码: 只依赖 LVGL API，不依赖任何硬件
- 构建系统: Keil 继续用于 STM32，CMake 用于 PC 模拟

---

## 文件结构

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
└── Main/
    └── main.c             ← 修改：调用 ui_init()
```

---

### Task 1: 创建共享 UI 层头文件

**Files:**
- Create: `ui/ui.h`

**Interfaces:**
- Produces: `void ui_init(void)` — 平台无关的 UI 初始化函数

- [ ] **Step 1: 创建 ui 目录**

```bash
mkdir -p ui
```

- [ ] **Step 2: 创建 ui/ui.h 头文件**

```c
#ifndef UI_H
#define UI_H

#include "lvgl.h"

/**
 * @brief 初始化 UI 界面
 * 
 * 创建所有 UI 控件，设置事件回调。
 * 此函数是平台无关的，PC 和 STM32 共享。
 */
void ui_init(void);

#endif /* UI_H */
```

- [ ] **Step 3: 验证头文件语法**

```bash
gcc -fsyntax-only -I LVGL ui/ui.h
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui.h
git commit -m "feat: add shared UI header"
```

---

### Task 2: 创建共享 UI 实现

**Files:**
- Create: `ui/ui_main.c`

**Interfaces:**
- Consumes: `ui/ui.h`
- Produces: `ui_init()` 函数实现，包含测试按钮 UI

- [ ] **Step 1: 创建 ui/ui_main.c**

```c
#include "ui.h"

/**
 * @brief 按钮点击事件回调
 * 
 * 在 STM32 上会调用 LED0_Toggle()，PC 上只打印日志。
 * 当前版本只打印日志，硬件操作在后续版本中添加。
 */
static void btn_event_cb(lv_event_t * e)
{
    LV_LOG_USER("Button clicked!");
    /* STM32 上会调用 LED0_Toggle()，PC 上只打印日志 */
}

/**
 * @brief 初始化 UI 界面
 * 
 * 从 Main/main.c 抽取的 UI 创建代码。
 * 创建标题标签和两个按钮（Home 和 Settings）。
 */
void ui_init(void)
{
    /* 获取默认屏幕 */
    lv_obj_t * scr = lv_screen_active();
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建标题标签 */
    lv_obj_t *title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "LVGL Image Demo");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建带图标的按钮1 - Home */
    lv_obj_t *btn1 = lv_button_create(scr);
    if(btn1 != NULL)
    {
        lv_obj_set_size(btn1, 120, 50);
        lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);
        lv_obj_add_event_cb(btn1, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label1 = lv_label_create(btn1);
        if(label1 != NULL)
        {
            lv_label_set_text(label1, LV_SYMBOL_HOME " Home");
            lv_obj_center(label1);
        }
    }

    /* 创建带图标的按钮2 - Settings */
    lv_obj_t *btn2 = lv_button_create(scr);
    if(btn2 != NULL)
    {
        lv_obj_set_size(btn2, 120, 50);
        lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
        lv_obj_add_event_cb(btn2, btn_event_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label2 = lv_label_create(btn2);
        if(label2 != NULL)
        {
            lv_label_set_text(label2, LV_SYMBOL_SETTINGS " Settings");
            lv_obj_center(label2);
        }
    }
}
```

- [ ] **Step 2: 验证编译（语法检查）**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_main.c
```

Expected: 无错误输出（可能有未定义的警告，但语法正确）

- [ ] **Step 3: 提交**

```bash
git add ui/ui_main.c
git commit -m "feat: add shared UI implementation with test buttons"
```

---

### Task 3: 创建 SDL2 驱动层

**Files:**
- Create: `pc_sim/sdl_driver.h`
- Create: `pc_sim/sdl_driver.c`

**Interfaces:**
- Produces: `void sdl_driver_init(void)` — 初始化 SDL2 和 LVGL 输入设备

- [ ] **Step 1: 创建 pc 目录**

```bash
mkdir -p pc_sim
```

- [ ] **Step 2: 创建 pc_sim/sdl_driver.h**

```c
#ifndef SDL_DRIVER_H
#define SDL_DRIVER_H

/**
 * @brief 初始化 SDL2 驱动和 LVGL 输入设备
 * 
 * 创建 SDL2 窗口、显示驱动、鼠标、鼠标滚轮和键盘输入设备。
 */
void sdl_driver_init(void);

#endif /* SDL_DRIVER_H */
```

- [ ] **Step 3: 创建 pc_sim/sdl_driver.c**

```c
#include "lvgl.h"
#include "sdl_driver.h"

#define SDL_MAIN_HANDLED  /* 修复 SDL 的 "undefined reference to WinMain" 问题 */
#include <SDL2/SDL.h>

/* LVGL SDL 驱动头文件 */
#include "drivers/sdl/lv_sdl_window.h"
#include "drivers/sdl/lv_sdl_mouse.h"
#include "drivers/sdl/lv_sdl_mousewheel.h"
#include "drivers/sdl/lv_sdl_keyboard.h"

/* 静态变量 */
static lv_display_t * disp = NULL;
static lv_indev_t * mouse = NULL;
static lv_indev_t * mousewheel = NULL;
static lv_indev_t * keyboard = NULL;

void sdl_driver_init(void)
{
    /* 初始化 SDL 视频子系统 */
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LV_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return;
    }

    /* 创建 LVGL 显示窗口（匹配硬件分辨率 240x320） */
    disp = lv_sdl_window_create(240, 320);
    if (disp == NULL) {
        LV_LOG_ERROR("lv_sdl_window_create failed");
        return;
    }
    lv_sdl_window_set_title(disp, "LVGL PC Sim");

    /* 创建鼠标输入设备 */
    mouse = lv_sdl_mouse_create();
    if (mouse == NULL) {
        LV_LOG_ERROR("lv_sdl_mouse_create failed");
    }

    /* 创建鼠标滚轮输入设备 */
    mousewheel = lv_sdl_mousewheel_create();
    if (mousewheel == NULL) {
        LV_LOG_ERROR("lv_sdl_mousewheel_create failed");
    }

    /* 创建键盘输入设备 */
    keyboard = lv_sdl_keyboard_create();
    if (keyboard == NULL) {
        LV_LOG_ERROR("lv_sdl_keyboard_create failed");
    }

    LV_LOG_USER("SDL2 driver initialized");
}
```

- [ ] **Step 4: 验证头文件语法**

```bash
gcc -fsyntax-only -I LVGL pc_sim/sdl_driver.h
```

Expected: 无错误输出

- [ ] **Step 5: 提交**

```bash
git add pc_sim/sdl_driver.h pc_sim/sdl_driver.c
git commit -m "feat: add SDL2 driver for PC simulation"
```

---

### Task 4: 创建 PC 模拟主程序

**Files:**
- Create: `pc_sim/main.c`

**Interfaces:**
- Consumes: `sdl_driver_init()` from Task 3
- Consumes: `ui_init()` from Task 2

- [ ] **Step 1: 创建 pc_sim/main.c**

```c
/**
 * @file main.c
 * @brief LVGL PC 模拟主程序
 * 
 * 使用 SDL2 作为显示后端，在 PC 上运行 LVGL UI。
 */

#define SDL_MAIN_HANDLED  /* 修复 SDL 的 "undefined reference to WinMain" 问题 */
#include <SDL2/SDL.h>

#include "lvgl.h"
#include "sdl_driver.h"
#include "ui.h"

int main(void)
{
    /* 初始化 LVGL */
    lv_init();
    LV_LOG_USER("LVGL initialized");

    /* 初始化 SDL2 驱动 */
    sdl_driver_init();

    /* 初始化 UI（共享代码） */
    ui_init();
    LV_LOG_USER("UI initialized");

    /* 强制刷新一次屏幕 */
    lv_refr_now(NULL);

    /* 主循环 */
    LV_LOG_USER("Starting main loop...");
    while (1) {
        lv_timer_handler();
        SDL_Delay(5);  /* 5ms，匹配 STM32 的 delay_ms(5) */
    }

    return 0;
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . pc_sim/main.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add pc_sim/main.c
git commit -m "feat: add PC simulation main entry point"
```

---

### Task 5: 创建 CMake 构建配置

**Files:**
- Create: `pc_sim/CMakeLists.txt`

**Interfaces:**
- Consumes: LVGL 源码 (`LVGL/`)
- Consumes: `lv_conf.h`
- Consumes: `ui/ui_main.c` from Task 2
- Consumes: `pc_sim/sdl_driver.c` from Task 3
- Consumes: `pc_sim/main.c` from Task 4

- [ ] **Step 1: 创建 pc_sim/CMakeLists.txt**

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

- [ ] **Step 2: 验证 CMake 配置**

```bash
cd pc_sim
mkdir -p build && cd build
cmake .. 2>&1 | head -20
```

Expected: CMake 配置成功，无错误（可能有警告）

- [ ] **Step 3: 提交**

```bash
git add pc_sim/CMakeLists.txt
git commit -m "feat: add CMake build configuration for PC simulation"
```

---

### Task 6: 安装 SDL2 依赖并构建测试

**Files:**
- Modify: 无（依赖安装）

**Interfaces:**
- 无

- [ ] **Step 1: 安装 SDL2（Windows with vcpkg）**

```bash
# 如果已安装 vcpkg
vcpkg install sdl2

# 或者手动下载 SDL2 开发库
# https://www.libsdl.org/download-2.0.php
# 下载 SDL2-devel-2.x.x-VC.zip
```

- [ ] **Step 2: 安装 SDL2（Windows with MSYS2）**

```bash
# 如果使用 MSYS2
pacman -S mingw-w64-x86_64-SDL2
```

- [ ] **Step 3: 安装 SDL2（Linux）**

```bash
sudo apt install libsdl2-dev
```

- [ ] **Step 4: 构建 PC 模拟**

```bash
cd pc_sim
rm -rf build
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/path/to/sdl2
cmake --build .
```

Expected: 构建成功，生成 `lvgl_pc_sim` 可执行文件

- [ ] **Step 5: 运行测试**

```bash
./lvgl_pc_sim
```

Expected: 
- 弹出 240x320 的窗口
- 白色背景
- 显示 "LVGL Image Demo" 标题
- 显示两个按钮："Home" 和 "Settings"
- 可以用鼠标点击按钮

- [ ] **Step 6: 提交（如果有配置调整）**

```bash
git add pc_sim/
git commit -m "feat: PC simulation build working with SDL2"
```

---

### Task 7: 修改 STM32 main.c 使用共享 UI

**Files:**
- Modify: `Main/main.c:178-277`

**Interfaces:**
- Consumes: `ui_init()` from Task 2

- [ ] **Step 1: 修改 Main/main.c**

在文件开头添加 `#include "ui.h"`：

```c
#include "led.h"
#include "beep.h"
#include "key.h"
#include "lcd.h"
#include "touch.h"
#include "usart.h"
#include "log.h"
#include "test_log.h"
#include "gui_driver.h"     /* 新增: LVGL驱动接口 */
#include "lvgl.h"           /* 新增: LVGL头文件 */
#include "ui.h"             /* 新增: 共享UI接口 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
```

删除原来的 `btn_event_cb` 函数（第 178-184 行）：

```c
/* 删除这段代码 */
/* 按钮点击事件回调 - LED闪烁 */
static void btn_event_cb(lv_event_t *e)
{
	LED0 = 0;   /* 亮 */
	delay_ms(100);
	LED0 = 1;   /* 灭 */
}
```

修改 `main()` 函数中的 LVGL 初始化部分（第 214-270 行）：

```c
	/* LVGL初始化 */
	lv_init();
	gui_log_init();
	gui_tick_init();
	gui_disp_init();
	gui_touch_init();
	Log_Write(LOG_MODULE_SYSTEM, LOG_LEVEL_INFO, "LVGL initialized");

	/* 使用共享UI代码（替换原来的UI创建代码） */
	ui_init();

	/* 强制刷新整个屏幕 */
	lv_refr_now(NULL);
```

- [ ] **Step 2: 验证语法**

```bash
# 如果有 arm-none-eabi-gcc
arm-none-eabi-gcc -fsyntax-only -I LVGL -I GUI -I Common -I USER/LED -I USER/BEEP -I USER/KEY -I USER/LCD -I USER/TOUCH -I USER/USART -I USER/LOG -I TEST Main/main.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add Main/main.c
git commit -m "refactor: use shared UI code in STM32 main"
```

---

### Task 8: 端到端验证

**Files:**
- 无（验证步骤）

**Interfaces:**
- 无

- [ ] **Step 1: PC 模拟验证**

```bash
cd pc_sim/build
./lvgl_pc_sim
```

验证清单：
- [ ] 窗口标题显示 "LVGL PC Sim"
- [ ] 窗口大小为 240x320
- [ ] 背景为白色
- [ ] 显示 "LVGL Image Demo" 标题
- [ ] 显示 "Home" 按钮（带房子图标）
- [ ] 显示 "Settings" 按钮（带齿轮图标）
- [ ] 点击按钮控制台输出 "Button clicked!"
- [ ] 鼠标可以悬停在按钮上

- [ ] **Step 2: STM32 硬件验证**

1. 使用 Keil 或 CMake 编译 STM32 固件
2. 烧录到 STM32F407 开发板
3. 验证清单：
   - [ ] LCD 显示相同的 UI（标题 + 两个按钮）
   - [ ] 触摸 "Home" 按钮 LED0 闪烁
   - [ ] 触摸 "Settings" 按钮 LED0 闪烁

- [ ] **Step 3: 一致性检查**

比较 PC 和 STM32 上的 UI：
- [ ] 按钮位置相同
- [ ] 按钮大小相同
- [ ] 按钮颜色相同
- [ ] 标题文字相同

- [ ] **Step 4: 最终提交**

```bash
git add -A
git commit -m "feat: LVGL PC simulation environment complete

- Added shared UI layer (ui/)
- Added SDL2 driver for PC simulation (pc_sim/)
- Modified STM32 main to use shared UI
- Verified PC and STM32 consistency"
```

---

## 验证清单

完成所有任务后，检查以下内容：

- [ ] PC 上能正常运行 LVGL 模拟器
- [ ] 窗口显示正确的 UI 布局
- [ ] 鼠标可以点击按钮
- [ ] STM32 上功能与 PC 一致
- [ ] UI 代码无平台特定的 #ifdef
- [ ] 所有文件已提交到 git
