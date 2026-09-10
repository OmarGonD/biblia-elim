#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "main/startup_profile.h"

static guint failures;

static void
check(gboolean condition, const gchar *message)
{
	if (condition)
		return;
	g_printerr("FAIL: %s\n", message);
	failures++;
}

int
main(void)
{
	g_autofree gchar *root = g_dir_make_tmp("biblia-elim-profile-XXXXXX", NULL);
	g_autofree gchar *home = NULL;
	g_autofree gchar *xdg_config = NULL;
	g_autofree gchar *profile = NULL;
	g_autofree gchar *expected_profile = NULL;
	g_autofree gchar *home_dot_config = NULL;
	g_autofree gchar *blocked = NULL;
	g_autofree gchar *blocked_child = NULL;
	gint error_number = -1;
	gint result;

	check(root != NULL, "temporary root was created");
	if (!root)
		return 1;
	home = g_build_filename(root, "home", NULL);
	xdg_config = g_build_filename(root, "new", "config", NULL);
	g_mkdir(home, 0700);
	g_setenv("HOME", home, TRUE);
	g_setenv("XDG_CONFIG_HOME", xdg_config, TRUE);

	profile = startup_profile_config_directory();
	expected_profile = g_build_filename(xdg_config, "xiphos", NULL);
	check(g_str_equal(profile, expected_profile),
	      "profile path uses XDG_CONFIG_HOME");
	result = startup_profile_ensure_directory(profile, &error_number);
	check(result == 0, "fresh profile directory creation succeeds");
	check(error_number == 0, "successful creation reports errno zero");
	check(g_file_test(profile, G_FILE_TEST_IS_DIR),
	      "fresh profile directory and missing parents are created");
	home_dot_config = g_build_filename(home, ".config", NULL);
	check(!g_file_test(home_dot_config, G_FILE_TEST_EXISTS),
	      "profile creation does not fall back to HOME/.config");

	result = startup_profile_ensure_directory(profile, &error_number);
	check(result == 0 && error_number == 0,
	      "reused profile directory setup succeeds");

	blocked = g_build_filename(root, "not-a-directory", NULL);
	blocked_child = g_build_filename(blocked, "xiphos", NULL);
	g_file_set_contents(blocked, "file", -1, NULL);
	result = startup_profile_ensure_directory(blocked_child, &error_number);
	check(result == -1, "directory creation failure returns minus one");
	check(error_number == ENOTDIR,
	      "errno is captured immediately after directory creation failure");
	check(g_strcmp0(g_strerror(error_number), g_strerror(0)) != 0,
	      "failure text cannot be strerror(0)");

	g_print("startup_profile_failures=%u failure_return=%d failure_errno=%d "
		"failure_error=%s\n", failures, result, error_number,
		g_strerror(error_number));

	g_remove(blocked);
	g_rmdir(profile);
	g_rmdir(xdg_config);
	{
		g_autofree gchar *new_parent = g_build_filename(root, "new", NULL);
		g_rmdir(new_parent);
	}
	g_rmdir(home);
	g_rmdir(root);
	return failures == 0 ? 0 : 1;
}
