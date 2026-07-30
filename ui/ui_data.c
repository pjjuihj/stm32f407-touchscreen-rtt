#include "ui.h"
#include "ui_swipe.h"
#include <math.h>

/* 控件对象 */
static lv_obj_t * chart = NULL;
static lv_chart_series_t * ser1 = NULL;

/**
 * @brief 初始化数据控件页面
 * @param parent 父对象（tileview tile）
 */
lv_obj_t * ui_data_create(lv_obj_t * parent)
{
    /* 创建容器 */
    lv_obj_t * scr = lv_obj_create(parent);

    /* 设置屏幕背景色 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* 设置容器尺寸（与 ui_input.c / ui_display.c 保持一致） */
    lv_obj_set_size(scr, 240, 320);

    /* 创建标题标签 */
    lv_obj_t * title = lv_label_create(scr);
    if(title != NULL)
    {
        lv_label_set_text(title, "Data Controls");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    }

    /* 创建滚动容器 */
    lv_obj_t * container = lv_obj_create(scr);
    if(container != NULL)
    {
        lv_obj_set_size(container, 230, 280);
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
            lv_obj_set_size(chart, LV_PCT(100), 100);
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
            lv_obj_set_width(table, LV_PCT(100));
            lv_table_set_col_cnt(table, 3);
            lv_table_set_row_cnt(table, 4);

            /* 设置表头 */
            lv_table_set_cell_value(table, 0, 0, "Name");
            lv_table_set_cell_value(table, 0, 1, "Value");
            lv_table_set_cell_value(table, 0, 2, "Status");

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
