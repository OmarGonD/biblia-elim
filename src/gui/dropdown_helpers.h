/*
 * Biblia Elim
 * dropdown_helpers.h - a GtkDropDown used the way the interface used
 *                      GtkComboBoxText
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

#ifndef GUI_DROPDOWN_HELPERS_H
#define GUI_DROPDOWN_HELPERS_H

#include <gtk/gtk.h>
#include <string.h>

/* GtkComboBox and GtkComboBoxText are deprecated: a list to pick one entry
 * from is a GtkDropDown over a GtkStringList. A dropdown made here keeps an
 * optional id per row next to its text (what gtk_combo_box_text_append()'s
 * ID was) and answers "which row" as a plain int, -1 for none.
 *
 * Differences from the combo box that callers must know:
 *  - a dropdown with rows always has one selected; appending the first row
 *    selects it, which emits "notify::selected" (a combo box stayed empty
 *    until told). Block the handler, or connect it afterwards, while filling.
 *    "Nothing chosen" therefore needs a row of its own (a "none" or "choose"
 *    row first in the list); there is no way to unselect.
 *  - the change signal is "notify::selected" (GObject *, GParamSpec *,
 *    gpointer), not "changed". */

#define ELIM_DROPDOWN_IDS_KEY "elim-dropdown-ids"

static inline GPtrArray *
elim_dropdown_ids(GtkDropDown *dd)
{
	GPtrArray *ids = (GPtrArray *)g_object_get_data(G_OBJECT(dd),
							ELIM_DROPDOWN_IDS_KEY);

	if (!ids) {
		ids = g_ptr_array_new_with_free_func(g_free);
		g_object_set_data_full(G_OBJECT(dd), ELIM_DROPDOWN_IDS_KEY,
				       ids, (GDestroyNotify)g_ptr_array_unref);
	}
	return ids;
}

/* The string list behind DD, made on first use. */
static inline GtkStringList *
elim_dropdown_strings(GtkDropDown *dd)
{
	GListModel *model = gtk_drop_down_get_model(dd);

	if (!model || !GTK_IS_STRING_LIST(model)) {
		GtkStringList *list = gtk_string_list_new(NULL);

		gtk_drop_down_set_model(dd, G_LIST_MODEL(list));
		g_object_unref(list);
		model = gtk_drop_down_get_model(dd);
	}
	return GTK_STRING_LIST(model);
}

/* Makes a dropdown from the GtkBuilder (or any other) source ready for the
 * functions below: it shows plain strings and knows no ids. */
static inline void
elim_dropdown_prepare(GtkDropDown *dd)
{
	(void)elim_dropdown_strings(dd);
	(void)elim_dropdown_ids(dd);
}

/* A dropdown that shows plain strings. */
static inline GtkWidget *
elim_dropdown_new(void)
{
	GtkWidget *dd = gtk_drop_down_new(NULL, NULL);

	elim_dropdown_prepare(GTK_DROP_DOWN(dd));
	return dd;
}

static inline guint
elim_dropdown_n_items(GtkDropDown *dd)
{
	GListModel *model = gtk_drop_down_get_model(dd);

	return model ? g_list_model_get_n_items(model) : 0;
}

/* Every row goes, and so does the selection. */
static inline void
elim_dropdown_remove_all(GtkDropDown *dd)
{
	GtkStringList *list = elim_dropdown_strings(dd);

	gtk_string_list_splice(list, 0, g_list_model_get_n_items(G_LIST_MODEL(list)),
			       NULL);
	g_ptr_array_set_size(elim_dropdown_ids(dd), 0);
}

/* Adds a row showing TEXT and known as ID (NULL for none). */
static inline void
elim_dropdown_append(GtkDropDown *dd, const char *id, const char *text)
{
	GPtrArray *ids = elim_dropdown_ids(dd);
	GtkStringList *list = elim_dropdown_strings(dd);

	/* rows a GtkBuilder file listed carry no id: pad up to this one, and
	 * do it before the row exists, so nothing sees it without its id */
	while (ids->len < g_list_model_get_n_items(G_LIST_MODEL(list)))
		g_ptr_array_add(ids, NULL);
	g_ptr_array_add(ids, g_strdup(id));
	gtk_string_list_append(list, text ? text : "");
}

/* The selected row, -1 when none. */
static inline gint
elim_dropdown_get_active(GtkDropDown *dd)
{
	guint sel = gtk_drop_down_get_selected(dd);

	return sel == GTK_INVALID_LIST_POSITION ? -1 : (gint)sel;
}

/* Selects ROW. A row that is not there (-1 included) changes nothing: a
 * dropdown that has rows cannot be left with none selected. */
static inline void
elim_dropdown_set_active(GtkDropDown *dd, gint row)
{
	if (row >= 0 && (guint)row < elim_dropdown_n_items(dd))
		gtk_drop_down_set_selected(dd, (guint)row);
}

/* The text of the selected row, or NULL; owned by the dropdown, valid until
 * its rows change. */
static inline const char *
elim_dropdown_get_active_text(GtkDropDown *dd)
{
	gint row = elim_dropdown_get_active(dd);

	if (row < 0)
		return NULL;
	return gtk_string_list_get_string(elim_dropdown_strings(dd), (guint)row);
}

/* Lets the reader type in the open list to narrow it down (a long list such
 * as the books of the Bible). */
static inline void
elim_dropdown_enable_search(GtkDropDown *dd)
{
	gtk_drop_down_set_expression(dd,
				     gtk_property_expression_new(GTK_TYPE_STRING_OBJECT,
								 NULL, "string"));
	gtk_drop_down_set_enable_search(dd, TRUE);
}

static inline void
elim_dropdown_label_setup(GtkSignalListItemFactory *factory, GtkListItem *item,
			  gpointer data)
{
	GtkWidget *label = gtk_label_new(NULL);

	(void)factory;
	(void)data;
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_list_item_set_child(item, label);
}

static inline void
elim_dropdown_label_bind(GtkSignalListItemFactory *factory, GtkListItem *item,
			 gpointer data)
{
	GtkStringObject *row = GTK_STRING_OBJECT(gtk_list_item_get_item(item));

	(void)factory;
	(void)data;
	gtk_label_set_text(GTK_LABEL(gtk_list_item_get_child(item)),
			   gtk_string_object_get_string(row));
}

/* Like elim_dropdown_enable_search(), but what the reader types is matched
 * against a text of its own for each row (a name and its abbreviation, say)
 * while the rows still show only their string. TEXT_FOR_ROW is
 * gchar *(GtkStringObject *row, gpointer data) and returns a new string. */
static inline void
elim_dropdown_enable_search_with(GtkDropDown *dd, GCallback text_for_row,
				 gpointer data)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();

	g_signal_connect(factory, "setup", G_CALLBACK(elim_dropdown_label_setup),
			 NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(elim_dropdown_label_bind),
			 NULL);
	gtk_drop_down_set_factory(dd, factory);
	g_object_unref(factory);
	gtk_drop_down_set_expression(dd,
				     gtk_cclosure_expression_new(G_TYPE_STRING, NULL, 0,
								 NULL, text_for_row, data,
								 NULL));
	gtk_drop_down_set_enable_search(dd, TRUE);
}

/* The text of the selected row, "" when none is selected. */
static inline const char *
elim_dropdown_get_active_text_or_empty(GtkDropDown *dd)
{
	const char *text = elim_dropdown_get_active_text(dd);

	return text ? text : "";
}

/* The text of ROW, or NULL. */
static inline const char *
elim_dropdown_get_text(GtkDropDown *dd, gint row)
{
	if (row < 0 || (guint)row >= elim_dropdown_n_items(dd))
		return NULL;
	return gtk_string_list_get_string(elim_dropdown_strings(dd), (guint)row);
}

/* The id of the selected row, or NULL when none is selected or the row has
 * no id; owned by the dropdown. */
static inline const char *
elim_dropdown_get_active_id(GtkDropDown *dd)
{
	GPtrArray *ids = elim_dropdown_ids(dd);
	gint row = elim_dropdown_get_active(dd);

	if (row < 0 || (guint)row >= ids->len)
		return NULL;
	return (const char *)g_ptr_array_index(ids, (guint)row);
}

/* Selects the row known as ID; FALSE, and nothing changes, when there is
 * none. */
static inline gboolean
elim_dropdown_set_active_id(GtkDropDown *dd, const char *id)
{
	GPtrArray *ids = elim_dropdown_ids(dd);
	guint i;

	if (!id)
		return FALSE;
	for (i = 0; i < ids->len; i++) {
		const char *have = (const char *)g_ptr_array_index(ids, i);

		if (have && strcmp(have, id) == 0) {
			gtk_drop_down_set_selected(dd, i);
			return TRUE;
		}
	}
	return FALSE;
}

/* The row whose text is TEXT, -1 when none. */
static inline gint
elim_dropdown_find_text(GtkDropDown *dd, const char *text)
{
	guint i, n = elim_dropdown_n_items(dd);

	for (i = 0; text && i < n; i++)
		if (strcmp(gtk_string_list_get_string(elim_dropdown_strings(dd), i),
			   text) == 0)
			return (gint)i;
	return -1;
}

#endif /* GUI_DROPDOWN_HELPERS_H */
