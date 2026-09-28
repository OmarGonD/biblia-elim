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
	enum { ELIM_CELL_STRING, ELIM_CELL_INT, ELIM_CELL_DOUBLE } type;
	gint number;
	gdouble decimal;
	gchar *text;
} ElimCell;

struct _ElimRow
{
	GObject parent;
	guint n_columns;
	ElimCell *cells;
};

enum { ROW_CHANGED, ROW_LAST_SIGNAL };
static guint row_signals[ROW_LAST_SIGNAL];

G_DEFINE_FINAL_TYPE(ElimRow, elim_row, G_TYPE_OBJECT)

static void
elim_row_finalize(GObject *object)
{
	ElimRow *row = ELIM_ROW(object);
	guint i;

	for (i = 0; i < row->n_columns; i++)
		g_free(row->cells[i].text);
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
	row->cells[column].text = g_strdup(value ? value : "");
	row->cells[column].type = ELIM_CELL_STRING;
	row->cells[column].number = 0;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
}

void
elim_row_set_int(ElimRow *row, guint column, gint value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	row->cells[column].text = NULL;
	row->cells[column].type = ELIM_CELL_INT;
	row->cells[column].number = value;
	row->cells[column].decimal = 0;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
}

void
elim_row_set_double(ElimRow *row, guint column, gdouble value)
{
	g_return_if_fail(column < row->n_columns);
	g_free(row->cells[column].text);
	row->cells[column].text = NULL;
	row->cells[column].type = ELIM_CELL_DOUBLE;
	row->cells[column].number = 0;
	row->cells[column].decimal = value;
	g_signal_emit(row, row_signals[ROW_CHANGED], 0, column);
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
	return g_strdup(row->cells[column].text);
}

GListStore *
elim_table_new(void)
{
	return g_list_store_new(ELIM_TYPE_ROW);
}

/* ---- views ---- */

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
	sorted = gtk_sort_list_model_new(G_LIST_MODEL(g_object_ref(store)),
					 gtk_column_view_get_sorter(GTK_COLUMN_VIEW(view)));
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

/* ---- text cells ---- */

typedef struct {
	ElimTextColumn spec;
	GtkLabel *label;
	ElimRow *row;
	gulong changed;
} CellBinding;

static void
cell_binding_free(gpointer data)
{
	CellBinding *b = data;

	if (b->row && b->changed)
		g_signal_handler_disconnect(b->row, b->changed);
	g_free(b);
}

static void
cell_show(CellBinding *b)
{
	char *text = elim_row_dup_text(b->row, b->spec.column);

	if (b->spec.markup)
		gtk_label_set_markup(b->label, text);
	else
		gtk_label_set_text(b->label, text);
	g_free(text);
	if (b->spec.weight_column >= 0 || b->spec.foreground_column >= 0) {
		PangoAttrList *attributes = pango_attr_list_new();
		if (b->spec.weight_column >= 0)
			pango_attr_list_insert(attributes,
				pango_attr_weight_new((PangoWeight)elim_row_get_int(
					b->row, (guint)b->spec.weight_column)));
		if (b->spec.foreground_column >= 0) {
			GdkRGBA color;
			const char *foreground = elim_row_get_string(
				b->row, (guint)b->spec.foreground_column);
			if (*foreground && gdk_rgba_parse(&color, foreground))
				pango_attr_list_insert(attributes, pango_attr_foreground_new(
					(guint16)(color.red * 65535), (guint16)(color.green * 65535),
					(guint16)(color.blue * 65535)));
		}
		gtk_label_set_attributes(b->label, attributes);
		pango_attr_list_unref(attributes);
	} else {
		gtk_label_set_attributes(b->label, NULL);
	}
	if (b->spec.tooltip_column >= 0) {
		text = elim_row_dup_text(b->row, (guint)b->spec.tooltip_column);
		gtk_widget_set_tooltip_text(GTK_WIDGET(b->label), text);
		g_free(text);
	}
}

static void
cell_row_changed(ElimRow *row, guint column, gpointer data)
{
	CellBinding *b = data;

	(void)row;
	if (column == b->spec.column || (gint)column == b->spec.tooltip_column ||
	    (gint)column == b->spec.weight_column ||
	    (gint)column == b->spec.foreground_column)
		cell_show(b);
}

static void
column_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	const ElimTextColumn *spec = data;
	GtkWidget *label = gtk_label_new(NULL);

	(void)factory;
	gtk_label_set_xalign(GTK_LABEL(label), spec->xalign);
	gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_margin_start(label, 6);
	gtk_widget_set_margin_end(label, 6);
	gtk_widget_set_margin_top(label, spec->pad_y);
	gtk_widget_set_margin_bottom(label, spec->pad_y);
	gtk_list_item_set_child(item, label);
}

static void
column_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	CellBinding *b = g_new0(CellBinding, 1);

	(void)factory;
	b->spec = *(const ElimTextColumn *)data;
	b->label = GTK_LABEL(gtk_list_item_get_child(item));
	b->row = ELIM_ROW(gtk_list_item_get_item(item));
	b->changed = g_signal_connect(b->row, "changed",
				      G_CALLBACK(cell_row_changed), b);
	g_object_set_data_full(G_OBJECT(item), "elim-cell", b, cell_binding_free);
	cell_show(b);
}

static void
column_unbind(GtkSignalListItemFactory *factory, GtkListItem *item,
	      gpointer data)
{
	(void)factory;
	(void)data;
	/* drops the binding, which disconnects from the row */
	g_object_set_data(G_OBJECT(item), "elim-cell", NULL);
}

static GtkListItemFactory *
text_factory(const ElimTextColumn *spec)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	ElimTextColumn *copy = g_memdup2(spec, sizeof *spec);

	g_signal_connect(factory, "setup", G_CALLBACK(column_setup), copy);
	g_signal_connect(factory, "bind", G_CALLBACK(column_bind), copy);
	g_signal_connect(factory, "unbind", G_CALLBACK(column_unbind), NULL);
	/* the factory keeps the copy for as long as it can bind cells */
	g_object_set_data_full(G_OBJECT(factory), "elim-spec", copy, g_free);
	return factory;
}

void
elim_table_setup_list(GtkWidget *list_view, GListStore *store,
		      const ElimTextColumn *spec)
{
	GtkListItemFactory *factory = text_factory(spec);

	elim_table_setup(list_view, store);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);
	g_object_unref(factory);
}

/* ---- rows of several columns, no header ---- */

typedef struct {
	guint n;
	ElimTextColumn *specs;
	GtkSizeGroup **groups;	/* one per column: the same width in every row */
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
	CellBinding **cells;
} RowsBinding;

static void
rows_spec_free(gpointer data)
{
	RowsSpec *r = data;
	guint i;

	for (i = 0; i < r->n; i++)
		g_object_unref(r->groups[i]);
	g_free(r->groups);
	g_free(r->specs);
	g_free(r);
}

static void
rows_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	RowsSpec *r = data;
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	guint i;

	(void)factory;
	for (i = 0; i < r->n; i++) {
		const ElimTextColumn *spec = &r->specs[i];
		GtkWidget *label = gtk_label_new(NULL);

		gtk_label_set_xalign(GTK_LABEL(label), spec->xalign);
		if (spec->wrap) {
			/* the label may shrink to a word (or a character) and wraps
			 * to the width the row leaves it; its natural width is
			 * capped, so a long text does not ask for a line of its
			 * own length. The row gives the other columns their natural
			 * width first, as their gap between minimum and natural is
			 * the smaller one. */
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
		gtk_widget_set_hexpand(label, spec->expand);
		gtk_size_group_add_widget(r->groups[i], label);
		gtk_box_append(GTK_BOX(box), label);
	}
	gtk_list_item_set_child(item, box);
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
		cell_binding_free(b->cells[i]);
	g_free(b->cells);
	g_free(b);
}

static void
rows_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	const RowsSpec *r = data;
	RowsBinding *b = g_new0(RowsBinding, 1);
	ElimRow *row = ELIM_ROW(gtk_list_item_get_item(item));
	GtkWidget *label = gtk_widget_get_first_child(gtk_list_item_get_child(item));
	guint i;

	(void)factory;
	b->n = r->n;
	b->cells = g_new0(CellBinding *, r->n);
	for (i = 0; i < r->n; i++, label = gtk_widget_get_next_sibling(label)) {
		CellBinding *c = g_new0(CellBinding, 1);

		c->spec = r->specs[i];
		c->label = GTK_LABEL(label);
		c->row = row;
		c->changed = g_signal_connect(row, "changed",
					      G_CALLBACK(cell_row_changed), c);
		b->cells[i] = c;
		cell_show(c);
	}
	g_object_set_data(G_OBJECT(gtk_list_item_get_child(item)), "elim-row", row);
	g_object_set_data_full(G_OBJECT(item), "elim-rows", b, rows_binding_free);
}

static void
rows_unbind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	(void)factory;
	(void)data;
	g_object_set_data(G_OBJECT(gtk_list_item_get_child(item)), "elim-row", NULL);
	/* drops the bindings, which disconnect from the row */
	g_object_set_data(G_OBJECT(item), "elim-rows", NULL);
}

static void
setup_rows(GtkWidget *list_view, GListStore *store, const ElimTextColumn *specs,
	   guint n_specs, ElimTableReorderedFunc reordered, gpointer reordered_data)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	RowsSpec *r = g_new0(RowsSpec, 1);
	guint i;

	r->n = n_specs;
	r->view = list_view;
	r->reordered = reordered;
	r->reordered_data = reordered_data;
	r->specs = g_memdup2(specs, n_specs * sizeof *specs);
	r->groups = g_new0(GtkSizeGroup *, n_specs);
	for (i = 0; i < n_specs; i++)
		r->groups[i] = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	g_signal_connect(factory, "setup", G_CALLBACK(rows_setup), r);
	g_signal_connect(factory, "bind", G_CALLBACK(rows_bind), r);
	g_signal_connect(factory, "unbind", G_CALLBACK(rows_unbind), NULL);
	/* the factory keeps the spec for as long as it can make rows */
	g_object_set_data_full(G_OBJECT(factory), "elim-spec", r, rows_spec_free);
	elim_table_setup(list_view, store);
	gtk_list_view_set_factory(GTK_LIST_VIEW(list_view), factory);
	g_object_unref(factory);
}

void
elim_table_setup_rows(GtkWidget *list_view, GListStore *store,
		      const ElimTextColumn *specs, guint n_specs)
{
	setup_rows(list_view, store, specs, n_specs, NULL, NULL);
}

void
elim_table_setup_reorderable_rows(GtkWidget *list_view, GListStore *store,
				  const ElimTextColumn *specs, guint n_specs,
				  ElimTableReorderedFunc reordered, gpointer data)
{
	setup_rows(list_view, store, specs, n_specs, reordered, data);
}

void
elim_table_replace(GListStore *store, ElimRow **rows, guint n_rows)
{
	g_list_store_splice(store, 0, g_list_model_get_n_items(G_LIST_MODEL(store)),
			    (gpointer *)rows, n_rows);
}

GtkColumnViewColumn *
elim_table_add_column(GtkWidget *view, const char *title,
		      const ElimTextColumn *spec)
{
	/* the column takes over the factory */
	GtkColumnViewColumn *col =
	    gtk_column_view_column_new(title, text_factory(spec));

	gtk_column_view_column_set_expand(col, spec->expand);
	gtk_column_view_column_set_resizable(col, TRUE);
	if (spec->fixed_width > 0)
		gtk_column_view_column_set_fixed_width(col, spec->fixed_width);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), col);
	g_object_unref(col);
	return col;
}

GtkColumnViewColumn *
elim_table_add_text_column(GtkWidget *view, const char *title, guint column,
			   gboolean expand)
{
	ElimTextColumn spec = elim_text_column(column);

	spec.expand = expand;
	return elim_table_add_column(view, title, &spec);
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

/* ---- check boxes ---- */

typedef struct {
	guint column;
	ElimToggleFunc func;
	gpointer data;
} ToggleSpec;

/* The function that runs when the box is clicked may refill the store, which
 * unbinds this cell in the middle of it: the binding lives until that call
 * has returned, and holds its row. */
typedef struct {
	ToggleSpec *spec;
	GtkCheckButton *check;
	ElimRow *row;
	gulong row_changed;
	gulong toggled;
	gboolean updating;
	gboolean bound;
	guint refs;
} ToggleBinding;

static void
toggle_binding_unref(ToggleBinding *b)
{
	if (--b->refs)
		return;
	g_object_unref(b->row);
	g_free(b);
}

static void
toggle_binding_free(gpointer data)
{
	ToggleBinding *b = data;

	b->bound = FALSE;
	g_signal_handler_disconnect(b->row, b->row_changed);
	g_signal_handler_disconnect(b->check, b->toggled);
	toggle_binding_unref(b);
}

static void
toggle_show(ToggleBinding *b)
{
	b->updating = TRUE;
	gtk_check_button_set_active(b->check,
				    elim_row_get_int(b->row, b->spec->column) != 0);
	b->updating = FALSE;
}

static void
toggle_row_changed(ElimRow *row, guint column, gpointer data)
{
	ToggleBinding *b = data;

	(void)row;
	if (column == b->spec->column)
		toggle_show(b);
}

static void
toggle_clicked(GtkCheckButton *check, gpointer data)
{
	ToggleBinding *b = data;

	(void)check;
	if (b->updating)
		return;
	b->refs++;
	if (b->spec->func)
		b->spec->func(b->row, b->spec->data);
	/* what the row holds now is what the box shows, unless the cell went
	 * with the call */
	if (b->bound)
		toggle_show(b);
	toggle_binding_unref(b);
}

static void
toggle_setup(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	GtkWidget *check = gtk_check_button_new();

	(void)factory;
	(void)data;
	gtk_widget_set_halign(check, GTK_ALIGN_CENTER);
	gtk_list_item_set_child(item, check);
}

static void
toggle_bind(GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
	ToggleBinding *b = g_new0(ToggleBinding, 1);

	(void)factory;
	b->spec = data;
	b->refs = 1;
	b->bound = TRUE;
	b->check = GTK_CHECK_BUTTON(gtk_list_item_get_child(item));
	b->row = g_object_ref(ELIM_ROW(gtk_list_item_get_item(item)));
	b->row_changed = g_signal_connect(b->row, "changed",
					  G_CALLBACK(toggle_row_changed), b);
	b->toggled = g_signal_connect(b->check, "toggled",
				      G_CALLBACK(toggle_clicked), b);
	g_object_set_data_full(G_OBJECT(item), "elim-toggle", b,
			       toggle_binding_free);
	toggle_show(b);
}

static void
toggle_unbind(GtkSignalListItemFactory *factory, GtkListItem *item,
	      gpointer data)
{
	(void)factory;
	(void)data;
	g_object_set_data(G_OBJECT(item), "elim-toggle", NULL);
}

GtkColumnViewColumn *
elim_table_add_toggle_column(GtkWidget *view, const char *title, guint column,
			     ElimToggleFunc toggled, gpointer data)
{
	GtkListItemFactory *factory = gtk_signal_list_item_factory_new();
	ToggleSpec *spec = g_new0(ToggleSpec, 1);
	GtkColumnViewColumn *col;

	spec->column = column;
	spec->func = toggled;
	spec->data = data;
	g_signal_connect(factory, "setup", G_CALLBACK(toggle_setup), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(toggle_bind), spec);
	g_signal_connect(factory, "unbind", G_CALLBACK(toggle_unbind), NULL);
	g_object_set_data_full(G_OBJECT(factory), "elim-spec", spec, g_free);
	/* the column takes over the factory */
	col = gtk_column_view_column_new(title, factory);
	gtk_column_view_append_column(GTK_COLUMN_VIEW(view), col);
	g_object_unref(col);
	return col;
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
		   ? ELIM_ROW(gtk_single_selection_get_selected_item(
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
	return G_LIST_STORE(model);
}
