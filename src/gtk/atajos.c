/*
 * Biblia Elim
 * atajos.c - ventana de ayuda con los atajos de teclado
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/atajos.h"
#include "gui/widgets.h"

#include "gui/debug_glib_null.h"

typedef struct {
	const gchar *accel;
	const gchar *title;
} Atajo;

typedef struct {
	const gchar *title;
	const Atajo *items;
} Grupo;

/* Keep in step with on_vbox1_key_press_event() in main_window.c and the
 * accelerators shown in main_menu.c. */
static const Atajo navegar[] = {
	{"<Control>l", N_("Ir a una referencia")},
	{"j Down", N_("Versículo siguiente")},
	{"k Up", N_("Versículo anterior")},
	{"n", N_("Capítulo siguiente")},
	{"p", N_("Capítulo anterior")},
	{"<Shift>n", N_("Libro siguiente")},
	{"<Shift>p", N_("Libro anterior")},
	{"<Alt>Left", N_("Atrás en el historial")},
	{"<Alt>Right", N_("Adelante en el historial")},
	{NULL, NULL}};
static const Atajo pestanas[] = {
	{"<Control>t", N_("Nueva pestaña")},
	{"<Control>w", N_("Cerrar pestaña")},
	{"<Control>Tab", N_("Pestaña siguiente")},
	{"<Control><Shift>Tab", N_("Pestaña anterior")},
	{"<Control>1", N_("Ir a la pestaña 1 … 9 (Ctrl+número)")},
	{"<Control><Shift>Page_Up", N_("Mover pestaña a la izquierda")},
	{"<Control><Shift>Page_Down", N_("Mover pestaña a la derecha")},
	{NULL, NULL}};
static const Atajo buscar[] = {
	{"<Control>f", N_("Buscar en el texto")},
	{"F3", N_("Búsqueda avanzada")},
	{"<Alt>d", N_("Ir a la entrada del diccionario")},
	{"<Alt>c", N_("Ir a comentarios")},
	{NULL, NULL}};
static const Atajo vista[] = {
	{"<Control><Shift>f", N_("Modo lectura")},
	{"Escape", N_("Salir del modo lectura")},
	{"<Control>s", N_("Mostrar u ocultar el panel lateral")},
	{"<Control>plus", N_("Aumentar el texto")},
	{"<Control>minus", N_("Reducir el texto")},
	{"<Control>0", N_("Tamaño de texto original")},
	{"<Control>r", N_("Leer en voz alta")},
	{NULL, NULL}};
static const Atajo estudio[] = {
	{"<Alt>b", N_("Añadir marcador")},
	{"<Alt>a", N_("Anotar el versículo")},
	{"<Alt>s", N_("Biblia interlineal")},
	{"<Alt>l", N_("Lemas (Strong)")},
	{"<Alt>m", N_("Morfología")},
	{"<Alt>n", N_("Notas al pie")},
	{"<Alt>x", N_("Referencias cruzadas")},
	{"<Alt>r", N_("Palabras de Cristo en rojo")},
	{NULL, NULL}};
static const Atajo general[] = {
	{"F1", N_("Ayuda")},
	{"F2", N_("Preferencias")},
	{"F4", N_("Instalar Biblias")},
	{"<Control>slash", N_("Atajos de teclado")},
	{"<Control>q", N_("Salir")},
	{NULL, NULL}};

static const Grupo grupos[] = {
	{N_("Navegación"), navegar}, {N_("Pestañas"), pestanas},
	{N_("Búsqueda"), buscar},    {N_("Vista"), vista},
	{N_("Estudio"), estudio},    {N_("General"), general},
};

static GtkWidget *ventana;

/* "<Control>l" -> "Ctrl+L"; several alternatives are separated by spaces
 * ("j Down" -> "J / Down"). */
static gchar *etiqueta_atajo(const gchar *accel)
{
	gchar **partes = g_strsplit(accel, " ", -1);
	GString *out = g_string_new(NULL);

	for (guint i = 0; partes[i]; i++) {
		guint key = 0;
		GdkModifierType mods = 0;
		gchar *l;

		if (!gtk_accelerator_parse(partes[i], &key, &mods))
			continue;
		l = gtk_accelerator_get_label(key, mods);
		if (out->len)
			g_string_append(out, " / ");
		g_string_append(out, l);
		g_free(l);
	}
	g_strfreev(partes);
	return g_string_free(out, FALSE);
}

static void ventana_destruida(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	ventana = NULL;
}

void gui_atajos_mostrar(void)
{
	if (ventana) {
		gtk_window_present(GTK_WINDOW(ventana));
		return;
	}

	ventana = gtk_window_new();
	gtk_widget_set_name(ventana, "elim-atajos");
	gtk_window_set_title(GTK_WINDOW(ventana), _("Atajos de teclado"));
	gtk_window_set_default_size(GTK_WINDOW(ventana), 520, 600);
	if (widgets.app) {
		gtk_window_set_transient_for(GTK_WINDOW(ventana),
					     GTK_WINDOW(widgets.app));
		gtk_window_set_modal(GTK_WINDOW(ventana), TRUE);
	}

	GtkWidget *scroll = gtk_scrolled_window_new();
	GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_widget_set_margin_top(caja, 12);
	gtk_widget_set_margin_bottom(caja, 12);
	gtk_widget_set_margin_start(caja, 18);
	gtk_widget_set_margin_end(caja, 18);

	for (guint g = 0; g < G_N_ELEMENTS(grupos); g++) {
		GtkWidget *tit = gtk_label_new(NULL);
		gchar *m = g_markup_printf_escaped("<b>%s</b>", _(grupos[g].title));
		gtk_label_set_markup(GTK_LABEL(tit), m);
		g_free(m);
		gtk_label_set_xalign(GTK_LABEL(tit), 0);
		gtk_widget_set_margin_top(tit, g ? 12 : 0);
		gtk_box_append(GTK_BOX(caja), tit);

		for (const Atajo *a = grupos[g].items; a->accel; a++) {
			GtkWidget *fila = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
			GtkWidget *t = gtk_label_new(_(a->title));
			gchar *e = etiqueta_atajo(a->accel);
			GtkWidget *k = gtk_label_new(e);

			g_free(e);
			gtk_label_set_xalign(GTK_LABEL(t), 0);
			gtk_widget_set_hexpand(t, TRUE);
			gtk_label_set_xalign(GTK_LABEL(k), 1);
			gtk_widget_add_css_class(k, "dim-label");
			gtk_box_append(GTK_BOX(fila), t);
			gtk_box_append(GTK_BOX(fila), k);
			gtk_box_append(GTK_BOX(caja), fila);
		}
	}

	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), caja);
	gtk_window_set_child(GTK_WINDOW(ventana), scroll);
	g_signal_connect(ventana, "destroy", G_CALLBACK(ventana_destruida), NULL);
	gtk_window_present(GTK_WINDOW(ventana));
}
