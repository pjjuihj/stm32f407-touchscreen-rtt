#include "ui.h"
#include "gui_fonts.h"

/* 中文字体声明 */
LV_FONT_DECLARE(font_cn_16);

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

    /* 创建标题标签 - 中文 */
    lv_obj_t *title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "LVGL字体演示");
        lv_obj_set_style_text_font(title, &font_cn_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建说明标签 - 中文 */
    lv_obj_t *info = lv_label_create(scr);
    if(info != NULL)
    {
        lv_label_set_text(info, "中文: 你好世界!");
        lv_obj_set_style_text_font(info, &font_cn_16, 0);
        lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 35);
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
