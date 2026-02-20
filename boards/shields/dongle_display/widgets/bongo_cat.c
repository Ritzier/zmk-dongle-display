/*
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/wpm.h>

#include "bongo_cat.h"
#include "animations/bongo_cat_images.h"
#include "animations/frog_piano_images.h"

/* ── Animation speed constants ─────────────────────────────── */
#define ANIM_SPEED_IDLE 1000
#define ANIM_SPEED_SLOW  800
#define ANIM_SPEED_MID   500
#define ANIM_SPEED_FAST  300

/* ── Animal definitions ─────────────────────────────────────── */
typedef enum {
    ANIMAL_BONGO_CAT = 0,
    ANIMAL_FROG_PIANO,
    ANIMAL_COUNT
} animal_type_t;

static animal_type_t current_animal = ANIMAL_BONGO_CAT;

typedef struct {
    const lv_img_dsc_t **idle;
    uint8_t idle_cnt;
    const lv_img_dsc_t **slow;
    uint8_t slow_cnt;
    const lv_img_dsc_t **mid;
    uint8_t mid_cnt;
    const lv_img_dsc_t **fast;
    uint8_t fast_cnt;
} animal_def_t;

/* ── Bongo Cat frames ───────────────────────────────────────── */
static const lv_img_dsc_t *bongo_idle[] = { &bongo_cat_none };
static const lv_img_dsc_t *bongo_slow[] = { &bongo_cat_left1, &bongo_cat_none,
                                             &bongo_cat_right1, &bongo_cat_none };
static const lv_img_dsc_t *bongo_mid[]  = { &bongo_cat_left1, &bongo_cat_left2,
                                             &bongo_cat_right1, &bongo_cat_right2 };
static const lv_img_dsc_t *bongo_fast[] = { &bongo_cat_both1, &bongo_cat_both1_open,
                                             &bongo_cat_both2, &bongo_cat_both1_open };

/* ── Frog Piano frames ──────────────────────────────────────── */
static const lv_img_dsc_t *frog_idle[] = { &frog_piano_none };
static const lv_img_dsc_t *frog_slow[] = { &frog_piano_left1, &frog_piano_none,
                                            &frog_piano_right1, &frog_piano_none };
static const lv_img_dsc_t *frog_mid[]  = { &frog_piano_left1, &frog_piano_left2,
                                            &frog_piano_right1, &frog_piano_right2 };
static const lv_img_dsc_t *frog_fast[] = { &frog_piano_both1, &frog_piano_both2,
                                            &frog_piano_both1, &frog_piano_both2 };

static const animal_def_t animals[ANIMAL_COUNT] = {
    [ANIMAL_BONGO_CAT] = {
        .idle     = bongo_idle, .idle_cnt = ARRAY_SIZE(bongo_idle),
        .slow     = bongo_slow, .slow_cnt = ARRAY_SIZE(bongo_slow),
        .mid      = bongo_mid,  .mid_cnt  = ARRAY_SIZE(bongo_mid),
        .fast     = bongo_fast, .fast_cnt = ARRAY_SIZE(bongo_fast),
    },
    [ANIMAL_FROG_PIANO] = {
        .idle     = frog_idle, .idle_cnt = ARRAY_SIZE(frog_idle),
        .slow     = frog_slow, .slow_cnt = ARRAY_SIZE(frog_slow),
        .mid      = frog_mid,  .mid_cnt  = ARRAY_SIZE(frog_mid),
        .fast     = frog_fast, .fast_cnt = ARRAY_SIZE(frog_fast),
    },
};

/* ── Widget state ───────────────────────────────────────────── */
typedef enum {
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast,
} anim_state_t;

static anim_state_t current_anim_state = anim_state_idle;

struct zmk_widget_bongo_cat {
    sys_snode_t node;
    lv_obj_t *obj;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/* ── Widget init & obj getter ───────────────────────────────── */
int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_set_size(widget->obj, 50, 26);

    lv_animimg_set_src(widget->obj, (const void **)bongo_idle,
                       ARRAY_SIZE(bongo_idle));
    lv_animimg_set_duration(widget->obj, ANIM_SPEED_IDLE);
    lv_animimg_set_repeat_count(widget->obj, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(widget->obj);

    sys_slist_append(&widgets, &widget->node);
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}

/* ── Set animation based on WPM ─────────────────────────────── */
static void set_animation(lv_obj_t *animing, uint8_t wpm) {
    const animal_def_t *a = &animals[current_animal];

    if (wpm < 5) {
        if (current_anim_state == anim_state_idle) return;
        lv_animimg_set_src(animing, (const void **)a->idle, a->idle_cnt);
        lv_animimg_set_duration(animing, ANIM_SPEED_IDLE);
        current_anim_state = anim_state_idle;
    } else if (wpm < 30) {
        if (current_anim_state == anim_state_slow) return;
        lv_animimg_set_src(animing, (const void **)a->slow, a->slow_cnt);
        lv_animimg_set_duration(animing, ANIM_SPEED_SLOW);
        current_anim_state = anim_state_slow;
    } else if (wpm < 70) {
        if (current_anim_state == anim_state_mid) return;
        lv_animimg_set_src(animing, (const void **)a->mid, a->mid_cnt);
        lv_animimg_set_duration(animing, ANIM_SPEED_MID);
        current_anim_state = anim_state_mid;
    } else {
        if (current_anim_state == anim_state_fast) return;
        lv_animimg_set_src(animing, (const void **)a->fast, a->fast_cnt);
        lv_animimg_set_duration(animing, ANIM_SPEED_FAST);
        current_anim_state = anim_state_fast;
    }
    lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(animing);
}

/* ── WPM listener ───────────────────────────────────────────── */
struct bongo_cat_wpm_status_state {
    uint8_t wpm;
};

static struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state){ .wpm = ev ? ev->state : 0 };
}

static void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_animation(widget->obj, state.wpm);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                             bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

/* ── Animal switch listener ─────────────────────────────────── */
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
        /* Force animation refresh on next WPM event by resetting state */
        current_anim_state = (anim_state_t)-1;
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_animal_switch, struct animal_switch_state,
                             animal_switch_update_cb, animal_switch_get_state)
ZMK_SUBSCRIPTION(widget_animal_switch, zmk_keycode_state_changed);

