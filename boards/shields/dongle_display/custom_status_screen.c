/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include "custom_status_screen.h"
#include "widgets/battery_status.h"
#include "widgets/output_status.h"
#include "widgets/bongo_cat.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static struct zmk_widget_output_status output_status_widget;

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
static struct zmk_widget_dongle_battery_status dongle_battery_status_widget;
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
static struct zmk_widget_bongo_cat bongo_cat_widget;
#endif

/* ── Expose to bongo_cat.c for hide/show ─────────────────────── */
lv_obj_t *g_output_status_obj  = NULL;
lv_obj_t *g_battery_status_obj = NULL;

lv_style_t global_style;

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen;
    screen = lv_obj_create(NULL);

    lv_style_init(&global_style);
    lv_style_set_text_font(&global_style, &lv_font_unscii_8);
    lv_style_set_text_letter_space(&global_style, 1);
    lv_style_set_text_line_space(&global_style, 1);
    lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

    /* ── Output status — top left ── */
    zmk_widget_output_status_init(&output_status_widget, screen);
    lv_obj_align(zmk_widget_output_status_obj(&output_status_widget),
                 LV_ALIGN_TOP_LEFT, 0, 0);
    g_output_status_obj = zmk_widget_output_status_obj(&output_status_widget);

#if IS_ENABLED(CONFIG_ZMK_BATTERY)
    /* ── Battery — below output status ── */
    zmk_widget_dongle_battery_status_init(&dongle_battery_status_widget, screen);
    lv_obj_align_to(zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget),
                    zmk_widget_output_status_obj(&output_status_widget),
                    LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    g_battery_status_obj = zmk_widget_dongle_battery_status_obj(&dongle_battery_status_widget);
#endif

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_BONGO_CAT)
    /* ── Image — right side ── */
    zmk_widget_bongo_cat_init(&bongo_cat_widget, screen);
    lv_obj_align(zmk_widget_bongo_cat_obj(&bongo_cat_widget),
                 LV_ALIGN_RIGHT_MID, 0, 0);
#endif

    return screen;
}

