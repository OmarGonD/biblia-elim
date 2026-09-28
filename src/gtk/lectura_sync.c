/*
 * Biblia Elim
 * lectura_sync.c - pantalla dividida de lectura sincronizada
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <strings.h>

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"
#include <glib/gi18n.h>

#include "gui/bibletext.h"
#include "gui/lectura_sync.h"
#include "gui/main_window.h"
#include "gui/main_menu.h"
#include "gui/instalar_biblias.h"
#include "gui/mod_mgr.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/display.hh"
#include "main/lectura_sync.h"
#include "main/lists.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/xml.h"

#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

#define LSYNC_MAX 4


static gulong combo_changed_id[LSYNC_MAX];
static GtkWidget *combo_slot[LSYNC_MAX];
static GtkWidget *slot_box[LSYNC_MAX];
static GtkWidget *drag_handle[LSYNC_MAX];
static GtkWidget *btn_ord[LSYNC_MAX]; /* ⇄ between slot i and i+1 */
static GtkWidget *btn_add = NULL;
static GtkWidget *btn_install = NULL;
static gboolean paned_positioned = FALSE;
static gboolean ignore_pos = FALSE;
static gboolean ficha_strongs = FALSE;
static gboolean split_forzado = FALSE;
static gchar *last_master = NULL;
static GtkWidget *label_ref = NULL;
static GtkWidget *btn_swap = NULL;
static GtkWidget *bar_comparar = NULL;
static GtkWidget *bar_ficha = NULL;
static GtkWidget *ficha_lab = NULL;
static GtkWidget *html_holder = NULL;
static GtkWidget *nota_box = NULL;
static GtkWidget *nota_hl = NULL;
static GtkTextView *nota_view = NULL;
static gchar *nota_mod = NULL;
static gchar *nota_osis = NULL;
static gboolean ficha_nota = FALSE;

static GtkWidget *
icon_btn(const char *icon, const char *tip)
{
	GtkWidget *b;

	b = gtk_button_new_from_icon_name(icon);
	gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
	gtk_widget_set_tooltip_text(b, tip);
	gtk_widget_set_focus_on_click(b, FALSE);
	gtk_widget_show(b);
	return b;
}

static gchar *
label_corto(const char *desc, const char *name)
{
	if (desc && *desc) {
		glong len = g_utf8_strlen(desc, -1);
		if (len <= 42)
			return g_strdup(desc);
		gchar *cut = g_utf8_substring(desc, 0, 40);
		gchar *out = g_strdup_printf("%s…", cut);
		g_free(cut);
		return out;
	}
	return g_strdup(name);
}

static gint
n_textos(void)
{
	gint n = 0;
	for (GList *l = get_list(TEXT_LIST); l; l = l->next)
		if (l->data)
			n++;
	return n;
}

static gchar **
lsync_split(void)
{
	gchar **raw, **out;
	int i, n = 0;

	if (!settings.LecturaSyncModule || !*settings.LecturaSyncModule)
		return g_new0(gchar *, 1);
	raw = g_strsplit(settings.LecturaSyncModule, ",", LSYNC_MAX);
	out = g_new0(gchar *, LSYNC_MAX + 1);
	for (i = 0; raw && raw[i] && n < LSYNC_MAX; i++) {
		g_strstrip(raw[i]);
		if (raw[i][0])
			out[n++] = g_strdup(raw[i]);
	}
	g_strfreev(raw);
	return out;
}

static void
lsync_save(gchar **names)
{
	GString *s = g_string_new(NULL);
	int i;

	for (i = 0; names && names[i]; i++) {
		if (!names[i][0])
			continue;
		if (s->len)
			g_string_append_c(s, ',');
		g_string_append(s, names[i]);
	}
	xml_set_or_create_value("modules", "lecturasync",
				s->len ? s->str : "");
	settings.LecturaSyncModule = xml_get_value("modules", "lecturasync");
	g_string_free(s, TRUE);
}

static void
lsync_save_from_ui(void)
{
	gchar *names[LSYNC_MAX + 1];
	int i, n = 0;

	memset(names, 0, sizeof(names));
	for (i = 0; i < LSYNC_MAX; i++) {
		const gchar *id;
		int j;
		gboolean dup = FALSE;
		if (!combo_slot[i] || !slot_box[i] ||
		    !gtk_widget_get_visible(slot_box[i]))
			continue;
		id = elim_dropdown_get_active_id(GTK_DROP_DOWN(combo_slot[i]));
		if (!id || !*id)
			continue;
		for (j = 0; j < n; j++) {
			if (!strcmp(names[j], id)) {
				dup = TRUE;
				break;
			}
		}
		if (!dup)
			names[n++] = (gchar *)id;
	}
	lsync_save(n ? names : NULL);
}

static void
fill_one_combo(GtkDropDown *combo, const char *selected,
	       gchar **already, int n_already)
{
	GList *bibles = get_list(TEXT_LIST);
	GList *descs = get_list(TEXT_DESC_LIST);
	int index = 0, active = 0, fallback = -1;
	gboolean found = FALSE;

	elim_dropdown_remove_all(combo);
	for (GList *l = bibles, *d = descs; l; l = l->next, d = d ? d->next : NULL) {
		const char *name = (const char *)l->data;
		const char *desc = d ? (const char *)d->data : NULL;
		gchar *label;
		int j;
		gboolean taken = FALSE;

		if (!name)
			continue;
		for (j = 0; j < n_already; j++) {
			if (already[j] && !strcmp(already[j], name)) {
				taken = TRUE;
				break;
			}
		}
		label = label_corto(desc, name);
		elim_dropdown_append(combo, name, label);
		g_free(label);
		if (selected && !strcmp(name, selected)) {
			active = index;
			found = TRUE;
		}
		if (!taken && fallback < 0)
			fallback = index;
		index++;
	}
	if (index > 0)
		elim_dropdown_set_active(combo,
				 found ? active
				       : (fallback >= 0 ? fallback : 0));
}

static void
lsync_apply_slot_visibility(int nslots)
{
	int i, unused = 0;
	GList *b;

	if (nslots < 1)
		nslots = 1;
	if (nslots > LSYNC_MAX)
		nslots = LSYNC_MAX;
	for (i = 0; i < LSYNC_MAX; i++) {
		if (slot_box[i])
			gtk_widget_set_visible(slot_box[i], i < nslots);
		if (combo_slot[i])
			gtk_widget_set_hexpand(combo_slot[i], nslots == 1);
		if (drag_handle[i])
			gtk_widget_set_visible(drag_handle[i],
					      nslots > 1 && i < nslots);
		/* ⇄ lives between i and i+1: only if both slots are shown. */
		if (btn_ord[i])
			gtk_widget_set_visible(btn_ord[i],
					      nslots > 1 && i < nslots - 1);
	}
	for (b = get_list(TEXT_LIST); b; b = b->next)
		if (b->data)
			unused++;
	if (btn_add)
		gtk_widget_set_visible(btn_add, nslots < LSYNC_MAX && unused > nslots);
}

static void
lectura_sync_fill_combo(void)
{
	gchar **names;
	const gchar *master;
	int i, nslots = 1;

	if (!combo_slot[0])
		return;
	master = settings.MainWindowModule;
	names = lsync_split();
	if (!names[0]) {
		gchar *def = main_lectura_sync_default_module();
		g_strfreev(names);
		names = g_new0(gchar *, 2);
		names[0] = def ? def : g_strdup(master ? master : "");
		lsync_save(names);
	}
	for (i = 0; names[i] && i < LSYNC_MAX; i++) {
		if (combo_changed_id[i])
			g_signal_handler_block(combo_slot[i], combo_changed_id[i]);
		fill_one_combo(GTK_DROP_DOWN(combo_slot[i]), names[i],
			       names, i);
		if (combo_changed_id[i])
			g_signal_handler_unblock(combo_slot[i], combo_changed_id[i]);
		nslots = i + 1;
	}
	lsync_apply_slot_visibility(nslots);
	if (btn_swap)
		gtk_widget_set_sensitive(btn_swap,
					 n_textos() > 1 && names[0] && master &&
					     strcmp(names[0], master) != 0);
	g_strfreev(names);
}

static void
lsync_swap_slots(int a, int b)
{
	gchar **cur;
	gchar *tmp;
	int n = 0;

	if (a == b || a < 0 || b < 0)
		return;
	cur = lsync_split();
	while (cur[n])
		n++;
	if (a >= n || b >= n) {
		g_strfreev(cur);
		return;
	}
	tmp = cur[a];
	cur[a] = cur[b];
	cur[b] = tmp;
	lsync_save(cur);
	g_strfreev(cur);
	lectura_sync_fill_combo();
	ficha_strongs = FALSE;
	main_lectura_sync_actualizar();
}

void
gui_lectura_sync_intercambiar(int a, int b)
{
	lsync_swap_slots(a, b);
}

static void
on_intercambiar(GtkButton *button, gpointer user_data)
{
	int i = GPOINTER_TO_INT(user_data);

	(void)button;
	lsync_swap_slots(i, i + 1);
}

/* Dragging a slot's handle onto another slot swaps the two; the slot
 * number travels as an int, which only this app understands. */
static GdkContentProvider *
on_slot_drag_prepare(GtkDragSource *source, gdouble x, gdouble y,
		     gpointer user_data)
{
	(void)source;
	(void)x;
	(void)y;
	return gdk_content_provider_new_typed(G_TYPE_INT,
					      GPOINTER_TO_INT(user_data));
}

static void
on_slot_drag_begin(GtkDragSource *source, GdkDrag *drag, gpointer user_data)
{
	GtkIconTheme *theme = gtk_icon_theme_get_for_display(
	    gtk_widget_get_display(gtk_event_controller_get_widget(
		GTK_EVENT_CONTROLLER(source))));
	GtkIconPaintable *icon = gtk_icon_theme_lookup_icon(
	    theme, "view-list-symbolic", NULL, 16, 1, GTK_TEXT_DIR_NONE, 0);

	(void)drag;
	(void)user_data;
	gtk_drag_source_set_icon(source, GDK_PAINTABLE(icon), 8, 8);
	g_object_unref(icon);
}

static gboolean
on_slot_drop(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y,
	     gpointer user_data)
{
	(void)target;
	(void)x;
	(void)y;
	if (!G_VALUE_HOLDS_INT(value))
		return FALSE;
	lsync_swap_slots(g_value_get_int(value), GPOINTER_TO_INT(user_data));
	return TRUE;
}

static void
lsync_dnd_setup(GtkWidget *w, int slot)
{
	GtkDropTarget *target = gtk_drop_target_new(G_TYPE_INT, GDK_ACTION_MOVE);

	g_signal_connect(target, "drop", G_CALLBACK(on_slot_drop),
			 GINT_TO_POINTER(slot));
	gtk_widget_add_controller(w, GTK_EVENT_CONTROLLER(target));
}

void
gui_lectura_sync_rellenar_combo(void)
{
	g_free(last_master);
	last_master = g_strdup(settings.MainWindowModule);
	lectura_sync_fill_combo();
}

void
gui_lectura_sync_set_ref(const char *ref)
{
	if (!label_ref)
		return;
	gtk_label_set_text(GTK_LABEL(label_ref), ref ? ref : "");
	gtk_widget_set_tooltip_text(label_ref,
				    _("Mismo versículo que en la pantalla de arriba."));
}

static void
on_combo_lectura_sync_changed(GObject *combo, GParamSpec *pspec,
			      gpointer user_data)
{
	(void)combo;
	(void)pspec;
	(void)user_data;
	lsync_save_from_ui();
	ficha_strongs = FALSE;
	main_lectura_sync_actualizar();
}

static void
on_add_version(GtkButton *button, gpointer user_data)
{
	gchar **cur;
	int n, i;
	const char *master = settings.MainWindowModule;
	GList *b;

	(void)button;
	(void)user_data;
	cur = lsync_split();
	n = 0;
	while (cur[n])
		n++;
	if (n >= LSYNC_MAX) {
		g_strfreev(cur);
		return;
	}
	for (b = get_list(TEXT_LIST); b; b = b->next) {
		const char *nm = (const char *)b->data;
		gboolean used = FALSE;
		if (!nm)
			continue;
		if (master && !strcmp(nm, master))
			continue;
		for (i = 0; i < n; i++) {
			if (!strcmp(cur[i], nm)) {
				used = TRUE;
				break;
			}
		}
		if (!used) {
			gchar **next = g_new0(gchar *, n + 2);
			for (i = 0; i < n; i++)
				next[i] = g_strdup(cur[i]);
			next[n] = g_strdup(nm);
			lsync_save(next);
			g_strfreev(next);
			break;
		}
	}
	g_strfreev(cur);
	lectura_sync_fill_combo();
	main_lectura_sync_actualizar();
}

static void
on_remove_slot(GtkButton *button, gpointer user_data)
{
	int slot = GPOINTER_TO_INT(user_data);
	gchar **cur, *keep[LSYNC_MAX + 1];
	int i, n = 0;

	(void)button;
	cur = lsync_split();
	memset(keep, 0, sizeof(keep));
	for (i = 0; cur[i]; i++) {
		if (i == slot)
			continue;
		keep[n++] = cur[i];
	}
	if (n == 0 && cur[0]) {
		keep[0] = cur[0];
		n = 1;
	}
	lsync_save(n ? keep : NULL);
	/* lsync_save copies strings; cur still owns originals */
	g_strfreev(cur);
	lectura_sync_fill_combo();
	main_lectura_sync_actualizar();
}

static void
on_install_bibles(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	gui_instalar_biblias();
}

static void
on_close_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	gui_lectura_sync_set_visible(FALSE);
}

static void
nota_guardar(void)
{
	GtkTextBuffer *buf;
	GtkTextIter s, e;
	gchar *text;

	if (!nota_view || !nota_mod || !nota_osis)
		return;
	buf = gtk_text_view_get_buffer(nota_view);
	gtk_text_buffer_get_bounds(buf, &s, &e);
	text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
	if (text)
		g_strstrip(text);
	if (text && *text)
		highlight_set_verse_note(nota_mod, nota_osis, text);
	g_free(text);
	if (settings.currentverse) {
		main_bible_note_interlinear_html();
		main_display_bible(NULL, settings.currentverse);
	}
}

static void
nota_ocultar(gboolean guardar)
{
	if (ficha_nota && guardar)
		nota_guardar();
	ficha_nota = FALSE;
	if (nota_box)
		gtk_widget_hide(nota_box);
	if (html_holder)
		gtk_widget_show(html_holder);
}

static void
on_nota_guardar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	nota_guardar();
}

static void
on_ficha_close_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	gui_lectura_sync_ficha_clear();
}

static void
on_hl_note_clicked(GtkButton *button, gpointer user_data)
{
	const char *id = g_object_get_data(G_OBJECT(button), "hl-id");

	(void)user_data;
	if (id)
		gui_open_highlight_note_by_id(id);
}

static void
rellenar_notas_subrayado(const char *osis)
{
	GList *notes, *n;
	int count = 0;

	if (!nota_hl)
		return;
	gui_box_remove_all(nota_hl);
	if (!osis)
		return;
	notes = highlight_list_notes(osis);
	for (n = notes; n; n = n->next) {
		HighlightNote *note = (HighlightNote *)n->data;
		GtkWidget *b;
		gchar *lab;

		if (!note || !note->group_id)
			continue;
		if (note->text && *note->text)
			lab = g_strdup_printf(_("Subrayado: “%s”"), note->text);
		else
			lab = g_strdup(_("Nota de un subrayado"));
		b = gtk_button_new_with_label(lab);
		gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
		gtk_widget_set_halign(b, GTK_ALIGN_START);
		g_object_set_data_full(G_OBJECT(b), "hl-id",
				       g_strdup(note->group_id), g_free);
		g_signal_connect(b, "clicked", G_CALLBACK(on_hl_note_clicked), NULL);
		gtk_widget_show(b);
		gtk_box_append(GTK_BOX(nota_hl), b);
		g_free(lab);
		count++;
	}
	g_list_free_full(notes, (GDestroyNotify)highlight_note_free);
	if (count)
		gtk_widget_show(nota_hl);
	else
		gtk_widget_hide(nota_hl);
}

static void
on_swap_clicked(GtkButton *button, gpointer user_data)
{
	gchar *new_top;
	gchar *new_bot;
	const gchar *verse;

	(void)button;
	(void)user_data;
	gchar **cur, **next;
	int i;

	if (!settings.MainWindowModule || !settings.LecturaSyncModule)
		return;
	cur = lsync_split();
	if (!cur[0] || !strcmp(cur[0], settings.MainWindowModule)) {
		g_strfreev(cur);
		return;
	}
	/* Check before reordering the list: a verse with no counterpart in
	 * the new main Bible leaves both the pane and the list untouched. */
	{
		gchar *mapped = main_reference_for_module(
		    settings.MainWindowModule, settings.currentverse, cur[0]);
		if (!mapped) {
			main_warn_reference_unmapped(settings.currentverse,
						     cur[0]);
			g_strfreev(cur);
			return;
		}
		g_free(mapped);
	}
	new_top = g_strdup(cur[0]);
	next = g_new0(gchar *, LSYNC_MAX + 1);
	next[0] = g_strdup(settings.MainWindowModule);
	for (i = 1; cur[i]; i++)
		next[i] = g_strdup(cur[i]);
	lsync_save(next);
	g_strfreev(next);
	g_strfreev(cur);
	verse = settings.currentverse;
	g_free(last_master);
	last_master = NULL;
	main_display_bible_from_module(settings.MainWindowModule, verse,
				       new_top);
	g_free(new_top);
	lectura_sync_fill_combo();
}

static void
on_paned_position(GObject *obj, GParamSpec *pspec, gpointer user_data)
{
	gint p;
	gchar buf[16];

	(void)pspec;
	(void)user_data;
	if (ignore_pos || (!settings.show_lectura_sync && !ficha_strongs && !split_forzado))
		return;
	p = gtk_paned_get_position(GTK_PANED(obj));
	if (p < 80)
		return;
	settings.lectura_sync_pos = p;
	g_snprintf(buf, sizeof(buf), "%d", p);
	xml_set_or_create_value("layout", "lecturasyncpos", buf);
}

static void
on_paned_lectura_sync_size_allocate(GtkWidget *widget,
				    GdkRectangle *allocation,
				    gpointer user_data)
{
	gint pos;

	(void)user_data;
	if (paned_positioned)
		return;
	if (!settings.show_lectura_sync && !ficha_strongs && !split_forzado)
		return;
	if (!allocation || allocation->height < 120)
		return;

	if (settings.lectura_sync_pos > 80 &&
	    settings.lectura_sync_pos < allocation->height - 80)
		pos = settings.lectura_sync_pos;
	else
		pos = allocation->height * 58 / 100;

	ignore_pos = TRUE;
	gtk_paned_set_position(GTK_PANED(widget), pos);
	ignore_pos = FALSE;
	paned_positioned = TRUE;
	/* Height of the reading pane just changed; put the navigated verse
	 * back at the top now that the split has a real allocation. */
	gui_bibletext_lectura_sync_focus_current();
}

GtkWidget *
gui_lectura_sync_wrap(GtkWidget *html_master)
{
	GtkWidget *paned;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *btn_close;

	g_return_val_if_fail(html_master != NULL, html_master);

	paned = UI_VPANE();
	widgets.paned_lectura_sync = paned;
	gtk_widget_show(paned);
	gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);

	gtk_paned_set_start_child(GTK_PANED(paned), html_master);
	gtk_paned_set_resize_start_child(GTK_PANED(paned), TRUE);
	gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

	UI_VBOX(widgets.box_lectura_sync, FALSE, 0);

	UI_HBOX(hbox, FALSE, 6);
	bar_comparar = hbox;
	gtk_widget_add_css_class(hbox, "elim-toolbar-strip");
	gtk_widget_show(hbox);
	gtk_widget_set_margin_start(hbox, 8);
	gtk_widget_set_margin_end(hbox, 4);
	gtk_widget_set_margin_top(hbox, 4);
	gtk_widget_set_margin_bottom(hbox, 2);
	gtk_box_append(GTK_BOX(widgets.box_lectura_sync), hbox);

	label = gtk_label_new(_("Comparar:"));
	gtk_widget_show(label);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_box_append(GTK_BOX(hbox), label);

	{
		int i;
		for (i = 0; i < LSYNC_MAX; i++) {
			GtkWidget *rm, *grip;
			slot_box[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
			drag_handle[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
			grip = gtk_image_new_from_icon_name("open-menu-symbolic");
			gtk_box_append(GTK_BOX(drag_handle[i]), grip);
			gtk_widget_set_tooltip_text(drag_handle[i],
						    _("Arrastra sobre otra versión para intercambiar el orden"));
			gtk_widget_set_valign(drag_handle[i], GTK_ALIGN_CENTER);
			gtk_widget_add_css_class(drag_handle[i], "elim-drag-handle");
			{
				GtkDragSource *source = gtk_drag_source_new();

				gtk_drag_source_set_actions(source, GDK_ACTION_MOVE);
				g_signal_connect(source, "prepare",
						 G_CALLBACK(on_slot_drag_prepare),
						 GINT_TO_POINTER(i));
				g_signal_connect(source, "drag-begin",
						 G_CALLBACK(on_slot_drag_begin),
						 GINT_TO_POINTER(i));
				gtk_widget_add_controller(drag_handle[i],
							  GTK_EVENT_CONTROLLER(source));
			}
			gtk_widget_set_cursor_from_name(drag_handle[i], "grab");
			gtk_box_append(GTK_BOX(slot_box[i]), drag_handle[i]);

			combo_slot[i] = elim_dropdown_new();
			gtk_widget_set_valign(combo_slot[i], GTK_ALIGN_CENTER);
			gtk_widget_set_hexpand(combo_slot[i], i == 0);
			gui_box_pack(GTK_BOX(slot_box[i]), combo_slot[i], TRUE, TRUE, 0);
			if (i > 0) {
				rm = icon_btn("window-close-symbolic",
					      _("Quitar esta versión"));
				g_signal_connect(rm, "clicked",
						 G_CALLBACK(on_remove_slot),
						 GINT_TO_POINTER(i));
				gtk_box_append(GTK_BOX(slot_box[i]), rm);
			}
			lsync_dnd_setup(drag_handle[i], i);
			lsync_dnd_setup(slot_box[i], i);
			lsync_dnd_setup(combo_slot[i], i);
			gtk_widget_show(slot_box[i]);
			if (i > 0)
				gtk_widget_hide(slot_box[i]);
			gtk_widget_hide(drag_handle[i]);
			gui_box_pack(GTK_BOX(hbox), slot_box[i], TRUE, TRUE, 0);

			if (i < LSYNC_MAX - 1) {
				btn_ord[i] = gtk_button_new_with_label("⇄");
				gtk_button_set_has_frame(GTK_BUTTON(btn_ord[i]), FALSE);
				gtk_widget_set_tooltip_text(btn_ord[i],
							    _("Intercambiar el orden de estas dos versiones"));
				gtk_widget_set_focus_on_click(btn_ord[i], FALSE);
				gtk_widget_add_css_class(btn_ord[i], "elim-swap-ord");
				g_signal_connect(btn_ord[i], "clicked",
						 G_CALLBACK(on_intercambiar),
						 GINT_TO_POINTER(i));
				gtk_box_append(GTK_BOX(hbox), btn_ord[i]);
			}
		}
		widgets.combo_lectura_sync = combo_slot[0];
	}

	btn_add = icon_btn("list-add-symbolic",
			   _("Añadir otra Biblia (hasta 4) para este versículo"));
	gtk_box_append(GTK_BOX(hbox), btn_add);
	g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_version), NULL);

	btn_install = gtk_button_new_with_label(_("Instalar Biblias"));
	gtk_button_set_has_frame(GTK_BUTTON(btn_install), FALSE);
	gtk_widget_set_tooltip_text(btn_install,
				    _("Descargar e instalar Biblias de CrossWire, eBible y otras fuentes"));
	gtk_widget_show(btn_install);
	gtk_box_append(GTK_BOX(hbox), btn_install);
	g_signal_connect(btn_install, "clicked", G_CALLBACK(on_install_bibles), NULL);

	label_ref = gtk_label_new("");
	gtk_widget_show(label_ref);
	gtk_label_set_ellipsize(GTK_LABEL(label_ref), PANGO_ELLIPSIZE_END);
	gtk_widget_set_valign(label_ref, GTK_ALIGN_CENTER);
	gtk_widget_set_opacity(label_ref, 0.7);
	gtk_box_append(GTK_BOX(hbox), label_ref);

	btn_swap = icon_btn("go-up-symbolic",
			    _("Poner esta versión arriba. La de arriba pasa aquí."));
	gtk_box_append(GTK_BOX(hbox), btn_swap);
	g_signal_connect(btn_swap, "clicked", G_CALLBACK(on_swap_clicked), NULL);

	btn_close = icon_btn("window-close-symbolic",
			     _("Cerrar pantalla dividida"));
	gtk_box_append(GTK_BOX(hbox), btn_close);
	g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), NULL);

	UI_HBOX(bar_ficha, FALSE, 6);
	gtk_widget_add_css_class(bar_ficha, "elim-toolbar-strip");
	gtk_widget_set_margin_start(bar_ficha, 8);
	gtk_widget_set_margin_end(bar_ficha, 4);
	gtk_widget_set_margin_top(bar_ficha, 4);
	gtk_widget_set_margin_bottom(bar_ficha, 2);
	gtk_box_append(GTK_BOX(widgets.box_lectura_sync), bar_ficha);
	{
		GtkWidget *btn_f;
		ficha_lab = gtk_label_new(_("Término original"));
		gtk_widget_show(ficha_lab);
		gtk_widget_set_valign(ficha_lab, GTK_ALIGN_CENTER);
		gtk_widget_set_hexpand(ficha_lab, TRUE);
		gtk_label_set_xalign(GTK_LABEL(ficha_lab), 0.0);
		gtk_label_set_ellipsize(GTK_LABEL(ficha_lab), PANGO_ELLIPSIZE_END);
		gui_box_pack(GTK_BOX(bar_ficha), ficha_lab, TRUE, TRUE, 0);
		btn_f = icon_btn("window-close-symbolic",
				 _("Cerrar (Esc)"));
		/* after the label, which takes the rest of the row */
		gtk_box_append(GTK_BOX(bar_ficha), btn_f);
		g_signal_connect(btn_f, "clicked",
				 G_CALLBACK(on_ficha_close_clicked), NULL);
	}

	widgets.html_lectura_sync =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	XIPHOS_HTML_SET_SURFACE_NAME(widgets.html_lectura_sync, "bible-compare");
	gtk_widget_show(widgets.html_lectura_sync);
	html_holder = widgets.html_lectura_sync;
	gui_box_pack(GTK_BOX(widgets.box_lectura_sync), widgets.html_lectura_sync, TRUE, TRUE, 0);

	nota_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(nota_box, 10);
	gtk_widget_set_margin_end(nota_box, 10);
	gtk_widget_set_margin_bottom(nota_box, 8);
	gui_box_pack(GTK_BOX(widgets.box_lectura_sync), nota_box, TRUE, TRUE, 0);
	{
		GtkWidget *scroll, *btn_save, *bar;
		GtkTextBuffer *buf;

		nota_hl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_box_append(GTK_BOX(nota_box), nota_hl);

		scroll = gtk_scrolled_window_new();
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
					       GTK_POLICY_AUTOMATIC,
					       GTK_POLICY_AUTOMATIC);
		gtk_widget_set_vexpand(scroll, TRUE);
		nota_view = GTK_TEXT_VIEW(gtk_text_view_new());
		gtk_text_view_set_wrap_mode(nota_view, GTK_WRAP_WORD_CHAR);
		gtk_text_view_set_left_margin(nota_view, 8);
		gtk_text_view_set_right_margin(nota_view, 8);
		gtk_text_view_set_top_margin(nota_view, 8);
		gtk_text_view_set_bottom_margin(nota_view, 8);
		buf = gtk_text_view_get_buffer(nota_view);
		gtk_text_buffer_set_text(buf, "", 0);
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(nota_view));
		gui_box_pack(GTK_BOX(nota_box), scroll, TRUE, TRUE, 0);

		bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
		btn_save = gtk_button_new_with_label(_("Guardar"));
		gtk_widget_set_halign(btn_save, GTK_ALIGN_END);
		g_signal_connect(btn_save, "clicked", G_CALLBACK(on_nota_guardar), NULL);
		/* at the far end of the bar */
		gtk_widget_set_hexpand(btn_save, TRUE);
		gtk_box_append(GTK_BOX(bar), btn_save);
		gtk_box_append(GTK_BOX(nota_box), bar);
		gtk_widget_show(nota_box);
		gtk_widget_hide(nota_box);
	}

	gtk_paned_set_end_child(GTK_PANED(paned), widgets.box_lectura_sync);
	gtk_paned_set_resize_end_child(GTK_PANED(paned), TRUE);
	gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);

	{
		int i;
		for (i = 0; i < LSYNC_MAX; i++)
			combo_changed_id[i] =
			    g_signal_connect(G_OBJECT(combo_slot[i]), "notify::selected",
					     G_CALLBACK(on_combo_lectura_sync_changed),
					     NULL);
	}
	gui_widget_watch_size(paned, on_paned_lectura_sync_size_allocate, NULL);
	g_signal_connect(G_OBJECT(paned), "notify::position",
			 G_CALLBACK(on_paned_position), NULL);

	lectura_sync_fill_combo();
	gui_lectura_sync_set_visible(settings.show_lectura_sync);
	return paned;
}

void
gui_lectura_sync_escribir(const char *html)
{
	nota_ocultar(TRUE);
	ficha_strongs = TRUE;
	if (ficha_lab)
		gtk_label_set_text(GTK_LABEL(ficha_lab), _("Término original"));
	if (widgets.box_lectura_sync) {
		if (!gtk_widget_get_visible(widgets.box_lectura_sync) &&
		    !settings.show_lectura_sync)
			split_forzado = TRUE;
		gtk_widget_set_visible(widgets.box_lectura_sync, TRUE);
		paned_positioned = FALSE;
	}
	if (bar_comparar)
		gtk_widget_set_visible(bar_comparar, FALSE);
	if (bar_ficha)
		gtk_widget_set_visible(bar_ficha, TRUE);
	if (html_holder)
		gtk_widget_show(html_holder);
	if (widgets.html_lectura_sync) {
		if (!gtk_widget_get_realized(widgets.html_lectura_sync))
			gtk_widget_realize(widgets.html_lectura_sync);
		HtmlOutput((char *)html, widgets.html_lectura_sync, NULL, NULL);
	}
}

void
gui_lectura_sync_ficha_nota(const char *mod, const char *osis, const char *cita)
{
	GtkTextBuffer *buf;
	gchar *existente, *titulo;
	const char *use_osis = osis;

	if (!mod || !osis)
		return;
	nota_ocultar(FALSE);
	g_free(nota_mod);
	g_free(nota_osis);
	nota_mod = g_strdup(mod);
	nota_osis = g_strdup(use_osis);

	ficha_strongs = TRUE;
	ficha_nota = TRUE;
	if (widgets.box_lectura_sync) {
		if (!gtk_widget_get_visible(widgets.box_lectura_sync) &&
		    !settings.show_lectura_sync)
			split_forzado = TRUE;
		gtk_widget_set_visible(widgets.box_lectura_sync, TRUE);
		paned_positioned = FALSE;
	}
	if (bar_comparar)
		gtk_widget_set_visible(bar_comparar, FALSE);
	if (bar_ficha)
		gtk_widget_set_visible(bar_ficha, TRUE);
	titulo = g_strdup_printf(_("Nota · %s"),
				 (cita && *cita) ? cita : osis);
	if (ficha_lab)
		gtk_label_set_text(GTK_LABEL(ficha_lab), titulo);
	g_free(titulo);

	if (html_holder)
		gtk_widget_hide(html_holder);
	if (nota_box)
		gtk_widget_show(nota_box);

	buf = nota_view ? gtk_text_view_get_buffer(nota_view) : NULL;
	existente = highlight_get_verse_note(mod, osis);
	if (buf)
		gtk_text_buffer_set_text(buf,
					 (existente && *existente &&
					  strcmp(existente, "user content"))
						 ? existente
						 : "",
					 -1);
	g_free(existente);
	rellenar_notas_subrayado(osis);
	if (nota_view)
		gtk_widget_grab_focus(GTK_WIDGET(nota_view));
}

void
gui_lectura_sync_ficha_clear(void)
{
	nota_ocultar(TRUE);
	ficha_strongs = FALSE;
	if (bar_ficha)
		gtk_widget_set_visible(bar_ficha, FALSE);
	if (bar_comparar)
		gtk_widget_set_visible(bar_comparar, settings.show_lectura_sync != 0);
	if (html_holder)
		gtk_widget_show(html_holder);
	if (widgets.box_lectura_sync &&
	    ((split_forzado && !settings.show_lectura_sync) ||
	     settings.reading_mode))
		gtk_widget_set_visible(widgets.box_lectura_sync, FALSE);
	split_forzado = FALSE;
}

gboolean
gui_lectura_sync_ficha_activa(void)
{
	return ficha_strongs;
}

void
gui_lectura_sync_set_visible(gboolean visible)
{
	/* Re-entrancy guard: syncing the menu checkbox or the "Comparar"
	 * button below re-emits their own change signal regardless of
	 * what triggered it, which would otherwise run this whole function
	 * a second time on top of itself. */
	static gboolean in_progress = FALSE;
	if (in_progress)
		return;
	in_progress = TRUE;

	nota_ocultar(TRUE);
	ficha_strongs = FALSE;
	split_forzado = FALSE;
	settings.show_lectura_sync = visible ? 1 : 0;
	xml_set_or_create_value("misc", "show_lectura_sync",
				settings.show_lectura_sync ? "1" : "0");

	if (!widgets.box_lectura_sync) {
		in_progress = FALSE;
		return;
	}

	gtk_widget_set_visible(widgets.box_lectura_sync, visible);
	if (bar_ficha)
		gtk_widget_set_visible(bar_ficha, FALSE);
	if (bar_comparar)
		gtk_widget_set_visible(bar_comparar, visible);
	if (nota_box)
		gtk_widget_hide(nota_box);
	if (html_holder)
		gtk_widget_set_visible(html_holder, visible);
	if (visible) {
		if (widgets.html_lectura_sync &&
		    gtk_widget_get_realized(gui_widget_get_toplevel(widgets.html_lectura_sync)) &&
		    !gtk_widget_get_realized(widgets.html_lectura_sync))
			gtk_widget_realize(widgets.html_lectura_sync);
		paned_positioned = FALSE;
		g_free(last_master);
		last_master = NULL;
		lectura_sync_fill_combo();
		/* renders the panel for settings.currentverse *and* (see
		 * main_lectura_sync_actualizar() in lectura_sync.cc) focuses
		 * that same verse up in the main pane -- so opening starts
		 * on the verse the user actually navigated to, not wherever
		 * a fresh chapter's natural scroll position happens to land.
		 * Scrolling from there on is what hands off to the reading
		 * focus tracking in bibletext.c (reading_focus_update()). */
		main_lectura_sync_actualizar();
	} else if (gui_main_window_ready()) {
		gui_bibletext_lectura_sync_clear_focus();
	}

	/* keep every entry point (menu checkbox, "Comparar" button, the
	 * panel's own close button) showing the same state. */
	gui_main_menu_set_state("split", visible);
	if (widgets.lectura_sync_button &&
	    gui_toggle_get_active(GTK_WIDGET(widgets.lectura_sync_button)) != visible)
		gui_toggle_set_active(GTK_WIDGET(widgets.lectura_sync_button), visible);

	in_progress = FALSE;
}

void
gui_lectura_sync_actualizar(void)
{
	main_lectura_sync_actualizar();
}

G_MODULE_EXPORT void
on_lectura_sync_activate(gpointer menuitem, gpointer user_data)
{
	(void)menuitem;
	gui_lectura_sync_set_visible(GPOINTER_TO_INT(user_data) != 0);
}
