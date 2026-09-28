/*
 * Biblia Elim
 * entry_suggest.c - suggestions under a text entry
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

#include "gui/entry_suggest.h"

#include "gui/table_helpers.h"

#define SUGGEST_MAX_ROWS 12
#define SUGGEST_MAX_HEIGHT 240

struct _ElimEntrySuggest {
	GtkWidget *entry;
	GtkWidget *popover;
	GtkWidget *scroll;
	GtkWidget *list;
	GListStore *shown;
	GPtrArray *candidates;
	ElimSuggestMatchFunc match;
	gpointer match_data;
	gboolean updating;	/* the entry is being set from a pick */
};

static gboolean
suggest_is_open(ElimEntrySuggest *s)
{
	return s->popover && gtk_widget_get_visible(s->popover);
}

static void
suggest_close(ElimEntrySuggest *s)
{
	if (!s->popover)
		return;
	if (suggest_is_open(s))
		gtk_popover_popdown(GTK_POPOVER(s->popover));
	elim_tree_unselect(s->list);
}

static gboolean
entry_has_focus(ElimEntrySuggest *s)
{
	GtkRoot *root = gtk_widget_get_root(s->entry);
	GtkWidget *focus = root ? gtk_root_get_focus(root) : NULL;

	return focus && (focus == s->entry || gtk_widget_is_ancestor(focus, s->entry));
}

static void
suggest_refresh(ElimEntrySuggest *s)
{
	const char *key = gtk_editable_get_text(GTK_EDITABLE(s->entry));
	ElimRow *rows[SUGGEST_MAX_ROWS];
	guint n = 0, i;

	/* only what the reader types asks for suggestions, not a program that
	 * fills the entry in */
	if (s->updating || !*key || !entry_has_focus(s)) {
		suggest_close(s);
		return;
	}
	for (i = 0; i < s->candidates->len && n < SUGGEST_MAX_ROWS; i++) {
		const char *candidate = g_ptr_array_index(s->candidates, i);

		if (!s->match(key, candidate, s->match_data))
			continue;
		rows[n] = elim_row_new(1);
		elim_row_set_string(rows[n], 0, candidate);
		n++;
	}
	elim_table_replace(s->shown, rows, n);
	for (i = 0; i < n; i++)
		g_object_unref(rows[i]);
	elim_tree_unselect(s->list);
	if (!n) {
		suggest_close(s);
		return;
	}
	gtk_widget_set_size_request(s->scroll, gtk_widget_get_width(s->entry), -1);
	if (!suggest_is_open(s))
		gtk_popover_popup(GTK_POPOVER(s->popover));
}

static void
suggest_pick(ElimEntrySuggest *s, ElimRow *row)
{
	s->updating = TRUE;
	gtk_editable_set_text(GTK_EDITABLE(s->entry), elim_row_get_string(row, 0));
	gtk_editable_set_position(GTK_EDITABLE(s->entry), -1);
	s->updating = FALSE;
	suggest_close(s);
	gtk_widget_grab_focus(s->entry);
}

static void
on_changed(GtkEditable *editable, gpointer data)
{
	(void)editable;
	suggest_refresh(data);
}

static void
on_list_activate(GtkWidget *list, guint position, gpointer data)
{
	ElimRow *row = elim_table_row_at(list, position);

	if (row)
		suggest_pick(data, row);
}

/* Enter takes the suggestion the reader moved to, and is the entry's own
 * "activate" when none was. This runs before the program's handlers. */
static void
on_activate(GtkWidget *entry, gpointer data)
{
	ElimEntrySuggest *s = data;
	ElimRow *row = suggest_is_open(s) ? elim_table_get_selected(s->list) : NULL;

	if (!row)
		return;
	suggest_pick(s, row);
	g_signal_stop_emission_by_name(entry, "activate");
}

static gboolean
on_key_pressed(GtkEventControllerKey *controller, guint keyval, guint keycode,
	       GdkModifierType state, gpointer data)
{
	ElimEntrySuggest *s = data;
	guint n, position;

	(void)controller;
	(void)keycode;
	(void)state;
	if (!suggest_is_open(s))
		return FALSE;
	n = g_list_model_get_n_items(G_LIST_MODEL(s->shown));
	position = elim_table_get_selected_position(s->list);
	switch (keyval) {
	case GDK_KEY_Down:
		elim_table_select(s->list,
				  position == GTK_INVALID_LIST_POSITION ? 0
				  : position + 1 < n		      ? position + 1
								      : n - 1,
				  TRUE);
		return TRUE;
	case GDK_KEY_Up:
		if (position == GTK_INVALID_LIST_POSITION || position == 0)
			elim_tree_unselect(s->list);
		else
			elim_table_select(s->list, position - 1, TRUE);
		return TRUE;
	case GDK_KEY_Escape:
		suggest_close(s);
		return TRUE;
	default:
		return FALSE;
	}
}

static void
on_focus_leave(GtkEventControllerFocus *controller, gpointer data)
{
	ElimEntrySuggest *s = data;
	GtkRoot *root = gtk_widget_get_root(s->entry);
	GtkWidget *focus = root ? gtk_root_get_focus(root) : NULL;

	(void)controller;
	/* clicking a suggestion moves the focus into the list */
	if (s->popover && focus && gtk_widget_is_ancestor(focus, s->popover))
		return;
	suggest_close(s);
}

static void
on_entry_destroy(GtkWidget *entry, gpointer data)
{
	ElimEntrySuggest *s = data;

	(void)entry;
	gtk_widget_unparent(s->popover);
	s->popover = NULL;
}

static void
suggest_free(gpointer data)
{
	ElimEntrySuggest *s = data;

	g_ptr_array_free(s->candidates, TRUE);
	g_object_unref(s->shown);
	g_free(s);
}

ElimEntrySuggest *
elim_entry_suggest_new(GtkWidget *entry, ElimSuggestMatchFunc match, gpointer data)
{
	ElimEntrySuggest *s = g_new0(ElimEntrySuggest, 1);
	ElimTextColumn column = elim_text_column(0);
	GtkEventController *keys = gtk_event_controller_key_new();
	GtkEventController *focus = gtk_event_controller_focus_new();

	g_return_val_if_fail(GTK_IS_EDITABLE(entry), NULL);
	s->entry = entry;
	s->match = match;
	s->match_data = data;
	s->candidates = g_ptr_array_new_with_free_func(g_free);
	s->shown = elim_table_new();

	s->list = gtk_list_view_new(NULL, NULL);
	elim_table_setup_list(s->list, s->shown, &column);
	gtk_list_view_set_single_click_activate(GTK_LIST_VIEW(s->list), TRUE);
	g_signal_connect(s->list, "activate", G_CALLBACK(on_list_activate), s);

	s->scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s->scroll), s->list);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(s->scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(s->scroll), TRUE);
	gtk_scrolled_window_set_max_content_height(GTK_SCROLLED_WINDOW(s->scroll),
						   SUGGEST_MAX_HEIGHT);

	/* the entry keeps the focus: the list only shows */
	s->popover = gtk_popover_new();
	gtk_popover_set_child(GTK_POPOVER(s->popover), s->scroll);
	gtk_popover_set_autohide(GTK_POPOVER(s->popover), FALSE);
	gtk_popover_set_has_arrow(GTK_POPOVER(s->popover), FALSE);
	gtk_popover_set_position(GTK_POPOVER(s->popover), GTK_POS_BOTTOM);
	gtk_widget_set_halign(s->popover, GTK_ALIGN_START);
	gtk_widget_set_parent(s->popover, entry);

	gtk_event_controller_set_propagation_phase(keys, GTK_PHASE_CAPTURE);
	g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key_pressed), s);
	gtk_widget_add_controller(entry, keys);
	g_signal_connect(focus, "leave", G_CALLBACK(on_focus_leave), s);
	gtk_widget_add_controller(entry, focus);
	g_signal_connect(entry, "changed", G_CALLBACK(on_changed), s);
	g_signal_connect(entry, "activate", G_CALLBACK(on_activate), s);
	g_signal_connect(entry, "destroy", G_CALLBACK(on_entry_destroy), s);
	g_object_set_data_full(G_OBJECT(entry), "elim-entry-suggest", s, suggest_free);
	return s;
}

void
elim_entry_suggest_set_candidates(ElimEntrySuggest *s, const GList *candidates)
{
	const GList *l;

	g_ptr_array_set_size(s->candidates, 0);
	for (l = candidates; l; l = l->next)
		g_ptr_array_add(s->candidates, g_strdup(l->data));
}

GtkWidget *
elim_entry_suggest_get_list(ElimEntrySuggest *s)
{
	return s->list;
}

gboolean
elim_entry_suggest_is_open(ElimEntrySuggest *s)
{
	return suggest_is_open(s);
}
