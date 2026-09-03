/*
 * Biblia Elim
 * memorizacion.c - diálogo Memorización
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

#include "gui/memorizacion.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/memorizacion.h"
#include "main/settings.h"
#include "main/texto_verso.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

enum {
	MCOL_CITA = 0,
	MCOL_CAJA,
	MCOL_PROXIMO,
	MCOL_ACIERTOS,
	MCOL_CLAVE,	/* oculta: la que entiende el motor */
	N_MCOLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *lbl_cabecera;
	GtkWidget *marco_tarjeta;
	GtkWidget *lbl_cita;
	GtkWidget *lbl_texto;
	GtkWidget *lbl_lista;
	GtkWidget *lbl_nota;
	GtkWidget *btn_ver;
	GtkWidget *btn_fallo;
	GtkWidget *btn_acierto;
	GtkWidget *btn_anadir;
	GtkWidget *btn_quitar;
	GtkWidget *tree;
	GtkListStore *versos;

	gchar *actual;		/* la clave que está en la tarjeta */
	gboolean destapado;	/* si ya se enseñó el texto */
} MEM_UI;

static MEM_UI *ui = NULL;

static void refrescar(void);

static void
guardar_ya(void)
{
	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);
}

/* --------------------------------------------------------------------
 * La tarjeta
 * ------------------------------------------------------------------ */

static void
pintar_tarjeta(void)
{
	gchar *markup;

	if (!ui->actual) {
		gtk_widget_set_sensitive(ui->marco_tarjeta, FALSE);
		gtk_label_set_text(GTK_LABEL(ui->lbl_cita), "");
		gtk_label_set_text(GTK_LABEL(ui->lbl_texto), "");
		gtk_widget_set_sensitive(ui->btn_ver, FALSE);
		gtk_widget_set_sensitive(ui->btn_fallo, FALSE);
		gtk_widget_set_sensitive(ui->btn_acierto, FALSE);
		return;
	}

	gtk_widget_set_sensitive(ui->marco_tarjeta, TRUE);
	markup = g_markup_printf_escaped(
	    "<span size='large' weight='bold'>%s</span>", ui->actual);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_cita), markup);
	g_free(markup);

	if (ui->destapado) {
		gchar *texto = main_texto_de(ui->actual);

		if (texto) {
			markup = g_markup_printf_escaped(
			    "<span size='large'>%s</span>", texto);
			gtk_label_set_markup(GTK_LABEL(ui->lbl_texto), markup);
			g_free(markup);
		} else
			gtk_label_set_text(
			    GTK_LABEL(ui->lbl_texto),
			    _("No se pudo leer el texto: abre una Biblia en "
			      "la ventana principal."));
		g_free(texto);
	} else {
		/* Tapado a propósito: la gracia está en recitarlo antes de
		 * verlo. Comprobar sin haberlo intentado no es un repaso. */
		markup = g_markup_printf_escaped(
		    "<span size='large' alpha='45%%'><i>%s</i></span>",
		    _("Recítalo de memoria, y luego destápalo."));
		gtk_label_set_markup(GTK_LABEL(ui->lbl_texto), markup);
		g_free(markup);
	}

	gtk_widget_set_sensitive(ui->btn_ver, !ui->destapado);
	/* Calificarse antes de mirar sería calificarse a ciegas. */
	gtk_widget_set_sensitive(ui->btn_fallo, ui->destapado);
	gtk_widget_set_sensitive(ui->btn_acierto, ui->destapado);
}

/* Toma el primero que toque, o deja la tarjeta vacía. */
static void
siguiente_tarjeta(void)
{
	GList *hoy = main_memoria_de_hoy();

	g_clear_pointer(&ui->actual, g_free);
	ui->destapado = FALSE;
	if (hoy)
		ui->actual = g_strdup(((MEM_VERSO *)hoy->data)->clave);
	main_memoria_libre(hoy);
}

static void
on_ver(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	ui->destapado = TRUE;
	pintar_tarjeta();
}

static void
calificar(gboolean acertado)
{
	if (!ui->actual)
		return;
	main_memoria_repasar(ui->actual, acertado);
	guardar_ya();
	siguiente_tarjeta();
	refrescar();
}

static void
on_acierto(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	calificar(TRUE);
}

static void
on_fallo(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	calificar(FALSE);
}

/* --------------------------------------------------------------------
 * La lista
 * ------------------------------------------------------------------ */

/* "en 3 días", "hoy", "atrasado 2 días": lo que hace falta saber de un
 * vistazo es cuándo vuelve, no la fecha exacta. */
static gchar *
cuando_texto(const char *fecha)
{
	GDate cuando, hoy;
	GDateTime *ahora;
	int dias, anio, mes, dia;

	if (!fecha || !*fecha ||
	    sscanf(fecha, "%4d-%2d-%2d", &anio, &mes, &dia) != 3 ||
	    !g_date_valid_dmy(dia, mes, anio))
		return g_strdup(_("hoy"));

	g_date_clear(&cuando, 1);
	g_date_set_dmy(&cuando, dia, mes, anio);

	ahora = g_date_time_new_now_local();
	g_date_clear(&hoy, 1);
	g_date_set_dmy(&hoy, g_date_time_get_day_of_month(ahora),
		       g_date_time_get_month(ahora),
		       g_date_time_get_year(ahora));
	g_date_time_unref(ahora);

	dias = (int)g_date_days_between(&hoy, &cuando);
	if (dias == 0)
		return g_strdup(_("hoy"));
	if (dias < 0)
		return g_strdup_printf(ngettext("atrasado %d día",
						"atrasado %d días", -dias),
				       -dias);
	if (dias == 1)
		return g_strdup(_("mañana"));
	return g_strdup_printf(_("en %d días"), dias);
}

static void
llenar_lista(void)
{
	GList *todos = main_memoria_todos(), *l;

	gtk_list_store_clear(ui->versos);
	for (l = todos; l; l = l->next) {
		MEM_VERSO *v = l->data;
		GtkTreeIter iter;
		gchar *caja, *cuando;

		caja = g_strdup_printf(_("%d de %d"), v->caja, MEM_CAJAS);
		cuando = cuando_texto(v->proximo);

		gtk_list_store_append(ui->versos, &iter);
		gtk_list_store_set(ui->versos, &iter,
				   MCOL_CITA, v->clave,
				   MCOL_CAJA, caja,
				   MCOL_PROXIMO, cuando,
				   MCOL_ACIERTOS, v->aciertos,
				   MCOL_CLAVE, v->clave,
				   -1);
		g_free(caja);
		g_free(cuando);
	}
	main_memoria_libre(todos);
}

/* --------------------------------------------------------------------
 * Pintarlo todo
 * ------------------------------------------------------------------ */

static void
refrescar(void)
{
	int pendientes = main_memoria_pendientes();
	int cuantos = main_memoria_cuantos();
	int asentados = main_memoria_asentados();
	int semana = main_memoria_altas_de_esta_semana();
	gchar *mensaje, *markup;

	if (cuantos < 1)
		mensaje = g_strdup(_("Todavía no estás memorizando ninguno"));
	else if (pendientes < 1)
		mensaje = g_strdup(_("Nada que repasar hoy"));
	else
		mensaje = g_strdup_printf(
		    ngettext("%d versículo para repasar hoy",
			     "%d versículos para repasar hoy", pendientes),
		    pendientes);

	markup = g_markup_printf_escaped(
	    "<span size='x-large' weight='bold'>%s</span>", mensaje);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_cabecera), markup);
	g_free(markup);
	g_free(mensaje);

	llenar_lista();

	if (cuantos < 1)
		mensaje = g_strdup(_("Tus versículos"));
	else
		mensaje = g_strdup_printf(
		    ngettext("Tus versículos · %d en total",
			     "Tus versículos · %d en total", cuantos),
		    cuantos);
	markup = g_markup_printf_escaped("<b>%s</b>", mensaje);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_lista), markup);
	g_free(markup);
	g_free(mensaje);

	/* El pie es el que lleva el ritmo de uno por semana. No impide
	 * añadir más: es una sugerencia, no una reja. */
	{
		GString *g = g_string_new(NULL);

		if (semana < 1)
			g_string_append(
			    g, _("Esta semana todavía no has elegido ninguno. "
				 "Uno por semana es un ritmo que se sostiene."));
		else
			g_string_append_printf(
			    g,
			    ngettext("Esta semana ya elegiste %d.",
				     "Esta semana ya elegiste %d.", semana),
			    semana);

		if (asentados > 0)
			g_string_append_printf(
			    g,
			    ngettext("  ·  %d en la última caja",
				     "  ·  %d en la última caja", asentados),
			    asentados);

		markup = g_markup_printf_escaped(
		    "<span size='small' alpha='70%%'>%s</span>", g->str);
		gtk_label_set_markup(GTK_LABEL(ui->lbl_nota), markup);
		g_free(markup);
		g_string_free(g, TRUE);
	}

	pintar_tarjeta();
}

/* --------------------------------------------------------------------
 * Añadir y quitar
 * ------------------------------------------------------------------ */

void
gui_memorizacion_anadir(const char *clave)
{
	gchar *aviso;

	if (!clave || !*clave) {
		gui_set_statusbar(_("No hay ningún versículo abierto."));
		return;
	}

	if (main_memoria_tiene(clave)) {
		aviso = g_strdup_printf(_("%s ya estaba en tu lista."), clave);
		gui_set_statusbar(aviso);
		g_free(aviso);
		return;
	}

	if (!main_memoria_anadir(clave)) {
		gui_set_statusbar(_("No se pudo añadir ese versículo."));
		return;
	}
	guardar_ya();

	aviso = g_strdup_printf(_("%s añadido para memorizar."), clave);
	gui_set_statusbar(aviso);
	g_free(aviso);

	if (ui && ui->dialog) {
		if (!ui->actual)
			siguiente_tarjeta();
		refrescar();
	}
}

static void
on_anadir(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	/* El versículo abierto en la ventana, que es el que se está
	 * leyendo cuando a uno le apetece aprendérselo. */
	gui_memorizacion_anadir(settings.currentverse);
}

static void
on_quitar(GtkButton *boton, gpointer datos)
{
	GtkTreeSelection *sel;
	GtkTreeModel *modelo;
	GtkTreeIter iter;
	gchar *clave = NULL;

	(void)boton;
	(void)datos;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->tree));
	if (!gtk_tree_selection_get_selected(sel, &modelo, &iter))
		return;
	gtk_tree_model_get(modelo, &iter, MCOL_CLAVE, &clave, -1);
	if (!clave)
		return;

	main_memoria_quitar(clave);
	guardar_ya();

	if (ui->actual && !strcmp(ui->actual, clave))
		siguiente_tarjeta();
	g_free(clave);
	refrescar();
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
montar_columnas(void)
{
	GtkCellRenderer *celda;
	GtkTreeViewColumn *col;
	GtkTreeView *t = GTK_TREE_VIEW(ui->tree);

	gtk_tree_view_set_model(t, GTK_TREE_MODEL(ui->versos));

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(
	    _("Versículo"), celda, "text", MCOL_CITA, NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(t, col);

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Caja"), celda, "text",
						       MCOL_CAJA, NULL);
	gtk_tree_view_append_column(t, col);

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Vuelve"), celda,
						       "text", MCOL_PROXIMO,
						       NULL);
	gtk_tree_view_append_column(t, col);

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(_("Aciertos"), celda,
						       "text", MCOL_ACIERTOS,
						       NULL);
	gtk_tree_view_append_column(t, col);
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
	if (ui->versos)
		g_object_unref(ui->versos);
	g_free(ui->actual);
	g_free(ui);
	ui = NULL;
}

void
gui_memorizacion_dialog(GtkWindow *padre)
{
	GtkBuilder *gxml;
	GtkWidget *btn_cerrar;

	if (ui && ui->dialog) {
		refrescar();
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}

	gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/memorizacion.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo "
				      "Memorización."));
		return;
	}

	ui = g_new0(MEM_UI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_memoria");
	ui->lbl_cabecera = UI_GET_ITEM(gxml, "lbl_cabecera");
	ui->marco_tarjeta = UI_GET_ITEM(gxml, "marco_tarjeta");
	ui->lbl_cita = UI_GET_ITEM(gxml, "lbl_cita");
	ui->lbl_texto = UI_GET_ITEM(gxml, "lbl_texto");
	ui->lbl_lista = UI_GET_ITEM(gxml, "lbl_lista");
	ui->lbl_nota = UI_GET_ITEM(gxml, "lbl_nota");
	ui->btn_ver = UI_GET_ITEM(gxml, "btn_ver");
	ui->btn_fallo = UI_GET_ITEM(gxml, "btn_fallo");
	ui->btn_acierto = UI_GET_ITEM(gxml, "btn_acierto");
	ui->btn_anadir = UI_GET_ITEM(gxml, "btn_anadir");
	ui->btn_quitar = UI_GET_ITEM(gxml, "btn_quitar");
	ui->tree = UI_GET_ITEM(gxml, "tree_versos");
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    padre ? padre
					  : (widgets.app ? GTK_WINDOW(widgets.app)
							 : NULL));

	ui->versos = gtk_list_store_new(N_MCOLS, G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_INT,
					G_TYPE_STRING);
	montar_columnas();

	g_signal_connect(ui->btn_ver, "clicked", G_CALLBACK(on_ver), NULL);
	g_signal_connect(ui->btn_acierto, "clicked", G_CALLBACK(on_acierto),
			 NULL);
	g_signal_connect(ui->btn_fallo, "clicked", G_CALLBACK(on_fallo), NULL);
	g_signal_connect(ui->btn_anadir, "clicked", G_CALLBACK(on_anadir),
			 NULL);
	g_signal_connect(ui->btn_quitar, "clicked", G_CALLBACK(on_quitar),
			 NULL);
	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	siguiente_tarjeta();
	refrescar();
	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
}
