#include "settings.h"
#include "i18n.h"
#include "reminder.h"

Settings g_set;

const uint8_t SESSION_LENS[NUM_SESSION_LENS] = {1, 2, 3, 5, 10, 15};
const int16_t REMINDERS[NUM_REMINDERS] = {
  REMINDER_OFF, 7 * 60, 8 * 60, 12 * 60, 17 * 60, 19 * 60, 21 * 60, 22 * 60,
};

// Fixed presets, tenths of a second. Custom lives in g_set.custom.
static const BreathPattern PRESETS[NUM_PRESETS] = {
  [PRESET_COHERENT] = {55, 0, 55, 0},     // 5.5 s in / 5.5 s out — ~5.5 breaths/min
  [PRESET_BOX]      = {40, 40, 40, 40},
  [PRESET_478]      = {40, 70, 80, 0},
  [PRESET_CUSTOM]   = {40, 0, 60, 0},     // only a fallback; see g_set.custom
};

static int8_t read_int_or(uint32_t key, int8_t def) {
  return persist_exists(key) ? (int8_t)persist_read_int(key) : def;
}

void settings_load(void) {
  g_set.preset_idx = read_int_or(PERSIST_PRESET_IDX, PRESET_COHERENT);
  if (g_set.preset_idx >= NUM_PRESETS) g_set.preset_idx = PRESET_COHERENT;

  const BreathPattern *cd = &PRESETS[PRESET_CUSTOM];
  const uint32_t ck[4] = {PERSIST_CUSTOM_IN, PERSIST_CUSTOM_HOLD1,
                          PERSIST_CUSTOM_OUT, PERSIST_CUSTOM_HOLD2};
  const uint16_t cdef[4] = {cd->in, cd->hold1, cd->out, cd->hold2};
  for (int i = 0; i < 4; i++) {
    g_set.custom[i] = persist_exists(ck[i]) ? (uint16_t)persist_read_int(ck[i]) : cdef[i];
    if (g_set.custom[i] > CUSTOM_MAX) g_set.custom[i] = CUSTOM_MAX;
  }
  // in/out carry the breath, so they can never be zero.
  if (g_set.custom[0] < CUSTOM_MIN_IN) g_set.custom[0] = CUSTOM_MIN_IN;
  if (g_set.custom[2] < CUSTOM_MIN_IN) g_set.custom[2] = CUSTOM_MIN_IN;

  g_set.len_idx = read_int_or(PERSIST_SESSION_LEN, 2);   // 3 minutes
  if (g_set.len_idx >= NUM_SESSION_LENS) g_set.len_idx = 2;

  // Default Off: QEMU has no speaker and any playback kills the app, so the
  // emulator build must be silent until the user opts in on hardware.
  // Soft by default: the cues are part of the guidance, and a first-time user
  // who never opens Settings should hear them. (It was Off while the sound was
  // being written, because the emulator has no speaker and exits on the first
  // note — build with CAIRN_MUTE=1 for emulator work instead, see sfx.c.)
  g_set.sound_vol = read_int_or(PERSIST_SOUND_VOL, 1);
  if (g_set.sound_vol < 0 || g_set.sound_vol > 3) g_set.sound_vol = 1;

  g_set.haptics_on = persist_exists(PERSIST_HAPTICS_ON)
                       ? persist_read_bool(PERSIST_HAPTICS_ON) : true;

  // Off by default: on a Time 2 every sound ends in a tick when the firmware
  // cuts the amplifier (see sfx.c), and a tick every few seconds is the
  // opposite of calming. The vibration carries the phases; the sounds that
  // frame a session (open, result, badge, reminder) still follow Sound.
  g_set.phase_sound = persist_exists(PERSIST_PHASE_SOUND)
                        ? persist_read_bool(PERSIST_PHASE_SOUND) : false;

  g_set.bl_mode = read_int_or(PERSIST_BL_MODE, BL_TIMED);
  if (g_set.bl_mode < 0 || g_set.bl_mode >= NUM_BL_MODES) g_set.bl_mode = BL_TIMED;

  g_set.show_hr = persist_exists(PERSIST_SHOW_HR)
                    ? persist_read_bool(PERSIST_SHOW_HR) : true;

  g_set.reminder = persist_exists(PERSIST_REMINDER)
                     ? (int16_t)persist_read_int(PERSIST_REMINDER) : REMINDER_OFF;
  if (g_set.reminder < REMINDER_OFF || g_set.reminder >= 24 * 60) g_set.reminder = REMINDER_OFF;
}

void settings_set_preset(uint8_t idx) {
  g_set.preset_idx = idx % NUM_PRESETS;
  persist_write_int(PERSIST_PRESET_IDX, g_set.preset_idx);
}

void settings_set_custom(int slot, int32_t tenths) {
  if (slot < 0 || slot > 3) return;
  // in/out carry the breath and can never be zero; the holds may be.
  int32_t lo = (slot == 0 || slot == 2) ? CUSTOM_MIN_IN : 0;
  if (tenths > CUSTOM_MAX) tenths = lo;      // stepping past either end wraps
  else if (tenths < lo) tenths = CUSTOM_MAX;
  g_set.custom[slot] = (uint16_t)tenths;
  const uint32_t ck[4] = {PERSIST_CUSTOM_IN, PERSIST_CUSTOM_HOLD1,
                          PERSIST_CUSTOM_OUT, PERSIST_CUSTOM_HOLD2};
  persist_write_int(ck[slot], tenths);
}

void settings_set_len(uint8_t idx) {
  g_set.len_idx = idx % NUM_SESSION_LENS;
  persist_write_int(PERSIST_SESSION_LEN, g_set.len_idx);
}

void settings_set_sound(int8_t vol) {
  g_set.sound_vol = vol & 3;
  persist_write_int(PERSIST_SOUND_VOL, g_set.sound_vol);
}

void settings_set_haptics(bool on) {
  g_set.haptics_on = on;
  persist_write_bool(PERSIST_HAPTICS_ON, on);
}

void settings_set_phase_sound(bool on) {
  g_set.phase_sound = on;
  persist_write_bool(PERSIST_PHASE_SOUND, on);
}

void settings_set_backlight(int8_t mode) {
  g_set.bl_mode = mode % NUM_BL_MODES;
  persist_write_int(PERSIST_BL_MODE, g_set.bl_mode);
}

void settings_set_show_hr(bool on) {
  g_set.show_hr = on;
  persist_write_bool(PERSIST_SHOW_HR, on);
}

void settings_set_reminder(int16_t minutes) {
  if (minutes < REMINDER_OFF || minutes >= 24 * 60) minutes = REMINDER_OFF;
  g_set.reminder = minutes;
  persist_write_int(PERSIST_REMINDER, minutes);
  reminder_schedule();
}

void settings_next_reminder(void) {
  int i = 0;
  while (i < NUM_REMINDERS && REMINDERS[i] != g_set.reminder) i++;
  // A time set from the phone that is not in the list steps back to Off.
  settings_set_reminder(i >= NUM_REMINDERS ? REMINDER_OFF : REMINDERS[(i + 1) % NUM_REMINDERS]);
}

BreathPattern settings_pattern(void) {
  if (g_set.preset_idx == PRESET_CUSTOM) {
    return (BreathPattern){g_set.custom[0], g_set.custom[1],
                           g_set.custom[2], g_set.custom[3]};
  }
  return PRESETS[g_set.preset_idx];
}

uint8_t settings_len_min(void) { return SESSION_LENS[g_set.len_idx]; }

const char *settings_preset_name(uint8_t idx) {
  switch (idx) {
    case PRESET_BOX:    return S(S_PRESET_BOX);
    case PRESET_478:    return S(S_PRESET_478);
    case PRESET_CUSTOM: return S(S_PRESET_CUSTOM);
    default:            return S(S_PRESET_COHERENT);
  }
}

uint16_t settings_cycle_tenths(void) {
  BreathPattern p = settings_pattern();
  return p.in + p.hold1 + p.out + p.hold2;
}
