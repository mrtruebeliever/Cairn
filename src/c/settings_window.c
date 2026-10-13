#include "settings.h"
#include "touch_nav.h"
#include "i18n.h"
// ---------------------------------------------------------------------------
// On-watch settings screen. Custom-drawn rather than a MenuLayer so it matches
// the look of the rest of the app (same pattern as PebbleDiver's settings).
// ---------------------------------------------------------------------------
#include "art.h"
#include "backlight.h"
#include "comms.h"
#include "session.h"
#include "home_window.h"
#include "sfx.h"
#include "stats.h"

// Without a heart-rate sensor the Heart rate row sits past NUM_ROWS, so it is
// never listed while the code that handles it still compiles.
enum { ROW_PRESET, ROW_RHYTHM, ROW_LEN, ROW_SOUND, ROW_PHASE_SOUND, ROW_HAPTICS,
       ROW_BACKLIGHT,
#if HAS_HRM
       ROW_SHOW_HR,
#endif
       ROW_REMINDER, ROW_RESET, NUM_ROWS,
#if !HAS_HRM
       ROW_SHOW_HR,
#endif
};

// The rows follow the watch's text size (Settings -> Display), one step either
// side of its default -- Gothic 24 on the 228px screens, 18 on the compact one.
// Fewer rows fit as they grow; the list scrolls, which also keeps Reset stats
// -- the destructive one, last -- out of sight until wanted.
typedef struct {
  const char *font;
  int16_t row_h, text_dy, text_h;
} RowMetrics;

static const RowMetrics METRICS[] = {
  {FONT_KEY_GOTHIC_14, 18,  0, 16},
  {FONT_KEY_GOTHIC_18, 23,  1, 20},
  {FONT_KEY_GOTHIC_24, 30, -1, 28},
  {FONT_KEY_GOTHIC_28, 35, -2, 32},
};

#define ROW_Y0 BY_SIZE(28, 20)
#define HINT_RESERVE BY_SIZE(18, 0)   // the hint line under the rows
#define LAYOUT_ROWS_H (BY_SIZE(228, 168) - ROW_Y0 - HINT_RESERVE)

static const RowMetrics *s_m;
static int s_visible;   // rows on screen

static void pick_metrics(void) {
  int def = PBL_DISPLAY_HEIGHT >= 200 ? PreferredContentSizeLarge : PreferredContentSizeMedium;
  int step = (int)preferred_content_size() - def;
  if (step < -1) step = -1;
  if (step > 1) step = 1;
  s_m = &METRICS[BY_SIZE(2, 1) + step];
  s_visible = LAYOUT_ROWS_H / s_m->row_h;
  if (s_visible > NUM_ROWS) s_visible = NUM_ROWS;
}

#define ROW_H  (s_m->row_h)
#define TEXT_H (s_m->text_h)
#define VISIBLE_ROWS s_visible

static Window *s_win;
static Layer *s_lay;
static int8_t s_sel;
static bool s_reset_armed;
static int8_t s_edit_slot;   // -1 = not editing the custom rhythm
static int8_t s_top;         // first row on screen

static void fmt_secs(uint16_t tenths, char *buf, size_t len) {
  if (tenths > CUSTOM_MAX) tenths = CUSTOM_MAX;   // bounds the output at "12.0"
  unsigned whole = tenths / 10u, frac = tenths % 10u;
  if (frac == 0) snprintf(buf, len, "%u", whole);
  else snprintf(buf, len, "%u.%u", whole, frac);
}

static const char *row_label(int row) {
  switch (row) {
    case ROW_PRESET:    return S(S_OPT_PRESET);
    case ROW_RHYTHM:    return S(S_OPT_RHYTHM);
    case ROW_LEN:       return S(S_OPT_LEN);
    case ROW_SOUND:     return S(S_OPT_SOUND);
    case ROW_PHASE_SOUND: return S(S_OPT_PHASE_SOUND);
    case ROW_HAPTICS:   return S(S_OPT_HAPTICS);
    case ROW_BACKLIGHT: return S(S_OPT_BACKLIGHT);
    case ROW_SHOW_HR:   return S(S_OPT_SHOW_HR);
    case ROW_REMINDER:  return S(S_OPT_REMINDER);
    default:            return S(S_OPT_RESET);
  }
}

// Fills buf with the row's current value; returns false for rows without one.
static bool row_value(int row, char *buf, size_t len) {
  switch (row) {
    case ROW_PRESET:
      snprintf(buf, len, "%s", settings_preset_name(g_set.preset_idx));
      return true;
    case ROW_RHYTHM: {
      char a[6], b[6], c[6], d[6];
      fmt_secs(g_set.custom[0], a, sizeof(a));
      fmt_secs(g_set.custom[1], b, sizeof(b));
      fmt_secs(g_set.custom[2], c, sizeof(c));
      fmt_secs(g_set.custom[3], d, sizeof(d));
      snprintf(buf, len, "%s-%s-%s-%s", a, b, c, d);
      return true;
    }
    case ROW_LEN:
      snprintf(buf, len, S(S_MIN_FMT), (int)settings_len_min());
      return true;
    case ROW_SOUND:
      // A level the watch's own mute overrides says so, rather than leaving
      // the user to wonder why "Normal" makes no sound.
      snprintf(buf, len, "%s", g_set.sound_vol == 0 ? S(S_OFF)
                             : speaker_is_muted() ? S(S_SND_MUTED)
                             : g_set.sound_vol == 1 ? S(S_SND_SOFT)
                             : g_set.sound_vol == 2 ? S(S_SND_NORMAL)
                                                    : S(S_SND_LOUD));
      return true;
    case ROW_PHASE_SOUND:
      snprintf(buf, len, "%s", g_set.phase_sound ? S(S_ON) : S(S_OFF));
      return true;
    case ROW_HAPTICS:
      snprintf(buf, len, "%s", g_set.haptics_on ? S(S_ON) : S(S_OFF));
      return true;
    case ROW_BACKLIGHT:
      snprintf(buf, len, "%s", g_set.bl_mode == BL_ALWAYS ? S(S_BL_ALWAYS)
                             : g_set.bl_mode == BL_TIMED  ? S(S_BL_TIMED)
                                                          : S(S_OFF));
      return true;
    case ROW_SHOW_HR:
      snprintf(buf, len, "%s", g_set.show_hr ? S(S_ON) : S(S_OFF));
      return true;
    case ROW_REMINDER: {
      if (g_set.reminder == REMINDER_OFF) {
        snprintf(buf, len, "%s", S(S_OFF));
        return true;
      }
      struct tm tm = {.tm_hour = g_set.reminder / 60, .tm_min = g_set.reminder % 60};
      strftime(buf, len, clock_is_24h_style() ? "%H:%M" : "%I:%M %p", &tm);
      if (!clock_is_24h_style() && buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
      return true;
    }
    default:
      return false;
  }
}

static void settings_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  GFont frow = fonts_get_system_font(s_m->font);

#if COMPACT
  // No room below the rows: the rhythm editor's hint takes the title's place.
  if (s_edit_slot >= 0) {
    draw_center(ctx, S(S_RHYTHM_HINT), f14, 1, 18, b, COL_ACCENT);
  } else
#endif
  {
    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, S(S_SETTINGS_TITLE), f18b, GRect(8, BY_SIZE(2, -3), b.size.w - 16, 24),
                       GTextOverflowModeTrailingEllipsis, TITLE_ALIGN, NULL);
  }

  for (int i = s_top; i < NUM_ROWS && i < s_top + VISIBLE_ROWS; i++) {
    int16_t y = ROW_Y0 + (i - s_top) * ROW_H;
    GRect row = GRect(4, y, b.size.w - 8, ROW_H - 2);
    int16_t ty = y + s_m->text_dy;
    if (i == s_sel) {
      graphics_context_set_fill_color(ctx, COL_PANEL);
      graphics_fill_rect(ctx, row, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, s_edit_slot >= 0 ? COL_ACCENT : COL_TEXT);
      graphics_draw_round_rect(ctx, row, 4);
    }

    // Armed reset turns the whole row into a red confirmation prompt. The red is
    // the bar, not the text: red on the panel colour was the lowest-contrast
    // pair on screen, in the one state that has to be read before it is pressed.
    if (i == ROW_RESET && s_reset_armed) {
      graphics_context_set_fill_color(ctx, CBW(GColorRed, COL_PANEL));
      graphics_fill_rect(ctx, row, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, COL_TEXT);
      graphics_draw_round_rect(ctx, row, 4);
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, S(S_RESET_CONFIRM), frow,
                         GRect(row.origin.x + 6, ty, row.size.w - 12, TEXT_H),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      continue;
    }

    // Rows without a value column get the whole width. Reset stats is the one
    // that needs it -- FR "Effacer les stats", DE "Statistik loschen" and ES
    // "Borrar estadisticas" all overran the 96px the value column left behind,
    // truncating the label on the destructive row.
    // The value is measured, and the label gets what is left: at the larger
    // text sizes a fixed value column either truncated "Coherent" or the label.
    char val[28];
    bool has_val = row_value(i, val, sizeof(val));
    int16_t val_w = 0;
    if (has_val) {
      val_w = graphics_text_layout_get_content_size(
          val, frow, GRect(0, 0, row.size.w, TEXT_H), GTextOverflowModeWordWrap,
          GTextAlignmentLeft).w + 2;
      if (val_w > row.size.w / 2) val_w = row.size.w / 2;
    }
    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, row_label(i), frow,
                       GRect(row.origin.x + 6, ty,
                             row.size.w - 12 - (has_val ? val_w + 6 : 0), TEXT_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    if (!has_val) continue;

    // While editing the custom rhythm, underline the slot being changed.
    if (i == ROW_RHYTHM && s_edit_slot >= 0) {
      GRect vr = GRect(row.origin.x + row.size.w - val_w - 6, ty, val_w, TEXT_H);
      graphics_context_set_text_color(ctx, COL_ACCENT);
      graphics_draw_text(ctx, val, frow, vr, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentRight, NULL);

      // The underline has to be measured, not divided. The value is drawn
      // right-aligned and its four numbers are variable width ("5.5" against
      // "4"), so splitting the box into equal quarters from its left edge put
      // the marker nowhere near the number it claimed to point at -- for
      // "4-4-4-4" slot 0 landed entirely left of the first digit.
      int start = 0, end = (int)strlen(val), seen = 0;
      for (int k = 0; val[k]; k++) {
        if (val[k] != '-') continue;
        if (seen == s_edit_slot) { end = k; break; }
        seen++;
        start = k + 1;
      }
      // Measure inside a box the size of the one the text is drawn in, with
      // WordWrap: that is the combination that reports the text's own width.
      const GRect mbox = GRect(0, 0, val_w, TEXT_H);
      char part[28];
      int16_t w_all = graphics_text_layout_get_content_size(
          val, frow, mbox, GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
      int16_t w_pre = 0;
      if (start > 0) {
        memcpy(part, val, start);
        part[start] = '\0';
        w_pre = graphics_text_layout_get_content_size(
            part, frow, mbox, GTextOverflowModeWordWrap, GTextAlignmentLeft).w;
      }
      memcpy(part, val + start, end - start);
      part[end - start] = '\0';
      int16_t w_seg = graphics_text_layout_get_content_size(
          part, frow, mbox, GTextOverflowModeWordWrap, GTextAlignmentLeft).w;

      // One pixel above y + ROW_H - 3: the row's own outline runs along there, and while
      // editing that outline is drawn in COL_ACCENT too -- so the marker was
      // landing on top of it and could not be told apart from the border at
      // all. White also separates it from the accent-coloured digits above.
      int16_t tx = vr.origin.x + vr.size.w - w_all;   // right-aligned text start
      graphics_context_set_stroke_color(ctx, COL_TEXT);
      graphics_draw_line(ctx, GPoint(tx + w_pre, y + ROW_H - 4),
                              GPoint(tx + w_pre + w_seg - 1, y + ROW_H - 4));
      continue;
    }

    bool active = true;
    if (i == ROW_SOUND) active = g_set.sound_vol > 0 && !speaker_is_muted();
    // Breath sounds play at the Sound level, so they are dimmed with it.
    else if (i == ROW_PHASE_SOUND) active = g_set.phase_sound && g_set.sound_vol > 0
                                            && !speaker_is_muted();
    else if (i == ROW_HAPTICS) active = g_set.haptics_on;
    else if (i == ROW_BACKLIGHT) active = g_set.bl_mode != BL_OFF;
    else if (i == ROW_SHOW_HR) active = g_set.show_hr;
    else if (i == ROW_REMINDER) active = g_set.reminder != REMINDER_OFF;
    graphics_context_set_text_color(ctx, active ? COL_VALUE : COL_DIM);
    graphics_draw_text(ctx, val, frow,
                       GRect(row.origin.x + row.size.w - val_w - 6, ty, val_w, TEXT_H),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  // More rows above or below: a small arrow at the right end of the title
  // line and under the last row.
  graphics_context_set_fill_color(ctx, COL_DIM);
  int16_t ax = b.size.w - PBL_IF_ROUND_ELSE(40, 14);
  if (s_top > 0) {
    GPoint t[3] = {{ax - 5, ROW_Y0 - 6}, {ax + 5, ROW_Y0 - 6}, {ax, ROW_Y0 - 11}};
    GPathInfo info = {.num_points = 3, .points = t};
    GPath *path = gpath_create(&info);
    if (path) { gpath_draw_filled(ctx, path); gpath_destroy(path); }
  }
  if (s_top + VISIBLE_ROWS < NUM_ROWS) {
    int16_t ay = ROW_Y0 + VISIBLE_ROWS * ROW_H + 1;
    GPoint t[3] = {{ax - 5, ay}, {ax + 5, ay}, {ax, ay + 5}};
    GPathInfo info = {.num_points = 3, .points = t};
    GPath *path = gpath_create(&info);
    if (path) { gpath_draw_filled(ctx, path); gpath_destroy(path); }
  }

#if COMPACT
  return;   // the rows fill the screen; the rhythm hint went up top instead
#elif defined(PBL_ROUND)
  // The bottom of a round screen only fits a short line: the general hint is
  // the standard SELECT/BACK behaviour and can go, the rhythm editor's cannot.
  if (s_edit_slot < 0) return;
#endif
  graphics_context_set_text_color(ctx, COL_DIM);
  graphics_draw_text(ctx, s_edit_slot >= 0 ? S(S_RHYTHM_HINT) : S(S_SETTINGS_HINT), f14,
                     // Full width and one row up: the old box started at y=211
                     // with h=18, i.e. its last pixel row fell off a 228px
                     // screen, and DE filled the width exactly.
                     GRect(2, ROW_Y0 + VISIBLE_ROWS * ROW_H + 2, b.size.w - 4, 18),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void activate_row(void) {
  switch (s_sel) {
    case ROW_PRESET:
      settings_set_preset(g_set.preset_idx + 1);
      break;
    case ROW_RHYTHM:
      // Entering the editor also selects Custom, so the change is audible.
      s_edit_slot = 0;
      settings_set_preset(PRESET_CUSTOM);
      break;
    case ROW_LEN:
      settings_set_len(g_set.len_idx + 1);
      break;
    case ROW_SOUND:
      settings_set_sound(g_set.sound_vol + 1);
      if (g_set.sound_vol) cue_sample();
      break;
    case ROW_PHASE_SOUND:
      settings_set_phase_sound(!g_set.phase_sound);
      break;
    case ROW_HAPTICS:
      settings_set_haptics(!g_set.haptics_on);
      if (g_set.haptics_on) cue_phase_sample();
      break;
    case ROW_BACKLIGHT:
      settings_set_backlight(g_set.bl_mode + 1);
      backlight_apply();
      break;
    case ROW_SHOW_HR:
      settings_set_show_hr(!g_set.show_hr);
      hr_sampling_apply();
      break;
    case ROW_REMINDER:
      settings_next_reminder();
      break;
    case ROW_RESET:
      if (s_reset_armed) {
        stats_reset();
        comms_send_reset();   // the phone keeps the longer history; wipe it too
        s_reset_armed = false;
        cue_deny();
      } else {
        s_reset_armed = true;   // first press arms, a second confirms
      }
      break;
  }
  layer_mark_dirty(s_lay);
}

static void set_sel_click(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  if (s_edit_slot >= 0) {
    s_edit_slot++;
    if (s_edit_slot > 3) s_edit_slot = -1;   // past the last slot leaves the editor
    layer_mark_dirty(s_lay);
    return;
  }
  activate_row();
}

// UP/DOWN mean two things: move through the list, or — while the custom rhythm
// editor is open — raise/lower the highlighted value. UP always means "more".
static void set_move(int sel_delta, int value_delta) {
  backlight_touch();
  if (s_edit_slot >= 0) {
    settings_set_custom(s_edit_slot,
                        (int32_t)g_set.custom[s_edit_slot] + value_delta * CUSTOM_STEP);
    layer_mark_dirty(s_lay);
    return;
  }
  s_reset_armed = false;
  s_sel = (s_sel + NUM_ROWS + sel_delta) % NUM_ROWS;
  if (s_sel < s_top) s_top = s_sel;
  else if (s_sel >= s_top + VISIBLE_ROWS) s_top = s_sel - VISIBLE_ROWS + 1;
  layer_mark_dirty(s_lay);
}

static void set_up(ClickRecognizerRef rec, void *ctx)   { set_move(-1, +1); }
static void set_down(ClickRecognizerRef rec, void *ctx) { set_move(+1, -1); }

static void set_back(ClickRecognizerRef rec, void *ctx) {
  if (s_edit_slot >= 0) {
    s_edit_slot = -1;
    layer_mark_dirty(s_lay);
    return;
  }
  window_stack_pop(true);
}

// --- Touch -------------------------------------------------------------------
//
// The system touch bridge faults inside firmware on 4.33.1, so touch is handled
// from the raw stream here (see touch_nav.h). This list is custom-drawn rather
// than a MenuLayer, so the rows are hit-tested against the same ROW_Y0/ROW_H
// geometry settings_update() draws with. Gestures mirror the buttons, including
// inside the custom-rhythm editor, where UP/DOWN change the value.

bool settings_window_is_top(void) {
  return s_win && window_stack_get_top_window() == s_win;
}

void settings_window_handle_touch(const TouchEvent *event) {
  GPoint point;
  switch (touch_nav_feed(NULL, event, &point)) {
    case TOUCH_NAV_TAP: {
      if (s_edit_slot < 0) {
        int row = (point.y - ROW_Y0) / ROW_H;
        if (point.y < ROW_Y0 || row < 0 || row >= VISIBLE_ROWS) { return; }
        row += s_top;
        if (row >= NUM_ROWS) { return; }
        s_reset_armed = false;   // tapping elsewhere disarms, as moving away does
        s_sel = (int8_t)row;
      }
      set_sel_click(NULL, NULL);
      break;
    }
    case TOUCH_NAV_SWIPE_UP:   set_down(NULL, NULL); break;
    case TOUCH_NAV_SWIPE_DOWN: set_up(NULL, NULL); break;
    case TOUCH_NAV_BACK:       set_back(NULL, NULL); break;
    default: break;
  }
}

static void set_clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, set_sel_click);
  window_single_click_subscribe(BUTTON_ID_UP, set_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, set_down);
  window_single_click_subscribe(BUTTON_ID_BACK, set_back);
}

static void set_load(Window *window) {
  pick_metrics();
  Layer *root = window_get_root_layer(window);
  s_lay = layer_create(panel_frame(root));
  layer_set_update_proc(s_lay, settings_update);
  layer_add_child(root, s_lay);
}

static void set_unload(Window *window) {
  layer_destroy(s_lay);
  s_lay = NULL;
  window_destroy(s_win);
  s_win = NULL;
  home_window_refresh();
}

void settings_window_push(void) {
  s_sel = 0;
  s_top = 0;
  s_reset_armed = false;
  s_edit_slot = -1;
  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_click_config_provider(s_win, set_clicks);
  window_set_touch_bridge_disabled(s_win, true);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = set_load, .unload = set_unload,
  });
  window_stack_push(s_win, true);
}
