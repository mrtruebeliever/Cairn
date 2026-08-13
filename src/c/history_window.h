#pragma once
#include <pebble.h>

void history_window_push(void);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool history_window_is_top(void);
void history_window_handle_touch(const TouchEvent *event);
