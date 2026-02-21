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

#define KEYCODE_F23  114   /* toggle fullscreen */
#define KEYCODE_F24  115   /* cycle animal */

typedef enum {
    ANIMAL_BONGO_CAT = 0,
    ANIMAL_FROG_PIANO,
    ANIMAL_GRADIENT,
    ANIMAL_COUNT
} animal_type_t;

typedef enum {
    MODE_NORMAL     = 0,
    MODE_FULLSCREEN = 1,
} display_mode_t;

static animal_type_t  current_animal = ANIMAL_BONGO_CAT;
static display_mode_t current_mode   = MODE_NORMAL;

/* Two image tables — small and full */
static const lv_img_dsc_t *animal_small[ANIMAL_COUNT] = {
    [ANIMAL_BONGO_CAT]  = &bongo_cat_small,
    [ANIMAL_FROG_PIANO] = &frog_piano_small,
    [ANIMAL_GRADIENT]=&gradient_small,
};

static const lv_img_dsc_t *animal_full[ANIMAL_COUNT] = {
    [ANIMAL_BONGO_CAT]  = &bongo_cat_full,
    [ANIMAL_FROG_PIANO] = &frog_piano_full,
    [ANIMAL_GRADIENT]=&gradient_full,
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* Pick correct image based on current mode + animal */
static const lv_img_dsc_t *current_image(void) {
    if (current_mode == MODE_FULLSCREEN) {
        return animal_full[current_animal];
    }
    return animal_small[current_animal];
}

static void refresh_widgets(void) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (current_mode == MODE_FULLSCREEN) {
            lv_obj_set_size(widget->obj, 128, 64);
            lv_obj_align(widget->obj, LV_ALIGN_CENTER, 0, 0);
        } else {
            lv_obj_set_size(widget->obj, 50, 50);
            lv_obj_align(widget->obj, LV_ALIGN_RIGHT_MID, 0, 0);
        }
        lv_img_set_src(widget->obj, current_image());
    }
}

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_img_create(parent);
    lv_img_set_src(widget->obj, current_image());
    lv_obj_set_size(widget->obj, 50, 50);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}

/* Key listener */
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

    if (state.keycode == KEYCODE_F23) {
        current_mode = (current_mode == MODE_NORMAL)
                       ? MODE_FULLSCREEN : MODE_NORMAL;
        refresh_widgets();
    } else if (state.keycode == KEYCODE_F24) {
        current_animal = (current_animal + 1) % ANIMAL_COUNT;
        refresh_widgets();
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_key_event, struct key_event_state,
                             key_event_update_cb, key_event_get_state)
ZMK_SUBSCRIPTION(widget_key_event, zmk_keycode_state_changed);

