#include "insights_window.h"
#include "touch_nav.h"
#include "art.h"
#include "backlight.h"
#include "comms.h"
#include "i18n.h"
#include "insights.h"
#include "stats.h"
#include <string.h>

// Progress view: calm score and resting heart rate over 7 and 30 days with a
// direction indicator, the tip of the week, and the badge shelf. Numbers come
// from the phone's full history when it is reachable, otherwise from the last
// HISTORY_MAX sessions on the watch.

static Window *s_win;
static Layer *s_lay;
static TrendData s_trend;

#define LBL_X   BY_SIZE(5, 3)
#define LBL_W   BY_SIZE(96, 62)
#define C7_X    BY_SIZE(100, 60)
#define C7_W    BY_SIZE(50, 40)
#define C30_X   BY_SIZE(152, 101)
#define C30_W   BY_SIZE(43, 40)
#define ROW_Y0    BY_SIZE(41, 33)
#define ROW_PITCH BY_SIZE(22, 17)
#define TABLE_END (ROW_Y0 + 3 * ROW_PITCH + 1)   // the rule under the table

static void draw_value(GContext *ctx, int8_t v, GFont f, GRect r, GColor color) {
  char buf[8];
  if (v < 0) snprintf(buf, sizeof(buf), "-");
  else snprintf(buf, sizeof(buf), "%d", (int)v);
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, buf, f, r, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentRight, NULL);
}

static void draw_row(GContext *ctx, int16_t y, const char *label,
                     int8_t v7, int8_t v30, int dir, bool show_dir, GColor color) {
  GFont f   = BY_SIZE(fonts_get_system_font(FONT_KEY_GOTHIC_18),
                      fonts_get_system_font(FONT_KEY_GOTHIC_14));
  GFont fb  = BY_SIZE(fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                      fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD));
  const int16_t h = ROW_PITCH;

  graphics_context_set_text_color(ctx, COL_TEXT);
  graphics_draw_text(ctx, label, f, GRect(LBL_X, y + 1, LBL_W, h - 2),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  draw_value(ctx, v7, fb, GRect(C7_X, y, C7_W - 14, h), color);
  if (show_dir && v7 >= 0) {
    draw_trend_arrow(ctx, GPoint(C7_X + C7_W - 6, y + h / 2), dir,
                     dir == 0 ? COL_DIM : (dir > 0 ? COL_ACCENT : COL_PHASE_IN));
  }
  draw_value(ctx, v30, f, GRect(C30_X, y + 1, C30_W, h - 2), COL_DIM);
}

static void insights_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, COL_TEXT);
  graphics_draw_text(ctx, S(S_INSIGHTS_TITLE), f18b,
                     PBL_IF_ROUND_ELSE(GRect(0, 0, b.size.w, 22),
                                       GRect(LBL_X, BY_SIZE(0, -3), 110, 22)),
                     GTextOverflowModeTrailingEllipsis, TITLE_ALIGN, NULL);
  if (!s_trend.from_phone) {
    // On a round screen the title's row has no corner to spare, but the label
    // column of the header row below it is empty.
    graphics_context_set_text_color(ctx, COL_DIM);
    graphics_draw_text(ctx, S(S_IN_LOCAL), f14,
                       PBL_IF_ROUND_ELSE(GRect(LBL_X, 24, LBL_W - 4, 18),
                                         GRect(BY_SIZE(96, 62), BY_SIZE(4, 0),
                                               b.size.w - BY_SIZE(102, 65), 18)),
                       GTextOverflowModeTrailingEllipsis,
                       PBL_IF_ROUND_ELSE(GTextAlignmentLeft, GTextAlignmentRight), NULL);
  }

  // Column headers.
  graphics_context_set_text_color(ctx, COL_DIM);
  const int16_t head_y = BY_SIZE(22, 16);
  graphics_draw_text(ctx, S(S_IN_7D), f14, GRect(C7_X, head_y, C7_W, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  graphics_draw_text(ctx, S(S_IN_30D), f14, GRect(C30_X, head_y, C30_W, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  graphics_context_set_stroke_color(ctx, COL_SAND);
  graphics_draw_line(ctx, GPoint(LBL_X, ROW_Y0 - 1), GPoint(b.size.w - LBL_X, ROW_Y0 - 1));

  draw_row(ctx, ROW_Y0, S(S_IN_CALM), s_trend.score_7, s_trend.score_30,
           s_trend.dir_score, true, COL_ACCENT);
  draw_row(ctx, ROW_Y0 + ROW_PITCH, S(S_IN_RESTHR), s_trend.hr_7, s_trend.hr_30,
           s_trend.dir_hr, true, COL_HR);
  // A streak is a single number: the 30-day column stays empty.
  draw_row(ctx, ROW_Y0 + 2 * ROW_PITCH, S(S_IN_STREAK), s_trend.streak, -1, 0, false, COL_VALUE);

  graphics_draw_line(ctx, GPoint(LBL_X, TABLE_END), GPoint(b.size.w - LBL_X, TABLE_END));

  // Tip of the week. Four lines at Gothic 18 fit the longest advice in every
  // language, which is why the table above is packed tighter than it looks.
  char tip[160];
  const char *text = insights_trend_text(&s_trend, tip, sizeof(tip));
  graphics_context_set_text_color(ctx, COL_ACCENT);
  graphics_draw_text(ctx, S(S_TIP_WEEK), f14, GRect(LBL_X, TABLE_END + 2, b.size.w - 10, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  // Compact: Gothic 14 and the room down to the badge shelf, about four lines.
  draw_wrapped(ctx, text ? text : S(S_NO_DATA), BY_SIZE(f18, f14),
               GRect(LBL_X, TABLE_END + 19, b.size.w - 2 * LBL_X, BY_SIZE(78, 48)),
               text ? COL_TEXT : COL_DIM);

  // Badge shelf: earned badges light up, the rest stay grey.
  // A round screen is narrowest down here, so the shelf shrinks and closes up.
  const int16_t margin = PBL_IF_ROUND_ELSE(50, BY_SIZE(20, 14));
  for (int i = 0; i < NUM_BADGES; i++) {
    int16_t x = margin + i * ((b.size.w - 2 * margin) / (NUM_BADGES - 1));
    draw_badge(ctx, GPoint(x, PBL_IF_ROUND_ELSE(213, BY_SIZE(217, 160))),
               PBL_IF_ROUND_ELSE(7, BY_SIZE(9, 7)), i,
               (g_stats.badges & (1u << i)) != 0);
  }
}

void insights_window_set_trend(const TrendData *t) {
  s_trend = *t;
  if (s_lay) layer_mark_dirty(s_lay);
}

static void ins_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_lay = layer_create(panel_frame(root));
  layer_set_update_proc(s_lay, insights_update);
  layer_add_child(root, s_lay);
}

static void ins_unload(Window *window) {
  layer_destroy(s_lay);
  s_lay = NULL;
  window_destroy(s_win);
  s_win = NULL;
}

// --- Touch: nothing to scroll or activate here, so only the back swipe ---

bool insights_window_is_top(void) {
  return s_win && window_stack_get_top_window() == s_win;
}

void insights_window_handle_touch(const TouchEvent *event) {
  if (touch_nav_feed(NULL, event, NULL) == TOUCH_NAV_BACK) {
    window_stack_pop(true);
  }
}

void insights_window_push(void) {
  // Show the watch's own numbers immediately; the phone's fuller answer
  // replaces them via insights_window_set_trend() when it arrives.
  insights_local_trend(&s_trend);

  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_touch_bridge_disabled(s_win, true);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = ins_load, .unload = ins_unload,
  });
  window_stack_push(s_win, true);
  backlight_apply();
  comms_request_trend();
}
