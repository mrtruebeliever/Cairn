#include "sfx.h"
#include "settings.h"
#include "sfx_synth.h"

#define SFX_N(a) (sizeof(a) / sizeof((a)[0]))
#define SI SpeakerWaveformSine
#define TRI SpeakerWaveformTriangle

// Fallback cues, used only when the PCM stream will not open (see sfx_synth.c,
// which is what actually plays on the watch). Notes restart the waveform at
// full amplitude, so these click a little on the way in — unavoidable here, and
// the reason the generated versions exist.
// MIDI notes: 60 = C4. Velocity 0 means "use the call's global volume".
static const SpeakerNote SFX_IN[]   = {{69, SI, 110, 0, 0}, {74, SI, 190, 0, 0}};   // A4 -> D5, rising
static const SpeakerNote SFX_OUT[]  = {{74, SI, 110, 0, 0}, {67, SI, 220, 0, 0}};   // D5 -> G4, falling
static const SpeakerNote SFX_HOLD[] = {{71, TRI, 70, 0, 0}};                        // single soft tick
static const SpeakerNote SFX_DONE[] = {{67, SI, 130, 0, 0}, {71, SI, 130, 0, 0},
                                       {74, SI, 130, 0, 0}, {79, SI, 320, 0, 0}};   // gentle resolve
static const SpeakerNote SFX_BADGE[]= {{76, TRI, 90, 0, 0}, {83, TRI, 90, 0, 0},
                                       {88, TRI, 200, 0, 0}};
static const SpeakerNote SFX_OK[]   = {{72, TRI, 60, 0, 0}, {79, TRI, 120, 0, 0}};
static const SpeakerNote SFX_NO[]   = {{60, TRI, 80, 0, 0}, {55, TRI, 140, 0, 0}};

// Master volume per Sound level (Off/Soft/Normal/Loud). Each cue keeps its own
// tuned loudness; sv() scales it, so Loud == the original tuning.
#define BASE_VOL 70
static const uint8_t SND_VOL[4] = {0, 35, 70, 100};
static uint8_t sv(uint8_t base) {
  return (uint16_t)base * SND_VOL[g_set.sound_vol & 3] / 100;
}

// Every cue asks the synth first and only drops to the note table if the stream
// could not be opened, so a watch with a busy or absent speaker still gets
// something (and Sound=Off stays completely silent either way).
static void play(SynthCue cue, const SpeakerNote *n, uint32_t count) {
  if (!g_set.sound_vol) return;
  if (synth_play(cue, sv(BASE_VOL))) return;
  speaker_play_notes(n, count, sv(BASE_VOL));
}

// Phase haptics differ per phase so the pattern is recognizable eyes-closed:
// one nudge to breathe in, a double tick to hold, a longer pull to breathe out.
static void vibe(const uint32_t *segments, uint32_t count) {
  if (!g_set.haptics_on) return;
  VibePattern pat = {.durations = segments, .num_segments = count};
  vibes_enqueue_custom_pattern(pat);
}

static const uint32_t VIB_IN[]   = {60};
static const uint32_t VIB_HOLD[] = {25, 90, 25};
static const uint32_t VIB_OUT[]  = {140};
static const uint32_t VIB_DONE[] = {90, 120, 90, 120, 220};

void cue_phase_in(void)   { vibe(VIB_IN,   SFX_N(VIB_IN));   play(SYNTH_IN,   SFX_IN,   SFX_N(SFX_IN)); }
void cue_phase_hold(void) { vibe(VIB_HOLD, SFX_N(VIB_HOLD)); play(SYNTH_HOLD, SFX_HOLD, SFX_N(SFX_HOLD)); }
void cue_phase_out(void)  { vibe(VIB_OUT,  SFX_N(VIB_OUT));  play(SYNTH_OUT,  SFX_OUT,  SFX_N(SFX_OUT)); }

// One wave breaking as the app opens — the same sound the exhale cue is made
// of, given room to finish. No vibration: the watch was just woken by hand.
void cue_app_open(void) { play(SYNTH_OPEN, SFX_DONE, SFX_N(SFX_DONE)); }

void cue_session_done(void) {
  vibe(VIB_DONE, SFX_N(VIB_DONE));
  play(SYNTH_DONE, SFX_DONE, SFX_N(SFX_DONE));
}

void cue_badge(void) {
  if (g_set.haptics_on) vibes_double_pulse();
  play(SYNTH_BADGE, SFX_BADGE, SFX_N(SFX_BADGE));
}

void cue_confirm(void) {
  if (g_set.haptics_on) vibes_short_pulse();
  play(SYNTH_HOLD, SFX_OK, SFX_N(SFX_OK));
}

void cue_deny(void) {
  if (g_set.haptics_on) vibes_double_pulse();
  play(SYNTH_DENY, SFX_NO, SFX_N(SFX_NO));
}

void cue_sample(void) { play(SYNTH_IN, SFX_OK, SFX_N(SFX_OK)); }
