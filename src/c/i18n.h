#pragma once
#include <pebble.h>

// Localization: device language via i18n_get_system_locale(), English fallback.
// Strings are UTF-8; Pebble system fonts cover Latin-1 only, so no ● / arrows —
// those symbols are drawn as primitives instead.

typedef enum { L_EN, L_NL, L_FR, L_DE, L_ES, NUM_LANGS } Lang;

enum {
  S_APP_NAME,

  // Home
  S_HOME_STREAK,       // "Streak: %d d"
  S_HOME_STREAK_NONE,
  S_HOME_LAST,         // "Last: %d"
  S_HOME_NEVER,
  S_HOME_FIRST,        // onboarding line while the cairn is still empty
  S_HINT_START,
  S_HINT_PRESET,
  S_HINT_HISTORY,
  S_HINT_SETTINGS,
  S_HINT_INSIGHTS,

  // Presets
  S_PRESET_COHERENT, S_PRESET_BOX, S_PRESET_478, S_PRESET_CUSTOM,

  // Session
  S_PHASE_IN, S_PHASE_HOLD, S_PHASE_OUT,
  S_BPM_FMT,           // "%d bpm"
  S_HR_SEARCH,         // shown while the sensor has not produced a reading yet
  S_HR_SKIP,
  S_HR_TIGHTEN,
  S_STOP_TITLE,
  S_STOP_HINT,
  S_PAUSED, S_HINT_RESUME,

  // Result
  S_RESULT_TITLE,
  S_RESULT_CALM,
  S_RESULT_HR_FMT,     // "%d > %d bpm" (the arrow is drawn, not typed)
  S_RESULT_NO_HR,
  S_NEW_BEST,
  S_STREAK_FMT,        // "%d days in a row"
  S_STREAK_ONE,
  S_BADGE_NEW,
  S_HINT_CLOSE,

  // Settings
  S_SETTINGS_TITLE, S_OPT_PRESET, S_OPT_RHYTHM, S_OPT_LEN, S_OPT_SOUND,
  S_OPT_HAPTICS, S_OPT_BACKLIGHT, S_OPT_SHOW_HR, S_OPT_RESET,
  S_ON, S_OFF, S_BL_TIMED, S_BL_ALWAYS,
  S_SND_SOFT, S_SND_NORMAL, S_SND_LOUD, S_SND_MUTED,
  S_RESET_CONFIRM, S_SETTINGS_HINT, S_RHYTHM_HINT,
  S_MIN_FMT,           // "%d min"

  // History
  S_HISTORY_TITLE, S_HISTORY_EMPTY,

  // Insights
  S_INSIGHTS_TITLE, S_IN_CALM, S_IN_RESTHR, S_IN_STREAK,
  S_IN_7D, S_IN_30D, S_TIP_WEEK, S_NO_DATA, S_IN_LOCAL,

  // Badge names + unlock conditions
  S_BADGE_FIRST, S_BADGE_STEADY, S_BADGE_ZENWEEK, S_BADGE_DEEP,
  S_BADGE_CENTURY, S_BADGE_TIDE,
  S_BADGE_FIRST_D, S_BADGE_STEADY_D, S_BADGE_ZENWEEK_D, S_BADGE_DEEP_D,
  S_BADGE_CENTURY_D, S_BADGE_TIDE_D,

  // Per-session coaching tips (rule table, highest matching priority wins)
  S_TIP_BEST, S_TIP_NOHR, S_TIP_HR_UP, S_TIP_HR_HIGH, S_TIP_BIGDROP,
  S_TIP_FLAT, S_TIP_RSA_GOOD, S_TIP_RSA_LOW, S_TIP_SHORT, S_TIP_LONG_DONE,
  S_TIP_HRV_GOOD, S_TIP_HRV_LOW,

  // Trend coaching tips (7/30-day patterns)
  S_TT_SCORE_UP, S_TT_SCORE_DOWN, S_TT_PLATEAU, S_TT_HR_DOWN, S_TT_HR_UP,
  S_TT_STREAK_UP, S_TT_STREAK_BROKEN, S_TT_FEW, S_TT_QUITS, S_TT_PRESET_BEST,

  // Settings, added later (appended so the rows above keep their places)
  S_OPT_REMINDER,

  NUM_STRS
};

extern const char **s_tr;
#define S(id) s_tr[id]

void i18n_init(void);
