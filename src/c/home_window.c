#include "home_window.h"
#include "art.h"
#include "backlight.h"
#include "history_window.h"
#include "i18n.h"
#include "insights_window.h"
#include "session.h"
#include "settings.h"
#include "sfx.h"
#include "stats.h"

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_hint_timer;
static uint8_t s_hint_idx;
static uint8_t s_hr;        // last reading, 0 = nothing yet
static bool s_hr_possible;  // false in the emulator, which has no sensor

#define HINT_MS 3000
#define SHORE_Y 130   // the waterline the cairn stands at on Home

// The four button gestures don't fit on one line, so the bottom row cycles
// through them.
static const int HINTS[] = {S_HINT_PRESET, S_HINT_INSIGHTS, S_HINT_HISTORY, S_HINT_SETTINGS};
#define NUM_HINTS (int)(sizeof(HINTS) / sizeof(HINTS[0]))

// "55" tenths -> "5.5", "40" -> "4". Whole seconds lose the ".0" so the common
// presets read as 4-4-4-4 rather than 4.0-4.0-4.0-4.0.
static void fmt_tenths(uint16_t t, char *buf, size_t len) {
  if (t > CUSTOM_MAX) t = CUSTOM_MAX;   // bounds the output at "12.0"
  unsigned whole = t / 10u, frac = t % 10u;
  if (frac == 0) snprintf(buf, len, "%u", whole);
  else snprintf(buf, len, "%u.%u", whole, frac);
}

static void fmt_pattern(char *buf, size_t len) {
  BreathPattern p = settings_pattern();
  char a[6], b[6], c[6], d[6];
  fmt_tenths(p.in, a, sizeof(a));
  fmt_tenths(p.out, c, sizeof(c));
  if (p.hold1 || p.hold2) {
    fmt_tenths(p.hold1, b, sizeof(b));
    fmt_tenths(p.hold2, d, sizeof(d));
    if (p.hold2) snprintf(buf, len, "%s-%s-%s-%s", a, b, c, d);
    else snprintf(buf, len, "%s-%s-%s", a, b, c);
  } else {
    snprintf(buf, len, "%s-%s", a, c);
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  char buf[64];

  // Streak headline.
  if (g_stats.streak > 0) {
    snprintf(buf, sizeof(buf), S(S_HOME_STREAK), (int)g_stats.streak);
    draw_center(ctx, buf, f18b, 4, 22, b, COL_ACCENT);
  } else {
    draw_center(ctx, S(S_HOME_STREAK_NONE), f18b, 4, 22, b, COL_DIM);
  }

  // Live heart rate in the corner: the sensor is already sampling at 1 Hz, so
  // this shows it warming up before the user even starts a session.
  if (g_set.show_hr && s_hr_possible) {
    draw_heart(ctx, GPoint(b.size.w - 26, 12), 7, s_hr > 0 ? COL_HR : COL_STONE_DARK);
    if (s_hr > 0) {
      snprintf(buf, sizeof(buf), "%d", (int)s_hr);
      graphics_context_set_text_color(ctx, COL_HR);
      graphics_draw_text(ctx, buf, f14, GRect(b.size.w - 18, 2, 16, 18),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
  }

  // Last session's calm score, or — on a fresh install — a line explaining what
  // the empty patch of ground below is for.
  SessionRecord last;
  if (stats_history_get(0, &last)) {
    snprintf(buf, sizeof(buf), S(S_HOME_LAST), (int)last.score);
    draw_center(ctx, buf, f18, 24, 22, b, COL_DIM);
  } else {
    draw_center(ctx, S(S_HOME_NEVER), f18, 24, 22, b, COL_DIM);
    graphics_context_set_text_color(ctx, COL_VALUE);
    graphics_draw_text(ctx, S(S_HOME_FIRST), f18, GRect(12, 58, b.size.w - 24, 60),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  // The same scene as the session screen, at rest: the streak cairn standing on
  // the shore with the tide out. Capped at 8 stones so a maxed-out cairn never
  // reaches the score line above it.
  int stones = stats_cairn_stones();
  if (stones > 8) stones = 8;
  draw_shore(ctx, b, SHORE_Y);
  draw_cairn(ctx, GPoint(b.size.w / 2, SHORE_Y), stones, 64, g_stats.streak >= 7);
  draw_tide(ctx, b, SHORE_Y + 2, 0, SHORE_Y + 14);

  // Selected pattern and duration.
  draw_center(ctx, settings_preset_name(g_set.preset_idx), f24b, 146, 30, b, COL_TEXT);
  char pat[40], mins[16];
  fmt_pattern(pat, sizeof(pat));
  snprintf(mins, sizeof(mins), S(S_MIN_FMT), (int)settings_len_min());
  snprintf(buf, sizeof(buf), "%s  ·  %s", pat, mins);
  draw_center(ctx, buf, f18, 171, 22, b, COL_VALUE);

  draw_center(ctx, S(S_HINT_START), f18b, 190, 22, b, COL_TEXT);
  draw_center(ctx, S(HINTS[s_hint_idx]), f14, 208, 18, b, COL_DIM);
}

static void hint_tick(void *data) {
  s_hint_idx = (s_hint_idx + 1) % NUM_HINTS;
  layer_mark_dirty(s_layer);
  s_hint_timer = app_timer_register(HINT_MS, hint_tick, NULL);
}

// The health service only signals that something changed; the value itself is
// read by polling.
static void health_handler(HealthEventType event, void *context) {
  if (event != HealthEventHeartRateUpdate && event != HealthEventSignificantUpdate) return;
  int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm >= 30 && bpm <= 220) {
    s_hr = (uint8_t)bpm;
    if (s_layer) layer_mark_dirty(s_layer);
  }
}

static void stop_hints(void) {
  if (s_hint_timer) {
    app_timer_cancel(s_hint_timer);
    s_hint_timer = NULL;
  }
}

void home_window_refresh(void) {
  if (s_layer) layer_mark_dirty(s_layer);
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  session_window_push();
}

static void select_long(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  insights_window_push();
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  settings_set_preset((g_set.preset_idx + NUM_PRESETS - 1) % NUM_PRESETS);
  layer_mark_dirty(s_layer);
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  settings_set_preset(g_set.preset_idx + 1);
  layer_mark_dirty(s_layer);
}

static void up_long(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  history_window_push();
}

static void down_long(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  settings_window_push();
}

static void click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_UP, up_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
  window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long, NULL);
  window_long_click_subscribe(BUTTON_ID_UP, 500, up_long, NULL);
  window_long_click_subscribe(BUTTON_ID_DOWN, 500, down_long, NULL);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
  stop_hints();
  layer_destroy(s_layer);
  s_layer = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

static void window_appear(Window *window) {
  backlight_apply();
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(
      HealthMetricHeartRateBPM, time(NULL) - 60, time(NULL));
  s_hr_possible = (mask & HealthServiceAccessibilityMaskAvailable) != 0;
  health_service_events_subscribe(health_handler, NULL);
  health_handler(HealthEventHeartRateUpdate, NULL);
  s_hint_idx = 0;
  stop_hints();
  s_hint_timer = app_timer_register(HINT_MS, hint_tick, NULL);
  layer_mark_dirty(s_layer);
}

static void window_disappear(Window *window) {
  stop_hints();
}

void home_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COL_BG);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
      .appear = window_appear,
      .disappear = window_disappear,
    });
  }
  window_stack_push(s_window, true);
}
