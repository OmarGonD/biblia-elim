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

/* A column can also hold an object, an image usually (a GdkPaintable). The row
 * takes its own reference; NULL empties the column. */
void elim_row_set_object(ElimRow *row, guint column, GObject *value);
/* A pixbuf is kept as a texture, which is what an image shows. */
void elim_row_set_pixbuf(ElimRow *row, guint column, GdkPixbuf *pixbuf);
/* Owned by the row; NULL when the column holds anything else. */
GObject *elim_row_get_object(ElimRow *row, guint column);

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
	gint fixed_width;	/* where it starts, pixels; 0 for the natural width (in
				 * a row of a list view: the least it takes) */
	gint tooltip_column;	/* the row column whose text is the cell's tooltip; -1 none */
	gboolean markup;	/* the text is Pango markup */
	gint weight_column;	/* an int row column holding a PangoWeight; -1 none */
	gint pad_y;		/* room above and below the text, pixels */
	gboolean wrap;		/* long text wraps onto more lines instead of being
				 * cut short (elim_table_setup_rows() only) */
	gint foreground_column; /* a row string with the cell text colour; -1 none */
	gfloat xalign;          /* 0 left, 1 right */
	gboolean expander;	/* the column of a tree that holds the expanders and
				 * indents its rows by depth (elim_tree_setup()) */
} ElimTextColumn;

static inline ElimTextColumn
elim_text_column(guint column)
{
 ElimTextColumn spec = { column, FALSE, 0, -1, FALSE, -1, 0, FALSE, -1, 0.0f, FALSE };

	return spec;
}

/* Called after the reader clicked the check box of ROW. The box shows what
 * the row's COLUMN (an int, 0 or 1) holds once this returns, so a function
 * that leaves the row alone leaves the box as it was. */
typedef void (*ElimToggleFunc)(ElimRow *row, gpointer data);

/* What a column shows in each row. A column of a table (a GtkColumnView
 * column) and a column of a row of a list view (elim_table_setup_row_columns())
 * are made of the same kinds of cell. */
typedef enum {
	ELIM_COLUMN_TEXT,
	ELIM_COLUMN_TOGGLE,	/* a check box: an int row column, 0 or 1 */
	ELIM_COLUMN_IMAGE,	/* an image: a GdkPaintable or a themed icon name */
	ELIM_COLUMN_PROGRESS	/* a progress bar: an int row column, 0 to 100 */
} ElimColumnKind;

typedef struct {
	ElimColumnKind kind;
	/* The layout of every kind (expand, fixed_width, pad_y, expander) and
	 * for TEXT what the cell shows. text.column is the row column any kind
	 * shows. */
	ElimTextColumn text;
	/* TOGGLE: the box shows its mixed state for a row whose int column
	 * here is not 0; -1 never. */
	gint inconsistent_column;
	/* Any kind: the cell is shown for a row whose int column here is not
	 * 0, and stays as a blank (the columns line up) for one whose is 0; -1
	 * always shown. */
	gint visible_column;
	gint pixel_size;	/* IMAGE: 0 keeps the icon's own size */
	const char *tooltip;	/* TOGGLE, IMAGE: the tooltip of every cell of the
				 * column; copied */
	gint progress_text_column; /* PROGRESS: the row column of the text on the
				 * bar; -1 none */
	ElimToggleFunc toggled;	/* TOGGLE: runs after the reader clicked a box */
	gpointer toggled_data;
} ElimColumn;

static inline ElimColumn
elim_column_text(guint column)
{
	ElimColumn col = { ELIM_COLUMN_TEXT, { 0 }, -1, -1, 0, NULL, -1, NULL, NULL };

	col.text = elim_text_column(column);
	return col;
}

static inline ElimColumn
elim_column_toggle(guint column, ElimToggleFunc toggled, gpointer data)
{
	ElimColumn col = elim_column_text(column);

	col.kind = ELIM_COLUMN_TOGGLE;
	col.toggled = toggled;
	col.toggled_data = data;
	return col;
}

static inline ElimColumn
elim_column_image(guint column, gint pixel_size, const char *tooltip)
{
	ElimColumn col = elim_column_text(column);

	col.kind = ELIM_COLUMN_IMAGE;
	col.pixel_size = pixel_size;
	col.tooltip = tooltip;
	return col;
}

static inline ElimColumn
elim_column_progress(guint value_column, gint text_column)
{
	ElimColumn col = elim_column_text(value_column);

	col.kind = ELIM_COLUMN_PROGRESS;
	col.progress_text_column = text_column;
	return col;
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

/* As elim_table_setup_rows(), with cells of any kind. */
void elim_table_setup_row_columns(GtkWidget *list_view, GListStore *store,
				  const ElimColumn *cols, guint n_cols);

/* A column of VIEW (a GtkColumnView) made of COL's kind of cell. */
GtkColumnViewColumn *elim_table_add_cell_column(GtkWidget *view, const char *title,
						const ElimColumn *col);

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

/* Adds a column of check boxes showing COLUMN of every row. */
GtkColumnViewColumn *elim_table_add_toggle_column(GtkWidget *view,
						  const char *title,
						  guint column,
						  ElimToggleFunc toggled,
						  gpointer data);

/* As elim_table_add_toggle_column(), and the box is shown only for a row whose
 * VISIBLE_COLUMN (an int) is not 0; -1 shows it in every row. */
GtkColumnViewColumn *elim_table_add_toggle_column_visible(
	GtkWidget *view, const char *title, guint column, gint visible_column,
	ElimToggleFunc toggled, gpointer data);

/* Adds a column of images: row COLUMN holds a GdkPaintable (elim_row_set_object()
 * or elim_row_set_pixbuf()) or the name of a themed icon (a string), and an
 * empty one shows nothing. PIXEL_SIZE 0 keeps the icon's own size. TOOLTIP,
 * when not NULL, is the tooltip of every image of the column. */
GtkColumnViewColumn *elim_table_add_image_column(GtkWidget *view,
						 const char *title, guint column,
						 gint pixel_size,
						 const char *tooltip);

/* The tooltip of the row under the pointer: FUNC fills TOOLTIP and returns TRUE
 * to show it. A cell's own tooltip (ElimTextColumn.tooltip_column) wins. */
typedef gboolean (*ElimTooltipFunc)(ElimRow *row, GtkTooltip *tooltip,
				    gpointer data);
void elim_table_set_tooltip_func(GtkWidget *view, ElimTooltipFunc func,
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

/* The row the view shows at POSITION (the position "activate" passes, or a
 * selection reports), NULL when there is none. Unlike elim_table_get() it
 * counts the rows of a tree that are showing and follows a sorted view. */
ElimRow *elim_table_row_at(GtkWidget *view, guint position);

/* Where ROW is in STORE, -1 when it is not. */
gint elim_table_find(GListStore *store, ElimRow *row);

/* The store a view made by elim_table_view_new() shows (the root rows of a
 * tree). */
GListStore *elim_table_get_store(GtkWidget *view);

/* The reader activated a row (double click, Enter): connect to "activate"
 * of the view, which passes the row's position. */

/* ---- trees ----
 *
 * A tree is the same store of ElimRow, but a row may hold children of its own,
 * and the view is a GtkTreeListModel over it (elim_tree_setup()): the rows
 * that are showing are the ones whose parents are open. It stands in for
 * GtkTreeStore and its GtkTreeIter: hold the ElimRow, not an iterator, and walk
 * a row's children with elim_row_get_child().
 *
 * What differs from the tree view:
 *  - the column that carries the expander is one whose ElimTextColumn has
 *    EXPANDER set (the first, usually);
 *  - a row with no children shows no expander. A row whose children are
 *    costly is marked with elim_row_set_lazy() and gets them from the view's
 *    populate function the first time it is opened;
 *  - children added to a row that has been drawn as a leaf do not make its
 *    expander appear: fill a row before it is shown, or refill the tree;
 *  - there is no type-ahead search. */

/* The row's place in the tree. The parent of a root row is NULL. */
ElimRow *elim_row_get_parent(ElimRow *row);
guint elim_row_n_children(ElimRow *row);
ElimRow *elim_row_get_child(ElimRow *row, guint position);
/* 0 for a root row. */
guint elim_row_get_depth(ElimRow *row);
/* The store that holds ROW: its parent's children, or ROOTS. */
GListStore *elim_row_get_siblings(ElimRow *row, GListStore *roots);

/* Marks ROW as one whose children come on demand: it shows an expander at
 * once and the populate function runs when it is opened. */
void elim_row_set_lazy(ElimRow *row, gboolean lazy);
gboolean elim_row_is_lazy(ElimRow *row);

/* New row in the tree whose roots are ROOTS, under PARENT (NULL for a root),
 * the way gtk_tree_store_append() and its kin made one. The tree owns the
 * row: the caller borrows it. POSITION -1 appends. */
ElimRow *elim_tree_insert(GListStore *roots, ElimRow *parent, gint position,
			  guint n_columns);
ElimRow *elim_tree_append(GListStore *roots, ElimRow *parent, guint n_columns);
ElimRow *elim_tree_prepend(GListStore *roots, ElimRow *parent, guint n_columns);
ElimRow *elim_tree_insert_after(GListStore *roots, ElimRow *parent,
				ElimRow *sibling, guint n_columns);
/* Removes ROW and what is under it. FALSE when it was not there. */
gboolean elim_tree_remove(GListStore *roots, ElimRow *row);

/* Makes VIEW (a GtkColumnView or a GtkListView) show the tree of ROOTS, with
 * no row selected. */
void elim_tree_setup(GtkWidget *view, GListStore *roots);
/* As elim_table_setup_list(): a GtkListView with one column, SPEC's, which
 * carries the expanders. */
void elim_tree_setup_list(GtkWidget *list_view, GListStore *roots,
			  const ElimTextColumn *spec);
/* As elim_table_setup_row_columns() for a tree: every row is COLS side by
 * side, behind the expander, and the reader sees no header. */
void elim_tree_setup_row_columns(GtkWidget *list_view, GListStore *roots,
				 const ElimColumn *cols, guint n_cols);
GtkWidget *elim_tree_view_new(GListStore *roots);

/* Runs the first time a lazy ROW is opened (or asked to open with
 * elim_tree_expand_row()): it adds ROW's children with elim_tree_append(). */
typedef void (*ElimTreePopulateFunc)(GtkWidget *view, ElimRow *row, gpointer data);
void elim_tree_set_populate(GtkWidget *view, ElimTreePopulateFunc func,
			    gpointer data);

/* Runs after the reader opens or closes a row of the tree. */
typedef void (*ElimTreeExpandedFunc)(GtkWidget *view, ElimRow *row,
				     gboolean expanded, gpointer data);
void elim_tree_set_expanded_func(GtkWidget *view, ElimTreeExpandedFunc func,
				 gpointer data);

/* Opens ROW, and the rows above it as needed; with DESCENDANTS what is under
 * it too. */
void elim_tree_expand_row(GtkWidget *view, ElimRow *row, gboolean descendants);
void elim_tree_collapse_row(GtkWidget *view, ElimRow *row);
/* Opens the rows above ROW so that it shows. */
void elim_tree_expand_to_row(GtkWidget *view, ElimRow *row);
gboolean elim_tree_row_expanded(GtkWidget *view, ElimRow *row);
void elim_tree_expand_all(GtkWidget *view);
void elim_tree_collapse_all(GtkWidget *view);

/* Replaces the whole tree of ROOTS with the rows that STAGING holds (a tree
 * made apart, where adding a row shows nothing), in one change. STAGING is
 * left empty. */
void elim_tree_replace(GListStore *roots, GListStore *staging);

/* Where a dragged row lands relative to the row it is dropped on. */
typedef enum {
	ELIM_DROP_BEFORE,
	ELIM_DROP_INTO,		/* as the last child of the row */
	ELIM_DROP_AFTER
} ElimTreeDropPosition;

/* Whether ROW, dragged, may be dropped at POSITION of TARGET. The tree itself
 * already refuses a drop on the row itself and inside what it holds. */
typedef gboolean (*ElimTreeCanDropFunc)(GtkWidget *view, ElimRow *row,
					ElimRow *target,
					ElimTreeDropPosition position,
					gpointer data);
/* Runs after ROW was moved by a drop. */
typedef void (*ElimTreeMovedFunc)(GtkWidget *view, ElimRow *row, gpointer data);

/* Sets how a row can be dragged: CAN_DROP may be NULL (any place the tree
 * allows), MOVED too. Nothing can be dragged until elim_tree_set_reorderable(). */
void elim_tree_set_drag_funcs(GtkWidget *view, ElimTreeCanDropFunc can_drop,
			      ElimTreeMovedFunc moved, gpointer data);
/* Lets the reader drag rows: before, after or into (as the last child of)
 * another row, the way the tree view's reorderable mode did. */
void elim_tree_set_reorderable(GtkWidget *view, gboolean reorderable);
gboolean elim_tree_get_reorderable(GtkWidget *view);

/* Moves ROW (and what is under it) to POSITION of TARGET, keeping the rows it
 * had open open, and picks it. FALSE when that is not a place it can go: TARGET
 * is ROW or under it. This is what a drop does. */
gboolean elim_tree_move_row(GtkWidget *view, ElimRow *row, ElimRow *target,
			    ElimTreeDropPosition position);

/* Runs after the tree of ROOTS changed (a row added, removed, moved, or a
 * column of a row set), once for as many changes as the program makes before it
 * next returns to the main loop. NULL FUNC stops it. */
typedef void (*ElimTreeChangedFunc)(GListStore *roots, gpointer data);
void elim_tree_set_changed_func(GListStore *roots, ElimTreeChangedFunc func,
				gpointer data);

/* The row of VIEW under the point X, Y (in VIEW's coordinates), NULL when the
 * point is on no row. Owned by the tree. */
ElimRow *elim_table_row_at_point(GtkWidget *view, gdouble x, gdouble y);

/* Whether the point X, Y (in VIEW's coordinates) is on the arrow that opens
 * and closes a row, which does that by itself. */
gboolean elim_tree_point_on_expander(GtkWidget *view, gdouble x, gdouble y);

/* Selects ROW, opening the rows above it, and with SCROLL brings it into
 * view. FALSE when it is not in the tree. */
gboolean elim_tree_select_row(GtkWidget *view, ElimRow *row, gboolean scroll);
void elim_tree_unselect(GtkWidget *view);

#ifdef __cplusplus
}
#endif

#endif /* GUI_TABLE_HELPERS_H */
