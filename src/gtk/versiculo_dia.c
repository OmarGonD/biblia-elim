/*
 * Biblia Elim
 * versiculo_dia.c - diálogo Versículo del día
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

#include "gui/versiculo_dia.h"
#include "gui/memorizacion.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/versiculo_dia.h"
#include "main/texto_verso.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/navbar_versekey.h"
#include "main/url.hh"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* Lo que se tarda en dejar de escribir antes de guardar solo, igual que
 * en las notas de versículo: corto para que no se pierda nada al cerrar
 * de golpe, largo para no escribir el archivo en cada tecla. */
#define AUTOGUARDADO_MS 1200

typedef struct {
	GtkWidget *dialog;
	GtkWidget *lbl_fecha;
	GtkWidget *lbl_cita;
	GtkWidget *lbl_texto;
	GtkWidget *lbl_version;
	GtkWidget *lbl_reflexion;
	GtkWidget *lbl_estado;
	GtkWidget *btn_antes;
	GtkWidget *btn_despues;
	GtkWidget *btn_hoy;
	GtkWidget *btn_abrir;
	GtkWidget *btn_memorizar;
	GtkTextView *vista;

	GDateTime *fecha;	/* el día que se está mirando */
	gchar *clave;		/* "John 3:16", para abrirlo en la ventana */
	/* Lo último que se guardó, para no reescribir el archivo entero
	 * cuando el lector abre el cuadro y lo cierra sin tocar nada. */
	gchar *original;
	guint temporizador;
	/* set_text() al cargar emite "changed" igual que si se hubiera
	 * tecleado; esto evita que eso dispare un guardado. */
	gboolean cargando;
	/* Al destruir el diálogo todavía se guarda lo escrito, pero ya no
	 * se puede pintar nada: los rótulos se están yendo. */
	gboolean cerrando;
} VD_UI;

static VD_UI *ui = NULL;

static void mostrar(void);

/* --------------------------------------------------------------------
 * El texto del versículo
 *
 * No está escrito en ninguna tabla: se le pide al módulo que el lector
 * tenga abierto, así que sale en su versión y con su ortografía.
 * ------------------------------------------------------------------ */

/* El texto lo trae main_texto_de(), que además le devuelve al módulo la
 * clave que tenía: si no, abrir este diálogo le arrastraría la lectura
 * al lector hasta el versículo del día. */

/* --------------------------------------------------------------------
 * La reflexión
 * ------------------------------------------------------------------ */

static gchar *
texto_del_cuadro(void)
{
	GtkTextBuffer *buf;
	GtkTextIter ini, fin;

	if (!ui || !ui->vista)
		return NULL;
	buf = gtk_text_view_get_buffer(ui->vista);
	gtk_text_buffer_get_bounds(buf, &ini, &fin);
	return gtk_text_buffer_get_text(buf, &ini, &fin, FALSE);
}

static void
refrescar_estado(void)
{
	int n;
	gchar *mensaje, *markup;

	if (!ui || ui->cerrando)
		return;

	n = main_versiculo_reflexiones_cuantas();
	if (n < 1)
		mensaje = g_strdup(_("Se guarda sola mientras escribes."));
	else
		mensaje = g_strdup_printf(
		    ngettext("Se guarda sola · %d día con algo escrito.",
			     "Se guarda sola · %d días con algo escrito.", n),
		    n);

	markup = g_markup_printf_escaped(
	    "<span size='small' alpha='70%%'>%s</span>", mensaje);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_estado), markup);
	g_free(markup);
	g_free(mensaje);
}

/* Guarda solo si de verdad cambió: escribir settings.xml entero cada vez
 * que se cierra el diálogo no le hace bien a nadie. */
static void
guardar_si_cambio(void)
{
	gchar *ahora, *fecha;

	if (!ui || !ui->fecha)
		return;

	ahora = texto_del_cuadro();
	if (!g_strcmp0(ahora, ui->original)) {
		g_free(ahora);
		return;
	}

	fecha = main_versiculo_fecha(ui->fecha);
	main_versiculo_reflexion_poner(fecha, ahora);
	g_free(fecha);

	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);

	g_free(ui->original);
	ui->original = ahora;
	refrescar_estado();
}

static gboolean
guardar_por_pausa(gpointer datos)
{
	(void)datos;
	ui->temporizador = 0;
	guardar_si_cambio();
	return G_SOURCE_REMOVE;
}

static void
on_cambio(GtkTextBuffer *buf, gpointer datos)
{
	(void)buf;
	(void)datos;
	if (!ui || ui->cargando)
		return;
	if (ui->temporizador)
		g_source_remove(ui->temporizador);
	ui->temporizador = g_timeout_add(AUTOGUARDADO_MS, guardar_por_pausa,
					 NULL);
}

static gboolean
on_sale_del_cuadro(GtkWidget *widget, GdkEvent *evento, gpointer datos)
{
	(void)widget;
	(void)evento;
	(void)datos;
	if (ui && ui->temporizador) {
		g_source_remove(ui->temporizador);
		ui->temporizador = 0;
	}
	guardar_si_cambio();
	return FALSE;
}

void
gui_versiculo_dia_guardar_pendiente(void)
{
	if (!ui || !ui->dialog)
		return;
	if (ui->temporizador) {
		g_source_remove(ui->temporizador);
		ui->temporizador = 0;
	}
	guardar_si_cambio();
}

/* --------------------------------------------------------------------
 * Pintar el día
 * ------------------------------------------------------------------ */

/* En castellano el día de la semana sale en minúscula ("jueves"), que
 * está bien dentro de una frase pero no abriendo el diálogo. */
static gchar *
con_mayuscula(const gchar *texto)
{
	const gchar *segunda;
	gchar *primera, *todo;

	if (!texto || !*texto)
		return g_strdup("");

	segunda = g_utf8_next_char(texto);
	primera = g_utf8_strup(texto, segunda - texto);
	if (!primera)
		return g_strdup(texto);
	todo = g_strconcat(primera, segunda, NULL);
	g_free(primera);
	return todo;
}

static gboolean
es_hoy(GDateTime *fecha)
{
	GDateTime *hoy = g_date_time_new_now_local();
	gboolean igual;
	gchar *a = main_versiculo_fecha(fecha);
	gchar *b = main_versiculo_fecha(hoy);

	igual = !g_strcmp0(a, b);
	g_free(a);
	g_free(b);
	g_date_time_unref(hoy);
	return igual;
}

static void
mostrar(void)
{
	gchar *cita, *fecha_larga, *cabeza, *markup, *texto, *guardado, *fecha;
	GtkTextBuffer *buf;

	if (!ui || !ui->fecha)
		return;

	/* La fecha */
	fecha_larga = g_date_time_format(ui->fecha, "%A, %e de %B de %Y");
	cabeza = con_mayuscula(fecha_larga ? fecha_larga : "");
	markup = g_markup_printf_escaped(
	    "<span size='small' alpha='70%%'>%s</span>", cabeza);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_fecha), markup);
	g_free(markup);
	g_free(cabeza);
	g_free(fecha_larga);

	/* La cita */
	cita = main_versiculo_cita(ui->fecha);
	markup = g_markup_printf_escaped(
	    "<span size='large' weight='bold'>%s</span>", cita);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_cita), markup);
	g_free(markup);
	g_free(cita);

	/* El texto, del módulo que tenga abierto */
	g_free(ui->clave);
	ui->clave = main_versiculo_clave(ui->fecha);
	texto = main_texto_de(ui->clave);

	if (texto) {
		markup = g_markup_printf_escaped("<span size='large'>%s</span>",
						 texto);
		gtk_label_set_markup(GTK_LABEL(ui->lbl_texto), markup);
		g_free(markup);
	} else {
		gtk_label_set_text(
		    GTK_LABEL(ui->lbl_texto),
		    _("No se pudo leer el texto: abre una Biblia en la "
		      "ventana principal."));
	}
	gtk_widget_set_sensitive(ui->btn_abrir, texto != NULL);
	gtk_widget_set_sensitive(ui->btn_memorizar, texto != NULL);
	g_free(texto);

	if (settings.MainWindowModule && *settings.MainWindowModule) {
		const char *desc =
		    main_get_module_description(settings.MainWindowModule);
		markup = g_markup_printf_escaped(
		    "<span size='small' alpha='70%%'>%s</span>",
		    (desc && *desc) ? desc : settings.MainWindowModule);
		gtk_label_set_markup(GTK_LABEL(ui->lbl_version), markup);
		g_free(markup);
	} else
		gtk_label_set_text(GTK_LABEL(ui->lbl_version), "");

	/* El rótulo de la reflexión dice de qué día es cuando no es hoy,
	 * que si no se escribe uno en el día que no era. */
	if (es_hoy(ui->fecha))
		markup = g_markup_printf_escaped("<b>%s</b>",
						 _("Tu reflexión de hoy"));
	else {
		gchar *corta = g_date_time_format(ui->fecha, "%e de %B");
		if (corta)
			g_strstrip(corta);	/* %e rellena con un espacio */
		markup = g_markup_printf_escaped(
		    "<b>%s</b>", (corta && *corta) ? corta : _("Tu reflexión"));
		g_free(corta);
	}
	gtk_label_set_markup(GTK_LABEL(ui->lbl_reflexion), markup);
	g_free(markup);

	/* Y lo escrito ese día */
	fecha = main_versiculo_fecha(ui->fecha);
	guardado = main_versiculo_reflexion(fecha);
	g_free(fecha);

	ui->cargando = TRUE;
	buf = gtk_text_view_get_buffer(ui->vista);
	gtk_text_buffer_set_text(buf, guardado ? guardado : "", -1);
	ui->cargando = FALSE;

	g_free(ui->original);
	ui->original = texto_del_cuadro();
	g_free(guardado);

	/* Mañana todavía no ha pasado: no hay nada sobre lo que pensar. */
	gtk_widget_set_sensitive(ui->btn_despues, !es_hoy(ui->fecha));
	gtk_widget_set_sensitive(ui->btn_hoy, !es_hoy(ui->fecha));

	refrescar_estado();
}

/* --------------------------------------------------------------------
 * Los botones
 * ------------------------------------------------------------------ */

static void
mover_dias(int dias)
{
	GDateTime *nueva;

	if (!ui || !ui->fecha)
		return;

	/* Lo escrito se guarda antes de cambiar de día: si no, se perdería
	 * al recargar el cuadro. */
	if (ui->temporizador) {
		g_source_remove(ui->temporizador);
		ui->temporizador = 0;
	}
	guardar_si_cambio();

	nueva = g_date_time_add_days(ui->fecha, dias);
	if (!nueva)
		return;
	g_date_time_unref(ui->fecha);
	ui->fecha = nueva;
	mostrar();
}

static void
on_antes(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	mover_dias(-1);
}

static void
on_despues(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (!es_hoy(ui->fecha))
		mover_dias(1);
}

static void
on_hoy(GtkButton *boton, gpointer datos)
{
	GDateTime *hoy;

	(void)boton;
	(void)datos;
	if (ui->temporizador) {
		g_source_remove(ui->temporizador);
		ui->temporizador = 0;
	}
	guardar_si_cambio();

	hoy = g_date_time_new_now_local();
	g_date_time_unref(ui->fecha);
	ui->fecha = hoy;
	mostrar();
}

static void
on_abrir(GtkButton *boton, gpointer datos)
{
	gchar *url;
	char *valida;

	(void)boton;
	(void)datos;
	if (!ui->clave || !*ui->clave)
		return;

	/* Igual que las referencias de los planes: la clave lleva el
	 * nombre OSIS del libro, y el motor la deja en el idioma del
	 * módulo antes de navegar. */
	valida = (char *)main_get_valid_key(settings.MainWindowModule,
					    ui->clave);
	url = g_strdup_printf("sword:///%s",
			      (valida && *valida) ? valida : ui->clave);
	free(valida);
	main_url_handler(url, TRUE);
	g_free(url);
}

static void
on_memorizar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (ui->clave && *ui->clave)
		gui_memorizacion_anadir(ui->clave);
}

static void
on_cerrar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (ui && ui->dialog)
		gtk_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *widget, gpointer datos)
{
	(void)widget;
	(void)datos;
	if (!ui)
		return;

	ui->cerrando = TRUE;
	if (ui->temporizador) {
		g_source_remove(ui->temporizador);
		ui->temporizador = 0;
	}
	guardar_si_cambio();

	if (ui->fecha)
		g_date_time_unref(ui->fecha);
	g_free(ui->clave);
	g_free(ui->original);
	g_free(ui);
	ui = NULL;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

void
gui_versiculo_dia_dialog(GtkWindow *padre)
{
	GtkBuilder *gxml;
	GtkWidget *btn_cerrar;
	GtkTextBuffer *buf;

	if (ui && ui->dialog) {
		/* Puede haber cambiado de versión, o de día. */
		mostrar();
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}

	gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/versiculo-dia.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo Versículo "
				      "del día."));
		return;
	}

	ui = g_new0(VD_UI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_versiculo");
	ui->lbl_fecha = UI_GET_ITEM(gxml, "lbl_fecha");
	ui->lbl_cita = UI_GET_ITEM(gxml, "lbl_cita");
	ui->lbl_texto = UI_GET_ITEM(gxml, "lbl_texto");
	ui->lbl_version = UI_GET_ITEM(gxml, "lbl_version");
	ui->lbl_reflexion = UI_GET_ITEM(gxml, "lbl_reflexion");
	ui->lbl_estado = UI_GET_ITEM(gxml, "lbl_estado");
	ui->btn_antes = UI_GET_ITEM(gxml, "btn_antes");
	ui->btn_despues = UI_GET_ITEM(gxml, "btn_despues");
	ui->btn_hoy = UI_GET_ITEM(gxml, "btn_hoy");
	ui->btn_abrir = UI_GET_ITEM(gxml, "btn_abrir");
	ui->btn_memorizar = UI_GET_ITEM(gxml, "btn_memorizar");
	ui->vista = GTK_TEXT_VIEW(UI_GET_ITEM(gxml, "vista_reflexion"));
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    padre ? padre
					  : (widgets.app ? GTK_WINDOW(widgets.app)
							 : NULL));

	ui->fecha = g_date_time_new_now_local();

	buf = gtk_text_view_get_buffer(ui->vista);
	g_signal_connect(buf, "changed", G_CALLBACK(on_cambio), NULL);
	g_signal_connect(ui->vista, "focus-out-event",
			 G_CALLBACK(on_sale_del_cuadro), NULL);

	g_signal_connect(ui->btn_antes, "clicked", G_CALLBACK(on_antes), NULL);
	g_signal_connect(ui->btn_despues, "clicked", G_CALLBACK(on_despues),
			 NULL);
	g_signal_connect(ui->btn_hoy, "clicked", G_CALLBACK(on_hoy), NULL);
	g_signal_connect(ui->btn_abrir, "clicked", G_CALLBACK(on_abrir), NULL);
	g_signal_connect(ui->btn_memorizar, "clicked",
			 G_CALLBACK(on_memorizar), NULL);
	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	mostrar();
	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
}
