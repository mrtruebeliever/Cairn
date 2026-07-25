// Renders every generated cue to an 8 kHz WAV so the sound design can be judged
// on a PC. The emulator has no speaker at all — any playback exits the app — so
// this is the only way to hear a change before it goes on the watch.
//
// The synth is included as source rather than linked, so the generator and its
// state are reachable directly and no speaker or timer is involved.
//
// Build & run: tools/test/preview_sfx.sh   ->  build/sfx-preview/*.wav
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../src/c/sfx_synth.c"

// --- the bits of the SDK the synth touches ----------------------------------
int32_t sin_lookup(int32_t angle) {
  return (int32_t)(sin(2.0 * M_PI * (double)angle / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
int32_t cos_lookup(int32_t angle) {
  return (int32_t)(cos(2.0 * M_PI * (double)angle / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}
AppTimer *app_timer_register(uint32_t ms, void (*cb)(void *), void *data) { return NULL; }
bool app_timer_cancel(AppTimer *timer) { return true; }
bool speaker_stream_open(SpeakerPcmFormat format, uint8_t volume) { return false; }
uint32_t speaker_stream_write(const void *data, uint32_t num_bytes) { return num_bytes; }
void speaker_stream_close(void) {}
void speaker_stop(void) {}

static void put32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void put16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

// 8-bit WAV samples are unsigned, the speaker's are signed — hence the +128.
static void write_wav(const char *path, const int8_t *pcm, uint32_t n) {
  FILE *f = fopen(path, "wb");
  if (!f) { perror(path); exit(1); }
  fwrite("RIFF", 1, 4, f); put32(f, 36 + n); fwrite("WAVE", 1, 4, f);
  fwrite("fmt ", 1, 4, f); put32(f, 16); put16(f, 1); put16(f, 1);
  put32(f, SR); put32(f, SR); put16(f, 1); put16(f, 8);
  fwrite("data", 1, 4, f); put32(f, n);
  for (uint32_t i = 0; i < n; i++) fputc((int)pcm[i] + 128, f);
  fclose(f);
}

static void render(SynthCue cue, const char *name, const char *dir) {
  memset(&s, 0, sizeof(s));
  s.cue = cue;
  s.total = cue_len(cue);
  s.rng = 0x1234567u + (uint32_t)cue * 7919u;

  int8_t *pcm = malloc(s.total);
  int peak = 0, clipped = 0;
  double sum2 = 0;
  for (s.n = 0; s.n < s.total; s.n++) {
    int32_t raw = sample();
    pcm[s.n] = (int8_t)raw;
    int a = abs(raw);
    if (a > peak) peak = a;
    if (a >= 127) clipped++;
    sum2 += (double)raw * raw;
  }

  char path[512];
  snprintf(path, sizeof(path), "%s/%s.wav", dir, name);
  write_wav(path, pcm, s.total);
  printf("%-8s %5.2f s  peak %3d  rms %5.1f  clipped %u%%  %s\n",
         name, (double)s.total / SR, peak, sqrt(sum2 / s.total),
         (unsigned)(100 * clipped / s.total), path);
  free(pcm);
}

int main(int argc, char **argv) {
  const char *dir = (argc > 1) ? argv[1] : ".";
  render(SYNTH_OPEN,  "open",  dir);
  render(SYNTH_IN,    "in",    dir);
  render(SYNTH_HOLD,  "hold",  dir);
  render(SYNTH_OUT,   "out",   dir);
  render(SYNTH_DONE,  "done",  dir);
  render(SYNTH_BADGE, "badge", dir);
  render(SYNTH_DENY,  "deny",  dir);
  return 0;
}
