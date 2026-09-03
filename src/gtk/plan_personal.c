/*
 * Biblia Elim
 * plan_personal.c - diálogo del plan de lectura que arma el lector
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

#include "gui/plan_personal.h"
#include "gui/dialog.h"
#include "gui/utilities.h"

#include "main/planes_lectura.h"

#include "gui/debug_glib_null.h"

enum {
	LCOL_MARCA = 0,
	LCOL_MEDIAS,	/* el testamento con solo unos cuantos libros */
	LCOL_NOMBRE,
	LCOL_CAPS,
	LCOL_LIBRO,	/* -1 en las filas de testamento */
	N_LCOLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *tree;
	GtkWidget *ent_nombre;
	GtkWidget *spin_dias;
	GtkWidget *spin_ritmo;
	GtkWidget *lbl_resumen;
	GtkWidget *lbl_aviso;
	GtkWidget *btn_guardar;
	GtkTreeStore *modelo;
	gboolean *marcados;	/* uno por libro, en orden canónico */
	int n_libros;
	/* El nombre se sugiere solo mientras el lector no escriba el
	 * suyo; a partir de ahí es cosa suya y no se toca. */
	gboolean nombre_propio;
	gboolean poniendo_nombre;
	/* Los días y el ritmo son la misma cifra vista de dos maneras:
	 * al mover uno se recalcula el otro, y esto evita el bucle. */
	gboolean recalculando;
} PERSONAL_UI;

/* --------------------------------------------------------------------
 * El árbol de libros
 * ------------------------------------------------------------------ */

static void
fila_de_libro(PERSONAL_UI *u, GtkTreeIter *grupo, int libro)
{
	GtkTreeIter iter;
	gchar *caps = g_strdup_printf(ngettext("%d capítulo", "%d capítulos",
					       main_planes_libro_capitulos(libro)),
				      main_planes_libro_capitulos(libro));

	gtk_tree_store_append(u->modelo, &iter, grupo);
	gtk_tree_store_set(u->modelo, &iter,
			   LCOL_MARCA, u->marcados[libro],
			   LCOL_MEDIAS, FALSE,
			   LCOL_NOMBRE, main_planes_libro_nombre(libro),
			   LCOL_CAPS, caps,
			   LCOL_LIBRO, libro,
			   -1);
	g_free(caps);
}

static void
fila_de_testamento(PERSONAL_UI *u, const char *nombre, gboolean nt)
{
	GtkTreeIter grupo;
	int libro, caps = 0;
	gchar *texto;

	for (libro = 0; libro < u->n_libros; ++libro)
		if (main_planes_libro_es_nt(libro) == nt)
			caps += main_planes_libro_capitulos(libro);
	texto = g_strdup_printf(_("%d capítulos"), caps);

	gtk_tree_store_append(u->modelo, &grupo, NULL);
	gtk_tree_store_set(u->modelo, &grupo,
			   LCOL_MARCA, FALSE,
			   LCOL_MEDIAS, FALSE,
			   LCOL_NOMBRE, nombre,
			   LCOL_CAPS, texto,
			   LCOL_LIBRO, -1,
			   -1);
	g_free(texto);

	for (libro = 0; libro < u->n_libros; ++libro)
		if (main_planes_libro_es_nt(libro) == nt)
			fila_de_libro(u, &grupo, libro);
}

/* Lleva al modelo lo que hay en u->marcados, y deja la fila del
 * testamento a medias cuando lleva unos libros sí y otros no. */
static void
pintar_marcas(PERSONAL_UI *u)
{
	GtkTreeIter grupo;

	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(u->modelo), &grupo))
		return;
	do {
		GtkTreeIter hijo;
		int marcados = 0, total = 0;

		if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(u->modelo),
						  &hijo, &grupo))
			continue;
		do {
			gint libro;
			gtk_tree_model_get(GTK_TREE_MODEL(u->modelo), &hijo,
					   LCOL_LIBRO, &libro, -1);
			gtk_tree_store_set(u->modelo, &hijo,
					   LCOL_MARCA, u->marcados[libro],
					   -1);
			if (u->marcados[libro])
				++marcados;
			++total;
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(u->modelo),
						  &hijo));

		gtk_tree_store_set(u->modelo, &grupo,
				   LCOL_MARCA, (marcados == total),
				   LCOL_MEDIAS, (marcados > 0 &&
						 marcados < total),
				   -1);
	} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(u->modelo), &grupo));
}

/* --------------------------------------------------------------------
 * Nombre sugerido
 * ------------------------------------------------------------------ */

static gchar *
nombre_sugerido(PERSONAL_UI *u)
{
	int libro, n = 0, n_at = 0, n_nt = 0, at = 0, nt = 0, primero = -1;

	for (libro = 0; libro < u->n_libros; ++libro) {
		if (main_planes_libro_es_nt(libro))
			++n_nt;
		else
			++n_at;
		if (!u->marcados[libro])
			continue;
		if (primero < 0)
			primero = libro;
		++n;
		if (main_planes_libro_es_nt(libro))
			++nt;
		else
			++at;
	}

	if (!n)
		return NULL;
	if (at == n_at && nt == n_nt)
		return g_strdup(_("Toda la Biblia"));
	if (at == n_at && !nt)
		return g_strdup(_("Antiguo Testamento"));
	if (nt == n_nt && !at)
		return g_strdup(_("Nuevo Testamento"));
	if (n == 1)
		return g_strdup(main_planes_libro_nombre(primero));
	return g_strdup_printf(ngettext("%s y %d libro más",
					"%s y %d libros más", n - 1),
			       main_planes_libro_nombre(primero), n - 1);
}

static void
sugerir_nombre(PERSONAL_UI *u)
{
	gchar *nombre;

	if (u->nombre_propio)
		return;
	nombre = nombre_sugerido(u);
	u->poniendo_nombre = TRUE;
	gtk_entry_set_text(GTK_ENTRY(u->ent_nombre), nombre ? nombre : "");
	u->poniendo_nombre = FALSE;
	g_free(nombre);
}

/* --------------------------------------------------------------------
 * Días, ritmo y resumen
 * ------------------------------------------------------------------ */

static gchar *
fecha_de_final(int dias)
{
	GDateTime *hoy = g_date_time_new_now_local();
	GDateTime *fin = g_date_time_add_days(hoy, dias - 1);
	/* "%e" antepone un espacio de cifra a los días de una sola cifra,
	 * y no es de los que quita g_strchug(): el día va a mano. */
	gchar *mes = g_date_time_format(fin, "%B");
	gchar *texto = g_strdup_printf(_("%d de %s de %d"),
				       g_date_time_get_day_of_month(fin),
				       mes ? mes : "",
				       g_date_time_get_year(fin));

	g_free(mes);
	g_date_time_unref(hoy);
	g_date_time_unref(fin);
	return texto;
}

/* Los libros marcados dan el total de capítulos; a partir de ahí, días
 * y ritmo son la misma cuenta: si el lector toca uno, el otro se
 * recalcula. El ritmo es el techo (capítulos como mucho en un día),
 * porque el reparto deja unos días con uno menos. */
static void
recalcular(PERSONAL_UI *u, gboolean desde_ritmo)
{
	int total = main_planes_capitulos_de(u->marcados);
	int libros = 0, dias, ritmo, i;
	gchar *ritmo_txt, *fin;

	if (u->recalculando)
		return;
	u->recalculando = TRUE;

	for (i = 0; i < u->n_libros; ++i)
		if (u->marcados[i])
			++libros;

	gtk_widget_set_sensitive(u->spin_dias, total > 0);
	gtk_widget_set_sensitive(u->spin_ritmo, total > 0);
	gtk_widget_set_sensitive(u->btn_guardar, total > 0);

	if (total < 1) {
		gtk_label_set_markup(GTK_LABEL(u->lbl_resumen),
				     _("<i>Marca los libros que quieras leer.</i>"));
		u->recalculando = FALSE;
		return;
	}

	gtk_spin_button_set_range(GTK_SPIN_BUTTON(u->spin_dias), 1, total);
	gtk_spin_button_set_range(GTK_SPIN_BUTTON(u->spin_ritmo), 1, total);

	dias = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(u->spin_dias));
	ritmo = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(u->spin_ritmo));

	if (desde_ritmo)
		dias = (total + ritmo - 1) / ritmo;
	else
		ritmo = (total + dias - 1) / dias;

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->spin_dias), dias);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(u->spin_ritmo), ritmo);

	ritmo_txt = main_planes_ritmo_texto(total, dias);
	fin = fecha_de_final(dias);

	{
		gchar *caps_txt = g_strdup_printf(ngettext("%d capítulo",
							   "%d capítulos",
							   total),
						  total);
		gchar *libros_txt = g_strdup_printf(ngettext("%d libro",
							     "%d libros",
							     libros),
						    libros);
		gchar *dias_txt = g_strdup_printf(ngettext("%d día", "%d días",
							   dias),
						  dias);
		gchar *cola = g_strdup_printf(_("Si empiezas hoy, terminarías "
						"el %s."),
					      fin);
		gchar *texto = g_markup_printf_escaped(
		    "<b>%s</b> de %s\n%s · %s\n"
		    "<span alpha='70%%'>%s</span>",
		    caps_txt, libros_txt, dias_txt, ritmo_txt, cola);

		gtk_label_set_markup(GTK_LABEL(u->lbl_resumen), texto);
		g_free(caps_txt);
		g_free(libros_txt);
		g_free(dias_txt);
		g_free(cola);
		g_free(texto);
	}

	g_free(ritmo_txt);
	g_free(fin);
	u->recalculando = FALSE;
}

/* --------------------------------------------------------------------
 * Señales
 * ------------------------------------------------------------------ */

static void
on_libro_marcado(GtkCellRendererToggle *celda, gchar *ruta, gpointer datos)
{
	PERSONAL_UI *u = datos;
	GtkTreeIter iter;
	gboolean marca;
	gint libro;

	(void)celda;
	if (!gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(u->modelo),
						 &iter, ruta))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(u->modelo), &iter,
			   LCOL_MARCA, &marca, LCOL_LIBRO, &libro, -1);

	if (libro >= 0) {
		u->marcados[libro] = !marca;
	} else {
		/* Fila de testamento: se lleva a todos sus libros. Si
		 * está a medias, la primera pulsación los marca todos. */
		GtkTreeIter hijo;
		gboolean medias;
		gboolean nuevo;

		gtk_tree_model_get(GTK_TREE_MODEL(u->modelo), &iter,
				   LCOL_MEDIAS, &medias, -1);
		nuevo = medias ? TRUE : !marca;
		if (gtk_tree_model_iter_children(GTK_TREE_MODEL(u->modelo),
						 &hijo, &iter)) {
			do {
				gint lb;
				gtk_tree_model_get(GTK_TREE_MODEL(u->modelo),
						   &hijo, LCOL_LIBRO, &lb, -1);
				u->marcados[lb] = nuevo;
			} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(u->modelo),
							  &hijo));
		}
	}

	pintar_marcas(u);
	sugerir_nombre(u);
	recalcular(u, FALSE);
}

static void
marcar_rango(PERSONAL_UI *u, gboolean at, gboolean nt)
{
	int libro;

	for (libro = 0; libro < u->n_libros; ++libro)
		u->marcados[libro] = main_planes_libro_es_nt(libro) ? nt : at;
	pintar_marcas(u);
	sugerir_nombre(u);
	recalcular(u, FALSE);
}

static void
on_todo(GtkButton *boton, gpointer datos)
{
	(void)boton;
	marcar_rango(datos, TRUE, TRUE);
}

static void
on_at(GtkButton *boton, gpointer datos)
{
	(void)boton;
	marcar_rango(datos, TRUE, FALSE);
}

static void
on_nt(GtkButton *boton, gpointer datos)
{
	(void)boton;
	marcar_rango(datos, FALSE, TRUE);
}

static void
on_nada(GtkButton *boton, gpointer datos)
{
	(void)boton;
	marcar_rango(datos, FALSE, FALSE);
}

static void
on_evangelios(GtkButton *boton, gpointer datos)
{
	PERSONAL_UI *u = datos;
	int libro, vistos = 0;

	(void)boton;
	/* Los cuatro primeros libros del Nuevo Testamento son Mateo,
	 * Marcos, Lucas y Juan. */
	for (libro = 0; libro < u->n_libros; ++libro) {
		u->marcados[libro] = (main_planes_libro_es_nt(libro) &&
				      vistos < 4);
		if (u->marcados[libro])
			++vistos;
	}
	pintar_marcas(u);
	sugerir_nombre(u);
	recalcular(u, FALSE);
}

static void
on_dias(GtkSpinButton *spin, gpointer datos)
{
	(void)spin;
	recalcular(datos, FALSE);
}

static void
on_ritmo(GtkSpinButton *spin, gpointer datos)
{
	(void)spin;
	recalcular(datos, TRUE);
}

static void
on_nombre(GtkEditable *entrada, gpointer datos)
{
	PERSONAL_UI *u = datos;

	(void)entrada;
	if (!u->poniendo_nombre)
		u->nombre_propio = TRUE;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
montar_arbol(PERSONAL_UI *u)
{
	GtkCellRenderer *celda;
	GtkTreeViewColumn *col;

	u->modelo = gtk_tree_store_new(N_LCOLS, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN,
				       G_TYPE_STRING, G_TYPE_STRING,
				       G_TYPE_INT);
	gtk_tree_view_set_model(GTK_TREE_VIEW(u->tree),
				GTK_TREE_MODEL(u->modelo));

	celda = gtk_cell_renderer_toggle_new();
	g_object_set(celda, "activatable", TRUE, NULL);
	g_signal_connect(celda, "toggled", G_CALLBACK(on_libro_marcado), u);
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "active", LCOL_MARCA,
						       "inconsistent", LCOL_MEDIAS,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(u->tree), col);

	celda = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "text", LCOL_NOMBRE,
						       NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(u->tree), col);

	celda = gtk_cell_renderer_text_new();
	g_object_set(celda, "xalign", 1.0, NULL);
	col = gtk_tree_view_column_new_with_attributes(NULL, celda,
						       "text", LCOL_CAPS,
						       NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(u->tree), col);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(u->tree), LCOL_NOMBRE);

	fila_de_testamento(u, _("Antiguo Testamento"), FALSE);
	fila_de_testamento(u, _("Nuevo Testamento"), TRUE);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(u->tree));
}

const PL_PLAN *
gui_plan_personal_dialog(GtkWindow *padre, const PL_PLAN *plan)
{
	GtkBuilder *gxml = elim_gtk_builder_new();
	const PL_PLAN *resultado = NULL;
	PERSONAL_UI u;
	GtkWidget *btn;
	gint respuesta;

	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/plan-personal.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo del plan personalizado."));
		return NULL;
	}

	memset(&u, 0, sizeof(u));
	u.dialog = UI_GET_ITEM(gxml, "dialog_personal");
	u.tree = UI_GET_ITEM(gxml, "tree_libros");
	u.ent_nombre = UI_GET_ITEM(gxml, "ent_nombre");
	u.spin_dias = UI_GET_ITEM(gxml, "spin_dias");
	u.spin_ritmo = UI_GET_ITEM(gxml, "spin_ritmo");
	u.lbl_resumen = UI_GET_ITEM(gxml, "lbl_resumen");
	u.lbl_aviso = UI_GET_ITEM(gxml, "lbl_aviso");
	u.btn_guardar = UI_GET_ITEM(gxml, "btn_guardar");

	u.n_libros = main_planes_libros_cuantos();
	u.marcados = g_new0(gboolean, u.n_libros);

	if (plan) {
		int hechos = main_planes_dias_hechos(plan);

		main_planes_personal_libros(plan, u.marcados);
		u.nombre_propio = TRUE;
		u.poniendo_nombre = TRUE;
		gtk_entry_set_text(GTK_ENTRY(u.ent_nombre), _(plan->nombre));
		u.poniendo_nombre = FALSE;
		gtk_spin_button_set_range(GTK_SPIN_BUTTON(u.spin_dias), 1,
					  plan->dias);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(u.spin_dias),
					  plan->dias);
		gtk_window_set_title(GTK_WINDOW(u.dialog),
				     _("Editar el plan"));
		gtk_button_set_label(GTK_BUTTON(u.btn_guardar),
				     _("Guardar los cambios"));
		if (hechos > 0) {
			gchar *aviso = g_strdup_printf(
			    ngettext("Llevas %d día marcado. Las marcas se "
				     "quedan donde están: si cambias los "
				     "libros o los días, el día 1 pasa a ser "
				     "otra lectura.",
				     "Llevas %d días marcados. Las marcas se "
				     "quedan donde están: si cambias los "
				     "libros o los días, el día 1 pasa a ser "
				     "otra lectura.",
				     hechos),
			    hechos);
			gchar *marca = g_markup_printf_escaped(
			    "<span size='small' alpha='70%%'>%s</span>", aviso);
			gtk_label_set_markup(GTK_LABEL(u.lbl_aviso), marca);
			gtk_widget_show(u.lbl_aviso);
			g_free(aviso);
			g_free(marca);
		}
	}

	montar_arbol(&u);
	pintar_marcas(&u);

	g_signal_connect(u.spin_dias, "value-changed", G_CALLBACK(on_dias), &u);
	g_signal_connect(u.spin_ritmo, "value-changed", G_CALLBACK(on_ritmo),
			 &u);
	g_signal_connect(u.ent_nombre, "changed", G_CALLBACK(on_nombre), &u);

	btn = UI_GET_ITEM(gxml, "btn_todo");
	g_signal_connect(btn, "clicked", G_CALLBACK(on_todo), &u);
	btn = UI_GET_ITEM(gxml, "btn_at");
	g_signal_connect(btn, "clicked", G_CALLBACK(on_at), &u);
	btn = UI_GET_ITEM(gxml, "btn_nt");
	g_signal_connect(btn, "clicked", G_CALLBACK(on_nt), &u);
	btn = UI_GET_ITEM(gxml, "btn_evangelios");
	g_signal_connect(btn, "clicked", G_CALLBACK(on_evangelios), &u);
	btn = UI_GET_ITEM(gxml, "btn_nada");
	g_signal_connect(btn, "clicked", G_CALLBACK(on_nada), &u);

	recalcular(&u, FALSE);

	gui_prepare_floating_dialog(GTK_WINDOW(u.dialog), padre);
	gtk_window_set_modal(GTK_WINDOW(u.dialog), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(u.dialog),
					GTK_RESPONSE_OK);

	respuesta = gtk_dialog_run(GTK_DIALOG(u.dialog));
	if (respuesta == GTK_RESPONSE_OK) {
		const gchar *nombre =
		    gtk_entry_get_text(GTK_ENTRY(u.ent_nombre));
		gchar *limpio = g_strdup(nombre ? nombre : "");
		int dias = gtk_spin_button_get_value_as_int(
		    GTK_SPIN_BUTTON(u.spin_dias));

		g_strstrip(limpio);
		if (!*limpio) {
			gchar *sugerido = nombre_sugerido(&u);
			g_free(limpio);
			limpio = sugerido ? sugerido : g_strdup(_("Mi plan"));
		}

		if (plan) {
			if (main_planes_personal_editar(plan, limpio,
							u.marcados, dias))
				resultado = plan;
		} else {
			resultado = main_planes_personal_nuevo(limpio,
							       u.marcados,
							       dias);
		}
		g_free(limpio);
	}

	gtk_widget_destroy(u.dialog);
	g_object_unref(u.modelo);
	g_object_unref(gxml);
	g_free(u.marcados);
	return resultado;
}
