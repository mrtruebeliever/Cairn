#include "glance.h"
#include "i18n.h"
#include "stats.h"

static char   s_subtitle[48];
static time_t s_expires;

static void reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  if (limit < 1) { return; }
  AppGlanceSlice slice = {
    .layout.icon = APP_GLANCE_SLICE_DEFAULT_ICON,
    .layout.subtitle_template_string = s_subtitle,
    .expiration_time = s_expires,
  };
  AppGlanceResult r = app_glance_add_slice(session, slice);
  if (r != APP_GLANCE_RESULT_SUCCESS) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "glance: add_slice %d", (int)r);
  }
}

void glance_update(void) {
  if (g_stats.streak <= 0) { app_glance_reload(NULL, NULL); return; }
  // A streak survives until the end of the day after its last session: done
  // today, it lasts through tomorrow; done yesterday, only through today. The
  // line disappears at the moment the streak breaks, app open or not.
  time_t midnight = time_start_of_today() + SECONDS_PER_DAY;
  bool done_today = (uint32_t)g_stats.last_date == stats_today();
  s_expires = done_today ? midnight + SECONDS_PER_DAY : midnight;
  snprintf(s_subtitle, sizeof(s_subtitle), S(S_HOME_STREAK), (int)g_stats.streak);
  app_glance_reload(reload, NULL);
}
