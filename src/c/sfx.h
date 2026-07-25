#pragma once
#include "config.h"

// Cues for the breathing phases and the result screen. Every call is gated by
// the Sound / Vibration settings, so Sound=Off runs completely silent (which is
// also what keeps the emulator alive — QEMU has no speaker and any playback
// exits the app).

// A small chime when the app opens. Silent unless Sound is on.
void cue_app_open(void);

// The phase cues are vibration only: see sfx.c for why the tones went away.
void cue_phase_in(void);
void cue_phase_hold(void);
void cue_phase_out(void);

// Vibration marking the end of a session. The matching chime is
// cue_result_chime(), played by the result screen when it appears — the session
// itself ends inside a timer callback, which is the one place the speaker must
// not be touched.
void cue_session_done(void);
void cue_result_chime(void);
void cue_badge(void);
void cue_confirm(void);
void cue_deny(void);

// A sample blip at the current volume, for the Sound row in Settings.
void cue_sample(void);
