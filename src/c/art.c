#include "art.h"

#define LAYOUT_H 228

GRect screen_frame(Layer *root, bool to_bottom) {
  GRect b = layer_get_bounds(root);
#if defined(PBL_ROUND)
  int16_t top = (b.size.h - LAYOUT_H) / 2;
  b.origin.y = top;
  b.size.h = to_bottom ? b.size.h - top : LAYOUT_H;
#else
  (void)to_bottom;
#endif
  return b;
}

GRect panel_frame(Layer *root) {
  GRect b = layer_get_bounds(root);
#if defined(PBL_ROUND)
  b = GRect((b.size.w - 200) / 2, (b.size.h - LAYOUT_H) / 2, 200, LAYOUT_H);
#endif
  return b;
}

void draw_center(GContext *ctx, const char *text, GFont font,
                 int16_t y, int16_t h, GRect b, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, GRect(2, y, b.size.w - 4, h),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

int16_t draw_wrapped(GContext *ctx, const char *text, GFont font, GRect r, GColor color) {
  graphics_context_set_text_color(ctx, color);
  GSize used = graphics_text_layout_get_content_size(text, font, r,
                                                     GTextOverflowModeWordWrap,
                                                     GTextAlignmentLeft);
  graphics_draw_text(ctx, text, font, r, GTextOverflowModeWordWrap,
                     GTextAlignmentLeft, NULL);
  return used.h;
}

// Stone geometry: the bottom stone is the widest and each one above loses a
// slice of width, so eight stones still fit inside max_w without overlapping.
// Pitch of 11 px: nine stones (the maximum) stay inside 100 px of height, so a
// full cairn never reaches the text above it on either screen that draws one.
#define STONE_H  10
#define STONE_GAP 1
#define STONE_MAX 9

static GRect stone_rect(GPoint base, int i, int max_w) {
  // Width tapers from max_w at the base to roughly a third at the top.
  int16_t w = max_w - (int16_t)((int32_t)i * max_w * 2 / (3 * STONE_MAX));
  if (w < 12) w = 12;
  int16_t y = base.y - 2 - (int16_t)(i + 1) * (STONE_H + STONE_GAP);
  return GRect(base.x - w / 2, y, w, STONE_H);
}

void draw_cairn(GContext *ctx, GPoint base, int stones, int max_w, bool glow) {
  // Ground line, always drawn — an empty cairn is a bare patch of shore.
  graphics_context_set_stroke_color(ctx, COL_SAND);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(base.x - max_w / 2, base.y),
                          GPoint(base.x + max_w / 2, base.y));
  graphics_context_set_stroke_width(ctx, 1);

  if (stones <= 0) return;
  if (stones > STONE_MAX) stones = STONE_MAX;

  for (int i = 0; i < stones; i++) {
    GRect r = stone_rect(base, i, max_w);
    bool top = (i == stones - 1);
    graphics_context_set_fill_color(ctx, (top && glow) ? COL_ACCENT
                                     : (i % 2) ? COL_STONE_DARK : COL_STONE);
    graphics_fill_rect(ctx, r, 5, GCornersAll);
    graphics_context_set_stroke_color(ctx, COL_PANEL);
    graphics_draw_round_rect(ctx, r, 5);
  }
}

// The stones as seen *through* the water, painted back on top of the tide. The
// screen has no alpha, so transparency is faked the way water actually looks:
// everything below the surface keeps its shape but loses its own colour and
// takes on one washed-out tint. The 1 px gaps between the stones stay water, so
// the stack still reads as separate stones without any outline.
//
// Only the band below `water_y` is redrawn, and callers pass the flat part of
// the surface, never a wave crest — that keeps the foam line untouched, so the
// water still visibly closes over the stack.
void draw_cairn_underwater(GContext *ctx, GPoint base, int stones, int max_w,
                           bool glow, int16_t water_y) {
  if (stones <= 0) return;
  if (stones > STONE_MAX) stones = STONE_MAX;

  // Not antialiased: redrawn every frame as the water moves, and the soft
  // tint hides the harder corners.
  graphics_context_set_antialiased(ctx, false);
  for (int i = 0; i < stones; i++) {
    GRect r = stone_rect(base, i, max_w);
    int16_t bottom = r.origin.y + r.size.h;
    if (bottom <= water_y) continue;              // still in the air

    bool top = (i == stones - 1);
    graphics_context_set_fill_color(ctx, (top && glow) ? COL_STONE_WET_GLOW
                                                       : COL_STONE_WET);
    if (r.origin.y >= water_y) {
      graphics_fill_rect(ctx, r, 5, GCornersAll);
    } else {
      // Half in, half out: only the drowned slice, rounded at the bottom.
      graphics_fill_rect(ctx, GRect(r.origin.x, water_y, r.size.w, bottom - water_y),
                         5, GCornersBottom);
    }
  }
  graphics_context_set_antialiased(ctx, true);
}

void draw_trend_arrow(GContext *ctx, GPoint c, int dir, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  if (dir == 0) {
    graphics_draw_line(ctx, GPoint(c.x - 5, c.y), GPoint(c.x + 5, c.y));
    graphics_draw_line(ctx, GPoint(c.x + 1, c.y - 4), GPoint(c.x + 5, c.y));
    graphics_draw_line(ctx, GPoint(c.x + 1, c.y + 4), GPoint(c.x + 5, c.y));
  } else {
    int16_t tip = (dir > 0) ? c.y - 6 : c.y + 6;
    int16_t tail = (dir > 0) ? c.y + 6 : c.y - 6;
    int16_t barb = (dir > 0) ? tip + 5 : tip - 5;
    graphics_draw_line(ctx, GPoint(c.x, tail), GPoint(c.x, tip));
    graphics_draw_line(ctx, GPoint(c.x - 4, barb), GPoint(c.x, tip));
    graphics_draw_line(ctx, GPoint(c.x + 4, barb), GPoint(c.x, tip));
  }
  graphics_context_set_stroke_width(ctx, 1);
}

void draw_delta_arrow(GContext *ctx, GPoint c, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_line(ctx, GPoint(c.x - 6, c.y), GPoint(c.x + 6, c.y));
  graphics_draw_line(ctx, GPoint(c.x + 2, c.y - 4), GPoint(c.x + 6, c.y));
  graphics_draw_line(ctx, GPoint(c.x + 2, c.y + 4), GPoint(c.x + 6, c.y));
  graphics_context_set_stroke_width(ctx, 1);
}

// Each badge gets a distinct emblem so it reads at 20 px: a single stone, a
// three-stone cairn, a seven-ray sun, concentric rings, a "100", and a wave.
static void draw_emblem(GContext *ctx, GPoint c, int r, int badge, GColor fg) {
  graphics_context_set_fill_color(ctx, fg);
  graphics_context_set_stroke_color(ctx, fg);
  switch (badge) {
    case BADGE_FIRST:
      // Tall enough to read as a pebble rather than a dash.
      graphics_fill_rect(ctx, GRect(c.x - r / 2, c.y - 3 * r / 10, r, 3 * r / 5),
                         r / 3, GCornersAll);
      break;
    case BADGE_STEADY:
      for (int i = 0; i < 3; i++) {
        int16_t w = r - i * (r / 4);
        graphics_fill_rect(ctx, GRect(c.x - w / 2, c.y + r / 2 - (i + 1) * (r / 3), w, r / 4),
                           2, GCornersAll);
      }
      break;
    case BADGE_ZENWEEK:
      graphics_fill_circle(ctx, c, r / 3);
      graphics_context_set_stroke_width(ctx, 2);
      for (int i = 0; i < 7; i++) {
        int32_t a = TRIG_MAX_ANGLE * i / 7;
        int16_t x1 = c.x + (int16_t)(sin_lookup(a) * (r / 2) / TRIG_MAX_RATIO);
        int16_t y1 = c.y - (int16_t)(cos_lookup(a) * (r / 2) / TRIG_MAX_RATIO);
        int16_t x2 = c.x + (int16_t)(sin_lookup(a) * r / TRIG_MAX_RATIO);
        int16_t y2 = c.y - (int16_t)(cos_lookup(a) * r / TRIG_MAX_RATIO);
        graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
      }
      graphics_context_set_stroke_width(ctx, 1);
      break;
    case BADGE_DEEP:
      graphics_draw_circle(ctx, c, r);
      graphics_draw_circle(ctx, c, 2 * r / 3);
      graphics_fill_circle(ctx, c, r / 4);
      break;
    case BADGE_CENTURY: {
      // The badge shelf draws these at r = 6, where the bold 18 no longer fits.
      GFont f = fonts_get_system_font(r >= 14 ? FONT_KEY_GOTHIC_18_BOLD
                                              : FONT_KEY_GOTHIC_14);
      graphics_context_set_text_color(ctx, fg);
      graphics_draw_text(ctx, "100", f, GRect(c.x - r - 6, c.y - (r >= 14 ? 12 : 9),
                                             2 * r + 12, 20),
                         GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      break;
    }
    default:   // BADGE_TIDE — two stacked waves
      graphics_context_set_stroke_width(ctx, 2);
      for (int row = 0; row < 2; row++) {
        int16_t y = c.y - r / 3 + row * (2 * r / 3);
        for (int i = -1; i <= 0; i++) {
          graphics_draw_line(ctx, GPoint(c.x + i * r, y + 3), GPoint(c.x + i * r + r / 2, y - 3));
          graphics_draw_line(ctx, GPoint(c.x + i * r + r / 2, y - 3), GPoint(c.x + i * r + r, y + 3));
        }
      }
      graphics_context_set_stroke_width(ctx, 1);
      break;
  }
}

// A locked badge is the same coin in grey: the emblem is cut out in the
// background colour either way, so both states read at 22 px on a black screen.
void draw_badge(GContext *ctx, GPoint center, int radius, int badge, bool unlocked) {
  graphics_context_set_fill_color(ctx, unlocked ? COL_ACCENT : COL_PANEL);
  graphics_fill_circle(ctx, center, radius);
  graphics_context_set_stroke_color(ctx, unlocked ? COL_TEXT : COL_DIM);
  graphics_draw_circle(ctx, center, radius);
  draw_emblem(ctx, center, radius - 5, badge, COL_BG);
}

// ---------------------------------------------------------------------------
// The tide
// ---------------------------------------------------------------------------
// Two sine components of different length drifting in opposite directions. A
// single sine reads as a mechanical ripple; the sum never repeats the same crest
// twice in a row and reads as water.
#define WAVE_AMP_A      5    // px, the long swell
#define WAVE_AMP_B      3    // px, the short chop running the other way
#define WAVE_MAX        (WAVE_AMP_A + WAVE_AMP_B)
#define WAVE_LEN_A     94    // px per full wave
#define WAVE_LEN_B     43
#define WAVE_STEP       2    // px per drawn segment; 4 made the crests visibly stepped
#define BAND_TOP_H     22    // the sunlit layer just under the surface

static int16_t wave_y(int16_t surface_y, int32_t phase, int16_t x) {
  int32_t a = phase + (int32_t)x * TRIG_MAX_ANGLE / WAVE_LEN_A;
  int32_t c = -(phase * 3 / 2) + (int32_t)x * TRIG_MAX_ANGLE / WAVE_LEN_B;
  return surface_y
       + (int16_t)(sin_lookup(a) * WAVE_AMP_A / TRIG_MAX_RATIO)
       + (int16_t)(sin_lookup(c) * WAVE_AMP_B / TRIG_MAX_RATIO);
}

int16_t tide_flat_y(int16_t surface_y) {
  return surface_y + WAVE_MAX;
}

void draw_tide(GContext *ctx, GRect b, int16_t surface_y, int32_t wave_phase,
               int16_t bottom) {
  if (bottom > b.size.h) bottom = b.size.h;
  // Everything from here down is solid water; only the strip above it is wavy.
  int16_t flat = surface_y + WAVE_MAX;
  if (flat >= bottom) return;   // tide is fully out

  // Two depth colours only: a black third band read as a hole in the screen
  // rather than as deep water.
  int16_t top_h = BAND_TOP_H;
  if (flat + top_h > bottom) top_h = bottom - flat;
  graphics_context_set_fill_color(ctx, COL_WATER_TOP);
  graphics_fill_rect(ctx, GRect(0, flat, b.size.w, top_h), 0, GCornerNone);
  if (flat + BAND_TOP_H < bottom) {
    graphics_context_set_fill_color(ctx, COL_WATER_DEEP);
    graphics_fill_rect(ctx, GRect(0, flat + BAND_TOP_H, b.size.w,
                                 bottom - flat - BAND_TOP_H), 0, GCornerNone);
  }

  // The wavy strip: one column per step, filled up to the crest.
  graphics_context_set_fill_color(ctx, COL_WATER_TOP);
  for (int16_t x = 0; x < b.size.w; x += WAVE_STEP) {
    int16_t wy = wave_y(surface_y, wave_phase, x);
    graphics_fill_rect(ctx, GRect(x, wy, WAVE_STEP, flat - wy), 0, GCornerNone);
  }

  // Foam crest: per column a 2 px band that also spans the step to the previous
  // column, so it reads as one unbroken line. Filled rectangles, not lines: a
  // hundred 2 px-wide line segments every frame was the most expensive thing on
  // the session screen (~13 ms of a 40 ms frame on a Time 2, antialiased or
  // not), and a busy CPU makes the phase sounds stutter.
  graphics_context_set_fill_color(ctx, COL_FOAM);
  int16_t prev = wave_y(surface_y, wave_phase, 0);
  for (int16_t x = WAVE_STEP; x <= b.size.w; x += WAVE_STEP) {
    int16_t wy = wave_y(surface_y, wave_phase, x);
    int16_t lo = wy < prev ? wy : prev;
    int16_t hi = wy < prev ? prev : wy;
    graphics_fill_rect(ctx, GRect(x - WAVE_STEP, lo - 1, WAVE_STEP, hi - lo + 2), 0, GCornerNone);
    prev = wy;
  }
}

void draw_shore(GContext *ctx, GRect b, int16_t y) {
  graphics_context_set_fill_color(ctx, COL_SAND);
  graphics_fill_rect(ctx, GRect(0, y, b.size.w, 4), 0, GCornerNone);
}

// ---------------------------------------------------------------------------
// Heart
// ---------------------------------------------------------------------------
// Two lobes plus a scanline-filled taper. Built from primitives rather than a
// GPath because the beat animation changes its size every frame, and GPath
// points are fixed at creation.
void draw_heart(GContext *ctx, GPoint c, int16_t size, GColor color) {
  if (size < 3) size = 3;
  const int16_t lobe_r = size / 2;
  const int16_t lobe_y = c.y - size / 4;
  const int16_t tip_y = c.y + size;

  graphics_context_set_fill_color(ctx, color);
  graphics_fill_circle(ctx, GPoint(c.x - lobe_r, lobe_y), lobe_r);
  graphics_fill_circle(ctx, GPoint(c.x + lobe_r, lobe_y), lobe_r);
  graphics_context_set_stroke_color(ctx, color);
  for (int16_t y = lobe_y; y <= tip_y; y++) {
    int16_t half = size - size * (y - lobe_y) / (tip_y - lobe_y);
    graphics_draw_line(ctx, GPoint(c.x - half, y), GPoint(c.x + half, y));
  }
}
