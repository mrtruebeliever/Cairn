#pragma once
#include "config.h"

// Shared drawing helpers. Everything here is primitives — the only bitmap in
// the app is the launcher menu icon (plus the badge icons, loaded on demand).

// The frame a screen's layer gets. Every screen is laid out for a 228px-tall
// rectangle; on a round display that layout is centred vertically (the extra
// width only widens the centred rows), and `to_bottom` lets a screen whose
// scenery fills downwards -- the session's tide -- run on to the bezel.
GRect screen_frame(Layer *root, bool to_bottom);

// The frame for the table- and list-like screens, which are laid out column by
// column for exactly 200x228. On a round display that rectangle is centred, so
// only what sits in its corners needs a round-specific position.
GRect panel_frame(Layer *root);

// flint's 144x168 screen gets its own row positions and smaller fonts: the
// 228px layout does not fit, and scaling its rows would overlap the text.
// BY_SIZE(full, compact) picks the value for the screen at hand.
#if PBL_DISPLAY_HEIGHT < 200
#define COMPACT 1
// The full-size value is still evaluated (and discarded) so a font fetched only
// for it does not trip -Wunused-variable on this platform.
#define BY_SIZE(full, compact) ((void)(full), (compact))
#else
#define COMPACT 0
#define BY_SIZE(full, compact) (full)
#endif

// Titles hug the left edge on a rectangle; on a round screen the top-left
// corner is cut off, so they centre instead.
#define TITLE_ALIGN PBL_IF_ROUND_ELSE(GTextAlignmentCenter, GTextAlignmentLeft)

// Draws text centred horizontally in `b` at row y with height h.
void draw_center(GContext *ctx, const char *text, GFont font,
                 int16_t y, int16_t h, GRect b, GColor color);

// Word-wrapped block of text, left-aligned inside `r`. Returns the height used.
int16_t draw_wrapped(GContext *ctx, const char *text, GFont font, GRect r, GColor color);

// The cairn: `stones` pebbles stacked on a ground line, widest at the bottom.
// `base` is the centre of the ground line. `glow` highlights the top stone
// (used for a 7-day streak and during the growth animation).
void draw_cairn(GContext *ctx, GPoint base, int stones, int max_w, bool glow);

// The same stack seen through the water, drawn *after* draw_tide: everything
// below `water_y` comes back in a washed-out tint instead of vanishing.
// `water_y` should be the flat water level, not a wave crest, so the foam line
// stays on top of the stones.
void draw_cairn_underwater(GContext *ctx, GPoint base, int stones, int max_w,
                           bool glow, int16_t water_y);

// Trend glyph: dir < 0 down, 0 flat, > 0 up. Pebble fonts have no arrows.
void draw_trend_arrow(GContext *ctx, GPoint center, int dir, GColor color);

// A small right-pointing arrow, for "88 -> 74 bpm" style deltas.
void draw_delta_arrow(GContext *ctx, GPoint center, GColor color);

// Badge medal, drawn from primitives with a per-badge emblem.
void draw_badge(GContext *ctx, GPoint center, int radius, int badge, bool unlocked);

// The tide. Fills the band between a wavy surface line at `surface_y` and
// `bottom` with the two depth colours and draws the foam crest on top.
// `wave_phase` scrolls the wave so the water keeps moving even while the breath
// is held. Drawn last, over the cairn, so the stones are submerged for free as
// the level rises. Home passes a shallow `bottom`; the session passes b.size.h.
void draw_tide(GContext *ctx, GRect b, int16_t surface_y, int32_t wave_phase,
               int16_t bottom);

// The y below which the tide is solid water at any wave phase — the level to
// hand draw_cairn_underwater.
int16_t tide_flat_y(int16_t surface_y);

// The limestone shore the cairn stands on, spanning the full width at `y`.
void draw_shore(GContext *ctx, GRect b, int16_t y);

// A heart, drawn from two lobes and a taper so it scales to any size. `size` is
// the half-width. Always filled: an outlined version at this scale reads as two
// circles, so "no reading yet" is expressed with a dim colour instead.
void draw_heart(GContext *ctx, GPoint center, int16_t size, GColor color);
