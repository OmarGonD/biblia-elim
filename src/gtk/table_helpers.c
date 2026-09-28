/*
 * Biblia Elim
 * table_helpers.c - a GtkColumnView over a GListStore of rows
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

#include <config.h>

#include "gui/table_helpers.h"

#include <string.h>

typedef struct {
	enum { ELIM_CELL_STRING, ELIM_CELL_INT, ELIM_CELL_DOUBLE, ELIM_CELL_OBJECT } type;
	gint number;
	gdouble decimal;
	gchar *text;
	GObject *object;
} ElimCell;

struct _ElimRow
{
	GObject parent;
	guint n_columns;
	ElimCell *cells;
	ElimRow *parent_row;	/* not owned: the row that holds this one as a child */
	GListStore *roots;	/* not owned: the store of the tree's root rows */
	GListStore *children;	/* NULL for a row that is a leaf */
	gboolean lazy;		/* its children are still to be made on expansion */
};

enum { ROW_CHANGED, ROW_LAST_SIGNAL };
static guint row_signals[ROW_LAST_SIGNAL];

static void tree_touch(GListStore *roots);

G_DEFINE_FINAL_TYPE(ElimRow, elim_row, G_TYPE_OBJECT)

static void
elim_row_finalize(GObject *object)
{
	ElimRow *row = ELIM_ROW(object);
	guint i;

	if (row->children) {
		guint n = g_list_model_get_n_items(G_LIST_MODEL(row->children));

		/* a child that outlives this row must not point back at it */
		for (i = 0; i < n; i++) {
			ElimRow *child = ELIM_ROW(g_list_model_get_item(
			    G_LIST_MODEL(row->children), i));

			child->parent_row = NULL;
			g_object_unref(child);
		}
		g_object_unref(row->children);
	}
	for (i = 0; i < row->n_columns; i++) {
		g_free(row->cells[i].text);
		g_clear_object(&row->cells[i].object);
	}
	g_free(row->cells);
	G_OBJECT_CLASS(elim_row_parent_class)->finalize(object);
}

static void
elim_row_class_init(ElimRowClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = elim_row_finalize;
	row_signals[ROW_CHANGED] =
	    g_signal_new("changed", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST, 0,
			 NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_UINT);
}

static void
elim_row_init(ElimRow *row)
{
	(void)row;
}

ElimRow *
elim_row_new(guint n_columns)
{
	ElimRow *row = g_object_new(ELIM_TYPE_ROW, NULL);
	guint i;

	row->n_columns = n_columns;
	row->cells = g_new0(ElimCell, n_columns ? n_columns : 1);
	for (i = 0; i < n_columns; i++)
		row->cells[i].text = g_strdup("");
	return row;
}

guint
elim_row_n_columns(ElimRow *row)
{
	return row->n_columns;
}

void
elim_row_set_string(ElimRow *row, guint column, const char *value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	g_clear_object(&row->cells[column].object);
	row->cells[column].text = g_strdup(value ? value : "");
	row->cells[column].type = ELIM_CELL_STRING;
	row->cells[column].number = 0;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
	tree_touch(row->roots);
}

void
elim_row_set_int(ElimRow *row, guint column, gint value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	g_clear_object(&row->cells[column].object);
	row->cells[column].text = NULL;
	row->cells[column].type = ELIM_CELL_INT;
	row->cells[column].number = value;
	row->cells[column].decimal = 0;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
	tree_touch(row->roots);
}

void
elim_row_set_double(ElimRow *row, guint column, gdouble value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	g_clear_object(&row->cells[column].object);
	row->cells[column].text = NULL;
	row->cells[column].type = ELIM_CELL_DOUBLE;
	row->cells[column].number = 0;
	row->cells[column].decimal = value;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
	tree_touch(row->roots);
}

void
elim_row_set_object(ElimRow *row, guint column, GObject *value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	row->cells[column].text = NULL;
	if (value)
		g_object_ref(value);
	g_clear_object(&row->cells[column].object);
	row->cells[column].object = value;
	row->cells[column].type = ELIM_CELL_OBJECT;
	row->cells[column].number = 0;
	row->cells[column].decimal = 0;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
	tree_touch(row->roots);
}

void
elim_row_set_pixbuf(ElimRow *row, guint column, GdkPixbuf *pixbuf)
{
	GdkTexture *texture = pixbuf ? gdk_texture_new_for_pixbuf(pixbuf) : NULL;

	elim_row_set_object(row, column, G_OBJECT(texture));
	g_clear_object(&texture);
}

GObject *
elim_row_get_object(ElimRow *row, guint column)
{
	g_return_val_if_fail(column < row->n_columns, NULL);
	return row->cells[column].type == ELIM_CELL_OBJECT ? row->cells[column].object : NULL;
}

const char *
elim_row_get_string(ElimRow *row, guint column)
{
	g_return_val_if_fail(column < row->n_columns, "");
	return row->cells[column].type == ELIM_CELL_STRING ? row->cells[column].text : "";
}

gint
elim_row_get_int(ElimRow *row, guint column)
{
	g_return_val_if_fail(column < row->n_columns, 0);
	return row->cells[column].type == ELIM_CELL_INT ? row->cells[column].number : 0;
}

gdouble
elim_row_get_double(ElimRow *row, guint column)
{
	g_return_val_if_fail(column < row->n_columns, 0);
	return row->cells[column].type == ELIM_CELL_DOUBLE ? row->cells[column].decimal : 0;
}

char *
elim_row_dup_text(ElimRow *row, guint column)
{
	g_return_val_if_fail(column < row->n_columns, g_strdup(""));
	if (row->cells[column].type == ELIM_CELL_INT)
		return g_strdup_printf("%d", row->cells[column].number);
	if (row->cells[column].type == ELIM_CELL_DOUBLE)
		return g_strdup_printf("%g", row->cells[column].decimal);
	if (row->cells[column].type == ELIM_CELL_OBJECT)
		return g_strdup("");
	return g_strdup(row->cells[column].text);
}

GListStore *
elim_table_new(void)
{
	return g_list_store_new(ELIM_TYPE_ROW);
}

/* ---- views ---- */

/* The row an item of a view's model stands for: a tree shows its rows through
 * GtkTreeListRow, a list shows them as they are. Owned by its store. */
static ElimRow *
item_row(gpointer item)
{
	if (GTK_IS_TREE_LIST_ROW(item)) {
		GObject *row = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));

		if (row)
			g_object_unref(row);
		return row ? ELIM_ROW(row) : NULL;
	}
	return item ? ELIM_ROW(item) : NULL;
}

static ElimRow *
list_item_row(GtkListItem *item)
{
	return item_row(gtk_list_item_get_item(item));
}

static GtkSelectionModel *
view_model(GtkWidget *view)
{
	if (GTK_IS_COLUMN_VIEW(view))
		return gtk_column_view_get_model(GTK_COLUMN_VIEW(view));
	if (GTK_IS_LIST_VIEW(view))
		return gtk_list_view_get_model(GTK_LIST_VIEW(view));
	return NULL;
}

static void
view_set_model(GtkWidget *view, GtkSelectionModel *model)
{
	if (GTK_IS_COLUMN_VIEW(view))
		gtk_column_view_set_model(GTK_COLUMN_VIEW(view), model);
	else if (GTK_IS_LIST_VIEW(view))
		gtk_list_view_set_model(GTK_LIST_VIEW(view), model);
}

void
elim_table_setup(GtkWidget *view, GListStore *store)
{
	GtkSingleSelection *selection =
	    gtk_single_selection_new(G_LIST_MODEL(g_object_ref(store)));

	/* nothing is selected until the reader picks a row, and a row can be
	 * unselected again */
	gtk_single_selection_set_autoselect(selection, FALSE);
	gtk_single_selection_set_can_unselect(selection, TRUE);
	gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
	view_set_model(view, GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
}

void
elim_table_setup_sortable(GtkWidget *view, GListStore *store)
{
	GtkSortListModel *sorted;
	GtkSingleSelection *selection;

	g_return_if_fail(GTK_IS_COLUMN_VIEW(view));
	/* the sort model takes over a reference to the sorter, the view keeps its own */
	sorted = gtk_sort_list_model_new(G_LIST_MODEL(g_object_ref(store)),
					 GTK_SORTER(g_object_ref(gtk_column_view_get_sorter(
					     GTK_COLUMN_VIEW(view)))));
	selection = gtk_single_selection_new(G_LIST_MODEL(sorted));
	gtk_single_selection_set_autoselect(selection, FALSE);
	gtk_single_selection_set_can_unselect(selection, TRUE);
	gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
	view_set_model(view, GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
}

GtkWidget *
elim_table_view_new(GListStore *store)
{
	GtkWidget *view = gtk_column_view_new(NULL);

	elim_table_setup(view, store);
	return view;
}

void
elim_table_set_browse(GtkWidget *view, gboolean browse)
{
	GtkSingleSelection *selection = elim_table_selection(view);

	gtk_single_selection_set_autoselect(selection, browse);
	gtk_single_selection_set_can_unselect(selection, !browse);
}

/* ---- cells ----
 *
 * What a column of a table, or a column of a row, shows is a cell: a label, a
 * check box, an image or a progress bar. Each kind knows how to make its
 * widget and how to show a row in it; the layouts (a GtkColumnView column, or
 * a row of a GtkListView) build their cells from these, so every kind works in
 * both. */

/* "changed" of a row for something that is not one of its columns: what it
 * holds under it. */
#define ELIM_ROW_CHILDREN G_MAXUINT

typedef struct _Cell Cell;

typedef struct {
	GtkWidget *(*make)(const ElimColumn *col);
	/* shows the row in the cell */
	void (*show)(Cell *cell);
	/* whether row column COLUMN is one the cell shows */
	gboolean (*depends)(const ElimColumn *col, guint column);
	/* the cell is bound to a row / let go of it */
	void (*attach)(Cell *cell);
	void (*detach)(Cell *cell);
} CellKind;

/* A cell shown for a row. The function that runs when a check box is clicked
 * may refill the store, which unbinds the cell in the middle of it: the cell
 * lives until that call has returned, and holds its row. */
struct _Cell {
	const CellKind *kind;
	ElimColumn col;
	GtkWidget *widget;
	ElimRow *row;
	gulong changed;
	gulong toggled;
	gboolean updating;
	gboolean bound;
	guint refs;
};

static const CellKind *cell_kind(ElimColumnKind kind);

static void
cell_unref(Cell *c)
{
	if (--c->refs)
		return;
	g_object_unref(c->row);
	g_free(c);
}

/* A cell that is not wanted for this row keeps its place in the layout, so
 * that the columns stay lined up, but shows nothing and takes no click. */
static void
cell_apply_visible(Cell *c)
{
	gboolean shown = c->col.visible_column < 0 ||
			 elim_row_get_int(c->row, (guint)c->col.visible_column) != 0;

	gtk_widget_set_opacity(c->widget, shown ? 1.0 : 0.0);
	gtk_widget_set_can_target(c->widget, shown);
	gtk_widget_set_can_focus(c->widget, shown && c->col.kind == ELIM_COLUMN_TOGGLE);
}

static void
cell_show(Cell *c)
{
	c->kind->show(c);
	cell_apply_visible(c);
}

static void
cell_row_changed(ElimRow *row, guint column, gpointer data)
{
	Cell *c = data;

	(void)row;
	if (column == ELIM_ROW_CHILDREN)
		return;
	if ((gint)column == c->col.visible_column)
		cell_apply_visible(c);
	if (c->kind->depends(&c->col, column))
		c->kind->show(c);
}

static Cell *
cell_bind(const ElimColumn *col, GtkWidget *widget, ElimRow *row)
{
	Cell *c = g_new0(Cell, 1);

	c->kind = cell_kind(col->kind);
	c->col = *col;
	c->widget = widget;
	c->row = g_object_ref(row);
	c->refs = 1;
	c->bound = TRUE;
	c->changed = g_signal_connect(row, "changed", G_CALLBACK(cell_row_changed), c);
	if (c->kind->attach)
		c->kind->attach(c);
	/* what the tooltip and drag code look for */
	g_object_set_data(G_OBJECT(widget), "elim-row", row);
	cell_show(c);
	return c;
}

static void
cell_release(gpointer data)
{
	Cell *c = data;

	c->bound = FALSE;
	g_signal_handler_disconnect(c->row, c->changed);
	if (c->kind->detach)
		c->kind->detach(c);
	g_object_set_data(G_OBJECT(c->widget), "elim-row", NULL);
	cell_unref(c);
}

/* -- text -- */

static GtkWidget *
text_make(const ElimColumn *col)
{
	const ElimTextColumn *spec = &col->text;
	GtkWidget *label = gtk_label_new(NULL);

	gtk_label_set_xalign(GTK_LABEL(label), spec->xalign);
	if (spec->wrap) {
		/* the label may shrink to a word (or a character) and wraps to the
		 * width the row leaves it; its natural width is capped, so a long
		 * text does not ask for a line of its own length. The row gives
		 * the other columns their natural width first, as their gap
		 * between minimum and natural is the smaller one. */
		gtk_label_set_wrap(GTK_LABEL(label), TRUE);
		gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_max_width_chars(GTK_LABEL(label), 80);
	} else {
		gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	}
	gtk_widget_set_margin_start(label, 6);
	gtk_widget_set_margin_end(label, 6);
	gtk_widget_set_margin_top(label, spec->pad_y);
	gtk_widget_set_margin_bottom(label, spec->pad_y);
	return label;
}

static void
text_show(Cell *c)
{
	const ElimTextColumn *spec = &c->col.text;
	GtkLabel *label = GTK_LABEL(c->widget);
	char *text = elim_row_dup_text(c->row, spec->column);

	if (spec->markup)
		gtk_label_set_markup(label, text);
	else
		gtk_label_set_text(label, text);
	g_free(text);
	if (spec->weight_column >= 0 || spec->foreground_column >= 0) {
		PangoAttrList *attributes = pango_attr_list_new();

		if (spec->weight_column >= 0)
			pango_attr_list_insert(attributes,
				pango_attr_weight_new((PangoWeight)elim_row_get_int(
					c->row, (guint)spec->weight_column)));
		if (spec->foreground_column >= 0) {
			GdkRGBA color;
			const char *foreground = elim_row_get_string(
				c->row, (guint)spec->foreground_column);

			if (*foreground && gdk_rgba_parse(&color, foreground))
				pango_attr_list_insert(attributes, pango_attr_foreground_new(
					(guint16)(color.red * 65535), (guint16)(color.green * 65535),
					(guint16)(color.blue * 65535)));
		}
		gtk_label_set_attributes(label, attributes);
		pango_attr_list_unref(attributes);
	} else {
		gtk_label_set_attributes(label, NULL);
	}
	if (spec->tooltip_column >= 0) {
		text = elim_row_dup_text(c->row, (guint)spec->tooltip_column);
		gtk_widget_set_tooltip_text(c->widget, text);
		g_free(text);
	}
}

static gboolean
text_depends(const ElimColumn *col, guint column)
{
	return column == col->text.column || (gint)column == col->text.tooltip_column ||
	       (gint)column == col->text.weight_column ||
	       (gint)column == col->text.foreground_column;
}

/* -- check boxes -- */

static GtkWidget *
toggle_make(const ElimColumn *col)
{
	GtkWidget *check = gtk_check_button_new();

	gtk_widget_set_halign(check, GTK_ALIGN_CENTER);
	if (col->tooltip)
		gtk_widget_set_tooltip_text(check, col->tooltip);
	return check;
}

static void
toggle_show(Cell *c)
{
	GtkCheckButton *check = GTK_CHECK_BUTTON(c->widget);

	c->updating = TRUE;
	gtk_check_button_set_active(check,
				    elim_row_get_int(c->row, c->col.text.column) != 0);
	gtk_check_button_set_inconsistent(
	    check, c->col.inconsistent_column >= 0 &&
		       elim_row_get_int(c->row, (guint)c->col.inconsistent_column) != 0);
	c->updating = FALSE;
}

static gboolean
toggle_depends(const ElimColumn *col, guint column)
{
	return column == col->text.column || (gint)column == col->inconsistent_column;
}

static void
toggle_clicked(GtkCheckButton *check, gpointer data)
{
	Cell *c = data;

	(void)check;
	if (c->updating)
		return;
	c->refs++;
	if (c->col.toggled)
		c->col.toggled(c->row, c->col.toggled_data);
	/* what the row holds now is what the box shows, unless the cell went
	 * with the call */
	if (c->bound)
		toggle_show(c);
	cell_unref(c);
}

static void
toggle_attach(Cell *c)
{
	c->toggled = g_signal_connect(c->widget, "toggled", G_CALLBACK(toggle_clicked), c);
}

static void
toggle_detach(Cell *c)
{
	g_signal_handler_disconnect(c->widget, c->toggled);
}

/* -- images -- */

static GtkWidget *
image_make(const ElimColumn *col)
{
	GtkWidget *image = gtk_image_new();

	if (col->pixel_size > 0)
		gtk_image_set_pixel_size(GTK_IMAGE(image), col->pixel_size);
	gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
	if (col->tooltip)
		gtk_widget_set_tooltip_text(image, col->tooltip);
	return image;
}

static void
image_show(Cell *c)
{
	GtkImage *image = GTK_IMAGE(c->widget);
	GObject *object = elim_row_get_object(c->row, c->col.text.column);
	const char *name = elim_row_get_string(c->row, c->col.text.column);

	if (object && GDK_IS_PAINTABLE(object))
		gtk_image_set_from_paintable(image, GDK_PAINTABLE(object));
	else if (*name)
		gtk_image_set_from_icon_name(image, name);
	else
		gtk_image_clear(image);
}

static gboolean
image_depends(const ElimColumn *col, guint column)
{
	return column == col->text.column;
}

/* -- progress -- */

static GtkWidget *
progress_make(const ElimColumn *col)
{
	GtkWidget *bar = gtk_progress_bar_new();

	gtk_widget_set_valign(bar, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(bar, 6);
	gtk_widget_set_margin_end(bar, 6);
	gtk_widget_set_margin_top(bar, col->text.pad_y);
	gtk_widget_set_margin_bottom(bar, col->text.pad_y);
	gtk_widget_set_hexpand(bar, TRUE);
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(bar), col->progress_text_column >= 0);
	return bar;
}

static void
progress_show(Cell *c)
{
	GtkProgressBar *bar = GTK_PROGRESS_BAR(c->widget);
	gint value = elim_row_get_int(c->row, c->col.text.column);

	gtk_progress_bar_set_fraction(bar, CLAMP(value, 0, 100) / 100.0);
	if (c->col.progress_text_column >= 0) {
		char *text = elim_row_dup_text(c->row, (guint)c->col.progress_text_column);

		gtk_progress_bar_set_text(bar, text);
		g_free(text);
	}
}

static gboolean
progress_depends(const ElimColumn *col, guint column)
{
	return column == col->text.column || (gint)column == col->progress_text_column;
}

static const CellKind cell_kinds[] = {
	[ELIM_COLUMN_TEXT] = { text_make, text_show, text_depends, NULL, NULL },
	[ELIM_COLUMN_TOGGLE] = { toggle_make, toggle_show, toggle_depends,
				 toggle_attach, toggle_detach },
	[ELIM_COLUMN_IMAGE] = { image_make, image_show, image_depends, NULL, NULL },
	[ELIM_COLUMN_PROGRESS] = { progress_make, progress_show, progress_depends,
				   NULL, NULL },
};

static const CellKind *
cell_kind(ElimColumnKind kind)
{
	return &cell_kinds[kind];
}

/* The columns a factory keeps for as long as it can make cells: the caller's
 * strings are copied. */
typedef struct {
	guint n;
	ElimColumn *cols;
	char **tooltips;
} ColumnSpecs;

static ColumnSpecs *
column_specs_new(const ElimColumn *cols, guint n)
{
	ColumnSpecs *specs = g_new0(ColumnSpecs, 1);
	guint i;

	specs->n = n;
	specs->cols = g_memdup2(cols, n * sizeof *cols);
	specs->tooltips = g_new0(char *, n);
	for (i = 0; i < n; i++) {
		specs->tooltips[i] = g_strdup(cols[i].tooltip);
		specs->cols[i].tooltip = specs->tooltips[i];
	}
	return specs;
}

static void
column_specs_free(gpointer data)
{
	ColumnSpecs *specs = data;
	guint i;

	for (i = 0; i < specs->n; i++)
		g_free(specs->tooltips[i]);
	g_free(specs->tooltips);
	g_free(specs->cols);
	g_free(specs);
}

/* The widget of a cell in a list item's child: a tree column wraps it in an
 * expander. */
static GtkWidget *
cell_widget_of(GtkWidget *child)
{
	return GTK_IS_TREE_EXPANDER(child)
		   ? gtk_tree_expander_get_child(GTK_TREE_EXPANDER(child))
		   : child;
}

typedef struct _TreeBind TreeBind;
static TreeBind *tree_bind_new(GtkListItem *item, GtkWidget *expander, ElimRow *row);
static void tree_bind_free(gpointer data);
static void tree_dnd_attach(GtkWidget *cell);

/* -- one column of a table -- */

static void
column_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	const ColumnSpecs *specs = data;
	GtkWidget *widget = cell_kind(specs->cols[0].kind)->make(&specs->cols[0]);

	(void)factory;
	if (specs->cols[0].text.expander) {
		/* the column of a tree that carries the expanders */
		GtkWidget *expander = gtk_tree_expander_new();

		gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), widget);
		gtk_widget_set_hexpand(widget, TRUE);
		gtk_list_item_set_child(item, expander);
		tree_dnd_attach(expander);
		return;
	}
	gtk_list_item_set_child(item, widget);
}

static void
column_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	const ColumnSpecs *specs = data;
	GtkWidget *child = gtk_list_item_get_child(item);
	ElimRow *row = list_item_row(item);

	(void)factory;
	g_object_set_data_full(G_OBJECT(item), "elim-cell",
			       cell_bind(&specs->cols[0], cell_widget_of(child), row),
			       cell_release);
	g_object_set_data(G_OBJECT(child), "elim-row", row);
	if (GTK_IS_TREE_EXPANDER(child))
		g_object_set_data_full(G_OBJECT(item), "elim-tree-bind",
				       tree_bind_new(item, child, row), tree_bind_free);
}

static void
column_unbind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	GtkWidget *child = gtk_list_item_get_child(item);

	(void)factory;
	(void)data;
	/* drops the tree binding, then the cell, which disconnect from the row */
	g_object_set_data(G_OBJECT(item), "elim-tree-bind", NULL);
	if (GTK_IS_TREE_EXPANDER(child))
		gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(child), NULL);
	g_object_set_data(G_OBJECT(child), "elim-row", NULL);
	g_object_set_data(G_OBJECT(item), "elim-cell", NULL);
}

static GtkListItemFactory *
column_factory(const ElimColumn *col)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	ColumnSpecs *specs = column_specs_new(col, 1);

	g_signal_connect(factory, "setup", G_CALLBACK(column_setup), specs);
	g_signal_connect(factory, "bind", G_CALLBACK(column_bind), specs);
	g_signal_connect(factory, "unbind", G_CALLBACK(column_unbind), specs);
	/* the factory keeps the columns for as long as it can bind cells */
	g_object_set_data_full(G_OBJECT(factory), "elim-spec", specs, column_specs_free);
	return factory;
}

static GtkColumnViewColumn *
add_column(GtkWidget *view, const char *title, const ElimColumn *col)
{
	/* the column takes over the factory */
	GtkColumnViewColumn *column =
	    gtk_column_view_column_new(title, column_factory(col));

	gtk_column_view_column_set_expand(column, col->text.expand);
	gtk_column_view_column_set_resizable(column, col->kind == ELIM_COLUMN_TEXT);
	if (col->text.fixed_width > 0)
		gtk_column_view_column_set_fixed_width(column, col->text.fixed_width);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), column);
	g_object_unref(column);
	return column;
}

/* -- a GtkListView of one column -- */

void
elim_table_setup_list(GtkWidget *list_view, GListStore *store,
		      const ElimTextColumn *spec)
{
	ElimColumn col = elim_column_text(spec->column);
	GtkListItemFactory *factory;

	col.text = *spec;
	factory = column_factory(&col);
	elim_table_setup(list_view, store);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);
	g_object_unref(factory);
}

/* -- rows of several columns, no header -- */

typedef struct {
	ColumnSpecs *columns;
	GtkSizeGroup **groups;	/* one per column: the same width in every row */
	gboolean expander;	/* the whole row sits behind a tree expander */
	GtkWidget *view;
	ElimTableReorderedFunc reordered;
	gpointer reordered_data;
} RowsSpec;

static GdkContentProvider *row_drag_prepare(GtkDragSource *source, gdouble x,
					     gdouble y, gpointer data);
static gboolean row_drop(GtkDropTarget *target, const GValue *value, gdouble x,
			 gdouble y, gpointer data);

typedef struct {
	guint n;
	Cell **cells;
} RowsBinding;

static void
rows_spec_free(gpointer data)
{
	RowsSpec *r = data;
	guint i;

	for (i = 0; i < r->columns->n; i++)
		g_object_unref(r->groups[i]);
	g_free(r->groups);
	column_specs_free(r->columns);
	g_free(r);
}

static void
rows_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	RowsSpec *r = data;
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	guint i;

	(void)factory;
	for (i = 0; i < r->columns->n; i++) {
		const ElimColumn *col = &r->columns->cols[i];
		GtkWidget *widget = cell_kind(col->kind)->make(col);

		gtk_widget_set_hexpand(widget, col->text.expand);
		if (col->text.fixed_width > 0)
			gtk_widget_set_size_request(widget, col->text.fixed_width, -1);
		gtk_size_group_add_widget(r->groups[i], widget);
		gtk_box_append(GTK_BOX(box), widget);
	}
	if (r->expander) {
		GtkWidget *expander = gtk_tree_expander_new();

		gtk_tree_expander_set_child(GTK_TREE_EXPANDER(expander), box);
		gtk_widget_set_hexpand(box, TRUE);
		gtk_list_item_set_child(item, expander);
		tree_dnd_attach(expander);
	} else {
		gtk_list_item_set_child(item, box);
	}
	if (r->reordered) {
		GtkDragSource *source = gtk_drag_source_new();
		GtkDropTarget *target = gtk_drop_target_new(ELIM_TYPE_ROW,
							    GDK_ACTION_MOVE);

		g_signal_connect(source, "prepare", G_CALLBACK(row_drag_prepare), r);
		g_signal_connect(target, "drop", G_CALLBACK(row_drop), r);
		gtk_drag_source_set_actions(source, GDK_ACTION_MOVE);
		gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(source));
		gtk_widget_add_controller(box, GTK_EVENT_CONTROLLER(target));
		gtk_widget_set_cursor_from_name(box, "grab");
	}
}

static GdkContentProvider *
row_drag_prepare(GtkDragSource *source, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *child = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(source));
	ElimRow *row = g_object_get_data(G_OBJECT(child), "elim-row");

	(void)x;
	(void)y;
	(void)data;
	return row ? gdk_content_provider_new_typed(ELIM_TYPE_ROW, row) : NULL;
}

static gboolean
row_drop(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y,
	 gpointer data)
{
	GtkWidget *child = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
	RowsSpec *spec = data;
	ElimRow *target_row = g_object_get_data(G_OBJECT(child), "elim-row");
	ElimRow *source_row;

	(void)x;
	if (!G_VALUE_HOLDS(value, ELIM_TYPE_ROW) || !target_row)
		return FALSE;
	source_row = ELIM_ROW(g_value_get_object(value));
	if (!elim_table_move_row(spec->view, source_row, target_row,
				y > gtk_widget_get_height(child) / 2.0))
		return FALSE;
	if (spec->reordered)
		spec->reordered(spec->view, spec->reordered_data);
	return TRUE;
}

static void
rows_binding_free(gpointer data)
{
	RowsBinding *b = data;
	guint i;

	for (i = 0; i < b->n; i++)
		cell_release(b->cells[i]);
	g_free(b->cells);
	g_free(b);
}

static void
rows_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	const RowsSpec *r = data;
	RowsBinding *b = g_new0(RowsBinding, 1);
	ElimRow *row = list_item_row(item);
	GtkWidget *child = gtk_list_item_get_child(item);
	GtkWidget *box = cell_widget_of(child);
	GtkWidget *widget = gtk_widget_get_first_child(box);
	guint i;

	(void)factory;
	b->n = r->columns->n;
	b->cells = g_new0(Cell *, b->n);
	for (i = 0; i < b->n; i++, widget = gtk_widget_get_next_sibling(widget))
		b->cells[i] = cell_bind(&r->columns->cols[i], widget, row);
	g_object_set_data(G_OBJECT(box), "elim-row", row);
	g_object_set_data(G_OBJECT(child), "elim-row", row);
	g_object_set_data_full(G_OBJECT(item), "elim-rows", b, rows_binding_free);
	if (GTK_IS_TREE_EXPANDER(child))
		g_object_set_data_full(G_OBJECT(item), "elim-tree-bind",
				       tree_bind_new(item, child, row), tree_bind_free);
}

static void
rows_unbind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	GtkWidget *child = gtk_list_item_get_child(item);

	(void)factory;
	(void)data;
	g_object_set_data(G_OBJECT(item), "elim-tree-bind", NULL);
	if (GTK_IS_TREE_EXPANDER(child))
		gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(child), NULL);
	g_object_set_data(G_OBJECT(child), "elim-row", NULL);
	g_object_set_data(G_OBJECT(cell_widget_of(child)), "elim-row", NULL);
	/* drops the bindings, which disconnect from the row */
	g_object_set_data(G_OBJECT(item), "elim-rows", NULL);
}

static void
setup_rows(GtkWidget *list_view, GListStore *store, GListStore *roots,
	   const ElimColumn *cols, guint n_cols, ElimTableReorderedFunc reordered,
	   gpointer reordered_data)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	RowsSpec *r = g_new0(RowsSpec, 1);
	guint i;

	r->columns = column_specs_new(cols, n_cols);
	r->view = list_view;
	r->reordered = reordered;
	r->reordered_data = reordered_data;
	r->expander = roots != NULL;
	r->groups = g_new0(GtkSizeGroup *, n_cols);
	for (i = 0; i < n_cols; i++)
		r->groups[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	g_signal_connect(factory, "setup", G_CALLBACK(rows_setup), r);
	g_signal_connect(factory, "bind", G_CALLBACK(rows_bind), r);
	g_signal_connect(factory, "unbind", G_CALLBACK(rows_unbind), NULL);
	/* the factory keeps the spec for as long as it can make rows */
	g_object_set_data_full(G_OBJECT(factory), "elim-spec", r, rows_spec_free);
	if (roots)
		elim_tree_setup(list_view, roots);
	else
		elim_table_setup(list_view, store);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);
	g_object_unref(factory);
}

static ElimColumn *
text_columns(const ElimTextColumn *specs, guint n)
{
	ElimColumn *cols = g_new0(ElimColumn, n);
	guint i;

	for (i = 0; i < n; i++) {
		cols[i] = elim_column_text(specs[i].column);
		cols[i].text = specs[i];
	}
	return cols;
}

void
elim_table_setup_row_columns(GtkWidget *list_view, GListStore *store,
			     const ElimColumn *cols, guint n_cols)
{
	setup_rows(list_view, store, NULL, cols, n_cols, NULL, NULL);
}

void
elim_table_setup_rows(GtkWidget *list_view, GListStore *store,
		      const ElimTextColumn *specs, guint n_specs)
{
	ElimColumn *cols = text_columns(specs, n_specs);

	setup_rows(list_view, store, NULL, cols, n_specs, NULL, NULL);
	g_free(cols);
}

void
elim_table_setup_reorderable_rows(GtkWidget *list_view, GListStore *store,
				  const ElimTextColumn *specs, guint n_specs,
				  ElimTableReorderedFunc reordered, gpointer data)
{
	ElimColumn *cols = text_columns(specs, n_specs);

	setup_rows(list_view, store, NULL, cols, n_specs, reordered, data);
	g_free(cols);
}

void
elim_tree_setup_row_columns(GtkWidget *list_view, GListStore *roots,
			    const ElimColumn *cols, guint n_cols)
{
	setup_rows(list_view, NULL, roots, cols, n_cols, NULL, NULL);
}

void
elim_table_replace(GListStore *store, ElimRow **rows, guint n_rows)
{
	g_list_store_splice(store, 0, g_list_model_get_n_items(G_LIST_MODEL(store)),
			    (gpointer *)rows, n_rows);
}

/* ---- columns of a table ---- */

GtkColumnViewColumn *
elim_table_add_cell_column(GtkWidget *view, const char *title, const ElimColumn *col)
{
	return add_column(view, title, col);
}

GtkColumnViewColumn *
elim_table_add_column(GtkWidget *view, const char *title,
		      const ElimTextColumn *spec)
{
	ElimColumn col = elim_column_text(spec->column);

	col.text = *spec;
	return add_column(view, title, &col);
}

GtkColumnViewColumn *
elim_table_add_text_column(GtkWidget *view, const char *title, guint column,
			   gboolean expand)
{
	ElimTextColumn spec = elim_text_column(column);

	spec.expand = expand;
	return elim_table_add_column(view, title, &spec);
}

GtkColumnViewColumn *
elim_table_add_toggle_column(GtkWidget *view, const char *title, guint column,
			     ElimToggleFunc toggled, gpointer data)
{
	ElimColumn col = elim_column_toggle(column, toggled, data);

	return add_column(view, title, &col);
}

GtkColumnViewColumn *
elim_table_add_toggle_column_visible(GtkWidget *view, const char *title,
				     guint column, gint visible_column,
				     ElimToggleFunc toggled, gpointer data)
{
	ElimColumn col = elim_column_toggle(column, toggled, data);

	col.visible_column = visible_column;
	return add_column(view, title, &col);
}

GtkColumnViewColumn *
elim_table_add_image_column(GtkWidget *view, const char *title, guint column,
			    gint pixel_size, const char *tooltip)
{
	ElimColumn col = elim_column_image(column, pixel_size, tooltip);

	return add_column(view, title, &col);
}

typedef struct {
	guint column;
	ElimTableSortType type;
} SortSpec;

static GtkOrdering
row_sort(gconstpointer first, gconstpointer second, gpointer data)
{
	const SortSpec *spec = data;
	ElimRow *a = ELIM_ROW((gpointer)first);
	ElimRow *b = ELIM_ROW((gpointer)second);
	gint compare;

	switch (spec->type) {
	case ELIM_TABLE_SORT_INT:
		compare = (elim_row_get_int(a, spec->column) > elim_row_get_int(b, spec->column)) -
			(elim_row_get_int(a, spec->column) < elim_row_get_int(b, spec->column));
		break;
	case ELIM_TABLE_SORT_DOUBLE:
		compare = (elim_row_get_double(a, spec->column) > elim_row_get_double(b, spec->column)) -
			(elim_row_get_double(a, spec->column) < elim_row_get_double(b, spec->column));
		break;
	default:
		compare = g_utf8_collate(elim_row_get_string(a, spec->column),
					 elim_row_get_string(b, spec->column));
	}
	return compare < 0 ? GTK_ORDERING_SMALLER :
		compare > 0 ? GTK_ORDERING_LARGER : GTK_ORDERING_EQUAL;
}

GtkColumnViewColumn *
elim_table_add_sortable_column(GtkWidget *view, const char *title,
			       const ElimTextColumn *spec, guint sort_column,
			       ElimTableSortType sort_type)
{
	GtkColumnViewColumn *column = elim_table_add_column(view, title, spec);
	SortSpec *sort_spec = g_new(SortSpec, 1);
	GtkSorter *sorter;

	sort_spec->column = sort_column;
	sort_spec->type = sort_type;
	sorter = GTK_SORTER(gtk_custom_sorter_new(row_sort, sort_spec, g_free));
	gtk_column_view_column_set_sorter(column, sorter);
	g_object_unref(sorter);
	return column;
}

ElimRow *
elim_table_get(GListStore *store, guint position)
{
	ElimRow *row = ELIM_ROW(g_list_model_get_item(G_LIST_MODEL(store), position));

	/* the store holds the row; the reference taken here is not needed */
	if (row)
		g_object_unref(row);
	return row;
}

static gint
find_row(GListStore *store, ElimRow *row)
{
	guint i, n = g_list_model_get_n_items(G_LIST_MODEL(store));

	for (i = 0; i < n; i++)
		if (elim_table_get(store, i) == row)
			return (gint)i;
	return -1;
}

gboolean
elim_table_remove_row(GtkWidget *view, ElimRow *row)
{
	GListStore *store = elim_table_get_store(view);
	gint position = find_row(store, row);

	if (position < 0)
		return FALSE;
	g_list_store_remove(store, (guint)position);
	return TRUE;
}

gboolean
elim_table_move_row(GtkWidget *view, ElimRow *row, ElimRow *target,
			    gboolean after)
{
	GListStore *store = elim_table_get_store(view);
	gint from = find_row(store, row);
	gint to = find_row(store, target);

	if (from < 0 || to < 0 || from == to)
		return FALSE;
	if (after)
		to++;
	if (from < to)
		to--;
	g_object_ref(row);
	g_list_store_remove(store, (guint)from);
	g_list_store_insert(store, (guint)to, row);
	g_object_unref(row);
	return TRUE;
}

/* ---- selection ---- */

GtkSingleSelection *
elim_table_selection(GtkWidget *view)
{
	return GTK_SINGLE_SELECTION(view_model(view));
}

ElimRow *
elim_table_get_selected(GtkWidget *view)
{
	GtkSelectionModel *model = view_model(view);

	return GTK_IS_SINGLE_SELECTION(model)
		   ? item_row(gtk_single_selection_get_selected_item(
			 GTK_SINGLE_SELECTION(model)))
		   : NULL;
}

guint
elim_table_get_selected_position(GtkWidget *view)
{
	return gtk_single_selection_get_selected(elim_table_selection(view));
}

void
elim_table_select(GtkWidget *view, guint position, gboolean scroll)
{
	gtk_single_selection_set_selected(elim_table_selection(view), position);
	if (!scroll)
		return;
	if (GTK_IS_COLUMN_VIEW(view))
		gtk_column_view_scroll_to(GTK_COLUMN_VIEW(view), position, NULL,
					  GTK_LIST_SCROLL_FOCUS, NULL);
	else if (GTK_IS_LIST_VIEW(view))
		gtk_list_view_scroll_to(GTK_LIST_VIEW(view), position,
					GTK_LIST_SCROLL_FOCUS, NULL);
}

GListStore *
elim_table_get_store(GtkWidget *view)
{
	GListModel *model = gtk_single_selection_get_model(elim_table_selection(view));

	if (GTK_IS_SORT_LIST_MODEL(model))
		model = gtk_sort_list_model_get_model(GTK_SORT_LIST_MODEL(model));
	if (GTK_IS_TREE_LIST_MODEL(model))
		model = gtk_tree_list_model_get_model(GTK_TREE_LIST_MODEL(model));
	return G_LIST_STORE(model);
}

ElimRow *
elim_table_row_at(GtkWidget *view, guint position)
{
	GListModel *model = gtk_single_selection_get_model(elim_table_selection(view));
	gpointer item = g_list_model_get_item(model, position);
	ElimRow *row = item_row(item);

	/* the store holds the row; the reference on the item is not needed */
	if (item)
		g_object_unref(item);
	return row;
}

gint
elim_table_find(GListStore *store, ElimRow *row)
{
	return find_row(store, row);
}

/* ---- tooltips ---- */

typedef struct {
	ElimTooltipFunc func;
	gpointer data;
} TooltipSpec;

static gboolean
view_query_tooltip(GtkWidget *view, gint x, gint y, gboolean keyboard,
		   GtkTooltip *tooltip, gpointer data)
{
	const TooltipSpec *spec = data;
	GtkWidget *widget;
	ElimRow *row = NULL;

	(void)keyboard;
	widget = gtk_widget_pick(view, x, y, GTK_PICK_DEFAULT);
	for (; widget && widget != view; widget = gtk_widget_get_parent(widget)) {
		row = g_object_get_data(G_OBJECT(widget), "elim-row");
		if (row)
			break;
	}
	return row ? spec->func(row, tooltip, spec->data) : FALSE;
}

void
elim_table_set_tooltip_func(GtkWidget *view, ElimTooltipFunc func, gpointer data)
{
	TooltipSpec *spec = g_new0(TooltipSpec, 1);

	spec->func = func;
	spec->data = data;
	gtk_widget_set_has_tooltip(view, TRUE);
	g_signal_connect_data(view, "query-tooltip", G_CALLBACK(view_query_tooltip),
			      spec, (GClosureNotify)g_free, 0);
}

/* ---- trees ---- */

typedef struct {
	GtkWidget *view;	/* not owned: the tree lives with its view */
	GtkTreeListModel *model;
	ElimTreePopulateFunc populate;
	gpointer populate_data;
	ElimTreeExpandedFunc expanded;
	gpointer expanded_data;
	gboolean reorderable;
	ElimTreeCanDropFunc can_drop;
	ElimTreeMovedFunc moved;
	gpointer drag_data;
} ElimTree;

static ElimTree *
view_tree(GtkWidget *view)
{
	return view ? g_object_get_data(G_OBJECT(view), "elim-tree") : NULL;
}

static GListModel *
tree_create_children(gpointer item, gpointer data)
{
	ElimRow *row = ELIM_ROW(item);

	(void)data;
	/* Every row made by elim_tree_insert() has a store of children, empty
	 * as it is made: a view that shows a row before its children are added
	 * asks at once, and would be told for good that it has none. A row
	 * with nothing under it hides its expander instead. */
	return row->children ? g_object_ref(G_LIST_MODEL(row->children)) : NULL;
}

/* Whether ROW has (or will get) children to show. */
static gboolean
row_has_children(ElimRow *row)
{
	return row->lazy ||
	       (row->children && g_list_model_get_n_items(G_LIST_MODEL(row->children)) > 0);
}
static void
tree_populate(ElimTree *tree, ElimRow *row)
{
	if (!row->lazy)
		return;
	row->lazy = FALSE;
	if (tree && tree->populate)
		tree->populate(tree->view, row, tree->populate_data);
}

/* What the view needs of a row of a tree while its cell is on screen: the
 * expander shows only for a row with children, follows the row's opening and
 * closing (the populate and expanded functions run then), and follows the
 * children the row gets. */
struct _TreeBind {
	GtkWidget *view;
	GtkWidget *expander;
	ElimRow *row;
	GtkTreeListRow *tree_row;
	gulong expanded;
	gulong row_changed;
	GListStore *children;
	gulong children_changed;
};

static void
tree_bind_show(TreeBind *tb)
{
	gtk_tree_expander_set_hide_expander(GTK_TREE_EXPANDER(tb->expander),
					    !row_has_children(tb->row));
}

static void
tree_bind_children_changed(GListModel *children, guint position, guint removed,
			   guint added, gpointer data)
{
	(void)children;
	(void)position;
	(void)removed;
	(void)added;
	tree_bind_show(data);
}

static void
tree_bind_row_changed(ElimRow *row, guint column, gpointer data)
{
	(void)row;
	if (column == ELIM_ROW_CHILDREN)
		tree_bind_show(data);
}

static void
tree_bind_expanded(GtkTreeListRow *tree_row, GParamSpec *pspec, gpointer data)
{
	TreeBind *tb = data;
	ElimTree *tree = view_tree(tb->view);
	gboolean expanded = gtk_tree_list_row_get_expanded(tree_row);

	(void)pspec;
	if (!tree)
		return;
	if (expanded)
		tree_populate(tree, tb->row);
	if (tree->expanded)
		tree->expanded(tree->view, tb->row, expanded, tree->expanded_data);
}

static TreeBind *
tree_bind_new(GtkListItem *item, GtkWidget *expander, ElimRow *row)
{
	gpointer object = gtk_list_item_get_item(item);
	TreeBind *tb;
	GtkWidget *view;

	if (!GTK_IS_TREE_LIST_ROW(object))
		return NULL;
	view = gtk_widget_get_ancestor(expander, GTK_TYPE_COLUMN_VIEW);
	if (!view)
		view = gtk_widget_get_ancestor(expander, GTK_TYPE_LIST_VIEW);
	tb = g_new0(TreeBind, 1);
	tb->view = view;
	tb->expander = expander;
	tb->row = g_object_ref(row);
	tb->tree_row = g_object_ref(GTK_TREE_LIST_ROW(object));
	gtk_tree_expander_set_list_row(GTK_TREE_EXPANDER(expander), tb->tree_row);
	tree_bind_show(tb);
	tb->row_changed = g_signal_connect(row, "changed",
					   G_CALLBACK(tree_bind_row_changed), tb);
	if (row->children) {
		tb->children = g_object_ref(row->children);
		tb->children_changed = g_signal_connect(tb->children, "items-changed",
							G_CALLBACK(tree_bind_children_changed), tb);
	}
	tb->expanded = g_signal_connect(tb->tree_row, "notify::expanded",
					G_CALLBACK(tree_bind_expanded), tb);
	return tb;
}

static void
tree_bind_free(gpointer data)
{
	TreeBind *tb = data;

	g_signal_handler_disconnect(tb->tree_row, tb->expanded);
	g_signal_handler_disconnect(tb->row, tb->row_changed);
	if (tb->children) {
		g_signal_handler_disconnect(tb->children, tb->children_changed);
		g_object_unref(tb->children);
	}
	g_object_unref(tb->tree_row);
	g_object_unref(tb->row);
	g_free(tb);
}

ElimRow *
elim_row_get_parent(ElimRow *row)
{
	return row->parent_row;
}

guint
elim_row_n_children(ElimRow *row)
{
	return row->children ? g_list_model_get_n_items(G_LIST_MODEL(row->children)) : 0;
}

ElimRow *
elim_row_get_child(ElimRow *row, guint position)
{
	return row->children ? elim_table_get(row->children, position) : NULL;
}

guint
elim_row_get_depth(ElimRow *row)
{
	guint depth = 0;

	for (; row->parent_row; row = row->parent_row)
		depth++;
	return depth;
}

GListStore *
elim_row_get_siblings(ElimRow *row, GListStore *roots)
{
	return row->parent_row ? row->parent_row->children : roots;
}

void
elim_row_set_lazy(ElimRow *row, gboolean lazy)
{
	row->lazy = lazy;
	if (lazy && !row->children)
		row->children = elim_table_new();
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, ELIM_ROW_CHILDREN);
}

gboolean
elim_row_is_lazy(ElimRow *row)
{
	return row->lazy;
}

static GListStore *
tree_store_of(GListStore *roots, ElimRow *parent)
{
	if (!parent)
		return roots;
	if (!parent->children)
		parent->children = elim_table_new();
	return parent->children;
}

ElimRow *
elim_tree_insert(GListStore *roots, ElimRow *parent, gint position, guint n_columns)
{
	GListStore *store = tree_store_of(roots, parent);
	ElimRow *row = elim_row_new(n_columns);
	guint n = g_list_model_get_n_items(G_LIST_MODEL(store));

	row->parent_row = parent;
	row->roots = roots;
	row->children = elim_table_new();
	/* the store keeps the row; the caller borrows it */
	if (position < 0 || (guint)position >= n)
		g_list_store_append(store, row);
	else
		g_list_store_insert(store, (guint)position, row);
	g_object_unref(row);
	tree_touch(roots);
	return row;
}

ElimRow *
elim_tree_append(GListStore *roots, ElimRow *parent, guint n_columns)
{
	return elim_tree_insert(roots, parent, -1, n_columns);
}

ElimRow *
elim_tree_prepend(GListStore *roots, ElimRow *parent, guint n_columns)
{
	return elim_tree_insert(roots, parent, 0, n_columns);
}

ElimRow *
elim_tree_insert_after(GListStore *roots, ElimRow *parent, ElimRow *sibling,
		       guint n_columns)
{
	gint at = sibling ? find_row(tree_store_of(roots, parent), sibling) : -1;

	return elim_tree_insert(roots, parent, at < 0 ? -1 : at + 1, n_columns);
}

gboolean
elim_tree_remove(GListStore *roots, ElimRow *row)
{
	GListStore *store = elim_row_get_siblings(row, roots);
	gint position = store ? find_row(store, row) : -1;

	if (position < 0)
		return FALSE;
	row->parent_row = NULL;
	row->roots = NULL;
	g_list_store_remove(store, (guint)position);
	tree_touch(roots);
	return TRUE;
}

void
elim_tree_setup(GtkWidget *view, GListStore *roots)
{
	ElimTree *tree = g_new0(ElimTree, 1);
	GtkSingleSelection *selection;

	tree->view = view;
	tree->model = gtk_tree_list_model_new(G_LIST_MODEL(g_object_ref(roots)),
					      FALSE, FALSE, tree_create_children,
					      NULL, NULL);
	g_object_set_data_full(G_OBJECT(view), "elim-tree", tree, g_free);
	selection = gtk_single_selection_new(G_LIST_MODEL(tree->model));
	gtk_single_selection_set_autoselect(selection, FALSE);
	gtk_single_selection_set_can_unselect(selection, TRUE);
	gtk_single_selection_set_selected(selection, GTK_INVALID_LIST_POSITION);
	view_set_model(view, GTK_SELECTION_MODEL(selection));
	g_object_unref(selection);
}

void
elim_tree_setup_list(GtkWidget *list_view, GListStore *roots,
		     const ElimTextColumn *spec)
{
	ElimColumn col = elim_column_text(spec->column);
	GtkListItemFactory *factory;

	col.text = *spec;
	col.text.expander = TRUE;
	factory = column_factory(&col);
	elim_tree_setup(list_view, roots);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);
	g_object_unref(factory);
}

GtkWidget *
elim_tree_view_new(GListStore *roots)
{
	GtkWidget *view = gtk_column_view_new(NULL);

	elim_tree_setup(view, roots);
	return view;
}

void
elim_tree_set_populate(GtkWidget *view, ElimTreePopulateFunc func, gpointer data)
{
	ElimTree *tree = view_tree(view);

	g_return_if_fail(tree);
	tree->populate = func;
	tree->populate_data = data;
}

void
elim_tree_set_expanded_func(GtkWidget *view, ElimTreeExpandedFunc func,
			    gpointer data)
{
	ElimTree *tree = view_tree(view);

	g_return_if_fail(tree);
	tree->expanded = func;
	tree->expanded_data = data;
}

/* The position of ROW among the rows the view shows, FALSE when it is
 * hidden inside a collapsed row. */
static gboolean
tree_position(ElimTree *tree, ElimRow *row, guint *position)
{
	guint i, n = g_list_model_get_n_items(G_LIST_MODEL(tree->model));

	for (i = 0; i < n; i++) {
		GtkTreeListRow *tree_row = gtk_tree_list_model_get_row(tree->model, i);
		GObject *item = gtk_tree_list_row_get_item(tree_row);
		gboolean found = item == G_OBJECT(row);

		g_object_unref(item);
		g_object_unref(tree_row);
		if (found) {
			*position = i;
			return TRUE;
		}
	}
	return FALSE;
}

static GtkTreeListRow *
tree_list_row(ElimTree *tree, ElimRow *row)
{
	guint position;

	return tree_position(tree, row, &position)
		   ? gtk_tree_list_model_get_row(tree->model, position)
		   : NULL;
}

static void
tree_set_expanded(ElimTree *tree, ElimRow *row, gboolean expanded)
{
	GtkTreeListRow *tree_row = tree_list_row(tree, row);

	if (!tree_row)
		return;
	if (expanded) {
		tree_populate(tree, row);
		if (!row_has_children(row)) {
			g_object_unref(tree_row);
			return;
		}
	}
	gtk_tree_list_row_set_expanded(tree_row, expanded);
	g_object_unref(tree_row);
}

void
elim_tree_expand_to_row(GtkWidget *view, ElimRow *row)
{
	ElimTree *tree = view_tree(view);
	GPtrArray *chain = g_ptr_array_new();
	ElimRow *ancestor;
	guint i;

	g_return_if_fail(tree);
	for (ancestor = row->parent_row; ancestor; ancestor = ancestor->parent_row)
		g_ptr_array_add(chain, ancestor);
	/* from the top down: a row can only be found once its parent is open */
	for (i = chain->len; i > 0; i--)
		tree_set_expanded(tree, g_ptr_array_index(chain, i - 1), TRUE);
	g_ptr_array_free(chain, TRUE);
}

void
elim_tree_expand_row(GtkWidget *view, ElimRow *row, gboolean descendants)
{
	ElimTree *tree = view_tree(view);
	guint position;

	g_return_if_fail(tree);
	elim_tree_expand_to_row(view, row);
	tree_set_expanded(tree, row, TRUE);
	if (!descendants || !tree_position(tree, row, &position))
		return;
	/* the descendants follow the row, so one pass down the list opens them */
	for (position++; position < g_list_model_get_n_items(G_LIST_MODEL(tree->model));
	     position++) {
		GtkTreeListRow *tree_row = gtk_tree_list_model_get_row(tree->model, position);
		ElimRow *item = item_row(tree_row);
		gboolean inside = item != NULL;
		ElimRow *up;

		for (up = item ? item->parent_row : NULL; up && up != row; up = up->parent_row)
			;
		inside = inside && up == row;
		if (inside && row_has_children(item)) {
			tree_populate(tree, item);
			gtk_tree_list_row_set_expanded(tree_row, TRUE);
		}
		g_object_unref(tree_row);
		if (!inside)
			break;
	}
}

void
elim_tree_collapse_row(GtkWidget *view, ElimRow *row)
{
	ElimTree *tree = view_tree(view);

	g_return_if_fail(tree);
	tree_set_expanded(tree, row, FALSE);
}

gboolean
elim_tree_row_expanded(GtkWidget *view, ElimRow *row)
{
	ElimTree *tree = view_tree(view);
	GtkTreeListRow *tree_row = tree ? tree_list_row(tree, row) : NULL;
	gboolean expanded = tree_row && gtk_tree_list_row_get_expanded(tree_row);

	if (tree_row)
		g_object_unref(tree_row);
	return expanded;
}

void
elim_tree_expand_all(GtkWidget *view)
{
	ElimTree *tree = view_tree(view);
	guint i;

	g_return_if_fail(tree);
	/* what a row opens is inserted right after it, so a pass down the list
	 * reaches every level */
	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(tree->model)); i++) {
		GtkTreeListRow *tree_row = gtk_tree_list_model_get_row(tree->model, i);
		ElimRow *row = item_row(tree_row);

		if (row && row_has_children(row)) {
			tree_populate(tree, row);
			gtk_tree_list_row_set_expanded(tree_row, TRUE);
		}
		g_object_unref(tree_row);
	}
}

void
elim_tree_collapse_all(GtkWidget *view)
{
	ElimTree *tree = view_tree(view);
	guint i;

	g_return_if_fail(tree);
	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(tree->model)); i++) {
		GtkTreeListRow *tree_row = gtk_tree_list_model_get_row(tree->model, i);

		if (gtk_tree_list_row_get_depth(tree_row) == 0)
			gtk_tree_list_row_set_expanded(tree_row, FALSE);
		g_object_unref(tree_row);
	}
}

gboolean
elim_tree_select_row(GtkWidget *view, ElimRow *row, gboolean scroll)
{
	ElimTree *tree = view_tree(view);
	guint position;

	g_return_val_if_fail(tree, FALSE);
	elim_tree_expand_to_row(view, row);
	if (!tree_position(tree, row, &position))
		return FALSE;
	elim_table_select(view, position, scroll);
	return TRUE;
}

void
elim_tree_unselect(GtkWidget *view)
{
	gtk_single_selection_set_selected(elim_table_selection(view),
					  GTK_INVALID_LIST_POSITION);
}

/* ---- changes of a tree ---- */

typedef struct {
	ElimTreeChangedFunc func;
	gpointer data;
	guint idle;
} TreeWatch;

static gboolean
tree_watch_fire(gpointer data)
{
	GListStore *roots = data;
	TreeWatch *watch = g_object_get_data(G_OBJECT(roots), "elim-tree-watch");

	if (watch) {
		watch->idle = 0;
		if (watch->func)
			watch->func(roots, watch->data);
	}
	g_object_unref(roots);
	return G_SOURCE_REMOVE;
}

static void
tree_touch(GListStore *roots)
{
	TreeWatch *watch = roots ? g_object_get_data(G_OBJECT(roots), "elim-tree-watch") : NULL;

	if (!watch || !watch->func || watch->idle)
		return;
	/* the idle holds the store, and with it the watch, until it runs */
	watch->idle = g_idle_add(tree_watch_fire, g_object_ref(roots));
}

void
elim_tree_set_changed_func(GListStore *roots, ElimTreeChangedFunc func, gpointer data)
{
	TreeWatch *watch = g_object_get_data(G_OBJECT(roots), "elim-tree-watch");

	if (!watch) {
		watch = g_new0(TreeWatch, 1);
		g_object_set_data_full(G_OBJECT(roots), "elim-tree-watch", watch, g_free);
	}
	watch->func = func;
	watch->data = data;
}

/* ---- rows under a point ---- */

ElimRow *
elim_table_row_at_point(GtkWidget *view, gdouble x, gdouble y)
{
	GtkWidget *widget = gtk_widget_pick(view, x, y, GTK_PICK_DEFAULT);

	for (; widget && widget != view; widget = gtk_widget_get_parent(widget)) {
		ElimRow *row = g_object_get_data(G_OBJECT(widget), "elim-row");

		if (row)
			return row;
	}
	return NULL;
}

/* ---- moving rows of a tree ---- */

static gboolean
row_is_under(ElimRow *row, ElimRow *ancestor)
{
	for (; row; row = row->parent_row)
		if (row == ancestor)
			return TRUE;
	return FALSE;
}

/* ROW and the rows under it that are open, top down. */
static void
collect_open(ElimTree *tree, ElimRow *row, GPtrArray *open)
{
	guint i, n = elim_row_n_children(row);
	GtkTreeListRow *tree_row;

	if (n == 0)
		return;
	tree_row = tree_list_row(tree, row);
	if (tree_row) {
		if (gtk_tree_list_row_get_expanded(tree_row))
			g_ptr_array_add(open, g_object_ref(row));
		g_object_unref(tree_row);
	}
	for (i = 0; i < n; i++)
		collect_open(tree, elim_row_get_child(row, i), open);
}

gboolean
elim_tree_move_row(GtkWidget *view, ElimRow *row, ElimRow *target,
		   ElimTreeDropPosition position)
{
	ElimTree *tree = view_tree(view);
	GListStore *roots = row->roots;
	GListStore *from, *to;
	ElimRow *new_parent;
	GPtrArray *open;
	gint at;
	guint i;

	g_return_val_if_fail(tree, FALSE);
	if (!roots || row == target || row_is_under(target, row))
		return FALSE;
	new_parent = position == ELIM_DROP_INTO ? target : target->parent_row;
	from = elim_row_get_siblings(row, roots);
	at = find_row(from, row);
	if (at < 0)
		return FALSE;
	to = tree_store_of(roots, new_parent);

	/* what is open now is open again where the row lands */
	open = g_ptr_array_new_with_free_func(g_object_unref);
	collect_open(tree, row, open);
	g_object_ref(row);
	g_list_store_remove(from, (guint)at);
	row->parent_row = new_parent;
	if (position == ELIM_DROP_INTO) {
		at = -1;
	} else {
		/* the place counts after the row left its old one */
		at = find_row(to, target);
		if (position == ELIM_DROP_AFTER)
			at++;
	}
	if (at < 0 || (guint)at >= g_list_model_get_n_items(G_LIST_MODEL(to)))
		g_list_store_append(to, row);
	else
		g_list_store_insert(to, (guint)at, row);
	g_object_unref(row);

	if (new_parent)
		elim_tree_expand_row(view, new_parent, FALSE);
	for (i = 0; i < open->len; i++)
		elim_tree_expand_row(view, g_ptr_array_index(open, i), FALSE);
	g_ptr_array_free(open, TRUE);
	elim_tree_select_row(view, row, FALSE);
	tree_touch(roots);
	return TRUE;
}

/* ---- dragging rows of a tree ---- */

static const char *drop_classes[] = {
	[ELIM_DROP_BEFORE] = "elim-drop-before",
	[ELIM_DROP_INTO] = "elim-drop-into",
	[ELIM_DROP_AFTER] = "elim-drop-after",
};

static GtkWidget *
drag_view_of(GtkWidget *cell)
{
	GtkWidget *view = gtk_widget_get_ancestor(cell, GTK_TYPE_COLUMN_VIEW);

	return view ? view : gtk_widget_get_ancestor(cell, GTK_TYPE_LIST_VIEW);
}

static void
drop_mark_clear(GtkWidget *cell)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(drop_classes); i++)
		gtk_widget_remove_css_class(cell, drop_classes[i]);
}

/* Where in the cell the pointer is decides the place: the upper quarter is
 * before it, the lower quarter after it, and between them into it (when the
 * program allows a row there, else the nearer half of before and after). */
static gboolean
drop_place(GtkWidget *cell, GtkWidget *view, ElimRow *row, gdouble y,
	   ElimTreeDropPosition *position)
{
	ElimTree *tree = view_tree(view);
	ElimRow *target = g_object_get_data(G_OBJECT(cell), "elim-row");
	gdouble height = gtk_widget_get_height(cell);
	ElimTreeDropPosition candidates[3];
	guint n = 0, i;

	if (!tree || !tree->reorderable || !target || row == target ||
	    row_is_under(target, row))
		return FALSE;
	if (y > height * 0.25 && y < height * 0.75)
		candidates[n++] = ELIM_DROP_INTO;
	candidates[n++] = y < height / 2 ? ELIM_DROP_BEFORE : ELIM_DROP_AFTER;
	candidates[n++] = y < height / 2 ? ELIM_DROP_AFTER : ELIM_DROP_BEFORE;
	for (i = 0; i < n; i++)
		if (!tree->can_drop ||
		    tree->can_drop(view, row, target, candidates[i], tree->drag_data)) {
			*position = candidates[i];
			return TRUE;
		}
	return FALSE;
}

static GdkContentProvider *
tree_drag_prepare(GtkDragSource *source, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *cell = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(source));
	ElimTree *tree = view_tree(drag_view_of(cell));
	ElimRow *row = g_object_get_data(G_OBJECT(cell), "elim-row");

	(void)x;
	(void)y;
	(void)data;
	if (!tree || !tree->reorderable || !row)
		return NULL;
	return gdk_content_provider_new_typed(ELIM_TYPE_ROW, row);
}

static void
tree_drag_begin(GtkDragSource *source, GdkDrag *drag, gpointer data)
{
	GtkWidget *cell = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(source));
	GdkPaintable *icon = gtk_widget_paintable_new(cell);

	(void)drag;
	(void)data;
	gtk_drag_source_set_icon(source, icon, 0, 0);
	g_object_unref(icon);
}

static GdkDragAction
tree_drop_motion(GtkDropTarget *target, gdouble x, gdouble y, gpointer data)
{
	GtkWidget *cell = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
	GtkWidget *view = drag_view_of(cell);
	const GValue *value = gtk_drop_target_get_value(target);
	ElimTreeDropPosition position;
	ElimRow *row = value && G_VALUE_HOLDS(value, ELIM_TYPE_ROW)
			   ? ELIM_ROW(g_value_get_object(value))
			   : NULL;

	(void)x;
	(void)data;
	drop_mark_clear(cell);
	if (!row || !drop_place(cell, view, row, y, &position))
		return 0;
	gtk_widget_add_css_class(cell, drop_classes[position]);
	return GDK_ACTION_MOVE;
}

static void
tree_drop_leave(GtkDropTarget *target, gpointer data)
{
	(void)data;
	drop_mark_clear(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target)));
}

static gboolean
tree_drop(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y,
	  gpointer data)
{
	GtkWidget *cell = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(target));
	GtkWidget *view = drag_view_of(cell);
	ElimTree *tree = view_tree(view);
	ElimTreeDropPosition position;
	ElimRow *row, *destination = g_object_get_data(G_OBJECT(cell), "elim-row");

	(void)x;
	(void)data;
	drop_mark_clear(cell);
	if (!tree || !G_VALUE_HOLDS(value, ELIM_TYPE_ROW))
		return FALSE;
	row = ELIM_ROW(g_value_get_object(value));
	if (!drop_place(cell, view, row, y, &position) ||
	    !elim_tree_move_row(view, row, destination, position))
		return FALSE;
	if (tree->moved)
		tree->moved(view, row, tree->drag_data);
	return TRUE;
}

/* Makes the cell of a tree row (the expander that holds the row) draggable and
 * a place to drop on. What is dragged, and where, is settled by the tree of the
 * view the cell is in, so this is done for every tree row. */
static void
tree_dnd_attach(GtkWidget *cell)
{
	static gsize css_loaded;
	GtkDragSource *source = gtk_drag_source_new();
	GtkDropTarget *target = gtk_drop_target_new(ELIM_TYPE_ROW, GDK_ACTION_MOVE);

	if (g_once_init_enter(&css_loaded)) {
		GtkCssProvider *css = gtk_css_provider_new();

		gtk_css_provider_load_from_string(css,
		    ".elim-drop-before { box-shadow: inset 0 2px 0 0 currentColor; }"
		    ".elim-drop-after { box-shadow: inset 0 -2px 0 0 currentColor; }"
		    ".elim-drop-into { background-color: alpha(currentColor, 0.15); }");
		gtk_style_context_add_provider_for_display(gdk_display_get_default(),
							   GTK_STYLE_PROVIDER(css),
							   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		g_object_unref(css);
		g_once_init_leave(&css_loaded, 1);
	}
	gtk_drag_source_set_actions(source, GDK_ACTION_MOVE);
	g_signal_connect(source, "prepare", G_CALLBACK(tree_drag_prepare), NULL);
	g_signal_connect(source, "drag-begin", G_CALLBACK(tree_drag_begin), NULL);
	g_signal_connect(target, "motion", G_CALLBACK(tree_drop_motion), NULL);
	g_signal_connect(target, "leave", G_CALLBACK(tree_drop_leave), NULL);
	g_signal_connect(target, "drop", G_CALLBACK(tree_drop), NULL);
	gtk_widget_add_controller(cell, GTK_EVENT_CONTROLLER(source));
	gtk_widget_add_controller(cell, GTK_EVENT_CONTROLLER(target));
}

void
elim_tree_set_drag_funcs(GtkWidget *view, ElimTreeCanDropFunc can_drop,
			 ElimTreeMovedFunc moved, gpointer data)
{
	ElimTree *tree = view_tree(view);

	g_return_if_fail(tree);
	tree->can_drop = can_drop;
	tree->moved = moved;
	tree->drag_data = data;
}

void
elim_tree_set_reorderable(GtkWidget *view, gboolean reorderable)
{
	ElimTree *tree = view_tree(view);

	g_return_if_fail(tree);
	tree->reorderable = reorderable;
}

gboolean
elim_tree_get_reorderable(GtkWidget *view)
{
	ElimTree *tree = view_tree(view);

	return tree && tree->reorderable;
}

gboolean
elim_tree_point_on_expander(GtkWidget *view, gdouble x, gdouble y)
{
	GtkWidget *widget = gtk_widget_pick(view, x, y, GTK_PICK_DEFAULT);

	/* the arrow is the icon of the expander, the indent before it another */
	return widget && GTK_IS_TREE_EXPANDER(gtk_widget_get_parent(widget)) &&
	       !g_strcmp0(gtk_widget_get_css_name(widget), "expander");
}

static void
adopt_roots(ElimRow *row, GListStore *roots)
{
	guint i;

	row->roots = roots;
	for (i = 0; i < elim_row_n_children(row); i++)
		adopt_roots(elim_row_get_child(row, i), roots);
}

void
elim_tree_replace(GListStore *roots, GListStore *staging)
{
	guint i, n = g_list_model_get_n_items(G_LIST_MODEL(staging));
	ElimRow **rows = g_new0(ElimRow *, n ? n : 1);

	for (i = 0; i < n; i++) {
		rows[i] = g_object_ref(elim_table_get(staging, i));
		adopt_roots(rows[i], roots);
	}
	/* one change for the whole tree, so that the views update once */
	g_list_store_splice(roots, 0, g_list_model_get_n_items(G_LIST_MODEL(roots)),
			    (gpointer *)rows, n);
	g_list_store_remove_all(staging);
	for (i = 0; i < n; i++)
		g_object_unref(rows[i]);
	g_free(rows);
	tree_touch(roots);
}
