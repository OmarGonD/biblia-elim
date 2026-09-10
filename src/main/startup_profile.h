#ifndef STARTUP_PROFILE_H
#define STARTUP_PROFILE_H

#include <glib.h>

G_BEGIN_DECLS

gchar *startup_profile_config_directory(void);
gint startup_profile_ensure_directory(const gchar *path, gint *error_number);

G_END_DECLS

#endif
