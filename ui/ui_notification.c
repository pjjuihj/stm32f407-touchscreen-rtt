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
        lv_label_set_text(title, "Notification Center");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_set_width(title, LV_PCT(100));
    }

    /* 创建快速设置标题 */
    lv_obj_t * quick_settings_title = lv_label_create(notification_panel);
    if(quick_settings_title != NULL)
    {
        lv_label_set_text(quick_settings_title, "Quick Settings");
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
        const char * quick_settings_labels[] = {"Wi-Fi", "Bluetooth", "Brightness", "Airplane", "Flashlight", "Auto"};
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
                    lv_obj_set_style_text_font(label, &lv_font_montserrat_12, 0);
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
        lv_label_set_text(notifications_title, "Notifications");
        lv_obj_set_style_text_font(notifications_title, &lv_font_montserrat_14, 0);
        lv_obj_set_width(notifications_title, LV_PCT(100));
    }

    /* 创建通知列表 */
    lv_obj_t * notification_list = lv_list_create(notification_panel);
    if(notification_list != NULL)
    {
        lv_obj_set_size(notification_list, LV_PCT(100), 150);

        /* 添加示例通知 */
        lv_obj_t * notif1 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "App Notification 1");
        lv_obj_t * notif2 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "App Notification 2");
        lv_obj_t * notif3 = lv_list_add_btn(notification_list, LV_SYMBOL_IMAGE, "App Notification 3");
        LV_UNUSED(notif1);
        LV_UNUSED(notif2);
        LV_UNUSED(notif3);
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
            lv_label_set_text(close_label, "Close");
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
