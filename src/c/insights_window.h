#pragma once
#include "config.h"

void insights_window_push(void);

// Called when the phone answers with TREND_* values, so the open screen can
// swap its locally computed numbers for the full-history ones.
void insights_window_set_trend(const TrendData *t);
