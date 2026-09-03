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
#include "main/racha.h"

#include "gui/debug_glib_null.h"

enum {
	PCOL_NOMBRE = 0,
	PCOL_PORCIENTO,
	PCOL_CUENTA,	/* "12 / 50" dentro de la barra */
	PCOL_PESO,	/* negrita en los totales y en el plan en curso */
	N_PCOLS
};

/* El calendario de constancia: una casilla por día, las semanas en
 * columnas y los días de la semana en filas, que es como se lee de un
 * vistazo si hubo huecos. Un año cabe en 53 columnas. */
#define CAL_CELDA 13
#define CAL_HUECO 3
#define CAL_PASO (CAL_CELDA + CAL_HUECO)
#define CAL_SEMANAS 53
#define CAL_IZQ 26	/* sitio para las iniciales de los días */
#define CAL_ARRIBA 17	/* sitio para los meses */

typedef struct {
	GtkWidget *dialog;
	GtkWidget *lbl_total;
	GtkWidget *lbl_testamentos;
	GtkWidget *lbl_nota;
	GtkWidget *barra_total;
	GtkWidget *tree_libros;
	GtkWidget *tree_planes;
	GtkWidget *lbl_racha;
	GtkWidget *lbl_racha_pie;
	GtkWidget *calendario;
	GtkWidget *lbl_calendario_pie;
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
 * Constancia: la racha y el calendario
 * ------------------------------------------------------------------ */

static void
fecha_texto(const GDate *d, gchar *buf, gsize n)
{
	g_snprintf(buf, n, "%04d-%02d-%02d", g_date_get_year(d),
		   g_date_get_month(d), g_date_get_day(d));
}

static void
cal_hoy(GDate *d)
{
	GDateTime *ahora = g_date_time_new_now_local();

	g_date_clear(d, 1);
	g_date_set_dmy(d, g_date_time_get_day_of_month(ahora),
		       g_date_time_get_month(ahora),
		       g_date_time_get_year(ahora));
	g_date_time_unref(ahora);
}

/* El lunes de la primera columna: se retrocede un año de semanas desde
 * hoy y se cuadra al lunes, para que cada fila sea siempre el mismo día
 * de la semana. */
static void
cal_inicio(GDate *d)
{
	cal_hoy(d);
	g_date_subtract_days(d, (CAL_SEMANAS - 1) * 7);
	while (g_date_get_weekday(d) != G_DATE_MONDAY)
		g_date_subtract_days(d, 1);
}

/* La fecha de una casilla, o FALSE si esa casilla cae en el futuro. */
static gboolean
cal_fecha_de(int semana, int fila, GDate *fuera)
{
	GDate d, hoy;

	if (semana < 0 || semana >= CAL_SEMANAS || fila < 0 || fila > 6)
		return FALSE;
	cal_inicio(&d);
	g_date_add_days(&d, semana * 7 + fila);
	cal_hoy(&hoy);
	if (g_date_compare(&d, &hoy) > 0)
		return FALSE;
	*fuera = d;
	return TRUE;
}

static gboolean
on_calendario_draw(GtkWidget *widget, cairo_t *cr, gpointer datos)
{
	GtkStyleContext *ctx = gtk_widget_get_style_context(widget);
	GdkRGBA fg;
	GDate hoy;
	int semana, fila;
	int mes_pintado = 0;

	(void)datos;

	/* El color sale del tema, no de una tabla: así el calendario se
	 * ve bien en claro, en oscuro y en pergamino sin tocar nada. */
	gtk_style_context_get_color(ctx, gtk_style_context_get_state(ctx), &fg);
	cal_hoy(&hoy);

	cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
			       CAIRO_FONT_WEIGHT_NORMAL);
	cairo_set_font_size(cr, 9.0);

	/* Iniciales de los días, en filas alternas para no apelmazar */
	{
		static const char *ini[7] = {"L", "M", "X", "J", "V", "S", "D"};
		int f;
		for (f = 0; f < 7; f += 2) {
			cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue,
					      fg.alpha * 0.45);
			cairo_move_to(cr, 4,
				      CAL_ARRIBA + f * CAL_PASO + CAL_CELDA - 3);
			cairo_show_text(cr, ini[f]);
		}
	}

	for (semana = 0; semana < CAL_SEMANAS; ++semana) {
		for (fila = 0; fila < 7; ++fila) {
			GDate d;
			gchar buf[16];
			double x, y;
			gboolean leido, es_hoy;

			if (!cal_fecha_de(semana, fila, &d))
				continue;

			x = CAL_IZQ + semana * CAL_PASO;
			y = CAL_ARRIBA + fila * CAL_PASO;

			/* El mes se rotula sobre la columna donde empieza */
			if (fila == 0 && g_date_get_day(&d) <= 7 &&
			    g_date_get_month(&d) != mes_pintado) {
				gchar mbuf[32];
				GDate primero = d;

				mes_pintado = g_date_get_month(&d);
				g_date_set_day(&primero, 1);
				g_date_strftime(mbuf, sizeof(mbuf), "%b",
						&primero);
				cairo_set_source_rgba(cr, fg.red, fg.green,
						      fg.blue, fg.alpha * 0.45);
				cairo_move_to(cr, x, CAL_ARRIBA - 5);
				cairo_show_text(cr, mbuf);
			}

			fecha_texto(&d, buf, sizeof(buf));
			leido = main_racha_dia(buf);
			es_hoy = (g_date_compare(&d, &hoy) == 0);

			/* Leído o no, solo hay dos estados: el progreso no
			 * guarda cuánto se leyó cada día, así que pintar
			 * intensidades sería inventárselas. */
			cairo_set_source_rgba(cr, fg.red, fg.green, fg.blue,
					      leido ? fg.alpha * 0.85
						    : fg.alpha * 0.10);
			cairo_rectangle(cr, x, y, CAL_CELDA, CAL_CELDA);
			cairo_fill(cr);

			if (es_hoy) {
				cairo_set_source_rgba(cr, fg.red, fg.green,
						      fg.blue, fg.alpha * 0.9);
				cairo_set_line_width(cr, 1.0);
				cairo_rectangle(cr, x - 1.5, y - 1.5,
						CAL_CELDA + 3, CAL_CELDA + 3);
				cairo_stroke(cr);
			}
		}
	}
	return FALSE;
}

/* Qué día hay bajo el ratón, para el globo. */
static gboolean
on_calendario_tooltip(GtkWidget *widget, gint x, gint y, gboolean del_teclado,
		      GtkTooltip *tooltip, gpointer datos)
{
	GDate d;
	int semana, fila;
	gchar buf[16], fecha[64];
	gchar *texto;

	(void)widget;
	(void)del_teclado;
	(void)datos;

	if (x < CAL_IZQ || y < CAL_ARRIBA)
		return FALSE;
	semana = (x - CAL_IZQ) / CAL_PASO;
	fila = (y - CAL_ARRIBA) / CAL_PASO;
	/* Dentro de la casilla, no en el hueco de al lado. */
	if ((x - CAL_IZQ) % CAL_PASO >= CAL_CELDA ||
	    (y - CAL_ARRIBA) % CAL_PASO >= CAL_CELDA)
		return FALSE;
	if (!cal_fecha_de(semana, fila, &d))
		return FALSE;

	fecha_texto(&d, buf, sizeof(buf));
	g_date_strftime(fecha, sizeof(fecha), "%A, %e de %B de %Y", &d);
	g_strstrip(fecha);

	texto = g_strdup_printf("%s\n%s", fecha,
				main_racha_dia(buf) ? _("Leíste")
						    : _("Sin marcar"));
	gtk_tooltip_set_text(tooltip, texto);
	g_free(texto);
	return TRUE;
}

static void
refrescar_constancia(void)
{
	int actual = main_racha_actual();
	int mejor = main_racha_mejor();
	int total = main_racha_total();
	const char *desde = main_racha_desde();
	gchar *mensaje, *markup;

	/* La cabecera: la racha, que es lo que se viene a mirar */
	if (total < 1)
		mensaje = g_strdup(_("Todavía no hay nada que contar"));
	else if (actual < 1)
		mensaje = g_strdup(_("Sin racha ahora mismo"));
	else
		mensaje = g_strdup_printf(
		    ngettext("%d día seguido", "%d días seguidos", actual),
		    actual);

	markup = g_markup_printf_escaped(
	    "<span size='xx-large' weight='bold'>%s</span>", mensaje);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_racha), markup);
	g_free(markup);
	g_free(mensaje);

	/* Y el pie, con lo demás */
	if (total < 1)
		mensaje = g_strdup(
		    _("La constancia empieza a medirse con la primera lectura "
		      "que marques. De lo de antes no hay registro: el "
		      "progreso guardaba qué días del plan estaban hechos, "
		      "pero no en qué fecha los marcaste."));
	else {
		GString *g = g_string_new(NULL);

		if (main_racha_hoy_pendiente())
			g_string_append(
			    g, _("Hoy todavía no. La racha sigue viva hasta "
				 "medianoche.  ·  "));

		g_string_append_printf(
		    g, ngettext("La más larga: %d día", "La más larga: %d días",
				mejor),
		    mejor);
		g_string_append_printf(
		    g, ngettext("  ·  %d día con lectura",
				"  ·  %d días con lectura", total),
		    total);

		if (desde) {
			GDate d;
			int anio, mes, dia;

			if (sscanf(desde, "%4d-%2d-%2d", &anio, &mes, &dia) == 3 &&
			    g_date_valid_dmy(dia, mes, anio)) {
				gchar largo[64];

				g_date_clear(&d, 1);
				g_date_set_dmy(&d, dia, mes, anio);
				g_date_strftime(largo, sizeof(largo),
						"%e de %B de %Y", &d);
				g_string_append_printf(g, _("  ·  desde el %s"),
						       g_strstrip(largo));
			}
		}
		mensaje = g_string_free(g, FALSE);
	}

	markup = g_markup_printf_escaped(
	    "<span size='small' alpha='70%%'>%s</span>", mensaje);
	gtk_label_set_markup(GTK_LABEL(ui->lbl_racha_pie), markup);
	g_free(markup);
	g_free(mensaje);

	markup = g_markup_printf_escaped(
	    "<span size='small' alpha='70%%'>%s</span>",
	    _("Cada casilla es un día del último año. Solo cuentan los días "
	      "en que marcaste una lectura: ponerse al día dando por leído "
	      "lo atrasado no pinta casillas, porque es justo decir que "
	      "esos días no se leyeron."));
	gtk_label_set_markup(GTK_LABEL(ui->lbl_calendario_pie), markup);
	g_free(markup);

	gtk_widget_set_size_request(ui->calendario,
				    CAL_IZQ + CAL_SEMANAS * CAL_PASO,
				    CAL_ARRIBA + 7 * CAL_PASO + 2);
	gtk_widget_queue_draw(ui->calendario);
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
	refrescar_constancia();

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
	ui->lbl_racha = UI_GET_ITEM(gxml, "lbl_racha");
	ui->lbl_racha_pie = UI_GET_ITEM(gxml, "lbl_racha_pie");
	ui->calendario = UI_GET_ITEM(gxml, "calendario");
	ui->lbl_calendario_pie = UI_GET_ITEM(gxml, "lbl_calendario_pie");
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

	g_signal_connect(ui->calendario, "draw",
			 G_CALLBACK(on_calendario_draw), NULL);
	g_signal_connect(ui->calendario, "query-tooltip",
			 G_CALLBACK(on_calendario_tooltip), NULL);

	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	llenar();
	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
}
