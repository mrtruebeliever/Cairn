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
  // Stacked by hand, not centred by a machine: each stone sits a pixel off.
  static const int8_t JITTER[STONE_MAX] = {0, 1, -1, 0, 1, -1, 1, 0, -1};
  int16_t y = base.y - 2 - (int16_t)(i + 1) * (STONE_H + STONE_GAP);
  return GRect(base.x - w / 2 + JITTER[i], y, w, STONE_H);
}

// The stones are pixel art in the style of the sprites in PebbleDiver and
// PBL-7: a black outline, a highlight on the upper left, a shadow along the
// bottom and a speck of grit, built row by row from an inset table instead of
// a rounded rect. They are drawn as 1 px rows rather than bitmaps because every
// stone has its own width. The flat bottom is what makes them sit.
static const int8_t STONE_IN[STONE_H] = {5, 3, 1, 1, 0, 0, 0, 0, 1, 3};

typedef struct {
  GColor hi, base, shade, outline;
} StoneInk;

// Per stone, one side bulges a pixel further than the other, so the stack
// does not read as nine copies of one shape.
#define IMAX(a, b) ((a) > (b) ? (a) : (b))

static int8_t stone_inset(int i, int row, bool right) {
  int8_t in = (row < 0 || row >= STONE_H) ? STONE_H : STONE_IN[row];
  bool lean = ((i * 5 + 2) % 3 == 0) == right;
  if (lean && row <= 2) in += 3 - row;
  else if (lean && row >= 7) in += row - 6;
  return in;
}

static void stone_row(GContext *ctx, int16_t x0, int16_t x1, int16_t y, GColor c) {
  if (x1 < x0) return;
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(x0, y, x1 - x0 + 1, 1), 0, GCornerNone);
}

// Rows above clip_y are left alone (the underwater pass redraws only the part
// below the surface).
static void draw_pixel_stone(GContext *ctx, GRect r, int i, const StoneInk *ink,
                             int16_t clip_y) {
  int16_t right = r.origin.x + r.size.w - 1;
  for (int row = 0; row < STONE_H; row++) {
    int16_t y = r.origin.y + row;
    if (y < clip_y) continue;
    int16_t l = r.origin.x + stone_inset(i, row, false);
    int16_t rr = right - stone_inset(i, row, true);
    // Silhouette in the outline colour, then the inside: the shape shrunk by
    // one pixel, taking the neighbouring rows into account so the outline
    // stays closed on the staircase corners.
    stone_row(ctx, l, rr, y, ink->outline);
    if (row == 0 || row == STONE_H - 1) continue;
    int16_t il = r.origin.x + 1 + IMAX(stone_inset(i, row - 1, false),
                                  IMAX(stone_inset(i, row, false), stone_inset(i, row + 1, false)));
    int16_t ir = right - 1 - IMAX(stone_inset(i, row - 1, true),
                             IMAX(stone_inset(i, row, true), stone_inset(i, row + 1, true)));
    if (ir < il) continue;
    int16_t span = ir - il + 1;
    if (row >= STONE_H - 4) {
      // Shadow, growing from the lower right: a third, two thirds, all of it.
      int16_t from = il + span * (STONE_H - 2 - row) / 3;
      stone_row(ctx, il, from - 1, y, ink->base);
      stone_row(ctx, from, ir, y, ink->shade);
    } else if (row <= 2) {
      // Highlight: a band across the upper left, shorter on the second row.
      int16_t hi_w = (row == 1) ? span * 2 / 3 : span / 3;
      stone_row(ctx, il, il + hi_w - 1, y, ink->hi);
      stone_row(ctx, il + hi_w, ir, y, ink->base);
    } else {
      stone_row(ctx, il, ir, y, ink->base);
      // A speck of grit, somewhere different on every stone.
      if (row == 4 + (i & 1) && span > 8) {
        int16_t gx = il + 3 + (i * 7) % (span - 6);
        stone_row(ctx, gx, gx, y, ink->shade);
      }
    }
  }
}

static const StoneInk INK_LIGHT = {
  CBW(GColorWhite, GColorWhite), COL_STONE, CBW(GColorDarkGray, GColorWhite), GColorBlack,
};
static const StoneInk INK_DARK = {
  CBW(GColorLightGray, GColorDarkGray), COL_STONE_DARK, CBW(GColorOxfordBlue, GColorBlack),
  GColorBlack,
};
static const StoneInk INK_GLOW = {
  GColorWhite, COL_ACCENT, CBW(GColorBrass, GColorWhite), GColorBlack,
};
// Under water the stones keep their shape but lose their own colours: one
// washed-out tint, outlined in the deep water's colour.
static const StoneInk INK_WET = {
  COL_STONE_WET, COL_STONE_WET, CBW(GColorTiffanyBlue, GColorWhite),
  CBW(GColorMidnightGreen, GColorBlack),
};
static const StoneInk INK_WET_GLOW = {
  COL_STONE_WET_GLOW, COL_STONE_WET_GLOW, CBW(GColorArmyGreen, GColorWhite),
  CBW(GColorMidnightGreen, GColorBlack),
};

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
    bool top = (i == stones - 1);
    const StoneInk *ink = (top && glow) ? &INK_GLOW : (i % 2) ? &INK_DARK : &INK_LIGHT;
    draw_pixel_stone(ctx, stone_rect(base, i, max_w), i, ink, INT16_MIN);
  }
}

// The stones as seen *through* the water, painted back on top of the tide. The
// screen has no alpha, so transparency is faked the way water actually looks:
// everything below the surface keeps its shape but loses its own colour and
// takes on one washed-out tint.
//
// Only the band below `water_y` is redrawn, and callers pass the flat part of
// the surface, never a wave crest — that keeps the foam line untouched, so the
// water still visibly closes over the stack.
void draw_cairn_underwater(GContext *ctx, GPoint base, int stones, int max_w,
                           bool glow, int16_t water_y) {
  if (stones <= 0) return;
  if (stones > STONE_MAX) stones = STONE_MAX;

  for (int i = 0; i < stones; i++) {
    GRect r = stone_rect(base, i, max_w);
    if (r.origin.y + r.size.h <= water_y) continue;   // still in the air
    bool top = (i == stones - 1);
    draw_pixel_stone(ctx, r, i, (top && glow) ? &INK_WET_GLOW : &INK_WET, water_y);
  }
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

// The water's detail, in the pixel-art style of the stones: glints on the
// sunlit layer, a stepped seam into the deep instead of a hard edge, and faint
// streaks of light further down -- the way the 16-bit consoles drew water.
// Positions are given for a 200 px wide screen and scaled to the one at hand;
// each layer drifts at its own speed with the wave phase, the deeper ones
// slower, so the water has depth when it moves. All 1 px filled rows: lines
// cost far more on a Time 2 (see the foam crest below).
typedef struct { int8_t dy; uint8_t x, w; } Streak;

// Sunlit layer, below the flat level. Each glint is two rows, the second one
// narrower: a small reflection rather than a dash.
static const Streak GLINTS[] = {
  {4, 10, 6}, {4, 72, 4}, {4, 132, 5}, {4, 186, 3},
  {9, 42, 4}, {9, 106, 6}, {9, 162, 4},
  {14, 22, 3}, {14, 90, 5}, {14, 150, 3},
};
// Deep water, below the seam.
static const Streak DEEP[] = {
  {7, 30, 10}, {7, 140, 6}, {14, 90, 12}, {21, 15, 8}, {21, 172, 10},
  {29, 60, 6}, {29, 122, 9}, {38, 100, 12}, {47, 36, 7}, {47, 160, 8},
  {57, 12, 9}, {57, 118, 6}, {68, 70, 10}, {68, 182, 7},
};
#define SEAM_PERIOD 10

static int16_t wrap_x(int32_t x, int16_t w) {
  x %= w;
  return (int16_t)(x < 0 ? x + w : x);
}

static void streak(GContext *ctx, int16_t x, int16_t y, int16_t w) {
  graphics_fill_rect(ctx, GRect(x, y, w, 1), 0, GCornerNone);
}

static void draw_water_detail(GContext *ctx, GRect b, int16_t flat, int32_t phase,
                              int16_t bottom) {
  int16_t sw = b.size.w;
  int16_t seam = flat + BAND_TOP_H;

  // Glints, drifting with the swell; odd rows the other way, so they shimmer.
  graphics_context_set_fill_color(ctx, COL_FOAM);
  int32_t drift = phase / 2048;
  for (unsigned i = 0; i < ARRAY_LENGTH(GLINTS); i++) {
    const Streak *g = &GLINTS[i];
    int16_t y = flat + g->dy;
    if (y + 1 >= bottom || y + 1 >= seam - 3) continue;
    int16_t x = wrap_x((int32_t)g->x * sw / 200 + ((g->dy & 1) ? -drift : drift), sw + 8) - 4;
    streak(ctx, x, y, g->w);
    if (g->w > 2) streak(ctx, x + 1, y + 1, g->w - 2);
  }

  if (seam + 1 >= bottom) return;

  // The seam: the deep colour reaches up in widening dashes and the light one
  // reaches down once, so the two layers step into each other.
  int16_t off = (int16_t)wrap_x(phase / 4096, SEAM_PERIOD);
  graphics_context_set_fill_color(ctx, COL_WATER_DEEP);
  for (int16_t x = off - SEAM_PERIOD; x < sw; x += SEAM_PERIOD) {
    streak(ctx, x + 3, seam - 2, 3);
    streak(ctx, x + 1, seam - 1, 7);
  }
  graphics_context_set_fill_color(ctx, COL_WATER_TOP);
  for (int16_t x = off - SEAM_PERIOD + SEAM_PERIOD / 2; x < sw; x += SEAM_PERIOD) {
    streak(ctx, x + 3, seam, 3);
  }

  // Light reaching into the deep, drifting slowly against the swell.
  int32_t deep_drift = -phase / 4096;
  for (unsigned i = 0; i < ARRAY_LENGTH(DEEP); i++) {
    const Streak *d = &DEEP[i];
    int16_t y = seam + d->dy;
    if (y >= bottom) break;
    streak(ctx, wrap_x((int32_t)d->x * sw / 200 + deep_drift, sw + 12) - 6, y, d->w);
  }
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

  draw_water_detail(ctx, b, flat, wave_phase, bottom);

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
