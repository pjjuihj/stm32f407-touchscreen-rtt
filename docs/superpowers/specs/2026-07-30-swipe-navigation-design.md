# LVGL 滑动导航设计

## 概述

扩展现有的 LVGL 控件演示 UI，添加滑动导航功能。使用 lv_tileview 实现子页面之间的左右滑动切换，支持循环顺序。

## 目标

- 在子页面之间左右滑动切换，无需返回主菜单
- 支持循环顺序：Input → Display → Data → Selection → Input
- 保持现有的菜单导航功能
- 保持 PC 和 STM32 的代码共享

## 非目标

- 不改变现有的控件功能
- 不添加复杂的滑动手势识别
- 不实现上下滑动切换页面

## 页面结构

### 主菜单页面

保持不变，点击按钮进入对应的 tile 页面。

### 子页面

使用 lv_tileview 组织 4 个子页面：
- Input Controls (tile 0)
- Display Controls (tile 1)
- Data Controls (tile 2)
- Selection Controls (tile 3)

支持左右滑动切换，循环顺序。

## lv_tileview 实现

### 控件结构

```
lv_tileview (全屏)
├── tile_input (0, 0)
│   └── Input 页面内容
├── tile_display (1, 0)
│   └── Display 页面内容
├── tile_data (2, 0)
│   └── Data 页面内容
└── tile_selection (3, 0)
    └── Selection 页面内容
```

### 实现细节

- 使用 `lv_tileview_create(NULL)` 创建全屏 tileview
- 每个 tile 使用 `lv_tileview_add_tile(tv, col, row, dir)` 添加
- 设置 `dir` 参数为 `LV_DIR_LEFT | LV_DIR_RIGHT` 支持左右滑动
- 使用 `lv_obj_set_tile(tv, tile, LV_ANIM_ON)` 实现程序化切换

### 滑动方向

- 左滑：切换到下一个 tile（Input → Display → Data → Selection → Input）
- 右滑：切换到上一个 tile（Input → Selection → Data → Display → Input）
- 循环：第一个 tile 左滑到最后一个，最后一个 tile 右滑到第一个

## 滑动导航管理

### 新文件 `ui/ui_swipe.c`

管理 lv_tileview 的创建和初始化，处理页面切换逻辑，提供 API 给其他模块调用。

### API 设计

```c
/* 创建并初始化滑动导航 */
void ui_swipe_init(void);

/* 切换到指定页面 */
void ui_swipe_goto(uint8_t page_index);

/* 获取当前页面索引 */
uint8_t ui_swipe_get_current(void);

/* 页面索引定义 */
#define SWIPE_PAGE_INPUT      0
#define SWIPE_PAGE_DISPLAY    1
#define SWIPE_PAGE_DATA       2
#define SWIPE_PAGE_SELECTION  3
#define SWIPE_PAGE_COUNT      4
```

### 实现细节

- `ui_swipe_init()`：创建 tileview，添加 4 个 tile，每个 tile 包含对应页面的内容
- `ui_swipe_goto()`：使用 `lv_obj_set_tile()` 切换到指定 tile
- `ui_swipe_get_current()`：返回当前 tile 的索引

### 与主菜单的集成

- 主菜单的按钮回调调用 `ui_swipe_goto()` 而不是直接创建页面
- 返回按钮调用 `lv_screen_load(g_main_menu_page)` 返回主菜单

## 页面修改

### 修改现有页面

每个子页面不再创建独立的屏幕，改为创建页面内容容器，返回容器对象。移除返回按钮（由 tileview 的滑动功能替代）。

### 修改后的函数签名

```c
/* 修改前 */
lv_obj_t * ui_input_create(void);  // 创建完整屏幕

/* 修改后 */
lv_obj_t * ui_input_create(lv_obj_t * parent);  // 在 parent 上创建内容
```

### 修改的文件

- `ui/ui_input.c`：创建内容容器，返回容器对象
- `ui/ui_display.c`：创建内容容器，返回容器对象
- `ui/ui_data.c`：创建内容容器，返回容器对象
- `ui/ui_selection.c`：创建内容容器，返回容器对象

### 保持不变

- 控件创建逻辑不变
- 回调函数不变
- 样式设置不变

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

## 导航流程

```
主菜单 → 点击 Input → Input 页面
       → 左滑 → Display 页面
       → 左滑 → Data 页面
       → 左滑 → Selection 页面
       → 左滑 → Input 页面（循环）
       → 右滑 → Data 页面
       → 点击返回 → 主菜单
```

## 平台兼容

### PC 模拟

- 所有 UI 代码保持平台无关
- 鼠标拖拽模拟滑动
- 控件交互只输出日志

### STM32 硬件

- 触摸滑动正常工作
- LED 控件操作实际控制 LED
- 其他控件交互输出日志

## 测试验证

### PC 模拟验证

1. 运行 `lvgl_pc_sim.exe`
2. 验证主菜单显示 4 个选项
3. 点击选项进入对应的 tile 页面
4. 验证左右滑动切换功能
5. 验证循环顺序正确
6. 验证返回按钮功能

### STM32 硬件验证

1. 烧录到 STM32F407
2. 验证触摸滑动交互
3. 验证 LED 控件操作实际 LED
4. 验证显示效果与 PC 一致

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| lv_tileview 内存占用大 | 使用部分渲染模式，优化内存使用 |
| 滑动与控件交互冲突 | 使用 lv_indev_set_group() 管理焦点 |
| 240x320 分辨率显示不全 | 使用滚动容器，确保所有控件可见 |
