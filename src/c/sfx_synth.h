#pragma once
#include "config.h"

// Generated audio for the cues, streamed as raw PCM.
//
// The note API (speaker_play_notes) restarts a waveform at full amplitude for
// every note, which is exactly the click that makes a short beep sound harsh —
// and it cannot make a wave break. Everything here is synthesised sample by
// sample instead, so each cue can start from silence, swell, and fade out, and
// so the app can make surf out of filtered noise. No audio resources: 8 kHz
// mono costs 8 KB per second of sound, which is more than the whole app.
//
// Playback is fed from a timer in ~32 ms chunks, so a cue never blocks the UI.
// Returns false when the speaker will not open (busy, muted, or an emulator
// with no speaker at all); callers fall back to the note tables in sfx.c.

typedef enum {
  SYNTH_OPEN,    // the wave that greets you when the app opens
  SYNTH_IN,      // inhale: a rising swell, the tide coming in
  SYNTH_HOLD,    // hold: one quiet tick
  SYNTH_OUT,     // exhale: a falling swell, the water drawing back
  SYNTH_DONE,    // session finished: a soft resolving chord over a last wash
  SYNTH_BADGE,   // badge unlocked: a small struck chime, three steps up
  SYNTH_DENY,    // refused: two low, dull ticks
} SynthCue;

bool synth_play(SynthCue cue, uint8_t volume);
void synth_stop(void);
