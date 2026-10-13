#include "session.h"
#include "art.h"
#include "backlight.h"
#include "comms.h"
#include "i18n.h"
#include "result_window.h"
#include "settings.h"
#include "sfx.h"
#include "stats.h"
#include <string.h>

#define TICK_MS      40    // ~25 fps; smooth enough for a slow-moving circle
#define HR_EVERY_MS  1000  // the sensor updates about once a second at best
// The tide range is matched to the height of the scenery cairn below, so the
// surface sweeps across the stack instead of hovering far above it: at high tide
// the top stone is just awash, at low tide the whole stack is dry.
#define WATER_HI     BY_SIZE(112, 80)    // surface y at the top of an inhale (high tide)
#define WATER_LO     BY_SIZE(194, 142)   // surface y at the bottom of an exhale (low tide)
#define SCENE_STONES BY_SIZE(7, 5)       // decorative, not the streak — see below
#define SCENE_W      BY_SIZE(64, 48)     // the scene cairn's base width
// Inset of the pause and stop panels; a round screen's sides curve away there.
#define PANEL_X      PBL_IF_ROUND_ELSE(26, BY_SIZE(6, 3))
#define PROGRESS_H   3     // session-progress bar along the top edge
#define WAVE_SPEED   (TRIG_MAX_ANGLE / 90)    // wave drift per tick (~3.6 s/cycle)
#define SWELL_AMP    3     // px the whole level bobs, so holds are never dead still

// Priming. Without an explicit sample-period request the watch picks its own
// heart-rate schedule — minutes apart while the user sits still — so polling
// every second just re-reads the same stale value. We ask for 1 Hz for the
// duration of the session and give the sensor a moment to find a pulse before
// the breathing starts, so the opening heart rate is a real baseline.
#define HR_SAMPLE_HZ    1
#define PRIME_MAX_MS    12000   // give up waiting after this and breathe anyway
#define PRIME_HINT_MS   5000    // after this, suggest tightening the strap
#define PRIME_SETTLE_MS 1500    // once a pulse is found, hold it briefly as baseline
#define NUM_SEGMENTS 5     // fifths of the session, for the start-vs-end HR delta

typedef enum { PH_IN, PH_HOLD1, PH_OUT, PH_HOLD2, NUM_PHASES } Phase;

static Window *s_win;
static Layer *s_lay;
#if defined(PBL_ROUND)
static Layer *s_ring;   // the progress ring, full-screen so it reaches the bezel
#endif
static AppTimer *s_timer;

static BreathPattern s_pat;
static uint32_t s_phase_ms[NUM_PHASES];
static uint32_t s_total_ms;
static uint32_t s_elapsed_ms;
static uint32_t s_in_phase_ms;
static Phase    s_phase;
static bool     s_paused;
static bool     s_confirm_stop;
static int32_t  s_wave_phase;   // scrolls the surface wave
static bool     s_priming;      // waiting for the first heart-rate reading
static uint32_t s_prime_ms;
static uint32_t s_settle_ms;    // time since the first reading arrived
static uint32_t s_beat_ms;      // drives the heart animation

// Heart rate. Instead of keeping every sample we accumulate per-fifth sums
// (enough for the start-vs-end delta) and per-cycle swings (enough for RSA),
// which keeps the memory flat regardless of session length.
static bool     s_hr_available;
static uint32_t s_hr_accum_ms;
static uint32_t s_seg_sum[NUM_SEGMENTS];
static uint16_t s_seg_n[NUM_SEGMENTS];
static uint8_t  s_hr_first, s_hr_last;
static uint8_t  s_cyc_min, s_cyc_max;
static uint32_t s_swing_sum;
static uint16_t s_swing_n;

// HRV (SDK 4.33+). RMSSD — root mean square of successive PPI differences —
// accumulated as a running sum of squares so memory stays flat regardless of
// session length, same approach as the HR segment sums above.
#define HRV_MIN_SAMPLES 3   // fewer than this and the number is too noisy to show
static uint16_t s_last_ppi_ms;
static uint32_t s_rmssd_sq_sum;
static uint16_t s_rmssd_n;

static void tick(void *data);
static int32_t breath_fullness(void);
static void begin_breathing(void);

// ---------------------------------------------------------------------------
// Phase machine
// ---------------------------------------------------------------------------

static bool phase_used(Phase p) { return s_phase_ms[p] > 0; }

static Phase next_used_phase(Phase from) {
  for (int i = 1; i <= NUM_PHASES; i++) {
    Phase p = (Phase)((from + i) % NUM_PHASES);
    if (phase_used(p)) return p;
  }
  return PH_IN;   // unreachable: in/out are always non-zero
}

static void cue_for_phase(Phase p) {
  switch (p) {
    case PH_IN:  cue_phase_in(); break;
    case PH_OUT: cue_phase_out(); break;
    default:     cue_phase_hold(); break;
  }
}

// Closes the breath cycle: one peak-to-trough HR swing per cycle feeds the RSA
// term of the calm score.
static void close_cycle(void) {
  if (s_cyc_max >= s_cyc_min && s_cyc_min != 0xFF) {
    s_swing_sum += (uint32_t)(s_cyc_max - s_cyc_min);
    s_swing_n++;
  }
  s_cyc_min = 0xFF;
  s_cyc_max = 0;
}

// True when the watch can actually report a heart rate at all. False in the
// emulator, which is what drives every graceful-degradation path in the app.
static bool hr_accessible(void) {
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(
      HealthMetricHeartRateBPM, time(NULL) - 60, time(NULL));
  return (mask & HealthServiceAccessibilityMaskAvailable) != 0;
}

// hr_accessible() asks the health service, which is too slow to call on every
// frame; the drawing uses this copy, refreshed with each once-a-second sample.
static bool s_hr_ok;

static void sample_hr(void) {
  s_hr_ok = hr_accessible();
  if (!s_hr_ok) return;

  int bpm = (int)health_service_peek_current_value(HealthMetricHeartRateBPM);
  if (bpm < 30 || bpm > 220) return;   // sensor noise / no contact

  if (!s_hr_available) {
    s_hr_available = true;
    s_hr_first = (uint8_t)bpm;
  }
  s_hr_last = (uint8_t)bpm;

  int seg = (int)((uint64_t)s_elapsed_ms * NUM_SEGMENTS / (s_total_ms ? s_total_ms : 1));
  if (seg >= NUM_SEGMENTS) seg = NUM_SEGMENTS - 1;
  s_seg_sum[seg] += (uint32_t)bpm;
  s_seg_n[seg]++;

  if (bpm < s_cyc_min) s_cyc_min = (uint8_t)bpm;
  if (bpm > s_cyc_max) s_cyc_max = (uint8_t)bpm;
}

// One HRV reading. Fires on HealthEventHRVUpdate, independently of the once-
// per-second sample_hr() polling above.
static void sample_hrv(void) {
  uint16_t ppi = health_service_peek_hrv_ppi_ms();
  if (ppi < 300 || ppi > 2000) return;   // sensor noise / no contact

  if (s_last_ppi_ms) {
    int32_t diff = (int32_t)ppi - (int32_t)s_last_ppi_ms;
    if (diff > -200 && diff < 200) {   // reject a missed-beat artifact
      s_rmssd_sq_sum += (uint32_t)(diff * diff);
      s_rmssd_n++;
    }
  }
  s_last_ppi_ms = ppi;
}

// Newton's method integer square root. Avoids linking libm for a single call.
static uint16_t isqrt_u32(uint32_t v) {
  if (v == 0) return 0;
  uint32_t x = v, y = (x + 1) / 2;
  while (y < x) { x = y; y = (x + v / x) / 2; }
  return (uint16_t)x;
}

// Leaves the priming screen and starts the first inhale. The reading taken while
// priming stays as the session's opening heart rate, but the per-fifth sums are
// cleared so the start-vs-end delta only covers actual breathing.
static void begin_breathing(void) {
  s_priming = false;
  s_in_phase_ms = 0;
  s_phase = PH_IN;
  memset(s_seg_sum, 0, sizeof(s_seg_sum));
  memset(s_seg_n, 0, sizeof(s_seg_n));
  s_cyc_min = 0xFF;
  s_cyc_max = 0;
  s_swing_sum = 0;
  s_swing_n = 0;
  s_last_ppi_ms = 0;
  s_rmssd_sq_sum = 0;
  s_rmssd_n = 0;
  cue_for_phase(PH_IN);
}

static uint8_t s_quiet_frame;
#define QUIET_FRAME_EVERY 4

static void stop_timer(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
}

static int16_t seg_avg(int seg) {
  return s_seg_n[seg] ? (int16_t)(s_seg_sum[seg] / s_seg_n[seg]) : -1;
}

static void finish_session(void) {
  stop_timer();
  backlight_tint_reset();
  close_cycle();
  cue_session_done();

  SessionResult res;
  memset(&res, 0, sizeof(res));
  res.dur_s      = (uint16_t)(s_elapsed_ms / 1000);
  res.completion = (uint8_t)(s_total_ms ? (uint32_t)s_elapsed_ms * 100 / s_total_ms : 0);
  if (res.completion > 100) res.completion = 100;
  res.preset  = g_set.preset_idx;
  res.has_hr  = s_hr_available;

  if (s_hr_available) {
    res.hr_start = s_hr_first;
    res.hr_end   = s_hr_last;
    int16_t first = seg_avg(0), last = seg_avg(NUM_SEGMENTS - 1);
    // Fall back to the raw endpoints when a fifth never got a sample (short or
    // interrupted sessions).
    if (first < 0) first = s_hr_first;
    if (last < 0)  last = s_hr_last;
    res.hr_drop = first - last;
    res.rsa = s_swing_n ? (int16_t)(s_swing_sum / s_swing_n) : 0;
  }
  if (s_rmssd_n >= HRV_MIN_SAMPLES) {
    res.has_hrv  = true;
    res.rmssd_ms = isqrt_u32(s_rmssd_sq_sum / s_rmssd_n);
  }
  res.score = insights_score(&res);

  SessionRecord rec = {
    .date       = stats_today(),
    .dur_s      = res.dur_s,
    .hr_start   = res.hr_start,
    .hr_end     = res.hr_end,
    .score      = res.score,
    .preset     = res.preset,
    .completion = res.completion,
  };
  bool new_best = false;
  uint32_t fresh = stats_record(&rec, &new_best);
  // The very first session beats a best of zero, which is not worth celebrating
  // (and would make rule 1 of the tip table fire for everyone's first session).
  res.new_best = new_best && g_stats.total_sessions > 1;
  comms_send_session(&rec);

  // The result screen replaces the session on the stack, so BACK from it lands
  // on Home rather than re-entering a finished session.
  result_window_push(&res, fresh);
  window_stack_remove(s_win, false);
}

static void tick(void *data) {
  s_timer = NULL;
  if (s_paused) return;

  s_timer = app_timer_register(TICK_MS, tick, NULL);
  s_wave_phase += WAVE_SPEED;
  s_beat_ms += TICK_MS;

  if (s_priming) {
    s_prime_ms += TICK_MS;
    s_hr_accum_ms += TICK_MS;
    if (s_hr_accum_ms >= HR_EVERY_MS) {
      s_hr_accum_ms = 0;
      sample_hr();
    }
    if (s_hr_available) s_settle_ms += TICK_MS;
    // Start once we have a settled reading, or once waiting stops being worth it.
    if ((s_hr_available && s_settle_ms >= PRIME_SETTLE_MS) || s_prime_ms >= PRIME_MAX_MS) {
      begin_breathing();
    }
    layer_mark_dirty(s_lay);
    return;
  }

  s_elapsed_ms += TICK_MS;
  s_in_phase_ms += TICK_MS;

  s_hr_accum_ms += TICK_MS;
  if (s_hr_accum_ms >= HR_EVERY_MS) {
    s_hr_accum_ms = 0;
    sample_hr();
  }

  // Ended before the phase turns: a session that runs out on a phase boundary
  // (every minute of Box) would otherwise cue one more inhale, and that sound
  // still playing is what kept the result screen's bowl from being heard.
  if (s_elapsed_ms >= s_total_ms) {
    finish_session();
    return;
  }

  if (s_in_phase_ms >= s_phase_ms[s_phase]) {
    s_in_phase_ms = 0;
    Phase next = next_used_phase(s_phase);
    if (next <= s_phase) close_cycle();   // wrapped around: a full breath done
    s_phase = next;
    cue_for_phase(s_phase);
    if (g_set.bl_mode == BL_TIMED) backlight_touch();
  }

  // While a breath sound plays (only with Breath sound on) the screen drops to
  // every 4th frame and the light holds its colour, or the speaker runs dry and
  // the sound stutters (sfx.h). It used to skip every frame instead, which left
  // the tide standing still through each turn. Without the sound nothing is
  // skipped at all.
  if (sfx_phase_sounding()) {
    if (++s_quiet_frame % QUIET_FRAME_EVERY) return;
  } else {
    s_quiet_frame = 0;
    // The light breathes with the tide on screen.
    backlight_tint_breath(breath_fullness());
  }
  layer_mark_dirty(s_lay);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// The scene cairn never changes during a session, but its antialiased rounded
// stones cost ~10 ms a frame on a Time 2 -- a quarter of the frame budget, and
// a busy CPU is what makes the phase sounds stutter. So it is drawn once, the
// pixels are copied out of the frame buffer, and every later frame blits them.
// Colour only: a copy needs the 8-bit frame buffer layout. The copy waits a
// second: while the window is still sliding in, the frame buffer holds the
// screen it is replacing, and the first build of this copied the home screen's
// text along with the stones.
#define CAIRN_CAPTURE_FRAMES 25
static GBitmap *s_cairn_bmp;
static uint8_t s_cairn_frames;

static GRect scene_cairn_rect(GPoint base) {
  int16_t top = base.y - 2 - SCENE_STONES * 11 - 1;   // 11 = stone pitch (art.c)
  return GRect(base.x - SCENE_W / 2 - 2, top, SCENE_W + 4, base.y + 2 - top);
}

#if defined(PBL_COLOR)
static void capture_scene_cairn(GContext *ctx, GRect r) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  GBitmap *bmp = gbitmap_create_blank(r.size, GBitmapFormat8Bit);
  if (bmp) {
    GPoint off = layer_get_frame(s_lay).origin;
    uint8_t *dst = gbitmap_get_data(bmp);
    uint16_t stride = gbitmap_get_bytes_per_row(bmp);
    for (int16_t y = 0; y < r.size.h; y++) {
      GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, off.y + r.origin.y + y);
      for (int16_t x = 0; x < r.size.w; x++) {
        int16_t fx = off.x + r.origin.x + x;
        dst[y * stride + x] = (fx >= row.min_x && fx <= row.max_x) ? row.data[fx] : COL_BG.argb;
      }
    }
  }
  graphics_release_frame_buffer(ctx, fb);
  s_cairn_bmp = bmp;
}
#endif

static void draw_scene_cairn(GContext *ctx, GPoint base) {
  GRect r = scene_cairn_rect(base);
  if (s_cairn_bmp) {
    graphics_draw_bitmap_in_rect(ctx, s_cairn_bmp, r);
    return;
  }
  draw_cairn(ctx, base, SCENE_STONES, SCENE_W, g_stats.streak >= 7);
#if defined(PBL_COLOR)
  if (++s_cairn_frames >= CAIRN_CAPTURE_FRAMES) capture_scene_cairn(ctx, r);
#endif
}

// Sine ease-in-out over 0..1000, so the tide glides rather than jerks at the
// phase boundaries.
static int32_t ease(int32_t p) {
  if (p < 0) p = 0;
  if (p > 1000) p = 1000;
  int32_t a = (TRIG_MAX_ANGLE / 2) * p / 1000;
  return (TRIG_MAX_RATIO - cos_lookup(a)) * 1000 / (2 * TRIG_MAX_RATIO);
}

// How full the breath is, 0 (breathed out) .. 1000 (breathed in), eased.
static int32_t breath_fullness(void) {
  uint32_t len = s_phase_ms[s_phase] ? s_phase_ms[s_phase] : 1;
  int32_t p = (int32_t)(s_in_phase_ms * 1000 / len);
  switch (s_phase) {
    case PH_IN:    return ease(p);
    case PH_OUT:   return 1000 - ease(p);
    case PH_HOLD1: return 1000;
    default:       return 0;
  }
}

// Where the water surface sits right now. Breathing in raises it (smaller y),
// breathing out lets it recede and uncovers the cairn again. On top of the breath
// the whole level rides a slow swell, so a hold looks like calm water rather than
// a frozen frame.
static int16_t current_surface_y(void) {
  const int16_t range = WATER_LO - WATER_HI;
  int16_t y = WATER_LO - (int16_t)(range * breath_fullness() / 1000);
  return y + (int16_t)(sin_lookup(s_wave_phase / 3) * SWELL_AMP / TRIG_MAX_RATIO);
}

static GColor phase_color(void) {
  switch (s_phase) {
    case PH_IN:  return COL_PHASE_IN;
    case PH_OUT: return COL_PHASE_OUT;
    default:     return COL_VALUE;
  }
}

static const char *phase_label(void) {
  switch (s_phase) {
    case PH_IN:  return S(S_PHASE_IN);
    case PH_OUT: return S(S_PHASE_OUT);
    default:     return S(S_PHASE_HOLD);
  }
}

// The heart's half-width for this frame. With a reading it contracts sharply at
// the start of each beat and relaxes over the rest, at the measured rate.
static int16_t heart_size(int16_t base) {
  if (!s_hr_available || s_hr_last < 30) return base;
  uint32_t period = 60000u / s_hr_last;
  uint32_t p = s_beat_ms % period;
  if (p * 5 < period) return base + base / 3;
  if (p * 5 < period * 2) return base + base / 6;
  return base;
}

// Heart plus reading, centred as one block. Without a reading yet the heart is
// an outline followed by a question mark, so "still looking" never reads as
// "broken".
static void draw_hr_block(GContext *ctx, GRect b, int16_t y, GFont font, int16_t base) {
  char buf[16];
  if (s_hr_available) snprintf(buf, sizeof(buf), S(S_BPM_FMT), (int)s_hr_last);
  else snprintf(buf, sizeof(buf), "?");

  GSize sz = graphics_text_layout_get_content_size(buf, font,
                 GRect(0, 0, b.size.w, 40), GTextOverflowModeTrailingEllipsis,
                 GTextAlignmentLeft);
  int16_t gap = 8;
  int16_t total = base * 2 + gap + sz.w;
  int16_t x = (b.size.w - total) / 2;

  draw_heart(ctx, GPoint(x + base, y + 12), heart_size(base),
             s_hr_available ? COL_HR : COL_STONE_DARK);
  graphics_context_set_text_color(ctx, s_hr_available ? COL_HR : COL_DIM);
  graphics_draw_text(ctx, buf, font, GRect(x + base * 2 + gap, y, sz.w + 8, 34),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void draw_priming(GContext *ctx, GRect b) {
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18  = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

  draw_center(ctx, S(S_HR_SEARCH), BY_SIZE(f24b, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD)),
              BY_SIZE(18, 8), 30, b, COL_TEXT);
  draw_hr_block(ctx, b, BY_SIZE(74, 42), f24b, 11);

  // A countdown, so the wait never feels open-ended.
  char buf[8];
  uint32_t left = (PRIME_MAX_MS > s_prime_ms ? PRIME_MAX_MS - s_prime_ms + 999 : 0) / 1000;
  snprintf(buf, sizeof(buf), "%d", (int)left);
  draw_center(ctx, buf, f18, BY_SIZE(112, 78), 22, b, COL_DIM);

  // Only nag about the strap once waiting has actually dragged on.
  if (s_prime_ms >= PRIME_HINT_MS && !s_hr_available) {
    draw_wrapped(ctx, S(S_HR_TIGHTEN), BY_SIZE(f18, f14),
                 BY_SIZE(GRect(10, 138, b.size.w - 20, 60), GRect(6, 100, b.size.w - 12, 48)),
                 COL_DIM);
  }
  draw_center(ctx, S(S_HR_SKIP), f14, BY_SIZE(202, 148), 20, b, COL_DIM);
}

#if defined(PBL_ROUND)
// The session's progress on a round screen, which has no straight top edge for
// the bar: a ring around the bezel, clockwise from twelve o'clock. It lives in
// its own full-screen layer because the scene's layer is inset from the top
// and would clip it there. Thicker than the bar, since the outermost pixels of
// the panel sit under the bezel.
#define RING_W (PROGRESS_H + 4)

static void ring_update(Layer *layer, GContext *ctx) {
  if (s_priming) return;
  GRect b = layer_get_bounds(layer);
  graphics_context_set_fill_color(ctx, COL_PANEL);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, RING_W, 0, TRIG_MAX_ANGLE);
  int32_t done_angle = s_total_ms
      ? (int32_t)((uint64_t)s_elapsed_ms * TRIG_MAX_ANGLE / s_total_ms) : 0;
  if (done_angle > TRIG_MAX_ANGLE) done_angle = TRIG_MAX_ANGLE;
  graphics_context_set_fill_color(ctx, COL_ACCENT);
  graphics_fill_radial(ctx, b, GOvalScaleModeFitCircle, RING_W, 0, done_angle);
}
#endif

static void session_update(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f14  = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

  graphics_context_set_fill_color(ctx, COL_BG);
  graphics_fill_rect(ctx, b, 0, GCornerNone);

  if (s_priming) {
    draw_priming(ctx, b);
    return;
  }

  // Session progress: a thin bar along the very top, the only chrome on screen.
#if !defined(PBL_ROUND)   // round: see ring_update
  graphics_context_set_fill_color(ctx, COL_PANEL);
  graphics_fill_rect(ctx, GRect(0, 0, b.size.w, PROGRESS_H), 0, GCornerNone);
  int16_t done_w = s_total_ms
      ? (int16_t)((uint32_t)s_elapsed_ms * b.size.w / s_total_ms) : 0;
  if (done_w > b.size.w) done_w = b.size.w;
  graphics_context_set_fill_color(ctx, COL_ACCENT);
  graphics_fill_rect(ctx, GRect(0, 0, done_w, PROGRESS_H), 0, GCornerNone);
#endif

  // Everything the user reads lives above the highest the water ever gets.
  draw_center(ctx, phase_label(), BY_SIZE(f24b, f18b), BY_SIZE(2, 0), BY_SIZE(30, 22), b,
              phase_color());

  uint32_t left = s_phase_ms[s_phase] > s_in_phase_ms
                    ? (s_phase_ms[s_phase] - s_in_phase_ms + 999) / 1000 : 0;
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", (int)left);
  draw_center(ctx, buf, BY_SIZE(f28b, f24b), BY_SIZE(28, 16), BY_SIZE(34, 30), b, COL_TEXT);

  uint32_t rem_s = (s_total_ms > s_elapsed_ms ? s_total_ms - s_elapsed_ms : 0) / 1000;
  snprintf(buf, sizeof(buf), "%d:%02d", (int)(rem_s / 60), (int)(rem_s % 60));
  draw_center(ctx, buf, BY_SIZE(f18b, f14), BY_SIZE(62, 44), BY_SIZE(22, 18), b, COL_DIM);

  // The shore, the cairn standing on it, and then the tide painted over both —
  // that ordering is what submerges the stones as the level rises.
  // A fixed-height cairn, not the streak one: this is scenery whose only job is
  // to give the water something to uncover, and a one-stone stack (a first-day
  // streak) would stay submerged for all but the last moment of every exhale.
  // The real, growing cairn lives on Home and the result screen.
  // The water is not opaque: after the tide, the drowned stones come back in a
  // washed-out tint, so the cairn stays readable through the water instead of
  // being swallowed on every inhale.
  int16_t surface = current_surface_y();
  draw_shore(ctx, b, WATER_LO);
  draw_scene_cairn(ctx, GPoint(b.size.w / 2, WATER_LO));
  draw_tide(ctx, b, surface, s_wave_phase, b.size.h);
  draw_cairn_underwater(ctx, GPoint(b.size.w / 2, WATER_LO), SCENE_STONES, SCENE_W,
                        g_stats.streak >= 7, tide_flat_y(surface));

  // Live heart rate, drawn after the tide. Shown even before the first reading
  // lands, as an outline heart and a question mark, so the user can see it is
  // still measuring. Drawn last because at the top of an inhale the crest
  // reaches the digits, and the readout has to win that overlap.
  if (g_set.show_hr && s_hr_ok) {
    draw_hr_block(ctx, b, BY_SIZE(82, 58), BY_SIZE(f24b, f18b), BY_SIZE(10, 7));
  }

  if (s_paused && !s_confirm_stop) {
    const int16_t top = BY_SIZE(118, 84);
    GRect panel = GRect(PANEL_X, top, b.size.w - 2 * PANEL_X, 52);
    graphics_context_set_fill_color(ctx, COL_PANEL);
    graphics_fill_rect(ctx, panel, 6, GCornersAll);
    graphics_context_set_stroke_color(ctx, COL_TEXT);
    graphics_draw_round_rect(ctx, panel, 6);
    draw_center(ctx, S(S_PAUSED), f24b, top + 4, 30, b, COL_ACCENT);
    draw_center(ctx, S(S_HINT_RESUME), f14, top + 30, 18, b, COL_TEXT);
  }

  // Stop confirmation, drawn as a panel over the circle.
  if (s_confirm_stop) {
    const int16_t top = BY_SIZE(112, 80);
    GRect panel = GRect(PANEL_X, top, b.size.w - 2 * PANEL_X, 66);
    graphics_context_set_fill_color(ctx, COL_PANEL);
    graphics_fill_rect(ctx, panel, 6, GCornersAll);
    graphics_context_set_stroke_color(ctx, COL_TEXT);
    graphics_draw_round_rect(ctx, panel, 6);
    draw_center(ctx, S(S_STOP_TITLE), f18b, top + 6, 24, b, COL_TEXT);
    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, S(S_STOP_HINT), f14,
                       GRect(PANEL_X + 6, top + 28, b.size.w - 2 * PANEL_X - 12, 36),
                       GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  }
}

// ---------------------------------------------------------------------------
// Input & lifecycle
// ---------------------------------------------------------------------------

static void set_paused(bool paused) {
  s_paused = paused;
  if (paused) {
    stop_timer();
    backlight_tint_reset();
  } else if (!s_timer) {
    s_timer = app_timer_register(TICK_MS, tick, NULL);
  }
  layer_mark_dirty(s_lay);
}

static void sess_select(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  if (s_confirm_stop) {
    finish_session();
    return;
  }
  if (s_priming) {          // don't make anyone wait who doesn't want to
    begin_breathing();
    layer_mark_dirty(s_lay);
    return;
  }
  set_paused(!s_paused);
}

static void sess_back(ClickRecognizerRef rec, void *ctx) {
  backlight_touch();
  if (s_priming) {          // nothing has been breathed yet: just leave
    window_stack_pop(true);
    return;
  }
  if (s_confirm_stop) {           // second BACK cancels the prompt
    s_confirm_stop = false;
    set_paused(false);
    return;
  }
  s_confirm_stop = true;
  set_paused(true);
}

static void sess_clicks(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, sess_select);
  window_single_click_subscribe(BUTTON_ID_BACK, sess_back);
}

// Losing focus (notification, quick view) pauses instead of breathing on
// unseen — same reflex as the games' auto-pause.
static void focus_handler(bool in_focus) {
  if (!in_focus && !s_paused) set_paused(true);
}

static void sess_load(Window *window) {
  sfx_phase_prepare();
  Layer *root = window_get_root_layer(window);
  s_lay = layer_create(screen_frame(root, true));
  layer_set_update_proc(s_lay, session_update);
  layer_add_child(root, s_lay);
#if defined(PBL_ROUND)
  s_ring = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_ring, ring_update);
  layer_add_child(root, s_ring);
#endif
}

static void sess_unload(Window *window) {
  stop_timer();
  if (s_cairn_bmp) {
    gbitmap_destroy(s_cairn_bmp);
    s_cairn_bmp = NULL;
  }
  s_cairn_frames = 0;
  backlight_tint_reset();
  app_focus_service_unsubscribe();
  layer_destroy(s_lay);
#if defined(PBL_ROUND)
  layer_destroy(s_ring);
#endif
  s_lay = NULL;
  window_destroy(s_win);
  s_win = NULL;
}

// The health service only pushes events; the values are read by polling, so the
// handler just nudges a sample.
static void health_handler(HealthEventType event, void *context) {
  if (event == HealthEventHeartRateUpdate || event == HealthEventSignificantUpdate) {
    sample_hr();
  } else if (event == HealthEventHRVUpdate) {
    sample_hrv();
  }
}

#define HR_SAMPLE_INTERVAL_S 1

void hr_sampling_apply(void) {
  health_service_set_heart_rate_sample_period(g_set.show_hr ? HR_SAMPLE_INTERVAL_S : 0);
  health_service_set_hrv_sample_period(g_set.show_hr ? HR_SAMPLE_INTERVAL_S : 0);
}

void session_window_push(void) {
  s_pat = settings_pattern();
  s_phase_ms[PH_IN]    = s_pat.in * 100u;
  s_phase_ms[PH_HOLD1] = s_pat.hold1 * 100u;
  s_phase_ms[PH_OUT]   = s_pat.out * 100u;
  s_phase_ms[PH_HOLD2] = s_pat.hold2 * 100u;
  s_total_ms   = (uint32_t)settings_len_min() * 60000u;
  s_elapsed_ms = 0;
  s_in_phase_ms = 0;
  s_phase = PH_IN;
  s_paused = false;
  s_confirm_stop = false;
  s_wave_phase = 0;
  s_beat_ms = 0;
  s_prime_ms = 0;
  s_settle_ms = 0;

  s_hr_available = false;
  s_hr_accum_ms = 0;
  s_hr_first = s_hr_last = 0;
  s_cyc_min = 0xFF;
  s_cyc_max = 0;
  s_swing_sum = 0;
  s_swing_n = 0;
  s_last_ppi_ms = 0;
  s_rmssd_sq_sum = 0;
  s_rmssd_n = 0;
  memset(s_seg_sum, 0, sizeof(s_seg_sum));
  memset(s_seg_n, 0, sizeof(s_seg_n));

  s_win = window_create();
  window_set_background_color(s_win, COL_BG);
  window_set_click_config_provider(s_win, sess_clicks);
  window_set_window_handlers(s_win, (WindowHandlers){
    .load = sess_load, .unload = sess_unload,
  });
  window_stack_push(s_win, true);

  health_service_events_subscribe(health_handler, NULL);
  app_focus_service_subscribe(focus_handler);
  backlight_apply();
  sample_hr();

  // Wait for a pulse before the first inhale, so the opening heart rate is a
  // real baseline rather than whatever the sensor last happened to record. The
  // app already asked for 1 Hz sampling on launch, so by the time the user gets
  // here from Home there is usually a reading waiting and this passes at once.
  // Skipped entirely where a reading can never arrive (emulator) or where the
  // user has turned the heart rate off.
  s_priming = g_set.show_hr && hr_accessible() && !s_hr_available;
  if (!s_priming) cue_for_phase(PH_IN);
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}
