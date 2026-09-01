/*
 * Biblia Elim
 * notas_verso.c - pestaña de notas del versículo enfocado, dentro del
 * panel Comentario/Libro
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/main_window.h"
#include "gui/notas_verso.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/display.hh"
#include "main/interlineal.h"
#include "main/settings.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"

/* Tercera pestaña de widgets.notebook_comm_book, después de Comentario
 * (0) y Libro (1). gui_set_bible_comm_layout() en main_window.c solo
 * distingue 0 de "no-0" para esas dos, así que esta pestaña siempre se
 * selecciona explícitamente después de mostrar el panel -- ver
 * gui_verse_notes_panel_actualizar() más abajo. */
#define NOTAS_TAB_INDEX 2

static GtkTextView *notas_view = NULL;
static GtkWidget *notas_label = NULL;
static gchar *notas_mod = NULL;
static gchar *notas_osis = NULL;

/* Último versículo evaluado, como "módulo osisref". Si hay notas,
 * gui_verse_notes_panel_actualizar() siempre fuerza el panel abierto
 * sin importar si el versículo "cambió" -- así se recupera aunque
 * algo externo (memoria por pestaña en tabbed_browser.c, restaurada
 * después de esta función en el arranque) haya pisado
 * settings.showcomms de por medio. Si NO hay notas, en cambio, solo
 * se cierra en un cambio real de versículo, para no arrancarle al
 * usuario un panel que abrió a mano para escribir una nota nueva. */
static gchar *ultimo_verso = NULL;

static void
notas_guardar(void)
{
	GtkTextBuffer *buf;
	GtkTextIter s, e;
	gchar *text;

	if (!notas_view || !notas_mod || !notas_osis)
		return;
	buf = gtk_text_view_get_buffer(notas_view);
	gtk_text_buffer_get_bounds(buf, &s, &e);
	text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
	if (text)
		g_strstrip(text);
	if (text && *text)
		highlight_set_verse_note(notas_mod, notas_osis, text);
	g_free(text);
}

static void
on_notas_guardar_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	notas_guardar();
	if (settings.currentverse) {
		main_bible_note_interlinear_html();
		main_display_bible(NULL, settings.currentverse);
	}
}

GtkWidget *
gui_create_notes_pane(void)
{
	GtkWidget *box, *scroll, *bar, *btn_save;
	GtkTextBuffer *buf;

	UI_VBOX(box, FALSE, 6);
	gtk_widget_set_margin_start(box, 10);
	gtk_widget_set_margin_end(box, 10);
	gtk_widget_set_margin_top(box, 8);
	gtk_widget_set_margin_bottom(box, 8);
	gtk_widget_show(box);

	notas_label = gtk_label_new("");
	gtk_widget_set_halign(notas_label, GTK_ALIGN_START);
	gtk_label_set_ellipsize(GTK_LABEL(notas_label), PANGO_ELLIPSIZE_END);
	gtk_widget_show(notas_label);
	gtk_box_pack_start(GTK_BOX(box), notas_label, FALSE, FALSE, 0);

	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(scroll, TRUE);
	gtk_widget_show(scroll);
	notas_view = GTK_TEXT_VIEW(gtk_text_view_new());
	gtk_text_view_set_wrap_mode(notas_view, GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_left_margin(notas_view, 8);
	gtk_text_view_set_right_margin(notas_view, 8);
	gtk_text_view_set_top_margin(notas_view, 8);
	gtk_text_view_set_bottom_margin(notas_view, 8);
	buf = gtk_text_view_get_buffer(notas_view);
	gtk_text_buffer_set_text(buf, "", 0);
	gtk_widget_show(GTK_WIDGET(notas_view));
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(notas_view));
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

	bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(bar);
	btn_save = gtk_button_new_with_label(_("Guardar"));
	gtk_widget_set_halign(btn_save, GTK_ALIGN_END);
	gtk_widget_show(btn_save);
	g_signal_connect(btn_save, "clicked",
			 G_CALLBACK(on_notas_guardar_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(bar), btn_save, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), bar, FALSE, FALSE, 0);

	return box;
}

/* chapter*1000+verse a partir del versículo/módulo actuales, en el
 * mismo formato que usa highlight_count_notes_at() -- calco de
 * anchor_from_current_verse() en bibletext.c (static allí, no
 * exportada). */
static int
verso_enfocado_chapter_verse(const char *osisref)
{
	gchar *copia, *dot1, *dot2;
	int chapter, verse;

	if (!osisref)
		return 0;
	copia = g_strdup(osisref);
	dot1 = strrchr(copia, '.');
	if (!dot1) {
		g_free(copia);
		return 0;
	}
	verse = atoi(dot1 + 1);
	*dot1 = '\0';
	dot2 = strrchr(copia, '.');
	if (!dot2) {
		g_free(copia);
		return 0;
	}
	chapter = atoi(dot2 + 1);
	g_free(copia);
	return (chapter * 1000) + verse;
}

void
gui_verse_notes_panel_actualizar(void)
{
	const char *osisref;
	gchar *clave, *cita, *titulo;
	gboolean tiene_notas, mismo_verso;
	GtkTextBuffer *buf;
	gchar *existente;

	if (!widgets.notebook_comm_book || !settings.MainWindowModule ||
	    !settings.currentverse)
		return;

	osisref = main_get_osisref_from_key(settings.MainWindowModule,
					    settings.currentverse);
	if (!osisref)
		return;

	clave = g_strdup_printf("%s %s", settings.MainWindowModule, osisref);
	mismo_verso = ultimo_verso && !strcmp(ultimo_verso, clave);
	g_free(ultimo_verso);
	ultimo_verso = clave;

	tiene_notas = highlight_count_notes_at(
			  verso_enfocado_chapter_verse(osisref)) > 0;

	/* No recargar el contenido en un re-render del mismo versículo
	 * (p.ej. cambio de tamaño de letra): pisaría una nota que el
	 * usuario esté escribiendo todavía sin guardar. notas_mod/notas_osis
	 * -el destino real del guardado- tienen que moverse junto con el
	 * título y el contenido, nunca por separado: si no, un redisplay
	 * incidental (p.ej. el bridge de scroll de lectura_sync) mientras el
	 * usuario todavía está escribiendo puede repuntar el guardado a otro
	 * versículo sin que el título en pantalla lo refleje. */
	if (!mismo_verso) {
		g_free(notas_mod);
		g_free(notas_osis);
		notas_mod = g_strdup(settings.MainWindowModule);
		notas_osis = g_strdup(osisref);

		cita = main_interlineal_cita_es(osisref);
		titulo = g_strdup_printf(_("Nota · %s"),
					 (cita && *cita) ? cita : osisref);
		if (notas_label)
			gtk_label_set_text(GTK_LABEL(notas_label), titulo);
		g_free(titulo);
		g_free(cita);

		existente = highlight_get_verse_note(notas_mod, notas_osis);
		buf = notas_view ? gtk_text_view_get_buffer(notas_view) : NULL;
		if (buf)
			gtk_text_buffer_set_text(buf,
						 (existente && *existente &&
						  strcmp(existente, "user content"))
							 ? existente
							 : "",
						 -1);
		g_free(existente);
	}

	/* Visibilidad siempre determinista según si hay notas -- sin
	 * excepción por "mismo versículo", porque esta función es la
	 * única fuente de verdad para settings.showcomms y debe poder
	 * corregir cualquier valor obsoleto que algo externo (memoria por
	 * pestaña en tabbed_browser.c) le haya pisado de por medio. Como
	 * solo se llama al navegar/redisplay-ear el versículo actual, un
	 * panel abierto a mano para escribir una nota nueva sigue a salvo
	 * mientras el usuario no dispare uno de esos eventos. */
	gui_show_hide_comms(tiene_notas);
	if (tiene_notas)
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(widgets.notebook_comm_book), NOTAS_TAB_INDEX);
}
