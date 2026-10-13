#include "pebble.h"
#include <math.h>

TestPersistSlot g_persist[TEST_PERSIST_KEYS];

void test_persist_clear(void) { memset(g_persist, 0, sizeof(g_persist)); }

static TestPersistSlot *slot(uint32_t key) {
  if (key >= TEST_PERSIST_KEYS) return NULL;
  return &g_persist[key];
}

bool persist_exists(uint32_t key) {
  TestPersistSlot *s = slot(key);
  return s && s->used;
}

int32_t persist_read_int(uint32_t key) {
  TestPersistSlot *s = slot(key);
  return (s && s->used) ? s->as_int : 0;
}

bool persist_read_bool(uint32_t key) { return persist_read_int(key) != 0; }

int persist_read_data(uint32_t key, void *buffer, size_t size) {
  TestPersistSlot *s = slot(key);
  if (!s || !s->used || s->data_len <= 0) return 0;
  int n = s->data_len < (int)size ? s->data_len : (int)size;
  memcpy(buffer, s->data, n);
  return n;
}

void persist_write_int(uint32_t key, int32_t value) {
  TestPersistSlot *s = slot(key);
  if (!s) return;
  s->used = true;
  s->as_int = value;
}

void persist_write_bool(uint32_t key, bool value) { persist_write_int(key, value ? 1 : 0); }

int persist_write_data(uint32_t key, const void *data, size_t size) {
  TestPersistSlot *s = slot(key);
  if (!s) return 0;
  if (size > TEST_PERSIST_BYTES) return 0;
  s->used = true;
  memcpy(s->data, data, size);
  s->data_len = (int)size;
  return (int)size;
}

// Force English so the assertions can compare against known strings.
const char *i18n_get_system_locale(void) { return "en_GB"; }

int32_t sin_lookup(int32_t angle) {
  return (int32_t)(sin((double)angle * 2 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

int32_t cos_lookup(int32_t angle) {
  return (int32_t)(cos((double)angle * 2 * M_PI / TRIG_MAX_ANGLE) * TRIG_MAX_RATIO);
}

// The reminder module is not under test; settings.c only reschedules it.
void reminder_schedule(void) {}
