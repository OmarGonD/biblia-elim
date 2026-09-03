/*
 * Biblia Elim
 * progreso_lectura.c - diálogo Tu progreso
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

#include "gui/progreso_lectura.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/planes_lectura.h"

#include "gui/debug_glib_null.h"

enum {
	PCOL_NOMBRE = 0,
	PCOL_PORCIENTO,
	PCOL_CUENTA,	/* "12 / 50" dentro de la barra */
	PCOL_PESO,	/* negrita en los totales y en el plan en curso */
	N_PCOLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *lbl_total;
	GtkWidget *lbl_testamentos;
	GtkWidget *lbl_nota;
	GtkWidget *barra_total;
	GtkWidget *tree_libros;
	GtkWidget *tree_planes;
	GtkTreeStore *libros;
	GtkListStore *planes;
} PROGRESO_UI;

static PROGRESO_UI *ui = NULL;

static int
porciento(int parte, int total)
{
	if (total < 1)
		return 0;
	return (parte * 100) / total;
}

/* --------------------------------------------------------------------
 * Por libro y de la Biblia entera
 * ------------------------------------------------------------------ */

static void
fila_libro(GtkTreeIter *grupo, int libro, int leidos)
{
	GtkTreeIter iter;
	int caps = main_planes_libro_capitulos(libro);
	gchar *cuenta = g_strdup_printf(_("%d de %d"), leidos, caps);

	gtk_tree_store_append(ui->libros, &iter, grupo);
	gtk_tree_store_set(ui->libros, &iter,
			   PCOL_NOMBRE, main_planes_libro_nombre(libro),
			   PCOL_PORCIENTO, porciento(leidos, caps),
			   PCOL_CUENTA, cuenta,
			   PCOL_PESO, PANGO_WEIGHT_NORMAL,
			   -1);
	g_free(cuenta);
}

/* Devuelve por referencia lo leído y lo que hay en el testamento, que
 * es lo que luego dice el renglón de debajo del título. */
static void
grupo_testamento(const char *nombre, gboolean nt, const int *por_libro,
		 int *leidos_fuera, int *total_fuera)
{
	GtkTreeIter grupo;
	int libro, leidos = 0, total = 0;
	gchar *cuenta;

	for (libro = 0; libro < main_planes_libros_cuantos(); ++libro) {
		if (main_planes_libro_es_nt(libro) != nt)
			continue;
		leidos += por_libro[libro];
		total += main_planes_libro_capitulos(libro);
	}
	cuenta = g_strdup_printf(_("%d de %d"), leidos, total);

	gtk_tree_store_append(ui->libros, &grupo, NULL);
	gtk_tree_store_set(ui->libros, &grupo,
			   PCOL_NOMBRE, nombre,
			   PCOL_PORCIENTO, porciento(leidos, total),
			   PCOL_CUENTA, cuenta,
			   PCOL_PESO, PANGO_WEIGHT_BOLD,
			   -1);
	g_free(cuenta);

	for (libro = 0; libro < main_planes_libros_cuantos(); ++libro)
		if (main_planes_libro_es_nt(libro) == nt)
			fila_libro(&grupo, libro, por_libro[libro]);

	*leidos_fuera = leidos;
	*total_fuera = total;
}

/* --------------------------------------------------------------------
 * Por plan
 * ------------------------------------------------------------------ */

static void
llenar_planes(void)
{
	const char *activo = main_planes_activo();
	int i;

	gtk_list_store_clear(ui->planes);
	for (i = 0; i < main_planes_cuantos(); ++i) {
		const PL_PLAN *plan = main_planes_get(i);
		GtkTreeIter iter;
		gboolean en_curso;
		int hechos;
		gchar *cuenta;

		if (!plan)
			continue;
		en_curso = (activo && !strcmp(activo, plan->id));
		hechos = main_planes_dias_hechos(plan);

		if (hechos >= plan->dias)
			cuenta = g_strdup_printf(_("terminado · %d días"),
						 plan->dias);
		else if (en_curso) {
			int atraso = main_planes_dias_atrasados(plan);
			if (atraso > 0)
				cuenta = g_strdup_printf(
				    ngettext("%d de %d días · %d por detrás",
					     "%d de %d días · %d por detrás",
					     atraso),
				    hechos, plan->dias, atraso);
			else
				cuenta = g_strdup_printf(_("%d de %d días · al día"),
							 hechos, plan->dias);
		} else
			cuenta = g_strdup_printf(_("%d de %d días"), hechos,
						 plan->dias);

		gtk_list_store_append(ui->planes, &iter);
		gtk_list_store_set(ui->planes, &iter,
				   PCOL_NOMBRE, _(plan->nombre),
				   PCOL_PORCIENTO, porciento(hechos, plan->dias),
				   PCOL_CUENTA, cuenta,
				   PCOL_PESO, en_curso ? PANGO_WEIGHT_BOLD
						       : PANGO_WEIGHT_NORMAL,
				   -1);
		g_free(cuenta);
	}
}

/* --------------------------------------------------------------------
 * Llenar el diálogo
 * ------------------------------------------------------------------ */

static void
llenar(void)
{
	int n = main_planes_libros_cuantos();
	int *por_libro = g_new0(int, n);
	int total = main_planes_capitulos_leidos(por_libro);
	int caps_biblia = 0, i;
	int at = 0, at_total = 0, nt = 0, nt_total = 0;
	gchar *texto;

	for (i = 0; i < n; ++i)
		caps_biblia += main_planes_libro_capitulos(i);

	gtk_tree_store_clear(ui->libros);
	grupo_testamento(_("Antiguo Testamento"), FALSE, por_libro,
			 &at, &at_total);
	grupo_testamento(_("Nuevo Testamento"), TRUE, por_libro, &nt, &nt_total);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(ui->tree_libros));

	{
		gchar *cabeza =
		    total ? g_strdup_printf(_("%d de %d capítulos de la "
					      "Biblia · %d %%"),
					    total, caps_biblia,
					    porciento(total, caps_biblia))
			  : g_strdup(_("Todavía no has marcado ninguna lectura"));

		texto = g_markup_printf_escaped(
		    "<span size='x-large' weight='bold'>%s</span>", cabeza);
		gtk_label_set_markup(GTK_LABEL(ui->lbl_total), texto);
		g_free(cabeza);
		g_free(texto);
	}

	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ui->barra_total),
				      caps_biblia
					  ? (double)total / (double)caps_biblia
					  : 0.0);

	texto = g_strdup_printf(_("Antiguo Testamento %d de %d (%d %%)  ·  "
				  "Nuevo Testamento %d de %d (%d %%)"),
				at, at_total, porciento(at, at_total),
				nt, nt_total, porciento(nt, nt_total));
	gtk_label_set_text(GTK_LABEL(ui->lbl_testamentos), texto);
	g_free(texto);

	llenar_planes();

	texto = g_markup_printf_escaped(
	    "<span size='small' alpha='70%%'>%s</span>",
	    _("Se cuenta lo que has marcado como leído en tus planes. Un "
	      "capítulo leído con dos planes distintos cuenta una vez."));
	gtk_label_set_markup(GTK_LABEL(ui->lbl_nota), texto);
	g_free(texto);

	g_free(por_libro);
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
montar_columnas(GtkWidget *tree, GtkTreeModel *modelo)
{
	GtkCellRenderer *celda;
	GtkTreeViewColumn *col;

	gtk_tree_view_set_model(GTK_TREE_VIEW(tree), modelo);

	celda = gtk_cell_renderer_text_new();
	g_object_set(celda, "ypad", 2, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "text", PCOL_NOMBRE,
						       "weight", PCOL_PESO,
						       NULL);
	gtk_tree_view_column_set_min_width(col, 220);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);

	/* La barra lleva la cuenta escrita dentro: una columna menos y se
	 * lee de un vistazo cuánto falta. */
	celda = gtk_cell_renderer_progress_new();
	g_object_set(celda, "ypad", 2, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "value", PCOL_PORCIENTO,
						       "text", PCOL_CUENTA,
						       NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col);
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
	if (ui->libros)
		g_object_unref(ui->libros);
	if (ui->planes)
		g_object_unref(ui->planes);
	g_free(ui);
	ui = NULL;
}

void
gui_progreso_lectura_dialog(GtkWindow *padre)
{
	GtkBuilder *gxml;
	GtkWidget *btn_cerrar;

	if (ui && ui->dialog) {
		/* Puede haber cambiado algo desde que se abrió. */
		llenar();
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}

	gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/progreso-lectura.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo Tu progreso."));
		return;
	}

	ui = g_new0(PROGRESO_UI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_progreso");
	ui->lbl_total = UI_GET_ITEM(gxml, "lbl_total");
	ui->lbl_testamentos = UI_GET_ITEM(gxml, "lbl_testamentos");
	ui->lbl_nota = UI_GET_ITEM(gxml, "lbl_nota");
	ui->barra_total = UI_GET_ITEM(gxml, "barra_total");
	ui->tree_libros = UI_GET_ITEM(gxml, "tree_libros");
	ui->tree_planes = UI_GET_ITEM(gxml, "tree_planes");
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    padre ? padre
					  : (widgets.app ? GTK_WINDOW(widgets.app)
							 : NULL));

	ui->libros = gtk_tree_store_new(N_PCOLS, G_TYPE_STRING, G_TYPE_INT,
					G_TYPE_STRING, G_TYPE_INT);
	ui->planes = gtk_list_store_new(N_PCOLS, G_TYPE_STRING, G_TYPE_INT,
					G_TYPE_STRING, G_TYPE_INT);
	montar_columnas(ui->tree_libros, GTK_TREE_MODEL(ui->libros));
	montar_columnas(ui->tree_planes, GTK_TREE_MODEL(ui->planes));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(ui->tree_libros),
					PCOL_NOMBRE);

	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	llenar();
	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
}
