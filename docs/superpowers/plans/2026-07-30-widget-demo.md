# LVGL 控件演示 UI 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 扩展现有的 LVGL PC 模拟环境，添加一个完整的控件演示 UI，使用菜单导航方式组织不同类型的 LVGL 控件。

**Architecture:** 使用菜单导航方式，主菜单页面显示 4 个控件类型选项，点击进入对应的子页面。每个子页面展示相应类型的 LVGL 控件，包括输入控件、显示控件、数据控件和选择控件。

**Tech Stack:** LVGL v9.5.0, SDL2 (PC), CMake, GCC (PC) / arm-none-eabi-gcc (STM32)

## Global Constraints

- LVGL 版本: v9.5.0 (已存在于 LVGL/ 目录)
- 配置文件: lv_conf.h 保持一份，PC 和 STM32 共用
- 分辨率: 240x320 (匹配 ILI9341 硬件)
- UI 代码: 只依赖 LVGL API，不依赖任何硬件
- 导航: 使用 lv_scr_load_anim() 实现页面切换

---

## 文件结构

```
ui/
├── ui.h                    ← UI 初始化接口（修改）
├── ui_main.c               ← 主菜单页面（修改）
├── ui_input.c              ← 输入控件页面（新建）
├── ui_display.c            ← 显示控件页面（新建）
├── ui_data.c               ← 数据控件页面（新建）
└── ui_selection.c          ← 选择控件页面（新建）
```

---

### Task 1: 更新 UI 头文件接口

**Files:**
- Modify: `ui/ui.h`

**Interfaces:**
- Produces: 页面初始化函数声明和全局变量声明

- [ ] **Step 1: 读取现有 ui/ui.h**

读取 `ui/ui.h` 文件，了解当前接口。

- [ ] **Step 2: 添加页面初始化函数声明**

```c
#ifndef UI_H
#define UI_H

#include "lvgl.h"

/* 页面索引 */
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
 *
 * 创建所有 UI 控件，设置事件回调。
 * 此函数是平台无关的，PC 和 STM32 共享。
 */
void ui_init(void);

/**
 * @brief 初始化主菜单页面
 * @return 主菜单页面对象
 */
lv_obj_t * ui_main_create(void);

/**
 * @brief 初始化输入控件页面
 * @return 输入控件页面对象
 */
lv_obj_t * ui_input_create(void);

/**
 * @brief 初始化显示控件页面
 * @return 显示控件页面对象
 */
lv_obj_t * ui_display_create(void);

/**
 * @brief 初始化数据控件页面
 * @return 数据控件页面对象
 */
lv_obj_t * ui_data_create(void);

/**
 * @brief 初始化选择控件页面
 * @return 选择控件页面对象
 */
lv_obj_t * ui_selection_create(void);

/**
 * @brief 切换到指定页面
 * @param page 目标页面对象
 */
void ui_navigate_to(lv_obj_t * page);

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
git commit -m "refactor: update UI header with navigation interface"
```

---

### Task 2: 重构主菜单页面

**Files:**
- Modify: `ui/ui_main.c`

**Interfaces:**
- Consumes: `ui/ui.h` 中的接口
- Produces: `ui_main_create()` 函数，`g_main_menu_page` 全局变量

- [ ] **Step 1: 读取现有 ui/ui_main.c**

读取 `ui/ui_main.c` 文件，了解当前实现。

- [ ] **Step 2: 重写 ui_main.c 为菜单导航页面**

```c
#include "ui.h"

/* 全局变量 */
lv_obj_t * g_current_page = NULL;
lv_obj_t * g_main_menu_page = NULL;

/* 页面创建函数声明 */
static void btn_input_cb(lv_event_t * e);
static void btn_display_cb(lv_event_t * e);
static void btn_data_cb(lv_event_t * e);
static void btn_selection_cb(lv_event_t * e);

/**
 * @brief 初始化 UI 界面
 */
void ui_init(void)
{
    /* 创建主菜单页面 */
    g_main_menu_page = ui_main_create();
    
    /* 加载主菜单页面 */
    lv_scr_load(g_main_menu_page);
    g_current_page = g_main_menu_page;
}

/**
 * @brief 初始化主菜单页面
 */
lv_obj_t * ui_main_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "LVGL 控件演示");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    /* 创建菜单列表 */
    lv_obj_t * list = lv_list_create(scr);
    if(list != NULL)
    {
        lv_obj_set_size(list, 200, 240);
        lv_obj_center(list);
        
        /* 输入控件按钮 */
        lv_obj_t * btn_input = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "输入控件");
        if(btn_input != NULL)
        {
            lv_obj_add_event_cb(btn_input, btn_input_cb, LV_EVENT_CLICKED, NULL);
        }
        
        /* 显示控件按钮 */
        lv_obj_t * btn_display = lv_list_add_btn(list, LV_SYMBOL_IMAGE, "显示控件");
        if(btn_display != NULL)
        {
            lv_obj_add_event_cb(btn_display, btn_display_cb, LV_EVENT_CLICKED, NULL);
        }
        
        /* 数据控件按钮 */
        lv_obj_t * btn_data = lv_list_add_btn(list, LV_SYMBOL_LIST, "数据控件");
        if(btn_data != NULL)
        {
            lv_obj_add_event_cb(btn_data, btn_data_cb, LV_EVENT_CLICKED, NULL);
        }
        
        /* 选择控件按钮 */
        lv_obj_t * btn_selection = lv_list_add_btn(list, LV_SYMBOL_OK, "选择控件");
        if(btn_selection != NULL)
        {
            lv_obj_add_event_cb(btn_selection, btn_selection_cb, LV_EVENT_CLICKED, NULL);
        }
    }
    
    return scr;
}

/* 导航到输入控件页面 */
static void btn_input_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to input page");
    lv_obj_t * page = ui_input_create();
    ui_navigate_to(page);
}

/* 导航到显示控件页面 */
static void btn_display_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to display page");
    lv_obj_t * page = ui_display_create();
    ui_navigate_to(page);
}

/* 导航到数据控件页面 */
static void btn_data_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to data page");
    lv_obj_t * page = ui_data_create();
    ui_navigate_to(page);
}

/* 导航到选择控件页面 */
static void btn_selection_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating to selection page");
    lv_obj_t * page = ui_selection_create();
    ui_navigate_to(page);
}

/**
 * @brief 切换到指定页面
 */
void ui_navigate_to(lv_obj_t * page)
{
    if(page != NULL)
    {
        g_current_page = page;
        lv_scr_load_anim(page, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
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
git commit -m "refactor: rewrite main menu with navigation"
```

---

### Task 3: 创建输入控件页面

**Files:**
- Create: `ui/ui_input.c`

**Interfaces:**
- Consumes: `ui/ui.h` 中的接口
- Produces: `ui_input_create()` 函数

- [ ] **Step 1: 创建 ui/ui_input.c**

```c
#include "ui.h"

/* 控件对象 */
static lv_obj_t * slider = NULL;
static lv_obj_t * slider_label = NULL;
static lv_obj_t * arc = NULL;
static lv_obj_t * arc_label = NULL;

/* 回调函数 */
static void slider_event_cb(lv_event_t * e);
static void arc_event_cb(lv_event_t * e);
static void back_btn_cb(lv_event_t * e);

/**
 * @brief 初始化输入控件页面
 */
lv_obj_t * ui_input_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* 创建返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(scr);
    if(back_btn != NULL)
    {
        lv_obj_set_size(back_btn, 40, 30);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * back_label = lv_label_create(back_btn);
        if(back_label != NULL)
        {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
            lv_obj_center(back_label);
        }
    }
    
    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "输入控件");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 260);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);
        
        /* Slider 标签 */
        lv_obj_t * slider_title = lv_label_create(container);
        if(slider_title != NULL)
        {
            lv_label_set_text(slider_title, "Slider:");
            lv_obj_set_style_text_font(slider_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(slider_title, 200);
        }
        
        /* Slider 控件 */
        slider = lv_slider_create(container);
        if(slider != NULL)
        {
            lv_obj_set_width(slider, 200);
            lv_slider_set_range(slider, 0, 100);
            lv_slider_set_value(slider, 50, LV_ANIM_OFF);
            lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
        
        /* Slider 值标签 */
        slider_label = lv_label_create(container);
        if(slider_label != NULL)
        {
            lv_label_set_text(slider_label, "50");
            lv_obj_set_style_text_font(slider_label, &lv_font_montserrat_14, 0);
        }
        
        /* Switch 标签 */
        lv_obj_t * switch_title = lv_label_create(container);
        if(switch_title != NULL)
        {
            lv_label_set_text(switch_title, "Switch:");
            lv_obj_set_style_text_font(switch_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(switch_title, 200);
        }
        
        /* Switch 控件 */
        lv_obj_t * sw = lv_switch_create(container);
        if(sw != NULL)
        {
            lv_obj_set_width(sw, 50);
            lv_obj_set_height(sw, 25);
        }
        
        /* Arc 标签 */
        lv_obj_t * arc_title = lv_label_create(container);
        if(arc_title != NULL)
        {
            lv_label_set_text(arc_title, "Arc:");
            lv_obj_set_style_text_font(arc_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(arc_title, 200);
        }
        
        /* Arc 控件 */
        arc = lv_arc_create(container);
        if(arc != NULL)
        {
            lv_obj_set_size(arc, 100, 100);
            lv_arc_set_range(arc, 0, 100);
            lv_arc_set_value(arc, 50);
            lv_obj_add_event_cb(arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
        
        /* Arc 值标签 */
        arc_label = lv_label_create(container);
        if(arc_label != NULL)
        {
            lv_label_set_text(arc_label, "50");
            lv_obj_set_style_text_font(arc_label, &lv_font_montserrat_14, 0);
        }
    }
    
    return scr;
}

/* Slider 值变化回调 */
static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(obj);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", value);
    lv_label_set_text(slider_label, buf);
    LV_LOG_USER("Slider value: %ld", value);
}

/* Arc 值变化回调 */
static void arc_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t value = lv_arc_get_value(obj);
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", value);
    lv_label_set_text(arc_label, buf);
    LV_LOG_USER("Arc value: %ld", value);
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating back to main menu");
    ui_navigate_to(g_main_menu_page);
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_input.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_input.c
git commit -m "feat: add input controls page (Slider, Switch, Arc)"
```

---

### Task 4: 创建显示控件页面

**Files:**
- Create: `ui/ui_display.c`

**Interfaces:**
- Consumes: `ui/ui.h` 中的接口
- Produces: `ui_display_create()` 函数

- [ ] **Step 1: 创建 ui/ui_display.c**

```c
#include "ui.h"

/* 控件对象 */
static lv_obj_t * bar = NULL;
static lv_obj_t * led1 = NULL;
static lv_obj_t * led2 = NULL;
static lv_obj_t * led3 = NULL;

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);
static void led1_event_cb(lv_event_t * e);
static void led2_event_cb(lv_event_t * e);
static void led3_event_cb(lv_event_t * e);

/**
 * @brief 初始化显示控件页面
 */
lv_obj_t * ui_display_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* 创建返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(scr);
    if(back_btn != NULL)
    {
        lv_obj_set_size(back_btn, 40, 30);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * back_label = lv_label_create(back_btn);
        if(back_label != NULL)
        {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
            lv_obj_center(back_label);
        }
    }
    
    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "显示控件");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 260);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);
        
        /* Bar 标签 */
        lv_obj_t * bar_title = lv_label_create(container);
        if(bar_title != NULL)
        {
            lv_label_set_text(bar_title, "Bar:");
            lv_obj_set_style_text_font(bar_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(bar_title, 200);
        }
        
        /* Bar 控件 */
        bar = lv_bar_create(container);
        if(bar != NULL)
        {
            lv_obj_set_width(bar, 200);
            lv_obj_set_height(bar, 20);
            lv_bar_set_range(bar, 0, 100);
            lv_bar_set_value(bar, 60, LV_ANIM_ON);
        }
        
        /* Spinner 标签 */
        lv_obj_t * spinner_title = lv_label_create(container);
        if(spinner_title != NULL)
        {
            lv_label_set_text(spinner_title, "Spinner:");
            lv_obj_set_style_text_font(spinner_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(spinner_title, 200);
        }
        
        /* Spinner 控件 */
        lv_obj_t * spinner = lv_spinner_create(container);
        if(spinner != NULL)
        {
            lv_obj_set_size(spinner, 40, 40);
        }
        
        /* LED 标签 */
        lv_obj_t * led_title = lv_label_create(container);
        if(led_title != NULL)
        {
            lv_label_set_text(led_title, "LED:");
            lv_obj_set_style_text_font(led_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(led_title, 200);
        }
        
        /* LED 容器 */
        lv_obj_t * led_container = lv_obj_create(container);
        if(led_container != NULL)
        {
            lv_obj_set_size(led_container, 200, 40);
            lv_obj_set_style_bg_opa(led_container, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(led_container, 0, 0);
            lv_obj_set_style_pad_all(led_container, 0, 0);
            lv_obj_set_flex_flow(led_container, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(led_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            
            /* LED 1 */
            led1 = lv_led_create(led_container);
            if(led1 != NULL)
            {
                lv_obj_set_size(led1, 30, 30);
                lv_led_set_color(led1, lv_color_hex(0x00FF00));
                lv_led_on(led1);
                lv_obj_add_event_cb(led1, led1_event_cb, LV_EVENT_CLICKED, NULL);
            }
            
            /* LED 2 */
            led2 = lv_led_create(led_container);
            if(led2 != NULL)
            {
                lv_obj_set_size(led2, 30, 30);
                lv_led_set_color(led2, lv_color_hex(0x00FF00));
                lv_led_off(led2);
                lv_obj_add_event_cb(led2, led2_event_cb, LV_EVENT_CLICKED, NULL);
            }
            
            /* LED 3 */
            led3 = lv_led_create(led_container);
            if(led3 != NULL)
            {
                lv_obj_set_size(led3, 30, 30);
                lv_led_set_color(led3, lv_color_hex(0x00FF00));
                lv_led_on(led3);
                lv_obj_add_event_cb(led3, led3_event_cb, LV_EVENT_CLICKED, NULL);
            }
        }
    }
    
    return scr;
}

/* LED 1 点击回调 */
static void led1_event_cb(lv_event_t * e)
{
    if(lv_led_get_state(led1))
    {
        lv_led_off(led1);
        LV_LOG_USER("LED1 OFF");
    }
    else
    {
        lv_led_on(led1);
        LV_LOG_USER("LED1 ON");
    }
}

/* LED 2 点击回调 */
static void led2_event_cb(lv_event_t * e)
{
    if(lv_led_get_state(led2))
    {
        lv_led_off(led2);
        LV_LOG_USER("LED2 OFF");
    }
    else
    {
        lv_led_on(led2);
        LV_LOG_USER("LED2 ON");
    }
}

/* LED 3 点击回调 */
static void led3_event_cb(lv_event_t * e)
{
    if(lv_led_get_state(led3))
    {
        lv_led_off(led3);
        LV_LOG_USER("LED3 OFF");
    }
    else
    {
        lv_led_on(led3);
        LV_LOG_USER("LED3 ON");
    }
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating back to main menu");
    ui_navigate_to(g_main_menu_page);
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_display.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_display.c
git commit -m "feat: add display controls page (Bar, Spinner, LED)"
```

---

### Task 5: 创建数据控件页面

**Files:**
- Create: `ui/ui_data.c`

**Interfaces:**
- Consumes: `ui/ui.h` 中的接口
- Produces: `ui_data_create()` 函数

- [ ] **Step 1: 创建 ui/ui_data.c**

```c
#include "ui.h"
#include <math.h>

/* 控件对象 */
static lv_obj_t * chart = NULL;
static lv_chart_series_t * ser1 = NULL;

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);

/**
 * @brief 初始化数据控件页面
 */
lv_obj_t * ui_data_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* 创建返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(scr);
    if(back_btn != NULL)
    {
        lv_obj_set_size(back_btn, 40, 30);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * back_label = lv_label_create(back_btn);
        if(back_label != NULL)
        {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
            lv_obj_center(back_label);
        }
    }
    
    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "数据控件");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 260);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);
        
        /* Chart 标签 */
        lv_obj_t * chart_title = lv_label_create(container);
        if(chart_title != NULL)
        {
            lv_label_set_text(chart_title, "Chart:");
            lv_obj_set_style_text_font(chart_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(chart_title, 200);
        }
        
        /* Chart 控件 */
        chart = lv_chart_create(container);
        if(chart != NULL)
        {
            lv_obj_set_size(chart, 200, 100);
            lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
            lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -100, 100);
            lv_chart_set_point_count(chart, 50);
            
            /* 添加数据系列 */
            ser1 = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
            if(ser1 != NULL)
            {
                /* 生成正弦波数据 */
                for(int i = 0; i < 50; i++)
                {
                    int32_t y = (int32_t)(sin(i * 0.3) * 80);
                    lv_chart_set_next_value(chart, ser1, y);
                }
            }
        }
        
        /* Table 标签 */
        lv_obj_t * table_title = lv_label_create(container);
        if(table_title != NULL)
        {
            lv_label_set_text(table_title, "Table:");
            lv_obj_set_style_text_font(table_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(table_title, 200);
        }
        
        /* Table 控件 */
        lv_obj_t * table = lv_table_create(container);
        if(table != NULL)
        {
            lv_obj_set_width(table, 200);
            lv_table_set_col_cnt(table, 3);
            lv_table_set_row_cnt(table, 4);
            
            /* 设置表头 */
            lv_table_set_cell_value(table, 0, 0, "名称");
            lv_table_set_cell_value(table, 0, 1, "值");
            lv_table_set_cell_value(table, 0, 2, "状态");
            
            /* 设置数据行 */
            lv_table_set_cell_value(table, 1, 0, "A");
            lv_table_set_cell_value(table, 1, 1, "100");
            lv_table_set_cell_value(table, 1, 2, "OK");
            
            lv_table_set_cell_value(table, 2, 0, "B");
            lv_table_set_cell_value(table, 2, 1, "200");
            lv_table_set_cell_value(table, 2, 2, "OK");
            
            lv_table_set_cell_value(table, 3, 0, "C");
            lv_table_set_cell_value(table, 3, 1, "150");
            lv_table_set_cell_value(table, 3, 2, "WARN");
        }
    }
    
    return scr;
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating back to main menu");
    ui_navigate_to(g_main_menu_page);
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_data.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_data.c
git commit -m "feat: add data controls page (Chart, Table)"
```

---

### Task 6: 创建选择控件页面

**Files:**
- Create: `ui/ui_selection.c`

**Interfaces:**
- Consumes: `ui/ui.h` 中的接口
- Produces: `ui_selection_create()` 函数

- [ ] **Step 1: 创建 ui/ui_selection.c**

```c
#include "ui.h"

/* 回调函数 */
static void back_btn_cb(lv_event_t * e);
static void dropdown_event_cb(lv_event_t * e);
static void roller_event_cb(lv_event_t * e);

/**
 * @brief 初始化选择控件页面
 */
lv_obj_t * ui_selection_create(void)
{
    /* 创建新屏幕 */
    lv_obj_t * scr = lv_obj_create(NULL);
    
    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    
    /* 创建返回按钮 */
    lv_obj_t * back_btn = lv_btn_create(scr);
    if(back_btn != NULL)
    {
        lv_obj_set_size(back_btn, 40, 30);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
        lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);
        
        lv_obj_t * back_label = lv_label_create(back_btn);
        if(back_label != NULL)
        {
            lv_label_set_text(back_label, LV_SYMBOL_LEFT " 返回");
            lv_obj_center(back_label);
        }
    }
    
    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "选择控件");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 260);
        lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 35);
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(container, 10, 0);
        
        /* Dropdown 标签 */
        lv_obj_t * dropdown_title = lv_label_create(container);
        if(dropdown_title != NULL)
        {
            lv_label_set_text(dropdown_title, "Dropdown:");
            lv_obj_set_style_text_font(dropdown_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(dropdown_title, 200);
        }
        
        /* Dropdown 控件 */
        lv_obj_t * dropdown = lv_dropdown_create(container);
        if(dropdown != NULL)
        {
            lv_obj_set_width(dropdown, 200);
            lv_dropdown_set_options(dropdown, "Option 1\nOption 2\nOption 3\nOption 4");
            lv_dropdown_set_selected(dropdown, 0);
            lv_obj_add_event_cb(dropdown, dropdown_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
        
        /* Roller 标签 */
        lv_obj_t * roller_title = lv_label_create(container);
        if(roller_title != NULL)
        {
            lv_label_set_text(roller_title, "Roller:");
            lv_obj_set_style_text_font(roller_title, &lv_font_montserrat_14, 0);
            lv_obj_set_width(roller_title, 200);
        }
        
        /* Roller 控件 */
        lv_obj_t * roller = lv_roller_create(container);
        if(roller != NULL)
        {
            lv_obj_set_width(roller, 200);
            lv_roller_set_options(roller, "1\n2\n3\n4\n5", LV_ROLLER_MODE_NORMAL);
            lv_roller_set_selected(roller, 0, LV_ANIM_OFF);
            lv_obj_add_event_cb(roller, roller_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
        }
    }
    
    return scr;
}

/* Dropdown 值变化回调 */
static void dropdown_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(obj);
    LV_LOG_USER("Dropdown selected: %d", selected);
}

/* Roller 值变化回调 */
static void roller_event_cb(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    uint32_t selected = lv_roller_get_selected(obj);
    LV_LOG_USER("Roller selected: %lu", selected);
}

/* 返回按钮回调 */
static void back_btn_cb(lv_event_t * e)
{
    LV_LOG_USER("Navigating back to main menu");
    ui_navigate_to(g_main_menu_page);
}
```

- [ ] **Step 2: 验证语法**

```bash
gcc -fsyntax-only -I LVGL -I . ui/ui_selection.c
```

Expected: 无错误输出

- [ ] **Step 3: 提交**

```bash
git add ui/ui_selection.c
git commit -m "feat: add selection controls page (Dropdown, Roller)"
```

---

### Task 7: 更新 CMake 构建配置

**Files:**
- Modify: `pc_sim/CMakeLists.txt`

**Interfaces:**
- Consumes: 新创建的 UI 文件

- [ ] **Step 1: 读取现有 pc_sim/CMakeLists.txt**

读取 `pc_sim/CMakeLists.txt` 文件，了解当前配置。

- [ ] **Step 2: 添加新的 UI 源文件**

在 `target_sources` 部分添加新的 UI 文件：

```cmake
# 共享 UI 代码
target_sources(lvgl_pc_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ui/ui_main.c
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
git commit -m "build: add new UI files to CMake configuration"
```

---

### Task 8: 构建测试

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
- 点击每个选项，进入对应的子页面
- 验证每个子页面的控件显示正确
- 点击返回按钮，返回主菜单

- [ ] **Step 3: 提交（如果有配置调整）**

```bash
git add -A
git commit -m "test: verify widget demo UI works correctly"
```

---

## 验证清单

完成所有任务后，检查以下内容：

- [ ] PC 上能正常运行 LVGL 模拟器
- [ ] 主菜单显示 4 个选项
- [ ] 点击每个选项，进入对应的子页面
- [ ] 每个子页面的控件显示正确
- [ ] 返回按钮功能正常
- [ ] 控件交互正常（Slider、Switch、Arc 等）
- [ ] 所有文件已提交到 git
