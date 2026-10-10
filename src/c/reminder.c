#include "reminder.h"
#include "settings.h"
#include "stats.h"

// The next local occurrence of `minutes` after midnight, strictly after `now`.
// Built through mktime so a DST switch moves the clock time, not the reminder.
static time_t next_occurrence(int minutes, time_t now) {
  struct tm tm = *localtime(&now);
  tm.tm_hour = minutes / 60;
  tm.tm_min = minutes % 60;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  time_t at = mktime(&tm);
  if (at <= now + 60) {
    tm.tm_mday += 1;
    tm.tm_isdst = -1;
    at = mktime(&tm);
  }
  return at;
}

void reminder_schedule(void) {
  wakeup_cancel_all();
  if (g_set.reminder == REMINDER_OFF) return;
  time_t at = next_occurrence(g_set.reminder, time(NULL));
  WakeupId id = wakeup_schedule(at, 0, false);
  // E_RANGE: another app's wakeup already holds that minute. A couple of
  // minutes later is just as good for a once-a-day nudge.
  for (int i = 1; id == E_RANGE && i <= 5; i++) {
    id = wakeup_schedule(at + i * 120, 0, false);
  }
  if (id < 0) APP_LOG(APP_LOG_LEVEL_WARNING, "cairn: reminder wakeup failed (%d)", (int)id);
}

bool reminder_launched(void) {
  return launch_reason() == APP_LAUNCH_WAKEUP;
}

bool reminder_should_skip(void) {
  if (!reminder_launched()) return false;
  reminder_schedule();   // tomorrow's, whatever happens next
  if (quiet_time_is_active()) return true;
  return g_stats.last_date != 0 && (uint32_t)g_stats.last_date == stats_today();
}
