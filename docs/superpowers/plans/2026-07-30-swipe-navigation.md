# LVGL 滑动导航实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展现有的 LVGL 控件演示 UI，添加滑动导航功能。使用 lv_tileview 实现子页面之间的左右滑动切换，支持循环顺序。

**Architecture:** 使用 lv_tileview 创建全屏滑动容器，每个子页面作为一个 tile。主菜单保持不变，点击按钮进入对应的 tile 页面。支持左右滑动切换，循环顺序。

**Tech Stack:** LVGL v9.5.0, SDL2 (PC), CMake, GCC (PC) / arm-none-eabi-gcc (STM32)

## Global Constraints

- LVGL 版本: v9.5.0 (已存在于 LVGL/ 目录)
- 配置文件: lv_conf.h 保持一份，PC 和 STM32 共用
- 分辨率: 240x320 (匹配 ILI9341 硬件)
- UI 代码: 只依赖 LVGL API，不依赖任何硬件
- 导航: 使用 lv_tileview 实现滑动切换

---

## 文件结构

```
ui/
├── ui.h                    ← UI 初始化接口（修改）
├── ui_main.c               ← 主菜单页面（修改）
├── ui_swipe.c              ← 滑动导航管理（新建）
├── ui_swipe.h              ← 滑动导航头文件（新建）
├── ui_input.c              ← 输入控件页面（修改）
├── ui_display.c            ← 显示控件页面（修改）
├── ui_data.c               ← 数据控件页面（修改）
└── ui_selection.c          ← 选择控件页面（修改）
```

---

### Task 1: 创建滑动导航头文件

**Files:**
- Create: `ui/ui_swipe.h`

**Interfaces:**
- Produces: 滑动导航 API 声明和页面索引定义

- [ ] **Step 1: 创建 ui/ui_swipe.h**

```c
#ifndef UI_SWIPE_H
#define UI_SWIPE_H

#include "lvgl.h"

/* 页面索引定义 */
#define SWIPE_PAGE_INPUT      0
#define SWIPE_PAGE_DISPLAY    1
#define SWIPE_PAGE_DATA       2
#define SWIPE_PAGE_SELECTION  3
#define SWIPE_PAGE_COUNT      4

/**
 * @brief 创建并初始化滑动导航
 * 
 * 创建 lv_tileview，添加 4 个 tile，每个 tile 包含对应页面的内容。
 */
void ui_swipe_init(void);

/**
 * @brief 切换到指定页面
 * @param page_index 目标页面索引 (0-3)
 */
void ui_swipe_goto(uint8_t page_index);

/**
 * @brief 获取当前页面索引
 * @return 当前页面索引 (0-3)
 */
uint8_t ui_swipe_get_current(void);

/**
 * @brief 切换到主菜单页面
 */
void ui_swipe_goto_main(void);

#endif /* UI_SWIPE_H */
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL ui/ui_swipe.h
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_swipe.h
git commit -m "feat: add swipe navigation header"
```

---

### Task 2: 创建滑动导航实现

**Files:**
- Create: `ui/ui_swipe.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- Consumes: `ui/ui.h` 中的页面创建函数
- Produces: `ui_swipe_init()`, `ui_swipe_goto()`, `ui_swipe_get_current()`, `ui_swipe_goto_main()` 函数

- [ ] **Step 1: 创建 ui/ui_swipe.c**

```c
#include "ui.h"
#include "ui_swipe.h"

/* 静态变量 */
static lv_obj_t * tileview = NULL;
static lv_obj_t * tiles[SWIPE_PAGE_COUNT] = {NULL};
static uint8_t current_page = SWIPE_PAGE_INPUT;

/* 页面创建函数声明 */
extern lv_obj_t * ui_input_create(lv_obj_t * parent);
extern lv_obj_t * ui_display_create(lv_obj_t * parent);
extern lv_obj_t * ui_data_create(lv_obj_t * parent);
extern lv_obj_t * ui_selection_create(lv_obj_t * parent);

/**
 * @brief 创建并初始化滑动导航
 */
void ui_swipe_init(void)
{
    /* 创建 tileview */
    tileview = lv_tileview_create(lv_screen_active());
    if(tileview == NULL)
    {
        LV_LOG_ERROR("Failed to create tileview");
        return;
    }
    
    /* 设置 tileview 全屏 */
    lv_obj_set_size(tileview, LV_PCT(100), LV_PCT(100));
    
    /* 添加 Input tile (0, 0) - 支持左滑 */
    tiles[SWIPE_PAGE_INPUT] = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_LEFT);
    if(tiles[SWIPE_PAGE_INPUT] != NULL)
    {
        ui_input_create(tiles[SWIPE_PAGE_INPUT]);
    }
    
    /* 添加 Display tile (1, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DISPLAY] = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DISPLAY] != NULL)
    {
        ui_display_create(tiles[SWIPE_PAGE_DISPLAY]);
    }
    
    /* 添加 Data tile (2, 0) - 支持左右滑 */
    tiles[SWIPE_PAGE_DATA] = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_LEFT | LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_DATA] != NULL)
    {
        ui_data_create(tiles[SWIPE_PAGE_DATA]);
    }
    
    /* 添加 Selection tile (3, 0) - 支持右滑（循环） */
    tiles[SWIPE_PAGE_SELECTION] = lv_tileview_add_tile(tileview, 3, 0, LV_DIR_RIGHT);
    if(tiles[SWIPE_PAGE_SELECTION] != NULL)
    {
        ui_selection_create(tiles[SWIPE_PAGE_SELECTION]);
    }
    
    /* 设置初始页面 */
    lv_obj_set_tile(tileview, tiles[SWIPE_PAGE_INPUT], LV_ANIM_OFF);
    current_page = SWIPE_PAGE_INPUT;
    
    LV_LOG_USER("Swipe navigation initialized");
}

/**
 * @brief 切换到指定页面
 */
void ui_swipe_goto(uint8_t page_index)
{
    if(page_index < SWIPE_PAGE_COUNT && tiles[page_index] != NULL)
    {
        lv_obj_set_tile(tileview, tiles[page_index], LV_ANIM_ON);
        current_page = page_index;
        LV_LOG_USER("Navigated to page %d", page_index);
    }
}

/**
 * @brief 获取当前页面索引
 */
uint8_t ui_swipe_get_current(void)
{
    return current_page;
}

/**
 * @brief 切换到主菜单页面
 */
void ui_swipe_goto_main(void)
{
    LV_LOG_USER("Navigating to main menu");
    lv_screen_load(g_main_menu_page);
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_swipe.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_swipe.c
git commit -m "feat: add swipe navigation implementation"
```

---

### Task 3: 修改输入控件页面

**Files:**
- Modify: `ui/ui_input.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- 修改: `ui_input_create()` 函数签名，添加 parent 参数

- [ ] **Step 1: 读取现有 ui/ui_input.c**

读取 `ui/ui_input.c` 文件，了解当前实现。

- [ ] **Step 2: 修改 ui_input_create() 函数**

1. 修改函数签名：`lv_obj_t * ui_input_create(void)` → `lv_obj_t * ui_input_create(lv_obj_t * parent)`
2. 修改屏幕创建：`lv_obj_create(NULL)` → `lv_obj_create(parent)`
3. 移除返回按钮（由 tileview 的滑动功能替代）
4. 调整布局以适应 tileview

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_input.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_input.c
git commit -m "refactor: modify input page for tileview"
```

---

### Task 4: 修改显示控件页面

**Files:**
- Modify: `ui/ui_display.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- 修改: `ui_display_create()` 函数签名，添加 parent 参数

- [ ] **Step 1: 读取现有 ui/ui_display.c**

读取 `ui/ui_display.c` 文件，了解当前实现。

- [ ] **Step 2: 修改 ui_display_create() 函数**

1. 修改函数签名：`lv_obj_t * ui_display_create(void)` → `lv_obj_t * ui_display_create(lv_obj_t * parent)`
2. 修改屏幕创建：`lv_obj_create(NULL)` → `lv_obj_create(parent)`
3. 移除返回按钮（由 tileview 的滑动功能替代）
4. 调整布局以适应 tileview

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_display.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_display.c
git commit -m "refactor: modify display page for tileview"
```

---

### Task 5: 修改数据控件页面

**Files:**
- Modify: `ui/ui_data.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- 修改: `ui_data_create()` 函数签名，添加 parent 参数

- [ ] **Step 1: 读取现有 ui/ui_data.c**

读取 `ui/ui_data.c` 文件，了解当前实现。

- [ ] **Step 2: 修改 ui_data_create() 函数**

1. 修改函数签名：`lv_obj_t * ui_data_create(void)` → `lv_obj_t * ui_data_create(lv_obj_t * parent)`
2. 修改屏幕创建：`lv_obj_create(NULL)` → `lv_obj_create(parent)`
3. 移除返回按钮（由 tileview 的滑动功能替代）
4. 调整布局以适应 tileview

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_data.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_data.c
git commit -m "refactor: modify data page for tileview"
```

---

### Task 6: 修改选择控件页面

**Files:**
- Modify: `ui/ui_selection.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- 修改: `ui_selection_create()` 函数签名，添加 parent 参数

- [ ] **Step 1: 读取现有 ui/ui_selection.c**

读取 `ui/ui_selection.c` 文件，了解当前实现。

- [ ] **Step 2: 修改 ui_selection_create() 函数**

1. 修改函数签名：`lv_obj_t * ui_selection_create(void)` → `lv_obj_t * ui_selection_create(lv_obj_t * parent)`
2. 修改屏幕创建：`lv_obj_create(NULL)` → `lv_obj_create(parent)`
3. 移除返回按钮（由 tileview 的滑动功能替代）
4. 调整布局以适应 tileview

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_selection.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_selection.c
git commit -m "refactor: modify selection page for tileview"
```

---

### Task 7: 更新 UI 头文件接口

**Files:**
- Modify: `ui/ui.h`

**Interfaces:**
- 修改: 页面创建函数声明，添加 parent 参数
- 添加: `ui_swipe.h` 包含

- [ ] **Step 1: 读取现有 ui/ui.h**

读取 `ui/ui.h` 文件，了解当前接口。

- [ ] **Step 2: 修改页面创建函数声明**

1. 修改 `ui_input_create()` 声明：添加 `lv_obj_t * parent` 参数
2. 修改 `ui_display_create()` 声明：添加 `lv_obj_t * parent` 参数
3. 修改 `ui_data_create()` 声明：添加 `lv_obj_t * parent` 参数
4. 修改 `ui_selection_create()` 声明：添加 `lv_obj_t * parent` 参数
5. 添加 `#include "ui_swipe.h"`

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL ui/ui.h
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui.h
git commit -m "refactor: update UI header for tileview"
```

---

### Task 8: 修改主菜单页面

**Files:**
- Modify: `ui/ui_main.c`

**Interfaces:**
- Consumes: `ui/ui_swipe.h` 中的接口
- 修改: 按钮回调函数，调用 `ui_swipe_goto()` 切换到对应的 tile

- [ ] **Step 1: 读取现有 ui/ui_main.c**

读取 `ui/ui_main.c` 文件，了解当前实现。

- [ ] **Step 2: 修改按钮回调函数**

1. 修改 `btn_input_cb()`：调用 `ui_swipe_goto(SWIPE_PAGE_INPUT)`
2. 修改 `btn_display_cb()`：调用 `ui_swipe_goto(SWIPE_PAGE_DISPLAY)`
3. 修改 `btn_data_cb()`：调用 `ui_swipe_goto(SWIPE_PAGE_DATA)`
4. 修改 `btn_selection_cb()`：调用 `ui_swipe_goto(SWIPE_PAGE_SELECTION)`

- [ ] **Step 3: 修改 ui_init() 函数**

1. 调用 `ui_swipe_init()` 初始化滑动导航
2. 加载主菜单页面

- [ ] **Step 4: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_main.c
```

Expected: 无错误输出

- [ ] **Step 5: 提交**

```bash
git add ui/ui_main.c
git commit -m "refactor: integrate swipe navigation with main menu"
```

---

### Task 9: 更新 CMake 构建配置

**Files:**
- Modify: `pc_sim/CMakeLists.txt`

**Interfaces:**
- Consumes: 新创建的 ui_swipe.c 文件

- [ ] **Step 1: 读取现有 pc_sim/CMakeLists.txt**

读取 `pc_sim/CMakeLists.txt` 文件，了解当前配置。

- [ ] **Step 2: 添加 ui_swipe.c 到构建配置**

在 `target_sources` 部分添加 `ui_swipe.c`：

```cmake
# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_swipe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_input.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_display.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_data.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_selection.c
)
```

- [ ] **Step 3: 验证 CMake 配置**

```bash
cd pc_sim
rm -rf build
mkdir build && cd build
cmake .. -G "Ninja" -DCMAKE_PREFIX_PATH="C:/Users/CMJ/mingw64_bin"
```

Expected: CMake 配置成功

- [ ] **Step 4: 提交**

```bash
git add pc_sim/CMakeLists.txt
git commit -m "build: add ui_swipe.c to CMake configuration"
```

---

### Task 10: 构建测试

**Files:**
- 无（构建和测试）

**Interfaces:**
- 无

- [ ] **Step 1: 构建 PC 模拟**

```bash
cd pc_sim/build
cmake --build .
```

Expected: 构建成功，无错误

- [ ] **Step 2: 运行测试**

```bash
./lvgl_pc_sim.exe
```

Expected:
- 弹出 240x320 的窗口
- 显示主菜单，包含 4 个选项
- 点击选项进入对应的 tile 页面
- 左右滑动切换 tile（循环顺序）
- 所有控件显示正确

- [ ] **Step 3: 提交（如果有配置调整）**

```bash
git add -A
git commit -m "test: verify swipe navigation works correctly"
```

---

## 验证清单

完成所有任务后，检查以下内容：

- [ ] PC 上能正常运行 LVGL 模拟器
- [ ] 主菜单显示 4 个选项
- [ ] 点击选项进入对应的 tile 页面
- [ ] 左右滑动切换 tile（循环顺序）
- [ ] 所有控件显示正确
- [ ] 控件交互正常
- [ ] 所有文件已提交到 git
