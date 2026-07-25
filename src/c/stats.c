#include "stats.h"
#include "i18n.h"
#include <string.h>

Stats g_stats;

// History lives in RAM and is flushed to one persist blob. Oldest first, so a
// new session appends at the end. 20 records x 12 bytes = 240 B, just inside
// the 256-byte persist value limit — raising HISTORY_MAX needs a second key.
static SessionRecord s_hist[HISTORY_MAX];
static int s_hist_count;

static uint32_t ymd_from_time(time_t t) {
  struct tm *tm = localtime(&t);
  return (uint32_t)(tm->tm_year + 1900) * 10000u
       + (uint32_t)(tm->tm_mon + 1) * 100u
       + (uint32_t)tm->tm_mday;
}

uint32_t stats_today(void) { return ymd_from_time(time(NULL)); }

static void history_load(void) {
  s_hist_count = 0;
  memset(s_hist, 0, sizeof(s_hist));
  if (!persist_exists(PERSIST_HISTORY)) return;
  int bytes = persist_read_data(PERSIST_HISTORY, s_hist, sizeof(s_hist));
  if (bytes <= 0) return;
  int stored = bytes / (int)sizeof(SessionRecord);
  int count = persist_exists(PERSIST_HISTORY_COUNT)
                ? persist_read_int(PERSIST_HISTORY_COUNT) : stored;
  if (count > stored) count = stored;
  if (count < 0) count = 0;
  s_hist_count = count;
}

static void history_save(void) {
  persist_write_data(PERSIST_HISTORY, s_hist, sizeof(SessionRecord) * s_hist_count);
  persist_write_int(PERSIST_HISTORY_COUNT, s_hist_count);
}

static void history_append(const SessionRecord *rec) {
  if (s_hist_count == HISTORY_MAX) {
    memmove(&s_hist[0], &s_hist[1], sizeof(SessionRecord) * (HISTORY_MAX - 1));
    s_hist_count--;
  }
  s_hist[s_hist_count++] = *rec;
  history_save();
}

void stats_load(void) {
  g_stats.streak         = persist_read_int(PERSIST_STREAK);
  g_stats.last_date      = persist_read_int(PERSIST_LAST_DATE);
  g_stats.best_score     = persist_read_int(PERSIST_BEST_SCORE);
  g_stats.total_sessions = persist_read_int(PERSIST_TOTAL_SESSIONS);
  g_stats.total_minutes  = persist_read_int(PERSIST_TOTAL_MINUTES);
  g_stats.cairn_size     = persist_read_int(PERSIST_CAIRN_SIZE);
  g_stats.badges         = (uint32_t)persist_read_int(PERSIST_BADGES);

  // A streak that skipped more than a day is already dead when we start up, so
  // Home shows the truth instead of a stale number.
  if (g_stats.last_date) {
    time_t now = time(NULL);
    uint32_t today = ymd_from_time(now);
    uint32_t yesterday = ymd_from_time(now - 86400);
    if ((uint32_t)g_stats.last_date != today && (uint32_t)g_stats.last_date != yesterday) {
      g_stats.streak = 0;
    }
  }
  history_load();
}

static void stats_save(void) {
  persist_write_int(PERSIST_STREAK, g_stats.streak);
  persist_write_int(PERSIST_LAST_DATE, g_stats.last_date);
  persist_write_int(PERSIST_BEST_SCORE, g_stats.best_score);
  persist_write_int(PERSIST_TOTAL_SESSIONS, g_stats.total_sessions);
  persist_write_int(PERSIST_TOTAL_MINUTES, g_stats.total_minutes);
  persist_write_int(PERSIST_CAIRN_SIZE, g_stats.cairn_size);
  persist_write_int(PERSIST_BADGES, (int32_t)g_stats.badges);
}

void stats_reset(void) {
  memset(&g_stats, 0, sizeof(g_stats));
  s_hist_count = 0;
  memset(s_hist, 0, sizeof(s_hist));
  stats_save();
  history_save();
}

// Stones grow with the streak and get two bonus tiers from the lifetime total,
// so a long-time user keeps a visible cairn even after a missed day.
int stats_cairn_stones(void) {
  int stones = g_stats.streak;
  if (stones > 7) stones = 7;
  if (g_stats.total_sessions >= 30) stones++;
  if (g_stats.total_sessions >= 100) stones++;
  if (stones < 0) stones = 0;
  return stones;
}

uint32_t stats_record(const SessionRecord *rec, bool *out_new_best) {
  time_t now = time(NULL);
  uint32_t today = ymd_from_time(now);
  uint32_t yesterday = ymd_from_time(now - 86400);

  if ((uint32_t)g_stats.last_date == today) {
    // Already counted today; a second session doesn't extend the streak.
  } else if ((uint32_t)g_stats.last_date == yesterday) {
    g_stats.streak++;
  } else {
    g_stats.streak = 1;
  }
  g_stats.last_date = (int32_t)today;

  g_stats.total_sessions++;
  g_stats.total_minutes += (rec->dur_s + 30) / 60;   // round to the nearest minute

  bool new_best = rec->score > g_stats.best_score;
  if (new_best) g_stats.best_score = rec->score;
  if (out_new_best) *out_new_best = new_best;

  g_stats.cairn_size = stats_cairn_stones();

  const bool earned[NUM_BADGES] = {
    [BADGE_FIRST]   = g_stats.total_sessions >= 1,
    [BADGE_STEADY]  = g_stats.streak >= 3,
    [BADGE_ZENWEEK] = g_stats.streak >= 7,
    // Deep calm needs a measured session: without a heart rate the score is
    // capped at 90, so an unmeasured full session would otherwise earn it.
    [BADGE_DEEP]    = rec->score >= 90 && rec->hr_start != 0,
    [BADGE_CENTURY] = g_stats.total_sessions >= 100,
    [BADGE_TIDE]    = g_stats.total_minutes >= 1000,
  };
  uint32_t fresh = 0;
  for (int b = 0; b < NUM_BADGES; b++) {
    uint32_t bit = 1u << b;
    if (earned[b] && !(g_stats.badges & bit)) fresh |= bit;
  }
  g_stats.badges |= fresh;

  stats_save();
  history_append(rec);
  return fresh;
}

int stats_history_count(void) { return s_hist_count; }

bool stats_history_get(int idx, SessionRecord *out) {
  if (idx < 0 || idx >= s_hist_count || !out) return false;
  *out = s_hist[s_hist_count - 1 - idx];   // 0 = most recent
  return true;
}

const char *badge_name(int badge) {
  static const int ids[NUM_BADGES] = {
    S_BADGE_FIRST, S_BADGE_STEADY, S_BADGE_ZENWEEK,
    S_BADGE_DEEP, S_BADGE_CENTURY, S_BADGE_TIDE,
  };
  if (badge < 0 || badge >= NUM_BADGES) return "";
  return S(ids[badge]);
}

const char *badge_desc(int badge) {
  static const int ids[NUM_BADGES] = {
    S_BADGE_FIRST_D, S_BADGE_STEADY_D, S_BADGE_ZENWEEK_D,
    S_BADGE_DEEP_D, S_BADGE_CENTURY_D, S_BADGE_TIDE_D,
  };
  if (badge < 0 || badge >= NUM_BADGES) return "";
  return S(ids[badge]);
}
