#pragma once
#include "config.h"

// Cues for the breathing phases and the result screen. Every call is gated by
// the Sound / Vibration settings, so Sound=Off runs completely silent (which is
// also what keeps the emulator alive — QEMU has no speaker and any playback
// exits the app).

// A singing-bowl strike when the app opens. Silent unless Sound is on.
void cue_app_open(void);

// The daily breathing reminder: a vibration and two bowl strikes.
void cue_reminder(void);

// A phase change: a vibration and a short sound, each behind its own setting
// (the sound behind Breath sound, off by default, as well as Sound).
void cue_phase_in(void);
void cue_phase_hold(void);
void cue_phase_out(void);

// Loads the phase sounds into memory. Called when the session window opens, so
// nothing is read from flash once the breathing has started.
void sfx_phase_prepare(void);

// True while a phase sound is still playing. The firmware refills the speaker
// as it goes, and a session redrawing the whole screen every frame and
// stepping the LED colour starves it: the sound stutters. The session eases off
// for these half-seconds.
bool sfx_phase_sounding(void);

// Vibration marking the end of a session. The matching chime is
// cue_result_chime(), played by the result screen when it appears, so it sounds
// with the result rather than under the last breathing cue.
void cue_session_done(void);
void cue_result_chime(void);
void cue_badge(void);
void cue_confirm(void);
void cue_deny(void);

// A sample blip at the current volume, for the Sound row in Settings.
void cue_sample(void);

// The inhale and exhale vibrations back to back, for the Vibration row.
void cue_phase_sample(void);
