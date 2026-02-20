/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/wpm.h>
#include <zmk/hid.h>

#include "bongo_cat.h"
#include "animations/bongo_cat_images.h"
#include "animations/frog_piano_images.h"

#define SRC(array) (const void **)array, sizeof(array) / sizeof(lv_img_dsc_t *)

/* ── 动画帧集合（每套动物各一份）─────────────────────────── */

/* --- Bongo Cat --- */
#define ANIM_SPEED_IDLE 10000
static const lv_img_dsc_t *bc_idle[]  = { &bongo_cat_both1_open, &bongo_cat_both1_open,
                                           &bongo_cat_both1_open, &bongo_cat_both1 };
#define ANIM_SPEED_SLOW 2000
static const lv_img_dsc_t *bc_slow[]  = { &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_both1,
                                           &bongo_cat_right1, &bongo_cat_both1, &bongo_cat_both1,
                                           &bongo_cat_left1, &bongo_cat_both1, &bongo_cat_both1 };
#define ANIM_SPEED_MID  500
static const lv_img_dsc_t *bc_mid[]   = { &bongo_cat_left2, &bongo_cat_left1, &bongo_cat_none,
                                           &bongo_cat_right2, &bongo_cat_right1, &bongo_cat_none };
#define ANIM_SPEED_FAST 200
static const lv_img_dsc_t *bc_fast[]  = { &bongo_cat_both2, &bongo_cat_both1,
                                           &bongo_cat_none, &bongo_cat_none };

/* --- Frog Piano --- */
static const lv_img_dsc_t *fp_idle[]  = { &frog_piano_both1, &frog_piano_both1,
                                           &frog_piano_both1, &frog_piano_none };
static const lv_img_dsc_t *fp_slow[]  = { &frog_piano_left1, &frog_piano_none, &frog_piano_none,
                                           &frog_piano_right1, &frog_piano_none, &frog_piano_none };
static const lv_img_dsc_t *fp_mid[]   = { &frog_piano_left2, &frog_piano_left1, &frog_piano_none,
                                           &frog_piano_right2, &frog_piano_right1, &frog_piano_none };
static const lv_img_dsc_t *fp_fast[]  = { &frog_piano_both2, &frog_piano_both1,
                                           &frog_piano_none, &frog_piano_none };

/* ── 动物描述符表 ──────────────────────────────────────────── */
struct animal_def {
    const lv_img_dsc_t **idle;  size_t idle_cnt;
    const lv_img_dsc_t **slow;  size_t slow_cnt;
    const lv_img_dsc_t **mid;   size_t mid_cnt;
    const lv_img_dsc_t **fast;  size_t fast_cnt;
};

static const struct animal_def animals[] = {
    { bc_idle, ARRAY_SIZE(bc_idle), bc_slow, ARRAY_SIZE(bc_slow),
      bc_mid,  ARRAY_SIZE(bc_mid),  bc_fast, ARRAY_SIZE(bc_fast) },
    { fp_idle, ARRAY_SIZE(fp_idle), fp_slow, ARRAY_SIZE(fp_slow),
      fp_mid,  ARRAY_SIZE(fp_mid),  fp_fast, ARRAY_SIZE(fp_fast) },
};
#define ANIMAL_COUNT ARRAY_SIZE(animals)

/* ── 状态变量 ──────────────────────────────────────────────── */
static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static int64_t last_anim_update_time = 0;
#define ANIM_UPDATE_INTERVAL_MS 200

static uint8_t current_animal = 0;

enum anim_state {
    anim_state_none, anim_state_idle,
    anim_state_slow, anim_state_mid, anim_state_fast
} current_anim_state;

/* ── 动画切换逻辑 ──────────────────────────────────────────── */
static void set_animation(lv_obj_t *animing, uint8_t wpm) {
    int64_t now = k_uptime_get();
    if ((now - last_anim_update_time) < ANIM_UPDATE_INTERVAL_MS) return;
    last_anim_update_time = now;

    const struct animal_def *a = &animals[current_animal];

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

/* ── WPM 事件监听（原有逻辑） ──────────────────────────────── */
struct bongo_cat_wpm_status_state { uint8_t wpm; };

struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state){ .wpm = ev ? ev->state : 0 };
}

void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_animation(widget->obj, state.wpm);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                            bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

/* ── F24 切换键监听（新增） ─────────────────────────────────── */
static int animal_switch_event_handler(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (!ev || !ev->state) return ZMK_EV_EVENT_BUBBLE;

    if (ev->keycode == CONFIG_ZMK_BONGO_SWITCH_KEYCODE) {
        current_animal = (current_animal + 1) % ANIMAL_COUNT;
        current_anim_state = anim_state_none; /* 强制重新加载帧 */
        LOG_INF("Animation switched to %d", current_animal);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(animal_switch, animal_switch_event_handler);
ZMK_SUBSCRIPTION(animal_switch, zmk_keycode_state_changed);

/* ── Widget 初始化（与原版相同） ───────────────────────────── */
int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_center(widget->obj);
    sys_slist_append(&widgets, &widget->node);
    widget_bongo_cat_init();
    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}
