#pragma once
#include <pebble.h>

// A daily nudge to breathe, at the time set in Settings (Off by default). It is
// a wakeup: the watch opens Cairn at that time, which vibrates and strikes the
// bowl -- unless a session was already done today or Quiet Time is on, in which
// case the app closes again without a sound.

// Replaces any scheduled reminder with the next one for the current setting.
void reminder_schedule(void);

// True when this launch is a reminder that should stay silent and close at
// once (call before pushing any window; it reschedules either way).
bool reminder_should_skip(void);

// True when the app was opened by the reminder.
bool reminder_launched(void);
