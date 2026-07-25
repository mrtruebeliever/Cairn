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
#include "home_window.h"
#include "i18n.h"
#include "session.h"
#include "settings.h"
#include "sfx.h"
#include "sfx_synth.h"
#include "stats.h"

// Left to itself the watch samples the heart rate on its own schedule — minutes
// apart while the user sits still — so an app that polls every second just
// re-reads the same stale value. hr_sampling_apply() asks for 1 Hz the moment
// the app opens, which means the sensor has usually locked on by the time the
// user starts a session, and it is what makes the breath-by-breath swing (the
// RSA term of the score) measurable at all. The request outlives the app unless
// cancelled, so deinit hands the schedule back.

static void init(void) {
  i18n_init();
  settings_load();
  stats_load();
  comms_init();
  hr_sampling_apply();
  backlight_apply();
  home_window_push();
  cue_app_open();
}

static void deinit(void) {
  // Before anything else: the synth feeds the speaker from a timer, and that
  // timer must not outlive the app.
  synth_stop();
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
