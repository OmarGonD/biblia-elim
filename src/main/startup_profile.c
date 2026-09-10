#include <errno.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "startup_profile.h"

#define PROFILE_DIRECTORY "xiphos"

gchar *
startup_profile_config_directory(void)
{
	return g_build_filename(g_get_user_config_dir(), PROFILE_DIRECTORY, NULL);
}

gint
startup_profile_ensure_directory(const gchar *path, gint *error_number)
{
	gint result;
	gint saved_errno;

	errno = 0;
	result = g_mkdir_with_parents(path, S_IRWXU);
	saved_errno = errno;
	if (error_number)
		*error_number = result == 0 ? 0 : saved_errno;
	return result;
}
