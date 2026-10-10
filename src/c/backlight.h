#pragma once
#include "config.h"

// 3-state backlight shared across the projects: OFF hands back to the watch's
// own auto-dim, TIMED keeps the light on while the user interacts and dims
// BL_TIMEOUT_MS after the last touch, ALWAYS forces it on until we release it.
void backlight_apply(void);
void backlight_touch(void);
void backlight_release(void);

// Colour backlight (Time 2; a no-op elsewhere): tints the light along the
// breath, from dry sand when breathed out (`fullness` 0) to sea blue when
// breathed in (1000). backlight_tint_reset() hands the colour back to the
// user's own setting.
void backlight_tint_breath(int32_t fullness);
void backlight_tint_reset(void);
