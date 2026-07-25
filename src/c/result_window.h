#pragma once
#include "insights.h"

// `fresh_badges` is the bitmask returned by stats_record(): each set bit gets
// its own celebration page before the summary.
void result_window_push(const SessionResult *res, uint32_t fresh_badges);
