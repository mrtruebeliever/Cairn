#pragma once
#include "config.h"

// Phone link. The watch ships one record per finished session and can ask for
// trend numbers computed over the phone's much longer history; Clay settings
// arrive over the same inbox.
void comms_init(void);
void comms_send_session(const SessionRecord *rec);
void comms_request_trend(void);

// "Reset stats" on the watch has to wipe the phone's copy too, or the progress
// screen keeps reporting a history the user believes they deleted.
void comms_send_reset(void);
