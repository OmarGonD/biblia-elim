/*
 * Xiphos Bible Study Tool
 * bookmarks_treeview.c - gui for bookmarks using treeview
 *
 * Copyright (C) 2003-2026 Xiphos Developer Team
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

#include <glib.h>
#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include <libxml/parser.h>

#include <math.h>
#include <cairo.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "gui/xiphos.h"
#include "gui/bookmarks_treeview.h"
#include "gui/bookmarks_menu.h"
#include "gui/bookmark_dialog.h"
#include "gui/utilities.h"
#include "gui/main_window.h"
#include "gui/dialog.h"
#include "gui/bibletext_dialog.h"
#include "gui/commentary_dialog.h"
#include "gui/dictlex_dialog.h"
#include "gui/gbs_dialog.h"
#include "gui/sidebar.h"
#include "gui/widgets.h"

#include "main/settings.h"
#include "main/sidebar.h"
#include "main/sword.h"
#include "main/xml.h"
#include "main/module_dialogs.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"

GListStore *bookmark_roots;
gboolean button_one;
gboolean use_dialog;
GtkWidget *bookmark_tree;

BookMarksPixbufs *bm_pixbufs;

/* The text of COLUMN, NULL when it is empty: what the tree store held for a
 * column that was never set. */
gchar *bookmark_row_dup(ElimRow *row, guint column)
{
	const gchar *text = elim_row_get_string(row, column);

	return *text ? g_strdup(text) : NULL;
}

ElimRow *bookmark_selected(void)
{
	return bookmark_tree ? elim_table_get_selected(bookmark_tree) : NULL;
}

/* Creates a round color swatch pixbuf using Cairo (14x14 px) */
static GdkPixbuf *make_color_dot(const gchar *hex_color)
{
	const gint SIZE = 14;
	cairo_surface_t *surface;
	cairo_t *cr;
	GdkPixbuf *pixbuf;
	GdkRGBA rgba;

	if (!hex_color || !*hex_color)
		return NULL;
	if (!gdk_rgba_parse(&rgba, hex_color))
		return NULL;

	surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, SIZE, SIZE);
	cr = cairo_create(surface);

	/* transparent background */
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_paint(cr);

	/* filled circle */
	cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
	cairo_arc(cr, SIZE/2.0, SIZE/2.0, SIZE/2.0 - 1, 0, 2 * G_PI);
	cairo_fill_preserve(cr);

	/* thin dark border */
	cairo_set_source_rgba(cr, 0, 0, 0, 0.35);
	cairo_set_line_width(cr, 1.0);
	cairo_stroke(cr);

	cairo_destroy(cr);
	pixbuf = gdk_pixbuf_get_from_surface(surface, 0, 0, SIZE, SIZE);
	cairo_surface_destroy(surface);
	return pixbuf;
}

void bookmark_row_set_color(ElimRow *row, const gchar *color)
{
	GdkPixbuf *dot = make_color_dot(color);

	elim_row_set_string(row, COL_COLOR, color);
	elim_row_set_pixbuf(row, COL_DOT, dot);
	if (dot)
		g_object_unref(dot);
}

/* Puts everything DATA says in ROW. */
static void fill_row(ElimRow *row, BOOKMARK_DATA *data)
{
	elim_row_set_pixbuf(row, COL_OPEN_PIXBUF, data->opened);
	elim_row_set_pixbuf(row, COL_CLOSED_PIXBUF, data->closed);
	elim_row_set_string(row, COL_CAPTION, data->caption);
	elim_row_set_string(row, COL_KEY, data->key);
	elim_row_set_string(row, COL_MODULE, data->module);
	elim_row_set_string(row, COL_MODULE_DESC, data->module_desc);
	elim_row_set_string(row, COL_DESCRIPTION, data->description);
	bookmark_row_set_color(row, data->color);
}

/******************************************************************************
 * Name
 *   gui_verselist_to_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void gui_verselist_to_bookmarks(GList * list)
 *
 * Description
 *   add results of search to tree as a root node with children
 *
 * Return value
 *   void
 */

void gui_verselist_to_bookmarks(GList *verses, gint save_as_single)
{
	gint test;
	gchar *module_name = NULL;
	ElimRow *root;
	ElimRow *parent;
	GS_DIALOG *info;
	RESULTS *list_item;

	/*gchar *dlg = g_strdup_printf("<span weight=\"bold\">%s</span>\n%s",
	   _("Save these results as a single bookmark?"),
	   _("(rather than as a series of bookmarks)")); */
	if (save_as_single) {
		GString *name = g_string_new(NULL);
		GString *verse_string = g_string_new("");
		gboolean first_entry = TRUE;

		g_string_printf(name, _("Search result: %s"),
				settings.searchText);
		while (verses) {
			list_item = (RESULTS *)verses->data;
			if (main_is_Bible_key(list_item->module, list_item->key)) {
				if (first_entry) {
					module_name =
					    g_strdup(list_item->module);
					first_entry = FALSE;
				} else {
					verse_string =
					    g_string_append(verse_string,
							    "; ");
				}
				verse_string =
				    g_string_append(verse_string,
						    list_item->key);
			}
			verses = g_list_next(verses);
		}
		gui_bookmark_dialog(name->str, module_name,
				    verse_string->str);
		g_string_free(name, TRUE);
		g_string_free(verse_string, TRUE);
		g_free(module_name);
		return;
	}

	root = elim_table_get(bookmark_roots, 0);
	info = gui_new_dialog();
	info->stock_icon = "document-open";

	info->title = _("Bookmark");
	info->label_top = _("Enter Folder Name");
	info->text1 = g_strdup(settings.searchText);
	info->label1 = _("Folder: ");
	info->ok = TRUE;
	info->cancel = TRUE;

	/*** open dialog to get name for root node ***/

	test = gui_gs_dialog(info);
	if (test == GS_OK) {
		BOOKMARK_DATA folder = { 0 };

		folder.caption = info->text1;
		folder.opened = bm_pixbufs->pixbuf_opened;
		folder.closed = bm_pixbufs->pixbuf_closed;
		parent = gui_add_item_to_tree(root, &folder);
		//              set_results_position((char) 1); // TOP
		GString *str = g_string_new(" ");
		while (verses) {
			BOOKMARK_DATA data = { 0 };

			list_item = (RESULTS *)verses->data;
			module_name = list_item->module;
			gchar *tmpbuf = list_item->key;
			g_string_printf(str, "%s, %s", tmpbuf,
					module_name);
			XI_message(("bookmark: %s", str->str));

			data.caption = str->str;
			data.key = tmpbuf;
			data.module = module_name;
			if (!strcmp(data.module, "studypad"))
				data.module_desc = (gchar *)"studypad";
			else
				data.module_desc = (gchar *)main_get_module_description(data.module);
			data.description = (gchar *)"";
			data.is_leaf = TRUE;
			data.opened = bm_pixbufs->pixbuf_helpdoc;
			data.closed = NULL;
			data.color = NULL;
			gui_add_item_to_tree(parent, &data);

			verses = g_list_next(verses);
		}
		g_string_free(str, TRUE);
		bookmarks_changed = TRUE;
		gui_save_bookmarks_treeview();
	}
	g_free(info->text1);
	g_free(info);
}

/******************************************************************************
 * Name
 *   get_xml_folder_data
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void get_xml_folder_data(xmlNodePtr cur, BOOKMARK_DATA * data)
 *
 * Description
 *    get date from xml bookmark folder and put into
 *    BOOKMARK_DATA structure
 *
 * Return value
 *   void
 */

static void get_xml_folder_data(xmlNodePtr cur, BOOKMARK_DATA *data)
{
	xmlChar *folder;
	gchar *color;

	folder = xmlGetProp(cur, (const xmlChar *)"caption");
	data->caption = g_strdup((char *)folder);
	data->key = NULL;
	data->module = NULL;
	data->module_desc = NULL;
	data->description = NULL;
	data->is_leaf = FALSE;
	data->opened = bm_pixbufs->pixbuf_opened;
	data->closed = bm_pixbufs->pixbuf_closed;

	/* color is optional — NULL when the folder has no tag color assigned */
	color = xml_get_folder_color(cur);
	data->color = color ? g_strdup(color) : NULL;
	if (color)
		xmlFree((xmlChar *)color);
}

/******************************************************************************
 * Name
 *    get_xml_bookmark_data
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void get_xml_bookmark_data(xmlNodePtr cur, BOOKMARK_DATA * data)
 *
 * Description
 *    get date from xml bookmark and put into BOOKMARK_DATA structure
 *
 * Return value
 *   void
 */

static void get_xml_bookmark_data(xmlNodePtr cur, BOOKMARK_DATA *data)
{
	xmlChar *mod1;
	xmlChar *key;
	xmlChar *caption;
	xmlChar *mod_desc;
	xmlChar *description;
	gchar buf[500];

	data->opened = bm_pixbufs->pixbuf_helpdoc;
	data->closed = NULL;
	mod1 = xmlGetProp(cur, (const xmlChar *)"modulename");
	key = xmlGetProp(cur, (const xmlChar *)"key");
	mod_desc = xmlGetProp(cur, (const xmlChar *)"moduledescription");
	description = xmlGetProp(cur, (const xmlChar *)"description");
	caption = xmlGetProp(cur, (const xmlChar *)"description");
	if ((char *)caption) {
		if (strlen((char *)caption) > 0) {
			data->caption = g_strdup((char *)caption);
		} else {
			sprintf(buf, "%s, %s", key, mod1);
			data->caption = g_strdup((char *)buf);
		}
	} else
		data->caption = g_strdup((char *)key);
	data->key = g_strdup((char *)key);
	data->module = g_strdup((char *)mod1);
	data->description = g_strdup((char *)description);
	data->module_desc = g_strdup((char *)mod_desc);
	data->is_leaf = TRUE;
	data->color = NULL;  /* leaves (bookmarks) never carry a color */
}

/* What the tree took from DATA it copied: the strings are the caller's again. */
static void free_bookmark_data(BOOKMARK_DATA *data)
{
	g_free(data->caption);
	g_free(data->key);
	g_free(data->module);
	g_free(data->module_desc);
	g_free(data->description);
	g_free(data->color);
	g_free(data);
}

/******************************************************************************
 * Name
 *  gui_add_item_to_tree
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   ElimRow *gui_add_item_to_tree(ElimRow *parent, BOOKMARK_DATA * data)
 *
 * Description
 *    add a folder or a bookmark to the bookmark tree, under PARENT
 *
 * Return value
 *   the new row
 */

ElimRow *gui_add_item_to_tree(ElimRow *parent, BOOKMARK_DATA *data)
{
	ElimRow *row = elim_tree_append(bookmark_roots, parent, N_COLUMNS);

	fill_row(row, data);
	return row;
}

/******************************************************************************
 * Name
 *  add_node
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void add_node(xmlDocPtr doc, xmlNodePtr cur, GtkCTree * ctree,
						GtkCTreeNode *node)
 *
 * Description
 *    parse the xml bookmarks and add to bookmark ctree
 *
 * Return value
 *   void
 */

static void add_node(xmlNodePtr cur, ElimRow *parent)
{
	BOOKMARK_DATA *p = NULL;
	xmlNodePtr work = NULL;

	for (work = cur->xmlChildrenNode; work; work = work->next) {
		if (!xmlStrcmp(work->name, (const xmlChar *)"Bookmark")) {
			p = g_new0(BOOKMARK_DATA, 1);
			get_xml_bookmark_data(work, p);
			gui_add_item_to_tree(parent, p);
			free_bookmark_data(p);
		} else if (!xmlStrcmp(work->name, (const xmlChar *)"Folder")) {
			ElimRow *folder;

			p = g_new0(BOOKMARK_DATA, 1);
			get_xml_folder_data(work, p);
			folder = gui_add_item_to_tree(parent, p);
			free_bookmark_data(p);
			add_node(work, folder);
		}
	}
}

/******************************************************************************
 * Name
 *  gui_parse_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void gui_parse_bookmarks(GtkCTree * ctree)
 *
 * Description
 *    load a xml bookmark file
 *
 * Return value
 *   void
 */

void gui_parse_bookmarks(GtkWidget *tree, const xmlChar *file,
			 ElimRow *parent)
{
	xmlNodePtr cur = NULL;
	BOOKMARK_DATA *p = NULL;
	ElimRow *folder = NULL;

	cur = xml_load_bookmark_file(file);

	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"Bookmark")) {
			p = g_new0(BOOKMARK_DATA, 1);
			get_xml_bookmark_data(cur, p);
			gui_add_item_to_tree(parent, p);
			free_bookmark_data(p);
		} else {
			p = g_new0(BOOKMARK_DATA, 1);
			get_xml_folder_data(cur, p);
			if (p->caption)
				folder = gui_add_item_to_tree(parent, p);
			free_bookmark_data(p);
			/* what is under a nameless folder goes where the last one
			 * was, or into the tree's own parent */
			add_node(cur, folder ? folder : parent);
		}
		cur = cur->next;
	}

	xml_free_bookmark_doc();
	elim_tree_expand_to_row(tree, parent);
}

/******************************************************************************
 * Name
 *  load_xml_bookmarks
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void load_xml_bookmarks(GtkCTree * ctree)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void load_xml_bookmarks(GtkWidget *tree, ElimRow *root)
{
	GString *str;
	const xmlChar *file;

	str = g_string_new(NULL);
	g_string_printf(str, "%s/bookmarks.xml", settings.swbmDir);
	file = (const xmlChar *)str->str;
	gui_parse_bookmarks(tree, file, root);
	g_string_free(str, TRUE);
}

/******************************************************************************
 * Name
 *   bookmarks_tree_changed
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void bookmarks_tree_changed(GListStore *roots, gpointer user_data)
 *
 * Description
 *   the tree changed (a row added, removed, moved or edited): keep the
 *   bookmarks file as the tree is
 *
 * Return value
 *   void
 */

static void bookmarks_tree_changed(GListStore *roots, gpointer user_data)
{
	(void)roots;
	(void)user_data;
	bookmarks_changed = TRUE;
	gui_save_bookmarks_treeview();
}

/******************************************************************************
 * Name
 *   create_pixbufs
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void create_pixbufs(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void create_pixbufs(void)
{
	GtkWidget *app = GTK_WIDGET(widgets.app);

	bm_pixbufs = g_new0(BookMarksPixbufs, 1);

	/* Themed symbolic icons, like the sidebar tree -- see
	 * symbolic_pixbuf(). The raster set this replaces came from
	 * Epiphany and was drawn for a light theme; a folder of saved
	 * references also says more than the old glossy bookmark did.
	 * Leaves get the bookmark glyph, which is what they are. */
	bm_pixbufs->pixbuf_closed = symbolic_pixbuf("folder-symbolic", 16, app);
	bm_pixbufs->pixbuf_opened = symbolic_pixbuf("folder-open-symbolic", 16, app);
	bm_pixbufs->pixbuf_helpdoc = symbolic_pixbuf("user-bookmarks-symbolic", 16, app);
}

/******************************************************************************
 * Name
 *   gui_setup_bookmark_view
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   void gui_setup_bookmark_view(GtkWidget * list_view)
 *
 * Description
 *   show the bookmark tree in LIST_VIEW: an icon, the color swatch of a
 *   folder (which has one when it is a tag) and the caption
 *
 * Return value
 *   void
 */

void gui_setup_bookmark_view(GtkWidget *list_view)
{
	ElimColumn cols[3];

	/* One image per row (never the pair of open and closed icons the
	 * folder had: a row keeps its icon whether it is open or not). */
	cols[0] = elim_column_image(COL_OPEN_PIXBUF, 16, NULL);
	cols[1] = elim_column_image(COL_DOT, 14, NULL);
	cols[2] = elim_column_text(COL_CAPTION);
	cols[2].text.expand = TRUE;
	elim_tree_setup_row_columns(list_view, bookmark_roots, cols, 3);
}

/******************************************************************************
 * Name
 *   drag and drop
 *
 * Description
 *   the root row stays where it is, and only a folder takes rows into it
 *
 * Return value
 *   void
 */

static gboolean bookmark_can_drop(GtkWidget *view, ElimRow *row, ElimRow *target,
				  ElimTreeDropPosition position, gpointer data)
{
	(void)view;
	(void)data;
	if (!elim_row_get_parent(row))
		return FALSE;
	if (position == ELIM_DROP_INTO)
		return !*elim_row_get_string(target, COL_KEY);
	return elim_row_get_parent(target) != NULL;
}

/******************************************************************************
 * Name
 *   button_release_event
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   gboolean button_release_event(GtkWidget * widget,
			    GuiButtonEvent * event, gpointer user_data)
 *
 * Description
 *   catch button 3 and select the row the pointer is over
 *   but does not display the bookmark
 *
 * Return value
 *   void
 */

static void lambda_open_url(GSimpleAction *action, GVariant *url,
			    gpointer data)
{
	(void)action;
	(void)data;
	main_url_handler(g_variant_get_string(url, NULL), TRUE);
}

static gboolean button_release_event(GtkWidget *widget,
				     GuiButtonEvent *event, gpointer data)
{
	ElimRow *selected = NULL;
	gboolean is_selected = FALSE;
	gboolean is_folder = FALSE;
	gchar *caption = NULL;
	gchar *key = NULL;
	gchar *module = NULL;
	gchar *mod_desc = NULL;
	gchar *range_start = NULL;
	gchar *description = NULL;
	button_one = FALSE;

	/* Every menu field below is initialized before selection or popup use. */
	gui_create_bookmark_menu();
	selected = bookmark_selected();
	if (selected) {
		caption = bookmark_row_dup(selected, COL_CAPTION);
		key = bookmark_row_dup(selected, COL_KEY);
		module = bookmark_row_dup(selected, COL_MODULE);
		mod_desc = bookmark_row_dup(selected, COL_MODULE_DESC);
		description = bookmark_row_dup(selected, COL_DESCRIPTION);
		is_folder = elim_row_n_children(selected) > 0;
		if (!is_folder && key != NULL) {
			gboolean multi = (strpbrk(key, "-;,") != NULL);
			gui_bookmark_menu_enable("en-pestana", !multi);
			gui_bookmark_menu_enable("en-dialogo", !multi);
			gui_bookmark_menu_enable("carpeta", FALSE);
			gui_bookmark_menu_enable("insertar", FALSE);
		} else {
			/* click on treeview folder to expand or collapse it, unless
			 * the click was on the arrow, which does that itself */
			if (event->button == 1 &&
			    !elim_tree_point_on_expander(bookmark_tree, event->x,
							 event->y)) {
				if (elim_tree_row_expanded(bookmark_tree, selected))
					elim_tree_collapse_row(bookmark_tree, selected);
				else
					elim_tree_expand_row(bookmark_tree, selected, FALSE);
			}

			gui_bookmark_menu_enable("en-pestana", FALSE);
			gui_bookmark_menu_enable("en-dialogo", FALSE);
			gui_bookmark_menu_enable("carpeta", TRUE);
			gui_bookmark_menu_enable("insertar", TRUE);
		}

		gui_bookmark_menu_enable("importar", TRUE);
		gui_bookmark_menu_enable("editar", TRUE);
		gui_bookmark_menu_enable("eliminar", TRUE);
		is_selected = TRUE;
	}

	if (!module || !strcmp(module, "")) {
		g_free(module);
		module = g_strdup(settings.MainWindowModule);
		/* Saved without a module: the key is in KJV numbering, not
		 * the selected Bible's (BOOKMARK-V11N-101). */
		if (key && is_selected && event->button == 1 &&
		    main_is_Bible_key(module, key)) {
			gchar *native = main_legacy_bookmark_key(key, module);
			if (!native) {
				main_warn_reference_unmapped(key, module);
				g_free(caption);
				g_free(key);
				g_free(module);
				g_free(mod_desc);
				g_free(description);
				return FALSE;
			}
			g_free(key);
			key = native;
		}
	}

	switch (event->button) {
	case 1:
		button_one = TRUE;
		break;
	case 2:
		gui_generic_warning(_("Opening a multi-reference bookmark in\n"
				      "separate tabs is not supported."));
		break;
	case 3:
		g_free(caption);
		g_free(key);
		g_free(module);
		g_free(mod_desc);
		g_free(description);
		/* GTK4-PORT-101 step 2: a GMenu popover; «open in tab» is
		 * left out of the model when tabs are off. */
		gboolean reordering = gui_bookmark_menu_reordering();
		if (reordering) {
			gui_bookmark_menu_enable("en-pestana", FALSE);
			gui_bookmark_menu_enable("en-dialogo", FALSE);
			gui_bookmark_menu_enable("carpeta", FALSE);
			gui_bookmark_menu_enable("insertar", FALSE);
			gui_bookmark_menu_enable("editar", FALSE);
			gui_bookmark_menu_enable("eliminar", FALSE);
		}
		gui_bookmark_menu_enable("importar", TRUE);
		gui_bookmark_menu_popup(GTK_WIDGET(bookmark_tree));
		return reordering;
		break;
	}
	if (is_selected) {
		if (!is_folder && key != NULL) {
			const gchar *real_mod = main_abbrev_to_name(module);
			/* multi if contains ";", "-" (range) or comma between verse numbers */
			gboolean multi = (strchr(key, ';') != NULL);
			gboolean is_range = FALSE;
			if (!multi && strchr(key, '-')) {
				/* verse range: navigate to first verse directly */
				const gchar *colon = strchr(key, ':');
				const gchar *dash  = strchr(key, '-');
				if (colon && dash && dash > colon) {
					is_range = TRUE;
					range_start = g_strndup(key, dash - key);
				}
			}
			if (!multi && strchr(key, ',')) {
				/* check if comma separates verses: "Eph 2:8,9" */
				const gchar *colon = strchr(key, ':');
				const gchar *comma = strchr(key, ',');
				if (colon && comma && comma > colon)
					multi = TRUE;
			}

			if (is_range && button_one && range_start) {
				/* navigate directly to first verse of range */
				gchar *url = g_strdup_printf(
					"passagestudy.jsp?action=showBookmark&"
					"type=%s&value=%s&module=%s",
					"currentTab",
					main_url_encode(range_start),
					main_url_encode((real_mod ? real_mod : module)));
				main_url_handler(url, TRUE);
				g_free(url);
				g_free(range_start);
				range_start = NULL;
			} else if (multi && button_one && settings.crossref_popup) {
				GList *refs = main_parse_verse_list(module, key,
								    settings.currentverse);
				GMenu *popup = g_menu_new();
				for (GList *l = refs; l; l = l->next) {
					const gchar *ref = (const gchar *)l->data;
					GMenuItem *item = g_menu_item_new(ref, NULL);
					gchar *url = g_strdup_printf(
						"passagestudy.jsp?action=showBookmark&"
						"type=%s&value=%s&module=%s",
						"currentTab",
						main_url_encode(ref),
						main_url_encode((real_mod ? real_mod : module)));
					g_menu_item_set_action_and_target_value(
					    item, "referencias.ir",
					    g_variant_new_string(url));
					g_menu_append_item(popup, item);
					g_object_unref(item);
					g_free(url);
				}
				g_list_free_full(refs, g_free);
				gui_insert_single_action(widget, "referencias", "ir",
							 G_VARIANT_TYPE_STRING,
							 G_CALLBACK(lambda_open_url),
							 NULL);
				gui_popup_menu_model_at_pointer(G_MENU_MODEL(popup),
								widget);
				g_object_unref(popup);
			} else if (multi && button_one && !settings.crossref_popup) {
				gchar *url = g_strdup_printf(
					"passagestudy.jsp?action=showBookmark&"
					"type=%s&value=%s&module=%s",
					"currentTab",
					main_url_encode(key),
					main_url_encode((real_mod ? real_mod : module)));
				main_url_handler(url, TRUE);
				g_free(url);
			} else {
				gchar *url = NULL;
				if (!strcmp(module, "studypad"))
					url = g_strdup_printf(
						"passagestudy.jsp?action=showStudypad&"
						"type=9&value=%s&module=%s",
						main_url_encode(key),
						main_url_encode((real_mod ? real_mod : module)));
				else if (button_one)
					url = g_strdup_printf(
						"passagestudy.jsp?action=showBookmark&"
						"type=%s&value=%s&module=%s",
						"currentTab",
						main_url_encode(key),
						main_url_encode((real_mod ? real_mod : module)));
				if (url) {
					main_url_handler(url, TRUE);
					g_free(url);
				}
			}
		}
		g_free(caption);
		g_free(range_start);
		g_free(key);
		g_free(module);
		g_free(mod_desc);
		g_free(description);
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   gui_create_bookmark_tree
 *
 * Synopsis
 *   #include "gui/bookmarks_treeview.h"
 *
 *   GtkWidget *gui_create_bookmark_tree(void)
 *
 * Description
 *
 *
 * Return value
 *   GtkWidget*
 */

/**
 * bookmark_get_tag_color_for_key:
 * @osiskey: a verse key string, e.g. "Gen 1:1"
 *
 * Walks the bookmark tree and returns the color of the
 * first tag-group folder that contains a bookmark matching @osiskey.
 * Returns NULL if none found.  Caller must g_free() the result.
 */
static void debug_dump_recursive(ElimRow *parent, int depth)
{
	guint i;

	for (i = 0; i < elim_row_n_children(parent); i++) {
		ElimRow *child = elim_row_get_child(parent, i);

		debug_dump_recursive(child, depth + 1);
	}
}

void bookmark_debug_dump_colors(void)
{
	if (!bookmark_roots || !g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)))
		return;
	debug_dump_recursive(elim_table_get(bookmark_roots, 0), 0);
}


/* Recursively search @folder (and its sub-folders) for a bookmark whose
 * key resolves to @versekey_text. On match, returns "<immediate parent
 * folder caption>: <bookmark caption>". Helper for
 * bookmark_get_tag_info_for_key(). */
static gchar *bookmark_find_in_folder(ElimRow *folder, const gchar *versekey_text)
{
	const gchar *folder_caption = elim_row_get_string(folder, COL_CAPTION);
	gchar *result = NULL;
	guint i;

	for (i = 0; !result && i < elim_row_n_children(folder); i++) {
		ElimRow *child = elim_row_get_child(folder, i);
		gchar *node_key = bookmark_row_dup(child, COL_KEY);
		const gchar *node_caption = elim_row_get_string(child, COL_CAPTION);

		if (node_key) {
			/* leaf: a bookmark. Resolve its (possibly
			 * multi-reference) key via Sword and compare
			 * each resolved verse to versekey_text. */
			GList *verses = main_parse_verse_list(
				settings.MainWindowModule, node_key,
				(char *)settings.currentverse);
			for (GList *l = verses; l && !result; l = l->next) {
				gchar *v = g_strstrip(g_strdup((const char *)l->data));
				gchar *q = g_strstrip(g_strdup(versekey_text));
				if (!g_ascii_strcasecmp(v, q))
					result = g_strdup_printf("%s: %s",
						folder_caption,
						*node_caption ? node_caption : node_key);
				g_free(v);
				g_free(q);
			}
			for (GList *l = verses; l; l = l->next)
				g_free(l->data);
			g_list_free(verses);
		} else {
			/* sub-folder: recurse */
			result = bookmark_find_in_folder(child, versekey_text);
		}
		g_free(node_key);
	}
	return result;
}
gchar *bookmark_get_tag_color_for_key(const gchar *osiskey)
{
	guint i, j;

	if (!osiskey || !bookmark_roots)
		return NULL;

	/* iterate top-level folders */
	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)); i++) {
		ElimRow *folder = elim_table_get(bookmark_roots, i);
		const gchar *folder_color = elim_row_get_string(folder, COL_COLOR);

		if (!*folder_color)
			continue;
		/* scan children of this colored folder */
		for (j = 0; j < elim_row_n_children(folder); j++) {
			ElimRow *child = elim_row_get_child(folder, j);
			gchar *key = bookmark_row_dup(child, COL_KEY);
			/* compare case-insensitively;
			 * also try stripping trailing spaces */
			gchar *k = key ? g_strstrip(g_strdup(key)) : NULL;
			gchar *q = osiskey ? g_strstrip(g_strdup(osiskey)) : NULL;
			gboolean match = (k && q && !g_ascii_strcasecmp(k, q));
			g_free(k);
			g_free(q);
			g_free(key);
			if (match)
				return g_strdup(folder_color);
		}
	}

	return NULL;
}
/**
 * bookmark_get_tag_info_for_key:
 * @versekey_text: a Sword-formatted verse key string, e.g. "Rom 16:23"
 *
 * Finds the first bookmark, anywhere in the tree, whose (possibly
 * multi-reference) key resolves to @versekey_text, and returns
 * "<containing folder>: <bookmark name>" -- just the single
 * immediate parent folder, not the full nested path.
 * Returns NULL if no bookmark matches.  Caller must g_free() the result.
 */
gchar *bookmark_get_tag_info_for_key(const gchar *versekey_text)
{
	guint i;

	if (!versekey_text || !bookmark_roots)
		return NULL;
	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(bookmark_roots)); i++) {
		gchar *result = bookmark_find_in_folder(elim_table_get(bookmark_roots, i),
							versekey_text);

		if (result)
			return result;
	}
	return NULL;
}

GtkWidget *gui_create_bookmark_tree(void)
{
	GtkWidget *tree = gtk_list_view_new(NULL, NULL);
	BOOKMARK_DATA root = { 0 };
	ElimRow *row;

	create_pixbufs();
	bookmark_roots = elim_table_new();
	gui_setup_bookmark_view(tree);

	root.caption = (gchar *)_("Bookmarks");
	root.opened = bm_pixbufs->pixbuf_opened;
	root.closed = bm_pixbufs->pixbuf_closed;
	row = gui_add_item_to_tree(NULL, &root);

	load_xml_bookmarks(tree, row);

	gui_widget_on_button(GTK_WIDGET(tree), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)button_release_event, GINT_TO_POINTER(0));
	use_dialog = FALSE;
	bookmark_tree = tree;
	/* rows can be dragged once the reader allows it (menu: Allow reordering) */
	elim_tree_set_drag_funcs(tree, bookmark_can_drop, NULL, NULL);

	/* from here on the file follows the tree */
	elim_tree_set_changed_func(bookmark_roots, bookmarks_tree_changed, NULL);

	return tree;
}
