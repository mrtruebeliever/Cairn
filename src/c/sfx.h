#pragma once
#include "config.h"

// Cues for the breathing phases and the result screen. Every call is gated by
// the Sound / Vibration settings, so Sound=Off runs completely silent (which is
// also what keeps the emulator alive — QEMU has no speaker and any playback
// exits the app).

// A wave breaking as the app opens. Silent unless Sound is on.
void cue_app_open(void);

void cue_phase_in(void);
void cue_phase_hold(void);
void cue_phase_out(void);
void cue_session_done(void);
void cue_badge(void);
void cue_confirm(void);
void cue_deny(void);

// A sample blip at the current volume, for the Sound row in Settings.
void cue_sample(void);
