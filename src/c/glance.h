#pragma once
#include <pebble.h>

// The app's line in the launcher ("app glance"): the running streak, for as
// long as it can still be kept alive. Called when the app closes.
void glance_update(void);
