/*
 * Xiphos Bible Study Tool
 * gs_parallel.c - support for displaying multiple modules
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"

#include "xiphos_html/xiphos_html.h"

#include "gui/parallel_view.h"
#include "gui/parallel_dialog.h"
#include "gui/main_window.h"
#include "gui/xiphos.h"
#include "gui/widgets.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/preferences_dialog.h"

#include "main/parallel_view.h"
#include "main/reading_focus.h"
#include "main/global_ops.hh"
#include "main/lists.h"
#include "main/sword.h"
#include "main/xml.h"

extern gboolean shift_key_pressed;

/******************************************************************************
 * static
 */

/******************************************************************************
 * Name
 *   on_undockInt_activate
 *
 * Synopsis
 *   #include "gui/parallel.h
 *
 *   void on_undockInt_activate(gpointer unused)
 *
 * Description
 *   undock/dock parallel page
 *
 * Return value
 *   void
 */

void on_undockInt_activate(gpointer unused)
{
	if (settings.dockedInt) {
		settings.dockedInt = FALSE;
		gui_undock_parallel_page();

	} else {
		settings.dockedInt = TRUE;
		gui_btnDockInt_clicked(NULL, NULL);
	}
}

/******************************************************************************
 * Name
 *   on_paratab_activate
 *
 * Synopsis
 *   #include "gui/parallel_view.h
 *
 *   void on_paratab_activate(gpointer unused)
 *
 * Description
 *   open parallel view in a tab
 *
 * Return value
 *   void
 */

void on_paratab_activate(gpointer unused)
{
	gui_open_parallel_view_in_new_tab();
}

static void on_detach(GSimpleAction *action, GVariant *parameter, gpointer data)
{
	(void)action;
	(void)parameter;
	(void)data;
	on_undockInt_activate(NULL);
}


/* GTK4-PORT-101 step 2: a GMenu popover at the pointer over RELATIVE,
 * with fresh «paralelo» actions (their states follow the settings). */
void gui_popup_menu_parallel(GtkWidget *relative)
{
	GSimpleActionGroup *actions = g_simple_action_group_new();
	GMenu *menu = g_menu_new();
	if (!settings.showparatab) {
		static const GActionEntry detach[] = {
			{ "separar", on_detach, NULL, NULL, NULL, { 0 } },
		};
		g_action_map_add_action_entries(G_ACTION_MAP(actions), detach, 1, NULL);
		g_menu_append(menu, _("Detach/Attach"), "paralelo.separar");
	}
	GMenu *options = g_menu_new();
	main_parallel_options_menu(options, G_ACTION_MAP(actions));
	g_menu_append_submenu(menu, _("Module Options"), G_MENU_MODEL(options));
	g_object_unref(options);
	gui_widget_insert_action_group(relative, "paralelo", G_ACTION_GROUP(actions));
	g_object_unref(actions);

	gui_popup_menu_model_at_pointer(G_MENU_MODEL(menu), relative);
	g_object_unref(menu);
}

static void
_popupmenu_requested_cb(XiphosHtml *html, gchar *uri, gpointer user_data)
{
	gui_popup_menu_parallel(GTK_WIDGET(html));
}

/* The docked page: two versions side by side, each with its own chapter
 * pane, kept on the same verse. Whichever pane the reader last scrolled,
 * clicked or typed in leads; the other follows it verse by verse, through
 * each Bible's own numbering (main_parallel_map_anchor()). */

#define N_PANES 2
/* Reading line of each pane, as in the main pane: the verse on it carries
 * the focus band, and the other pane puts its counterpart at the same
 * height, so the two bands sit side by side. */
#define READING_LINE(vis) ((vis).y + (gint)((vis).height * READING_FOCUS_LINE_RATIO))
#define RESERVE_TAG "elim-parallel-reserve"

static GtkWidget *page_box;
static GtkWidget *pane_html[N_PANES];
static GtkWidget *pane_combo[N_PANES];
static gulong pane_combo_id[N_PANES];
static gint leader = -1;
static gboolean syncing;
static gboolean panes_dirty = TRUE;
static guint shown_idle;
/* last anchor mapped from each pane, and what it mapped to */
static gint mapped_from[N_PANES], mapped_to[N_PANES];
/* the verse each pane's focus band is on (0: none) */
static gint focus_anchor[N_PANES];
static guint reserve_idle[N_PANES];
/* Row alignment: the space added, per pane, in line order. */
typedef struct {
	gint line;
	gint px;
	gboolean below;	/* under `line` (a row's start) or above it (its verse) */
} RowPad;
static GArray *row_pads[N_PANES];
static guint align_timeout;
/* what each pane holds: the Bible and the book it was laid out for */
static gchar *rendered_module[N_PANES], *rendered_book[N_PANES];
static guint layouts;
/* a pane gliding to the next verse (gui_parallel_panes_follow()) */
#define GLIDE_US 220000
static struct {
	gdouble from, to;
	gint64 start;
	guint tick;
} glide[N_PANES];

static void schedule_alignment(void);

static gboolean
pane_is_bible(const char *name)
{
	for (GList *l = get_list(TEXT_LIST); name && l; l = l->next)
		if (l->data && !strcmp((const char *)l->data, name))
			return TRUE;
	return FALSE;
}

/* The first two of the parallel list (the same list Preferences, the
 * detached table and reading mode's comparison use), made up from the
 * main pane's Bible and the next one when it is shorter. */
const char *
gui_parallel_pane_module(gint pane)
{
	const char *first = NULL;
	gint n = 0;

	if (pane < 0 || pane >= N_PANES)
		return NULL;
	while (settings.parallel_list && settings.parallel_list[n])
		n++;
	if (pane < n)
		return settings.parallel_list[pane];
	first = n ? settings.parallel_list[0] : settings.MainWindowModule;
	if (pane == 0)
		return first;
	for (GList *l = get_list(TEXT_LIST); l; l = l->next)
		if (l->data && g_strcmp0((const char *)l->data, first))
			return (const char *)l->data;
	return first;
}

guint
gui_parallel_panes_layouts(void)
{
	return layouts;
}

GtkWidget *
gui_parallel_pane(gint pane)
{
	return (pane >= 0 && pane < N_PANES) ? pane_html[pane] : NULL;
}

/* Puts `name` in the list's place `pane`, the others kept in order, and
 * saves the list where Preferences keeps it. */
static void
set_pane_module(gint pane, const char *name)
{
	GPtrArray *list = g_ptr_array_new_with_free_func(g_free);
	gint i;

	for (i = 0; i < N_PANES; i++)
		g_ptr_array_add(list, g_strdup(i == pane ? name
							 : gui_parallel_pane_module(i)));
	for (i = N_PANES; settings.parallel_list && settings.parallel_list[i]; i++)
		g_ptr_array_add(list, g_strdup(settings.parallel_list[i]));
	g_ptr_array_add(list, NULL);

	gchar **names = (gchar **)g_ptr_array_free(list, FALSE);
	gchar *csv = g_strjoinv(",", names);
	xml_set_value("Xiphos", "modules", "parallels", csv);
	g_free(csv);
	g_strfreev(settings.parallel_list);
	settings.parallel_list = names;
}

static void
whole_label_setup(GtkSignalListItemFactory *factory, GtkListItem *item,
		  gpointer data)
{
	GtkWidget *label = gtk_label_new(NULL);

	(void)factory;
	(void)data;
	gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
	gtk_list_item_set_child(item, label);
}

static void
fill_pane_combo(gint pane)
{
	GtkDropDown *combo = GTK_DROP_DOWN(pane_combo[pane]);
	GList *d = get_list(TEXT_DESC_LIST);

	g_signal_handler_block(combo, pane_combo_id[pane]);
	elim_dropdown_remove_all(combo);
	for (GList *l = get_list(TEXT_LIST); l; l = l->next, d = d ? d->next : NULL) {
		const char *desc = d ? (const char *)d->data : NULL;

		if (l->data)
			elim_dropdown_append(combo, (const char *)l->data,
					     desc && *desc ? desc : (const char *)l->data);
	}
	elim_dropdown_set_active_id(combo, gui_parallel_pane_module(pane));
	g_signal_handler_unblock(combo, pane_combo_id[pane]);
}

static GtkAdjustment *
pane_vadj_of(GtkWidget *html)
{
	GtkTextView *view = html ? wk_html_get_view(WK_HTML(html)) : NULL;
	return view ? gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view)) : NULL;
}

static GtkAdjustment *
pane_vadj(gint pane)
{
	return pane_vadj_of(pane_html[pane]);
}

typedef struct {
	gint anchor, top, bottom;
} VerseBlock;

typedef struct {
	gint line;
	VerseBlock found;
} LineBlock;

/* The last verse (or chapter heading) starting at or above the line: the
 * one the line is in, or the one it has just gone past (the gap before the
 * next chapter's title, the room under the last verse). "TOP" and "0hdr"
 * are around the chapters, not verses. */
static gboolean
block_at_line(const gchar *name, gint top, gint bottom, gpointer data)
{
	LineBlock *at = data;
	gint anchor = atoi(name);

	if (top > at->line)
		return FALSE;
	if (anchor >= 1000)
		at->found = (VerseBlock){ anchor, top, bottom };
	return TRUE;
}

static gboolean
last_verse_block(const gchar *name, gint top, gint bottom, gpointer data)
{
	gint anchor = atoi(name);

	if (anchor < 1000 || anchor % 1000 == 0)
		return TRUE;
	*(VerseBlock *)data = (VerseBlock){ anchor, top, bottom };
	return FALSE;
}

/* Moves pane's focus band onto verse `anchor`, or takes it off (0). */
static void
set_focus(gint pane, gint anchor)
{
	GtkTextIter s, e;
	gchar name[16];

	if (!pane_html[pane] || focus_anchor[pane] == anchor)
		return;
	focus_anchor[pane] = anchor;
	g_snprintf(name, sizeof name, "%d", anchor);
	if (anchor && wk_html_anchor_bounds(WK_HTML(pane_html[pane]), name, &s, &e))
		wk_html_reading_focus_set(WK_HTML(pane_html[pane]), &s, &e, NULL, NULL);
	else
		wk_html_reading_focus_set(WK_HTML(pane_html[pane]), NULL, NULL,
					  NULL, NULL);
}

/* Room under the last verse, so the reading line can reach it: spacing
 * below the buffer's last paragraph (a tag), not text, as in the main
 * pane's reading reserve. Applied from an idle, never while GTK lays the
 * view out. */
static gboolean
apply_reserve(gpointer data)
{
	gint pane = GPOINTER_TO_INT(data);
	GtkTextView *view;
	GtkTextBuffer *buffer;
	GtkTextTag *tag;
	GtkTextIter start, end, last;
	GdkRectangle vis;
	VerseBlock last_verse = { 0, 0, 0 };
	gint applied = 0, want, line_y = 0, line_height = 0;
	gboolean on_last;

	reserve_idle[pane] = 0;
	view = pane_html[pane] ? wk_html_get_view(WK_HTML(pane_html[pane])) : NULL;
	if (!view || !gtk_widget_get_mapped(GTK_WIDGET(view)))
		return G_SOURCE_REMOVE;
	gtk_text_view_get_visible_rect(view, &vis);
	if (vis.height <= 1)
		return G_SOURCE_REMOVE;
	buffer = gtk_text_view_get_buffer(view);
	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer),
					RESERVE_TAG);
	if (!tag)
		tag = gtk_text_buffer_create_tag(buffer, RESERVE_TAG,
						 "pixels-below-lines", 0, NULL);
	g_object_get(tag, "pixels-below-lines", &applied, NULL);
	gtk_text_buffer_get_bounds(buffer, &start, &end);
	last = end;
	while (gtk_text_iter_backward_char(&last) && gtk_text_iter_ends_line(&last))
		;
	gtk_text_iter_set_line_offset(&last, 0);
	on_last = gtk_text_iter_has_tag(&last, tag);

	/* the main pane's rule: just enough for the last verse to pass the
	 * reading line */
	wk_html_foreach_anchor_block_reverse(WK_HTML(pane_html[pane]),
					     last_verse_block, &last_verse);
	if (!last_verse.anchor)
		return G_SOURCE_REMOVE;
	gtk_text_view_get_line_yrange(view, &end, &line_y, &line_height);
	want = reading_focus_bottom_reserve(
	    vis.height,
	    line_y + line_height + gtk_text_view_get_bottom_margin(view) -
		(on_last ? applied : 0),
	    last_verse.top);
	if (want == (on_last ? applied : 0))
		return G_SOURCE_REMOVE;
	g_object_set(tag, "pixels-below-lines", want, NULL);
	if (!on_last) {
		gtk_text_buffer_remove_tag(buffer, tag, &start, &end);
		gtk_text_buffer_apply_tag(buffer, tag, &last, &end);
	}
	return G_SOURCE_REMOVE;
}

static void
on_pane_range_changed(GtkAdjustment *adj, gpointer data)
{
	gint pane = GPOINTER_TO_INT(data);

	(void)adj;
	if (!reserve_idle[pane])
		reserve_idle[pane] = g_idle_add(apply_reserve, data);
	schedule_alignment();
}

/* ---- Rows: equivalent verses start at the same height --------------------
 *
 * Each verse is a row: the headings in front of it, then its text
 * (wk_html_anchor_row()). Walking the left pane's rows in order with their
 * counterparts on the right, two kinds of space line them up, as a table's
 * rows are:
 *   - under the previous row's last line of the pane whose previous row
 *     ended higher, so both rows start together (the verse rules are drawn
 *     there);
 *   - above the verse's first line in the pane with less heading in front
 *     of it, so both verses' text starts together.
 * Everything after a row then stays together, in two panes that still
 * scroll on their own. The space is paragraph spacing, not text: nothing
 * to select, copy or find. Line heights off screen are exact only once GTK
 * has laid those lines out, so this runs again whenever a pane's height
 * changes, and changes nothing more once both are laid out. */

#define ROW_PAD_TAG "elim-row-%s-%d"
/* the view's own pixels-above/below-lines, which a tag's replace */
#define VIEW_PIXELS_ABOVE 1
#define VIEW_PIXELS_BELOW 1

static gboolean
collect_anchor_name(const gchar *name, gint top, gint bottom, gpointer data)
{
	(void)top;
	(void)bottom;
	if (atoi(name) >= 1000)
		g_ptr_array_add((GPtrArray *)data, g_strdup(name));
	return TRUE;
}

static gboolean
pane_ready(gint pane)
{
	GtkTextView *view = pane_html[pane]
		? wk_html_get_view(WK_HTML(pane_html[pane])) : NULL;

	return view && gtk_widget_get_mapped(GTK_WIDGET(view));
}

/* Top of `line` in `pane` as laid out without any of the space added: the
 * space on earlier lines is taken off (a line's own is inside it, below its
 * top). */
static gint
natural_top(gint pane, gint line)
{
	GtkTextView *view = wk_html_get_view(WK_HTML(pane_html[pane]));
	GtkTextIter at;
	gint y, h, added = 0;
	guint i;

	for (i = 0; row_pads[pane] && i < row_pads[pane]->len; i++) {
		const RowPad *pad = &g_array_index(row_pads[pane], RowPad, i);

		if (pad->line >= line)
			break;
		added += pad->px;
	}
	gtk_text_buffer_get_iter_at_line(gtk_text_view_get_buffer(view), &at, line);
	gtk_text_view_get_line_yrange(view, &at, &y, &h);
	return y - added;
}

static gboolean
same_pads(GArray *a, GArray *b)
{
	guint i;

	if ((a ? a->len : 0) != (b ? b->len : 0))
		return FALSE;
	for (i = 0; a && i < a->len; i++) {
		const RowPad *x = &g_array_index(a, RowPad, i);
		const RowPad *y = &g_array_index(b, RowPad, i);

		if (x->line != y->line || x->px != y->px || x->below != y->below)
			return FALSE;
	}
	return TRUE;
}

static GtkTextTag *
pad_tag(GtkTextTagTable *table, GtkTextBuffer *buf, const RowPad *pad,
	gboolean create)
{
	gchar name[40];
	GtkTextTag *tag;

	g_snprintf(name, sizeof name, ROW_PAD_TAG,
		   pad->below ? "below" : "above", pad->px);
	tag = gtk_text_tag_table_lookup(table, name);
	if (!tag && create)
		tag = pad->below
			? gtk_text_buffer_create_tag(buf, name, "pixels-below-lines",
						     VIEW_PIXELS_BELOW + pad->px, NULL)
			: gtk_text_buffer_create_tag(buf, name, "pixels-above-lines",
						     VIEW_PIXELS_ABOVE + pad->px, NULL);
	return tag;
}

static void
apply_pads(gint pane, GArray *pads)
{
	GtkTextBuffer *buf = gtk_text_view_get_buffer(
	    wk_html_get_view(WK_HTML(pane_html[pane])));
	GtkTextTagTable *table = gtk_text_buffer_get_tag_table(buf);
	GtkTextIter s, e;
	guint i;

	gtk_text_buffer_get_bounds(buf, &s, &e);
	for (i = 0; row_pads[pane] && i < row_pads[pane]->len; i++) {
		GtkTextTag *tag = pad_tag(table, buf,
					  &g_array_index(row_pads[pane], RowPad, i),
					  FALSE);
		if (tag)
			gtk_text_buffer_remove_tag(buf, tag, &s, &e);
	}
	/* a paragraph's spacing comes from the tags on its first character */
	for (i = 0; i < pads->len; i++) {
		const RowPad *pad = &g_array_index(pads, RowPad, i);

		gtk_text_buffer_get_iter_at_line(buf, &s, pad->line);
		e = s;
		gtk_text_iter_forward_char(&e);
		gtk_text_buffer_apply_tag(buf, pad_tag(table, buf, pad, TRUE), &s, &e);
	}
	if (row_pads[pane])
		g_array_free(row_pads[pane], TRUE);
	row_pads[pane] = pads;
}

/* Adds `px` of space to pane's `pads` at `line`, and to what it shifts. */
static void
add_pad(GArray *pads, gint *shift, gint line, gint px, gboolean below)
{
	RowPad pad = { line, px, below };

	if (px <= 0)
		return;
	g_array_append_val(pads, pad);
	*shift += px;
}

static void sync_from(gint from);
static void schedule_placement(void);

/* Where `anchor`'s row starts and its verse's first line are, in pane. */
static gboolean
row_lines(gint pane, const gchar *anchor, gint *row_line, gint *text_line)
{
	GtkTextIter row, s, e;

	if (!wk_html_anchor_row(WK_HTML(pane_html[pane]), anchor, &row) ||
	    !wk_html_anchor_bounds(WK_HTML(pane_html[pane]), anchor, &s, &e))
		return FALSE;
	*row_line = gtk_text_iter_get_line(&row);
	*text_line = MAX(*row_line, gtk_text_iter_get_line(&s));
	return *row_line > 0;
}

static gboolean
align_rows(gpointer data)
{
	GPtrArray *names;
	GArray *pads[N_PANES];
	GHashTable *mapped;
	const char *left, *right;
	guint i;
	gint shift[N_PANES] = { 0, 0 }, last_line[N_PANES] = { -1, -1 };

	(void)data;
	align_timeout = 0;
	if (!pane_ready(0) || !pane_ready(1))
		return G_SOURCE_REMOVE;
	left = gui_parallel_pane_module(0);
	right = gui_parallel_pane_module(1);
	names = g_ptr_array_new_with_free_func(g_free);
	wk_html_foreach_anchor_block(WK_HTML(pane_html[0]), 0, G_MAXINT / 2,
				     collect_anchor_name, names);
	/* the mapping is fixed for a pair of layouts: kept on pane 0 */
	mapped = g_object_get_data(G_OBJECT(pane_html[0]), "elim-row-map");
	if (!mapped) {
		mapped = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
		g_object_set_data_full(G_OBJECT(pane_html[0]), "elim-row-map", mapped,
				       (GDestroyNotify)g_hash_table_unref);
	}
	for (i = 0; i < N_PANES; i++)
		pads[i] = g_array_new(FALSE, FALSE, sizeof(RowPad));

	for (i = 0; i < names->len; i++) {
		const gchar *name = g_ptr_array_index(names, i);
		gpointer hit;
		gint other, p, row[N_PANES], text[N_PANES], top[N_PANES];
		gchar other_name[16];

		if (g_hash_table_lookup_extended(mapped, name, NULL, &hit))
			other = GPOINTER_TO_INT(hit);
		else {
			other = main_parallel_map_anchor(left, right, atoi(name));
			g_hash_table_insert(mapped, g_strdup(name), GINT_TO_POINTER(other));
		}
		g_snprintf(other_name, sizeof other_name, "%d", other);
		if (!other || !row_lines(0, name, &row[0], &text[0]) ||
		    !row_lines(1, other_name, &row[1], &text[1]) ||
		    row[0] <= last_line[0] || row[1] <= last_line[1])
			continue;

		/* the rows start together: space under the previous row */
		for (p = 0; p < N_PANES; p++)
			top[p] = natural_top(p, row[p]) + shift[p];
		p = top[0] < top[1] ? 0 : 1;
		add_pad(pads[p], &shift[p], row[p] - 1, top[1 - p] - top[p], TRUE);

		/* and so do their verses: space above the verse's first line (a
		 * chapter's anchor comes after its title: its row is enough) */
		if (atoi(name) % 1000) {
			for (p = 0; p < N_PANES; p++)
				top[p] = natural_top(p, text[p]) + shift[p];
			p = top[0] < top[1] ? 0 : 1;
			add_pad(pads[p], &shift[p], text[p], top[1 - p] - top[p], FALSE);
		}

		last_line[0] = row[0];
		last_line[1] = row[1];
	}
	g_ptr_array_free(names, TRUE);

	if (same_pads(pads[0], row_pads[0]) && same_pads(pads[1], row_pads[1])) {
		g_array_free(pads[0], TRUE);
		g_array_free(pads[1], TRUE);
		return G_SOURCE_REMOVE;
	}
	for (i = 0; i < N_PANES; i++)
		apply_pads(i, pads[i]);
	/* the rows moved under the reader: the other pane catches up; before
	 * anyone has scrolled, the current verse goes back on the line */
	if (leader >= 0)
		sync_from(leader);
	else
		schedule_placement();
	return G_SOURCE_REMOVE;
}

static void
schedule_alignment(void)
{
	if (align_timeout)
		g_source_remove(align_timeout);
	/* below GTK's own layout validation (an idle of higher priority),
	 * so the heights measured are the laid-out ones */
	align_timeout = g_timeout_add_full(G_PRIORITY_LOW, 60, align_rows, NULL,
					   NULL);
}

/* Moves the focus band of pane `from` to the verse on its reading line and
 * brings the other pane's counterpart, with its band, to the same height,
 * as far into that verse as `from` is into its own. */
static void
sync_from(gint from)
{
	gint to = 1 - from;
	GtkTextView *from_view, *to_view;
	GtkAdjustment *from_adj, *to_adj;
	GdkRectangle from_vis, to_vis;
	LineBlock at = { 0, { 0, 0, 0 } };
	VerseBlock block;
	gint line, mapped, top, bottom;
	gchar name[16];
	gint target;

	if (!pane_html[from] || !pane_html[to])
		return;
	from_view = wk_html_get_view(WK_HTML(pane_html[from]));
	to_view = wk_html_get_view(WK_HTML(pane_html[to]));
	from_adj = pane_vadj(from);
	to_adj = pane_vadj(to);
	if (!from_view || !to_view || !from_adj || !to_adj ||
	    !gtk_widget_get_mapped(GTK_WIDGET(from_view)) ||
	    !gtk_widget_get_mapped(GTK_WIDGET(to_view)))
		return;
	gtk_text_view_get_visible_rect(from_view, &from_vis);
	gtk_text_view_get_visible_rect(to_view, &to_vis);
	if (from_vis.height <= 1 || to_vis.height <= 1)
		return;

	line = READING_LINE(from_vis);
	at.line = line;
	wk_html_foreach_anchor_block(WK_HTML(pane_html[from]), from_vis.y, line + 1,
				     block_at_line, &at);
	block = at.found;
	if (!block.anchor)
		return;
	if (block.anchor != mapped_from[from]) {
		mapped_from[from] = block.anchor;
		mapped_to[from] = main_parallel_map_anchor(
		    gui_parallel_pane_module(from), gui_parallel_pane_module(to),
		    block.anchor);
	}
	mapped = mapped_to[from];
	/* a chapter's heading block is not a verse: the bands stay put */
	if (block.anchor % 1000) {
		set_focus(from, block.anchor);
		set_focus(to, mapped % 1000 ? mapped : 0);
	}
	syncing = TRUE;
	wk_html_cancel_anchor_jump(WK_HTML(pane_html[to]));
	g_snprintf(name, sizeof name, "%d", mapped);
	if (mapped &&
	    wk_html_anchor_block(WK_HTML(pane_html[to]), name, &top, &bottom)) {
		/* The two verses start at the same height and, with the rows
		 * lined up, so does everything after them: the same distance
		 * into both. */
		target = top + (line - block.top);
		/* visible-rect and adjustment differ by the view's margin:
		 * move by the difference rather than set a coordinate */
		gtk_adjustment_set_value(
		    to_adj, gtk_adjustment_get_value(to_adj) +
				(target - (line - from_vis.y)) - to_vis.y);
	}
	syncing = FALSE;
}

static void
on_pane_scrolled(GtkAdjustment *adj, gpointer data)
{
	(void)adj;
	if (!syncing && leader == GPOINTER_TO_INT(data))
		sync_from(leader);
}

static void
stop_glide(gint pane)
{
	if (glide[pane].tick && pane_html[pane])
		gtk_widget_remove_tick_callback(
		    GTK_WIDGET(wk_html_get_view(WK_HTML(pane_html[pane]))),
		    glide[pane].tick);
	glide[pane].tick = 0;
}

static gboolean
glide_step(GtkWidget *widget, GdkFrameClock *clock, gpointer data)
{
	gint pane = GPOINTER_TO_INT(data);
	gdouble t = (gdk_frame_clock_get_frame_time(clock) - glide[pane].start) /
		    (gdouble)GLIDE_US;
	gdouble eased;

	(void)widget;
	t = CLAMP(t, 0.0, 1.0);
	eased = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);	/* ease out */
	syncing = TRUE;
	gtk_adjustment_set_value(pane_vadj(pane),
				 glide[pane].from +
				     (glide[pane].to - glide[pane].from) * eased);
	syncing = FALSE;
	if (t < 1.0)
		return G_SOURCE_CONTINUE;
	glide[pane].tick = 0;
	return G_SOURCE_REMOVE;
}

/* Scrolls pane to `value`: gliding there frame by frame, from wherever it
 * is now (a glide under way included), or at once. The other pane is moved
 * the same way by its own glide, not by following: the focus bands are
 * already where the glides end. */
static void
glide_to(gint pane, gdouble value, gboolean animate)
{
	GtkWidget *view = GTK_WIDGET(wk_html_get_view(WK_HTML(pane_html[pane])));
	GtkAdjustment *adj = pane_vadj(pane);
	GdkFrameClock *clock = gtk_widget_get_frame_clock(view);

	stop_glide(pane);
	value = CLAMP(value, gtk_adjustment_get_lower(adj),
		      gtk_adjustment_get_upper(adj) -
			  gtk_adjustment_get_page_size(adj));
	if (!animate || !clock || ABS(value - gtk_adjustment_get_value(adj)) < 1.0) {
		syncing = TRUE;
		gtk_adjustment_set_value(adj, value);
		syncing = FALSE;
		return;
	}
	glide[pane].from = gtk_adjustment_get_value(adj);
	glide[pane].to = value;
	glide[pane].start = gdk_frame_clock_get_frame_time(clock);
	glide[pane].tick = gtk_widget_add_tick_callback(view, glide_step,
							GINT_TO_POINTER(pane), NULL);
}

void
gui_parallel_pane_take_lead(gint pane)
{
	gint i;

	/* the reader takes over from a glide */
	for (i = 0; i < N_PANES; i++)
		stop_glide(i);
	leader = pane;
}

static gboolean
on_pane_scroll(GtkEventControllerScroll *controller, gdouble dx, gdouble dy,
	       gpointer data)
{
	(void)controller;
	(void)dx;
	(void)dy;
	gui_parallel_pane_take_lead(GPOINTER_TO_INT(data));
	return FALSE;
}

static gboolean
on_pane_key(GtkEventControllerKey *controller, guint keyval, guint keycode,
	    GdkModifierType state, gpointer data)
{
	(void)controller;
	(void)keyval;
	(void)keycode;
	(void)state;
	gui_parallel_pane_take_lead(GPOINTER_TO_INT(data));
	return FALSE;
}

static void
on_pane_pressed(GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y,
		gpointer data)
{
	(void)gesture;
	(void)n_press;
	(void)x;
	(void)y;
	gui_parallel_pane_take_lead(GPOINTER_TO_INT(data));
}

static gboolean
on_enter_notify_event(GtkWidget *widget,
		      GuiCrossingEvent *event, gpointer user_data)
{
	(void)event;
	(void)user_data;
	gtk_widget_grab_focus(GTK_WIDGET(wk_html_get_view(WK_HTML(widget))));
	return FALSE;
}

/* After a pane is laid out again, it joins the other where the reader is. */
static gboolean
follow_other_pane(gpointer data)
{
	gint pane = GPOINTER_TO_INT(data);

	sync_from(1 - pane);
	return G_SOURCE_REMOVE;
}

static void
render_pane(gint pane)
{
	const char *module = gui_parallel_pane_module(pane);

	mapped_from[0] = mapped_from[1] = 0;
	main_parallel_render_pane(pane_html[pane], module);
	layouts++;
	/* a new document: no space added yet, and pane 0's map of rows is
	 * for the old pair */
	if (row_pads[pane])
		g_array_set_size(row_pads[pane], 0);
	g_object_set_data(G_OBJECT(pane_html[0]), "elim-row-map", NULL);
	schedule_alignment();
	/* a new document: the band starts on the current verse */
	stop_glide(pane);
	g_free(rendered_module[pane]);
	g_free(rendered_book[pane]);
	rendered_module[pane] = g_strdup(module);
	focus_anchor[pane] = -1;
	set_focus(pane, main_parallel_current_position(module, &rendered_book[pane]));
}

static void
on_pane_combo_changed(GObject *combo, GParamSpec *pspec, gpointer data)
{
	gint pane = GPOINTER_TO_INT(data);
	const char *name = elim_dropdown_get_active_id(GTK_DROP_DOWN(combo));

	(void)pspec;
	if (!name || !pane_is_bible(name) ||
	    !g_strcmp0(name, gui_parallel_pane_module(pane)))
		return;
	set_pane_module(pane, name);
	render_pane(pane);
	g_timeout_add(200, follow_other_pane, GINT_TO_POINTER(pane));
}

static void
on_swap_clicked(GtkButton *button, gpointer data)
{
	gchar *left = g_strdup(gui_parallel_pane_module(0));
	gchar *right = g_strdup(gui_parallel_pane_module(1));

	(void)button;
	(void)data;
	set_pane_module(0, right);
	set_pane_module(1, left);
	g_free(left);
	g_free(right);
	gui_parallel_panes_update(TRUE);
}

/* After a layout, each pane's current verse (its band) goes to the
 * reading line, where scrolling will carry it on from. */
static guint place_timeout;

static gboolean
place_on_reading_line(gpointer data)
{
	gint pane;

	(void)data;
	place_timeout = 0;
	stop_glide(0);
	stop_glide(1);
	for (pane = 0; pane < 1; pane++) {
		GtkTextView *view = pane_html[pane]
			? wk_html_get_view(WK_HTML(pane_html[pane])) : NULL;
		GdkRectangle vis;
		gchar name[16];
		gint top, bottom;

		if (!view || focus_anchor[pane] <= 0)
			continue;
		g_snprintf(name, sizeof name, "%d", focus_anchor[pane]);
		if (!wk_html_anchor_block(WK_HTML(pane_html[pane]), name, &top, &bottom))
			continue;
		gtk_text_view_get_visible_rect(view, &vis);
		syncing = TRUE;
		wk_html_cancel_anchor_jump(WK_HTML(pane_html[pane]));
		gtk_adjustment_set_value(pane_vadj(pane),
					 gtk_adjustment_get_value(pane_vadj(pane)) +
					     top - READING_LINE(vis));
		syncing = FALSE;
		/* the right pane's counterpart comes to the same height */
		sync_from(pane);
	}
	return G_SOURCE_REMOVE;
}

static void
schedule_placement(void)
{
	if (place_timeout)
		g_source_remove(place_timeout);
	/* once the render's own jump to the verse has laid the view out */
	place_timeout = g_timeout_add(120, place_on_reading_line, NULL);
}

/* Where pane's scroll puts its verse `anchor` on the reading line. */
static gboolean
reading_line_value(gint pane, gint anchor, gdouble *value)
{
	GtkTextView *view = wk_html_get_view(WK_HTML(pane_html[pane]));
	GdkRectangle vis;
	gchar name[16];
	gint top, bottom;

	g_snprintf(name, sizeof name, "%d", anchor);
	if (!wk_html_anchor_block(WK_HTML(pane_html[pane]), name, &top, &bottom))
		return FALSE;
	gtk_text_view_get_visible_rect(view, &vis);
	*value = gtk_adjustment_get_value(pane_vadj(pane)) + top - READING_LINE(vis);
	return TRUE;
}

void
gui_parallel_panes_follow(gboolean animate)
{
	gint anchor[N_PANES], pane;
	gdouble value[N_PANES];
	gboolean in_place = page_box && gtk_widget_get_mapped(page_box) &&
			    !panes_dirty;

	for (pane = 0; in_place && pane < N_PANES; pane++) {
		const char *module = gui_parallel_pane_module(pane);
		gchar *book = NULL;

		anchor[pane] = main_parallel_current_position(module, &book);
		in_place = anchor[pane] && book &&
			   !g_strcmp0(book, rendered_book[pane]) &&
			   !g_strcmp0(module, rendered_module[pane]) &&
			   reading_line_value(pane, anchor[pane], &value[pane]);
		g_free(book);
	}
	if (!in_place) {
		gui_parallel_panes_update(FALSE);
		return;
	}
	/* already laid out: the bands move and both panes glide there */
	if (place_timeout) {
		g_source_remove(place_timeout);
		place_timeout = 0;
	}
	leader = -1;
	for (pane = 0; pane < N_PANES; pane++) {
		set_focus(pane, anchor[pane]);
		glide_to(pane, value[pane], animate);
	}
}

void
gui_parallel_panes_update(gboolean force)
{
	gint i;

	if (!page_box)
		return;
	if (!force && !gtk_widget_get_mapped(page_box)) {
		panes_dirty = TRUE;
		return;
	}
	panes_dirty = FALSE;
	leader = -1;
	for (i = 0; i < N_PANES; i++) {
		fill_pane_combo(i);
		render_pane(i);
	}
	schedule_placement();
}

static gboolean
on_page_shown(gpointer data)
{
	(void)data;
	shown_idle = 0;
	if (panes_dirty)
		gui_parallel_panes_update(TRUE);
	return G_SOURCE_REMOVE;
}

void
gui_parallel_page_shown(void)
{
	if (page_box && !shown_idle)
		shown_idle = g_idle_add(on_page_shown, NULL);
}

static GtkWidget *
create_pane(gint pane)
{
	GtkWidget *html = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, PARALLEL_TYPE));
	GtkEventController *controller;
	GtkGesture *click;

	XIPHOS_HTML_SET_SURFACE_NAME(html, "bible-parallel");
	gtk_widget_set_hexpand(html, TRUE);
	gtk_widget_set_vexpand(html, TRUE);
	gtk_widget_show(html);
	g_signal_connect((gpointer)html, "popupmenu_requested",
			 G_CALLBACK(_popupmenu_requested_cb), NULL);
	gui_widget_on_crossing(html, (GuiCrossingFunc)on_enter_notify_event,
			       NULL, NULL);

	/* what makes this pane the leading one; none of them consumes */
	controller = gtk_event_controller_scroll_new(
	    GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
	gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
	g_signal_connect(controller, "scroll", G_CALLBACK(on_pane_scroll),
			 GINT_TO_POINTER(pane));
	gtk_widget_add_controller(html, controller);
	controller = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
	g_signal_connect(controller, "key-pressed", G_CALLBACK(on_pane_key),
			 GINT_TO_POINTER(pane));
	gtk_widget_add_controller(html, controller);
	click = gtk_gesture_click_new();
	gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(click),
						   GTK_PHASE_CAPTURE);
	g_signal_connect(click, "pressed", G_CALLBACK(on_pane_pressed),
			 GINT_TO_POINTER(pane));
	gtk_widget_add_controller(html, GTK_EVENT_CONTROLLER(click));

	g_signal_connect(pane_vadj_of(html), "value-changed",
			 G_CALLBACK(on_pane_scrolled), GINT_TO_POINTER(pane));
	g_signal_connect(pane_vadj_of(html), "changed",
			 G_CALLBACK(on_pane_range_changed), GINT_TO_POINTER(pane));
	wk_html_set_verse_rules(WK_HTML(html), TRUE);
	return html;
}

/******************************************************************************
 * Name
 *   gui_create_parallel_page
 *
 * Synopsis
 *   #include "gui/parallel.h
 *
 *   void gui_create_parallel_page(guint page_num)
 *
 * Description
 *   the docked «Vista paralela» page: a version picker over each of two
 *   synchronized chapter panes.
 *
 * Return value
 *   void
 */

void gui_create_parallel_page(void)
{
	GtkWidget *label, *header, *half, *swap, *panes;
	GtkListItemFactory *factory;
	gint i;

	settings.dockedInt = TRUE;

	page_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_set_homogeneous(GTK_BOX(header), TRUE);
	gtk_widget_add_css_class(header, "elim-toolbar-strip");
	gui_widget_set_margins(header, 4);
	gtk_box_append(GTK_BOX(page_box), header);

	panes = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous(GTK_BOX(panes), TRUE);
	gtk_widget_set_vexpand(panes, TRUE);
	gtk_box_append(GTK_BOX(page_box), panes);

	for (i = 0; i < N_PANES; i++) {
		half = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		gtk_box_append(GTK_BOX(header), half);
		pane_combo[i] = elim_dropdown_new();
		elim_dropdown_enable_search(GTK_DROP_DOWN(pane_combo[i]));
		/* a long title shortens instead of widening its half */
		factory = gtk_signal_list_item_factory_new();
		g_signal_connect(factory, "setup",
				 G_CALLBACK(elim_dropdown_label_setup), NULL);
		g_signal_connect(factory, "bind",
				 G_CALLBACK(elim_dropdown_label_bind), NULL);
		gtk_drop_down_set_factory(GTK_DROP_DOWN(pane_combo[i]), factory);
		g_object_unref(factory);
		/* but the open list shows it whole */
		factory = gtk_signal_list_item_factory_new();
		g_signal_connect(factory, "setup", G_CALLBACK(whole_label_setup), NULL);
		g_signal_connect(factory, "bind",
				 G_CALLBACK(elim_dropdown_label_bind), NULL);
		gtk_drop_down_set_list_factory(GTK_DROP_DOWN(pane_combo[i]), factory);
		g_object_unref(factory);
		gtk_widget_set_hexpand(pane_combo[i], TRUE);
		gtk_widget_set_tooltip_text(pane_combo[i],
					    i == 0 ? _("Versión de la izquierda")
						   : _("Versión de la derecha"));
		gtk_box_append(GTK_BOX(half), pane_combo[i]);
		pane_combo_id[i] = g_signal_connect(pane_combo[i], "notify::selected",
						    G_CALLBACK(on_pane_combo_changed),
						    GINT_TO_POINTER(i));
		if (i == 0) {
			swap = gtk_button_new_with_label("⇄");
			gtk_button_set_has_frame(GTK_BUTTON(swap), FALSE);
			gtk_widget_set_focus_on_click(swap, FALSE);
			gtk_widget_set_tooltip_text(swap, _("Intercambiar las dos versiones"));
			g_signal_connect(swap, "clicked", G_CALLBACK(on_swap_clicked), NULL);
			gtk_box_append(GTK_BOX(half), swap);
		}

		pane_html[i] = create_pane(i);
		if (i > 0)
			gtk_widget_add_css_class(pane_html[i], "elim-parallel-right");
		gtk_box_append(GTK_BOX(panes), pane_html[i]);
	}
	widgets.html_parallel = pane_html[0];

	gtk_widget_show(page_box);
	gtk_notebook_append_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				 page_box, NULL);

	label = gtk_label_new(_("Parallel View"));
	gtk_widget_show(label);
	gtk_notebook_set_tab_label(GTK_NOTEBOOK(widgets.notebook_bible_parallel),
				   page_box, label);
}
