/* Exercise gui_init() up to GTK initialization, with the native platform
 * macros projected after host headers. This tests policy, not foreign GTK. */
#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#undef HAVE_CONFIG_H
#undef HAVE_DBUS
#undef DEBUG
#if defined(TEST_WINDOWS) || defined(TEST_MACOS)
#undef __linux__
#endif
#ifdef TEST_WINDOWS
#define WIN32 1
#endif
#ifdef TEST_MACOS
#define __APPLE__ 1
#endif

static jmp_buf reached_gtk;
static guint minor_version = 22;
static guint backend_calls;

static void record_backends(const char *backends)
{
	backend_calls++;
	g_assert_cmpstr(backends, ==, "wayland,x11");
}

static guint test_minor_version(void) { return minor_version; }
static gboolean stop_before_gtk(void) { longjmp(reached_gtk, 1); }
#ifdef TEST_WINDOWS
static gchar *test_install_directory(gpointer module)
{
	(void)module;
	return g_strdup("/unused-platform-test");
}
#define g_win32_get_package_installation_directory_of_module test_install_directory
#endif

#define gdk_set_allowed_backends record_backends
#define gtk_get_minor_version test_minor_version
#define gtk_init_check stop_before_gtk
#include "../src/gtk/gui.c"

int main(int argc, char **argv)
{
	gboolean explicit = argc > 1 && !strcmp(argv[1], "explicit");
	minor_version = argc > 1 && !strcmp(argv[1], "older-gtk") ? 20 : 22;
	g_unsetenv("GDK_BACKEND");
	g_unsetenv("GSK_RENDERER");
	if (explicit) {
		g_setenv("GDK_BACKEND", "native-override", TRUE);
		g_setenv("GSK_RENDERER", "cairo", TRUE);
	}
	if (!setjmp(reached_gtk)) {
		gui_init(argc, argv);
		g_assert_not_reached();
	}
#if defined(TEST_WINDOWS) || defined(TEST_MACOS)
	g_assert_cmpuint(backend_calls, ==, 0);
	g_assert_cmpstr(g_getenv("GSK_RENDERER"), ==, explicit ? "cairo" : NULL);
#else
	g_assert_cmpuint(backend_calls, ==, explicit ? 0 : 1);
	g_assert_cmpstr(g_getenv("GSK_RENDERER"), ==,
		explicit ? "cairo" : minor_version >= 22 ? "gl" : "ngl");
#endif
	g_assert_cmpstr(g_getenv("GDK_BACKEND"), ==, explicit ? "native-override" : NULL);
	g_print("gui_platform_defaults=PASS\n");
	return 0;
}
