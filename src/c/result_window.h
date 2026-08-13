#pragma once
#include "insights.h"

// `fresh_badges` is the bitmask returned by stats_record(): each set bit gets
// its own celebration page before the summary.
void result_window_push(const SessionResult *res, uint32_t fresh_badges);

// Raw touch, forwarded from the app-wide subscription while this window is on top.
bool result_window_is_top(void);
void result_window_handle_touch(const TouchEvent *event);
