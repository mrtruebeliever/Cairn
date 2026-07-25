#pragma once
#include "config.h"

// Everything the result screen and the coaching rules need to know about a
// session. `has_hr` is false whenever the heart-rate sensor never produced a
// reading (notably in the emulator), which changes both the score and the tip.
typedef struct {
  bool     has_hr;
  uint8_t  hr_start;
  uint8_t  hr_end;
  int16_t  hr_drop;      // avg(first fifth) - avg(last fifth), bpm; may be < 0
  int16_t  rsa;          // avg peak-to-trough HR swing per breath cycle, bpm
  uint8_t  completion;   // 0..100 % of the planned duration actually breathed
  uint8_t  score;        // 0..100 calm score
  uint16_t dur_s;
  uint8_t  preset;
  bool     new_best;
} SessionResult;

// Calm score from the measured session. Honest about its limits: the SDK only
// exposes BPM (no beat-to-beat RR intervals), so this is a calm/coherence
// score, not clinical HRV.
uint8_t insights_score(const SessionResult *r);

// The single per-session coaching line. Rule table, highest matching priority
// wins; returns an S_TIP_* string id.
int insights_session_tip(const SessionResult *r);

// Days between two yyyymmdd dates (a - b), correct across month/year borders.
int32_t insights_day_diff(uint32_t a, uint32_t b);

// Trends from the last HISTORY_MAX sessions stored on the watch. Used when the
// phone is out of reach; the phone computes the same shape from its full
// localStorage history and sends it back over TREND_*.
void insights_local_trend(TrendData *out);

// The trend advice line for a TrendTip, formatted into buf (some lines carry a
// streak count or preset name). Returns buf, or NULL when there is no tip.
const char *insights_trend_text(const TrendData *t, char *buf, size_t len);
