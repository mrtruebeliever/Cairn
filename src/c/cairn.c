// Cairn — a guided breathing coach with live heart rate for the Pebble Time 2.
//
// The SDK only exposes BPM (health_service_peek_current_value), never the
// beat-to-beat RR intervals clinical HRV needs, so the app computes a "calm
// score" from the heart-rate drop over a session plus how strongly the heart
// rate waves along with the breath (respiratory sinus arrhythmia). It is named
// a calm score in the UI, never HRV.
//
// The emulator has no heart-rate sensor, so every HR path degrades gracefully:
// the HR readouts disappear and the score falls back to completion only.

#include <pebble.h>
#include "backlight.h"
#include "comms.h"
#include "history_window.h"
#include "home_window.h"
#include "insights_window.h"
#include "result_window.h"
#include "touch_nav.h"
#include "i18n.h"
#include "session.h"
#include "settings.h"
#include "sfx.h"
#include "stats.h"

// Left to itself the watch samples the heart rate on its own schedule — minutes
// apart while the user sits still — so an app that polls every second just
// re-reads the same stale value. hr_sampling_apply() asks for 1 Hz the moment
// the app opens, which means the sensor has usually locked on by the time the
// user starts a session, and it is what makes the breath-by-breath swing (the
// RSA term of the score) measurable at all. The request outlives the app unless
// cancelled, so deinit hands the schedule back.

// One app-wide subscription powers the touch sensor; events go to the window on
// top. The system navigation bridge is not usable — see the note in init(). The
// session screen is deliberately absent: a stray touch mid-exercise should not
// interrupt the breathing.
static void touch_dispatch(const TouchEvent *event, void *context) {
  if (settings_window_is_top())      { settings_window_handle_touch(event); }
  else if (history_window_is_top())  { history_window_handle_touch(event); }
  else if (insights_window_is_top()) { insights_window_handle_touch(event); }
  else if (result_window_is_top())   { result_window_handle_touch(event); }
  else if (home_window_is_top())     { home_window_handle_touch(event); }
}

static void init(void) {
  // No app_touch_navigation_enable() here: on firmware 4.33.1 the system touch
  // bridge faults inside firmware on the first touch and kills the app. Buttons
  // only until a later firmware fixes it.
  i18n_init();
  settings_load();
  stats_load();
  comms_init();
  hr_sampling_apply();
  backlight_apply();
  home_window_push();
  touch_service_subscribe(touch_dispatch, NULL);
  cue_app_open();
}

static void deinit(void) {
  health_service_events_unsubscribe();
  health_service_set_heart_rate_sample_period(0);
  backlight_release();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
