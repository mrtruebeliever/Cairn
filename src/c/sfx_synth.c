#include "sfx_synth.h"

// 8 kHz mono, 8-bit signed: the cheapest format the speaker takes, and enough
// for surf and tones under ~1 kHz. One second of sound is 8000 samples, all
// generated on the fly — nothing is stored.
#define SR          8000
#define CHUNK       256                 // samples generated per timer tick (32 ms)
#define TICK_MS     20                  // fed a little faster than it drains

typedef struct {
  SynthCue cue;
  uint32_t n;             // samples emitted so far
  uint32_t total;         // samples in this cue
  int32_t phase, phase2;  // tone oscillators, TRIG_MAX_ANGLE per cycle
  int32_t lp1, lp2;       // one-pole filter state for the surf
  uint32_t rng;
  bool open;
} Synth;

static Synth s;
static AppTimer *s_timer;
static int8_t s_buf[CHUNK];
static uint32_t s_pending;      // samples in s_buf the speaker has not taken yet
static uint32_t s_pending_off;

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------

// A window that rises to 1024 at `peak` and falls back to 0 at `total`, both
// halves shaped by a quarter turn of a sine so there is no corner to click on.
static int32_t env(uint32_t n, uint32_t peak, uint32_t total) {
  if (n >= total || peak == 0 || total <= peak) return 0;
  if (n < peak) {
    int32_t a = (int32_t)((uint64_t)(TRIG_MAX_ANGLE / 4) * n / peak);
    return sin_lookup(a) * 1024 / TRIG_MAX_RATIO;
  }
  int32_t a = (int32_t)((uint64_t)(TRIG_MAX_ANGLE / 4) * (n - peak) / (total - peak));
  return cos_lookup(a) * 1024 / TRIG_MAX_RATIO;
}

// One step of an oscillator at `hz`, as -1024..1024.
static int32_t osc(int32_t *phase, int32_t hz) {
  *phase = (*phase + (int32_t)((int64_t)hz * TRIG_MAX_ANGLE / SR)) & 0xFFFF;
  return sin_lookup(*phase) * 1024 / TRIG_MAX_RATIO;
}

// A pitch that slides from `from` to `to` across the cue — the audible shape of
// water rising or draining, rather than a beep at a fixed note.
static int32_t glide(int32_t from, int32_t to, uint32_t n, uint32_t total) {
  return from + (int32_t)((int64_t)(to - from) * n / total);
}

// Surf: white noise through two one-poles. A single pole still hisses; two roll
// the top off far enough that it reads as water instead of static.
static int32_t surf(void) {
  s.rng = s.rng * 1664525u + 1013904223u;
  int32_t white = (int32_t)((s.rng >> 16) & 0x7FF) - 1024;
  s.lp1 += (white - s.lp1) / 6;
  s.lp2 += (s.lp1 - s.lp2) / 6;
  // Two poles throw away roughly nine tenths of the level; put it back so a
  // wash and a tone written at the same scale are actually the same loudness.
  return s.lp2 * 5;
}

static int32_t clip(int32_t v) {
  if (v > 127) return 127;
  if (v < -127) return -127;
  return v;
}

// ---------------------------------------------------------------------------
// The cues
// ---------------------------------------------------------------------------

static uint32_t cue_len(SynthCue cue) {
  switch (cue) {
    case SYNTH_OPEN: return SR * 9 / 5;    // 1.8 s
    case SYNTH_IN:   return SR / 2;        // 0.5 s
    case SYNTH_HOLD: return SR / 5;        // 0.2 s
    case SYNTH_OUT:  return SR * 7 / 10;   // 0.7 s
    case SYNTH_BADGE: return SR;           // 1.0 s
    case SYNTH_DENY: return SR * 2 / 5;    // 0.4 s
    default:         return SR * 3 / 2;    // 1.5 s
  }
}

// A struck-bell envelope: a fast onset and a squared decay to nothing. The
// onset is a ramp rather than a jump — 12 ms is far too short to soften the
// strike, but long enough that the speaker is not asked to step from silence to
// full amplitude in one sample, which is heard as a tick.
#define BELL_ATTACK (SR / 80)

static int32_t bell(uint32_t n, uint32_t len) {
  if (n >= len) return 0;
  int32_t d = (int32_t)(1024 - 1024 * n / len);
  d = d * d / 1024;
  if (n < BELL_ATTACK) d = d * (int32_t)n / BELL_ATTACK;
  return d;
}

// All tones sit between 175 Hz and 400 Hz — an octave below where the old note
// cues were. On a small speaker that is the difference between a chirp and a
// hum, and it is the register that reads as calm.
static int32_t sample(void) {
  const uint32_t n = s.n, total = s.total;
  switch (s.cue) {
    case SYNTH_IN: {
      // Rising: the water coming up the shore, with the noise swelling under it.
      int32_t e = env(n, total * 2 / 5, total);
      int32_t tone = osc(&s.phase, glide(196, 262, n, total)) * 60 / 1024;
      int32_t wash = surf() * 22 / 1024;
      return clip((tone + wash) * e / 1024);
    }
    case SYNTH_OUT: {
      // Falling, and a touch longer: the water drawing back out.
      int32_t e = env(n, total / 3, total);
      int32_t tone = osc(&s.phase, glide(262, 175, n, total)) * 60 / 1024;
      int32_t wash = surf() * 26 / 1024;
      return clip((tone + wash) * e / 1024);
    }
    case SYNTH_HOLD: {
      // Barely there — a held breath should be marked, not announced.
      int32_t e = env(n, total / 3, total);
      return clip(osc(&s.phase, 233) * 40 / 1024 * e / 1024);
    }
    case SYNTH_OPEN: {
      // One wave breaking, with a soft chime riding the front of it. Two
      // partials a fifth apart, decaying like a struck bell rather than a note
      // that simply stops.
      int32_t wash = surf() * 70 / 1024 * env(n, total / 4, total) / 1024;
      int32_t chime = (osc(&s.phase, 294) * 34 + osc(&s.phase2, 441) * 18) / 1024
                    * bell(n, SR) / 1024;
      return clip(wash + chime);
    }
    case SYNTH_BADGE: {
      // Three struck steps up, each ringing into the next. The last step owns
      // the remainder of the cue, so its decay reaches silence before the end
      // instead of being cut off mid-ring.
      uint32_t step = total / 3;
      uint32_t seg = n / step;
      if (seg > 2) seg = 2;
      uint32_t at = n - seg * step;
      uint32_t len = (seg == 2) ? total - 2 * step : step;
      int32_t hz = (seg == 0) ? 294 : (seg == 1) ? 392 : 494;
      return clip(osc(&s.phase, hz) * 46 / 1024 * bell(at, len) / 1024);
    }
    case SYNTH_DENY: {
      // Two dull taps, low enough that they read as "no" without a buzz. Each
      // tap rings for two thirds of its slot, so there is a gap between them.
      uint32_t half = total / 2;
      uint32_t seg = n / half;
      if (seg > 1) seg = 1;
      return clip(osc(&s.phase, 147) * 44 / 1024
                  * bell(n - seg * half, half * 2 / 3) / 1024);
    }
    default: {
      // Session done: a rising third that lands, over a last wash.
      int32_t e = env(n, total / 3, total);
      int32_t a = osc(&s.phase, 196) * 34 / 1024;
      int32_t b = (n > total / 4) ? osc(&s.phase2, 294) * 30 / 1024 : 0;
      int32_t wash = surf() * 20 / 1024;
      return clip((a + b + wash) * e / 1024);
    }
  }
}

// ---------------------------------------------------------------------------
// Streaming
// ---------------------------------------------------------------------------

static void close_stream(void) {
  if (s.open) {
    speaker_stream_close();
    s.open = false;
  }
  s_timer = NULL;
}

static void feed(void *data) {
  s_timer = NULL;
  if (!s.open) return;

  // A short write means the speaker's buffer is full. The leftovers are kept
  // and offered again next tick rather than regenerated: the oscillators have
  // already moved on, so regenerating would put a step in the middle of a wave.
  if (!s_pending) {
    uint32_t want = s.total - s.n;
    if (want > CHUNK) want = CHUNK;
    for (uint32_t i = 0; i < want; i++) s_buf[i] = (int8_t)sample();
    s.n += want;
    s_pending = want;
    s_pending_off = 0;
  }

  if (s_pending) {
    uint32_t wrote = speaker_stream_write(s_buf + s_pending_off, s_pending);
    s_pending -= wrote;
    s_pending_off += wrote;
  }

  if (!s_pending && s.n >= s.total) {
    close_stream();     // buffered audio still plays out after the close
    return;
  }
  s_timer = app_timer_register(TICK_MS, feed, NULL);
}

bool synth_play(SynthCue cue, uint8_t volume) {
  synth_stop();

  if (!speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, volume)) return false;

  s.cue = cue;
  s.n = 0;
  s.total = cue_len(cue);
  s.phase = s.phase2 = 0;
  s.lp1 = s.lp2 = 0;
  s.rng = 0x1234567u + (uint32_t)cue * 7919u;
  s.open = true;
  s_pending = s_pending_off = 0;

  feed(NULL);           // first chunk immediately, so the cue starts on time
  return true;
}

void synth_stop(void) {
  if (s_timer) {
    app_timer_cancel(s_timer);
    s_timer = NULL;
  }
  if (s.open) {
    speaker_stop();
    close_stream();
  }
}
