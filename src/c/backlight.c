#include "backlight.h"
#include "settings.h"

static AppTimer *s_timer;

static void timeout(void *data) {
  s_timer = NULL;
  light_enable(false);
}

static void stop_timer(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

void backlight_touch(void) {
  if (g_set.bl_mode != BL_TIMED) return;
  light_enable(true);
  if (s_timer) app_timer_reschedule(s_timer, BL_TIMEOUT_MS);
  else s_timer = app_timer_register(BL_TIMEOUT_MS, timeout, NULL);
}

void backlight_apply(void) {
  stop_timer();
  switch (g_set.bl_mode) {
    case BL_ALWAYS: light_enable(true); break;
    case BL_TIMED:  backlight_touch(); break;
    default:        light_enable(false); break;
  }
}

void backlight_release(void) {
  stop_timer();
  light_enable(false);
}
