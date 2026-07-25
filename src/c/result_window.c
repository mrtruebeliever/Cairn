#include "result_window.h"
#include "art.h"
#include "backlight.h"
#include "home_window.h"
#include "i18n.h"
#include "sfx.h"
#include "stats.h"
#include <string.h>

static Window *s_win;
static Layer *s_lay;
static SessionResult s_res;
static uint32_t s_fresh;
static int8_t s_badge_page;     // index into the fresh badges, -1 = summary
static GBitmap *s_badge_bmp;    // medal art for s_badge_page, NULL on the summary
static AppTimer *s_anim_timer;
static int8_t s_stones_shown;
static int8_t s_glow_ticks;

#define GROW_MS 600
#define GLOW_MS 220

static int next_fresh_badge(int from) {
  for (int b = from; b < NUM_BADGES; b++) {
    if (s_fresh & (1u << b)) return b;
  }
  return -1;
}

// The medal art, one 72x72 image per badge. Only the page on screen is held in
// memory: six at once is pointless when they are shown one after the other.
// Listed by name rather than RESOURCE_ID_IMG_BADGE_0 + badge, so a reordering of
// the resources in package.json cannot silently hand out the wrong medal.
static void badge_bitmap_load(int badge) {
  static const uint32_t ids[NUM_BADGES] = {
    RESOURCE_ID_IMG_BADGE_0, RESOURCE_ID_IMG_BADGE_1, RESOURCE_ID_IMG_BADGE_2,
    RESOURCE_ID_IMG_BADGE_3, RESOURCE_ID_IMG_BADGE_4, RESOURCE_ID_IMG_BADGE_5,
  };
  if (s_badge_bmp) {
    gbitmap_destroy(s_badge_bmp);
    s_badge_bmp = NULL;
  }
  if (badge >= 0 && badge < NUM_BADGES) s_badge_bmp = gbitmap_create_with_resource(ids[badge]);
}

// ---------------------------------------------------------------------------
// Cairn growth animation: the new stone lands a beat after the screen appears,
// then the top stone flashes a few times.
// ---------------------------------------------------------------------------
static void glow_tick(void *data) {
  s_anim_timer = NULL;
  if (--s_glow_ticks > 0) s_anim_timer = app_timer_register(GLOW_MS, glow_tick, NULL);
  layer_mark_dirty(s_lay);
}

static void grow_tick(void *data) {
  s_anim_timer = NULL;
  s_stones_shown = stats_cairn_stones();
  s_glow_ticks = 6;
  s_anim_timer = app_timer_register(GLOW_MS, glow_tick, NULL);
  layer_mark_dirty(s_lay);
}

static void stop_anim(void) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static void draw_badge_page(GContext *ctx, GRect b, int badge) {
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  draw_center(ctx, S(S_BADGE_NEW), f18b, 16, 24, b, COL_ACCENT);
  if (s_badge_bmp) {
    // Same 72 px footprint the primitive medal had. GCompOpSet keeps the corners
    // around the round medal transparent so the night sky shows through.
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, s_badge_bmp, GRect(b.size.w / 2 - 36, 60, 72, 72));
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
  } else {
    draw_badge(ctx, GPoint(b.size.w / 2, 96), 36, badge, true);
  }
  draw_center(ctx, badge_name(badge), f24b, 142, 30, b, COL_TEXT);
  draw_center(ctx, badge_desc(badge), f14, 172, 18, b, COL_DIM);
  draw_center(ctx, S(S_HINT_CLOSE), f14, 206, 18, b, COL_DIM);
}

// Measures a string so the pieces of the duration/heart-rate row can be laid out
// left to right without guessing at character widths.
static int16_t text_w(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 200, 40),
             GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft).w;
}

static void draw_summary(GContext *ctx, GRect b) {
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  char buf[48];

  // The heading spans the full width. It used to share the line with the cairn
  // column, which truncated "New best score!" to "New best sco..." for no reason
  // — nothing else is drawn up here.
  graphics_context_set_text_color(ctx, s_res.new_best ? COL_ACCENT : COL_TEXT);
  graphics_draw_text(ctx, s_res.new_best ? S(S_NEW_BEST) : S(S_RESULT_TITLE), f18b,
                     GRect(6, 2, b.size.w - 12, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_context_set_text_color(ctx, COL_DIM);
  graphics_draw_text(ctx, S(S_RESULT_CALM), f14, GRect(6, 26, 90, 16),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  snprintf(buf, sizeof(buf), "%d", (int)s_res.score);
  graphics_context_set_text_color(ctx, COL_ACCENT);
  graphics_draw_text(ctx, buf, f28b, GRect(6, 40, 90, 36),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  // The cairn grows beside the score. The streak has no text line here: the stack
  // *is* the streak, and Home spells the number out a moment later. Capped at 5
  // stones so a maxed-out cairn stays clear of the heading.
  int stones = s_stones_shown;
  if (stones > 5) stones = 5;
  draw_cairn(ctx, GPoint(b.size.w - 36, 84), stones, 40, (s_glow_ticks % 2) == 1);

  // Duration and the heart-rate delta share one full-width line at the larger
  // size. Without a reading the "no heart rate" note sits beside the duration
  // rather than below it, so the coaching line below keeps all four of its rows —
  // and that is exactly the case whose advice is the longest.
  int16_t x = 6;
  snprintf(buf, sizeof(buf), "%d:%02d", s_res.dur_s / 60, s_res.dur_s % 60);
  int16_t w = text_w(buf, f24b);
  graphics_context_set_text_color(ctx, COL_VALUE);
  graphics_draw_text(ctx, buf, f24b, GRect(x, 86, w + 4, 30),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  x += w + 14;

  if (s_res.has_hr) {
    snprintf(buf, sizeof(buf), "%d", (int)s_res.hr_start);
    w = text_w(buf, f24b);
    graphics_context_set_text_color(ctx, COL_HR);
    graphics_draw_text(ctx, buf, f24b, GRect(x, 86, w + 4, 30),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    x += w + 6;
    draw_delta_arrow(ctx, GPoint(x + 7, 101), COL_HR);
    x += 18;
    snprintf(buf, sizeof(buf), "%d bpm", (int)s_res.hr_end);
    graphics_draw_text(ctx, buf, f24b, GRect(x, 86, b.size.w - x - 6, 30),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  } else {
    graphics_context_set_text_color(ctx, COL_DIM);
    graphics_draw_text(ctx, S(S_RESULT_NO_HR), f18, GRect(x, 92, b.size.w - x - 6, 22),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }

  graphics_context_set_stroke_color(ctx, COL_SAND);
  graphics_draw_line(ctx, GPoint(6, 118), GPoint(b.size.w - 6, 118));

  // The one coaching line for this session: four rows of Gothic 18, which is
  // enough for the wordiest advice in every language.
  draw_wrapped(ctx, S(insights_session_tip(&s_res)), f18,
               GRect(6, 122, b.size.w - 12, 78), COL_TEXT);

  draw_center(ctx, S(S_HINT_CLOSE), f14, 202, 20, b, COL_DIM);
}

static void result_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  if (s_badge_page >= 0) draw_badge_page(ctx, b, s_badge_page);
  else draw_summary(ctx, b);
}

// ---------------------------------------------------------------------------
// Input & lifecycle
// ---------------------------------------------------------------------------

static void res_select(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  if (s_badge_page >= 0) {
    int next = next_fresh_badge(s_badge_page + 1);
    s_badge_page = (int8_t)next;
    badge_bitmap_load(next);
    if (next >= 0) cue_badge();
    layer_mark_dirty(s_lay);
    return;
  }
  window_stack_pop(true);
}

static void res_clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, res_select);
}

static void res_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  badge_bitmap_load(s_badge_page);
  s_lay = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_lay, result_update);
  layer_add_child(root, s_lay);
}

static void res_unload(Window *window) {
  stop_anim();
  badge_bitmap_load(-1);
  layer_destroy(s_lay);
  s_lay = NULL;
  window_destroy(s_win);
  s_win = NULL;
  home_window_refresh();
}

static void res_appear(Window *window) {
  backlight_apply();
  int final = stats_cairn_stones();
  s_stones_shown = (int8_t)(final > 0 ? final - 1 : 0);
  s_glow_ticks = 0;
  stop_anim();
  s_anim_timer = app_timer_register(GROW_MS, grow_tick, NULL);
  if (s_badge_page >= 0) cue_badge();
}

void result_window_push(const SessionResult *res, uint32_t fresh_badges) {
  s_res = *res;
  s_fresh = fresh_badges;
  s_badge_page = (int8_t)next_fresh_badge(0);

  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_click_config_provider(s_win, res_clicks);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = res_load, .unload = res_unload, .appear = res_appear,
  });
  window_stack_push(s_win, true);
}
