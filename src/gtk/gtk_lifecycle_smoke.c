/*
 * End-to-end GTK lifecycle smoke coverage for the assembled application.
 * The hook is deliberately dormant in normal runs and advances on GTK idles,
 * so the test depends on lifecycle completion rather than wall-clock sleeps.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#include "gtk_lifecycle_smoke.h"
#include "gui/buscar_notas.h"
#include "gui/lectura_sync.h"
#include "gui/main_menu.h"
#include "gui/main_window.h"
#include "gui/sidebar.h"
#include "gui/widgets.h"
#include "gui/nube_palabras.h"
#include "main/display.hh"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"
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
		if (GTK_IS_MESSAGE_DIALOG(l->data)) {
			gchar *text = NULL;
			g_object_get(l->data, "text", &text, NULL);
			g_printerr("GTK_LIFECYCLE_SMOKE_WARNING_DIALOG %s\n",
				   text ? text : "");
			g_free(text);
			n++;
		}
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

/* NOTES-STORE-101 in the running app: a verse note goes to notas.xml,
 * next to settings.xml, and never into settings.xml itself. */
static void
check_note_store(void)
{
	gchar *dir = g_path_get_dirname(settings.fnconfigure);
	gchar *store = g_build_filename(dir, "notas.xml", NULL);
	gchar *note, *cfg = NULL;

	highlight_set_verse_note(settings.MainWindowModule, "John.3.16",
				 "nota de humo");
	note = highlight_get_verse_note(settings.MainWindowModule, "John.3.16");
	check(note && !strcmp(note, "nota de humo"),
	      "verse note did not round-trip through the notes store");
	g_free(note);
	check(g_file_test(store, G_FILE_TEST_EXISTS),
	      "notes store file was not written next to settings.xml");
	note_remove_whole_verse(settings.MainWindowModule, "John.3.16");
	note = highlight_get_verse_note(settings.MainWindowModule, "John.3.16");
	check(!note || !*note, "verse note was not removed");
	g_free(note);
	xml_save_settings_doc(settings.fnconfigure);
	if (g_file_get_contents(settings.fnconfigure, &cfg, NULL, NULL))
		check(!strstr(cfg, "nota de humo"),
		      "a note was written into settings.xml");
	g_free(cfg);
	g_free(store);
	g_free(dir);
}

/* NOTES-V11N-101: a note on the whole verse written in another Bible is
 * listed at this Bible's counterpart, named after the Bible it came from,
 * and edited or removed where it was written. A highlighted phrase is
 * that edition's wording and stays there. */
static GList *
notes_at_john_3_16(void)
{
	notesCacheFill(settings.MainWindowModule, (gchar *)"John 3:16");
	return highlight_list_notes("John.3.16");
}

static void
check_foreign_verse_note(void)
{
	const gchar *other = "OtherBible";
	HighlightSegment seg = { (gchar *)"John.3.16", (gchar *)"God", 4 };
	GList *segs = g_list_append(NULL, &seg);
	GList *notes;
	HighlightNote *n;
	gchar *gid, *key = NULL, *native;

	if (!main_is_module((char *)other)) {
		check(FALSE, "smoke fixture did not provide a second Bible");
		g_list_free(segs);
		return;
	}
	highlight_add_verse_note(other, "John.3.16", "nota ajena");
	gid = highlight_create_group(other, segs, NULL);
	highlight_set_note(gid, "subrayado ajeno");
	g_list_free(segs);

	notes = notes_at_john_3_16();
	check(g_list_length(notes) == 1,
	      "foreign whole-verse note not listed alone at its counterpart");
	n = notes ? (HighlightNote *)notes->data : NULL;
	check(n && !n->group_id && !g_strcmp0(n->module, other) &&
	      !g_strcmp0(n->note, "nota ajena"),
	      "foreign note lost its text or its Bible's name");
	if (n)
		key = g_strdup(n->note_key);
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);

	highlight_set_verse_note_by_key(key, "nota ajena editada");
	notes = notes_at_john_3_16();
	n = notes ? (HighlightNote *)notes->data : NULL;
	check(g_list_length(notes) == 1 && n && !g_strcmp0(n->module, other) &&
	      !g_strcmp0(n->note, "nota ajena editada"),
	      "editing a foreign note did not update the note where it was written");
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);
	native = highlight_get_verse_note(settings.MainWindowModule, "John.3.16");
	check(!native || !*native, "editing a foreign note created a native one");
	g_free(native);

	notesCacheFill(settings.MainWindowModule, (gchar *)"John 3:16");
	highlight_remove_verse_note_by_key(key);
	notes = notes_at_john_3_16();
	check(notes == NULL, "removing a foreign note left it in place");
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);
	highlight_remove(gid);
	g_free(gid);
	g_free(key);
}

/* NOTES-DATES/EXPORT/TAGS/INDICATOR-101, in the running app. */
static void
check_notes_features(void)
{
	const gchar *other = "OtherBible";
	gint64 before = g_get_real_time() / G_USEC_PER_SEC;
	GList *notes, *toplevels, *l;
	HighlightNote *n;
	gchar *json, *marca, *copia = NULL, *key = NULL;
	NotesImportResult r;
	GError *error = NULL;

	highlight_set_verse_note(settings.MainWindowModule, "John.3.16",
				 "nota con #etiqueta");

	/* Dates: stamped when written. */
	notes = notes_at_john_3_16();
	n = notes ? (HighlightNote *)notes->data : NULL;
	check(n && n->created >= before && n->modified >= n->created,
	      "a new note has no creation date");
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);

	/* Indicator in another Bible (parallel view, compare panel): the
	 * note written in the main one is counted there, the marker opens
	 * the notes of that Bible at that verse, and editing it from there
	 * reaches the entry where it was written. */
	check(highlight_count_notes_for(other, "John.3.16") == 1,
	      "note not counted in another Bible");
	check(highlight_count_notes_for(other, "John.3.15") == 0,
	      "note counted at the wrong verse in another Bible");
	marca = highlight_note_marker_for(other, "John 3:16");
	check(marca && strstr(marca, "module=OtherBible&passage=John.3.16"),
	      "note marker for another Bible is missing or points elsewhere");
	g_free(marca);
	notes = highlight_list_notes_in(other, "John.3.16");
	n = notes ? (HighlightNote *)notes->data : NULL;
	check(g_list_length(notes) == 1 && n &&
	      !g_strcmp0(n->module, settings.MainWindowModule),
	      "notes listed from another Bible lost their origin");
	if (n)
		key = g_strdup(n->note_key);
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);
	highlight_set_verse_note_by_key(key, "nota con #etiqueta editada");
	g_free(key);
	notes = highlight_list_notes_in(other, "John.3.16");
	n = notes ? (HighlightNote *)notes->data : NULL;
	check(n && !g_strcmp0(n->note, "nota con #etiqueta editada") &&
	      !g_strcmp0(n->module, settings.MainWindowModule),
	      "editing from another Bible did not reach the original note");
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);

	/* Export, lose it, import: it comes back, and the file before the
	 * import is kept. */
	json = highlight_notes_export_json();
	check(json && strstr(json, "#etiqueta editada"),
	      "JSON export does not carry the note");
	note_remove_whole_verse(settings.MainWindowModule, "John.3.16");
	check(highlight_notes_import_json(json, &r, &copia, &error) &&
	      r.added == 1, "importing the JSON copy did not restore the note");
	g_clear_error(&error);
	check(copia && g_file_test(copia, G_FILE_TEST_EXISTS),
	      "no copy of the notes was kept before importing");
	notes = notes_at_john_3_16();
	check(notes && !g_strcmp0(((HighlightNote *)notes->data)->note,
				  "nota con #etiqueta editada"),
	      "imported note not shown");
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);
	check(highlight_notes_import_json(json, &r, NULL, NULL) &&
	      r.identical == 1 && r.added == 0,
	      "importing the same copy twice added notes");
	if (copia)
		g_remove(copia);
	g_free(copia);
	g_free(json);

	/* The notes dialog (tags, book and version filters, dates) opens
	 * and closes cleanly with a tagged note in it. */
	gui_buscar_notas_dialog(GTK_WINDOW(widgets.app));
	{
		GtkWidget *dialogo = NULL;
		toplevels = gtk_window_list_toplevels();
		for (l = toplevels; l && !dialogo; l = l->next)
			if (!g_strcmp0(gtk_window_get_title(GTK_WINDOW(l->data)),
				       "Buscar en mis notas"))
				dialogo = GTK_WIDGET(l->data);
		g_list_free(toplevels);
		check(dialogo && gtk_widget_get_visible(dialogo),
		      "notes dialog not shown");
		if (dialogo)
			gtk_widget_destroy(dialogo);
	}

	note_remove_whole_verse(settings.MainWindowModule, "John.3.16");
}

static void
check_word_cloud(void)
{
	NUBE_PALABRA words[3] = {
		{ .palabra = "dios", .cuenta = 124 },
		{ .palabra = "jesús", .cuenta = 106 },
		{ .palabra = "mano", .cuenta = 10 }
	};
	NUBE_CONTEO count = { .libro = "Lucas" };
	count.palabras = g_ptr_array_new();
	for (int i = 0; i < 3; ++i)
		g_ptr_array_add(count.palabras, &words[i]);
	gchar *html = gui_nube_palabras_html(&count, FALSE);
	GtkWidget *surface = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	g_object_ref_sink(surface);
	XIPHOS_HTML_OPEN_STREAM(surface, "text/html");
	XIPHOS_HTML_WRITE(surface, html, strlen(html));
	XIPHOS_HTML_CLOSE(surface);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(wk_html_get_view(WK_HTML(surface)));
	gdouble scales[3] = { 0, 0, 0 };
	for (int i = 0; i < 3; ++i) {
		GtkTextIter start, match, end;
		gtk_text_buffer_get_start_iter(buffer, &start);
		gboolean found = gtk_text_iter_forward_search(&start, words[i].palabra,
			GTK_TEXT_SEARCH_TEXT_ONLY, &match, &end, NULL);
		check(found, "cloud word reaches native renderer");
		if (found) {
			GtkTextAttributes *attrs = gtk_text_attributes_new();
			gtk_text_iter_get_attributes(&match, attrs);
			scales[i] = attrs->font_scale;
			check(attrs->justification == GTK_JUSTIFY_CENTER, "cloud centered");
			gtk_text_attributes_unref(attrs);
			check(gtk_text_iter_get_char(&end) == ' ', "cloud words separated");
		}
	}
	check(scales[0] > scales[1] && scales[1] > scales[2], "cloud frequency scales decrease");
	check(scales[0] >= 3.0 && scales[2] < 0.7, "cloud has visible size contrast");
	g_print("WORD_CLOUD_RENDER scales=%.3f,%.3f,%.3f\n", scales[0], scales[1], scales[2]);
	gtk_widget_destroy(surface);
	g_object_unref(surface);
	g_free(html);
	g_ptr_array_free(count.palabras, TRUE);
}

/* MENU-TIDY-101: the menu bar is the one the reader was promised, and
 * no Xiphos upstream link (mailing list, IRC chat, release notes) is
 * left in it. */
static void
check_menu_bar(void)
{
	const char *expected[] = {"_Archivo", "_Buscar", "_Estudio",
				  "_Lectura", "_Ver", "A_yuda"};
	GtkWidget *menu = widgets.readaloud_item
			      ? gtk_widget_get_parent(widgets.readaloud_item)
			      : NULL;
	GtkWidget *top = GTK_IS_MENU(menu)
			     ? gtk_menu_get_attach_widget(GTK_MENU(menu))
			     : NULL;
	GtkWidget *bar = top ? gtk_widget_get_parent(top) : NULL;
	GList *items, *l;
	guint i = 0;

	check(GTK_IS_MENU_BAR(bar), "main menu bar not found");
	if (!GTK_IS_MENU_BAR(bar))
		return;
	items = gtk_container_get_children(GTK_CONTAINER(bar));
	check(g_list_length(items) == G_N_ELEMENTS(expected),
	      "main menu bar does not have its six menus");
	for (l = items; l && i < G_N_ELEMENTS(expected); l = l->next, i++)
		check(!g_strcmp0(gtk_menu_item_get_label(GTK_MENU_ITEM(l->data)),
				 expected[i]),
		      "main menu bar menus are out of order");
	g_list_free(items);
	check(!g_strcmp0(gtk_menu_item_get_label(GTK_MENU_ITEM(top)), "_Lectura"),
	      "read aloud is not in the Lectura menu");
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
		check_note_store();
		check_foreign_verse_note();
		check_notes_features();
		check_menu_bar();
		check_word_cloud();
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
