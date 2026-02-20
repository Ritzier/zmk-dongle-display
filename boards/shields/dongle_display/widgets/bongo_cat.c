/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/events/keycode_state_changed.h>

#include "bongo_cat.h"
#include "animations/bongo_cat_images.h"
#include "animations/frog_piano_images.h"

/* ── HID keycodes for F23 and F24 ───────────────────────────── */
#define KEYCODE_F23  114
#define KEYCODE_F24  115

/* ── Animal list ─────────────────────────────────────────────── */
typedef enum {
    ANIMAL_BONGO_CAT = 0,
    ANIMAL_FROG_PIANO,
    ANIMAL_COUNT
} animal_type_t;

static animal_type_t current_animal = ANIMAL_BONGO_CAT;

static const lv_img_dsc_t *animal_frames[ANIMAL_COUNT] = {
    [ANIMAL_BONGO_CAT]  = &bongo_cat_none,
    [ANIMAL_FROG_PIANO] = &frog_piano_none,
};

/* ── Display mode ────────────────────────────────────────────── */
typedef enum {
    MODE_NORMAL     = 0,  /* image on right, status on left */
    MODE_FULLSCREEN = 1,  /* image fills whole screen */
} display_mode_t;

static display_mode_t current_mode = MODE_NORMAL;

/* ── Widget list ─────────────────────────────────────────────── */
static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ── Forward declare status widgets to hide/show ─────────────── */
extern lv_obj_t *g_output_status_obj;
extern lv_obj_t *g_battery_status_obj;

/* ── Apply current mode to image widget ─────────────────────── */
static void apply_mode(struct zmk_widget_bongo_cat *widget) {
    if (current_mode == MODE_FULLSCREEN) {
        /* Hide status widgets */
        if (g_output_status_obj)  lv_obj_add_flag(g_output_status_obj,  LV_OBJ_FLAG_HIDDEN);
        if (g_battery_status_obj) lv_obj_add_flag(g_battery_status_obj, LV_OBJ_FLAG_HIDDEN);
        /* Stretch image to fill screen */
        lv_obj_set_size(widget->obj, 128, 64);
        lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);
    } else {
        /* Show status widgets */
        if (g_output_status_obj)  lv_obj_clear_flag(g_output_status_obj,  LV_OBJ_FLAG_HIDDEN);
        if (g_battery_status_obj) lv_obj_clear_flag(g_battery_status_obj, LV_OBJ_FLAG_HIDDEN);
        /* Restore image to normal size on right */
        lv_obj_set_size(widget->obj, 50, 50);
        lv_obj_align(widget->obj, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

/* ── Widget init ─────────────────────────────────────────────── */
int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_img_create(parent);
    lv_img_set_src(widget->obj, animal_frames[current_animal]);
    lv_obj_set_size(widget->obj, 50, 50);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}

/* ── Key listener ────────────────────────────────────────────── */
struct key_event_state {
    uint16_t keycode;
    bool pressed;
};

static struct key_event_state key_event_get_state(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev) {
        return (struct key_event_state){
            .keycode = ev->keycode,
            .pressed = ev->state,
        };
    }
    return (struct key_event_state){ .keycode = 0, .pressed = false };
}

static void key_event_update_cb(struct key_event_state state) {
    if (!state.pressed) return;

    struct zmk_widget_bongo_cat *widget;

    if (state.keycode == KEYCODE_F23) {
        /* Toggle fullscreen mode */
        current_mode = (current_mode == MODE_NORMAL) ? MODE_FULLSCREEN : MODE_NORMAL;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            apply_mode(widget);
        }
    } else if (state.keycode == KEYCODE_F24) {
        /* Cycle animal */
        current_animal = (current_animal + 1) % ANIMAL_COUNT;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            lv_img_set_src(widget->obj, animal_frames[current_animal]);
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_key_event, struct key_event_state,
                             key_event_update_cb, key_event_get_state)
ZMK_SUBSCRIPTION(widget_key_event, zmk_keycode_state_changed);

