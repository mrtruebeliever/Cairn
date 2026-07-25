#pragma once
#include "config.h"

// 3-state backlight shared across the projects: OFF hands back to the watch's
// own auto-dim, TIMED keeps the light on while the user interacts and dims
// BL_TIMEOUT_MS after the last touch, ALWAYS forces it on until we release it.
void backlight_apply(void);
void backlight_touch(void);
void backlight_release(void);
