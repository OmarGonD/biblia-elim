/*
 * Xiphos Bible Study Tool
 * menu_popup.c - main window panes and dialogs popup menus
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

#include <gtk/gtk.h>


#include <glib.h>
#include <glib/gstdio.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "xiphos_html/xiphos_html.h"

#include "gui/menu_popup.h"
#include "gui/cipher_key_dialog.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"
#include "gui/bookmark_dialog.h"
#include "gui/export_dialog.h"
#include "gui/barra_busqueda.h"
#include "gui/find_dialog.h"
#include "gui/font_dialog.h"
#include "gui/about_modules.h"
#include "gui/interlineal.h"

#include "main/module_dialogs.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/lists.h"
#include "main/display.hh"
#include "main/search_sidebar.h"
#include "main/mod_mgr.h"
#include "main/url.hh"

#ifdef USE_WEBKIT_EDITOR
#include "editor/webkit_editor.h"
#else
#include "editor/slib-editor.h"
#endif

#include "gui/debug_glib_null.h"

static gchar *menu_mod_name = NULL;
static DIALOG_DATA *dialog = NULL;
static int is_dialog = 0;

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

gint _get_type_mod_list(void)
{
	switch (main_get_mod_type(menu_mod_name)) {
	case TEXT_TYPE:
		return TEXT_DESC_LIST;
		break;
	case COMMENTARY_TYPE:
	case PERCOM_TYPE:
		return COMM_DESC_LIST;
		break;
	case DICTIONARY_TYPE:
		return DICT_DESC_LIST;
		break;
	case BOOK_TYPE:
		return GBS_DESC_LIST;
		break;
	default:
		return -1;
		break;
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

gchar *_get_key(gchar *mod_name)
{
	gchar *key = NULL;

	if (is_dialog)
		return g_strdup(dialog->key);

	switch (main_get_mod_type(mod_name)) {
	case TEXT_TYPE:
	case COMMENTARY_TYPE:
	case PERCOM_TYPE:
		key = g_strdup(settings.currentverse);
		break;
	case DICTIONARY_TYPE:
		key = g_strdup(settings.dictkey);
		break;
	case BOOK_TYPE:
	case PRAYERLIST_TYPE:
		key = g_strdup(settings.book_key);
		break;
	}
	return key;
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

GtkWidget *_get_html(void)
{
	if (is_dialog)
		return dialog->html;
	else
		switch (main_get_mod_type(menu_mod_name)) {
		case TEXT_TYPE:
			return widgets.html_text;
			break;
		case COMMENTARY_TYPE:
		case PERCOM_TYPE:
			return widgets.html_comm;
			break;
		case DICTIONARY_TYPE:
			return widgets.html_dict;
			break;
		case BOOK_TYPE:
		case PRAYERLIST_TYPE:
			return widgets.html_book;
			break;
		}
	return NULL;
}

/******************************************************************************
 * Name
 *   _global_option_main_pane
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *   void _global_option_main_pane(gpointer  menuitem,
			   GBS_DATA * g)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void _global_option_main_pane(gboolean active, const gchar *option)
{
	gchar *key = _get_key(menu_mod_name);
	gchar *mod =
	    (gchar *)(is_dialog ? dialog->mod_name : menu_mod_name);
	XI_message(("module option = %s", option));
	if (key) {
		gchar *url = g_strdup_printf("sword://%s/%s",
					     mod,
					     key);
		main_save_module_options(mod, (gchar *)option, active);
		if (is_dialog) {
			/* show the change */
			main_dialogs_url_handler(dialog, url, TRUE);
		} else {
			/* show the change */
			main_url_handler(url, TRUE);
		}

		g_free(url);
		g_free(key);
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void on_edit_percomm_activate(gpointer menuitem,
				     gpointer user_data)
{
	gchar *key;

	if (is_dialog)
		key = g_strdup(dialog->key);
	else
		key = _get_key(menu_mod_name);
	if (key) {
		XI_message(("\n\npercomm key: %s\n\n", key));
		editor_create_new((gchar *)user_data, (gchar *)key,
				  NOTE_EDITOR);
		g_free(key);
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

static void on_edit_prayerlist_activate(gpointer menuitem,
					gpointer user_data)
{
	XI_message(("settings.book_key: %s", (char *)(is_dialog ? dialog->key : settings.book_key)));
	editor_create_new((gchar *)user_data,
			  (gchar *)(is_dialog ? dialog->key : settings.book_key), BOOK_EDITOR);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_about_activate(gpointer menuitem,
					     gpointer user_data)
{
	gui_display_about_module_dialog((is_dialog ? dialog->mod_name : menu_mod_name));
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_bookmark_activate(gpointer menuitem,
						gpointer user_data)
{
	gchar *key;

	if (is_dialog)
		return;

	if ((key = _get_key(menu_mod_name))) {
		gchar *label = g_strdup_printf("%s, %s",
					       key,
					       menu_mod_name);
		gui_bookmark_dialog(label, menu_mod_name, key);
		g_free(label);
		g_free(key);
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_export_passage_activate(gpointer
							  menuitem,
						      gpointer user_data)
{
	if (is_dialog)
		return;

	gui_export_dialog();
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_print_activate(gpointer menuitem,
					     gpointer user_data)
{
	/* there is some weirdness here, from having eliminated gtkhtml3.
	 * we must understand why this is an interesting conditional.  */
	if (is_dialog) {
		XIPHOS_HTML_PRINT_DOCUMENT((XiphosHtml *)user_data);
	} else {
		XIPHOS_HTML_PRINT_DOCUMENT((XiphosHtml *)user_data);
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_close_activate(gpointer menuitem,
				       gpointer user_data)
{
	/* FIXME */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_copy_activate(gpointer menuitem,
					    gpointer user_data)
{
	/* ditto above comment re: printing */
	if (is_dialog) {
		XIPHOS_HTML_COPY_SELECTION(dialog->html);
	} else {
		XIPHOS_HTML_COPY_SELECTION(_get_html());
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_find_activate(gpointer menuitem,
					    gpointer user_data)
{
	/* En la ventana principal la búsqueda es una franja empotrada, que
	 * no tapa el texto; las ventanas sueltas siguen con su diálogo. */
	if (is_dialog)
		gui_find_dlg(dialog->html, dialog->mod_name, FALSE, NULL);
	else
		gui_barra_busqueda_mostrar(_get_html());
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_popup_font_activate(gpointer menuitem,
					    gpointer user_data)
{
	if (is_dialog) {
		gchar *url = g_strdup_printf("sword://%s/%s",
					     dialog->mod_name,
					     dialog->key);
		gui_set_module_font(dialog->mod_name);
		/* show the change */
		main_dialogs_url_handler(dialog, url, TRUE);
		g_free(url);
	} else {
		gchar *key = _get_key(menu_mod_name);
		if (key) {
			gchar *url = g_strdup_printf("sword://%s/%s",
						     menu_mod_name,
						     key);
			gui_set_module_font(menu_mod_name);
			/* show the change */
			main_url_handler(url, TRUE);
			g_free(url);
			g_free(key);
		}
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_verse_per_line_activate(gpointer
						    menuitem,
						gpointer user_data)
{
	gboolean active = GPOINTER_TO_INT(user_data) != 0;
	(void)menuitem;
	gchar *file = g_strdup_printf("%s/modops.conf",
				      settings.gSwordDir);

	gchar *url = g_strdup_printf("sword://%s/%s",
				     (is_dialog ? dialog->mod_name : settings.MainWindowModule),
				     (is_dialog ? dialog->key : settings.currentverse));

	save_conf_file_item(file, (is_dialog
				       ? dialog->mod_name
				       : settings.MainWindowModule),
			    "style",
			    active ? "verse" : "paragraph");
	if (settings.havebible) {
		if (is_dialog) {
			/* show the change */
			main_dialogs_url_handler(dialog, url, TRUE);
		} else {
			settings.versestyle = active;
			main_url_handler(url, TRUE);
		}
	}
	g_free(url);
	g_free(file);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_words_of_christ_in_red_activate(gpointer
							    menuitem,
							gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Words of Christ in Red"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_strong_s_numbers_activate(gpointer
						      menuitem,
						  gpointer user_data)
{
	(void)menuitem;
	gui_interlineal_set_active(GPOINTER_TO_INT(user_data) != 0);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_morphological_tags_activate(gpointer
							menuitem,
						    gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Morphological Tags"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_footnotes_activate(gpointer menuitem,
					   gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Footnotes"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_greek_accents_activate(gpointer menuitem,
					       gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Greek Accents"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_lemmas_activate(gpointer menuitem,
					gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Lemmas"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void
on_scripture_cross_references_activate(gpointer menuitem,
				       gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Cross-references"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_hebrew_vowel_points_activate(gpointer
							 menuitem,
						     gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Hebrew Vowel Points"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_hebrew_cantillation_activate(gpointer
							 menuitem,
						     gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Hebrew Cantillation"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_headings_activate(gpointer menuitem,
					  gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Headings"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_italic_headings_activate(gpointer
						 menuitem,
						 gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Italic Headings"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_transliteration_activate(gpointer
						     menuitem,
						 gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Transliteration"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_commentary_by_chapter_activate(gpointer
							   menuitem,
						       gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Commentary by Chapter"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_doublespace_activate(gpointer menuitem,
					     gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Doublespace"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_xrefnotenumbers_activate(gpointer
						     menuitem,
						 gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "XrefNoteNumbers"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_xlit_activate(gpointer menuitem,
				      gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Transliterated Forms"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_enumerated_activate(gpointer menuitem,
					    gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Enumerations"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_glosses_activate(gpointer menuitem,
					 gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Glosses"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_morphseg_activate(gpointer menuitem,
					  gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Morpheme Segmentation"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_primary_reading_activate(gpointer
						     menuitem,
						 gpointer user_data)
{
	gchar *key = NULL;

	if (is_dialog) {
		reading_selector(dialog->mod_name,
				 dialog->key,
				 dialog,
				 (gpointer )menuitem,
				 GINT_TO_POINTER(0));
	} else {
		if ((key = _get_key(menu_mod_name))) {
			reading_selector(menu_mod_name,
					 key,
					 NULL,
					 (gpointer )menuitem,
					 GINT_TO_POINTER(0));
			g_free(key);
		}
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_secondary_reading_activate(gpointer
						       menuitem,
						   gpointer user_data)
{
	gchar *key = NULL;

	if (is_dialog) {
		reading_selector(dialog->mod_name,
				 dialog->key,
				 dialog,
				 (gpointer )menuitem,
				 GINT_TO_POINTER(1));
	} else {
		if ((key = _get_key(menu_mod_name))) {
			reading_selector(menu_mod_name,
					 key,
					 NULL,
					 (gpointer )menuitem,
					 GINT_TO_POINTER(1));
			g_free(key);
		}
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_all_readings_activate(gpointer menuitem,
					      gpointer user_data)
{
	gchar *key = NULL;

	if (is_dialog) {
		reading_selector(dialog->mod_name,
				 dialog->key,
				 dialog,
				 (gpointer )menuitem,
				 GINT_TO_POINTER(2));
	} else {
		if ((key = _get_key(menu_mod_name))) {
			reading_selector(menu_mod_name,
					 key,
					 NULL,
					 (gpointer )menuitem,
					 GINT_TO_POINTER(2));
			g_free(key);
		}
	}
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_image_content_activate(gpointer menuitem,
					       gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Image Content"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_respect_font_faces_activate(gpointer
							menuitem,
						    gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Respect Font Faces"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_chapter_N_activate(gpointer
							menuitem,
						    gpointer user_data)
{
	_global_option_main_pane(GPOINTER_TO_INT(user_data) != 0, "Display Chapter N"); /* string not seen by user */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_unlock_this_module_activate(gpointer menuitem,
						    gpointer user_data)
{
	if (is_dialog)
		return;
	main_check_unlock(menu_mod_name, FALSE);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_display_book_heading_activate(gpointer
							  menuitem,
						      gpointer user_data)
{
	if (is_dialog)
		main_dialogs_book_heading(dialog);
	else
		main_book_heading(menu_mod_name);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_display_chapter_heading_activate(gpointer
							     menuitem,
							 gpointer
							     user_data)
{
	if (is_dialog)
		main_dialogs_chapter_heading(dialog);
	else
		main_chapter_heading(menu_mod_name);
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_use_current_dictionary_activate(gpointer
							    menuitem,
							gpointer user_data)
{
	XIPHOS_HTML_COPY_SELECTION(_get_html());
	gtk_editable_select_region((GtkEditable *)widgets.entry_dict, 0,
				   -1);
	gtk_editable_paste_clipboard((GtkEditable *)widgets.entry_dict);
	gtk_widget_activate(widgets.entry_dict);
}

/******************************************************************************
 * Name
 *   on_lookup_biblemap_activate
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 * Description
 *   offer mouse-swept selection as a biblemap browser reference.
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_lookup_biblemap_activate(gpointer menuitem,
						 gpointer user_data)
{
	GtkWidget *html_widget = _get_html();
	GdkDisplay *display = gtk_widget_get_display(html_widget);

	GtkClipboard *clipboard =
		gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY);

	gchar *text = gtk_clipboard_wait_for_text(clipboard);
	int len = (text ? strlen(text) : 0);

	if (text && len && *text) {
		gchar *enc_key = g_uri_escape_string(text, NULL, FALSE);
		gchar *showstr = g_strconcat("http://www.biblemap.org/#", enc_key, NULL);
		xiphos_open_default(showstr);
		g_free(enc_key);
		g_free(showstr);
	} else
		gui_generic_warning(_("Antes hay que seleccionar algo.\n\n"
				      "Arrastra el ratón sobre el nombre de un "
				      "lugar -- «Jerusalén», «Galilea» -- y vuelve "
				      "a abrir este menú: BibleMap.org lo enseña "
				      "en el mapa."));
}

/******************************************************************************
 * Name
 *   on_translate_activate
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 * Description
 *   offer mouse-swept selection as a google translation request.
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_translate_activate(gpointer menuitem,
					   gpointer user_data)
{
	GtkWidget *html_widget = _get_html();
	GdkDisplay *display = gtk_widget_get_display(html_widget);

	GtkClipboard *clipboard =
		gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY);

	gchar *text = gtk_clipboard_wait_for_text(clipboard);
	int len = (text ? strlen(text) : 0);

	if (text && len && *text) {
		gchar *enc_key = g_uri_escape_string(text, NULL, FALSE);
		gchar *showstr = g_strconcat("https://translate.google.com/?sl=auto&tl=",
					     (sword_locale ? sword_locale : ""),
					     "&op=translate",
					     "&text=",
					     enc_key,
					     NULL);
		xiphos_open_default(showstr);
		g_free(enc_key);
		g_free(showstr);
	} else
		gui_generic_warning(_("Antes hay que seleccionar algo.\n\n"
				      "Arrastra el ratón sobre el texto que quieras "
				      "traducir y vuelve a abrir este menú."));
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_rename_perscomm_activate(gpointer menuitem,
						 gpointer user_data)
{
	if (is_dialog)
		return;

#if defined(WIN32)
	gui_generic_warning(_("Renaming is not available in Windows.\n\n"
			      "Xiphos is limited by Windows' filesystem,\n"
			      "because it disallows the renaming of filename\n"
			      "components of currently-open files,\n"
			      "such as the contents of this commentary.\n"
			      "Therefore, personal commentary renaming is\n"
			      "not available in the Windows environment."));
#else
	GS_DIALOG *info;
	GString *workstr;
	char *s;
	char *datapath_old, *datapath_new;
	const char *conf_old;
	char *conf_new;
	char *sworddir, *modsdir;

	// get a new name for the module.
	info = gui_new_dialog();
	info->title = _("Rename Commentary");
	workstr = g_string_new("");
	g_string_printf(workstr, "<span weight=\"bold\">%s</span>",
			_("Choose Commentary Name"));
	info->label_top = workstr->str;
	info->text1 = g_strdup(_("New Name"));
	info->label1 = _("Name: ");
	info->ok = TRUE;
	info->cancel = TRUE;

	if (gui_gs_dialog(info) != GS_OK)
		goto out1;

	for (s = info->text1; *s; ++s) {
		if (!isalnum(*s) && (*s != '_')) {
			gui_generic_warning_modal(_("Module names must contain [A-Za-z0-9_] only."));
			goto out1;
		}
	}

	if (main_is_module(info->text1)) {
		gui_generic_warning_modal(_("Xiphos already knows a module by that name."));
		goto out1;
	}

	sworddir = g_strdup_printf("%s/" DOTSWORD, settings.homedir);
	modsdir = g_strdup_printf("%s/mods.d", sworddir);

	conf_old =
	    main_get_mod_config_file(settings.CommWindowModule, sworddir);

	conf_new = g_strdup(info->text1); // dirname is lowercase.
	for (s = conf_new; *s; ++s)
		if (isupper(*s))
			*s = tolower(*s);

	datapath_old =
	    main_get_mod_config_entry(settings.CommWindowModule,
				      "DataPath");
	datapath_new = g_strdup(datapath_old);
	if ((s = strstr(datapath_new, "rawfiles/")) == NULL) {
		gui_generic_warning_modal("Malformed datapath in old configuration!");
		goto out2;
	}

	*(s + 9) = '\0'; // skip past "rawfiles/".
	s = g_strdup_printf("%s%s", datapath_new, conf_new);
	g_free(datapath_new); // out with the old...
	datapath_new = s;     // ..and in with the new.

	// move old data directory to new.
	if ((g_chdir(sworddir) != 0) ||
	    (rename(datapath_old, datapath_new) != 0)) {
		gui_generic_warning_modal("Failed to rename directory.");
		goto out2;
	}
	// manufacture new .conf from old.
	gchar *conf_path_old = g_strdup_printf("%s/%s", modsdir, conf_old);
	gchar *conf_path_new = g_strdup_printf("%s/%s.conf", modsdir, conf_new);
	gchar *sed_old_sec = g_strdup_printf("[%s]", info->text1);
	gchar *sed_old_path = g_strdup_printf("rawfiles/%s/", conf_new);
	gchar *sed_expr = g_strdup_printf(
			"/^\\[/s|^.*$|%s|;/^DataPath=/s|rawfiles/.*$|%s|",
			sed_old_sec, sed_old_path);
	gchar *argv[] = {
		(gchar *)"sed", (gchar *)"-e", sed_expr, conf_path_old, NULL
	};
	GError *spawn_error = NULL;
	gint exit_status = 0;
	gchar *spawn_stdout = NULL;
	gchar *spawn_stderr = NULL;
	gboolean spawn_ok = g_spawn_sync(modsdir, argv, NULL,
					 G_SPAWN_SEARCH_PATH,
					 NULL, NULL, &spawn_stdout, &spawn_stderr, &exit_status, &spawn_error);
	if (!spawn_ok) {
		gchar *msg = g_strdup_printf(
				_("Failed to create new configuration:\n%s"),
				spawn_error ? spawn_error->message : "unknown error");
		gui_generic_warning_modal(msg);
		g_free(msg);
		if (spawn_error) g_error_free(spawn_error);
		g_free(conf_path_old);
		g_free(conf_path_new);
		g_free(sed_old_sec);
		g_free(sed_old_path);
		g_free(sed_expr);
		g_free(spawn_stdout);
		g_free(spawn_stderr);
		goto out2;
	}
	if (exit_status != 0) {
		gchar *msg = g_strdup_printf(
				_("Configuration build error:\n\n%s"),
				spawn_stderr ? spawn_stderr : "(no error output)");
		gui_generic_warning_modal(msg);
		g_free(msg);
		g_free(conf_path_old);
		g_free(conf_path_new);
		g_free(sed_old_sec);
		g_free(sed_old_path);
		g_free(sed_expr);
		g_free(spawn_stdout);
		g_free(spawn_stderr);
		goto out2;
	}
	// Write sed output to the new .conf file.
	if (!g_file_set_contents(conf_path_new, spawn_stdout ? spawn_stdout : "",
				 spawn_stdout ? strlen(spawn_stdout) : 0, NULL)) {
		gui_generic_warning_modal("Failed to write new configuration file.");
		g_free(conf_path_old);
		g_free(conf_path_new);
		g_free(sed_old_sec);
		g_free(sed_old_path);
		g_free(sed_expr);
		g_free(spawn_stdout);
		g_free(spawn_stderr);
		goto out2;
	}
	g_free(conf_path_old);
	g_free(conf_path_new);
	g_free(sed_old_sec);
	g_free(sed_old_path);
	g_free(sed_expr);
	g_free(spawn_stdout);
	g_free(spawn_stderr);

	// unlink old conf.
	g_string_printf(workstr, "%s/%s", modsdir, conf_old);
	if (unlink(workstr->str) != 0) {
		g_string_printf(workstr,
				"Unlink of old configuration failed:\n%s",
				strerror(errno));
		gui_generic_warning_modal(workstr->str);
		goto out2;
	}
	main_update_module_lists();
	settings.CommWindowModule = g_strdup(info->text1);
	main_display_commentary(info->text1, settings.currentverse);

out2:
	g_free(conf_new);
	g_free((char *)conf_old);
	g_free(datapath_old);
	g_free(datapath_new);
	g_free(modsdir);
	g_free(sworddir);
out1:
	g_free(info->text1);
	g_free(info);
	g_string_free(workstr, TRUE);
#endif /* !WIN32 */
}

/******************************************************************************
 * Name
 *
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *
 *
 * Description
 *
 *
 * Return value
 *
 */

G_MODULE_EXPORT void on_dump_perscomm_activate(gpointer menuitem,
					       gpointer user_data)
{
	main_sidebar_perscomm_dump();
}

/******************************************************************************
 * Name
 *   on_read_selection_aloud_activate
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 * Description
 *   takes mouse-swept text and funnels it through TTS.
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_read_selection_aloud_activate(gpointer
							  menuitem,
						      gpointer user_data)
{
	GtkWidget *html_widget = _get_html();

	GdkDisplay *display = gtk_widget_get_display(html_widget);

	GtkClipboard *clipboard =
		gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY);

	gchar *text = gtk_clipboard_wait_for_text(clipboard);
	int len = (text ? strlen(text) : 0);

	if (text && len && *text) {
		ReadAloud(0, text);
		g_free(text);
	} else
		gui_generic_warning(_("Antes hay que seleccionar algo.\n\n"
				      "Arrastra el ratón sobre lo que quieras oír y "
				      "vuelve a abrir este menú."));
}

/******************************************************************************
 * Name
 *   on_mark_verse_activate
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 * Description
 *   mark/unmark a verse for highlighting and possible user comment.
 *
 * Return value
 *   void
 */

G_MODULE_EXPORT void on_mark_verse_activate(gpointer menuitem,
					    gpointer user_data)
{
	gchar *key;

	if (is_dialog)
		return;

	if ((key = _get_key(menu_mod_name))) {
		gui_mark_verse_dialog(menu_mod_name, key);
		g_free(key);
	}
}

/******************************************************************************
 * Name
 *  on_view_mod_activate
 *
 * Synopsis
 *   #include "gui/menu_popup.h"
 *
 *   void on_view_mod_activate(gpointer  menuitem, gpointer user_data)
 *
 * Description
 *   show a different module
 *
 * Return value
 *   void
 */

static void on_view_mod_activate(gpointer menuitem,
				 gpointer user_data)
{
	if (is_dialog)
		return;

	gchar *module_name = main_module_name_from_description((gchar *)user_data);
	gchar *key;

	if (module_name && (key = _get_key(menu_mod_name))) {
		gchar *url = g_strdup_printf("sword://%s/%s", module_name, key);
		main_url_handler(url, TRUE);
		g_free(url);
	}
	g_free(module_name);
	g_free(key);
}

/******************************************************************************
 * Name
 *  gui_lookup_bibletext_selection
 *
 * Synopsis
 *   #include "gui/bibletext.h"
 *
 * void gui_lookup_bibletext_selection(gpointer  menuitem,
					 gchar * dict_mod_description)
 *
 * Description
 *   lookup selection in a dict/lex module
 *
 * Return value
 *   void
 */

static void _lookup_selection(gpointer menuitem,
			      gchar *dict_mod_description)
{
	gchar *dict_key = NULL;
	gchar *mod_name = NULL;
	GtkWidget *html = _get_html();

	if (!html)
		return;
	mod_name = main_module_name_from_description(dict_mod_description);
	XIPHOS_HTML_COPY_SELECTION(html);
	gtk_editable_select_region((GtkEditable *)widgets.entry_dict, 0,
				   -1);
	gtk_editable_paste_clipboard((GtkEditable *)widgets.entry_dict);
	gtk_widget_activate(widgets.entry_dict);
	dict_key =
	    g_strdup(gtk_editable_get_chars((GtkEditable *)widgets.entry_dict, 0, -1));

	if (dict_key && mod_name) {
		main_display_dictionary(mod_name, dict_key);
	}
	if (dict_key)
		g_free(dict_key);
	if (mod_name)
		g_free(mod_name);
}

#define POPUP_ACTION(name, handler) \
	static void name(GSimpleAction *action, GVariant *parameter, gpointer data) \
	{ \
		(void)action; \
		(void)parameter; \
		handler(NULL, data); \
	}

POPUP_ACTION(about_action, on_popup_about_activate)
POPUP_ACTION(bookmark_action, on_popup_bookmark_activate)
POPUP_ACTION(mark_verse_action, on_mark_verse_activate)
POPUP_ACTION(export_action, on_popup_export_passage_activate)
POPUP_ACTION(print_action, on_popup_print_activate)
POPUP_ACTION(copy_action, on_popup_copy_activate)
POPUP_ACTION(find_action, on_popup_find_activate)
POPUP_ACTION(font_action, on_popup_font_activate)
POPUP_ACTION(unlock_action, on_unlock_this_module_activate)
POPUP_ACTION(book_heading_action, on_display_book_heading_activate)
POPUP_ACTION(chapter_heading_action, on_display_chapter_heading_activate)
POPUP_ACTION(rename_action, on_rename_perscomm_activate)
POPUP_ACTION(dump_action, on_dump_perscomm_activate)
POPUP_ACTION(read_aloud_action, on_read_selection_aloud_activate)
POPUP_ACTION(current_dictionary_action, on_use_current_dictionary_activate)
POPUP_ACTION(translate_action, on_translate_activate)
POPUP_ACTION(biblemap_action, on_lookup_biblemap_activate)

static void view_module_action(GSimpleAction *action, GVariant *parameter,
			       gpointer data)
{
	(void)action;
	(void)data;
	on_view_mod_activate(NULL, (gpointer)g_variant_get_string(parameter, NULL));
}

static void edit_note_action(GSimpleAction *action, GVariant *parameter,
			     gpointer data)
{
	(void)action;
	(void)data;
	on_edit_percomm_activate(NULL,
				  (gpointer)g_variant_get_string(parameter, NULL));
}

static void edit_prayer_action(GSimpleAction *action, GVariant *parameter,
			       gpointer data)
{
	(void)action;
	(void)data;
	on_edit_prayerlist_activate(NULL,
				     (gpointer)g_variant_get_string(parameter, NULL));
}

static void lookup_dictionary_action(GSimpleAction *action,
				     GVariant *parameter, gpointer data)
{
	(void)action;
	(void)data;
	_lookup_selection(NULL, (gchar *)g_variant_get_string(parameter, NULL));
}

static void option_state_changed(GSimpleAction *action, GVariant *state,
				 gpointer data)
{
	(void)data;
	g_simple_action_set_state(action, state);
	_global_option_main_pane(g_variant_get_boolean(state),
				 g_object_get_data(G_OBJECT(action), "module-option"));
}

static void verse_style_state_changed(GSimpleAction *action, GVariant *state,
				      gpointer data)
{
	(void)data;
	gboolean active = g_variant_get_boolean(state);
	g_simple_action_set_state(action, state);
	on_verse_per_line_activate(NULL, GINT_TO_POINTER(active));
}

static void strong_state_changed(GSimpleAction *action, GVariant *state,
				 gpointer data)
{
	(void)data;
	gboolean active = g_variant_get_boolean(state);
	g_simple_action_set_state(action, state);
	on_strong_s_numbers_activate(NULL, GINT_TO_POINTER(active));
}

static void variants_state_changed(GSimpleAction *action, GVariant *state,
				   gpointer data)
{
	(void)data;
	const gchar *choice = g_variant_get_string(state, NULL);
	g_simple_action_set_state(action, state);
	if (!g_strcmp0(choice, "secondary"))
		on_secondary_reading_activate(NULL, NULL);
	else if (!g_strcmp0(choice, "all"))
		on_all_readings_activate(NULL, NULL);
	else
		on_primary_reading_activate(NULL, NULL);
}

static void add_boolean_action(GSimpleActionGroup *group, const gchar *name,
			       gboolean state, GCallback callback,
			       const gchar *module_option)
{
	GSimpleAction *action = g_simple_action_new_stateful(
	    name, NULL, g_variant_new_boolean(state));
	if (module_option)
		g_object_set_data(G_OBJECT(action), "module-option",
				  (gpointer)module_option);
	g_signal_connect(action, "change-state", callback, NULL);
	g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
	g_object_unref(action);
}

static void append_target(GMenu *menu, const gchar *label,
			  const gchar *action, const gchar *target)
{
	GMenuItem *item = g_menu_item_new(label, NULL);
	g_menu_item_set_action_and_target(item, action, "s", target);
	g_menu_append_item(menu, item);
	g_object_unref(item);
}

static void append_modules(GMenu *menu, gint list_type, const gchar *action)
{
	for (GList *item = get_list(list_type); item; item = item->next)
		append_target(menu, (const gchar *)item->data, action,
			      (const gchar *)item->data);
}

static gboolean has_global_option(const gchar *module, const gchar *first,
				  const gchar *second, const gchar *third)
{
	return (first && main_check_for_global_option((gchar *)module, first)) ||
	       (second && main_check_for_global_option((gchar *)module, second)) ||
	       (third && main_check_for_global_option((gchar *)module, third));
}

static void append_option(GMenu *menu, GSimpleActionGroup *group,
			  const gchar *name, const gchar *label, gboolean state,
			  const gchar *module_option)
{
	add_boolean_action(group, name, state, G_CALLBACK(option_state_changed),
			   module_option);
	gchar *detailed = g_strdup_printf("contexto.%s", name);
	g_menu_append(menu, label, detailed);
	g_free(detailed);
}

static GMenu *create_module_options(const gchar *module,
				    GSimpleActionGroup *group)
{
	GLOBAL_OPS *ops = main_new_globals(module);
	gint type = main_get_mod_type((gchar *)module);
	GMenu *model = g_menu_new();
	GMenu *font = g_menu_new();
	g_menu_append(font, _("Seleccionar tipografía"), "contexto.tipografia");
	g_menu_append_section(model, NULL, G_MENU_MODEL(font));
	g_object_unref(font);

	GMenu *options = g_menu_new();
	if (type == TEXT_TYPE) {
		add_boolean_action(group, "verso-linea", ops->verse_per_line,
				   G_CALLBACK(verse_style_state_changed), NULL);
		g_menu_append(options, _("Un versículo por línea"),
			      "contexto.verso-linea");
	}
	if (has_global_option(module, "ThMLHeadings", "OSISHeadings", NULL)) {
		append_option(options, group, "titulos", _("Títulos"),
			      ops->headings, "Headings");
		if (ops->headings)
			append_option(options, group, "titulos-cursiva",
				      _("Títulos en cursiva"),
				      ops->italic_headings, "Italic Headings");
	}
	if (has_global_option(module, "GBFRedLetterWords", "OSISRedLetterWords", NULL))
		append_option(options, group, "palabras-rojas",
			      _("Palabras de Cristo en rojo"), ops->words_in_red,
			      "Words of Christ in Red");
	add_boolean_action(group, "strong", settings.show_interlineal != 0,
			   G_CALLBACK(strong_state_changed), NULL);
	if (has_global_option(module, "GBFStrongs", "ThMLStrongs", "OSISStrongs")) {
		g_menu_append(options, _("Números Strong"), "contexto.strong");
	}
	if (has_global_option(module, "ThMLLemma", "OSISLemma", NULL))
		append_option(options, group, "lemas", _("Lemas"), ops->lemmas,
			      "Lemmas");
	if (has_global_option(module, "GBFMorph", "ThMLMorph", "OSISMorph"))
		append_option(options, group, "morfologia", _("Etiquetas morfológicas"),
			      ops->morphs, "Morphological Tags");
	gboolean has_footnotes = has_global_option(
	    module, "GBFFootnotes", "ThMLFootnotes", "OSISFootnotes");
	if (has_footnotes)
		append_option(options, group, "notas-pie", _("Notas al pie"),
			      ops->footnotes, "Footnotes");
	gboolean has_xrefs = has_global_option(module, "ThMLScripref",
						 "OSISScripref", NULL);
	if (has_xrefs)
		append_option(options, group, "referencias", _("Referencias cruzadas"),
			      ops->scripturerefs, "Cross-references");
	if ((ops->scripturerefs && has_xrefs) || (ops->footnotes && has_footnotes))
		append_option(options, group, "marcas", _("Marcas de nota/referencia"),
			      ops->xrefnotenumbers, "XrefNoteNumbers");
	if (has_global_option(module, "UTF8GreekAccents", NULL, NULL))
		append_option(options, group, "acentos-griegos", _("Acentos griegos"),
			      ops->greekaccents, "Greek Accents");
	if (has_global_option(module, "UTF8HebrewPoints", NULL, NULL))
		append_option(options, group, "vocales-hebreas", _("Vocales hebreas"),
			      ops->hebrewpoints, "Hebrew Vowel Points");
	if (has_global_option(module, "UTF8Cantillation", NULL, NULL))
		append_option(options, group, "cantilacion", _("Cantilación hebrea"),
			      ops->hebrewcant, "Hebrew Cantillation");
	if (has_global_option(module, "ThMLVariants", "OSISVariants", NULL)) {
		const gchar *state = ops->variants_secondary ? "secondary" :
				     ops->variants_all ? "all" : "primary";
		GSimpleAction *variants = g_simple_action_new_stateful(
		    "variantes", G_VARIANT_TYPE_STRING, g_variant_new_string(state));
		g_signal_connect(variants, "change-state",
				 G_CALLBACK(variants_state_changed), NULL);
		g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(variants));
		g_object_unref(variants);
		GMenu *choices = g_menu_new();
		append_target(choices, _("Lectura principal"), "contexto.variantes",
			      "primary");
		append_target(choices, _("Lectura secundaria"), "contexto.variantes",
			      "secondary");
		append_target(choices, _("Todas las lecturas"), "contexto.variantes",
			      "all");
		g_menu_append_submenu(options, _("Variantes"), G_MENU_MODEL(choices));
		g_object_unref(choices);
	}
	if (has_global_option(module, "OSISXlit", NULL, NULL))
		append_option(options, group, "formas-transliteradas",
			      _("Formas transliteradas"), ops->xlit,
			      "Transliterated Forms");
	if (has_global_option(module, "OSISEnum", NULL, NULL))
		append_option(options, group, "enumeraciones", _("Enumeraciones"),
			      ops->enumerated, "Enumerations");
	if (has_global_option(module, "OSISGlosses", "OSISRuby", NULL))
		append_option(options, group, "glosas", _("Glosas"), ops->glosses,
			      "Glosses");
	if (has_global_option(module, "OSISMorphSegmentation", NULL, NULL))
		append_option(options, group, "morfemas", _("Segmentación de morfemas"),
			      ops->morphseg, "Morpheme Segmentation");
	if (ops->image_content != -1)
		append_option(options, group, "imagenes", _("Imágenes"),
			      ops->image_content, "Image Content");
	if (ops->respect_font_faces != -1)
		append_option(options, group, "tipografias-modulo",
			      _("Respetar tipografías del módulo"),
			      ops->respect_font_faces, "Respect Font Faces");
	if (type == TEXT_TYPE && ops->display_chapter_N != -1)
		append_option(options, group, "numero-capitulo",
			      _("Mostrar números de capítulo"),
			      ops->display_chapter_N, "Display Chapter N");
	if (type == COMMENTARY_TYPE || type == PERCOM_TYPE)
		append_option(options, group, "comentario-capitulo",
			      _("Comentario por capítulo"),
			      ops->commentary_by_chapter, "Commentary by Chapter");
	append_option(options, group, "doble-espacio", _("Doble espacio"),
		      ops->doublespace, "Doublespace");
	g_menu_append_section(model, NULL, G_MENU_MODEL(options));
	g_object_unref(options);
	g_free(ops);
	return model;
}

static GSimpleActionGroup *create_popup_actions(XiphosHtml *html,
						 gboolean has_selection)
{
	const GActionEntry entries[] = {
		{ "acerca", about_action, NULL, NULL, NULL, { 0 } },
		{ "marcador", bookmark_action, NULL, NULL, NULL, { 0 } },
		{ "anotar", mark_verse_action, NULL, NULL, NULL, { 0 } },
		{ "exportar", export_action, NULL, NULL, NULL, { 0 } },
		{ "imprimir", print_action, NULL, NULL, NULL, { 0 } },
		{ "copiar", copy_action, NULL, NULL, NULL, { 0 } },
		{ "buscar", find_action, NULL, NULL, NULL, { 0 } },
		{ "tipografia", font_action, NULL, NULL, NULL, { 0 } },
		{ "desbloquear", unlock_action, NULL, NULL, NULL, { 0 } },
		{ "titulo-libro", book_heading_action, NULL, NULL, NULL, { 0 } },
		{ "titulo-capitulo", chapter_heading_action, NULL, NULL, NULL, { 0 } },
		{ "renombrar", rename_action, NULL, NULL, NULL, { 0 } },
		{ "vaciar", dump_action, NULL, NULL, NULL, { 0 } },
		{ "leer", read_aloud_action, NULL, NULL, NULL, { 0 } },
		{ "diccionario-actual", current_dictionary_action, NULL, NULL, NULL, { 0 } },
		{ "traducir", translate_action, NULL, NULL, NULL, { 0 } },
		{ "biblemap", biblemap_action, NULL, NULL, NULL, { 0 } },
		{ "abrir", view_module_action, "s", NULL, NULL, { 0 } },
		{ "editar-nota", edit_note_action, "s", NULL, NULL, { 0 } },
		{ "editar-libro", edit_prayer_action, "s", NULL, NULL, { 0 } },
		{ "diccionario", lookup_dictionary_action, "s", NULL, NULL, { 0 } },
	};
	GSimpleActionGroup *group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(group), entries,
					G_N_ELEMENTS(entries), html);
	GSimpleAction *selection = g_simple_action_new("seleccion", NULL);
	g_simple_action_set_enabled(selection, has_selection);
	g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(selection));
	g_object_unref(selection);
	const gchar *selection_actions[] = {
		"leer", "diccionario-actual", "traducir", "biblemap", "diccionario"
	};
	for (guint i = 0; i < G_N_ELEMENTS(selection_actions); ++i) {
		GAction *action = g_action_map_lookup_action(
		    G_ACTION_MAP(group), selection_actions[i]);
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), has_selection);
	}
	return group;
}

static GMenuItem *submenu_item(const gchar *label, GMenuModel *submenu,
			       const gchar *action)
{
	GMenuItem *item = g_menu_item_new_submenu(label, submenu);
	if (action)
		g_menu_item_set_action_and_target_value(item, action, NULL);
	return item;
}

static GMenu *create_popup_model(const gchar *module,
				 GSimpleActionGroup *actions, gboolean has_selection)
{
	(void)has_selection;
	gint type = main_get_mod_type((gchar *)module);
	GMenu *model = g_menu_new();
	GMenu *primary = g_menu_new();
	g_menu_append(primary, _("Acerca de"), "contexto.acerca");
	if (!is_dialog)
		g_menu_append(primary, _("Marcador"), "contexto.marcador");
	if (!is_dialog && type == TEXT_TYPE)
		g_menu_append(primary, _("Anotar versículo"), "contexto.anotar");
	g_menu_append_section(model, NULL, G_MENU_MODEL(primary));
	g_object_unref(primary);

	if (type == TEXT_TYPE || type == COMMENTARY_TYPE || type == PERCOM_TYPE) {
		GMenu *exporting = g_menu_new();
		g_menu_append(exporting, _("Copiar/exportar pasaje"),
			      "contexto.exportar");
		g_menu_append_section(model, NULL, G_MENU_MODEL(exporting));
		g_object_unref(exporting);
	}

	GMenu *menus = g_menu_new();
	GMenu *file = g_menu_new();
	if (!is_dialog) {
		GMenu *modules = g_menu_new();
		append_modules(modules, _get_type_mod_list(), "contexto.abrir");
		g_menu_append_submenu(file, _("Abrir módulo"), G_MENU_MODEL(modules));
		g_object_unref(modules);
	}
	g_menu_append(file, _("Imprimir"), "contexto.imprimir");
	g_menu_append_submenu(menus, _("Archivo"), G_MENU_MODEL(file));
	g_object_unref(file);

	GMenu *edit = g_menu_new();
	g_menu_append(edit, _("Copiar"), "contexto.copiar");
	g_menu_append(edit, _("Buscar"), "contexto.buscar");
	if (!is_dialog && type == TEXT_TYPE) {
		GMenu *notes = g_menu_new();
		append_modules(notes, PERCOMM_LIST, "contexto.editar-nota");
		g_menu_append_submenu(edit, _("Nota"), G_MENU_MODEL(notes));
		g_object_unref(notes);
	} else if (type == PERCOM_TYPE) {
		append_target(edit, _("Abrir en el editor"), "contexto.editar-nota",
			      module);
	} else if (type == PRAYERLIST_TYPE) {
		append_target(edit, _("Abrir en el editor"), "contexto.editar-libro",
			      module);
	}
	g_menu_append_submenu(menus, _("Editar"), G_MENU_MODEL(edit));
	g_object_unref(edit);

	GMenu *options = create_module_options(module, actions);
	g_menu_append_submenu(menus, _("Opciones del módulo"),
			      G_MENU_MODEL(options));
	g_object_unref(options);

	GMenu *lookup = g_menu_new();
	GMenu *lookup_tools = g_menu_new();
	g_menu_append(lookup_tools, _("Usar el diccionario actual"),
		      "contexto.diccionario-actual");
	g_menu_append(lookup_tools, _("Traducir la selección"),
		      "contexto.traducir");
	g_menu_append(lookup_tools, _("Ver en BibleMap.org"), "contexto.biblemap");
	g_menu_append_section(lookup, NULL, G_MENU_MODEL(lookup_tools));
	g_object_unref(lookup_tools);
	GMenu *dictionaries = g_menu_new();
	append_modules(dictionaries, DICT_DESC_LIST, "contexto.diccionario");
	g_menu_append_section(lookup, NULL, G_MENU_MODEL(dictionaries));
	g_object_unref(dictionaries);
	GMenuItem *lookup_item = submenu_item(_("Buscar selección"),
					      G_MENU_MODEL(lookup),
					      "contexto.seleccion");
	g_menu_append_item(menus, lookup_item);
	g_object_unref(lookup_item);
	g_object_unref(lookup);
	g_menu_append_section(model, NULL, G_MENU_MODEL(menus));
	g_object_unref(menus);

	GMenu *extra = g_menu_new();
	if (!is_dialog && main_has_cipher_tag((gchar *)module))
		g_menu_append(extra, _("Desbloquear este módulo"),
			      "contexto.desbloquear");
	if (type == COMMENTARY_TYPE) {
		g_menu_append(extra, _("Mostrar título del libro"),
			      "contexto.titulo-libro");
		g_menu_append(extra, _("Mostrar título del capítulo"),
			      "contexto.titulo-capitulo");
	}
	if (type == PERCOM_TYPE) {
		g_menu_append(extra, _("Renombrar comentario personal"),
			      "contexto.renombrar");
		g_menu_append(extra, _("Vaciar comentario personal"),
			      "contexto.vaciar");
	}
	g_menu_append(extra, _("Leer la selección en voz alta"), "contexto.leer");
	g_menu_append_section(model, NULL, G_MENU_MODEL(extra));
	g_object_unref(extra);
	return model;
}

GtkWidget *gui_menu_popup(XiphosHtml *html, const gchar *mod_name,
			  DIALOG_DATA *d)
{
	if (d) {
		dialog = d;
		menu_mod_name = NULL;
		is_dialog = TRUE;
	} else if (mod_name) {
		menu_mod_name = (gchar *)mod_name;
		dialog = NULL;
		is_dialog = FALSE;
	} else {
		return NULL;
	}

	const gchar *module = is_dialog ? d->mod_name : mod_name;
	if (!module || !*module) {
		gui_generic_warning(_("No module in this pane."));
		return NULL;
	}
	if (!html)
		html = (XiphosHtml *)_get_html();
	if (!html)
		return NULL;
	gboolean has_selection = html && XIPHOS_HTML_HAS_SELECTION(html);
	GSimpleActionGroup *actions = create_popup_actions(html, has_selection);
	GMenu *model = create_popup_model(module, actions, has_selection);
	gtk_widget_insert_action_group(GTK_WIDGET(html), "contexto",
				       G_ACTION_GROUP(actions));
	g_object_unref(actions);
	GtkWidget *popover = gui_popup_menu_model_at_pointer(
	    G_MENU_MODEL(model), GTK_WIDGET(html));
	if (popover)
		g_object_set_data_full(G_OBJECT(popover), "elim-menu-model",
				       g_object_ref(model),
				       (GDestroyNotify)g_object_unref);
	g_object_unref(model);
	return popover;
}
