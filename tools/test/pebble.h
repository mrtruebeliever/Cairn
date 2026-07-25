// Minimal stand-in for <pebble.h> so the pure-logic modules (settings, stats,
// insights) can be compiled and exercised on the host. Only the SDK surface
// those three files actually touch is stubbed — no graphics, no windows.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define APP_LOG(...) do { } while (0)
#define APP_LOG_LEVEL_WARNING 1
#define APP_LOG_LEVEL_ERROR 2

// --- persist storage: a tiny in-memory key/value store -----------------------
#define TEST_PERSIST_KEYS 64
#define TEST_PERSIST_BYTES 256

typedef struct {
  bool used;
  int32_t as_int;
  uint8_t data[TEST_PERSIST_BYTES];
  int data_len;
} TestPersistSlot;

extern TestPersistSlot g_persist[TEST_PERSIST_KEYS];

void test_persist_clear(void);

bool persist_exists(uint32_t key);
int32_t persist_read_int(uint32_t key);
bool persist_read_bool(uint32_t key);
int persist_read_data(uint32_t key, void *buffer, size_t size);
void persist_write_int(uint32_t key, int32_t value);
void persist_write_bool(uint32_t key, bool value);
int persist_write_data(uint32_t key, const void *data, size_t size);

// --- i18n --------------------------------------------------------------------
const char *i18n_get_system_locale(void);

// --- trig (only used by art.c, kept so config.h consumers still compile) -----
#define TRIG_MAX_ANGLE 0x10000
#define TRIG_MAX_RATIO 0xffff
int32_t sin_lookup(int32_t angle);
int32_t cos_lookup(int32_t angle);

// --- timers & speaker --------------------------------------------------------
// Enough of the surface for sfx_synth.c to compile on the host, so preview_sfx.c
// can render the generated cues to WAV files and they can be judged without
// flashing a watch. Nothing here plays audio.
typedef struct AppTimer AppTimer;
AppTimer *app_timer_register(uint32_t ms, void (*cb)(void *), void *data);
bool app_timer_cancel(AppTimer *timer);

typedef enum {
  SpeakerPcmFormat_8kHz_8bit = 0,
  SpeakerPcmFormat_16kHz_8bit = 1,
  SpeakerPcmFormat_8kHz_16bit = 2,
  SpeakerPcmFormat_16kHz_16bit = 3,
} SpeakerPcmFormat;

bool speaker_stream_open(SpeakerPcmFormat format, uint8_t volume);
uint32_t speaker_stream_write(const void *data, uint32_t num_bytes);
void speaker_stream_close(void);
void speaker_stop(void);
