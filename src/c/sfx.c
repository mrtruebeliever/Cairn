#include "sfx.h"
#include "settings.h"

#define SFX_N(a) (sizeof(a) / sizeof((a)[0]))
#define SI SpeakerWaveformSine
#define TRI SpeakerWaveformTriangle

// Soft sine/triangle swells rather than chiptune blips — this is a calming app.
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

static void play(const SpeakerNote *n, uint32_t count) {
  if (g_set.sound_vol) speaker_play_notes(n, count, sv(BASE_VOL));
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

void cue_phase_in(void)   { vibe(VIB_IN,   SFX_N(VIB_IN));   play(SFX_IN,   SFX_N(SFX_IN)); }
void cue_phase_hold(void) { vibe(VIB_HOLD, SFX_N(VIB_HOLD)); play(SFX_HOLD, SFX_N(SFX_HOLD)); }
void cue_phase_out(void)  { vibe(VIB_OUT,  SFX_N(VIB_OUT));  play(SFX_OUT,  SFX_N(SFX_OUT)); }

void cue_session_done(void) {
  vibe(VIB_DONE, SFX_N(VIB_DONE));
  play(SFX_DONE, SFX_N(SFX_DONE));
}

void cue_badge(void) {
  if (g_set.haptics_on) vibes_double_pulse();
  play(SFX_BADGE, SFX_N(SFX_BADGE));
}

void cue_confirm(void) {
  if (g_set.haptics_on) vibes_short_pulse();
  play(SFX_OK, SFX_N(SFX_OK));
}

void cue_deny(void) {
  if (g_set.haptics_on) vibes_double_pulse();
  play(SFX_NO, SFX_N(SFX_NO));
}

void cue_sample(void) { play(SFX_OK, SFX_N(SFX_OK)); }
