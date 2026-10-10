#include "backlight.h"
#include "settings.h"

static AppTimer *s_timer;

#define TINT_EMPTY 0xFFC88Cu   // low tide: warm sand
#define TINT_FULL  0x78C8FFu   // high tide: sea blue
#define TINT_STEPS 32          // a step only every ~3%, so a 25 fps session
                               // doesn't hit the LED driver on every frame
static int s_tint_step = -1;   // -1 = the user's own colour

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

static uint32_t lerp_channel(uint32_t a, uint32_t b, int shift, int step) {
  int32_t ca = (a >> shift) & 0xFF, cb = (b >> shift) & 0xFF;
  return (uint32_t)(ca + (cb - ca) * step / TINT_STEPS) << shift;
}

void backlight_tint_breath(int32_t fullness) {
  if (fullness < 0) fullness = 0;
  if (fullness > 1000) fullness = 1000;
  int step = (int)(fullness * TINT_STEPS / 1000);
  if (step == s_tint_step) return;
  s_tint_step = step;
  light_set_color_rgb888(lerp_channel(TINT_EMPTY, TINT_FULL, 16, step) |
                         lerp_channel(TINT_EMPTY, TINT_FULL, 8, step) |
                         lerp_channel(TINT_EMPTY, TINT_FULL, 0, step));
}

void backlight_tint_reset(void) {
  if (s_tint_step < 0) return;
  s_tint_step = -1;
  light_set_system_color();
}
