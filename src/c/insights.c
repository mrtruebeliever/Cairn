#include "insights.h"
#include "i18n.h"
#include <string.h>

// Normalization ceilings. A 12-bpm drop over a session, an 8-bpm respiratory
// swing and a 40 ms RMSSD both count as "as good as it gets"; these are
// starting values to be calibrated on real hardware.
#define HR_DROP_FULL 12
#define RSA_FULL     8
#define RMSSD_FULL   40

static int32_t norm(int32_t v, int32_t full) {
  if (v <= 0) return 0;
  if (v >= full) return 100;
  return v * 100 / full;
}

uint8_t insights_score(const SessionResult *r) {
  if (!r->has_hr) {
    // Without a heart rate we only know that the user showed up and finished,
    // so the ceiling is deliberately below 100 — a measured session can beat
    // an unmeasured one.
    return (uint8_t)((int32_t)r->completion * 90 / 100);
  }
  int32_t drop = norm(r->hr_drop, HR_DROP_FULL);
  int32_t rsa  = norm(r->rsa, RSA_FULL);
  int32_t s;
  if (r->has_hrv) {
    // RMSSD is the more direct signal, so it takes the biggest share once the
    // sensor delivered it; RSA keeps a smaller weight rather than dropping out,
    // since the two agreeing is itself informative.
    int32_t rmssd = norm(r->rmssd_ms, RMSSD_FULL);
    s = ((int32_t)r->completion * 25 + drop * 25 + rsa * 20 + rmssd * 30) / 100;
  } else {
    s = ((int32_t)r->completion * 35 + drop * 35 + rsa * 30) / 100;
  }
  if (s < 0) s = 0;
  if (s > 100) s = 100;
  return (uint8_t)s;
}

// Rule table from the plan: {condition, string id}, evaluated top to bottom so
// the first match is the highest-priority advice. The RMSSD rules sit ahead of
// the RSA ones: RMSSD is the more direct signal, so it wins when both are
// available.
int insights_session_tip(const SessionResult *r) {
  if (r->new_best)                              return S_TIP_BEST;
  if (!r->has_hr)                               return S_TIP_NOHR;
  if (r->hr_end > r->hr_start + 3)              return S_TIP_HR_UP;
  if (r->hr_start > 90)                         return S_TIP_HR_HIGH;
  if (r->hr_drop >= 6)                          return S_TIP_BIGDROP;
  if (r->hr_drop <= 1 && r->completion >= 90)   return S_TIP_FLAT;
  if (r->has_hrv && r->rmssd_ms >= 35)          return S_TIP_HRV_GOOD;
  if (r->has_hrv && r->rmssd_ms <= 15 && r->completion >= 90) return S_TIP_HRV_LOW;
  if (r->rsa >= 5)                              return S_TIP_RSA_GOOD;
  if (r->rsa <= 2)                              return S_TIP_RSA_LOW;
  if (r->completion < 60)                       return S_TIP_SHORT;
  return S_TIP_LONG_DONE;
}

// ---------------------------------------------------------------------------
// Trends
// ---------------------------------------------------------------------------
#include "settings.h"
#include "stats.h"

// Days since 1970-01-01 for a proleptic-Gregorian date (Hinnant's algorithm).
// yyyymmdd strings can't be subtracted directly, so both dates go through this.
static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d) {
  y -= (m <= 2);
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = (uint32_t)(y - era * 400);
  const uint32_t doy = (153u * (m + (m > 2 ? -3u : 9u)) + 2u) / 5u + d - 1u;
  const uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
  return era * 146097 + (int32_t)doe - 719468;
}

static int32_t ymd_to_days(uint32_t ymd) {
  return days_from_civil((int32_t)(ymd / 10000u), (ymd / 100u) % 100u, ymd % 100u);
}

int32_t insights_day_diff(uint32_t a, uint32_t b) {
  if (!a || !b) return 0;
  return ymd_to_days(a) - ymd_to_days(b);
}

// One accumulator per window (last 7 days, 7-14 days ago, and so on).
typedef struct {
  int32_t score_sum, score_n;
  int32_t hr_sum, hr_n;
  int32_t quits, sessions;
} Bucket;

static void bucket_add(Bucket *bk, const SessionRecord *r) {
  bk->sessions++;
  bk->score_sum += r->score;
  bk->score_n++;
  if (r->hr_start) {
    bk->hr_sum += r->hr_start;
    bk->hr_n++;
  }
  if (r->completion < 60) bk->quits++;
}

static int8_t bucket_score(const Bucket *bk) {
  return bk->score_n ? (int8_t)(bk->score_sum / bk->score_n) : -1;
}

static int8_t bucket_hr(const Bucket *bk) {
  return bk->hr_n ? (int8_t)(bk->hr_sum / bk->hr_n) : -1;
}

// A direction with a deadband, so noise doesn't read as progress.
static int8_t direction(int8_t now, int8_t prev, int8_t deadband) {
  if (now < 0 || prev < 0) return 0;
  int diff = now - prev;
  if (diff > deadband) return 1;
  if (diff < -deadband) return -1;
  return 0;
}

void insights_local_trend(TrendData *out) {
  memset(out, 0, sizeof(*out));
  out->score_7 = out->score_30 = -1;
  out->hr_7 = out->hr_30 = -1;
  out->tip = -1;
  out->best_preset = -1;
  out->streak = (int8_t)(g_stats.streak > 127 ? 127 : g_stats.streak);

  Bucket b7 = {0}, b14 = {0}, b30 = {0};
  int32_t preset_sum[NUM_PRESETS] = {0};
  int32_t preset_n[NUM_PRESETS] = {0};

  uint32_t today = stats_today();
  int count = stats_history_count();
  for (int i = 0; i < count; i++) {
    SessionRecord r;
    if (!stats_history_get(i, &r)) continue;
    int32_t age = insights_day_diff(today, r.date);
    if (age < 0) age = 0;
    if (age < 7)        bucket_add(&b7, &r);
    else if (age < 14)  bucket_add(&b14, &r);
    if (age < 30)       bucket_add(&b30, &r);
    if (r.preset < NUM_PRESETS) {
      preset_sum[r.preset] += r.score;
      preset_n[r.preset]++;
    }
  }

  out->score_7  = bucket_score(&b7);
  out->score_30 = bucket_score(&b30);
  out->hr_7     = bucket_hr(&b7);
  out->hr_30    = bucket_hr(&b30);
  out->sessions_7 = (int8_t)(b7.sessions > 127 ? 127 : b7.sessions);
  out->dir_score = direction(out->score_7, bucket_score(&b14), 3);
  out->dir_hr    = direction(out->hr_7, bucket_hr(&b14), 2);

  // A preset only counts as "clearly best" with at least two sessions on it and
  // a five-point lead over the runner-up.
  int8_t best = -1, runner = -1;
  int32_t best_avg = -1, runner_avg = -1;
  for (int p = 0; p < NUM_PRESETS; p++) {
    if (preset_n[p] < 2) continue;
    int32_t avg = preset_sum[p] / preset_n[p];
    if (avg > best_avg) {
      runner = best; runner_avg = best_avg;
      best = (int8_t)p; best_avg = avg;
    } else if (avg > runner_avg) {
      runner = (int8_t)p; runner_avg = avg;
    }
  }
  if (best >= 0 && (runner < 0 || best_avg - runner_avg >= 5)) out->best_preset = best;

  // Trend-tip rules, most actionable first.
  if (g_stats.total_sessions > 0 && g_stats.streak == 0) out->tip = TT_STREAK_BROKEN;
  else if (b7.sessions > 0 && b7.sessions < 3)           out->tip = TT_FEW;
  else if (b7.sessions >= 2 && b7.quits * 2 >= b7.sessions) out->tip = TT_QUITS;
  else if (out->dir_score > 0)                           out->tip = TT_SCORE_UP;
  else if (out->dir_score < 0)                           out->tip = TT_SCORE_DOWN;
  else if (out->dir_hr < 0)                              out->tip = TT_HR_DOWN;
  else if (out->dir_hr > 0)                              out->tip = TT_HR_UP;
  else if (g_stats.streak >= 3)                          out->tip = TT_STREAK_UP;
  else if (out->best_preset >= 0)                        out->tip = TT_PRESET_BEST;
  else if (out->score_7 >= 0)                            out->tip = TT_PLATEAU;
}

const char *insights_trend_text(const TrendData *t, char *buf, size_t len) {
  static const int ids[NUM_TREND_TIPS] = {
    S_TT_SCORE_UP, S_TT_SCORE_DOWN, S_TT_PLATEAU, S_TT_HR_DOWN, S_TT_HR_UP,
    S_TT_STREAK_UP, S_TT_STREAK_BROKEN, S_TT_FEW, S_TT_QUITS, S_TT_PRESET_BEST,
  };
  if (t->tip < 0 || t->tip >= NUM_TREND_TIPS) return NULL;
  const char *fmt = S(ids[t->tip]);
  if (t->tip == TT_STREAK_UP) snprintf(buf, len, fmt, (int)t->streak);
  else if (t->tip == TT_PRESET_BEST) {
    snprintf(buf, len, fmt, settings_preset_name(t->best_preset < 0 ? 0 : t->best_preset));
  } else snprintf(buf, len, "%s", fmt);
  return buf;
}
