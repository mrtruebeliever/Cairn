#include "sfx.h"
#include "settings.h"

#define SFX_N(a) (sizeof(a) / sizeof((a)[0]))
#define SI SpeakerWaveformSine

// Cues are PCM samples where it matters -- the breathing phases and the singing
// bowl (see below) -- and short sine phrases for the small stuff. A note has no envelope
// -- it starts and stops at full level -- so softness is made the way a synth
// without envelopes makes it: the same note repeated in segments whose velocity
// ramps up and back down, which turns a flat beep into a swell.
//
// The pitches sit between A4 (440 Hz) and A5. Lower sounds calmer on paper, but
// a watch speaker barely moves air below ~400 Hz — an earlier attempt at
// 175-300 Hz was inaudible on the wrist.
//
// MIDI: 60 = C4, 69 = A4 (440 Hz). Velocity is a per-note volume (0-127);
// velocity 0 would mean "use the volume passed to speaker_play_notes", but
// these tables shape their own envelope, so play() scales the velocities by the
// Sound level instead.

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

// The level a cue plays at right now, or 0 when it must stay silent.
static uint8_t sound_level(void) {
#ifdef CAIRN_MUTE
  // Emulator builds (CAIRN_MUTE=1 pebble build): QEMU has no speaker and the
  // app exits on the first note, which since Sound defaults to Soft would mean
  // it exits the moment it opens. Nothing else changes.
  (void)SND_VOL;
  return 0;
#else
  // Quiet Time is the watch saying "not now". A breathing coach is exactly the
  // kind of app someone opens in a quiet room, so the cues go haptic-only for
  // the duration without touching the Sound setting.
  if (quiet_time_is_active()) return 0;
  // Muted system-wide (Settings → Sounds & Haptics): the firmware would drop
  // the notes anyway; don't hand it any.
  if (speaker_is_muted()) return 0;
  return SND_VOL[g_set.sound_vol & 3];
#endif
}

static void play(const SpeakerNote *notes, uint32_t count) {
  uint8_t level = sound_level();
  if (!level) return;
  if (count > MAX_CUE_NOTES) count = MAX_CUE_NOTES;

  // Nothing else is touched: no speaker_get_status, no speaker_stop, no
  // streaming, and cues are short enough not to overlap.
  SpeakerNote scaled[MAX_CUE_NOTES];
  for (uint32_t i = 0; i < count; i++) {
    scaled[i] = notes[i];
    uint16_t v = (uint16_t)notes[i].velocity * level / 100;
    scaled[i].velocity = (uint8_t)(v ? v : 1);   // 0 would mean "global volume"
  }
  speaker_play_notes(scaled, count, level);
}

// Phase haptics differ per phase so the pattern is recognizable eyes-closed:
// a short tap to breathe in, two light ticks to hold, a long pull to breathe
// out. They are the main cue now that the breath sounds are off by default, so
// in and out are kept far apart in length -- 60 ms against 140 ms was too close
// to tell apart on the wrist.
static void vibe(const uint32_t *segments, uint32_t count) {
  if (!g_set.haptics_on) return;
  VibePattern pat = {.durations = segments, .num_segments = count};
  vibes_enqueue_custom_pattern(pat);
}

static const uint32_t VIB_IN[]   = {45};
static const uint32_t VIB_HOLD[] = {25, 110, 25};
static const uint32_t VIB_OUT[]  = {300};
// In, then out: what turning Vibration on in Settings plays.
static const uint32_t VIB_SAMPLE[] = {45, 700, 300};
static const uint32_t VIB_DONE[] = {90, 120, 90, 120, 220};

// The breathing phases are samples from tools/gen_sfx.py rather than notes: a
// note cannot fade, so a short phase note lands as a click, and a long swell of
// notes is a lot to queue every few seconds. Each sample carries its own
// envelope and is played at its base pitch, so what the script generates is
// what you hear -- a warm tone gliding up over a faint breath of air (in), one
// soft bell (hold), the tone drawing back down (out).
//
// They play from the phase machine's timer callback. An earlier build of this
// app froze the watch and restarted PebbleOS doing that (streamed PCM, then a
// five-note phrase); firmware 4.40 fixed a reboot when the system task queue
// filled during speaker refill, and a one-note-per-phase run on a Time 2 then
// went clean (docs/phase-tone-test.md). PebbleDiver and PBL-7 play
// speaker_play_tracks() from their game timers on the same watch.
//
// The samples are read from flash once, when the session window opens, into a
// buffer each, and only played from RAM after that. Reloading one into a shared
// buffer at the phase change -- the first version -- blanked the screen for a
// moment on a Time 2 and the sound never came: a flash read under a playing
// speaker and a full-screen redraw is more than the watch does cleanly.
//
// The three together are about 25 KB, more than the Pebble 2 Duo (flint) can
// spare, so there they share one buffer sized for the longest and are loaded at
// the phase change after all; on the other platforms that path never runs.
//
// On a Time 2 each sound ends in a faint tick: the firmware's audio_stop() cuts
// the amplifier's power the moment the pipeline has drained, with no ramp
// (drivers/speaker/sf32lb52/audio.c in PebbleOS). Nothing in the sample
// changes that -- it already ends in silence.
//
// The firmware reads the sample while it plays, so a cue that comes while the
// previous one is still sounding -- custom patterns allow 0.5 s phases -- stays
// haptic rather than touching a buffer that is being played.
#define PHASE_BASE 72   // MIDI note that plays a sample unshifted
#define PCM_BYTES_PER_MS 16
#define HEAP_MARGIN 16384

enum { PS_IN, PS_HOLD, PS_OUT, PS_COUNT };
static const uint32_t PHASE_RES[PS_COUNT] = {
  RESOURCE_ID_SFX_BREATH_IN, RESOURCE_ID_SFX_BREATH_HOLD, RESOURCE_ID_SFX_BREATH_OUT,
};
static SpeakerSample s_phase_smp[PS_COUNT];
static bool s_phase_prepared;
static uint8_t *s_shared_buf;     // flint fallback: one buffer for all three
static uint32_t s_shared_cap;
static int s_shared_loaded = -1;  // phase in the shared buffer
static uint64_t s_phase_busy_until;

static uint64_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000 + ms;
}

static uint32_t load_into(uint8_t *buf, uint32_t cap, int phase) {
  return (uint32_t)resource_load(resource_get_handle(PHASE_RES[phase]), buf, cap);
}

void sfx_phase_prepare(void) {
  // Breath sounds off: keep the 25 KB for the screens.
  if (s_phase_prepared || !g_set.phase_sound) return;
  s_phase_prepared = true;
  uint32_t size[PS_COUNT], total = 0, cap = 0;
  for (int i = 0; i < PS_COUNT; i++) {
    size[i] = (uint32_t)resource_size(resource_get_handle(PHASE_RES[i]));
    total += size[i];
    if (size[i] > cap) cap = size[i];
    s_phase_smp[i] = (SpeakerSample){NULL, 0, SpeakerPcmFormat_16kHz_8bit, PHASE_BASE, false};
  }
  // All kept for the life of the app; skipped if it would leave the screens short.
  if (heap_bytes_free() >= total + HEAP_MARGIN) {
    for (int i = 0; i < PS_COUNT; i++) {
      uint8_t *buf = malloc(size[i]);
      if (!buf) continue;
      s_phase_smp[i].num_bytes = load_into(buf, size[i], i);
      s_phase_smp[i].data = buf;
    }
  } else if (heap_bytes_free() >= cap + HEAP_MARGIN / 4) {
    s_shared_buf = malloc(cap);
    s_shared_cap = s_shared_buf ? cap : 0;
  }
}

static void play_phase(int phase) {
  if (!g_set.phase_sound) return;
  uint8_t level = sound_level();
  if (!level) return;
  uint64_t now = now_ms();
  if (now < s_phase_busy_until) return;
  sfx_phase_prepare();
  SpeakerSample *smp = &s_phase_smp[phase];
  if (!smp->data && s_shared_buf) {
    if (s_shared_loaded != phase) {
      uint32_t n = load_into(s_shared_buf, s_shared_cap, phase);
      s_shared_loaded = n ? phase : -1;
      for (int i = 0; i < PS_COUNT; i++) s_phase_smp[i].data = NULL;
      if (!n) return;
      smp->num_bytes = n;
    }
    smp->data = s_shared_buf;
  }
  if (!smp->data || !smp->num_bytes) return;
  uint16_t dur = (uint16_t)(smp->num_bytes / PCM_BYTES_PER_MS);
  SpeakerNote note = {
    .midi_note = PHASE_BASE,
    .waveform = SpeakerWaveformSine,   // ignored: the sample is the voice
    .duration_ms = dur,
    .velocity = (uint8_t)(127 * level / 100),
  };
  SpeakerTrack track = {.notes = &note, .num_notes = 1, .sample = smp};
  if (speaker_play_tracks(&track, 1, level)) s_phase_busy_until = now + dur + 50;
}

bool sfx_phase_sounding(void) {
  return s_phase_busy_until && now_ms() < s_phase_busy_until;
}

void cue_phase_in(void) {
  vibe(VIB_IN, SFX_N(VIB_IN));
  play_phase(PS_IN);
}
void cue_phase_hold(void) {
  vibe(VIB_HOLD, SFX_N(VIB_HOLD));
  play_phase(PS_HOLD);
}
void cue_phase_out(void) {
  vibe(VIB_OUT, SFX_N(VIB_OUT));
  play_phase(PS_OUT);
}

// ---------------------------------------------------------------------------
// The singing bowl: one generated sample (tools/gen_sfx.py), struck at a few
// pitches for the moments that frame a session -- opening the app, the result,
// a badge, the daily reminder. A bowl rings out slowly, which is the sound a
// breathing app should end on; the sine phrases it replaces (still below as the
// fallback) were bright little chimes.
//
// Loaded on first use and kept, like the phase samples. 14 KB is more than the
// Pebble 2 Duo (flint) can spare next to them, so there the phrases play.
// ---------------------------------------------------------------------------
#define BOWL_BASE 69    // A4: the note that plays the sample as recorded (900 ms)

static SpeakerSample s_bowl = {NULL, 0, SpeakerPcmFormat_16kHz_8bit, BOWL_BASE, false};
static bool s_bowl_tried;

static bool bowl_ready(void) {
  if (!s_bowl_tried) {
    s_bowl_tried = true;
    ResHandle h = resource_get_handle(RESOURCE_ID_SFX_BOWL);
    size_t size = resource_size(h);
    if (heap_bytes_free() >= size + HEAP_MARGIN) {
      uint8_t *buf = malloc(size);
      if (buf) {
        s_bowl.num_bytes = (uint32_t)resource_load(h, buf, size);
        s_bowl.data = buf;
      }
    }
  }
  return s_bowl.data && s_bowl.num_bytes;
}

static void scale_notes(SpeakerNote *dst, const SpeakerNote *src, uint32_t n, uint8_t level) {
  for (uint32_t i = 0; i < n; i++) {
    dst[i] = src[i];
    if (!src[i].velocity) continue;   // a rest
    uint16_t v = (uint16_t)src[i].velocity * level / 100;
    dst[i].velocity = (uint8_t)(v ? v : 1);
  }
}

// Strikes the bowl (`strikes`, played on the sample) with an optional sine
// voice over it. Returns false when there is no bowl to strike, so the caller
// can fall back to its phrase; true also when the cue is meant to be silent.
static bool play_bowl(const SpeakerNote *strikes, uint32_t ns,
                      const SpeakerNote *voice, uint32_t nv) {
  uint8_t level = sound_level();
  if (!level) return true;
  if (!bowl_ready()) return false;
  if (ns > MAX_CUE_NOTES) ns = MAX_CUE_NOTES;
  if (nv > MAX_CUE_NOTES) nv = MAX_CUE_NOTES;
  SpeakerNote a[MAX_CUE_NOTES], b[MAX_CUE_NOTES];
  scale_notes(a, strikes, ns, level);
  scale_notes(b, voice, nv, level);
  SpeakerTrack tracks[2] = {
    {.notes = a, .num_notes = ns, .sample = &s_bowl},
    {.notes = b, .num_notes = nv, .sample = NULL},
  };
  speaker_play_tracks(tracks, nv ? 2 : 1, level);
  return true;
}

// A note on the bowl lasts until the sample runs out, which is shorter the
// higher it is struck: 900 ms at A4, so ~757 ms at C5 and ~1010 ms at G4.
#define REST(ms) {0, SI, (ms), 0, 0}
// Opening: one soft strike.
static const SpeakerNote BOWL_OPEN[] = {{72, SI, 757, 100, 0}};
// A finished session: a lower, longer strike, with a faint high shimmer
// coming in as it rings.
static const SpeakerNote BOWL_DONE[] = {{67, SI, 1010, 110, 0}};
static const SpeakerNote BOWL_DONE_SHIMMER[] = {
  REST(220), {88, SI, 160, 26, 0}, {88, SI, 220, 12, 0},
  REST(60), {91, SI, 160, 18, 0}, {91, SI, 200, 8, 0},
};
// A badge: struck higher, with a small sparkle on top.
static const SpeakerNote BOWL_BADGE[] = {{76, SI, 600, 110, 0}};
static const SpeakerNote BOWL_BADGE_SPARKLE[] = {
  REST(90), {95, SI, 70, 30, 0}, {100, SI, 90, 24, 0}, {100, SI, 160, 10, 0},
};
// The daily reminder: two strikes, the second lower and left to ring.
static const SpeakerNote BOWL_REMIND[] = {{72, SI, 520, 100, 0}, {67, SI, 1010, 110, 0}};

// No vibration on open — the watch was just woken by hand.
void cue_app_open(void) {
  if (!play_bowl(BOWL_OPEN, SFX_N(BOWL_OPEN), NULL, 0)) play(SFX_OPEN, SFX_N(SFX_OPEN));
}

// Haptic only; the chime that closes a session is played by the result screen
// as it appears.
void cue_session_done(void) { vibe(VIB_DONE, SFX_N(VIB_DONE)); }

// The speaker turns away a new cue while one is still sounding, and the result
// screen can appear while the last phase sound is ringing out. So the bowl
// waits for the speaker, briefly, rather than being dropped.
#define CHIME_POLL_MS 100
#define CHIME_MAX_TRIES 15

static void result_chime_try(void *data) {
  int tries = (int)(intptr_t)data;
  if (speaker_get_status() != SpeakerStatusIdle && tries < CHIME_MAX_TRIES) {
    app_timer_register(CHIME_POLL_MS, result_chime_try, (void *)(intptr_t)(tries + 1));
    return;
  }
  if (!play_bowl(BOWL_DONE, SFX_N(BOWL_DONE), BOWL_DONE_SHIMMER, SFX_N(BOWL_DONE_SHIMMER))) {
    play(SFX_DONE, SFX_N(SFX_DONE));
  }
}

void cue_result_chime(void) { result_chime_try((void *)(intptr_t)0); }

// Shown on the result screen too, so it waits for the speaker the same way.
static void badge_try(void *data) {
  int tries = (int)(intptr_t)data;
  if (speaker_get_status() != SpeakerStatusIdle && tries < CHIME_MAX_TRIES) {
    app_timer_register(CHIME_POLL_MS, badge_try, (void *)(intptr_t)(tries + 1));
    return;
  }
  if (!play_bowl(BOWL_BADGE, SFX_N(BOWL_BADGE), BOWL_BADGE_SPARKLE, SFX_N(BOWL_BADGE_SPARKLE))) {
    play(SFX_BADGE, SFX_N(SFX_BADGE));
  }
}

void cue_badge(void) {
  if (g_set.haptics_on) vibes_double_pulse();
  badge_try((void *)(intptr_t)0);
}

// The reminder always vibrates -- that is the reminder -- whatever the phase
// vibration setting says; the bowl follows Sound as every cue does.
void cue_reminder(void) {
  static const uint32_t VIB_REMIND[] = {120, 220, 120};
  VibePattern pat = {.durations = VIB_REMIND, .num_segments = SFX_N(VIB_REMIND)};
  vibes_enqueue_custom_pattern(pat);
  if (!play_bowl(BOWL_REMIND, SFX_N(BOWL_REMIND), NULL, 0)) play(SFX_OPEN, SFX_N(SFX_OPEN));
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

void cue_phase_sample(void) { vibe(VIB_SAMPLE, SFX_N(VIB_SAMPLE)); }
