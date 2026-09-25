/*
 * End-to-end GTK lifecycle smoke coverage for the assembled application.
 * The hook is deliberately dormant in normal runs and advances on GTK idles,
 * so the test depends on lifecycle completion rather than wall-clock sleeps.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "gtk_lifecycle_smoke.h"
#include "gui/lectura_sync.h"
#include "gui/main_menu.h"
#include "gui/main_window.h"
#include "gui/sidebar.h"
#include "gui/widgets.h"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"
#include "xiphos_html/xiphos_html.h"

typedef struct
{
	const char *name;
	GtkWidget *widget;
} SmokeSurface;

static gboolean requested;
static int exit_status;
static guint checks;
static guint navigation_checks;
static guint panel_checks;
static guint renderer_checks;

static void
fatal_gtk_log(const gchar *domain, GLogLevelFlags level, const gchar *message,
	      gpointer unused)
{
	(void)unused;
	g_printerr("GTK_LIFECYCLE_SMOKE_DIAGNOSTIC domain=%s level=%u %s\n",
		   domain ? domain : "", (unsigned int)level,
		   message ? message : "");
	abort();
}

int
gtk_lifecycle_smoke_exit_status(void)
{
	return exit_status;
}

void
gtk_lifecycle_smoke_install(void)
{
	const gchar *value = g_getenv("BIBLIA_ELIM_GTK_LIFECYCLE_SMOKE");
	GLogLevelFlags levels = G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING;

	requested = value && strcmp(value, "1") == 0;
	if (!requested)
		return;

	/* Catch classes of toolkit lifecycle/allocation failures, rather than a
	 * list of messages from past bugs. */
	g_log_set_handler("Gtk", levels, fatal_gtk_log, NULL);
	g_log_set_handler("Gdk", levels, fatal_gtk_log, NULL);
}

static void
check(gboolean condition, const char *what)
{
	checks++;
	if (condition)
		return;
	exit_status = 1;
	g_printerr("GTK_LIFECYCLE_SMOKE_CHECK_FAILED %s\n", what);
}

static void
check_allocation(GtkWidget *widget, gpointer unused)
{
	GtkAllocation allocation;

	(void)unused;
	if (!gtk_widget_get_realized(widget))
		return;
	gtk_widget_get_allocation(widget, &allocation);
	check(allocation.width >= 0 && allocation.height >= 0,
	      "realized widget has a negative allocation");
	if (GTK_IS_CONTAINER(widget))
		gtk_container_foreach(GTK_CONTAINER(widget), check_allocation, NULL);
}

static void
collect_surfaces(SmokeSurface surfaces[5])
{
	surfaces[0] = (SmokeSurface){ "bible", widgets.html_text };
	surfaces[1] = (SmokeSurface){ "commentary", widgets.html_comm };
	surfaces[2] = (SmokeSurface){ "compare", widgets.html_lectura_sync };
	surfaces[3] = (SmokeSurface){ "sidebar-preview", sidebar.html_viewer_widget };
	surfaces[4] = (SmokeSurface){ "lower-preview", widgets.html_previewer_text };
}

static void
render_surface(const SmokeSurface *surface)
{
	static const gchar content[] =
		"<html><body><h2>Lifecycle</h2><p><a name=\"v1\"></a>"
		"Rendered text</p><table><tr><td>cell</td></tr>"
		"<tr><td><hr></td></tr><tr><td>next</td></tr></table>"
		"</body></html>";

	check(WK_HTML_IS_HTML(surface->widget), surface->name);
	if (!WK_HTML_IS_HTML(surface->widget))
		return;
	XIPHOS_HTML_OPEN_STREAM(surface->widget, "text/html");
	XIPHOS_HTML_WRITE(surface->widget, content, (gint)strlen(content));
	XIPHOS_HTML_CLOSE(surface->widget);
	renderer_checks++;
}

/* Reopened panes get their splitter position on a later layout pass;
 * until then GtkPaned keeps the child unmapped. Wait for it, bounded. */
#define MAP_WAIT_MS 100
#define MAP_WAIT_TRIES 50

static gboolean
surfaces_mapped(const SmokeSurface surfaces[5])
{
	guint i;

	for (i = 0; i < 5; i++)
		if (!gtk_widget_get_mapped(surfaces[i].widget))
			return FALSE;
	return TRUE;
}

static gboolean
finish_smoke(gpointer unused)
{
	static guint tries;
	SmokeSurface surfaces[5];
	guint i;

	(void)unused;
	collect_surfaces(surfaces);
	if (!surfaces_mapped(surfaces) && tries++ < MAP_WAIT_TRIES) {
		g_timeout_add(MAP_WAIT_MS, finish_smoke, NULL);
		return G_SOURCE_REMOVE;
	}
	for (i = 0; i < G_N_ELEMENTS(surfaces); i++)
		check(gtk_widget_get_mapped(surfaces[i].widget),
		      "renderer did not map after its panel reopened");
	for (i = 0; i < G_N_ELEMENTS(surfaces); i++) {
		GtkWidget *top = gtk_widget_get_toplevel(surfaces[i].widget);
		check(top != surfaces[i].widget, "renderer is not anchored");
		check(gtk_widget_get_realized(surfaces[i].widget),
		      "renderer did not realize through its parent");
	}
	check_allocation(widgets.app, NULL);

	g_print("gtk_lifecycle_smoke_failures=%d checks=%u navigation=%u "
		"renderers=%u panels=%u\n",
		exit_status ? 1 : 0, checks, navigation_checks,
		renderer_checks, panel_checks);
	fflush(stdout);
	if (exit_status) {
		gtk_main_quit();
		return G_SOURCE_REMOVE;
	}

	/* Exercise the application's real orderly shutdown path. */
	on_quit_activate(NULL, NULL);
	return G_SOURCE_REMOVE;
}

static gboolean
show_panels(gpointer unused)
{
	(void)unused;
	gtk_widget_show(widgets.notebook_comm_book);
	gtk_widget_show(widgets.paned_sidebar);
	gtk_widget_show(widgets.box_side_preview);
	gtk_widget_show(widgets.vbox_previewer);
	/* The compare pane opens through its public helper, as «Comparar»
	 * does: the pane box alone leaves its renderer hidden since startup
	 * hid both (UI-SMOKE-103). */
	gui_lectura_sync_set_visible(TRUE);
	gtk_widget_show_all(widgets.app);
	check(gtk_widget_get_visible(widgets.notebook_comm_book),
	      "commentary panel did not reopen explicitly");
	{
		GtkWidget *cerrar = gtk_notebook_get_action_widget(
		    GTK_NOTEBOOK(widgets.notebook_comm_book), GTK_PACK_END);

		check(cerrar != NULL && GTK_IS_BUTTON(cerrar),
		      "commentary close button missing after show");
		if (cerrar && GTK_IS_BUTTON(cerrar)) {
			gint splitter;
			gint bible;

			gtk_button_clicked(GTK_BUTTON(cerrar));
			while (gtk_events_pending())
				gtk_main_iteration();
			check(!gtk_widget_get_visible(widgets.notebook_comm_book),
			      "commentary close button did not hide the panel");
			check(!gtk_widget_get_visible(widgets.vpaned2),
			      "study splitter still visible after commentary close");
			splitter = gtk_widget_get_allocated_width(widgets.hpaned);
			bible = gtk_widget_get_allocated_width(widgets.vpaned);
			check(splitter > 0 && bible >= splitter - 24,
			      "bible pane did not expand after commentary close");
			/* Reopen as «Ver comentario» does: raw show() left the
			 * settings closed, so the next layout pass (the stale
			 * dictionary request below) hid it again (UI-SMOKE-103). */
			gui_show_hide_comms(TRUE);
			check(gtk_widget_get_visible(widgets.notebook_comm_book),
			      "commentary panel did not reopen after close");
		}
	}
	/* Dictionary/Devotional has no visible entry point anymore.  A stale
	 * session/tab request must keep it closed rather than reviving a retired
	 * pane; exercising the public helper verifies that policy. */
	gui_show_hide_dicts(TRUE);
	check(!settings.showdicts && !gtk_widget_get_visible(widgets.notebook_dict_devot),
	      "retired dictionary panel reopened from a stale request");
	check(gtk_widget_get_visible(widgets.notebook_comm_book) &&
	      gtk_widget_get_visible(widgets.vpaned2),
	      "commentary panel closed again after the dictionary request");
	/* Without a commentary module the layout opens the «Notas» page, so
	 * the commentary renderer is behind a hidden notebook page. Select
	 * its tab as the reader would, after the last layout pass, so the
	 * surface's own map is exercised (UI-SMOKE-103). */
	{
		GtkNotebook *nb = GTK_NOTEBOOK(widgets.notebook_comm_book);
		gint page = gtk_notebook_page_num(nb, widgets.box_comm);

		check(page >= 0, "commentary page missing from its notebook");
		if (page >= 0)
			gtk_notebook_set_current_page(nb, page);
		check(gtk_notebook_get_current_page(nb) == page,
		      "commentary tab could not be selected");
	}
	check(gtk_widget_get_visible(widgets.box_lectura_sync),
	      "compare panel did not reopen explicitly");
	check(gtk_widget_get_visible(widgets.html_lectura_sync),
	      "compare renderer stayed hidden after the panel reopened");
	panel_checks += 16;
	g_idle_add(finish_smoke, NULL);
	return G_SOURCE_REMOVE;
}

static gboolean
hide_panels(gpointer unused)
{
	(void)unused;
	gtk_widget_hide(widgets.notebook_comm_book);
	gtk_widget_hide(widgets.notebook_dict_devot);
	gtk_widget_hide(widgets.paned_sidebar);
	gtk_widget_hide(widgets.box_side_preview);
	gtk_widget_hide(widgets.vbox_previewer);
	gui_lectura_sync_set_visible(FALSE);
	check(!gtk_widget_get_visible(widgets.notebook_comm_book),
	      "commentary panel did not close");
	check(!gtk_widget_get_visible(widgets.notebook_dict_devot),
	      "dictionary panel did not close");
	check(!gtk_widget_get_visible(widgets.paned_sidebar),
	      "sidebar did not close");
	check(!gtk_widget_get_visible(widgets.box_lectura_sync),
	      "compare panel did not close");
	panel_checks += 7;
	g_idle_add(show_panels, NULL);
	return G_SOURCE_REMOVE;
}

/* BOOKMARK-V11N-101 in the running app: the URL route every bookmark
 * opens through (it used to dereference the SWORD BackEnd, NULL under the
 * SQLite backend). A bookmark with its module navigates; one saved without
 * a module is read as KJV, which this fixture's Bible declares, so it
 * navigates too and raises no «no equivalent» warning. The unmapped case
 * (Vulgate) is covered by versification_transition_test. */
static gboolean
current_verse_is(const char *suffix)
{
	return settings.currentverse &&
	       g_str_has_suffix(settings.currentverse, suffix);
}

static guint
warning_dialogs(void)
{
	GList *tops = gtk_window_list_toplevels();
	GList *l;
	guint n = 0;

	for (l = tops; l; l = l->next)
		if (GTK_IS_MESSAGE_DIALOG(l->data))
			n++;
	g_list_free(tops);
	return n;
}

static void
check_bookmark_routes(void)
{
	gchar *module = g_strdup(main_url_encode(settings.MainWindowModule));
	gchar *url = g_strdup_printf(
	    "passagestudy.jsp?action=showBookmark&type=currentTab&"
	    "value=John%%203:16&module=%s", module);

	main_url_handler(url, TRUE);
	g_free(url);
	g_free(module);
	check(current_verse_is("3:16"),
	      "module-qualified bookmark did not navigate");

	main_url_handler("passagestudy.jsp?action=showBookmark&type=currentTab&"
			 "value=John%203:15&module=", TRUE);
	check(current_verse_is("3:15"),
	      "module-less bookmark did not navigate in a KJV Bible");
	check(warning_dialogs() == 0,
	      "module-less bookmark warned although KJV maps to KJV");
	navigation_checks += 3;
}

static gboolean
exercise_application(gpointer unused)
{
	SmokeSurface surfaces[5];
	guint i;

	(void)unused;
	check(GTK_IS_WINDOW(widgets.app), "main window was not created");
	check(gtk_widget_get_visible(widgets.app), "main window was not shown");
	check(gtk_widget_get_realized(widgets.app), "main window was not realized");
	check(gtk_widget_get_mapped(widgets.app), "main window was not mapped");
	check(gtk_widget_get_no_show_all(widgets.notebook_comm_book),
	      "startup-hidden commentary participates in show-all");
	check(gtk_widget_get_no_show_all(widgets.notebook_dict_devot),
	      "startup-hidden dictionary participates in show-all");
	check(gtk_widget_get_no_show_all(widgets.box_lectura_sync),
	      "startup-hidden compare pane participates in show-all");
	check(!gtk_widget_get_visible(widgets.notebook_comm_book),
	      "commentary is visible at default startup");
	check(gtk_notebook_get_action_widget(
		  GTK_NOTEBOOK(widgets.notebook_comm_book), GTK_PACK_END) != NULL,
	      "commentary panel has no close button");
	check(!gtk_widget_get_visible(widgets.notebook_dict_devot),
	      "dictionary is visible at default startup");
	check(!gtk_widget_get_visible(widgets.box_lectura_sync),
	      "compare pane is visible at default startup");

	collect_surfaces(surfaces);
	for (i = 0; i < G_N_ELEMENTS(surfaces); i++)
		render_surface(&surfaces[i]);

	if (settings.havebible) {
		gchar *reference = main_update_nav_controls(
			settings.MainWindowModule, "John 3:17");
		check(reference != NULL, "reference change was rejected");
		if (reference) {
			main_display_bible(settings.MainWindowModule, reference);
			g_free(reference);
		}
		main_navbar_versekey_spin_verse(navbar_versekey, 1);
		main_navbar_versekey_spin_verse(navbar_versekey, 0);
		main_navbar_versekey_spin_chapter(navbar_versekey, 1);
		main_navbar_versekey_spin_chapter(navbar_versekey, 0);
		navigation_checks += 5;
		check_bookmark_routes();
	} else {
		check(FALSE, "smoke fixture did not provide a Bible module");
	}

	gtk_widget_show_all(widgets.app);
	g_idle_add(hide_panels, NULL);
	return G_SOURCE_REMOVE;
}

void
gtk_lifecycle_smoke_schedule(void)
{
	if (requested)
		g_idle_add(exercise_application, NULL);
}
