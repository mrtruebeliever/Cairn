#include "home_window.h"
#include "art.h"
#include "backlight.h"
#include "history_window.h"
#include "i18n.h"
#include "insights_window.h"
#include "touch_nav.h"
#include "session.h"
#include "settings.h"
#include "sfx.h"
#include "stats.h"

static Window *s_window;
static Layer *s_layer;
static uint8_t s_hr;        // last reading, 0 = nothing yet
static bool s_hr_possible;  // false in the emulator, which has no sensor

#define SHORE_Y BY_SIZE(130, 100)   // the waterline the cairn stands at on Home


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

// --- Button bar ----------------------------------------------------------------
//
// Five gestures live on this screen, and a line of text that cycled through
// them made the user wait for the one they wanted. Instead, beside each button
// sits what it does: a large icon for a press and a small, dimmer one under it
// for a long press -- the Pebble action bar convention, all visible at once.
//
//   UP      previous pattern / hold: history
//   SELECT  start            / hold: progress
//   DOWN    next pattern     / hold: settings

#define BAR_W BY_SIZE(28, 22)

static void fill_tri(GContext *ctx, GPoint a, GPoint b, GPoint c) {
  GPoint pts[3] = {a, b, c};
  GPathInfo info = {.num_points = 3, .points = pts};
  GPath *path = gpath_create(&info);
  if (!path) return;
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void icon_up(GContext *ctx, GPoint c) {
  fill_tri(ctx, GPoint(c.x - 7, c.y + 4), GPoint(c.x + 7, c.y + 4), GPoint(c.x, c.y - 4));
}
static void icon_down(GContext *ctx, GPoint c) {
  fill_tri(ctx, GPoint(c.x - 7, c.y - 4), GPoint(c.x + 7, c.y - 4), GPoint(c.x, c.y + 4));
}
static void icon_play(GContext *ctx, GPoint c) {
  fill_tri(ctx, GPoint(c.x - 4, c.y - 7), GPoint(c.x - 4, c.y + 7), GPoint(c.x + 6, c.y));
}
// History: a short list.
static void icon_list(GContext *ctx, GPoint c) {
  for (int i = -1; i <= 1; i++) {
    graphics_fill_rect(ctx, GRect(c.x - 6, c.y + i * 4 - 1, 2, 2), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(c.x - 2, c.y + i * 4 - 1, 8, 2), 0, GCornerNone);
  }
}
// Progress: a rising bar chart.
static void icon_chart(GContext *ctx, GPoint c) {
  for (int i = 0; i < 3; i++) {
    int16_t h = 4 + i * 3;
    graphics_fill_rect(ctx, GRect(c.x - 6 + i * 4, c.y + 5 - h, 3, h), 0, GCornerNone);
  }
}
// Settings: two sliders.
static void icon_sliders(GContext *ctx, GPoint c) {
  graphics_fill_rect(ctx, GRect(c.x - 6, c.y - 3, 12, 1), 0, GCornerNone);
  graphics_fill_rect(ctx, GRect(c.x - 6, c.y + 3, 12, 1), 0, GCornerNone);
  graphics_fill_circle(ctx, GPoint(c.x - 2, c.y - 3), 2);
  graphics_fill_circle(ctx, GPoint(c.x + 3, c.y + 3), 2);
}

// Where the bar's icons go horizontally at screen row `y` (layer coords). On a
// round screen they follow the bezel, inside the circle; on a rectangle they
// sit in a strip down the right edge.
static int16_t bar_x(GRect b, int16_t y, int16_t layer_top) {
#if defined(PBL_ROUND)
  int32_t r = b.size.w / 2, dy = (int32_t)(y + layer_top) - r;
  int32_t dx = 0;
  while ((dx + 1) * (dx + 1) + dy * dy <= r * r) dx++;   // isqrt, a few dozen steps
  return (int16_t)(r + dx - 16);
#else
  (void)y; (void)layer_top;
  return b.size.w - BAR_W / 2;
#endif
}

static void draw_button_bar(GContext *ctx, GRect b, int16_t layer_top, int16_t screen_h) {
#if !defined(PBL_ROUND)
  graphics_context_set_fill_color(ctx, COL_PANEL);
  graphics_fill_rect(ctx, GRect(b.size.w - BAR_W, 0, BAR_W, b.size.h), 0, GCornerNone);
#endif
  // The buttons sit at a sixth, a half and five sixths of the screen height.
  const int16_t centers[3] = {screen_h / 6, screen_h / 2, screen_h * 5 / 6};
  const int16_t gap = BY_SIZE(11, 9);
  void (*shorts[3])(GContext *, GPoint) = {icon_up, icon_play, icon_down};
  void (*longs[3])(GContext *, GPoint)  = {icon_list, icon_chart, icon_sliders};
  for (int i = 0; i < 3; i++) {
    int16_t ys = centers[i] - layer_top - gap / 2 - 2;
    int16_t yl = centers[i] - layer_top + gap;
#if defined(PBL_ROUND)
    // On the round screen the tide runs right under SELECT, out to the bezel,
    // and would swallow a grey icon; the long press goes above it instead.
    if (i == 1) yl = ys - gap - 2;
#endif
    graphics_context_set_fill_color(ctx, COL_TEXT);
    shorts[i](ctx, GPoint(bar_x(b, ys, layer_top), ys));
    graphics_context_set_fill_color(ctx, COL_DIM);
    longs[i](ctx, GPoint(bar_x(b, yl, layer_top), yl));
  }
}

static void update_proc(Layer *layer, GContext *ctx) {
  GRect full = layer_get_bounds(layer);
  // Text and the cairn centre in what the button bar leaves free. A round
  // screen keeps them on the circle's centre; its icons ride the bezel.
  GRect b = full;
#if !defined(PBL_ROUND)
  b.size.w -= BAR_W;
#endif
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  // The secondary lines step down a size on a compact screen.
  GFont fsub = BY_SIZE(f18, f14);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, full, 0, GCornerNone);

  char buf[64];

  // Streak headline.
  if (g_stats.streak > 0) {
    snprintf(buf, sizeof(buf), S(S_HOME_STREAK), (int)g_stats.streak);
    draw_center(ctx, buf, f18b, BY_SIZE(4, -2), 22, b, COL_ACCENT);
  } else {
    draw_center(ctx, S(S_HOME_STREAK_NONE), f18b, BY_SIZE(4, -2), 22, b, COL_DIM);
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
    draw_center(ctx, buf, fsub, BY_SIZE(24, 18), 22, b, COL_DIM);
  } else {
    draw_center(ctx, S(S_HOME_NEVER), fsub, BY_SIZE(24, 18), 22, b, COL_DIM);
    graphics_context_set_text_color(ctx, COL_VALUE);
    graphics_draw_text(ctx, S(S_HOME_FIRST), fsub,
                       BY_SIZE(GRect(12, 58, b.size.w - 24, 60), GRect(6, 38, b.size.w - 12, 36)),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }

  // The same scene as the session screen, at rest: the streak cairn standing on
  // the shore with the tide out. Capped at 8 stones so a maxed-out cairn never
  // reaches the score line above it.
  int stones = stats_cairn_stones();
  if (stones > BY_SIZE(8, 5)) stones = BY_SIZE(8, 5);
  draw_shore(ctx, full, SHORE_Y);
  draw_cairn(ctx, GPoint(b.size.w / 2, SHORE_Y), stones, BY_SIZE(64, 48), g_stats.streak >= 7);
  draw_tide(ctx, full, SHORE_Y + 2, 0, SHORE_Y + 14);

  // Selected pattern and duration.
  draw_center(ctx, settings_preset_name(g_set.preset_idx), BY_SIZE(f24b, f18b),
              BY_SIZE(146, 112), BY_SIZE(30, 22), b, COL_TEXT);
  char pat[40], mins[16];
  fmt_pattern(pat, sizeof(pat));
  snprintf(mins, sizeof(mins), S(S_MIN_FMT), (int)settings_len_min());
  snprintf(buf, sizeof(buf), "%s  ·  %s", pat, mins);
  draw_center(ctx, buf, fsub, BY_SIZE(171, 132), BY_SIZE(22, 18), b, COL_VALUE);

  GRect frame = layer_get_frame(layer);
  draw_button_bar(ctx, full, frame.origin.y, frame.origin.y * 2 + full.size.h);
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

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge faults inside firmware on 4.33.1, so touch is handled
// from the raw stream here (see touch_nav.h). Nothing on this screen scrolls, so
// the gestures mirror the buttons: tap starts a session like SELECT, and a
// vertical flick steps through the presets like UP/DOWN. A right swipe means
// back, which on this root window leaves the app — exactly what BACK does.

bool home_window_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void home_window_handle_touch(const TouchEvent *event) {
  switch (touch_nav_feed(NULL, event, NULL)) {
    case TOUCH_NAV_TAP:        select_click(NULL, NULL); break;
    case TOUCH_NAV_SWIPE_UP:   down_click(NULL, NULL); break;
    case TOUCH_NAV_SWIPE_DOWN: up_click(NULL, NULL); break;
    case TOUCH_NAV_BACK:       window_stack_pop(true); break;   // leaves the app, as BACK does
    default: break;
  }
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
  s_layer = layer_create(screen_frame(root, false));
  layer_set_update_proc(s_layer, update_proc);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
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
  layer_mark_dirty(s_layer);
}

void home_window_push(void) {
  if (!s_window) {
    s_window = window_create();
    window_set_background_color(s_window, COL_BG);
    window_set_click_config_provider(s_window, click_config);
    window_set_touch_bridge_disabled(s_window, true);
    window_set_window_handlers(s_window, (WindowHandlers){
      .load = window_load,
      .unload = window_unload,
      .appear = window_appear,
    });
  }
  window_stack_push(s_window, true);
}
