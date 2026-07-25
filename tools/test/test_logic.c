// Host-side checks for the parts of Cairn that are pure logic: the calm score,
// the coaching rule tables, the date arithmetic behind the streak and the
// trend buckets, and settings/history persistence.
//
// Build & run:  tools/test/run.sh

#include "../../src/c/insights.h"
#include "../../src/c/settings.h"
#include "../../src/c/stats.h"
#include "../../src/c/i18n.h"

static int g_fail;

#define CHECK(cond, ...)                                                     \
  do {                                                                       \
    if (!(cond)) {                                                           \
      printf("  FAIL %s:%d  ", __FILE__, __LINE__);                          \
      printf(__VA_ARGS__);                                                   \
      printf("\n");                                                          \
      g_fail++;                                                              \
    }                                                                        \
  } while (0)

#define CHECK_EQ(actual, expect, label)                                      \
  CHECK((actual) == (expect), "%s: got %d, want %d", label, (int)(actual), (int)(expect))

// --- helpers ----------------------------------------------------------------

static uint32_t ymd_offset(int days_ago) {
  time_t t = time(NULL) - (time_t)days_ago * 86400;
  struct tm *tm = localtime(&t);
  return (uint32_t)(tm->tm_year + 1900) * 10000u + (uint32_t)(tm->tm_mon + 1) * 100u
       + (uint32_t)tm->tm_mday;
}

static SessionRecord mk(int days_ago, uint8_t score, uint8_t hr_start,
                        uint8_t completion, uint8_t preset) {
  SessionRecord r = {0};
  r.date = ymd_offset(days_ago);
  r.dur_s = 180;
  r.hr_start = hr_start;
  r.hr_end = hr_start ? (uint8_t)(hr_start - 4) : 0;
  r.score = score;
  r.preset = preset;
  r.completion = completion;
  return r;
}

static void reset_all(void) {
  test_persist_clear();
  settings_load();
  stats_load();
}

// --- date arithmetic --------------------------------------------------------

static void test_day_diff(void) {
  printf("day_diff\n");
  CHECK_EQ(insights_day_diff(20260301, 20260228), 1, "leap-year Feb 28 -> Mar 1 2026");
  CHECK_EQ(insights_day_diff(20240301, 20240229), 1, "leap day 2024");
  CHECK_EQ(insights_day_diff(20250101, 20241231), 1, "year boundary");
  CHECK_EQ(insights_day_diff(20260801, 20260701), 31, "July has 31 days");
  CHECK_EQ(insights_day_diff(20260725, 20260725), 0, "same day");
  CHECK_EQ(insights_day_diff(20260725, 20260726), -1, "negative direction");
  CHECK_EQ(insights_day_diff(0, 20260725), 0, "missing date is treated as no gap");
}

// --- calm score -------------------------------------------------------------

static void test_score(void) {
  printf("score\n");
  SessionResult r = {0};

  // No heart rate: completion only, capped below 100 so a measured session can win.
  r.has_hr = false;
  r.completion = 100;
  CHECK_EQ(insights_score(&r), 90, "no-HR full session");
  r.completion = 50;
  CHECK_EQ(insights_score(&r), 45, "no-HR half session");

  // With heart rate: 35% completion, 35% HR drop, 30% RSA.
  r.has_hr = true;
  r.completion = 100;
  r.hr_drop = 12;   // at the normalization ceiling
  r.rsa = 8;
  CHECK_EQ(insights_score(&r), 100, "perfect measured session");
  r.hr_drop = 0;
  r.rsa = 0;
  CHECK_EQ(insights_score(&r), 35, "completed but no measurable calming");
  r.hr_drop = -5;   // heart rate went up
  CHECK_EQ(insights_score(&r), 35, "negative drop clamps to zero, never below");
  r.hr_drop = 6;
  r.rsa = 4;
  CHECK_EQ(insights_score(&r), 67, "mid-range session");
}

// --- per-session tip rules --------------------------------------------------

static void test_session_tips(void) {
  printf("session tips\n");
  SessionResult r = {0};
  r.has_hr = true;
  r.completion = 100;
  r.hr_start = 70;
  r.hr_end = 66;
  r.hr_drop = 4;
  r.rsa = 4;

  r.new_best = true;
  CHECK_EQ(insights_session_tip(&r), S_TIP_BEST, "new best wins over everything");
  r.new_best = false;

  SessionResult n = r;
  n.has_hr = false;
  CHECK_EQ(insights_session_tip(&n), S_TIP_NOHR, "no HR beats every HR rule");

  n = r; n.hr_end = (uint8_t)(n.hr_start + 5);
  CHECK_EQ(insights_session_tip(&n), S_TIP_HR_UP, "HR climbed");

  n = r; n.hr_start = 95;
  CHECK_EQ(insights_session_tip(&n), S_TIP_HR_HIGH, "started rushed");

  n = r; n.hr_drop = 9;
  CHECK_EQ(insights_session_tip(&n), S_TIP_BIGDROP, "big drop");

  n = r; n.hr_drop = 0;
  CHECK_EQ(insights_session_tip(&n), S_TIP_FLAT, "flat HR on a completed session");

  n = r; n.rsa = 6;
  CHECK_EQ(insights_session_tip(&n), S_TIP_RSA_GOOD, "good coherence");

  n = r; n.rsa = 1;
  CHECK_EQ(insights_session_tip(&n), S_TIP_RSA_LOW, "low coherence");

  n = r; n.completion = 40;
  CHECK_EQ(insights_session_tip(&n), S_TIP_SHORT, "stopped early");

  CHECK_EQ(insights_session_tip(&r), S_TIP_LONG_DONE, "fallback for a clean session");
}

// --- streak logic -----------------------------------------------------------

static void test_streak(void) {
  printf("streak\n");
  reset_all();
  bool best = false;

  SessionRecord r = mk(0, 70, 70, 100, PRESET_COHERENT);
  stats_record(&r, &best);
  CHECK_EQ(g_stats.streak, 1, "first session starts the streak");
  CHECK(best, "first score is always a new best");

  // A second session on the same day must not extend the streak.
  stats_record(&r, &best);
  CHECK_EQ(g_stats.streak, 1, "second session today");
  CHECK_EQ(g_stats.total_sessions, 2, "but it does count as a session");
  CHECK(!best, "same score is not a new best");

  // Yesterday -> today extends it.
  reset_all();
  g_stats.last_date = (int32_t)ymd_offset(1);
  g_stats.streak = 4;
  g_stats.total_sessions = 4;
  stats_record(&r, &best);
  CHECK_EQ(g_stats.streak, 5, "yesterday extends the streak");

  // A skipped day resets it to 1.
  reset_all();
  g_stats.last_date = (int32_t)ymd_offset(3);
  g_stats.streak = 9;
  stats_record(&r, &best);
  CHECK_EQ(g_stats.streak, 1, "a gap resets the streak");

  // A stale streak is already dead at load time, before any new session.
  test_persist_clear();
  persist_write_int(PERSIST_STREAK, 12);
  persist_write_int(PERSIST_LAST_DATE, (int32_t)ymd_offset(5));
  stats_load();
  CHECK_EQ(g_stats.streak, 0, "stale streak is cleared on load");

  test_persist_clear();
  persist_write_int(PERSIST_STREAK, 12);
  persist_write_int(PERSIST_LAST_DATE, (int32_t)ymd_offset(1));
  stats_load();
  CHECK_EQ(g_stats.streak, 12, "yesterday's streak survives a relaunch");
}

// --- badges -----------------------------------------------------------------

static void test_badges(void) {
  printf("badges\n");
  reset_all();
  bool best = false;

  SessionRecord r = mk(0, 70, 70, 100, PRESET_COHERENT);
  uint32_t fresh = stats_record(&r, &best);
  CHECK(fresh & (1u << BADGE_FIRST), "first session unlocks First breath");
  CHECK(!(fresh & (1u << BADGE_DEEP)), "score 70 does not unlock Deep calm");

  // The same badge must never fire twice.
  fresh = stats_record(&r, &best);
  CHECK_EQ(fresh, 0, "no repeat unlocks");

  SessionRecord hi = mk(0, 92, 70, 100, PRESET_COHERENT);
  fresh = stats_record(&hi, &best);
  CHECK(fresh & (1u << BADGE_DEEP), "score 92 unlocks Deep calm");

  reset_all();
  g_stats.last_date = (int32_t)ymd_offset(1);
  g_stats.streak = 2;
  g_stats.total_sessions = 2;
  fresh = stats_record(&r, &best);
  CHECK(fresh & (1u << BADGE_STEADY), "a 3-day streak unlocks Steady");
  CHECK(!(fresh & (1u << BADGE_ZENWEEK)), "3 days is not yet Zen week");
}

// --- history ring + persistence --------------------------------------------

static void test_history(void) {
  printf("history\n");
  reset_all();
  bool best = false;

  for (int i = 0; i < HISTORY_MAX + 5; i++) {
    SessionRecord r = mk(0, (uint8_t)(i + 1), 70, 100, PRESET_BOX);
    stats_record(&r, &best);
  }
  CHECK_EQ(stats_history_count(), HISTORY_MAX, "history is capped");

  SessionRecord newest;
  CHECK(stats_history_get(0, &newest), "index 0 exists");
  CHECK_EQ(newest.score, HISTORY_MAX + 5, "index 0 is the most recent session");
  SessionRecord oldest;
  CHECK(stats_history_get(HISTORY_MAX - 1, &oldest), "last index exists");
  CHECK_EQ(oldest.score, 6, "the oldest five records were dropped");
  CHECK(!stats_history_get(HISTORY_MAX, &oldest), "reading past the end fails");
  CHECK(!stats_history_get(-1, &oldest), "reading before the start fails");

  // The blob must fit the 256-byte persist limit.
  CHECK(sizeof(SessionRecord) * HISTORY_MAX <= 256,
        "history blob is %d bytes, persist allows 256",
        (int)(sizeof(SessionRecord) * HISTORY_MAX));

  // Survives a reload.
  stats_load();
  CHECK_EQ(stats_history_count(), HISTORY_MAX, "history survives a relaunch");
  CHECK(stats_history_get(0, &newest) && newest.score == HISTORY_MAX + 5,
        "newest record survives a relaunch");
}

// --- settings persistence ---------------------------------------------------

static void test_settings(void) {
  printf("settings\n");
  reset_all();

  // Sound must default to Off: the emulator has no speaker and playback kills
  // the app, so a fresh install has to be silent.
  CHECK_EQ(g_set.sound_vol, 0, "sound defaults to Off");
  CHECK_EQ(g_set.haptics_on, true, "haptics default on");
  CHECK_EQ(g_set.preset_idx, PRESET_COHERENT, "default preset");
  CHECK_EQ(settings_len_min(), 3, "default duration");

  settings_set_preset(PRESET_478);
  settings_set_sound(2);
  settings_set_len(4);
  settings_set_haptics(false);
  settings_load();
  CHECK_EQ(g_set.preset_idx, PRESET_478, "preset persists");
  CHECK_EQ(g_set.sound_vol, 2, "volume persists");
  CHECK_EQ(settings_len_min(), 10, "duration persists");
  CHECK_EQ(g_set.haptics_on, false, "haptics persist");

  // Cycling wraps rather than running off the end.
  settings_set_preset(NUM_PRESETS);
  CHECK_EQ(g_set.preset_idx, 0, "preset wraps");
  settings_set_sound(4);
  CHECK_EQ(g_set.sound_vol, 0, "volume wraps");

  // Custom rhythm: in/out have a floor, holds may be zero, both ends wrap.
  settings_set_custom(0, 0);
  CHECK_EQ(g_set.custom[0], CUSTOM_MAX, "stepping below the inhale floor wraps to the top");
  settings_set_custom(0, CUSTOM_MAX + CUSTOM_STEP);
  CHECK_EQ(g_set.custom[0], CUSTOM_MIN_IN, "stepping past the top wraps to the floor");
  settings_set_custom(1, 0);
  CHECK_EQ(g_set.custom[1], 0, "a hold of zero is allowed");

  settings_set_preset(PRESET_CUSTOM);
  settings_set_custom(0, 40);
  settings_set_custom(1, 0);
  settings_set_custom(2, 60);
  settings_set_custom(3, 0);
  BreathPattern p = settings_pattern();
  CHECK_EQ(p.in, 40, "custom in");
  CHECK_EQ(p.out, 60, "custom out");
  CHECK_EQ(settings_cycle_tenths(), 100, "custom cycle length");

  settings_set_preset(PRESET_BOX);
  CHECK_EQ(settings_cycle_tenths(), 160, "box cycle is 16 s");
}

// --- trends -----------------------------------------------------------------

static void add(int days_ago, uint8_t score, uint8_t hr, uint8_t completion, uint8_t preset) {
  bool best;
  SessionRecord r = mk(days_ago, score, hr, completion, preset);
  stats_record(&r, &best);
}

static void test_trend(void) {
  printf("trends\n");

  // Improving scores across the two most recent weeks.
  reset_all();
  add(10, 50, 80, 100, PRESET_COHERENT);
  add(9, 52, 80, 100, PRESET_COHERENT);
  add(3, 75, 72, 100, PRESET_COHERENT);
  add(2, 79, 72, 100, PRESET_COHERENT);
  add(1, 77, 72, 100, PRESET_COHERENT);
  TrendData t;
  insights_local_trend(&t);
  CHECK_EQ(t.score_7, 77, "7-day average");
  CHECK_EQ(t.dir_score, 1, "scores rising");
  CHECK_EQ(t.dir_hr, -1, "resting HR falling");
  CHECK_EQ(t.tip, TT_SCORE_UP, "rising score is the headline");
  CHECK(!t.from_phone, "local trend is flagged as watch-only");

  // A quiet week gets the "few sessions" nudge before anything else.
  reset_all();
  add(1, 70, 70, 100, PRESET_COHERENT);
  insights_local_trend(&t);
  CHECK_EQ(t.sessions_7, 1, "one session this week");
  CHECK_EQ(t.tip, TT_FEW, "quiet week");

  // Frequent early stops.
  reset_all();
  add(3, 40, 70, 30, PRESET_COHERENT);
  add(2, 45, 70, 40, PRESET_COHERENT);
  add(1, 50, 70, 100, PRESET_COHERENT);
  insights_local_trend(&t);
  CHECK_EQ(t.tip, TT_QUITS, "two of three sessions cut short");

  // A broken streak outranks everything. stats_record() always stamps a session
  // with today's date (a session is only ever recorded as it finishes), so the
  // dead-streak state has to be set the way stats_load() would leave it after a
  // skipped day rather than by backdating the records.
  reset_all();
  add(4, 70, 70, 100, PRESET_COHERENT);
  add(3, 72, 70, 100, PRESET_COHERENT);
  add(2, 74, 70, 100, PRESET_COHERENT);
  g_stats.streak = 0;
  g_stats.last_date = (int32_t)ymd_offset(2);
  insights_local_trend(&t);
  CHECK_EQ(t.streak, 0, "no session yesterday or today");
  CHECK_EQ(t.tip, TT_STREAK_BROKEN, "broken streak is the headline");

  // Preset recommendation needs two sessions and a clear lead.
  reset_all();
  add(5, 90, 0, 100, PRESET_BOX);
  add(4, 92, 0, 100, PRESET_BOX);
  add(3, 60, 0, 100, PRESET_478);
  add(2, 62, 0, 100, PRESET_478);
  insights_local_trend(&t);
  CHECK_EQ(t.best_preset, PRESET_BOX, "Box scores clearly highest");

  reset_all();
  add(5, 80, 0, 100, PRESET_BOX);
  add(4, 81, 0, 100, PRESET_BOX);
  add(3, 79, 0, 100, PRESET_478);
  add(2, 80, 0, 100, PRESET_478);
  insights_local_trend(&t);
  CHECK_EQ(t.best_preset, -1, "a one-point lead is not a recommendation");

  // Empty history says nothing at all.
  reset_all();
  insights_local_trend(&t);
  CHECK_EQ(t.score_7, -1, "no data");
  CHECK_EQ(t.tip, -1, "no tip without data");
  char buf[160];
  CHECK(insights_trend_text(&t, buf, sizeof(buf)) == NULL, "no tip text without data");

  // Every trend tip must format without crashing and produce something.
  for (int i = 0; i < NUM_TREND_TIPS; i++) {
    TrendData x = {0};
    x.tip = (int8_t)i;
    x.streak = 5;
    x.best_preset = PRESET_BOX;
    const char *s = insights_trend_text(&x, buf, sizeof(buf));
    CHECK(s && s[0], "trend tip %d renders", i);
    CHECK(strchr(buf, '%') == NULL, "trend tip %d has no leftover format specifier", i);
  }
}

// --- i18n table integrity ---------------------------------------------------

static void test_i18n(void) {
  printf("i18n\n");
  i18n_init();
  for (int i = 0; i < NUM_STRS; i++) {
    CHECK(S(i) != NULL, "string %d is present in the active language", i);
    if (S(i)) CHECK(S(i)[0] != '\0', "string %d is not empty", i);
  }
  for (int b = 0; b < NUM_BADGES; b++) {
    CHECK(badge_name(b)[0], "badge %d has a name", b);
    CHECK(badge_desc(b)[0], "badge %d has a condition", b);
  }
  CHECK(badge_name(NUM_BADGES)[0] == '\0', "out-of-range badge name is empty, not a crash");
}

int main(void) {
  test_day_diff();
  test_score();
  test_session_tips();
  test_streak();
  test_badges();
  test_history();
  test_settings();
  test_trend();
  test_i18n();

  if (g_fail) {
    printf("\n%d check(s) FAILED\n", g_fail);
    return 1;
  }
  printf("\nall checks passed\n");
  return 0;
}
