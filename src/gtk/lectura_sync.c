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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/bibletext.h"
#include "gui/lectura_sync.h"
#include "gui/main_window.h"
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

static gulong combo_changed_id = 0;
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

#if GTK_CHECK_VERSION(3, 10, 0)
	b = gtk_button_new_from_icon_name(icon, GTK_ICON_SIZE_MENU);
#else
	b = gtk_button_new();
#endif
	gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(b, tip);
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(b, FALSE);
#endif
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

static void
lectura_sync_fill_combo(void)
{
	GtkComboBoxText *combo;
	GList *bibles;
	GList *descs;
	const gchar *saved;
	const gchar *master;
	gint index = 0;
	gint active = 0;
	gboolean known = FALSE;
	gboolean skip_master;
	gint n;

	if (!widgets.combo_lectura_sync)
		return;

	combo = GTK_COMBO_BOX_TEXT(widgets.combo_lectura_sync);
	if (combo_changed_id)
		g_signal_handler_block(combo, combo_changed_id);

	gtk_combo_box_text_remove_all(combo);

	n = n_textos();
	master = settings.MainWindowModule;
	skip_master = (n > 1 && master && *master);

	bibles = get_list(TEXT_LIST);
	descs = get_list(TEXT_DESC_LIST);

	if (settings.LecturaSyncModule && master &&
	    !strcmp(settings.LecturaSyncModule, master) && skip_master)
		settings.LecturaSyncModule = NULL;

	for (GList *l = bibles; l; l = l->next) {
		const char *nm = (const char *)l->data;
		if (!nm)
			continue;
		if (skip_master && !strcmp(nm, master))
			continue;
		if (settings.LecturaSyncModule &&
		    !strcmp(nm, settings.LecturaSyncModule)) {
			known = TRUE;
			break;
		}
	}
	if (!settings.LecturaSyncModule || !known) {
		gchar *def = main_lectura_sync_default_module();
		if (def) {
			xml_set_or_create_value("modules", "lecturasync", def);
			settings.LecturaSyncModule =
			    xml_get_value("modules", "lecturasync");
			g_free(def);
		}
	}

	saved = settings.LecturaSyncModule;
	for (GList *l = bibles, *d = descs; l; l = l->next, d = d ? d->next : NULL) {
		const char *name = (const char *)l->data;
		const char *desc = d ? (const char *)d->data : NULL;
		gchar *label;

		if (!name)
			continue;
		if (skip_master && !strcmp(name, master))
			continue;
		label = label_corto(desc, name);
		gtk_combo_box_text_append(combo, name, label);
		g_free(label);
		if (saved && !strcmp(name, saved))
			active = index;
		index++;
	}

	if (index > 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);

	if (saved) {
		gchar *tip = g_strdup_printf(
		    _("Comparar con %s. Se sincroniza al versículo de arriba."),
		    saved);
		gtk_widget_set_tooltip_text(widgets.combo_lectura_sync, tip);
		g_free(tip);
	}

	if (btn_swap)
		gtk_widget_set_sensitive(btn_swap,
					 n > 1 && saved && master &&
					     strcmp(saved, master) != 0);

	if (combo_changed_id)
		g_signal_handler_unblock(combo, combo_changed_id);
}

void
gui_lectura_sync_rellenar_combo(void)
{
	if (last_master && settings.MainWindowModule &&
	    !strcmp(last_master, settings.MainWindowModule) &&
	    widgets.combo_lectura_sync &&
	    gtk_combo_box_get_active_id(GTK_COMBO_BOX(widgets.combo_lectura_sync)))
		return;
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
on_combo_lectura_sync_changed(GtkComboBox *combo, gpointer user_data)
{
	const gchar *id = gtk_combo_box_get_active_id(combo);

	(void)user_data;
	if (!id || !*id)
		return;
	xml_set_or_create_value("modules", "lecturasync", id);
	settings.LecturaSyncModule = xml_get_value("modules", "lecturasync");
	if (btn_swap && settings.MainWindowModule)
		gtk_widget_set_sensitive(btn_swap,
					 strcmp(id, settings.MainWindowModule) != 0);
	ficha_strongs = FALSE;
	main_lectura_sync_actualizar();
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
	gtk_container_foreach(GTK_CONTAINER(nota_hl),
			      (GtkCallback)gtk_widget_destroy, NULL);
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
		gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
		gtk_widget_set_halign(b, GTK_ALIGN_START);
		g_object_set_data_full(G_OBJECT(b), "hl-id",
				       g_strdup(note->group_id), g_free);
		g_signal_connect(b, "clicked", G_CALLBACK(on_hl_note_clicked), NULL);
		gtk_widget_show(b);
		gtk_box_pack_start(GTK_BOX(nota_hl), b, FALSE, FALSE, 0);
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
	if (!settings.MainWindowModule || !settings.LecturaSyncModule)
		return;
	if (!strcmp(settings.MainWindowModule, settings.LecturaSyncModule))
		return;

	new_top = g_strdup(settings.LecturaSyncModule);
	new_bot = g_strdup(settings.MainWindowModule);
	verse = settings.currentverse;
	xml_set_or_create_value("modules", "lecturasync", new_bot);
	settings.LecturaSyncModule = xml_get_value("modules", "lecturasync");
	g_free(last_master);
	last_master = NULL;
	main_display_bible(new_top, verse);
	g_free(new_top);
	g_free(new_bot);
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
#ifndef USE_WEBKIT2
	GtkWidget *scrolled;
#endif

	g_return_val_if_fail(html_master != NULL, html_master);

	paned = UI_VPANE();
	widgets.paned_lectura_sync = paned;
	gtk_widget_show(paned);
#if GTK_CHECK_VERSION(3, 16, 0)
	gtk_paned_set_wide_handle(GTK_PANED(paned), TRUE);
#endif

	gtk_paned_pack1(GTK_PANED(paned), html_master, TRUE, TRUE);

	UI_VBOX(widgets.box_lectura_sync, FALSE, 0);

	UI_HBOX(hbox, FALSE, 6);
	bar_comparar = hbox;
	gtk_style_context_add_class(gtk_widget_get_style_context(hbox),
				    "elim-toolbar-strip");
	gtk_widget_show(hbox);
	gtk_widget_set_margin_start(hbox, 8);
	gtk_widget_set_margin_end(hbox, 4);
	gtk_widget_set_margin_top(hbox, 4);
	gtk_widget_set_margin_bottom(hbox, 2);
	gtk_box_pack_start(GTK_BOX(widgets.box_lectura_sync), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Comparar:"));
	gtk_widget_show(label);
	gtk_widget_set_valign(label, GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	widgets.combo_lectura_sync = gtk_combo_box_text_new();
	gtk_widget_show(widgets.combo_lectura_sync);
	gtk_widget_set_hexpand(widgets.combo_lectura_sync, TRUE);
	gtk_widget_set_valign(widgets.combo_lectura_sync, GTK_ALIGN_CENTER);
	gtk_box_pack_start(GTK_BOX(hbox), widgets.combo_lectura_sync, TRUE, TRUE, 0);

	label_ref = gtk_label_new("");
	gtk_widget_show(label_ref);
	gtk_label_set_ellipsize(GTK_LABEL(label_ref), PANGO_ELLIPSIZE_END);
	gtk_widget_set_valign(label_ref, GTK_ALIGN_CENTER);
	gtk_widget_set_opacity(label_ref, 0.7);
	gtk_box_pack_start(GTK_BOX(hbox), label_ref, FALSE, FALSE, 0);

	btn_swap = icon_btn("go-up-symbolic",
			    _("Poner esta versión arriba. La de arriba pasa aquí."));
	gtk_box_pack_start(GTK_BOX(hbox), btn_swap, FALSE, FALSE, 0);
	g_signal_connect(btn_swap, "clicked", G_CALLBACK(on_swap_clicked), NULL);

	btn_close = icon_btn("window-close-symbolic",
			     _("Cerrar pantalla dividida"));
	gtk_box_pack_start(GTK_BOX(hbox), btn_close, FALSE, FALSE, 0);
	g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close_clicked), NULL);

	UI_HBOX(bar_ficha, FALSE, 6);
	gtk_style_context_add_class(gtk_widget_get_style_context(bar_ficha),
				    "elim-toolbar-strip");
	gtk_widget_set_margin_start(bar_ficha, 8);
	gtk_widget_set_margin_end(bar_ficha, 4);
	gtk_widget_set_margin_top(bar_ficha, 4);
	gtk_widget_set_margin_bottom(bar_ficha, 2);
	gtk_box_pack_start(GTK_BOX(widgets.box_lectura_sync), bar_ficha,
			   FALSE, FALSE, 0);
	{
		GtkWidget *btn_f;
		ficha_lab = gtk_label_new(_("Término original"));
		gtk_widget_show(ficha_lab);
		gtk_widget_set_valign(ficha_lab, GTK_ALIGN_CENTER);
		gtk_widget_set_hexpand(ficha_lab, TRUE);
		gtk_label_set_xalign(GTK_LABEL(ficha_lab), 0.0);
		gtk_label_set_ellipsize(GTK_LABEL(ficha_lab), PANGO_ELLIPSIZE_END);
		gtk_box_pack_start(GTK_BOX(bar_ficha), ficha_lab, TRUE, TRUE, 0);
		btn_f = icon_btn("window-close-symbolic",
				 _("Cerrar (Esc)"));
		gtk_box_pack_end(GTK_BOX(bar_ficha), btn_f, FALSE, FALSE, 0);
		g_signal_connect(btn_f, "clicked",
				 G_CALLBACK(on_ficha_close_clicked), NULL);
	}

	widgets.html_lectura_sync =
	    GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	gtk_widget_show(widgets.html_lectura_sync);
#ifdef USE_WEBKIT2
	html_holder = widgets.html_lectura_sync;
	gtk_box_pack_start(GTK_BOX(widgets.box_lectura_sync),
			   widgets.html_lectura_sync, TRUE, TRUE, 0);
#else
	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolled);
	gtk_container_add(GTK_CONTAINER(scrolled), widgets.html_lectura_sync);
	html_holder = scrolled;
	gtk_box_pack_start(GTK_BOX(widgets.box_lectura_sync), scrolled, TRUE, TRUE, 0);
#endif

	nota_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_start(nota_box, 10);
	gtk_widget_set_margin_end(nota_box, 10);
	gtk_widget_set_margin_bottom(nota_box, 8);
	gtk_box_pack_start(GTK_BOX(widgets.box_lectura_sync), nota_box, TRUE, TRUE, 0);
	{
		GtkWidget *scroll, *btn_save, *bar;
		GtkTextBuffer *buf;

		nota_hl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_box_pack_start(GTK_BOX(nota_box), nota_hl, FALSE, FALSE, 0);

		scroll = gtk_scrolled_window_new(NULL, NULL);
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
		gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(nota_view));
		gtk_box_pack_start(GTK_BOX(nota_box), scroll, TRUE, TRUE, 0);

		bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
		btn_save = gtk_button_new_with_label(_("Guardar"));
		gtk_widget_set_halign(btn_save, GTK_ALIGN_END);
		g_signal_connect(btn_save, "clicked", G_CALLBACK(on_nota_guardar), NULL);
		gtk_box_pack_end(GTK_BOX(bar), btn_save, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(nota_box), bar, FALSE, FALSE, 0);
		gtk_widget_show_all(nota_box);
		gtk_widget_hide(nota_box);
	}

	gtk_paned_pack2(GTK_PANED(paned), widgets.box_lectura_sync, TRUE, TRUE);

	combo_changed_id =
	    g_signal_connect(G_OBJECT(widgets.combo_lectura_sync), "changed",
			     G_CALLBACK(on_combo_lectura_sync_changed), NULL);
	g_signal_connect(G_OBJECT(paned), "size-allocate",
			 G_CALLBACK(on_paned_lectura_sync_size_allocate), NULL);
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
		gtk_widget_show_all(nota_box);

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
	if (visible) {
		if (widgets.html_lectura_sync &&
		    gtk_widget_get_window(gtk_widget_get_toplevel(widgets.html_lectura_sync)) &&
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
		 * Scrolling from there on is what hands off to
		 * gui_bibletext_lectura_sync_focus_refresh(). */
		main_lectura_sync_actualizar();
	} else if (gui_main_window_ready()) {
		gui_bibletext_lectura_sync_clear_focus();
	}

	/* keep every entry point (menu checkbox, "Comparar" button, the
	 * panel's own close button) showing the same state. */
	if (widgets.lectura_sync_item &&
	    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widgets.lectura_sync_item)) != visible)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.lectura_sync_item), visible);
	if (widgets.lectura_sync_button &&
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widgets.lectura_sync_button)) != visible)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.lectura_sync_button), visible);

	in_progress = FALSE;
}

void
gui_lectura_sync_actualizar(void)
{
	main_lectura_sync_actualizar();
}

G_MODULE_EXPORT void
on_lectura_sync_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	(void)user_data;
	gui_lectura_sync_set_visible(gtk_check_menu_item_get_active(menuitem));
}
