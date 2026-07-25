#include "settings.h"
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

enum { ROW_PRESET, ROW_RHYTHM, ROW_LEN, ROW_SOUND, ROW_HAPTICS,
       ROW_BACKLIGHT, ROW_SHOW_HR, ROW_RESET, NUM_ROWS };

#define ROW_H 23
#define ROW_Y0 28

static Window *s_win;
static Layer *s_lay;
static int8_t s_sel;
static bool s_reset_armed;
static int8_t s_edit_slot;   // -1 = not editing the custom rhythm

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
    case ROW_HAPTICS:   return S(S_OPT_HAPTICS);
    case ROW_BACKLIGHT: return S(S_OPT_BACKLIGHT);
    case ROW_SHOW_HR:   return S(S_OPT_SHOW_HR);
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
      snprintf(buf, len, "%s", g_set.sound_vol == 0 ? S(S_OFF)
                             : g_set.sound_vol == 1 ? S(S_SND_SOFT)
                             : g_set.sound_vol == 2 ? S(S_SND_NORMAL)
                                                    : S(S_SND_LOUD));
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
    default:
      return false;
  }
}

static void settings_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  graphics_context_set_text_color(ctx, COL_TEXT);
  graphics_draw_text(ctx, S(S_SETTINGS_TITLE), f18b, GRect(8, 2, b.size.w - 16, 24),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  const int16_t val_w = 84;
  for (int i = 0; i < NUM_ROWS; i++) {
    int16_t y = ROW_Y0 + i * ROW_H;
    GRect row = GRect(4, y, b.size.w - 8, ROW_H - 2);
    if (i == s_sel) {
      graphics_context_set_fill_color(ctx, COL_PANEL);
      graphics_fill_rect(ctx, row, 4, GCornersAll);
      graphics_context_set_stroke_color(ctx, s_edit_slot >= 0 ? COL_ACCENT : COL_TEXT);
      graphics_draw_round_rect(ctx, row, 4);
    }

    // Armed reset turns the whole row into a red confirmation prompt.
    if (i == ROW_RESET && s_reset_armed) {
      graphics_context_set_text_color(ctx, GColorRed);
      graphics_draw_text(ctx, S(S_RESET_CONFIRM), f18,
                         GRect(row.origin.x + 6, y + 1, row.size.w - 12, 20),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
      continue;
    }

    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, row_label(i), f18,
                       GRect(row.origin.x + 6, y + 1, row.size.w - 12 - val_w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

    char val[28];
    if (!row_value(i, val, sizeof(val))) continue;

    // While editing the custom rhythm, box the slot being changed.
    if (i == ROW_RHYTHM && s_edit_slot >= 0) {
      GRect vr = GRect(row.origin.x + row.size.w - val_w - 6, y + 1, val_w, 20);
      graphics_context_set_text_color(ctx, COL_ACCENT);
      graphics_draw_text(ctx, val, f18, vr, GTextOverflowModeTrailingEllipsis,
                         GTextAlignmentRight, NULL);
      // Underline marks which of the four numbers UP/DOWN is moving.
      int16_t seg = val_w / 4;
      GPoint p1 = GPoint(vr.origin.x + s_edit_slot * seg + 2, y + 20);
      graphics_context_set_stroke_color(ctx, COL_ACCENT);
      graphics_draw_line(ctx, p1, GPoint(p1.x + seg - 4, p1.y));
      continue;
    }

    bool active = true;
    if (i == ROW_SOUND) active = g_set.sound_vol > 0;
    else if (i == ROW_HAPTICS) active = g_set.haptics_on;
    else if (i == ROW_BACKLIGHT) active = g_set.bl_mode != BL_OFF;
    else if (i == ROW_SHOW_HR) active = g_set.show_hr;
    graphics_context_set_text_color(ctx, active ? COL_VALUE : COL_DIM);
    graphics_draw_text(ctx, val, f18,
                       GRect(row.origin.x + row.size.w - val_w - 6, y + 1, val_w, 20),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }

  graphics_context_set_text_color(ctx, COL_DIM);
  graphics_draw_text(ctx, s_edit_slot >= 0 ? S(S_RHYTHM_HINT) : S(S_SETTINGS_HINT), f14,
                     GRect(4, ROW_Y0 + NUM_ROWS * ROW_H - 1, b.size.w - 8, 18),
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
    case ROW_HAPTICS:
      settings_set_haptics(!g_set.haptics_on);
      if (g_set.haptics_on) vibes_short_pulse();
      break;
    case ROW_BACKLIGHT:
      settings_set_backlight(g_set.bl_mode + 1);
      backlight_apply();
      break;
    case ROW_SHOW_HR:
      settings_set_show_hr(!g_set.show_hr);
      hr_sampling_apply();
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

static void set_clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, set_sel_click);
  window_single_click_subscribe(BUTTON_ID_UP, set_up);
  window_single_click_subscribe(BUTTON_ID_DOWN, set_down);
  window_single_click_subscribe(BUTTON_ID_BACK, set_back);
}

static void set_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_lay = layer_create(layer_get_bounds(root));
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
  s_reset_armed = false;
  s_edit_slot = -1;
  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_click_config_provider(s_win, set_clicks);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = set_load, .unload = set_unload,
  });
  window_stack_push(s_win, true);
}
