#pragma once
#include "config.h"

// User settings, mirrored in persist storage and settable from Clay too.
typedef struct {
  uint8_t  preset_idx;    // PRESET_*
  uint16_t custom[4];     // in / hold1 / out / hold2, tenths of a second
  uint8_t  len_idx;       // index into SESSION_LENS
  int8_t   sound_vol;     // 0=Off 1=Soft 2=Normal 3=Loud (default Soft; build
                          // with CAIRN_MUTE=1 for emulator work, see sfx.c)
  bool     haptics_on;
  bool     phase_sound;   // sounds on the breathing phases (default off)
  int8_t   bl_mode;       // BacklightMode
  bool     show_hr;
  int16_t  reminder;      // minutes after midnight, REMINDER_OFF = none
} Settings;

extern Settings g_set;

void settings_load(void);

// Each setter writes through to persist immediately, so a crash mid-session
// never loses a preference.
void settings_set_preset(uint8_t idx);
void settings_set_custom(int slot, int32_t tenths);   // wraps at both ends
void settings_set_len(uint8_t idx);
void settings_set_sound(int8_t vol);
void settings_set_haptics(bool on);
void settings_set_phase_sound(bool on);
void settings_set_backlight(int8_t mode);
void settings_set_show_hr(bool on);
// Also (re)schedules the reminder's wakeup.
void settings_set_reminder(int16_t minutes);
// Steps to the next offered reminder time (Off first).
void settings_next_reminder(void);

// The pattern the current preset resolves to (custom included).
BreathPattern settings_pattern(void);
uint8_t settings_len_min(void);
const char *settings_preset_name(uint8_t idx);

// One cycle of the active pattern, in tenths of a second.
uint16_t settings_cycle_tenths(void);

// The on-watch settings screen (custom-drawn, no MenuLayer).
void settings_window_push(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool settings_window_is_top(void);
void settings_window_handle_touch(const TouchEvent *event);
