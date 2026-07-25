#include "sfx.h"
#include "settings.h"

#define SFX_N(a) (sizeof(a) / sizeof((a)[0]))
#define SI SpeakerWaveformSine

// Cues are short sine phrases, played with speaker_play_notes.
//
// An earlier version synthesised them as streamed PCM to get click-free
// envelopes and actual surf. On the watch that froze the app and restarted
// PebbleOS: a cue opened and closed a stream every breathing phase, and the
// firmware does not survive that (Polly, which streams speech, keeps exactly
// one stream open per phrase). Notes are the safe API here, so softness is made
// the way a synth without envelopes makes it: the same note repeated in
// segments whose velocity ramps up and back down, which turns a flat beep into
// a swell.
//
// The pitches sit between A4 (440 Hz) and A5. Lower sounds calmer on paper, but
// a watch speaker barely moves air below ~400 Hz — an earlier attempt at
// 175-300 Hz was inaudible on the wrist.
//
// MIDI: 60 = C4, 69 = A4 (440 Hz). Velocity is a per-note volume (0-127);
// velocity 0 would mean "use the volume passed to speaker_play_notes", but
// these tables shape their own envelope, so play() scales the velocities by the
// Sound level instead.

// Rising swell, the water coming in: A4 fading up, D5 fading away.
static const SpeakerNote SFX_IN[] = {
  {69, SI, 70, 30, 0}, {69, SI, 70, 60, 0}, {74, SI, 90, 85, 0},
  {74, SI, 100, 55, 0}, {74, SI, 120, 25, 0},
};
// Falling swell, the water drawing back: a step lower and a longer tail.
static const SpeakerNote SFX_OUT[] = {
  {74, SI, 70, 35, 0}, {74, SI, 70, 70, 0}, {69, SI, 90, 85, 0},
  {69, SI, 110, 50, 0}, {65, SI, 130, 22, 0},
};
// A held breath is marked, not announced.
static const SpeakerNote SFX_HOLD[] = {{72, SI, 70, 40, 0}, {72, SI, 70, 18, 0}};
// Opening the app: a small chime that rises and fades out.
static const SpeakerNote SFX_OPEN[] = {
  {69, SI, 100, 45, 0}, {74, SI, 100, 70, 0}, {81, SI, 140, 60, 0},
  {81, SI, 200, 25, 0},
};
// Session finished: a resolve that lands and rings out.
static const SpeakerNote SFX_DONE[] = {
  {67, SI, 110, 50, 0}, {71, SI, 110, 70, 0}, {74, SI, 120, 85, 0},
  {79, SI, 150, 65, 0}, {79, SI, 220, 25, 0},
};
static const SpeakerNote SFX_BADGE[] = {
  {76, SI, 90, 65, 0}, {83, SI, 90, 85, 0}, {88, SI, 140, 70, 0},
  {88, SI, 180, 28, 0},
};
// Short and unmistakable: this is also the sample the Sound row plays, so it
// has to be obvious at a glance which level you just picked.
static const SpeakerNote SFX_OK[] = {
  {72, SI, 70, 70, 0}, {79, SI, 110, 90, 0}, {79, SI, 90, 30, 0},
};
static const SpeakerNote SFX_NO[] = {
  {62, SI, 90, 70, 0}, {57, SI, 140, 60, 0}, {57, SI, 110, 22, 0},
};

// Master volume per Sound level (Off/Soft/Normal/Loud), as a percentage of each
// cue's own tuning — so Loud is the tables above, played as written.
static const uint8_t SND_VOL[4] = {0, 40, 70, 100};
#define MAX_CUE_NOTES 8

static void play(const SpeakerNote *notes, uint32_t count) {
#ifdef CAIRN_MUTE
  // Emulator builds (CAIRN_MUTE=1 pebble build): QEMU has no speaker and the
  // app exits on the first note, which since Sound defaults to Soft would mean
  // it exits the moment it opens. Nothing else changes.
  (void)notes; (void)count; (void)SND_VOL;
  return;
#else
  uint8_t level = SND_VOL[g_set.sound_vol & 3];
  if (!level) return;
  if (count > MAX_CUE_NOTES) count = MAX_CUE_NOTES;

  // Nothing else is touched: no speaker_get_status, no speaker_stop, no
  // streaming. One watch has already restarted PebbleOS on this app's audio, so
  // until sound is confirmed good on hardware this stays the single call that
  // Polly ships (parrot_anim.c), and cues are short enough not to overlap.
  SpeakerNote scaled[MAX_CUE_NOTES];
  for (uint32_t i = 0; i < count; i++) {
    scaled[i] = notes[i];
    uint16_t v = (uint16_t)notes[i].velocity * level / 100;
    scaled[i].velocity = (uint8_t)(v ? v : 1);   // 0 would mean "global volume"
  }
  speaker_play_notes(scaled, count, level);
#endif
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

// No vibration on open — the watch was just woken by hand.
void cue_app_open(void) { play(SFX_OPEN, SFX_N(SFX_OPEN)); }

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
