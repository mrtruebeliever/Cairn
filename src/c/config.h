#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// Persist keys. Numbered from 1, independent of the AppMessage message keys.
// Never reuse a retired number — add a new one instead.
// ---------------------------------------------------------------------------
#define PERSIST_PRESET_IDX      1
#define PERSIST_CUSTOM_IN       2
#define PERSIST_CUSTOM_HOLD1    3
#define PERSIST_CUSTOM_OUT      4
#define PERSIST_CUSTOM_HOLD2    5
#define PERSIST_SESSION_LEN     6   // index into SESSION_LENS
#define PERSIST_SOUND_VOL       7   // 0=Off 1=Soft 2=Normal 3=Loud
#define PERSIST_HAPTICS_ON      8
#define PERSIST_BL_MODE         9   // BacklightMode
#define PERSIST_SHOW_HR         10
#define PERSIST_STREAK          11
#define PERSIST_LAST_DATE       12  // yyyymmdd of the last completed session
#define PERSIST_BEST_SCORE      13
#define PERSIST_TOTAL_SESSIONS  14
#define PERSIST_TOTAL_MINUTES   15
#define PERSIST_CAIRN_SIZE      16
#define PERSIST_BADGES          17  // bitmask over Badge
#define PERSIST_HISTORY         18  // packed SessionRecord[] via persist_write_data
#define PERSIST_HISTORY_COUNT   19
#define PERSIST_TREND           20  // packed TrendData from the phone

// ---------------------------------------------------------------------------
// Breathing presets. Phase lengths in tenths of a second so 5.5 s fits an int.
// ---------------------------------------------------------------------------
typedef struct {
  uint16_t in, hold1, out, hold2;
} BreathPattern;

#define PRESET_COHERENT  0
#define PRESET_BOX       1
#define PRESET_478       2
#define PRESET_CUSTOM    3
#define NUM_PRESETS      4

// Custom-pattern bounds, in tenths of a second (0 s .. 12 s, 0.5 s steps).
#define CUSTOM_STEP   5
#define CUSTOM_MIN_IN 20   // in/out must be at least 2 s; holds may be 0
#define CUSTOM_MAX    120

// Selectable session lengths in minutes.
#define NUM_SESSION_LENS 6
extern const uint8_t SESSION_LENS[NUM_SESSION_LENS];

// ---------------------------------------------------------------------------
// Badges — one bit each in PERSIST_BADGES.
// ---------------------------------------------------------------------------
typedef enum {
  BADGE_FIRST,     // first completed session
  BADGE_STEADY,    // 3-day streak
  BADGE_ZENWEEK,   // 7-day streak
  BADGE_DEEP,      // score >= 90
  BADGE_CENTURY,   // 100 sessions
  BADGE_TIDE,      // 1000 total minutes
  NUM_BADGES
} Badge;

// ---------------------------------------------------------------------------
// A finished session, as stored on the watch and shipped to the phone.
// ---------------------------------------------------------------------------
// 12 bytes after padding; 20 of these fit inside the 256-byte persist limit.
typedef struct {
  uint32_t date;        // yyyymmdd
  uint16_t dur_s;       // seconds actually breathed
  uint8_t  hr_start;    // 0 when no heart rate was available
  uint8_t  hr_end;
  uint8_t  score;       // 0..100 calm score
  uint8_t  preset;      // PRESET_*
  uint8_t  completion;  // 0..100 % of the planned duration
} SessionRecord;

#define HISTORY_MAX 20

// Trend-tip ids. The phone sends an index from this list in TREND_TIP, so the
// order is part of the wire format — append only.
typedef enum {
  TT_SCORE_UP, TT_SCORE_DOWN, TT_PLATEAU, TT_HR_DOWN, TT_HR_UP,
  TT_STREAK_UP, TT_STREAK_BROKEN, TT_FEW, TT_QUITS, TT_PRESET_BEST,
  NUM_TREND_TIPS
} TrendTip;

// Trend summary, either computed by the phone (full history) or locally from
// the last HISTORY_MAX sessions.
typedef struct {
  int8_t  score_7, score_30;    // average calm score, -1 when unknown
  int8_t  hr_7, hr_30;          // average resting (session-start) HR, -1 unknown
  int8_t  streak;
  int8_t  dir_score, dir_hr;    // -1 down, 0 flat, 1 up
  int8_t  tip;                  // TrendTip, -1 when there is nothing to say
  int8_t  best_preset;          // PRESET_* the user scores highest with, -1 none
  int8_t  sessions_7;
  bool    from_phone;
} TrendData;

// ---------------------------------------------------------------------------
// Backlight modes (shared 3-state convention across the projects).
// ---------------------------------------------------------------------------
typedef enum { BL_OFF, BL_TIMED, BL_ALWAYS, NUM_BL_MODES } BacklightMode;
#define BL_TIMEOUT_MS 6000

// ---------------------------------------------------------------------------
// Palette — "night tide": a black night sky over teal water, limestone shore,
// foam that lights up against the dark. The water is the only bright thing on
// screen, which suits an app used in the evening, and it avoids both the
// navy-cyan-orange look every other breathing app uses and any orange at all.
// ---------------------------------------------------------------------------
#define COL_BG          GColorBlack             // the night sky above the water
#define COL_PANEL       GColorDarkGray          // 555555 — panels, insets, highlights
#define COL_TEXT        GColorWhite
#define COL_DIM         GColorLightGray
#define COL_ACCENT      GColorIcterine          // FFFF55 — scores, streak, progress
#define COL_VALUE       GColorCeleste           // AAFFFF — secondary values
#define COL_HR          GColorMelon             // FFAAAA — a warm pulse colour

// The tide itself: a sunlit layer, the depth below it, and the foam crest.
#define COL_WATER_TOP   GColorTiffanyBlue       // 00AAAA — just under the surface
#define COL_WATER_DEEP  GColorMidnightGreen     // 005555 — everything below
#define COL_FOAM        GColorCeleste           // AAFFFF — the crest line
#define COL_SAND        GColorBrass             // AAAA55 — limestone shore

// Cairn stones.
#define COL_STONE       GColorLightGray
#define COL_STONE_DARK  GColorDarkGray
// Submerged stone: light enough to stand out against both water bands, so the
// cairn stays visible once the tide has closed over it.
#define COL_STONE_WET       GColorCadetBlue     // 55AAAA
#define COL_STONE_WET_GLOW  GColorBrass         // AAAA55 — the lit top stone, drowned

// Phase cues. The water's direction carries most of the meaning; these only
// tint the phase label.
#define COL_PHASE_IN    GColorCeleste
#define COL_PHASE_HOLD  GColorIcterine
#define COL_PHASE_OUT   GColorTiffanyBlue
