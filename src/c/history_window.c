#include "history_window.h"
#include "touch_nav.h"
#include "art.h"
#include "backlight.h"
#include "i18n.h"
#include "settings.h"
#include "stats.h"

// The recent sessions kept on the watch. A MenuLayer here (rather than the
// custom drawing used elsewhere) because the list scrolls and can be long —
// same pattern as PebbleDoist's task list.

static Window *s_win;
static MenuLayer *s_menu;
static TextLayer *s_empty;

static uint16_t num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return (uint16_t)stats_history_count();
}

static int16_t cell_height(MenuLayer *menu, MenuIndex *idx, void *ctx) {
  return 44;
}

static void draw_row(GContext *ctx, const Layer *cell, MenuIndex *idx, void *data) {
  SessionRecord r;
  if (!stats_history_get(idx->row, &r)) return;

  GRect b = layer_get_bounds(cell);
  bool selected = menu_cell_layer_is_highlighted(cell);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  char title[40], sub[48];
  // Date and score in the title — the two things the user scans for — with the
  // pattern and the closing heart rate underneath.
  snprintf(title, sizeof(title), "%d/%02d  ·  %s %d",
           (int)(r.date % 100), (int)((r.date / 100) % 100),
           S(S_RESULT_CALM), (int)r.score);
  if (r.hr_start && r.hr_end) {
    snprintf(sub, sizeof(sub), "%d:%02d  ·  %s  ·  %d bpm",
             r.dur_s / 60, r.dur_s % 60, settings_preset_name(r.preset), (int)r.hr_end);
  } else {
    snprintf(sub, sizeof(sub), "%d:%02d  ·  %s",
             r.dur_s / 60, r.dur_s % 60, settings_preset_name(r.preset));
  }

  graphics_context_set_text_color(ctx, selected ? GColorBlack : COL_TEXT);
  graphics_draw_text(ctx, title, f18b, GRect(6, 0, b.size.w - 12, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_context_set_text_color(ctx, selected ? GColorBlack : COL_DIM);
  graphics_draw_text(ctx, sub, f18, GRect(6, 22, b.size.w - 12, 22),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void hist_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect b = layer_get_bounds(root);

  if (stats_history_count() == 0) {
    s_empty = text_layer_create(GRect(8, b.size.h / 2 - 20, b.size.w - 16, 40));
    text_layer_set_background_color(s_empty, GColorClear);
    text_layer_set_text_color(s_empty, COL_DIM);
    text_layer_set_font(s_empty, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(s_empty, GTextAlignmentCenter);
    text_layer_set_text(s_empty, S(S_HISTORY_EMPTY));
    layer_add_child(root, text_layer_get_layer(s_empty));
    return;
  }

  s_menu = menu_layer_create(b);
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks){
    .get_num_rows = num_rows,
    .get_cell_height = cell_height,
    .draw_row = draw_row,
  });
  menu_layer_set_normal_colors(s_menu, COL_BG, COL_TEXT);
  menu_layer_set_highlight_colors(s_menu, COL_VALUE, GColorBlack);
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge faults inside firmware on 4.33.1, so touch is handled
// from the raw stream here — see touch_nav.h. Rows are read-only, so a tap does
// nothing and only scrolling and the back swipe are wired up.

// Keeps the highlight on a visible row after a drag or glide, so the next
// UP/DOWN does not yank the list back to where the finger started.
static void touch_settled(void) {
  if (!s_menu) { return; }
  GRect frame = layer_get_frame(menu_layer_get_layer(s_menu));
  int row = touch_nav_row_at(s_menu, GPoint(frame.size.w / 2, frame.origin.y + frame.size.h / 2),
                             num_rows(s_menu, 0, NULL), cell_height);
  if (row >= 0) {
    menu_layer_set_selected_index(s_menu, MenuIndex(0, (uint16_t)row), MenuRowAlignNone, false);
    layer_mark_dirty(menu_layer_get_layer(s_menu));   // no scroll means no repaint otherwise
  }
}

bool history_window_is_top(void) {
  return s_win && window_stack_get_top_window() == s_win;
}

void history_window_handle_touch(const TouchEvent *event) {
  ScrollLayer *scroll = s_menu ? menu_layer_get_scroll_layer(s_menu) : NULL;
  if (touch_nav_feed(scroll, event, NULL) == TOUCH_NAV_BACK) {
    window_stack_pop(true);
  }
}

static void hist_unload(Window *window) {
  touch_nav_reset();
  if (s_menu) { menu_layer_destroy(s_menu); s_menu = NULL; }
  if (s_empty) { text_layer_destroy(s_empty); s_empty = NULL; }
  window_destroy(s_win);
  s_win = NULL;
}

void history_window_push(void) {
  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = hist_load, .unload = hist_unload,
  });
  window_set_touch_bridge_disabled(s_win, true);
  touch_nav_set_settled_handler(touch_settled);
  window_stack_push(s_win, true);
  backlight_apply();
}
