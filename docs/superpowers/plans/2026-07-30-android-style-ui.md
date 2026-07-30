# Android 风格 UI 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 创建一个类似 Android 的手机 UI 界面，包含状态栏、应用图标网格、底部导航栏和通知中心。

**Architecture:** 使用 lv_tileview 组织多个页面，主屏幕作为第一个 tile，应用页面作为其他 tiles，通知中心作为可下拉的面板。

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
├── ui_main.c               ← 主屏幕页面（重写）
├── ui_swipe.c              ← 滑动导航管理（修改）
├── ui_status_bar.c         ← 状态栏组件（新建）
├── ui_bottom_nav.c         ← 底部导航栏（新建）
├── ui_notification.c       ← 通知中心（新建）
├── ui_app_input.c          ← Input 应用页面（重命名）
├── ui_app_display.c        ← Display 应用页面（重命名）
├── ui_app_data.c           ← Data 应用页面（重命名）
└── ui_app_selection.c      ← Selection 应用页面（重命名）
```

---

### Task 1: 创建状态栏组件

**Files:**
- Create: `ui/ui_status_bar.c`
- Create: `ui/ui_status_bar.h`

**Interfaces:**
- Produces: `ui_status_bar_create()` 函数

- [ ] **Step 1: 创建 ui/ui_status_bar.h**

```c
#ifndef UI_STATUS_BAR_H
#define UI_STATUS_BAR_H

#include "lvgl.h"

/**
 * @brief 创建状态栏组件
 * @param parent 父对象
 * @return 状态栏对象
 */
lv_obj_t * ui_status_bar_create(lv_obj_t * parent);

#endif /* UI_STATUS_BAR_H */
```

- [ ] **Step 2: 创建 ui/ui_status_bar.c**

```c
#include "ui_status_bar.h"
#include <stdio.h>

/**
 * @brief 创建状态栏组件
 */
lv_obj_t * ui_status_bar_create(lv_obj_t * parent)
{
    /* 创建状态栏容器 */
    lv_obj_t * status_bar = lv_obj_create(parent);
    if(status_bar == NULL) return NULL;

    /* 设置状态栏大小和样式 */
    lv_obj_set_size(status_bar, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0xF5F5F5), 0);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 5, 0);

    /* 创建时间标签 */
    lv_obj_t * time_label = lv_label_create(status_bar);
    if(time_label != NULL)
    {
        lv_label_set_text(time_label, "12:30");
        lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
        lv_obj_align(time_label, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* 创建电池图标 */
    lv_obj_t * battery_label = lv_label_create(status_bar);
    if(battery_label != NULL)
    {
        lv_label_set_text(battery_label, LV_SYMBOL_BATTERY_FULL " 85%");
        lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
        lv_obj_align(battery_label, LV_ALIGN_RIGHT_MID, -60, 0);
    }

    /* 创建信号图标 */
    lv_obj_t * signal_label = lv_label_create(status_bar);
    if(signal_label != NULL)
    {
        lv_label_set_text(signal_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_font(signal_label, &lv_font_montserrat_12, 0);
        lv_obj_align(signal_label, LV_ALIGN_RIGHT_MID, -30, 0);
    }

    /* 创建通知图标 */
    lv_obj_t * notification_label = lv_label_create(status_bar);
    if(notification_label != NULL)
    {
        lv_label_set_text(notification_label, LV_SYMBOL_BELL);
        lv_obj_set_style_text_font(notification_label, &lv_font_montserrat_12, 0);
        lv_obj_align(notification_label, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    return status_bar;
}
```

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_status_bar.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_status_bar.h ui/ui_status_bar.c
git commit -m "feat: add status bar component"
```

---

### Task 2: 创建底部导航栏组件

**Files:**
- Create: `ui/ui_bottom_nav.c`
- Create: `ui/ui_bottom_nav.h`

**Interfaces:**
- Produces: `ui_bottom_nav_create()` 函数
- Produces: `ui_bottom_nav_set_active()` 函数

- [ ] **Step 1: 创建 ui/ui_bottom_nav.h**

```c
#ifndef UI_BOTTOM_NAV_H
#define UI_BOTTOM_NAV_H

#include "lvgl.h"

/* 导航按钮索引 */
#define BOTTOM_NAV_BACK     0
#define BOTTOM_NAV_HOME     1
#define BOTTOM_NAV_RECENT   2
#define BOTTOM_NAV_NOTIFY   3
#define BOTTOM_NAV_COUNT    4

/**
 * @brief 创建底部导航栏组件
 * @param parent 父对象
 * @return 底部导航栏对象
 */
lv_obj_t * ui_bottom_nav_create(lv_obj_t * parent);

/**
 * @brief 设置当前活跃的导航按钮
 * @param index 按钮索引
 */
void ui_bottom_nav_set_active(uint8_t index);

#endif /* UI_BOTTOM_NAV_H */
```

- [ ] **Step 2: 创建 ui/ui_bottom_nav.c**

```c
#include "ui_bottom_nav.h"

/* 静态变量 */
static lv_obj_t * nav_buttons[BOTTOM_NAV_COUNT] = {NULL};
static uint8_t active_index = BOTTOM_NAV_HOME;

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);
static void home_btn_cb(lv_event_t * e);
static void recent_btn_cb(lv_event_t * e);
static void notify_btn_cb(lv_event_t * e);

/* 按钮回调数组 */
static void (*nav_callbacks[BOTTOM_NAV_COUNT])(lv_event_t * e) = {
    back_btn_cb, home_btn_cb, recent_btn_cb, notify_btn_cb
};

/**
 * @brief 创建底部导航栏组件
 */
lv_obj_t * ui_bottom_nav_create(lv_obj_t * parent)
{
    /* 创建导航栏容器 */
    lv_obj_t * nav_bar = lv_obj_create(parent);
    if(nav_bar == NULL) return NULL;

    /* 设置导航栏大小和样式 */
    lv_obj_set_size(nav_bar, LV_PCT(100), 50);
    lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(nav_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_pad_all(nav_bar, 0, 0);

    /* 设置 Flex 布局 */
    lv_obj_set_flex_flow(nav_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 创建导航按钮 */
    const char * symbols[BOTTOM_NAV_COUNT] = {
        LV_SYMBOL_LEFT, LV_SYMBOL_HOME, LV_SYMBOL_IMAGE, LV_SYMBOL_BELL
    };

    for(uint8_t i = 0; i < BOTTOM_NAV_COUNT; i++)
    {
        nav_buttons[i] = lv_btn_create(nav_bar);
        if(nav_buttons[i] != NULL)
        {
            lv_obj_set_size(nav_buttons[i], 50, 40);
            lv_obj_set_style_bg_opa(nav_buttons[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(nav_buttons[i], 0, 0);

            lv_obj_t * label = lv_label_create(nav_buttons[i]);
            if(label != NULL)
            {
                lv_label_set_text(label, symbols[i]);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0x757575), 0);
                lv_obj_center(label);
            }

            lv_obj_add_event_cb(nav_buttons[i], nav_callbacks[i], LV_EVENT_CLICKED, NULL);
        }
    }

    /* 设置默认活跃按钮 */
    ui_bottom_nav_set_active(BOTTOM_NAV_HOME);

    return nav_bar;
}

/**
 * @brief 设置当前活跃的导航按钮
 */
void ui_bottom_nav_set_active(uint8_t index)
{
    if(index >= BOTTOM_NAV_COUNT) return;

    /* 取消所有按钮的高亮状态 */
    for(uint8_t i = 0; i < BOTTOM_NAV_COUNT; i++)
    {
        if(nav_buttons[i] != NULL)
        {
            lv_obj_t * label = lv_obj_get_child(nav_buttons[i], 0);
            if(label != NULL)
            {
                lv_obj_set_style_text_color(label, lv_color_hex(0x757575), 0);
            }
        }
    }

    /* 高亮当前按钮 */
    if(nav_buttons[index] != NULL)
    {
        lv_obj_t * label = lv_obj_get_child(nav_buttons[index], 0);
        if(label != NULL)
        {
            lv_obj_set_style_text_color(label, lv_color_hex(0x2196F3), 0);
        }
    }

    active_index = index;
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Back button clicked");
    /* TODO: 实现返回功能 */
}

/* 主页按钮回调 */
static void home_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Home button clicked");
    /* TODO: 实现主页功能 */
}

/* 最近任务按钮回调 */
static void recent_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Recent button clicked");
    /* TODO: 实现最近任务功能 */
}

/* 通知按钮回调 */
static void notify_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Notify button clicked");
    /* TODO: 实现通知功能 */
}
```

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_bottom_nav.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_bottom_nav.h ui/ui_bottom_nav.c
git commit -m "feat: add bottom navigation bar component"
```

---

### Task 3: 创建通知中心组件

**Files:**
- Create: `ui/ui_notification.c`
- Create: `ui/ui_notification.h`

**Interfaces:**
- Produces: `ui_notification_create()` 函数
- Produces: `ui_notification_show()` 函数
- Produces: `ui_notification_hide()` 函数

- [ ] **Step 1: 创建 ui/ui_notification.h**

```c
#ifndef UI_NOTIFICATION_H
#define UI_NOTIFICATION_H

#include "lvgl.h"

/**
 * @brief 创建通知中心组件
 * @param parent 父对象
 * @return 通知中心对象
 */
lv_obj_t * ui_notification_create(lv_obj_t * parent);

/**
 * @brief 显示通知中心
 */
void ui_notification_show(void);

/**
 * @brief 隐藏通知中心
 */
void ui_notification_hide(void);

#endif /* UI_NOTIFICATION_H */
```

- [ ] **Step 2: 创建 ui/ui_notification.c**

```c
#include "ui_notification.h"

/* 静态变量 */
static lv_obj_t * notification_panel = NULL;
static bool is_visible = false;

/* 回调函数 */
static void close_btn_cb(lv_event_t * e);
static void wifi_btn_cb(lv_event_t * e);
static void bluetooth_btn_cb(lv_event_t * e);
static void brightness_btn_cb(lv_event_t * e);
static void airplane_btn_cb(lv_event_t * e);
static void flashlight_btn_cb(lv_event_t * e);
static void auto_btn_cb(lv_event_t * e);

/**
 * @brief 创建通知中心组件
 */
lv_obj_t * ui_notification_create(lv_obj_t * parent)
{
    /* 创建通知中心面板 */
    notification_panel = lv_obj_create(parent);
    if(notification_panel == NULL) return NULL;

    /* 设置面板大小和样式 */
    lv_obj_set_size(notification_panel, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(notification_panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(notification_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(notification_panel, 0, 0);
    lv_obj_set_style_pad_all(notification_panel, 10, 0);

    /* 设置 Flex 布局 */
    lv_obj_set_flex_flow(notification_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(notification_panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(notification_panel, 10, 0);

    /* 创建标题 */
    lv_obj_t * title = lv_label_create(notification_panel);
    if(title != NULL)
    {
        lv_label_set_text(title, "通知中心");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_set_width(title, LV_PCT(100));
    }

    /* 创建快速设置标题 */
    lv_obj_t * quick_settings_title = lv_label_create(notification_panel);
    if(quick_settings_title != NULL)
    {
        lv_label_set_text(quick_settings_title, "快速设置");
        lv_obj_set_style_text_font(quick_settings_title, &lv_font_montserrat_14, 0);
        lv_obj_set_width(quick_settings_title, LV_PCT(100));
    }

    /* 创建快速设置按钮网格 */
    lv_obj_t * quick_settings_grid = lv_obj_create(notification_panel);
    if(quick_settings_grid != NULL)
    {
        lv_obj_set_size(quick_settings_grid, LV_PCT(100), 100);
        lv_obj_set_style_bg_opa(quick_settings_grid, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(quick_settings_grid, 0, 0);
        lv_obj_set_style_pad_all(quick_settings_grid, 0, 0);

        /* 设置 3x2 网格 */
        lv_obj_set_flex_flow(quick_settings_grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(quick_settings_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(quick_settings_grid, 5, 0);

        /* 创建快速设置按钮 */
        const char * quick_settings_labels[] = {"Wi-Fi", "蓝牙", "亮度", "飞行", "手电", "自动"};
        void (*quick_settings_callbacks[])(lv_event_t * e) = {
            wifi_btn_cb, bluetooth_btn_cb, brightness_btn_cb,
            airplane_btn_cb, flashlight_btn_cb, auto_btn_cb
        };

        for(uint8_t i = 0; i < 6; i++)
        {
            lv_obj_t * btn = lv_btn_create(quick_settings_grid);
            if(btn != NULL)
            {
                lv_obj_set_size(btn, 60, 40);
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xE0E0E0), 0);
                lv_obj_set_style_radius(btn, 10, 0);

                lv_obj_t * label = lv_label_create(btn);
                if(label != NULL)
                {
                    lv_label_set_text(label, quick_settings_labels[i]);
                    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
                    lv_obj_center(label);
                }

                lv_obj_add_event_cb(btn, quick_settings_callbacks[i], LV_EVENT_CLICKED, NULL);
            }
        }
    }

    /* 创建通知标题 */
    lv_obj_t * notifications_title = lv_label_create(notification_panel);
    if(notifications_title != NULL)
    {
        lv_label_set_text(notifications_title, "通知");
        lv_obj_set_style_text_font(notifications_title, &lv_font_montserrat_14, 0);
        lv_obj_set_width(notifications_title, LV_PCT(100));
    }

    /* 创建通知列表 */
    lv_obj_t * notification_list = lv_list_create(notification_panel);
    if(notification_list != NULL)
    {
        lv_obj_set_size(notification_list, LV_PCT(100), 150);

        /* 添加示例通知 */
        lv_obj_t * notif1 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "应用通知 1");
        lv_obj_t * notif2 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "应用通知 2");
        lv_obj_t * notif3 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "应用通知 3");
    }

    /* 创建关闭按钮 */
    lv_obj_t * close_btn = lv_btn_create(notification_panel);
    if(close_btn != NULL)
    {
        lv_obj_set_size(close_btn, LV_PCT(100), 40);
        lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xF44336), 0);

        lv_obj_t * close_label = lv_label_create(close_btn);
        if(close_label != NULL)
        {
            lv_label_set_text(close_label, "关闭");
            lv_obj_center(close_label);
        }

        lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, NULL);
    }

    /* 默认隐藏通知中心 */
    ui_notification_hide();

    return notification_panel;
}

/**
 * @brief 显示通知中心
 */
void ui_notification_show(void)
{
    if(notification_panel != NULL)
    {
        lv_obj_clear_flag(notification_panel, LV_OBJ_FLAG_HIDDEN);
        is_visible = true;
        LV_LOG_USER("Notification center shown");
    }
}

/**
 * @brief 隐藏通知中心
 */
void ui_notification_hide(void)
{
    if(notification_panel != NULL)
    {
        lv_obj_add_flag(notification_panel, LV_OBJ_FLAG_HIDDEN);
        is_visible = false;
        LV_LOG_USER("Notification center hidden");
    }
}

/* 关闭按钮回调 */
static void close_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    ui_notification_hide();
}

/* Wi-Fi 按钮回调 */
static void wifi_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Wi-Fi toggled");
}

/* 蓝牙按钮回调 */
static void bluetooth_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Bluetooth toggled");
}

/* 亮度按钮回调 */
static void brightness_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Brightness toggled");
}

/* 飞行模式按钮回调 */
static void airplane_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Airplane mode toggled");
}

/* 手电筒按钮回调 */
static void flashlight_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Flashlight toggled");
}

/* 自动亮度按钮回调 */
static void auto_btn_cb(lv_event_t * e)
{
    LV_UNUSED(e);
    LV_LOG_USER("Auto brightness toggled");
}
```

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_notification.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_notification.h ui/ui_notification.c
git commit -m "feat: add notification center component"
```

---

### Task 4: 重写主屏幕页面

**Files:**
- Modify: `ui/ui_main.c`

**Interfaces:**
- Consumes: `ui_status_bar.h` 中的接口
- Consumes: `ui_bottom_nav.h` 中的接口
- Consumes: `ui_notification.h` 中的接口

- [ ] **Step 1: 读取现有 ui/ui_main.c**

读取 `ui/ui_main.c` 文件，了解当前实现。

- [ ] **Step 2: 重写主屏幕页面**

```c
#include "ui.h"
#include "ui_swipe.h"
#include "ui_status_bar.h"
#include "ui_bottom_nav.h"
#include "ui_notification.h"

/* 全局变量 */
lv_obj_t * g_current_page = NULL;
lv_obj_t * g_main_menu_page = NULL;

/* 应用图标数据 */
typedef struct {
    const char * name;
    const char * symbol;
    uint8_t page_index;
} app_info_t;

static const app_info_t apps[] = {
    {"Input", LV_SYMBOL_SETTINGS, SWIPE_PAGE_INPUT},
    {"Display", LV_SYMBOL_IMAGE, SWIPE_PAGE_DISPLAY},
    {"Data", LV_SYMBOL_LIST, SWIPE_PAGE_DATA},
    {"Selection", LV_SYMBOL_OK, SWIPE_PAGE_SELECTION},
};

#define APP_COUNT (sizeof(apps) / sizeof(apps[0]))

/* 回调函数 */
static void app_icon_cb(lv_event_t * e);

/**
 * @brief 初始化 UI 界面
 */
void ui_init(void)
{
    /* 初始化滑动导航 */
    ui_swipe_init();

    /* 主菜单已作为 tileview 的第一个 tile 创建 */
    g_current_page = g_main_menu_page;
}

/**
 * @brief 初始化主屏幕页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_main_create(lv_obj_t * parent)
{
    /* 创建容器 */
    lv_obj_t * scr = lv_obj_create(parent);
    if(scr == NULL) return NULL;

    /* 设置容器大小 */
    lv_obj_set_size(scr, LV_PCT(100), LV_PCT(100));

    /* 记录主菜单页面引用 */
    g_main_menu_page = scr;

    /* 设置容器背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 创建状态栏 */
    ui_status_bar_create(scr);

    /* 创建应用图标网格 */
    lv_obj_t * app_grid = lv_obj_create(scr);
    if(app_grid != NULL)
    {
        lv_obj_set_size(app_grid, LV_PCT(100), LV_PCT(100));
        lv_obj_align(app_grid, LV_ALIGN_TOP_MID, 0, 30);
        lv_obj_set_style_bg_opa(app_grid, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(app_grid, 0, 0);
        lv_obj_set_style_pad_all(app_grid, 10, 0);

        /* 设置 4x3 网格 */
        lv_obj_set_flex_flow(app_grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(app_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(app_grid, 20, 0);
        lv_obj_set_style_pad_column(app_grid, 20, 0);

        /* 创建应用图标 */
        for(uint8_t i = 0; i < APP_COUNT; i++)
        {
            lv_obj_t * app_icon = lv_obj_create(app_grid);
            if(app_icon != NULL)
            {
                lv_obj_set_size(app_icon, 60, 70);
                lv_obj_set_style_bg_opa(app_icon, LV_OPA_TRANSP, 0);
                lv_obj_set_style_border_width(app_icon, 0, 0);

                /* 创建图标 */
                lv_obj_t * icon = lv_label_create(app_icon);
                if(icon != NULL)
                {
                    lv_label_set_text(icon, apps[i].symbol);
                    lv_obj_set_style_text_font(icon, &lv_font_montserrat_30, 0);
                    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);
                }

                /* 创建名称标签 */
                lv_obj_t * name = lv_label_create(app_icon);
                if(name != NULL)
                {
                    lv_label_set_text(name, apps[i].name);
                    lv_obj_set_style_text_font(name, &lv_font_montserrat_10, 0);
                    lv_obj_align(name, LV_ALIGN_BOTTOM_MID, 0, 0);
                }

                /* 添加点击事件 */
                lv_obj_add_event_cb(app_icon, app_icon_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            }
        }
    }

    /* 创建底部导航栏 */
    ui_bottom_nav_create(scr);

    /* 创建通知中心 */
    ui_notification_create(scr);

    return scr;
}

/* 应用图标点击回调 */
static void app_icon_cb(lv_event_t * e)
{
    uint8_t index = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(index < APP_COUNT)
    {
        LV_LOG_USER("Launching app: %s", apps[index].name);
        ui_swipe_goto(apps[index].page_index);
    }
}
```

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_main.c
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui_main.c
git commit -m "feat: rewrite main screen with Android-style UI"
```

---

### Task 5: 更新 UI 头文件接口

**Files:**
- Modify: `ui/ui.h`

**Interfaces:**
- 添加新的组件接口声明

- [ ] **Step 1: 读取现有 ui/ui.h**

读取 `ui/ui.h` 文件，了解当前接口。

- [ ] **Step 2: 添加新的组件接口声明**

```c
#ifndef UI_H
#define UI_H

#include "lvgl.h"

/* 页面索引（与 ui_swipe.h 中的 SWIPE_PAGE_* 保持一致） */
#define PAGE_MAIN       0
#define PAGE_INPUT      1
#define PAGE_DISPLAY    2
#define PAGE_DATA       3
#define PAGE_SELECTION  4
#define PAGE_COUNT      5

/* 全局变量：当前页面 */
extern lv_obj_t * g_current_page;
extern lv_obj_t * g_main_menu_page;

/**
 * @brief 初始化 UI 界面
 */
void ui_init(void);

/**
 * @brief 初始化主菜单页面
 * @param parent 父对象（tileview tile）
 * @return 主菜单页面对象
 */
lv_obj_t * ui_main_create(lv_obj_t * parent);

/**
 * @brief 初始化输入控件页面
 * @param parent 父对象（tileview tile）
 * @return 输入控件页面对象
 */
lv_obj_t * ui_input_create(lv_obj_t * parent);

/**
 * @brief 初始化显示控件页面
 * @param parent 父对象（tileview tile）
 * @return 显示控件页面对象
 */
lv_obj_t * ui_display_create(lv_obj_t * parent);

/**
 * @brief 初始化数据控件页面
 * @param parent 父对象（tileview tile）
 * @return 数据控件页面对象
 */
lv_obj_t * ui_data_create(lv_obj_t * parent);

/**
 * @brief 初始化选择控件页面
 * @param parent 父对象（tileview tile）
 * @return 选择控件页面对象
 */
lv_obj_t * ui_selection_create(lv_obj_t * parent);

/* 组件接口 */
#include "ui_status_bar.h"
#include "ui_bottom_nav.h"
#include "ui_notification.h"

#endif /* UI_H */
```

- [ ] **Step 3: 验证语法**

```bash
gcc -fsyntax-only -I LVGL ui/ui.h
```

Expected: 无错误输出

- [ ] **Step 4: 提交**

```bash
git add ui/ui.h
git commit -m "feat: update UI header with Android-style components"
```

---

### Task 6: 更新 CMake 构建配置

**Files:**
- Modify: `pc_sim/CMakeLists.txt`

**Interfaces:**
- Consumes: 新创建的组件文件

- [ ] **Step 1: 读取现有 pc_sim/CMakeLists.txt**

读取 `pc_sim/CMakeLists.txt` 文件，了解当前配置。

- [ ] **Step 2: 添加新的组件文件到构建配置**

```cmake
# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_swipe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_status_bar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_bottom_nav.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_notification.c
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
git commit -m "build: add Android-style UI components to CMake"
```

---

### Task 7: 构建测试

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
- 显示主屏幕，包含状态栏、应用图标网格、底部导航栏
- 点击应用图标进入对应的应用页面
- 验证底部导航栏功能
- 验证通知中心下拉功能

- [ ] **Step 3: 提交（如果有配置调整）**

```bash
git add -A
git commit -m "test: verify Android-style UI works correctly"
```

---

## 验证清单

完成所有任务后，检查以下内容：

- [ ] PC 上能正常运行 LVGL 模拟器
- [ ] 主屏幕显示状态栏、应用图标网格、底部导航栏
- [ ] 点击应用图标进入对应的应用页面
- [ ] 底部导航栏功能正常
- [ ] 通知中心下拉功能正常
- [ ] 所有文件已提交到 git
