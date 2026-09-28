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
#include "gui/widget_helpers.h"
#include <stdlib.h>
#include <string.h>

#include "gtk_lifecycle_smoke.h"
#include "gui/buscar_notas.h"
#include "gui/lectura_sync.h"
#include "gui/main_menu.h"
#include "gui/elim_tema.h"
#include "gui/menu_popup.h"
#include "gui/main_window.h"
#include "gui/sidebar.h"
#include "gui/bookmarks_menu.h"
#include "main/sidebar.h"
#include "gui/gui.h"
#include "gui/widgets.h"
#include "gui/nube_palabras.h"
#include "gui/preferences_dialog.h"
#include "gui/search_dialog.h"
#include "main/search_dialog.h"
#include "gui/utilities.h"
#include "gui/interlineal.h"
#include "editor/editor.h"
#include "gui/dropdown_helpers.h"
#include "main/memorizacion.h"
#include "gui/table_helpers.h"
#include "gui/sqlite_module_manager_dialog.h"
#include "gui/dictlex_dialog.h"
#include "main/planes_lectura.h"
#include "gui/planes_lectura.h"
#include "gui/instalar_biblias.h"
#include "gui/memorizacion.h"
#include "gui/treekey-editor.h"
#include "main/display.hh"
#include "main/navbar_versekey.h"
#include "main/tab_history.h"
#include "gui/tabbed_browser.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/parallel_view.h"
#include "main/module_dialogs.h"
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

static GtkWidget *find_widget_of_type(GtkWidget *widget, GType type);

static void
check_allocation(GtkWidget *widget, gpointer unused)
{
	GtkAllocation allocation;
	GtkWidget *child;

	(void)unused;
	if (!gtk_widget_get_realized(widget))
		return;
	gtk_widget_get_allocation(widget, &allocation);
	check(allocation.width >= 0 && allocation.height >= 0,
	      "realized widget has a negative allocation");
	for (child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child))
		check_allocation(child, NULL);
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

/* GtkPaned 4 gives the second child of a reopened splitter the pixel
 * left over when the first one's natural size is taller than the window
 * (this one is ~300 px), and unmaps it at that size. The application sets
 * the saved position when it reopens a previewer; do the same for both,
 * again on each wait for the layout while one is still unmapped. */
static void
give_previews_room(void)
{
	GtkWidget *previews[] = { widgets.box_side_preview,
				  widgets.vbox_previewer };

	for (guint i = 0; i < G_N_ELEMENTS(previews); i++) {
		GtkWidget *paned = gtk_widget_get_parent(previews[i]);

		if (GTK_IS_PANED(paned) && gtk_widget_get_height(paned) > 1 &&
		    !gtk_widget_get_mapped(previews[i]))
			gtk_paned_set_position(GTK_PANED(paned),
					       gtk_widget_get_height(paned) / 2);
	}
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
		give_previews_room();
		g_timeout_add(MAP_WAIT_MS, finish_smoke, NULL);
		return G_SOURCE_REMOVE;
	}
	for (i = 0; i < G_N_ELEMENTS(surfaces); i++) {
		gchar *what = g_strdup_printf(
		    "renderer '%s' did not map after its panel reopened",
		    surfaces[i].name);
		check(gtk_widget_get_mapped(surfaces[i].widget), what);
		g_free(what);
	}
	for (i = 0; i < G_N_ELEMENTS(surfaces); i++) {
		GtkWidget *top = gui_widget_get_toplevel(surfaces[i].widget);
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
		gui_main_quit();
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
	give_previews_room();
	/* The compare pane opens through its public helper, as «Comparar»
	 * does: the pane box alone leaves its renderer hidden since startup
	 * hid both (UI-SMOKE-103). */
	gui_lectura_sync_set_visible(TRUE);
	gtk_widget_show(widgets.app);
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

			g_signal_emit_by_name(cerrar, "clicked");
			while (g_main_context_pending(NULL))
				g_main_context_iteration(NULL, FALSE);
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

/* Module id column of the sidebar module tree (main/sidebar.cc). */
#define MODULE_TREE_COL_MODULE 3

static gboolean
module_tree_row_is(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
		   gpointer data)
{
	gchar *name = NULL;
	gboolean *found = data;

	(void)path;
	gtk_tree_model_get(model, iter, MODULE_TREE_COL_MODULE, &name, -1);
	if (name && !strcmp(name, g_object_get_data(G_OBJECT(model), "smoke-wanted")))
		*found = TRUE;
	g_free(name);
	return *found;
}

static gboolean
module_tree_has(GtkTreeModel *model, const char *module)
{
	gboolean found = FALSE;

	g_object_set_data(G_OBJECT(model), "smoke-wanted", (gpointer)module);
	gtk_tree_model_foreach(model, module_tree_row_is, &found);
	g_object_set_data(G_OBJECT(model), "smoke-wanted", NULL);
	return found;
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

	/* A verse the fixture Bible has: this checks the module-less route,
	 * not what happens to a missing verse (below). */
	main_url_handler("passagestudy.jsp?action=showBookmark&type=currentTab&"
			 "value=John%203:17&module=", TRUE);
	check(current_verse_is("3:17"),
	      "module-less bookmark did not navigate in a KJV Bible");

	/* The fixture Bible has no John 3:15: navigation stays in the
	 * chapter, on the nearest verse it has, never the start of the
	 * Bible. */
	main_display_bible(settings.MainWindowModule, "John 3:15");
	check(current_verse_is("3:16"),
	      "a missing verse did not land on the nearest verse of its chapter");
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
		if (dialogo) {
			/* GTK4-PORT-101 step 2: «Exportar…» is a GMenu with actions. */
			GtkWidget *exportar = find_widget_of_type(dialogo, GTK_TYPE_MENU_BUTTON);
			GMenuModel *modelo = exportar ?
				gtk_menu_button_get_menu_model(GTK_MENU_BUTTON(exportar)) : NULL;
			check(modelo && g_menu_model_get_n_items(modelo) == 2,
			      "notes export menu model missing");
			GActionGroup *grupo = gui_widget_get_action_group(dialogo, "notas");
			check(grupo && g_action_group_has_action(grupo, "exportar-md") &&
			      g_action_group_has_action(grupo, "exportar-json"),
			      "notes export actions missing");
			/* GTK4-TREE-101: the results are a GtkColumnView of rows
			 * that hand back the note the reader picks. */
			GtkWidget *tabla = find_widget_of_type(dialogo, GTK_TYPE_COLUMN_VIEW);
			check(tabla != NULL, "notes results are not a GtkColumnView");
			if (tabla) {
				GListStore *filas = elim_table_get_store(tabla);

				check(g_list_model_get_n_items(gtk_column_view_get_columns(
					  GTK_COLUMN_VIEW(tabla))) == 4,
				      "notes results should have four columns");
				check(g_list_model_get_n_items(G_LIST_MODEL(filas)) >= 1,
				      "the tagged note is not in the results");
				check(elim_table_get_selected(tabla) == NULL,
				      "notes results opened with a row picked");
				elim_table_select(tabla, 0, FALSE);
				ElimRow *nota = elim_table_get_selected(tabla);
				check(nota && *elim_row_get_string(nota, 4) != '\0',
				      "picked note has no passage reference");
			}
			gui_widget_destroy(dialogo);
		}
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
	GtkTextTag *center_tag = gtk_text_tag_table_lookup(
	    gtk_text_buffer_get_tag_table(buffer), "center");
	gdouble scales[3] = { 0, 0, 0 };
	for (int i = 0; i < 3; ++i) {
		GtkTextIter start, match, end;
		gtk_text_buffer_get_start_iter(buffer, &start);
		gboolean found = gtk_text_iter_forward_search(&start, words[i].palabra,
			GTK_TEXT_SEARCH_TEXT_ONLY, &match, &end, NULL);
		check(found, "cloud word reaches native renderer");
		if (found) {
			/* GTK 4 keeps no per-iter attributes struct: the effective
			 * scale is the product of every "scale" tag covering the
			 * match, the same probe wk-html.c's font debug uses. */
			gdouble scale = 1.0;
			GSList *tags = gtk_text_iter_get_tags(&match);
			for (GSList *l = tags; l; l = l->next) {
				gboolean scale_set = FALSE;
				gdouble tag_scale = 1.0;

				g_object_get(l->data, "scale-set", &scale_set,
					     "scale", &tag_scale, NULL);
				if (scale_set)
					scale *= tag_scale;
			}
			g_slist_free(tags);
			scales[i] = scale;
			check(center_tag && gtk_text_iter_has_tag(&match, center_tag),
			      "cloud centered");
			check(gtk_text_iter_get_char(&end) == ' ', "cloud words separated");
		}
	}
	check(scales[0] > scales[1] && scales[1] > scales[2], "cloud frequency scales decrease");
	check(scales[0] >= 3.0 && scales[2] < 0.7, "cloud has visible size contrast");
	g_print("WORD_CLOUD_RENDER scales=%.3f,%.3f,%.3f\n", scales[0], scales[1], scales[2]);
	gui_widget_destroy(surface);
	g_object_unref(surface);
	g_free(html);
	g_ptr_array_free(count.palabras, TRUE);
}

/* The first widget of TYPE under WIDGET, depth first. */
static GtkWidget *
find_widget_of_type(GtkWidget *widget, GType type)
{
	if (G_TYPE_CHECK_INSTANCE_TYPE(widget, type))
		return widget;
	GList *children = gui_widget_get_children(widget);
	GtkWidget *found = NULL;
	for (GList *l = children; l && !found; l = l->next)
		found = find_widget_of_type(l->data, type);
	g_list_free(children);
	return found;
}

/* The stack holding the «cloud» and «message» pages (other stacks live in
 * the dialog's own widgets). */
static GtkWidget *
find_cloud_stack(GtkWidget *widget)
{
	if (GTK_IS_STACK(widget) &&
	    gtk_stack_get_child_by_name(GTK_STACK(widget), "cloud"))
		return widget;
	GList *children = gui_widget_get_children(widget);
	GtkWidget *found = NULL;
	for (GList *l = children; l && !found; l = l->next)
		found = find_cloud_stack(l->data);
	g_list_free(children);
	return found;
}

static gboolean
cloud_shown(GtkWidget *stack)
{
	return !g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(stack)), "cloud");
}

static void
pump_until(gboolean (*done)(GtkWidget *), GtkWidget *widget, gint64 limit_us)
{
	gint64 end = g_get_monotonic_time() + limit_us;
	while (!done(widget) && g_get_monotonic_time() < end) {
		if (!g_main_context_iteration(NULL, FALSE))
			g_usleep(5000);
	}
}

static gboolean
panel_b_visible(GtkWidget *stack)
{
	GtkWidget *panels = gtk_stack_get_child_by_name(GTK_STACK(stack), "cloud");
	GList *children = gui_widget_get_children(panels);
	gboolean visible = g_list_length(children) == 2 &&
		gtk_widget_get_visible(g_list_nth_data(children, 1));
	g_list_free(children);
	return visible;
}

static gboolean
widget_mapped(GtkWidget *widget)
{
	return gtk_widget_get_mapped(widget);
}

/* CLOUD-FIT-104: a file chooser that restores a size larger than the
 * screen (Hyprland at scale 2: 1720x720) stays inside the main window. */
static void
check_file_chooser_fits(void)
{
	GtkWidget *chooser = gtk_file_chooser_dialog_new("fit", GTK_WINDOW(widgets.app),
		GTK_FILE_CHOOSER_ACTION_SAVE, "_Cancel", GTK_RESPONSE_CANCEL, NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(chooser));
	gtk_window_set_default_size(GTK_WINDOW(chooser), 5000, 5000);
	gtk_widget_show(chooser);
	pump_until(widget_mapped, chooser, 3 * G_USEC_PER_SEC);
	int width = gtk_widget_get_width(chooser);
	int height = gtk_widget_get_height(chooser);
	check(gtk_widget_get_mapped(chooser), "file chooser not shown");
	check(width <= gtk_widget_get_allocated_width(widgets.app) &&
	      height <= gtk_widget_get_allocated_height(widgets.app),
	      "file chooser larger than the main window");
	gui_widget_destroy(chooser);
}

/* CLOUD-LOOK-102: the word cloud opens already drawn for a book, and
 * ticking «Comparar con» draws the comparison with another book. */
static void
check_word_cloud_dialog(void)
{
	gui_nube_palabras_dialog();
	GtkWidget *dialog = NULL;
	GList *toplevels = gtk_window_list_toplevels();
	for (GList *l = toplevels; l && !dialog; l = l->next)
		if (!g_strcmp0(gtk_window_get_title(GTK_WINDOW(l->data)), "Nube de palabras"))
			dialog = l->data;
	g_list_free(toplevels);
	check(dialog && gtk_widget_get_visible(dialog), "word cloud dialog not shown");
	if (!dialog)
		return;
	GtkWidget *stack = find_cloud_stack(dialog);
	GtkWidget *compare = find_widget_of_type(dialog, GTK_TYPE_CHECK_BUTTON);
	GtkWidget *grid = find_widget_of_type(dialog, GTK_TYPE_GRID);
	check(stack && compare && grid, "word cloud widgets missing");
	if (stack && compare && grid) {
		pump_until(cloud_shown, stack, 15 * G_USEC_PER_SEC);
		check(cloud_shown(stack), "word cloud not drawn on opening");
		GtkWidget *combo_a = gtk_grid_get_child_at(GTK_GRID(grid), 1, 0);
		GtkWidget *combo_b = gtk_grid_get_child_at(GTK_GRID(grid), 1, 1);
		const gchar *book_a = elim_dropdown_get_active_text(GTK_DROP_DOWN(combo_a));
		check(book_a && *book_a, "word cloud opened without a book");
		gtk_check_button_set_active(GTK_CHECK_BUTTON(compare), TRUE);
		const gchar *book_b = elim_dropdown_get_active_text(GTK_DROP_DOWN(combo_b));
		check(book_b && *book_b && g_strcmp0(book_a, book_b) != 0,
		      "comparing did not choose another book");
		GtkWidget *download = gtk_grid_get_child_at(GTK_GRID(grid), 2, 0);
		check(GTK_IS_BUTTON(download) && gtk_widget_get_sensitive(download),
		      "word cloud download not available");
		gtk_check_button_set_active(GTK_CHECK_BUTTON(compare), FALSE);
		pump_until(cloud_shown, stack, 5 * G_USEC_PER_SEC);
		check(cloud_shown(stack) && !panel_b_visible(stack),
		      "unticking comparison did not return to one cloud");
	}
	/* «Cerrar» sits at the foot of the dialog, off the edge, and closes it. */
	GtkWidget *close = NULL;
	{
		GtkWidget *paned = find_widget_of_type(dialog, GTK_TYPE_PANED);
		graphene_point_t origin = GRAPHENE_POINT_INIT(0, 0), at;
		GList *buttons = NULL;
		for (GtkWidget *b = gtk_widget_get_first_child(gtk_window_get_child(GTK_WINDOW(dialog)));
		     b; b = gtk_widget_get_next_sibling(b))
			buttons = g_list_append(buttons, b);
		for (GList *l = buttons; l && !close; l = l->next) {
			GtkWidget *btn = find_widget_of_type(l->data, GTK_TYPE_BUTTON);
			if (btn && !g_strcmp0(gtk_button_get_label(GTK_BUTTON(btn)), "Cerrar"))
				close = btn;
		}
		g_list_free(buttons);
		check(close != NULL, "word cloud close button missing");
		if (close && paned && gtk_widget_compute_point(close, dialog, &origin, &at)) {
			check(at.y > gtk_widget_get_height(paned) / 2,
			      "word cloud close button not at the foot of the dialog");
			check(at.x + gtk_widget_get_width(close) <= gtk_widget_get_width(dialog) - 6 &&
			      at.y + gtk_widget_get_height(close) <= gtk_widget_get_height(dialog) - 6,
			      "word cloud close button touches the dialog edge");
		}
	}
	if (close) {
		g_signal_emit_by_name(close, "clicked");
		while (g_main_context_pending(NULL))
			g_main_context_iteration(NULL, FALSE);
		gboolean listed = FALSE;
		GList *open = gtk_window_list_toplevels();
		for (GList *l = open; l; l = l->next)
			listed |= l->data == (gpointer)dialog;
		g_list_free(open);
		check(!listed && !gtk_widget_get_visible(dialog),
		      "word cloud close button did not close the dialog");
	} else {
		gui_widget_destroy(dialog);
	}
}

/* GTK4-PORT-101 step 6: builder dialogs whose XML GTK 4 rejected opened
 * empty or not at all (the failure was silent). Each one must show a new
 * window. */
static void
check_builder_dialog_opens(const char *name, void (*open_dialog)(void))
{
	GList *before = gtk_window_list_toplevels();
	GList *added = NULL;

	open_dialog();
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
	GList *after = gtk_window_list_toplevels();
	for (GList *l = after; l; l = l->next)
		if (!g_list_find(before, l->data) && gtk_widget_get_visible(l->data))
			added = g_list_prepend(added, l->data);
	{
		gchar *what = g_strdup_printf("%s dialog did not open", name);
		check(added != NULL, what);
		g_free(what);
	}
	for (GList *l = added; l; l = l->next)
		gui_widget_destroy(l->data);
	g_list_free(added);
	g_list_free(before);
	g_list_free(after);
}

/* GTK4-TREE-101: the memorization list is a GtkColumnView over rows, not a
 * GtkTreeView. It shows the verse added while it is open, in four columns,
 * and hands back the row the reader picks. */
static void
check_memorizacion_dialog(void)
{
	GList *before = gtk_window_list_toplevels();
	GtkWidget *dialog = NULL;
	const char *clave = "John 3:16";

	gui_memorizacion_dialog(GTK_WINDOW(widgets.app));
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
	GList *after = gtk_window_list_toplevels();
	for (GList *l = after; l; l = l->next)
		if (!g_list_find(before, l->data) && gtk_widget_get_visible(l->data))
			dialog = l->data;
	check(dialog != NULL, "memorization dialog did not open");
	if (dialog) {
		GtkWidget *view = find_widget_of_type(dialog, GTK_TYPE_COLUMN_VIEW);

		check(view != NULL, "memorization list is not a GtkColumnView");
		if (view) {
			GListModel *columns = gtk_column_view_get_columns(GTK_COLUMN_VIEW(view));
			GListStore *store = elim_table_get_store(view);
			guint rows = g_list_model_get_n_items(G_LIST_MODEL(store));

			check(g_list_model_get_n_items(columns) == 4,
			      "memorization list should have four columns");
			check(elim_table_get_selected(view) == NULL,
			      "memorization list opened with a row picked");
			gui_memorizacion_anadir(clave);
			while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
			check(g_list_model_get_n_items(G_LIST_MODEL(store)) == rows + 1 ||
			      main_memoria_tiene(clave) == FALSE,
			      "added verse missing from the memorization list");
			if (g_list_model_get_n_items(G_LIST_MODEL(store)) > rows) {
				gtk_selection_model_select_item(
				    gtk_column_view_get_model(GTK_COLUMN_VIEW(view)),
				    g_list_model_get_n_items(G_LIST_MODEL(store)) - 1, TRUE);
				ElimRow *row = elim_table_get_selected(view);
				check(row && strcmp(elim_row_get_string(row, 4), "") != 0,
				      "picked row has no key");
			}
		}
		gui_widget_destroy(dialog);
	}
	g_list_free(before);
	g_list_free(after);
}

/* GTK4-TREE-101: the Bible catalog is a GtkColumnView whose first column is
 * a check box per module. Whatever the catalog holds (it comes from the
 * profile's own repository list), the view has its seven columns and a
 * click on the box of a module that is not installed marks that row. */
static GtkWidget *
first_check_button(GtkWidget *widget)
{
	if (GTK_IS_CHECK_BUTTON(widget))
		return widget;
	for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
	     child = gtk_widget_get_next_sibling(child)) {
		GtkWidget *found = first_check_button(child);

		if (found)
			return found;
	}
	return NULL;
}

static void
check_install_dialog(void)
{
	GList *before = gtk_window_list_toplevels();
	GtkWidget *dialog = NULL;

	gui_instalar_biblias();
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
	GList *after = gtk_window_list_toplevels();
	for (GList *l = after; l; l = l->next)
		if (!g_list_find(before, l->data) && gtk_widget_get_visible(l->data))
			dialog = l->data;
	check(dialog != NULL, "install Bibles dialog did not open");
	if (dialog) {
		GtkWidget *view = find_widget_of_type(dialog, GTK_TYPE_COLUMN_VIEW);

		check(view != NULL, "Bible catalog is not a GtkColumnView");
		if (view) {
			GListStore *store = elim_table_get_store(view);
			ElimRow *row = elim_table_get(store, 0);

			check(g_list_model_get_n_items(gtk_column_view_get_columns(
				  GTK_COLUMN_VIEW(view))) == 7,
			      "Bible catalog should have seven columns");
			if (row && !elim_row_get_int(row, 7)) {
				GtkWidget *box = first_check_button(view);

				check(box != NULL, "catalog row has no check box");
				if (box) {
					check(!elim_row_get_int(row, 0),
					      "catalog row starts marked");
					gtk_check_button_set_active(GTK_CHECK_BUTTON(box), TRUE);
					while (g_main_context_pending(NULL))
						g_main_context_iteration(NULL, FALSE);
					check(elim_row_get_int(row, 0) == 1,
					      "clicking the check box did not mark the row");
				}
			}
		}
		gui_widget_destroy(dialog);
	}
	g_list_free(before);
	g_list_free(after);
}

/* GTK4-TREE-101: the reading plans are a GtkListView (plans, browse mode)
 * and a GtkColumnView (days, with a check box each). Opening picks a plan
 * and lists its days; picking another plan lists that one's; ticking a day
 * marks it and keeps the plan the reader is looking at. */
static void
check_plans_dialog(void)
{
	GList *before = gtk_window_list_toplevels();
	GtkWidget *dialog = NULL;

	gui_planes_lectura_dialog();
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
	GList *after = gtk_window_list_toplevels();
	for (GList *l = after; l; l = l->next)
		if (!g_list_find(before, l->data) && gtk_widget_get_visible(l->data))
			dialog = l->data;
	check(dialog != NULL, "reading plans dialog did not open");
	if (dialog) {
		GtkWidget *plans = find_widget_of_type(dialog, GTK_TYPE_LIST_VIEW);
		GtkWidget *days = find_widget_of_type(dialog, GTK_TYPE_COLUMN_VIEW);

		check(plans && days, "reading plans lists are not GtkListView/GtkColumnView");
		if (plans && days && main_planes_cuantos() > 0) {
			GListStore *plan_rows = elim_table_get_store(plans);
			GListStore *day_rows = elim_table_get_store(days);
			ElimRow *row = elim_table_get_selected(plans);

			check(g_list_model_get_n_items(G_LIST_MODEL(plan_rows)) ==
				  (guint)main_planes_cuantos(),
			      "plans list should hold every plan");
			check(row != NULL, "no plan is picked on opening");
			if (row) {
				const PL_PLAN *plan =
				    main_planes_get(elim_row_get_int(row, 1));

				check(g_list_model_get_n_items(G_LIST_MODEL(day_rows)) ==
					  (guint)plan->dias,
				      "days list does not match the picked plan");
			}
			if (main_planes_cuantos() > 1) {
				const PL_PLAN *other = main_planes_get(1);

				elim_table_select(plans, 1, TRUE);
				while (g_main_context_pending(NULL))
					g_main_context_iteration(NULL, FALSE);
				check(g_list_model_get_n_items(G_LIST_MODEL(day_rows)) ==
					  (guint)other->dias,
				      "days list did not follow the picked plan");
				GtkWidget *box = first_check_button(days);

				check(box != NULL, "days list has no check box");
				if (box) {
					int before_done = main_planes_dias_hechos(other);

					gtk_check_button_set_active(GTK_CHECK_BUTTON(box), TRUE);
					while (g_main_context_pending(NULL))
						g_main_context_iteration(NULL, FALSE);
					check(main_planes_dias_hechos(other) == before_done + 1,
					      "ticking a day did not mark it");
					check(elim_table_get_selected_position(plans) == 1,
					      "ticking a day changed the picked plan");
					check(g_list_model_get_n_items(G_LIST_MODEL(day_rows)) ==
						  (guint)other->dias,
					      "days list lost its days after ticking");
				}
			}
		}
		gui_widget_destroy(dialog);
	}
	g_list_free(before);
	g_list_free(after);
}

/* GTK4-TREE-101: the key list of a dictionary window is a GtkListView of
 * rows, without a module behind it (the fixture has no dictionary): the
 * window builds, the list shows the keys put in it, and picking one puts it
 * in the entry. */
static void
check_dictionary_key_list(void)
{
	DIALOG_DATA *dlg = g_new0(DIALOG_DATA, 1);

	gui_create_dictlex_dialog(dlg);
	check(dlg->listview && GTK_IS_LIST_VIEW(dlg->listview),
	      "dictionary key list is not a GtkListView");
	if (dlg->listview && GTK_IS_LIST_VIEW(dlg->listview)) {
		GListStore *keys = elim_table_get_store(dlg->listview);
		const char *names[] = { "Aarón", "Abba", "Abdías" };

		check(settings.cell_height > 0, "key list row height not measured");
		for (int i = 0; i < 3; i++) {
			ElimRow *row = elim_row_new(1);

			elim_row_set_string(row, 0, names[i]);
			g_list_store_append(keys, row);
			g_object_unref(row);
		}
		check(elim_table_get_selected(dlg->listview) == NULL,
		      "key list opened with a key picked");
		/* the entry's own "changed" refills the list from the module,
		 * which this window has none of: only the pick is looked at */
		g_signal_handlers_disconnect_matched(dlg->entry, G_SIGNAL_MATCH_ID,
						     g_signal_lookup("changed",
								      GTK_TYPE_EDITABLE),
						     0, NULL, NULL, NULL);
		elim_table_select(dlg->listview, 1, FALSE);
		check(strcmp(gtk_editable_get_text(GTK_EDITABLE(dlg->entry)), "Abba") == 0,
		      "picking a key did not put it in the entry");
	}
	if (dlg->dialog)
		gui_widget_destroy(dlg->dialog);
}

/* GTK4-TREE-101: the six flat lists of the advanced search dialog are
 * GtkListViews of two-string rows. Creating a custom module list and a range
 * puts a row in its list and picks it; typing the name in the entry renames
 * the row; adding a module fills the module list and the picked list's
 * modules; picking a results row and clearing the results run without a
 * search behind them. */
static gboolean
search_dialog_lists_are_tables(void)
{
	GtkWidget *lists[6];
	gboolean all = TRUE;

	lists[0] = search1.module_lists;
	lists[1] = search1.list_range_name;
	lists[2] = search1.list_ranges;
	lists[3] = search1.listview_modules;
	lists[4] = search1.listview_results;
	lists[5] = search1.listview_verses;
	for (guint i = 0; i < G_N_ELEMENTS(lists); i++) {
		gboolean is_table = lists[i] && GTK_IS_LIST_VIEW(lists[i]);

		check(is_table, "search dialog list is not a GtkListView");
		all &= is_table;
		if (is_table)
			check(elim_table_get_selected(lists[i]) == NULL,
			      "search dialog list opened with a row picked");
	}
	return all;
}

static void
check_search_dialog_list_rows(void)
{
	/* a custom module list: a row, picked, named like the entry */
	new_modlist(NULL, NULL);
	check(g_list_model_get_n_items(G_LIST_MODEL(
		  elim_table_get_store(search1.module_lists))) == 1,
	      "new module list did not add a row");
	ElimRow *list_row = elim_table_get_selected(search1.module_lists);
	check(list_row != NULL, "new module list was not picked");
	if (list_row) {
		check(*elim_row_get_string(list_row, 0) != '\0' &&
			  strcmp(elim_row_get_string(list_row, 0),
				 gtk_editable_get_text(GTK_EDITABLE(
				     search1.entry_list_name))) == 0,
		      "module list name and its entry differ");
		gtk_editable_set_text(GTK_EDITABLE(search1.entry_list_name),
				      "Mi lista");
		check(strcmp(elim_row_get_string(list_row, 0), "Mi lista") == 0,
		      "typing the list name did not rename its row");
	}

	/* a module added to it: listed, and kept with the list */
	gchar *module = g_strdup(settings.MainWindowModule);
	main_add_mod_to_list(search1.listview_modules, module);
	GListStore *modules = elim_table_get_store(search1.listview_modules);
	check(g_list_model_get_n_items(G_LIST_MODEL(modules)) == 1 &&
		  strcmp(elim_row_get_string(elim_table_get(modules, 0), 1),
			 module) == 0,
	      "added module is not in the list");
	check(list_row && strcmp(elim_row_get_string(list_row, 1), module) == 0,
	      "the module list did not keep its module");
	main_add_modlist_to_label();
	g_free(module);

	/* a custom range: a row, picked, renamed by its entry */
	new_range(NULL, NULL);
	ElimRow *range_row = elim_table_get_selected(search1.list_range_name);
	check(range_row != NULL, "new range was not picked");
	if (range_row) {
		check(*elim_row_get_string(range_row, 0) != '\0',
		      "new range has no name");
		gtk_editable_set_text(GTK_EDITABLE(search1.entry_range_name),
				      "Mi rango");
		check(strcmp(elim_row_get_string(range_row, 0), "Mi rango") == 0,
		      "typing the range name did not rename its row");
	}

	/* results: picking a summary row with no search behind it, then
	 * clearing both lists */
	{
		ElimRow *found = elim_row_new(2);
		ElimRow *verse = elim_row_new(2);

		elim_row_set_string(found, 0, "1 found in Test");
		elim_row_set_string(verse, 0, "Test: Genesis 1:1 text");
		g_list_store_append(elim_table_get_store(search1.listview_results), found);
		g_list_store_append(elim_table_get_store(search1.listview_verses), verse);
		g_object_unref(found);
		g_object_unref(verse);
		elim_table_select(search1.listview_results, 0, FALSE);
		check(elim_table_get_selected(search1.listview_results) != NULL,
		      "results row could not be picked");
		button_clean(NULL, NULL);
		check(g_list_model_get_n_items(G_LIST_MODEL(
			  elim_table_get_store(search1.listview_results))) == 0 &&
			  g_list_model_get_n_items(G_LIST_MODEL(
			      elim_table_get_store(search1.listview_verses))) == 0,
		      "clearing the results left rows behind");
	}
}

static void
check_search_dialog_lists(void)
{
	gui_create_search_dialog();
	if (search_dialog_lists_are_tables())
		check_search_dialog_list_rows();
	if (search1.dialog)
		gui_widget_destroy(search1.dialog);
	while (g_main_context_pending(NULL))
		g_main_context_iteration(NULL, FALSE);
}

/* GTK4-TREE-101: the SQLite module manager is a modal dialog listing the
 * modules of the profile in a GtkColumnView. It blocks in its own loop, so
 * it is looked at from an idle handler that then closes it. */
static gboolean sqlite_manager_inspected;

static gboolean
inspect_sqlite_manager(gpointer unused)
{
	GList *toplevels = gtk_window_list_toplevels();
	GtkWidget *dialog = NULL;

	(void)unused;
	for (GList *l = toplevels; l && !dialog; l = l->next)
		if (!g_strcmp0(gtk_window_get_title(GTK_WINDOW(l->data)),
			       "Módulos SQLite") &&
		    gtk_widget_get_visible(l->data))
			dialog = GTK_WIDGET(l->data);
	g_list_free(toplevels);
	if (!dialog)
		return G_SOURCE_CONTINUE;	/* not shown yet */
	sqlite_manager_inspected = TRUE;
	GtkWidget *view = find_widget_of_type(dialog, GTK_TYPE_COLUMN_VIEW);

	check(view != NULL, "SQLite module list is not a GtkColumnView");
	if (view) {
		GListStore *rows = elim_table_get_store(view);

		check(g_list_model_get_n_items(gtk_column_view_get_columns(
			  GTK_COLUMN_VIEW(view))) == 5,
		      "SQLite module list should have five columns");
		check(g_list_model_get_n_items(G_LIST_MODEL(rows)) >= 1,
		      "SQLite module list is empty");
		check(elim_table_get_selected(view) == NULL,
		      "SQLite module list opened with a row picked");
		elim_table_select(view, 0, FALSE);
		ElimRow *row = elim_table_get_selected(view);
		check(row && *elim_row_get_string(row, 0) != '\0',
		      "picked SQLite module has no id");
	}
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	return G_SOURCE_REMOVE;
}

static void
check_sqlite_module_manager(void)
{
	sqlite_manager_inspected = FALSE;
	g_timeout_add(50, inspect_sqlite_manager, NULL);
	gui_open_sqlite_module_manager();
	check(sqlite_manager_inspected, "SQLite module manager was never shown");
}

/* MENU-TIDY-101: the menu bar is the one the reader was promised, and
 * no Xiphos upstream link (mailing list, IRC chat, release notes) is
 * left in it. */
static void
check_menu_bar(void)
{
	const char *expected[] = {"_Archivo", "_Buscar", "_Estudio",
				  "_Lectura", "_Ver", "A_yuda"};
	GMenuModel *bar = gui_main_menu_model();
	check(bar && g_menu_model_get_n_items(bar) == G_N_ELEMENTS(expected),
	      "main menu bar does not have its six menus");
	for (guint i = 0; bar && i < G_N_ELEMENTS(expected); ++i) {
		gchar *label = NULL;
		g_menu_model_get_item_attribute(bar, i, G_MENU_ATTRIBUTE_LABEL,
						"s", &label);
		check(!g_strcmp0(label, expected[i]),
		      "main menu bar menus are out of order");
		g_free(label);
	}
	GActionGroup *actions = gui_main_menu_actions();
	check(actions && g_action_group_has_action(actions, "read-aloud") &&
	      g_action_group_has_action(actions, "interlinear") &&
	      g_action_group_has_action(actions, "reading-mode"),
	      "main menu state actions missing");
	GVariant *state = actions ? g_action_group_get_action_state(
	    actions, "interlinear") : NULL;
	check(state && g_variant_get_boolean(state) ==
			(settings.show_interlineal != 0),
	      "main menu interlinear state lost its setting");
	g_clear_pointer(&state, g_variant_unref);

	gchar *mode = g_strdup(settings.ui_mode);
	gui_elim_tema_set(!g_strcmp0(mode, "oscuro") ? "claro" : "oscuro");
	state = actions ? g_action_group_get_action_state(actions, "theme") : NULL;
	check(state && !g_strcmp0(g_variant_get_string(state, NULL), settings.ui_mode),
	      "main menu theme radio does not follow the theme");
	g_clear_pointer(&state, g_variant_unref);
	gui_elim_tema_set(mode);
	g_free(mode);
}

static void
check_sqlite_parallel(void)
{
    gchar **saved = settings.parallel_list;
    gboolean docked = settings.dockedInt;
    gchar *list[] = { "FakeBible", "OtherBible", NULL };
    settings.parallel_list = list;
    settings.dockedInt = TRUE;
    main_display_bible(settings.MainWindowModule, "John 3:16");
    check(!g_strcmp0(settings.cvparallel, settings.currentverse),
          "SQLite parallel follows current passage");
    gchar *html = main_parallel_html();
    check(html != NULL, "SQLite parallel chapter generated");
    if (html) {
        check(strstr(html, "OtherBible") != NULL, "SQLite-only second column");
        check(strstr(html, "For God so loved the world.") != NULL, "parallel verse body");
        check(strstr(html, "God sent his Son to save the world.") != NULL, "parallel chapter verse count");
        check(strstr(html, "SQLite parallel heading") != NULL, "parallel neutral heading");
    }
    g_free(html);
    settings.parallel_list = saved;
    settings.dockedInt = docked;
}

static gboolean
exercise_application(gpointer unused)
{
	SmokeSurface surfaces[5];
	guint i;

	(void)unused;

	GtkTreeModel *modules = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.module_list));
	check(GTK_IS_TREE_MODEL(modules), "hidden module tree has no model");
	check(g_object_get_data(G_OBJECT(sidebar.module_list), "elim-module-tree-pending") != NULL,
	      "hidden module tree populated at startup");
	gui_sidebar_showhide();
	/* Mapping is asynchronous in X11: a cold start may not have mapped the
	 * window yet, so wait for the real map before judging the lazy load. */
	{
		gint64 end = g_get_monotonic_time() + 5 * G_USEC_PER_SEC;
		while (!gtk_widget_get_mapped(sidebar.module_list) &&
		       g_get_monotonic_time() < end)
			if (!g_main_context_iteration(NULL, FALSE))
				g_usleep(5000);
	}
	check(gtk_widget_get_mapped(sidebar.module_list),
	      "module tree not mapped after showing the sidebar");
	check(!g_object_get_data(G_OBJECT(sidebar.module_list), "elim-module-tree-pending"),
	      "module tree not populated on first map");
	modules = gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.module_list));
	check(gtk_tree_model_iter_n_children(modules, NULL) > 0, "mapped module tree empty");
	/* SQLite mode lists SWORD's modules plus the Bibles only SQLite has:
	 * the fixture Bibles exist in SQLite alone. */
	check(module_tree_has(modules, "FakeBible"),
	      "module tree lacks a Bible only SQLite has");
	gui_sidebar_showhide();
	gui_sidebar_showhide();
	check(modules == gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.module_list)),
	      "module tree rebuilt on second map");
	gui_sidebar_showhide();
	main_load_module_tree(sidebar.module_list);
	check(gtk_tree_model_iter_n_children(gtk_tree_view_get_model(GTK_TREE_VIEW(sidebar.module_list)), NULL) > 0,
	      "hidden module reload empty");

	/* Hidden sidebar menus must stay unbuilt until first use. */
	check(sidebar.results_actions == NULL, "results popup built at startup");
	check(menu.actions == NULL, "bookmark popup built at startup");
	/* GTK4-PORT-101 step 2: verse lists and search results share a
	 * three-section GMenu over the «lista» action group. */
	GMenuModel *results_menu = gui_sidebar_results_menu();
	check(results_menu && g_menu_model_get_n_items(results_menu) == 3,
	      "results popup does not have its three sections");
	for (gint section = 0; section < 3; ++section) {
		GMenuModel *items = g_menu_model_get_item_link(
		    results_menu, section, G_MENU_LINK_SECTION);
		check(items && g_menu_model_get_n_items(items) == 2,
		      "results popup section does not have two items");
		if (items)
			g_object_unref(items);
	}
	GActionGroup *results_actions = gui_sidebar_results_actions();
	check(results_actions &&
	      g_action_group_has_action(results_actions, "guardar-uno") &&
	      g_action_group_has_action(results_actions, "guardar-varios") &&
	      g_action_group_has_action(results_actions, "rellenar") &&
	      g_action_group_has_action(results_actions, "historial") &&
	      g_action_group_has_action(results_actions, "biblesync") &&
	      g_action_group_has_action(results_actions, "exportar"),
	      "results popup actions missing");
	check(!g_action_group_get_action_enabled(results_actions, "guardar-uno") &&
	      !g_action_group_get_action_enabled(results_actions, "exportar"),
	      "results actions enabled without a verse list");
	check(results_menu == gui_sidebar_results_menu(), "results popup rebuilt");
	GtkWidget *results_popover = gui_sidebar_results_popup(sidebar.results_list);
	check(GTK_IS_POPOVER(results_popover) && gtk_widget_get_visible(results_popover),
	      "results popup popover was not shown");
	if (GTK_IS_POPOVER(results_popover))
		gtk_popover_popdown(GTK_POPOVER(results_popover));
	/* GTK4-PORT-101 step 2: the reader context menu is rebuilt from a
	 * GMenu so its module options and selection sensitivity are current. */
	GtkWidget *context_popover = gui_menu_popup(
	    (XiphosHtml *)widgets.html_text, settings.MainWindowModule, NULL);
	check(GTK_IS_POPOVER(context_popover) &&
	      gtk_widget_get_visible(context_popover),
	      "reader context popover was not shown");
	GMenuModel *context_model = context_popover ? g_object_get_data(
	    G_OBJECT(context_popover), "elim-menu-model") : NULL;
	check(context_model && g_menu_model_get_n_items(context_model) == 4,
	      "reader context menu does not have its four sections");
	GMenuModel *context_menus = context_model ? g_menu_model_get_item_link(
	    context_model, 2, G_MENU_LINK_SECTION) : NULL;
	check(context_menus && g_menu_model_get_n_items(context_menus) == 4,
	      "reader context menu lacks its four submenus");
	if (context_menus)
		g_object_unref(context_menus);
	GActionGroup *context_actions = gui_widget_get_action_group(
	    widgets.html_text, "contexto");
	check(context_actions &&
	      g_action_group_has_action(context_actions, "acerca") &&
	      g_action_group_has_action(context_actions, "copiar") &&
	      g_action_group_has_action(context_actions, "seleccion") &&
	      g_action_group_has_action(context_actions, "leer"),
	      "reader context actions missing");
	check(context_actions &&
	      !g_action_group_get_action_enabled(context_actions, "seleccion") &&
	      !g_action_group_get_action_enabled(context_actions, "leer"),
	      "selection actions enabled without a selection");
	check(context_actions &&
	      g_action_group_has_action(context_actions, "strong"),
	      "reader context Strong action missing for capable module");
	GVariant *strong_state = context_actions ? g_action_group_get_action_state(
	    context_actions, "strong") : NULL;
	check(strong_state &&
	      g_variant_get_boolean(strong_state) == (settings.show_interlineal != 0),
	      "reader context Strong state lost the setting");
	if (strong_state)
		g_variant_unref(strong_state);
	if (GTK_IS_POPOVER(context_popover))
		gtk_popover_popdown(GTK_POPOVER(context_popover));
	/* GTK4-PORT-101 step 2: the bookmark popup is «marcadores» actions. */
	gui_create_bookmark_menu();
	GSimpleActionGroup *bookmark_actions = menu.actions;
	check(G_IS_SIMPLE_ACTION_GROUP(bookmark_actions), "lazy bookmark popup missing");
	check(bookmark_actions &&
	      g_action_group_has_action(G_ACTION_GROUP(bookmark_actions), "insertar") &&
	      g_action_group_has_action(G_ACTION_GROUP(bookmark_actions), "en-pestana"),
	      "lazy bookmark popup fields missing");
	check(bookmark_actions &&
	      !g_action_group_get_action_enabled(G_ACTION_GROUP(bookmark_actions), "editar"),
	      "bookmark row actions enabled without a selection");
	GVariant *crossref = bookmark_actions ? g_action_group_get_action_state(
	    G_ACTION_GROUP(bookmark_actions), "referencias") : NULL;
	check(crossref && g_variant_get_boolean(crossref) == (settings.crossref_popup != 0),
	      "lazy bookmark popup lost cross-reference setting");
	GVariant *colorize = bookmark_actions ? g_action_group_get_action_state(
	    G_ACTION_GROUP(bookmark_actions), "colorear") : NULL;
	check(colorize && g_variant_get_boolean(colorize) == (settings.tag_colorize != 0),
	      "lazy bookmark popup lost colour setting");
	if (crossref) g_variant_unref(crossref);
	if (colorize) g_variant_unref(colorize);
	gui_create_bookmark_menu();
	check(bookmark_actions == menu.actions, "bookmark popup rebuilt");
	check(GTK_IS_WINDOW(widgets.app), "main window was not created");
	check(gtk_widget_get_visible(widgets.app), "main window was not shown");
	check(gtk_widget_get_realized(widgets.app), "main window was not realized");
	check(gtk_widget_get_mapped(widgets.app), "main window was not mapped");
	/* GTK 4 dropped show_all()/no-show-all along with it: gtk_widget_show()
	 * no longer cascades to children, so the hazard these checks used to
	 * guard against (a startup-hidden panel resurrected by some ancestor's
	 * blanket show) cannot happen any more. The check that still matters
	 * is that the panel is actually hidden. */
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
		/* GTK4-PORT-101 step 2: the history drop-down is a GMenu
		 * (commands and entries sections) over «historial» actions. */
		{
			GMenuModel *history = main_tab_history_menu_model(cur_passage_tab);
			check(g_menu_model_get_n_items(history) == 2,
			      "history menu lacks its two sections");
			GMenuModel *commands = g_menu_model_get_item_link(history, 0,
									 G_MENU_LINK_SECTION);
			check(commands && g_menu_model_get_n_items(commands) == 2,
			      "history menu commands missing");
			g_clear_object(&commands);
			g_object_unref(history);
			GActionGroup *actions = gui_widget_get_action_group(
			    navbar_versekey.button_history_menu, "historial");
			check(actions && g_action_group_has_action(actions, "ir") &&
			      g_action_group_has_action(actions, "limpiar") &&
			      g_action_group_has_action(actions, "a-lista"),
			      "history actions missing");
		}
		/* The Bible-version picker is a sectioned GMenu whose string-state
		 * radio action follows the module currently shown. */
		GMenuModel *versions = gtk_menu_button_get_menu_model(
		    GTK_MENU_BUTTON(widgets.combo_bible_version));
		check(versions && g_menu_model_get_n_items(versions) > 0,
		      "Bible-version menu has no language sections");
		GActionGroup *version_actions = gui_widget_get_action_group(
		    widgets.combo_bible_version, "version");
		check(version_actions &&
		      g_action_group_has_action(version_actions, "elegir"),
		      "Bible-version action missing");
		GVariant *version_state = version_actions
					      ? g_action_group_get_action_state(
						    version_actions, "elegir")
					      : NULL;
		check(version_state &&
		      !g_strcmp0(g_variant_get_string(version_state, NULL),
				 settings.MainWindowModule),
		      "Bible-version state lost the current module");
		g_clear_pointer(&version_state, g_variant_unref);
		main_navbar_versekey_spin_verse(navbar_versekey, 1);
		main_navbar_versekey_spin_verse(navbar_versekey, 0);
		main_navbar_versekey_spin_chapter(navbar_versekey, 1);
		main_navbar_versekey_spin_chapter(navbar_versekey, 0);
		navigation_checks += 5;
		check_bookmark_routes();
		check_note_store();
		check_foreign_verse_note();
		check_notes_features();
        gchar *status = main_backend_status();
        check(strstr(status, "SQLite") != NULL && strstr(status, "SWORD") != NULL,
              "mixed backend status missing");
        g_free(status);
		check_menu_bar();
		check_word_cloud();
		check_word_cloud_dialog();
		check_builder_dialog_opens("preferences", gui_setup_preferences_dialog);
		check_builder_dialog_opens("search", gui_create_search_dialog);
		check_search_dialog_lists();
		check_memorizacion_dialog();
		check_install_dialog();
		check_plans_dialog();
		check_dictionary_key_list();
		check_sqlite_module_manager();
		check_file_chooser_fits();
		/* GTK4-PORT-101 step 2: verse tools are a GMenu popover over
		 * «versiculo» actions. */
		{
			GtkWidget *tools = gui_verse_tools_popup("John 3:16");
			check(GTK_IS_POPOVER(tools), "verse tools popover not shown");
			GActionGroup *versiculo = gui_widget_get_action_group(
			    gtk_window_get_child(GTK_WINDOW(widgets.app)), "versiculo");
			check(versiculo && g_action_group_has_action(versiculo, "interlineal") &&
			      g_action_group_has_action(versiculo, "comparar") &&
			      g_action_group_has_action(versiculo, "xrefs"),
			      "verse tools actions missing");
			if (tools)
				gtk_popover_popdown(GTK_POPOVER(tools));
		}
		/* GTK4-PORT-101 step 2: parallel module options are a GMenu
		 * over stateful «paralelo» actions that mirror the settings. */
		{
			GMenu *options = g_menu_new();
			GSimpleActionGroup *actions = g_simple_action_group_new();
			main_parallel_options_menu(options, G_ACTION_MAP(actions));
			check(g_menu_model_get_n_items(G_MENU_MODEL(options)) >= 15,
			      "parallel options menu incomplete");
			GVariant *strongs = g_action_group_get_action_state(
			    G_ACTION_GROUP(actions), "op0");
			check(strongs && g_variant_get_boolean(strongs) ==
					     (settings.parallel_strongs != 0),
			      "parallel option state does not follow settings");
			GVariant *variants = g_action_group_get_action_state(
			    G_ACTION_GROUP(actions), "variantes");
			check(variants && g_variant_is_of_type(variants, G_VARIANT_TYPE_STRING),
			      "parallel textual variants action missing");
			if (strongs) g_variant_unref(strongs);
			if (variants) g_variant_unref(variants);
			g_object_unref(actions);
			g_object_unref(options);
		}
		/* GTK4-PORT-101 step 2: the book editor's tree menu is a GMenu
		 * over «arbol» actions (a missing book just leaves it empty). */
		{
			EDITOR editor = { 0 };
			editor.module = (gchar *)"NoSuchBook";
			GtkWidget *tree = gui_create_editor_tree(&editor);
			g_object_ref_sink(tree);
			GActionGroup *arbol = gui_widget_get_action_group(tree, "arbol");
			check(arbol && g_action_group_has_action(arbol, "hijo") &&
			      g_action_group_has_action(arbol, "hermano") &&
			      g_action_group_has_action(arbol, "quitar") &&
			      g_action_group_has_action(arbol, "editar"),
			      "book editor tree actions missing");
			gui_widget_destroy(tree);
			g_object_unref(tree);
		}
        check_sqlite_parallel();
		/* Keys of a Bible only SQLite holds resolve through SQLite, even
		 * with SWORD running beside it for commentaries. */
		{
			gchar *valid = main_get_valid_key("OtherBible", "John 3:17");
			check(valid && strstr(valid, "3:17"), "SQLite-only key resolves through SQLite");
			g_free(valid);
			check(main_is_Bible_key("OtherBible", "John 3:17"),
			      "SQLite-only Bible key recognised");
		}
		GList *references = main_parse_verse_list("FakeBible", "John 3:16-17", "John 3:16");
		check(g_list_length(references) == 2, "SQLite sidebar range resolves every verse");
		g_list_free_full(references, g_free);
		DIALOG_DATA *dialog = main_dialogs_open("OtherBible", "John 3:16", FALSE);
		check(dialog != NULL, "SQLite-only Bible opens in a separate window");
		if (dialog) {
			gchar url[] = "sword://OtherBible/John 3:17";
			main_dialogs_url_handler(dialog, url, TRUE);
			const gchar *osis = main_get_osisref_from_key("OtherBible", dialog->key);
			check(!g_strcmp0(osis, "John.3.17"), "SQLite dialog navigation follows its own module");
			g_free((gpointer)osis);
			gui_widget_destroy(dialog->dialog);
		}
	} else {
		check(FALSE, "smoke fixture did not provide a Bible module");
	}

	gtk_widget_show(widgets.app);
	g_idle_add(hide_panels, NULL);
	return G_SOURCE_REMOVE;
}

void
gtk_lifecycle_smoke_schedule(void)
{
	if (requested)
		g_idle_add(exercise_application, NULL);
}
