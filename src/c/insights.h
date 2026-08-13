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
  bool     has_hrv;      // true once the session collected enough valid PPI samples
  uint16_t rmssd_ms;     // RMSSD of successive heartbeat PPI differences, ms
  uint8_t  completion;   // 0..100 % of the planned duration actually breathed
  uint8_t  score;        // 0..100 calm score
  uint16_t dur_s;
  uint8_t  preset;
  bool     new_best;
} SessionResult;

// Calm score from the measured session: completion, HR drop and RSA (bpm swing
// per breath), plus RMSSD when the sensor delivered peak-to-peak intervals
// (SDK 4.33+; falls back to the RSA-only blend when it didn't).
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
