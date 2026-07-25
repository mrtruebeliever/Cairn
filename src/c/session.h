#pragma once
#include "insights.h"

void session_window_push(void);

// Brings the heart-rate sample-period request in line with the Show heart rate
// setting. Called at launch and whenever that setting is toggled, on the watch
// or from Clay — without it, turning the setting back on left the sensor on the
// watch's own multi-minute schedule for the rest of the session.
void hr_sampling_apply(void);
