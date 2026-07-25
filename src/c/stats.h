#pragma once
#include "config.h"

typedef struct {
  int32_t  streak;
  int32_t  last_date;       // yyyymmdd of the last completed session, 0 = never
  int32_t  best_score;
  int32_t  total_sessions;
  int32_t  total_minutes;
  int32_t  cairn_size;      // stones earned; derived from streak and totals
  uint32_t badges;          // bitmask over Badge
} Stats;

extern Stats g_stats;

void stats_load(void);
void stats_reset(void);

// Folds a finished session into the stats and the on-watch history.
// Returns the bitmask of badges unlocked by *this* session (0 for none) and
// sets *out_new_best when the score beat the previous record.
uint32_t stats_record(const SessionRecord *rec, bool *out_new_best);

int  stats_history_count(void);
bool stats_history_get(int idx, SessionRecord *out);   // idx 0 = most recent

const char *badge_name(int badge);
const char *badge_desc(int badge);

uint32_t stats_today(void);       // today as yyyymmdd
int stats_cairn_stones(void);     // how many stones the cairn currently shows
