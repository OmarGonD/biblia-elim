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
#include <gdk/gdkkeysyms.h>
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

/* Segunda pestaña de widgets.notebook_comm_book, después de Comentario
 * (0). gui_set_bible_comm_layout() en main_window.c solo distingue 0 de
 * "no-0", así que esta pestaña siempre se
 * selecciona explícitamente después de mostrar el panel -- ver
 * gui_verse_notes_panel_actualizar() más abajo. */
#define NOTAS_TAB_INDEX 1

/* Lo que se tarda en dejar de escribir antes de guardar solo. Bastante
 * corto para que una nota nunca se pierda por cerrar la ventana de
 * golpe, bastante largo para no guardar en cada tecla. */
#define AUTOGUARDADO_MS 1200

static GtkTextView *notas_view = NULL;
static GtkWidget *notas_label = NULL;
static GtkWidget *notas_estado = NULL;
static GtkWidget *notas_boton = NULL;
static GtkWidget *notas_lista = NULL;
static GtkWidget *notas_nueva = NULL;
static gchar *notas_mod = NULL;
static gchar *notas_osis = NULL;

/* El texto tal como se cargó. Sirve para dos cosas: saber si hay algo
 * que guardar -- guardar sin cambios reescribe el XML y el archivo
 * entero para nada -- y saber si la nota existía, que es lo único que
 * decide si hace falta redibujar el capítulo. */
static gchar *notas_original = NULL;
static guint notas_temporizador = 0;
/* gtk_text_buffer_set_text() al cargar una nota emite "changed" igual
 * que teclear; esto distingue una cosa de la otra. */
static gboolean notas_cargando = FALSE;
/* Al borrar una nota desde aquí, el redibujo llama de vuelta a
 * gui_verse_notes_panel_actualizar(), que cerraría el panel en las
 * narices de quien acaba de vaciarlo para escribir otra cosa. */
static gboolean notas_borrando = FALSE;

/* Último versículo evaluado, como "módulo osisref". Si hay notas,
 * gui_verse_notes_panel_actualizar() siempre fuerza el panel abierto
 * sin importar si el versículo "cambió" -- así se recupera aunque
 * algo externo (memoria por pestaña en tabbed_browser.c, restaurada
 * después de esta función en el arranque) haya pisado
 * settings.showcomms de por medio. Si NO hay notas, en cambio, solo
 * se cierra en un cambio real de versículo, para no arrancarle al
 * usuario un panel que abrió a mano para escribir una nota nueva. */
static gchar *ultimo_verso = NULL;
/* Guardar puede redibujar el capítulo, y el redibujo vuelve a llamar a
 * gui_verse_notes_panel_actualizar() desde main_display_bible(). Sin
 * esto, un cambio de versículo con nota nueva hacía el trabajo dos
 * veces. */
static gboolean notas_en_actualizar = FALSE;
/* El usuario cerró el panel manualmente con el botón ✕ o Escape.
 * Mientras siga en el mismo versículo, no reabrirlo aunque tenga notas.
 * Se limpia al cambiar de versículo. */
static gboolean notas_cerrado_manual = FALSE;

typedef struct {
	gchar *key;
	gchar *text;
} NotaFila;

static void nota_fila_free(gpointer data)
{
	NotaFila *fila = data;
	if (fila) { g_free(fila->key); g_free(fila->text); g_free(fila); }
}

static gboolean notas_dialogo_texto(const gchar *titulo, const gchar *inicial,
					gchar **salida)
{
	GtkWidget *dialog, *scroll, *view;
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gint response;
	dialog = gtk_dialog_new_with_buttons(titulo, NULL, GTK_DIALOG_MODAL,
					     _("Cancelar"), GTK_RESPONSE_CANCEL,
					     _("Guardar"), GTK_RESPONSE_OK, NULL);
	scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(scroll, 420, 180);
	view = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(view), GTK_WRAP_WORD_CHAR);
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
	gtk_text_buffer_set_text(buffer, inicial ? inicial : "", -1);
	gtk_container_add(GTK_CONTAINER(scroll), view);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
			   scroll, TRUE, TRUE, 8);
	gtk_widget_show_all(dialog);
	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_OK) {
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		*salida = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	}
	gtk_widget_destroy(dialog);
	return response == GTK_RESPONSE_OK;
}

static void notas_lista_reconstruir(void);

static void on_nota_fila_editar(GtkButton *button, gpointer data)
{
	NotaFila *fila = g_object_get_data(G_OBJECT(button), "nota-fila");
	gchar *texto = NULL;
	if (!fila || !notas_mod || !fila->key)
		return;
	if (notas_dialogo_texto(_("Editar nota"), fila->text, &texto)) {
		if (g_str_has_prefix(fila->key, "MV:"))
			highlight_set_verse_note_by_key(fila->key, texto);
		else if (g_str_has_prefix(fila->key, "HL:"))
			highlight_set_note(fila->key + 3, texto);
		main_display_bible(NULL, settings.currentverse);
		notas_lista_reconstruir();
	}
	g_free(texto);
}

static void on_nota_fila_borrar(GtkButton *button, gpointer data)
{
	NotaFila *fila = g_object_get_data(G_OBJECT(button), "nota-fila");
	if (!fila || !fila->key)
		return;
	if (g_str_has_prefix(fila->key, "MV:"))
		highlight_remove_verse_note_by_key(fila->key);
	else if (g_str_has_prefix(fila->key, "HL:"))
		highlight_remove(fila->key + 3);
	main_display_bible(NULL, settings.currentverse);
	notas_lista_reconstruir();
}

static void on_nota_nueva(GtkButton *button, gpointer data)
{
	gchar *texto = NULL;
	(void)button; (void)data;
	if (notas_dialogo_texto(_("Nueva nota"), NULL, &texto) && texto && *texto) {
		highlight_add_verse_note(notas_mod, notas_osis, texto);
		main_display_bible(NULL, settings.currentverse);
		notas_lista_reconstruir();
	}
	g_free(texto);
}

static void notas_lista_reconstruir(void)
{
	GList *items, *it;
	if (!notas_lista || !notas_osis)
		return;
	gtk_container_foreach(GTK_CONTAINER(notas_lista),
				      (GtkCallback)gtk_widget_destroy, NULL);
	items = highlight_list_notes(notas_osis);
	for (it = items; it; it = it->next) {
		HighlightNote *note = it->data;
		GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
		GtkWidget *text = gtk_label_new(note->note ? note->note : "");
		GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
		GtkWidget *edit = gtk_button_new_with_label(_("Editar"));
		GtkWidget *del = gtk_button_new_with_label(_("Eliminar"));
		NotaFila *fila = g_new0(NotaFila, 1);
		fila->key = g_strdup(note->note_key);
		fila->text = g_strdup(note->note);
		gtk_label_set_line_wrap(GTK_LABEL(text), TRUE);
		gtk_widget_set_halign(text, GTK_ALIGN_START);
		gtk_box_pack_start(GTK_BOX(row), text, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(actions), edit, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(actions), del, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(row), actions, FALSE, FALSE, 0);
		g_object_set_data_full(G_OBJECT(edit), "nota-fila", fila,
				       nota_fila_free);
		g_object_set_data(G_OBJECT(del), "nota-fila", fila);
		g_signal_connect(edit, "clicked", G_CALLBACK(on_nota_fila_editar), NULL);
		g_signal_connect(del, "clicked", G_CALLBACK(on_nota_fila_borrar), NULL);
		gtk_box_pack_start(GTK_BOX(notas_lista), row, FALSE, FALSE, 6);
	}
	g_list_free_full(items, (GDestroyNotify)highlight_note_free);
	gtk_widget_show_all(notas_lista);
}

static gchar *
notas_texto_actual(void)
{
	GtkTextBuffer *buf;
	GtkTextIter s, e;
	gchar *text;

	if (!notas_view)
		return g_strdup("");
	buf = gtk_text_view_get_buffer(notas_view);
	gtk_text_buffer_get_bounds(buf, &s, &e);
	text = gtk_text_buffer_get_text(buf, &s, &e, FALSE);
	if (!text)
		return g_strdup("");
	g_strstrip(text);
	return text;
}

static void
notas_estado_poner(const gchar *texto, gboolean pendiente)
{
	if (notas_estado)
		gtk_label_set_text(GTK_LABEL(notas_estado), texto ? texto : "");
	if (notas_boton)
		gtk_widget_set_sensitive(notas_boton, pendiente);
}

static void
notas_cancelar_temporizador(void)
{
	if (notas_temporizador) {
		g_source_remove(notas_temporizador);
		notas_temporizador = 0;
	}
}

/* Guarda si hay algo distinto de lo cargado. Es idempotente a
 * propósito: la llaman el temporizador, el botón, la salida del foco,
 * el cambio de versículo y el cierre de la aplicación. */
static void
notas_guardar(void)
{
	gchar *texto;
	gboolean habia, hay;

	notas_cancelar_temporizador();
	if (!notas_view || !notas_mod || !notas_osis)
		return;

	texto = notas_texto_actual();
	if (!g_strcmp0(texto, notas_original ? notas_original : "")) {
		g_free(texto);
		return;
	}

	habia = notas_original && *notas_original;
	hay = *texto != '\0';

	if (hay)
		highlight_set_verse_note(notas_mod, notas_osis, texto);
	else
		note_remove_whole_verse(notas_mod, notas_osis);

	g_free(notas_original);
	notas_original = texto;

	/* El texto de la nota no aparece en el capítulo: allí lo único que
	 * cambia es el resaltado del versículo, y ese sólo aparece o
	 * desaparece cuando la nota se crea o se borra. Redibujar en cada
	 * guardado costaba entre 15 y 25 ms -- frente a los 0,8 ms de
	 * escribir la nota -- y daba un parpadeo en mitad de la escritura.
	 * Ahora sólo se redibuja cuando el capítulo tiene de verdad algo
	 * distinto que enseñar. */
	if (habia != hay && settings.currentverse) {
		notas_borrando = !hay;
		main_bible_note_interlinear_html();
		main_display_bible(NULL, settings.currentverse);
		notas_borrando = FALSE;
	}

	notas_estado_poner(_("Guardada"), FALSE);
}

void
gui_verse_notes_guardar_pendiente(void)
{
	notas_guardar();
}

static gboolean
notas_autoguardar(gpointer datos)
{
	(void)datos;
	notas_temporizador = 0;
	notas_guardar();
	return G_SOURCE_REMOVE;
}

static void
on_notas_buffer_changed(GtkTextBuffer *buf, gpointer datos)
{
	(void)buf;
	(void)datos;
	if (notas_cargando)
		return;
	notas_estado_poner(_("Sin guardar…"), TRUE);
	notas_cancelar_temporizador();
	notas_temporizador = g_timeout_add(AUTOGUARDADO_MS, notas_autoguardar,
					   NULL);
}

static gboolean
on_notas_foco_fuera(GtkWidget *w, GdkEvent *ev, gpointer datos)
{
	(void)w;
	(void)ev;
	(void)datos;
	notas_guardar();
	return FALSE;
}

static gboolean
on_notas_tecla(GtkWidget *w, GdkEventKey *ev, gpointer datos)
{
	(void)w;
	(void)datos;
	if ((ev->state & GDK_CONTROL_MASK) &&
	    (ev->keyval == GDK_KEY_s || ev->keyval == GDK_KEY_S ||
	     ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter)) {
		notas_guardar();
		return TRUE;
	}
	if (ev->keyval == GDK_KEY_Escape) {
		notas_cerrado_manual = TRUE;
		gui_close_comms_panel();
		return TRUE;
	}
	return FALSE;
}

static void
on_notas_guardar_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	notas_guardar();
}

static void
on_notas_cerrar_clicked(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	notas_cerrado_manual = TRUE;
	gui_close_comms_panel();
}

GtkWidget *
gui_create_notes_pane(void)
{
	GtkWidget *box, *scroll, *bar, *header, *cerrar;
	GtkTextBuffer *buf;

	UI_VBOX(box, FALSE, 6);
	gtk_widget_set_margin_start(box, 10);
	gtk_widget_set_margin_end(box, 10);
	gtk_widget_set_margin_top(box, 8);
	gtk_widget_set_margin_bottom(box, 8);
	gtk_widget_show(box);

	/* Cabecera: título a la izquierda, botón ✕ a la derecha. */
	header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show(header);

	notas_label = gtk_label_new("");
	gtk_widget_set_halign(notas_label, GTK_ALIGN_START);
	gtk_label_set_ellipsize(GTK_LABEL(notas_label), PANGO_ELLIPSIZE_END);
	gtk_widget_show(notas_label);
	gtk_box_pack_start(GTK_BOX(header), notas_label, TRUE, TRUE, 0);
	notas_nueva = gtk_button_new_with_label(_("+ Nueva nota"));
	gtk_button_set_relief(GTK_BUTTON(notas_nueva), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(notas_nueva,
				    _("Añadir otra nota a este versículo"));
	gtk_widget_show(notas_nueva);
	g_signal_connect(notas_nueva, "clicked", G_CALLBACK(on_nota_nueva), NULL);
	gtk_box_pack_end(GTK_BOX(header), notas_nueva, FALSE, FALSE, 4);

	cerrar = gtk_button_new_from_icon_name("window-close-symbolic",
					       GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_relief(GTK_BUTTON(cerrar), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text(cerrar, _("Cerrar panel de notas"));
	gtk_widget_set_focus_on_click(cerrar, FALSE);
	gtk_widget_show(cerrar);
	g_signal_connect(cerrar, "clicked",
			 G_CALLBACK(on_notas_cerrar_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(header), cerrar, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(box), header, FALSE, FALSE, 0);

	notas_lista = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_margin_start(notas_lista, 4);
	gtk_widget_set_margin_end(notas_lista, 4);
	gtk_widget_show(notas_lista);
	gtk_box_pack_start(GTK_BOX(box), notas_lista, FALSE, FALSE, 0);


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
	g_signal_connect(buf, "changed", G_CALLBACK(on_notas_buffer_changed),
			 NULL);
	/* Salir del cuadro guarda: el caso que más notas se llevaba por
	 * delante era escribir y pulsar en otro sitio. */
	g_signal_connect(notas_view, "focus-out-event",
			 G_CALLBACK(on_notas_foco_fuera), NULL);
	g_signal_connect(notas_view, "key-press-event",
			 G_CALLBACK(on_notas_tecla), NULL);
	gtk_widget_show(GTK_WIDGET(notas_view));
	gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(notas_view));
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

	bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(bar);
	notas_estado = gtk_label_new("");
	gtk_widget_set_halign(notas_estado, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(notas_estado),
				    GTK_STYLE_CLASS_DIM_LABEL);
	gtk_widget_show(notas_estado);
	gtk_box_pack_start(GTK_BOX(bar), notas_estado, FALSE, FALSE, 0);

	notas_boton = gtk_button_new_with_label(_("Guardar"));
	gtk_widget_set_halign(notas_boton, GTK_ALIGN_END);
	gtk_widget_set_tooltip_text(notas_boton,
				    _("La nota se guarda sola al salir del "
				      "cuadro o al cambiar de versículo "
				      "(Ctrl+S para hacerlo ahora)"));
	gtk_widget_set_sensitive(notas_boton, FALSE);
	gtk_widget_show(notas_boton);
	g_signal_connect(notas_boton, "clicked",
			 G_CALLBACK(on_notas_guardar_clicked), NULL);
	gtk_box_pack_end(GTK_BOX(bar), notas_boton, FALSE, FALSE, 0);
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
	if (!osisref || notas_en_actualizar)
		return;
	notas_en_actualizar = TRUE;

	clave = g_strdup_printf("%s %s", settings.MainWindowModule, osisref);
	mismo_verso = ultimo_verso && !strcmp(ultimo_verso, clave);

	/* Antes de soltar el versículo anterior, guardar lo que quedara
	 * escrito. notas_mod/notas_osis todavía apuntan a él, así que la
	 * nota va a parar a donde debe. Irse a otro versículo sin pulsar
	 * "Guardar" era la forma más fácil de perder una nota entera. */
	if (!mismo_verso)
		notas_guardar();

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
		if (existente && (!*existente ||
				  !strcmp(existente, "user content"))) {
			g_free(existente);
			existente = NULL;
		}
		buf = notas_view ? gtk_text_view_get_buffer(notas_view) : NULL;
		if (buf) {
			notas_cargando = TRUE;
			gtk_text_buffer_set_text(buf, existente ? existente : "",
						 -1);
			notas_cargando = FALSE;
		}
		g_free(notas_original);
		notas_original = g_strdup(existente ? existente : "");
		g_free(existente);
		notas_lista_reconstruir();
		notas_cancelar_temporizador();
		notas_estado_poner(*notas_original
				       ? _("Guardada")
				       : _("Escribe una nota para este versículo"),
				   FALSE);
	}

	/* Las notas personales comparten el panel lateral con los comentarios
	 * del autor, pero no controlan su visibilidad. Ocultarlo aquí cuando
	 * el versículo no tiene una nota deshacía inmediatamente la elección
	 * manual "Comentarios del autor"; abrirlo automáticamente cuando sí
	 * tenía una nota también impedía que el usuario lo mantuviera oculto.
	 * Si el panel ya está visible, una nota existente puede seleccionar
	 * su pestaña, pero nunca abrir o cerrar el contenedor por su cuenta. */
	if (tiene_notas && settings.showcomms) {
		gtk_notebook_set_current_page(
		    GTK_NOTEBOOK(widgets.notebook_comm_book), NOTAS_TAB_INDEX);
	}

	notas_en_actualizar = FALSE;
}
