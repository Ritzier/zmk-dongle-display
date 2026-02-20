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

/* ── Animal definitions ─────────────────────────────────────── */
typedef enum {
    ANIMAL_BONGO_CAT = 0,
    ANIMAL_FROG_PIANO,
    ANIMAL_COUNT
} animal_type_t;

static animal_type_t current_animal = ANIMAL_BONGO_CAT;

static const lv_img_dsc_t *animal_static_frames[ANIMAL_COUNT] = {
    [ANIMAL_BONGO_CAT] = &bongo_cat_none,
    [ANIMAL_FROG_PIANO] = &frog_piano_none,
};

/* ── Widget state ───────────────────────────────────────────── */
static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ── Widget init & obj getter ───────────────────────────────── */
int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_img_create(parent);   /* plain lv_img, not animimg */
    lv_img_set_src(widget->obj, animal_static_frames[current_animal]);
    lv_obj_set_size(widget->obj, 50, 26);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}

/* ── Animal switch via F24 keycode ─────────────────────────── */
struct animal_switch_state {
    uint16_t keycode;
    bool pressed;
};

static struct animal_switch_state animal_switch_get_state(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev) {
        return (struct animal_switch_state){
            .keycode = ev->keycode,
            .pressed = ev->state,
        };
    }
    return (struct animal_switch_state){ .keycode = 0, .pressed = false };
}

static void animal_switch_update_cb(struct animal_switch_state state) {
    if (state.pressed && state.keycode == CONFIG_ZMK_BONGO_SWITCH_KEYCODE) {
        current_animal = (current_animal + 1) % ANIMAL_COUNT;
        struct zmk_widget_bongo_cat *widget;
        SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
            lv_img_set_src(widget->obj, animal_static_frames[current_animal]);
        }
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_animal_switch, struct animal_switch_state,
                             animal_switch_update_cb, animal_switch_get_state)
ZMK_SUBSCRIPTION(widget_animal_switch, zmk_keycode_state_changed);

