#include "comms.h"
#include "home_window.h"
#include "insights_window.h"
#include "session.h"
#include "settings.h"
#include <stdlib.h>
#include <string.h>

// Reads an integer tuple regardless of the width the sender used. The phone
// sends minimum-width integers, so a value that fits in a byte arrives as
// int8 — reading value->int32 blindly returns garbage on hardware even though
// the emulator often hides it.
static bool tuple_int(DictionaryIterator *iter, uint32_t key, int32_t *out) {
  Tuple *t = dict_find(iter, key);
  if (!t) return false;
  switch (t->type) {
    case TUPLE_INT:
      if (t->length == 1)      *out = t->value->int8;
      else if (t->length == 2) *out = t->value->int16;
      else                     *out = t->value->int32;
      return true;
    case TUPLE_UINT:
      if (t->length == 1)      *out = t->value->uint8;
      else if (t->length == 2) *out = t->value->uint16;
      else                     *out = (int32_t)t->value->uint32;
      return true;
    case TUPLE_CSTRING:
      *out = atoi(t->value->cstring);
      return true;
    default:
      return false;
  }
}

static void apply_settings(DictionaryIterator *iter, bool *changed) {
  int32_t v;
  if (tuple_int(iter, MESSAGE_KEY_SETTINGS_PRESET, &v)) {
    settings_set_preset((uint8_t)v);
    *changed = true;
  }
  const uint32_t ckeys[4] = {MESSAGE_KEY_CUSTOM_IN, MESSAGE_KEY_CUSTOM_HOLD1,
                             MESSAGE_KEY_CUSTOM_OUT, MESSAGE_KEY_CUSTOM_HOLD2};
  for (int i = 0; i < 4; i++) {
    if (tuple_int(iter, ckeys[i], &v)) {
      settings_set_custom(i, (uint16_t)v);
      *changed = true;
    }
  }
  if (tuple_int(iter, MESSAGE_KEY_SESSION_LEN, &v)) {
    // Clay sends the duration in minutes; map it onto the nearest offered slot.
    uint8_t best = 0;
    int32_t best_diff = 1000;
    for (int i = 0; i < NUM_SESSION_LENS; i++) {
      int32_t d = (int32_t)SESSION_LENS[i] - v;
      if (d < 0) d = -d;
      if (d < best_diff) { best_diff = d; best = (uint8_t)i; }
    }
    settings_set_len(best);
    *changed = true;
  }
  if (tuple_int(iter, MESSAGE_KEY_SOUND_VOL, &v))  { settings_set_sound((int8_t)v); *changed = true; }
  if (tuple_int(iter, MESSAGE_KEY_HAPTICS_ON, &v)) { settings_set_haptics(v != 0); *changed = true; }
  if (tuple_int(iter, MESSAGE_KEY_PHASE_SOUND, &v)) { settings_set_phase_sound(v != 0); *changed = true; }
  if (tuple_int(iter, MESSAGE_KEY_REMINDER, &v))   { settings_set_reminder((int16_t)v); *changed = true; }
  if (tuple_int(iter, MESSAGE_KEY_SHOW_HR, &v)) {
    settings_set_show_hr(v != 0);
    hr_sampling_apply();
    *changed = true;
  }
}

static void apply_trend(DictionaryIterator *iter) {
  int32_t v;
  // TREND_SCORE_7 is the marker that this message carries trend data at all.
  if (!tuple_int(iter, MESSAGE_KEY_TREND_SCORE_7, &v)) return;

  TrendData t;
  memset(&t, 0, sizeof(t));
  t.from_phone = true;
  t.score_7 = (int8_t)v;
  t.score_30 = tuple_int(iter, MESSAGE_KEY_TREND_SCORE_30, &v) ? (int8_t)v : -1;
  t.hr_7     = tuple_int(iter, MESSAGE_KEY_TREND_HR_7, &v) ? (int8_t)v : -1;
  t.hr_30    = tuple_int(iter, MESSAGE_KEY_TREND_HR_30, &v) ? (int8_t)v : -1;
  t.streak   = tuple_int(iter, MESSAGE_KEY_TREND_STREAK, &v) ? (int8_t)v : 0;
  t.tip      = tuple_int(iter, MESSAGE_KEY_TREND_TIP, &v) ? (int8_t)v : -1;
  t.best_preset = tuple_int(iter, MESSAGE_KEY_TREND_PRESET, &v) ? (int8_t)v : -1;

  // TREND_DIR packs both directions as (score_dir + 1) * 10 + (hr_dir + 1),
  // keeping the message to a single extra tuple.
  if (tuple_int(iter, MESSAGE_KEY_TREND_DIR, &v)) {
    t.dir_score = (int8_t)(v / 10 - 1);
    t.dir_hr    = (int8_t)(v % 10 - 1);
  }
  insights_window_set_trend(&t);
}

static void inbox_received(DictionaryIterator *iter, void *context) {
  bool changed = false;
  apply_settings(iter, &changed);
  apply_trend(iter);
  if (changed) home_window_refresh();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "cairn: inbox dropped (%d)", (int)reason);
}

static void outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "cairn: outbox failed (%d)", (int)reason);
}

void comms_init(void) {
  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  // Small buffers: everything on the wire is a handful of integers.
  app_message_open(256, 256);
}

void comms_send_session(const SessionRecord *rec) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_ES_DATE, (int32_t)rec->date);
  dict_write_int32(out, MESSAGE_KEY_ES_DUR, rec->dur_s);
  dict_write_int32(out, MESSAGE_KEY_ES_HRSTART, rec->hr_start);
  dict_write_int32(out, MESSAGE_KEY_ES_HREND, rec->hr_end);
  dict_write_int32(out, MESSAGE_KEY_ES_SCORE, rec->score);
  dict_write_int32(out, MESSAGE_KEY_ES_PRESET, rec->preset);
  dict_write_int32(out, MESSAGE_KEY_ES_COMPLETION, rec->completion);
  app_message_outbox_send();
}

void comms_request_trend(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_TREND_REQ, 1);
  app_message_outbox_send();
}

void comms_send_reset(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
  dict_write_int32(out, MESSAGE_KEY_RESET_ALL, 1);
  app_message_outbox_send();
}
