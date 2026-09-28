/*
 * Biblia Elim
 * table_helpers.h - a GtkColumnView over a GListStore of rows, used the way
 *                   the interface used GtkTreeView over a GtkListStore
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

#ifndef GUI_TABLE_HELPERS_H
#define GUI_TABLE_HELPERS_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GtkTreeView, GtkListStore and GtkCellRenderer are deprecated. A flat list
 * of rows is a GtkColumnView over a GListStore whose items are ElimRow: a
 * row holds a fixed number of columns, each a string or an int, addressed by
 * number the way the list store's columns were. A column of the view shows
 * one of those columns.
 *
 * Differences from the tree view that callers must know:
 *  - a row is an object: keep the ElimRow, not an iterator, and change it
 *    with elim_row_set_*() (the view follows) or replace it in the store;
 *  - the selection lives in the GtkSingleSelection the view was made with
 *    (elim_table_get_selected());
 *  - there is no per-cell renderer; a column shows its text, and a column
 *    that needs more (a progress bar, an icon) is a GtkListItemFactory of the
 *    caller's own added with gtk_column_view_append_column(). */

#define ELIM_TYPE_ROW (elim_row_get_type())
G_DECLARE_FINAL_TYPE(ElimRow, elim_row, ELIM, ROW, GObject)

/* A row of N_COLUMNS columns, all empty (an empty string, 0). */
ElimRow *elim_row_new(guint n_columns);
guint elim_row_n_columns(ElimRow *row);

/* Setting a column notifies "changed", which the views listen to. */
void elim_row_set_string(ElimRow *row, guint column, const char *value);
void elim_row_set_int(ElimRow *row, guint column, gint value);
void elim_row_set_double(ElimRow *row, guint column, gdouble value);

/* The string of COLUMN ("" when it holds none); owned by the row. */
const char *elim_row_get_string(ElimRow *row, guint column);
/* The int of COLUMN, 0 when the column holds a string. */
gint elim_row_get_int(ElimRow *row, guint column);
gdouble elim_row_get_double(ElimRow *row, guint column);
/* COLUMN as text: its string, or its int printed; new string. */
char *elim_row_dup_text(ElimRow *row, guint column);

/* An empty store of rows. */
GListStore *elim_table_new(void);

/* What a column of text shows and how. Start from elim_text_column(). */
typedef struct {
	guint column;		/* the row column shown */
	gboolean expand;	/* takes the spare width */
	gint fixed_width;	/* where it starts, pixels; 0 for the natural width */
	gint tooltip_column;	/* the row column whose text is the cell's tooltip; -1 none */
	gboolean markup;	/* the text is Pango markup */
	gint weight_column;	/* an int row column holding a PangoWeight; -1 none */
	gint pad_y;		/* room above and below the text, pixels */
	gboolean wrap;		/* long text wraps onto more lines instead of being
				 * cut short (elim_table_setup_rows() only) */
	gint foreground_column; /* a row string with the cell text colour; -1 none */
	gfloat xalign;          /* 0 left, 1 right */
} ElimTextColumn;

static inline ElimTextColumn
elim_text_column(guint column)
{
 ElimTextColumn spec = { column, FALSE, 0, -1, FALSE, -1, 0, FALSE, -1, 0.0f };

	return spec;
}

/* Makes VIEW (a GtkColumnView or a GtkListView, from a GtkBuilder file say)
 * show STORE. One row can be selected and none is until the reader picks
 * one; elim_table_set_browse() changes that. The view keeps the store
 * alive. A GtkListView has one column: SPEC says what it shows (a
 * GtkColumnView gets its columns from elim_table_add_*()). */
void elim_table_setup(GtkWidget *view, GListStore *store);
/* As elim_table_setup(), with the GtkColumnView's header sorters applied. */
void elim_table_setup_sortable(GtkWidget *view, GListStore *store);
void elim_table_setup_list(GtkWidget *list_view, GListStore *store,
			   const ElimTextColumn *spec);

/* Makes LIST_VIEW (a GtkListView) show STORE with no header, each row laying
 * out N_SPECS columns side by side, the way a tree view with its headers
 * hidden did: a column is as wide as its widest cell, so the columns line up
 * from row to row. The column of a SPEC with EXPAND takes the spare width. */
void elim_table_setup_rows(GtkWidget *list_view, GListStore *store,
			   const ElimTextColumn *specs, guint n_specs);

typedef void (*ElimTableReorderedFunc)(GtkWidget *view, gpointer data);

/* As elim_table_setup_rows(), and lets the reader drag a row before or after
 * another row. REORDERED runs after the store has its new order. */
void elim_table_setup_reorderable_rows(GtkWidget *list_view, GListStore *store,
				       const ElimTextColumn *specs, guint n_specs,
				       ElimTableReorderedFunc reordered, gpointer data);

/* Replaces every row of STORE with the N_ROWS of ROWS in one change: the
 * views update once, which matters for a long list. The store takes its own
 * references; the caller keeps its own. */
void elim_table_replace(GListStore *store, ElimRow **rows, guint n_rows);

/* A new column view of STORE, as elim_table_setup() makes it, no columns. */
GtkWidget *elim_table_view_new(GListStore *store);

/* With BROWSE a row is always selected as long as there is one (the first
 * after a refill), like the tree view's browse mode. */
void elim_table_set_browse(GtkWidget *view, gboolean browse);

GtkColumnViewColumn *elim_table_add_column(GtkWidget *view, const char *title,
					   const ElimTextColumn *spec);

/* Adds a column titled TITLE showing COLUMN of every row as text. EXPAND lets
 * it take the spare width. */
GtkColumnViewColumn *elim_table_add_text_column(GtkWidget *view,
						const char *title,
						guint column,
						gboolean expand);

typedef enum {
	ELIM_TABLE_SORT_STRING,
	ELIM_TABLE_SORT_INT,
	ELIM_TABLE_SORT_DOUBLE
} ElimTableSortType;

/* Adds SPEC and lets its header sort the view by SORT_COLUMN. */
GtkColumnViewColumn *elim_table_add_sortable_column(
	GtkWidget *view, const char *title, const ElimTextColumn *spec,
	guint sort_column, ElimTableSortType sort_type);

/* Called after the reader clicked the check box of ROW. The box shows what
 * the row's COLUMN (an int, 0 or 1) holds once this returns, so a function
 * that leaves the row alone leaves the box as it was. */
typedef void (*ElimToggleFunc)(ElimRow *row, gpointer data);

/* Adds a column of check boxes showing COLUMN of every row. */
GtkColumnViewColumn *elim_table_add_toggle_column(GtkWidget *view,
						  const char *title,
						  guint column,
						  ElimToggleFunc toggled,
						  gpointer data);

/* Row POSITION of STORE, NULL when there is none; owned by the store. */
ElimRow *elim_table_get(GListStore *store, guint position);

/* Moves ROW before or after TARGET in VIEW's store. FALSE when either row is
 * absent or they are the same. This is also the operation used by row drag. */
gboolean elim_table_move_row(GtkWidget *view, ElimRow *row, ElimRow *target,
				     gboolean after);

/* Removes ROW by identity, independent of the current selection order. */
gboolean elim_table_remove_row(GtkWidget *view, ElimRow *row);

/* The selection of VIEW: connect to "notify::selected" to hear the reader
 * pick a row (it also fires when the program does, or when the store is
 * refilled in browse mode). */
GtkSingleSelection *elim_table_selection(GtkWidget *view);

/* The selected row of VIEW, NULL when none; owned by the store. */
ElimRow *elim_table_get_selected(GtkWidget *view);

/* Its position, GTK_INVALID_LIST_POSITION when none. */
guint elim_table_get_selected_position(GtkWidget *view);

/* Selects row POSITION and, with SCROLL, brings it into view. */
void elim_table_select(GtkWidget *view, guint position, gboolean scroll);

/* The store a view made by elim_table_view_new() shows. */
GListStore *elim_table_get_store(GtkWidget *view);

/* The reader activated a row (double click, Enter): connect to "activate"
 * of the view, which passes the row's position. */

#ifdef __cplusplus
}
#endif

#endif /* GUI_TABLE_HELPERS_H */
