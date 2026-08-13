#pragma once
#include <pebble.h>

void home_window_push(void);
void home_window_refresh(void);   // after a session or a settings change

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool home_window_is_top(void);
void home_window_handle_touch(const TouchEvent *event);
