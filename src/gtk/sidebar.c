/*
 * Xiphos Bible Study Tool
 * sidebar.c - create and maintain the new sidebar bar
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#include "gui/widget_helpers.h"
#include "editor/editor.h"

#include "gui/pulpito.h"
#include "gui/sidebar.h"
#include "gui/table_helpers.h"
#include "gui/bookmarks_treeview.h"
#include "gui/dialog.h"
#include "gui/export_bookmarks.h"
#include "gui/utilities.h"
#include "gui/about_modules.h"
#include "gui/main_window.h"
#include "gui/xiphos.h"
#include "gui/bibletext_dialog.h"
#include "gui/commentary_dialog.h"
#include "gui/dictlex_dialog.h"
#include "gui/gbs_dialog.h"
#include "gui/widgets.h"
#include "gui/search_sidebar.h"
#include "gui/tabbed_browser.h"
#include "gui/search_dialog.h"
#include "gui/instalar_biblias.h"

#include "xiphos_html/xiphos_html.h"

#include "main/sword.h"
#include "main/settings.h"
#include "main/lists.h"
#include "main/prayerlists.h"
#include "main/previewer.h"
#include "main/sidebar.h"
#include "main/url.hh"
#include "main/xml.h"
#include "main/module_dialogs.h"
#include "main/biblesync_glue.h"
#include "gui/parallel_dialog.h"

#include "gui/debug_glib_null.h"
void gui_parallel_tab_activate(gpointer menuitem, gpointer user_data);

SIDEBAR sidebar;
static GtkWidget *button_bookmarks;
static GtkWidget *button_search;
static GtkWidget *button_v_lists;
static GtkWidget *button_modules;
static gchar *buf_module;
GList *list_of_verses;
GListStore *model_verselist;
gboolean is_search_result;

extern gboolean shift_key_pressed;
extern gboolean initialized;

static void create_menu_modules(void);
static void create_menu_percomm_mod(void);
static void module_menu_popup(GMenu *menu);
static void prayerlist_menu_popup(gboolean for_module);
static void on_export_verselist_activate(GSimpleAction *action,
					 GVariant *parameter, gpointer user_data);



/******************************************************************************
 * Name
 *   on_notebook_switch_page
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_notebook_switch_page(GtkNotebook *notebook,
 *				    GtkNotebookPage *page,
 *                                  guint page_num,
 *                                  gpointer user_data)
 *
 * Description
 *   sets the sidebar menu button label to the current page
 *
 * Return value
 *   void
 */
static void on_notebook_switch_page(GtkNotebook *notebook,
				    gpointer arg,
				    guint page_num, gpointer user_data)
{
	switch (page_num) {
	case 0:
		gui_toggle_set_active(GTK_WIDGET(button_v_lists), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_bookmarks), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_search), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_modules), TRUE);
		break;

	case 1:
		gui_toggle_set_active(GTK_WIDGET(button_v_lists), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_bookmarks), TRUE);
		gui_toggle_set_active(GTK_WIDGET(button_search), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_modules), FALSE);
		break;

	case 2:
		gui_toggle_set_active(GTK_WIDGET(button_v_lists), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_bookmarks), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_search), TRUE);
		gui_toggle_set_active(GTK_WIDGET(button_modules), FALSE);
		break;

	case 3:
		gui_toggle_set_active(GTK_WIDGET(button_v_lists), TRUE);
		gui_toggle_set_active(GTK_WIDGET(button_bookmarks), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_search), FALSE);
		gui_toggle_set_active(GTK_WIDGET(button_modules), FALSE);
		break;
	}
}


/******************************************************************************
 * Name
 *   gui_set_sidebar_program_start
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void gui_set_sidebar_program_start(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_set_sidebar_program_start(void)
{
	/*
	 *  show hide shortcut bar - set to options setting
	 */
	if (settings.showshortcutbar) {
		gtk_widget_show(widgets.shortcutbar);
		gtk_paned_set_position(GTK_PANED(widgets.epaned),
				       settings.sidebar_width);
	}
	else {
		gtk_widget_hide(widgets.shortcutbar);
		gtk_paned_set_position(GTK_PANED(widgets.epaned), 1);
	}
}

/******************************************************************************
 * Name
 *  gui_sidebar_showhide
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void gui_sidebar_showhide(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_sidebar_showhide(void)
{
	GtkAllocation allocation;
	if (!settings.docked) {
		gtk_window_present(GTK_WINDOW(widgets.dock_sb));
		return;
	}

	if (settings.showshortcutbar) {
		xml_set_value("Xiphos", "misc", "show_sidebar", "0");
		settings.showshortcutbar = FALSE;
		gtk_widget_hide(widgets.shortcutbar);
		sync_windows();
		gtk_widget_get_allocation(GTK_WIDGET(widgets.vpaned),
					  &allocation);
		settings.biblepane_width = allocation.width;

	} else {
		xml_set_value("Xiphos", "misc", "show_sidebar", "1");
		settings.showshortcutbar = TRUE;
		gtk_paned_set_position(GTK_PANED(widgets.epaned),
				       settings.sidebar_width);
		gtk_widget_show(widgets.shortcutbar);
		sync_windows();
		gtk_widget_get_allocation(GTK_WIDGET(widgets.vpaned),
					  &allocation);
		settings.biblepane_width = allocation.width;
	}

	/* mantener el botón de la cabecera en sync sin importar qué haya
	 * disparado el cambio (menú, Ctrl+S, o una búsqueda que abre el
	 * panel solo). on_sidebar_toggle_button_toggled() ya se protege
	 * contra el reingreso comparando contra settings.showshortcutbar
	 * (que ya quedó actualizado arriba), así que no hace falta
	 * bloquear la señal. */
	if (widgets.sidebar_toggle_button)
		gui_toggle_set_active(
		    GTK_WIDGET(widgets.sidebar_toggle_button),
		    settings.showshortcutbar);
	gui_schedule_bible_text_reflow(FALSE);
}

/******************************************************************************
 * Name
 *   on_modules_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_modules_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_modules_activate(GtkToggleButton *button,
				gpointer user_data)
{
	if (gui_toggle_get_active(button)) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar),
					      0);
	}
}

/******************************************************************************
 * Name
 *   on_bookmarks_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_bookmarks_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_bookmarks_activate(GtkToggleButton *button,
				  gpointer user_data)
{
	if (gui_toggle_get_active(button)) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar),
					      1);
	}
}

/******************************************************************************
 * Name
 *   on_search_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_search_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_search_activate(GtkToggleButton *button,
			       gpointer user_data)
{
	if (gui_toggle_get_active(button)) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar),
					      2);
		gtk_widget_grab_focus(ss.entrySearch);
	}
}

/******************************************************************************
 * Name
 *   on_search_results_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_search_results_activate (gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */
static void on_search_results_activate(GtkToggleButton *button,
				       gpointer user_data)
{
	if (gui_toggle_get_active(button)) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_sidebar),
					      3);
	}
}


/******************************************************************************
 * Name
 *   on_modules_list_button_release
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   gboolean on_modules_list_button_release(GtkWidget *widget,
 *                           GuiButtonEvent  *event, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_modules_list_button_release(GtkWidget *widget,
					       GuiButtonEvent *event,
					       gpointer user_data)
{
	ElimRow *selected;
	gchar *mod = NULL;
	gchar *caption = NULL;

	/* the row under the pointer, which is not always the picked one */
	selected = elim_table_row_at_point(sidebar.module_list, event->x, event->y);
	if (!selected)
		return FALSE;
	caption = *elim_row_get_string(selected, 2) ? g_strdup(elim_row_get_string(selected, 2)) : NULL;
	mod = *elim_row_get_string(selected, 3) ? g_strdup(elim_row_get_string(selected, 3)) : NULL;

	/* a click on a folder opens or closes it, except on its arrow, which
	 * does that by itself */
	if (!elim_tree_point_on_expander(sidebar.module_list, event->x, event->y)) {
		if (elim_tree_row_expanded(sidebar.module_list, selected))
			elim_tree_collapse_row(sidebar.module_list, selected);
		else
			elim_tree_expand_row(sidebar.module_list, selected, FALSE);
	}

	switch (event->button) {
	case 1:
		main_mod_treeview_button_one(selected);
		break;
	case 2:
		if (mod && (g_utf8_collate(mod, _("Parallel View"))) && (g_utf8_collate(mod, _("Standard View"))))
			gui_open_module_in_new_tab(mod);
		break;
	case 3:
		if (mod && !g_utf8_collate(mod, _("Parallel View"))) {
			GMenu *par_menu = g_menu_new();

			g_menu_append(par_menu, _("Abrir en ventana aparte"),
				      "modulo.paralelo-separar");
			g_menu_append(par_menu, _("Abrir en pestaña nueva"),
				      "modulo.paralelo-pestana");
			module_menu_popup(par_menu);
			g_free(mod);
			g_free(caption);
			return FALSE;
		}
		if (mod && (main_get_mod_type(mod) == PERCOM_TYPE)) {
			buf_module = mod;
			create_menu_percomm_mod();
		g_free(caption);
		return FALSE;
	}

		if (mod && (g_utf8_collate(mod, _("Parallel View"))) && (g_utf8_collate(mod, _("Standard View")))
			&& (main_get_mod_type(mod) != PRAYERLIST_TYPE)) {
		buf_module = mod;
		create_menu_modules();
			/*gtk_menu_popup(GTK_MENU(sidebar.menu_modules),
			   NULL, NULL, NULL, NULL,
			   0, gtk_get_current_event_time()); */
			g_free(caption);
			return FALSE;
		}
		if (caption &&
		    (!g_utf8_collate(caption, _("Prayer List/Journal")))) {
			gui_menu_prayerlist_popup(NULL, NULL);
			g_free(mod);
			return FALSE;
		}
		if (mod && (main_get_mod_type(mod) == PRAYERLIST_TYPE)) {
			buf_module = mod;
			prayerlist_menu_popup(TRUE);
			g_free(caption);
			return FALSE;
		}
		if (!mod && caption) {
			GMenu *group_menu = g_menu_new();
			GMenuItem *item;
			static const char *labels[] = {
				N_("Category, then Language"),
				N_("Category only"),
				N_("Language, then Category"),
			};

			for (gint i = 0; i < 3; i++) {
				item = g_menu_item_new(_(labels[i]), NULL);
				g_menu_item_set_action_and_target_value(
				    item, "modulo.agrupar", g_variant_new_int32(i));
				g_menu_append_item(group_menu, item);
				g_object_unref(item);
			}
			module_menu_popup(group_menu);
			g_free(caption);
			return FALSE;
		}

		break;
	}
	g_free(caption);
	g_free(mod);
	return FALSE;
}

/******************************************************************************
 * Name
 *   gui_verselist_button_release_event
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   gboolean gui_verselist_button_release_event(GtkWidget *widget,
 *                          GuiButtonEvent  *event, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

gboolean gui_verselist_button_release_event(GtkWidget *widget,
					    GuiButtonEvent *event,
					    gpointer user_data)
{
	ElimRow *selected;
	gchar *key = NULL;
	gchar *text = NULL;

	selected = elim_table_get_selected(sidebar.results_list);
	if (!selected)
		return FALSE;

	key = g_strdup(elim_row_get_string(selected, 0));
	if (!*key) {
		g_free(key);
		return FALSE;
	}

	if (event) {
		switch (event->button) {
		case 2:
			gui_open_passage_in_new_tab(key);
		/* fall through... */

		case 3: /* button 3 is no-op */
			return FALSE;

		default: /* button 1 handled below */
			break;
		}
	}
	// UTF-8 workaround (domcox)
	// main_get_search_results_text (renderText) doesn't render
	// non-ascii chars saved in numeric character reference format
	// in personal notes.
	// Notes are now saved in utf8 encoding (Xiphos > 2.2.2.1)
	if (main_get_mod_type(settings.sb_search_mod) == PERCOM_TYPE)
		text = main_get_raw_text(settings.sb_search_mod, key);
	else
		text =
		    main_get_search_results_text(settings.sb_search_mod,
						 key);

	if (text) {
		settings.displaySearchResults = TRUE;
		main_entry_display(settings.show_previewer_in_sidebar ? sidebar.html_viewer_widget : widgets.html_previewer_text, //sidebar.html_widget,
				   settings.sb_search_mod, text, key,
				   TRUE);
		settings.displaySearchResults = FALSE;
		g_free(text);
		gtk_widget_grab_focus(sidebar.results_list);
	}

	g_free(key);
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_treeview_button_press_event
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   gboolean on_treeview_button_press_event(GtkWidget *widget,
 *                           GuiButtonEvent  *event, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_treeview_button_press_event(GtkWidget *widget,
					       GuiButtonEvent *event,
					       gpointer user_data)
{
	ElimRow *selected;
	gchar *key = NULL;

	selected = elim_table_get_selected(sidebar.results_list);
	if (!selected)
		return FALSE;

	key = g_strdup(elim_row_get_string(selected, 0));
	if (!*key) {
		g_free(key);
		return FALSE;
	}

	if (event->n_press == 2) {
		/* The rows are native to the module the list was built for
		 * -- the Bible that published the cross reference, or the
		 * module that was searched -- which is not necessarily the
		 * Bible on screen. A module-less URI navigates whatever
		 * Bible is selected, so the key has to be carried into its
		 * numbering first; the text is never reparsed there. */
		gchar *main_key = main_bible_key_for_uri(settings.sb_search_mod,
							 key);
		if (main_key) {
			if (verse_selected)
				g_free(verse_selected);
			verse_selected = g_strdup_printf("sword:///%s",
							 main_key);
			main_url_handler(verse_selected, TRUE);
			g_free(main_key);
		} else {
			/* No counterpart in the Bible on screen: say so and
			 * stay put, rather than navigate to whatever the
			 * same text happens to mean there. */
			main_warn_reference_unmapped(key,
						     settings.MainWindowModule);
		}
	}
	switch (event->button) {
	case 3:
		gui_sidebar_results_popup(sidebar.results_list);
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_save_list_as_bookmarks_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_save_list_as_bookmarks_activate (gpointer menuitem,
 *                                       gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */
/*
G_MODULE_EXPORT static void on_save_list_as_bookmarks_activate(gpointer menuitem,
					       gpointer user_data)
{
	gui_verselist_to_bookmarks(list_of_verses);
}
*/

/******************************************************************************
 * Name
 *   on_open_in_dialog_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_open_in_dialog_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void
on_open_in_dialog_activate(gpointer menuitem, gpointer user_data)
{
	int mod_type = main_get_mod_type(buf_module);

	if ((mod_type == PERCOM_TYPE) || (mod_type == PRAYERLIST_TYPE)) {
		gchar *dialog_text =
		    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s",
				    _("Open module in editor?"),
				    _("If no, it will open for display only."));

		if (gui_yes_no_dialog(dialog_text, NULL)) {
			if (mod_type == PERCOM_TYPE)
				editor_create_new((gchar *)buf_module,
						  (gchar *)
						  settings.currentverse,
						  NOTE_EDITOR);
			else
				editor_create_new((gchar *)buf_module,
						  "0", BOOK_EDITOR);
		} else {
			if (mod_type == PERCOM_TYPE)
				main_dialogs_open(buf_module,
						  (gchar *) settings.currentverse,
						  FALSE);
			else
				main_dialogs_open(buf_module, NULL, FALSE);
		}
		g_free(dialog_text);
	} else
		main_dialogs_open(buf_module, NULL, FALSE);

	g_free(buf_module);
	buf_module = NULL;
}

/******************************************************************************
 * Name
 *   on_open_in_tab_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_open_in_tab_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void
on_open_in_tab_activate2(gpointer menuitem, gpointer user_data)
{
	gui_open_module_in_new_tab(buf_module);
	g_free(buf_module);
	buf_module = NULL;
}

/******************************************************************************
 * Name
 *   on_about2_activate
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void on_about2_activate(gpointer menuitem, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void
on_about2_activate(gpointer menuitem, gpointer user_data)
{
	gui_display_about_module_dialog(buf_module);
	g_free(buf_module);
	buf_module = NULL;
}

G_MODULE_EXPORT void
on_toggle_favorite_activate(gpointer menuitem, gpointer user_data)
{
	module_toggle_favorite(buf_module);
	g_free(buf_module);
	buf_module = NULL;
}

G_MODULE_EXPORT void
on_hide_module_activate(gpointer menuitem, gpointer user_data)
{
	module_toggle_hidden(buf_module);
	g_free(buf_module);
	buf_module = NULL;
}

G_MODULE_EXPORT void
on_save_list_as_a_single_bookmark_activate(GSimpleAction *action,
					   GVariant *parameter,
					   gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	gui_verselist_to_bookmarks(list_of_verses, TRUE);
}

G_MODULE_EXPORT void
on_save_list_as_a_series_of_bookmarks_activate(GSimpleAction *action,
					       GVariant *parameter,
					       gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	gui_verselist_to_bookmarks(list_of_verses, FALSE);
}

G_MODULE_EXPORT void
on_populate_verse_list_activate(GSimpleAction *action, GVariant *parameter,
				gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	GS_DIALOG *info = gui_new_dialog();

	info->stock_icon = g_strdup("dialog-warning");

	info->label_top = g_strdup(_("Paste verse references"));
	info->text1 = g_strdup("");
	info->label1 = _("List:");
	info->ok = TRUE;

	(void)gui_gs_dialog(info);

	main_display_verse_list_in_sidebar(settings.currentverse, settings.MainWindowModule,
					   info->text1);

	g_free(info->label_top);
	g_free(info->text1);
	g_free(info);
}

G_MODULE_EXPORT void
on_send_list_via_biblesync_activate(GSimpleAction *action,
				    GVariant *parameter, gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	if (biblesync_active_xmit_allowed()) {
		GList *verse;
		GString *vlist = g_string_new("");
		gboolean first = TRUE;

		for (verse = list_of_verses; verse;
		     verse = g_list_next(verse)) {
			RESULTS *item = (RESULTS *)verse->data;
			if (!first) {
				vlist = g_string_append_c(vlist, ';');
			}
			vlist = g_string_append(vlist, (char *)
						main_get_osisref_from_key(settings.MainWindowModule,
									  item->key));
			first = FALSE;
		}
		biblesync_transmit_verse_list(settings.MainWindowModule,
					      vlist->str);
		g_string_free(vlist, TRUE);
	} else {
		gui_generic_warning(_("BibleSync is not active for transmit."));
	}
}

G_MODULE_EXPORT void
on_preload_history_from_verse_list_activate(GSimpleAction *action,
					    GVariant *parameter,
					    gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	GList *verse;

	for (verse = list_of_verses; verse; verse = g_list_next(verse)) {
		RESULTS *item = (RESULTS *)verse->data;
		main_fake_tab_history_item((char *)
					   main_getShortText(item->key));
	}
}

static void
on_export_verselist_activate(GSimpleAction *action, GVariant *parameter,
			     gpointer user_data)
{
	(void)action;
	(void)parameter;
	(void)user_data;
	gui_export_bookmarks_dialog((is_search_result
					 ? SEARCH_RESULTS_EXPORT
					 : VERSE_LIST_EXPORT),
				    list_of_verses);
}

/******************************************************************************
 * Name
 *   create_results_menu
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   GtkWidget* create_results_menu (void)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget*
 */

static void create_results_menu(void)
{
	if (sidebar.results_actions)
		return;

	const GActionEntry entries[] = {
		{ "guardar-uno", on_save_list_as_a_single_bookmark_activate,
		  NULL, NULL, NULL, { 0 } },
		{ "guardar-varios", on_save_list_as_a_series_of_bookmarks_activate,
		  NULL, NULL, NULL, { 0 } },
		{ "rellenar", on_populate_verse_list_activate,
		  NULL, NULL, NULL, { 0 } },
		{ "historial", on_preload_history_from_verse_list_activate,
		  NULL, NULL, NULL, { 0 } },
		{ "biblesync", on_send_list_via_biblesync_activate,
		  NULL, NULL, NULL, { 0 } },
		{ "exportar", on_export_verselist_activate,
		  NULL, NULL, NULL, { 0 } },
	};
	sidebar.results_actions = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(sidebar.results_actions),
					entries, G_N_ELEMENTS(entries), NULL);
	gui_sidebar_results_menu_set_enabled(FALSE);

	sidebar.results_menu = g_menu_new();
	GMenu *bookmarks = g_menu_new();
	g_menu_append(bookmarks, _("Guardar la lista como un marcador"),
		      "lista.guardar-uno");
	g_menu_append(bookmarks, _("Guardar la lista como varios marcadores"),
		      "lista.guardar-varios");
	g_menu_append_section(sidebar.results_menu, NULL, G_MENU_MODEL(bookmarks));
	g_object_unref(bookmarks);

	GMenu *list = g_menu_new();
	g_menu_append(list, _("Rellenar lista de versículos"), "lista.rellenar");
	g_menu_append(list, _("Cargar historial desde la lista"), "lista.historial");
	g_menu_append_section(sidebar.results_menu, NULL, G_MENU_MODEL(list));
	g_object_unref(list);

	GMenu *sharing = g_menu_new();
	g_menu_append(sharing, _("Enviar lista por BibleSync"), "lista.biblesync");
	g_menu_append(sharing, _("Exportar lista"), "lista.exportar");
	g_menu_append_section(sidebar.results_menu, NULL, G_MENU_MODEL(sharing));
	g_object_unref(sharing);

	gui_widget_insert_action_group(sidebar.results_list, "lista",
				       G_ACTION_GROUP(sidebar.results_actions));
}

G_MODULE_EXPORT void
on_simple_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_basic_create();
}

G_MODULE_EXPORT void
on_subject_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_subject_create();
}

G_MODULE_EXPORT void
on_monthly_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_monthly_create();
}

G_MODULE_EXPORT void
on_journal_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_journal_create();
}

G_MODULE_EXPORT void
on_outlined_topic_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_outlined_topic_create();
}

G_MODULE_EXPORT void
on_book_chapter_activate(gpointer menuitem, gpointer user_data)
{
	main_prayerlist_book_chapter_create();
}

void on_edit_activate(gpointer menuitem, gpointer user_data)
{
		editor_create_new(buf_module, "0", BOOK_EDITOR);
}

/* Llevar al púlpito el bosquejo sobre el que se hizo clic derecho. */
G_MODULE_EXPORT void
on_abrir_en_pulpito_activate(gpointer menuitem, gpointer user_data)
{
	if (buf_module && *buf_module)
		gui_pulpito_abrir(buf_module);
}

G_MODULE_EXPORT void
on_edit_percomm_activate(gpointer menuitem, gpointer user_data)
{
	editor_create_new(buf_module,
			  (gchar *)settings.currentverse,
			  NOTE_EDITOR);
	g_free(buf_module);
	buf_module = NULL;
}

/* The module tree's context menus.
 *
 * GTK4-PORT-101: they were GtkMenus from xi-menus-popup.gtkbuilder; now
 * one «modulo» action group on the tree (the handlers above and below
 * run through thin wrappers) and a GMenu built for each popup, since the
 * labels and the items shown depend on the module. */
#define MODULE_ACTION(name, handler)                                           \
	static void name(GSimpleAction *action, GVariant *parameter,           \
			 gpointer data)                                       \
	{                                                                      \
		(void)action;                                                  \
		(void)parameter;                                               \
		(void)data;                                                    \
		handler(NULL, NULL);                                           \
	}

MODULE_ACTION(module_tab_action, on_open_in_tab_activate2)
MODULE_ACTION(module_dialog_action, on_open_in_dialog_activate)
MODULE_ACTION(module_pulpito_action, on_abrir_en_pulpito_activate)
MODULE_ACTION(module_about_action, on_about2_activate)
MODULE_ACTION(module_favorite_action, on_toggle_favorite_activate)
MODULE_ACTION(module_hide_action, on_hide_module_activate)
MODULE_ACTION(module_edit_note_action, on_edit_percomm_activate)
MODULE_ACTION(module_edit_action, on_edit_activate)
MODULE_ACTION(list_simple_action, on_simple_activate)
MODULE_ACTION(list_subject_action, on_subject_activate)
MODULE_ACTION(list_monthly_action, on_monthly_activate)
MODULE_ACTION(list_journal_action, on_journal_activate)
MODULE_ACTION(list_outlined_action, on_outlined_topic_activate)
MODULE_ACTION(list_book_action, on_book_chapter_activate)

static void parallel_detach_action(GSimpleAction *action, GVariant *parameter,
				   gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	gui_undock_parallel_page();
}

static void parallel_tab_changed(GSimpleAction *action, GVariant *state,
				 gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	gui_parallel_tab_activate(NULL,
				  GINT_TO_POINTER(g_variant_get_boolean(state)));
}

static void grouping_changed(GSimpleAction *action, GVariant *state,
			     gpointer data)
{
	gint mode = g_variant_get_int32(state);
	gchar buf[4];

	(void)data;
	g_simple_action_set_state(action, state);
	g_snprintf(buf, sizeof(buf), "%d", mode);
	xml_set_value("Xiphos", "modules", "grouping", buf);
	settings.module_tree_grouping = mode;
	main_load_module_tree(sidebar.module_list);
}

/* The group, fresh for each popup so the stateful actions follow the
 * settings. */
static void install_module_actions(void)
{
	static const GActionEntry entries[] = {
		{ "pestana", module_tab_action, NULL, NULL, NULL, { 0 } },
		{ "dialogo", module_dialog_action, NULL, NULL, NULL, { 0 } },
		{ "pulpito", module_pulpito_action, NULL, NULL, NULL, { 0 } },
		{ "acerca", module_about_action, NULL, NULL, NULL, { 0 } },
		{ "favorito", module_favorite_action, NULL, NULL, NULL, { 0 } },
		{ "ocultar", module_hide_action, NULL, NULL, NULL, { 0 } },
		{ "editar-nota", module_edit_note_action, NULL, NULL, NULL, { 0 } },
		{ "editar", module_edit_action, NULL, NULL, NULL, { 0 } },
		{ "lista-simple", list_simple_action, NULL, NULL, NULL, { 0 } },
		{ "lista-tema", list_subject_action, NULL, NULL, NULL, { 0 } },
		{ "lista-mensual", list_monthly_action, NULL, NULL, NULL, { 0 } },
		{ "lista-diario", list_journal_action, NULL, NULL, NULL, { 0 } },
		{ "lista-esquema", list_outlined_action, NULL, NULL, NULL, { 0 } },
		{ "lista-libro", list_book_action, NULL, NULL, NULL, { 0 } },
		{ "paralelo-separar", parallel_detach_action, NULL, NULL, NULL, { 0 } },
	};
	GSimpleActionGroup *group = g_simple_action_group_new();
	GSimpleAction *action;

	g_action_map_add_action_entries(G_ACTION_MAP(group), entries,
					G_N_ELEMENTS(entries), NULL);
	action = g_simple_action_new_stateful(
	    "paralelo-pestana", NULL, g_variant_new_boolean(settings.showparatab));
	g_signal_connect(action, "change-state",
			 G_CALLBACK(parallel_tab_changed), NULL);
	g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
	g_object_unref(action);
	action = g_simple_action_new_stateful(
	    "agrupar", G_VARIANT_TYPE_INT32,
	    g_variant_new_int32(settings.module_tree_grouping));
	g_signal_connect(action, "change-state", G_CALLBACK(grouping_changed),
			 NULL);
	g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
	g_object_unref(action);
	gui_widget_insert_action_group(sidebar.module_list, "modulo",
				       G_ACTION_GROUP(group));
	g_object_unref(group);
}

static void module_menu_popup(GMenu *menu)
{
	install_module_actions();
	gui_popup_menu_model_at_pointer(G_MENU_MODEL(menu), sidebar.module_list);
	g_object_unref(menu);
}

static void create_menu_modules(void)
{
	GMenu *menu = g_menu_new();

	g_menu_append(menu, _("Abrir en pestaña nueva"), "modulo.pestana");
	g_menu_append(menu, _("Abrir en ventana aparte"), "modulo.dialogo");
	/* Al púlpito solo suben los libros con árbol de puntos; en una
	 * Biblia o un diccionario el punto no pinta nada. */
	if (main_get_mod_type(buf_module) == BOOK_TYPE)
		g_menu_append(menu, _("Abrir en _púlpito"), "modulo.pulpito");
	g_menu_append(menu, _("Acerca de"), "modulo.acerca");
	g_menu_append(menu,
		      module_is_favorite(buf_module) ? _("Remove from Favorites")
						     : _("Add to Favorites"),
		      "modulo.favorito");
	g_menu_append(menu,
		      module_is_hidden(buf_module) ? _("Show this module")
						   : _("Hide this module"),
		      "modulo.ocultar");
	module_menu_popup(menu);
}

static void create_menu_percomm_mod(void)
{
	GMenu *menu = g_menu_new();

	g_menu_append(menu, _("Abrir en pestaña nueva"), "modulo.pestana");
	g_menu_append(menu, _("Abrir en ventana aparte"), "modulo.dialogo");
	g_menu_append(menu, _("Editar"), "modulo.editar-nota");
	g_menu_append(menu, _("Acerca de"), "modulo.acerca");
	module_menu_popup(menu);
}

/* The prayer lists: the kinds to create, or what to do with one. */
static void prayerlist_menu_popup(gboolean for_module)
{
	GMenu *menu = g_menu_new();

	if (for_module) {
		g_menu_append(menu, _("Abrir en pestaña nueva"), "modulo.pestana");
		g_menu_append(menu, _("Abrir en ventana aparte"), "modulo.dialogo");
		g_menu_append(menu, _("Abrir en _púlpito"), "modulo.pulpito");
		g_menu_append(menu, _("Editar"), "modulo.editar");
		g_menu_append(menu, _("Acerca de"), "modulo.acerca");
	} else {
		g_menu_append(menu, _("Simple"), "modulo.lista-simple");
		g_menu_append(menu, _("Tema"), "modulo.lista-tema");
		g_menu_append(menu, _("Mensual"), "modulo.lista-mensual");
		g_menu_append(menu, _("Diario"), "modulo.lista-diario");
		g_menu_append(menu, _("Esquema"), "modulo.lista-esquema");
		g_menu_append(menu, _("Libro/capítulo"), "modulo.lista-libro");
	}
	module_menu_popup(menu);
}

G_MODULE_EXPORT void gui_menu_prayerlist_popup(gpointer menuitem,
					       gpointer user_data)
{
	(void)menuitem;
	(void)user_data;
	prayerlist_menu_popup(FALSE);
}

static void tree_selection_changed_cb(GObject *selection, GParamSpec *pspec,
				      gpointer data)
{
	(void)selection;
	(void)pspec;
	(void)data;
	gui_verselist_button_release_event(NULL, NULL, NULL);
}

static gboolean tree_key_press_cb(GtkWidget *widget,
				  GuiKeyEvent *event, gpointer user_data)
{
	ElimRow *selected;
	gchar *key = NULL;

	selected = elim_table_get_selected(sidebar.results_list);
	if (!selected)
		return FALSE;

	key = g_strdup(elim_row_get_string(selected, 0));
	if (!*key) {
		g_free(key);
		return FALSE;
	}

	if (event) {

		switch (event->keyval) {
		case 0xff0d: /* "65293" */
		case 0xff8d: /* "65421" */
		{
			gchar *url =
			    g_strdup_printf("sword://%s/%s",
					    settings.sb_search_mod,
					    key);
			main_url_handler(url, TRUE);
			g_free(url);
		} break;

		case 0x20: /* "32" */
			gui_open_passage_in_new_tab(key);
			sync_windows();
			break;

		case 0xffe1: /* shift keys */
		case 0xffe2:
			XI_warning(("shift key pressed"));
			shift_key_pressed = TRUE;
			sync_windows();
			break;

		default:
			break;
		}
	}
	g_free(key);

	sync_windows();

	gtk_widget_grab_focus(sidebar.results_list);
	return FALSE;
}

/******************************************************************************
 * Name
 *   create_search_results_page
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   void create_search_results_page(GtkWidget * notebook)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

GMenuModel *gui_sidebar_results_menu(void)
{
	create_results_menu();
	return G_MENU_MODEL(sidebar.results_menu);
}

GActionGroup *gui_sidebar_results_actions(void)
{
	create_results_menu();
	return G_ACTION_GROUP(sidebar.results_actions);
}

void gui_sidebar_results_menu_set_enabled(gboolean enabled)
{
	static const char *const actions[] = {
		"guardar-uno", "guardar-varios", "rellenar",
		"historial", "biblesync", "exportar"
	};

	if (!sidebar.results_actions)
		create_results_menu();
	for (guint i = 0; i < G_N_ELEMENTS(actions); ++i) {
		GAction *action = g_action_map_lookup_action(
		    G_ACTION_MAP(sidebar.results_actions), actions[i]);
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
	}
}

GtkWidget *gui_sidebar_results_popup(GtkWidget *relative)
{
	return gui_popup_menu_model_at_pointer(gui_sidebar_results_menu(), relative);
}

static void create_search_results_page(GtkWidget *notebook)
{
	GtkWidget *scrolledwindow3;
	ElimTextColumn column = elim_text_column(0);
	/* The popup is built on first use. */
	scrolledwindow3 = gtk_scrolled_window_new();
	gtk_widget_show(scrolledwindow3);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolledwindow3, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow3),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *) scrolledwindow3), TRUE);

	/* the keys of the verse list or of the search, one to a row */
	model_verselist = elim_table_new();

	sidebar.results_list = gtk_list_view_new(NULL, NULL);
	elim_table_setup_list(sidebar.results_list, model_verselist, &column);
	gtk_widget_show(sidebar.results_list);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow3), sidebar.results_list);

	gui_widget_on_key_phase(GTK_WIDGET(sidebar.results_list), GTK_PHASE_CAPTURE, (GuiKeyFunc)tree_key_press_cb, NULL, NULL);
	g_signal_connect(elim_table_selection(sidebar.results_list),
			 "notify::selected-item",
			 G_CALLBACK(tree_selection_changed_cb), NULL);
	gui_widget_on_button(GTK_WIDGET(sidebar.results_list), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)gui_verselist_button_release_event, NULL);
	gui_widget_on_button(GTK_WIDGET(sidebar.results_list), GTK_PHASE_CAPTURE, (GuiButtonFunc)on_treeview_button_press_event, NULL, NULL);
}

/******************************************************************************
 * Name
 *   paned_button_release_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean paned_button_release_event(GtkWidget * widget,
 *			GuiButtonEvent * event, gpointer user_data)
 *
 * Description
 *    get and store pane sizes
 *
 * Return value
 *   void
 */

static gboolean paned_button_release_event(GtkWidget *widget,
					   GuiButtonEvent *event,
					   gpointer user_data)
{
	gint panesize;
	gchar layout[80];

	panesize = gtk_paned_get_position(GTK_PANED(widget));

	if (panesize > 15) {
		settings.sidebar_notebook_height = panesize;
		sprintf(layout, "%d", settings.sidebar_notebook_height);
		xml_set_value("Xiphos", "layout",
			      "sidebar_notebook_height", layout);
	}
	return FALSE;
}

void gui_show_previewer_in_sidebar(gint choice)
{
	/*
	 * don't show the previewer because of a state change of
	 * its placement if it's not supposed to be shown at all.
	 * exception: to get the world started properly, which is to
	 * say, if we have not yet initialized everything, we must
	 * go through this setup anyhow.
	 */
	if (initialized && !settings.showpreview)
		return;

	if (choice) {
		gtk_widget_show(widgets.box_side_preview);
		gtk_widget_show(widgets.box_side_preview);
		gtk_widget_hide(widgets.vbox_previewer);
		gtk_paned_set_position(GTK_PANED(widgets.paned_sidebar),
				       settings.sidebar_notebook_height);
	} else {
		gtk_widget_show(widgets.vbox_previewer);
		gtk_widget_hide(widgets.box_side_preview);
		gtk_paned_set_position(GTK_PANED(widgets.vpaned),
				       settings.biblepane_height);
	}
	main_set_previewer_widget(choice);
	main_init_previewer();
}

/******************************************************************************
 * Name
 *   gui_create_sidebar
 *
 * Synopsis
 *   #include "gui/sidebar.h"
 *
 *   GtkWidget *gui_create_sidebar(GtkWidget * paned)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget *
 */

/* Botón "Instalar Biblias" al pie del árbol de módulos -- antes solo
 * se podía llegar al instalador desde el menú Editar o desde el panel
 * Comparar; ahora también está a mano en el panel Módulos, que es
 * donde alguien realmente esperaría encontrarlo. */
static void
on_sidebar_install_bibles_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	gui_instalar_biblias();
}

GtkWidget *gui_create_sidebar(GtkWidget *paned)
{
	GtkWidget *vbox1;
	GtkWidget *scrolledwindow4;
	GtkWidget *vbox_modules_page;
	GtkWidget *btn_install_bibles;
	GtkWidget *scrolledwindow_bm;
	GtkWidget *title_label = NULL;

	GtkWidget *table2;

	UI_VBOX(vbox1, FALSE, 0);
	gtk_widget_add_css_class(vbox1, "elim-sidebar");
	gtk_widget_show(vbox1);

	widgets.paned_sidebar = UI_VPANE();
	gtk_paned_set_start_child(GTK_PANED(paned), widgets.paned_sidebar);
	gtk_paned_set_resize_start_child(GTK_PANED(paned), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(paned), TRUE);
	/* Do not map a hidden sidebar transiently while it is constructed. */
	if (settings.showshortcutbar)
		gtk_widget_show(widgets.paned_sidebar);
	gtk_paned_set_start_child(GTK_PANED(widgets.paned_sidebar), vbox1);
	gtk_paned_set_resize_start_child(GTK_PANED(widgets.paned_sidebar), FALSE);
	gtk_paned_set_shrink_start_child(GTK_PANED(widgets.paned_sidebar), TRUE);
	UI_VBOX(widgets.box_side_preview, FALSE, 0);
	gtk_paned_set_end_child(GTK_PANED(widgets.paned_sidebar), widgets.box_side_preview);
	gtk_paned_set_resize_end_child(GTK_PANED(widgets.paned_sidebar), FALSE);
	gtk_paned_set_shrink_end_child(GTK_PANED(widgets.paned_sidebar), TRUE);
	gui_widget_set_margins(widgets.box_side_preview, 2);
	gui_widget_on_button(GTK_WIDGET(widgets.paned_sidebar), GTK_PHASE_BUBBLE, NULL, (GuiButtonFunc)paned_button_release_event, (gchar *)"paned_sidebar");
	widgets.shortcutbar = widgets.paned_sidebar;


	sidebar.html_viewer_widget =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, SB_VIEWER_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(sidebar.html_viewer_widget,
				     "sidebar-previewer");
	gtk_widget_show(sidebar.html_viewer_widget);
	gui_box_pack(GTK_BOX(widgets.box_side_preview), sidebar.html_viewer_widget, TRUE, TRUE, 0);

/* ---------------------------------------------------------------- */
/* 2x2 button box set: modules/bookmarks/search/vlist */
/* ---------------------------------------------------------------- */

	table2 = gtk_grid_new();
	gtk_widget_show(table2);
	gtk_box_append(GTK_BOX(vbox1), table2);
	gui_widget_set_margins(table2, 2);
	gtk_grid_set_row_spacing(GTK_GRID(table2), 6);
	gtk_grid_set_column_spacing(GTK_GRID(table2), 6);
	gtk_grid_set_row_homogeneous(GTK_GRID(table2), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(table2), TRUE);

	button_bookmarks =
	    gtk_toggle_button_new_with_mnemonic(_("Bookmarks"));
	gtk_widget_show(button_bookmarks);
	gtk_grid_attach(GTK_GRID(table2), button_bookmarks, 1, 0, 1, 1);
	gtk_button_set_has_frame(GTK_BUTTON(button_bookmarks), TRUE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(button_bookmarks), FALSE);

	button_search = gtk_toggle_button_new_with_mnemonic(_("Search"));
	gtk_widget_show(button_search);
	gtk_grid_attach(GTK_GRID(table2), button_search, 0, 1, 1, 1);
	gtk_button_set_has_frame(GTK_BUTTON(button_search), TRUE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(button_search), FALSE);

	button_v_lists =
	    gtk_toggle_button_new_with_mnemonic(_("Verse List"));
	gtk_widget_show(button_v_lists);
	gtk_grid_attach(GTK_GRID(table2), button_v_lists, 1, 1, 1, 1);
	gtk_button_set_has_frame(GTK_BUTTON(button_v_lists), TRUE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(button_v_lists), FALSE);

	button_modules = gtk_toggle_button_new_with_mnemonic(_("Modules"));
	gtk_widget_show(button_modules);
	gtk_grid_attach(GTK_GRID(table2), button_modules, 0, 0, 1, 1);
	gtk_button_set_has_frame(GTK_BUTTON(button_modules), TRUE);
	gtk_widget_set_focus_on_click(GTK_WIDGET(button_modules), FALSE);

	gui_toggle_set_active(GTK_WIDGET(button_modules),
				     TRUE);
	/* ---------------------------------------------------------------- */

	widgets.notebook_sidebar = gtk_notebook_new();
	gtk_widget_show(widgets.notebook_sidebar);

	gui_box_pack(GTK_BOX(vbox1), widgets.notebook_sidebar, TRUE, TRUE, 0);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(widgets.notebook_sidebar),
				   FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(widgets.notebook_sidebar), FALSE);
	gui_widget_set_margins(widgets.notebook_sidebar, 2);

	/* la página de Módulos es un vbox: el árbol (expande) + un botón
	 * fijo abajo para instalar Biblias sin tener que ir al menú
	 * Editar o abrir el panel Comparar primero. */
	UI_VBOX(vbox_modules_page, FALSE, 0);
	gtk_widget_show(vbox_modules_page);
	gtk_notebook_append_page(GTK_NOTEBOOK(widgets.notebook_sidebar), vbox_modules_page, NULL);

	scrolledwindow4 = gtk_scrolled_window_new();
	gtk_widget_show(scrolledwindow4);
	gui_box_pack(GTK_BOX(vbox_modules_page), scrolledwindow4, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow4),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *) scrolledwindow4), TRUE);

	btn_install_bibles = gtk_button_new_with_label(_("Instalar Biblias"));
	gtk_button_set_has_frame(GTK_BUTTON(btn_install_bibles), FALSE);
	gtk_widget_set_tooltip_text(btn_install_bibles,
				    _("Descargar e instalar Biblias de CrossWire, eBible y otras fuentes"));
	gtk_widget_add_css_class(btn_install_bibles, "elim-pill");
	gtk_widget_set_margin_start(btn_install_bibles, 8);
	gtk_widget_set_margin_end(btn_install_bibles, 8);
	gtk_widget_set_margin_top(btn_install_bibles, 4);
	gtk_widget_set_margin_bottom(btn_install_bibles, 6);
	g_signal_connect(btn_install_bibles, "clicked",
			 G_CALLBACK(on_sidebar_install_bibles_clicked), NULL);
	gtk_widget_show(btn_install_bibles);
	gtk_box_append(GTK_BOX(vbox_modules_page), btn_install_bibles);

	sidebar.module_list = gtk_list_view_new(NULL, NULL);
	gtk_widget_show(sidebar.module_list);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow4), sidebar.module_list);

	scrolledwindow_bm = gtk_scrolled_window_new();
	gtk_widget_show(scrolledwindow_bm);
	gtk_notebook_append_page(GTK_NOTEBOOK(widgets.notebook_sidebar), scrolledwindow_bm, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_bm),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_has_frame(GTK_SCROLLED_WINDOW((GtkScrolledWindow *) scrolledwindow_bm), TRUE);

	widgets.bookmark_tree = gui_create_bookmark_tree();
	gtk_widget_show(widgets.bookmark_tree);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolledwindow_bm), widgets.bookmark_tree);

	gui_create_search_sidebar();

	create_search_results_page(widgets.notebook_sidebar);

	main_init_module_tree(sidebar.module_list);

	gui_widget_on_button(GTK_WIDGET(sidebar.module_list), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_modules_list_button_release, NULL);

	g_signal_connect((gpointer)button_bookmarks, "toggled",
			 G_CALLBACK(on_bookmarks_activate), NULL);
	g_signal_connect((gpointer)button_search, "toggled",
			 G_CALLBACK(on_search_activate), NULL);
	g_signal_connect((gpointer)button_v_lists, "toggled",
			 G_CALLBACK(on_search_results_activate), NULL);
	g_signal_connect((gpointer)button_modules, "toggled",
			 G_CALLBACK(on_modules_activate), NULL);
	g_signal_connect((gpointer)widgets.notebook_sidebar,
			 "switch-page",
			 G_CALLBACK(on_notebook_switch_page), title_label);
	return vbox1;
}
