/*
 * Biblia Elim
 * nube_palabras.c - diálogo Nube de palabras
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

#include <math.h>
#include <string.h>

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"
#include "gui/table_helpers.h"
#include <glib/gi18n.h>

#include "gui/nube_palabras.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/nube_palabras.h"
#include "main/settings.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"
#include "nube_canvas.h"

enum {
	COL_NOMBRE = 0,
	COL_ABREV,
	COL_OSIS,
	N_LIBRO_COLS
};

enum {
	TCOL_PALABRA = 0,
	TCOL_CUENTA_A,
	TCOL_CUENTA_B,
	TCOL_DIF,
	TCOL_PCT_A,
	TCOL_PCT_B,
	TCOL_DIF_PCT,
	TCOL_DIF_VALOR,
	TCOL_PCT_A_VALOR,
	TCOL_PCT_B_VALOR,
	TCOL_DIF_PCT_VALOR,
	TCOL_DIF_COLOR,
	TCOL_DIF_PCT_COLOR,
	N_TABLA_COLS
};

#define NUBE_LIMITE 100
#define NUBE_MAX_NUBE 80

typedef struct _nube_ui NUBE_UI;
struct _nube_ui {
	GtkWidget *dialog;
	GtkWidget *combo_libro;
	GtkWidget *combo_libro_b;
	GtkWidget *chk_comparar;
	GtkWidget *btn_descargar;
	GtkWidget *btn_cerrar;
	GtkWidget *lbl_resumen;
	GtkWidget *box_nube;
	GtkWidget *mensaje;
	GtkWidget *canvas;
	GtkWidget *canvas_b;
	GtkWidget *panel_b;
	GtkWidget *title_a;
	GtkWidget *title_b;
	GtkWidget *cloud_stack;
	GtkWidget *tree;
	GtkWidget *scroll_tabla;
	GtkListStore *libros;
	GHashTable *abreviaturas;	/* book name -> abbreviation, for typing in the list */
	GListStore *tabla;
	GtkColumnViewColumn *col_a;
	GtkColumnViewColumn *col_b;
	GtkColumnViewColumn *col_dif;
	GtkColumnViewColumn *col_pct_a;
	GtkColumnViewColumn *col_pct_b;
	GtkColumnViewColumn *col_dif_pct;
	guint espera;		/* debounce after typing a book name */
	gboolean ocupado;	/* counting; the loop below may re-enter */
	gboolean pendiente;	/* update once the cloud area has a size */
	gchar *mostrado;	/* what is shown: "A\nB" or "A" */
	gchar *titulo_a;	/* title markup, reused for downloads */
	gchar *titulo_b;
	gint reintentos;	/* random picks left if a random book is empty */
	gboolean poniendo;	/* the dialog, not the reader, sets a book */
	gboolean libro_b_puesto;	/* book B was chosen (a dropdown cannot be empty) */
	GHashTable *cache;	/* "module\nA[\nB]" -> NUBE_CONTEO */
	NUBE_CONTEO *conteo;	/* shown now; owned by cache */
	gboolean par;		/* shown as two clouds */
	double aspecto;		/* panel shape the clouds were laid out for */
	guint espera_tam;	/* debounce after a resize */
};

static NUBE_UI *ui = NULL;

static const char *COLORES_CLARO[] = {
    "#9c2c0d", "#1d4e89", "#1b4332", "#6a040f", "#5a189a", "#7f4f24", "#0a9396"};
static const char *COLORES_OSCURO[] = {
    "#f4a261", "#90e0ef", "#b7e4c7", "#ffb3c1", "#e0aaff", "#ffd166", "#94d2bd"};

static gboolean
color_es_oscuro(const char *hex)
{
	unsigned int r = 0, g = 0, b = 0;
	if (!hex || sscanf(hex, "#%02x%02x%02x", &r, &g, &b) != 3)
		return FALSE;
	return (0.299 * r + 0.587 * g + 0.114 * b) < 140.0;
}

static const gchar *
combo_texto(GtkWidget *combo)
{
	return elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo));
}

/* Chooses the book named TEXTO in COMBO; none when the list has no such book. */
static void
combo_set_texto(GtkWidget *combo, const gchar *texto)
{
	elim_dropdown_set_active(GTK_DROP_DOWN(combo),
				 elim_dropdown_find_text(GTK_DROP_DOWN(combo),
							 texto));
}

/* What the reader types in the open list is matched against: the name and
 * the abbreviation of the book (ui->abreviaturas maps one to the other). */
static gchar *
libro_texto_de_busqueda(GtkStringObject *fila, gpointer user_data)
{
	GHashTable *abreviaturas = user_data;
	const gchar *nombre = gtk_string_object_get_string(fila);
	const gchar *abrev = g_hash_table_lookup(abreviaturas, nombre);

	return g_strdup_printf("%s %s", nombre, abrev ? abrev : "");
}

static void
poblar_libros(GtkListStore *store, GHashTable *abreviaturas, GtkWidget *combo,
	      GtkWidget *combo_b)
{
	gtk_list_store_clear(store);
	g_hash_table_remove_all(abreviaturas);
	elim_dropdown_remove_all(GTK_DROP_DOWN(combo));
	elim_dropdown_remove_all(GTK_DROP_DOWN(combo_b));
	if (!settings.MainWindowModule)
		return;

	GList *lista = main_nube_lista_libros(settings.MainWindowModule);
	for (GList *l = lista; l; l = l->next) {
		NUBE_LIBRO *libro = l->data;
		GtkTreeIter iter;
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   COL_NOMBRE, libro->nombre,
				   COL_ABREV, libro->abrev,
				   COL_OSIS, libro->osis,
				   -1);
		g_hash_table_insert(abreviaturas, g_strdup(libro->nombre),
				    g_strdup(libro->abrev));
		elim_dropdown_append(GTK_DROP_DOWN(combo), NULL, libro->nombre);
		elim_dropdown_append(GTK_DROP_DOWN(combo_b), NULL, libro->nombre);
	}
	main_nube_lista_libros_free(lista);

	elim_dropdown_enable_search_with(GTK_DROP_DOWN(combo),
					 G_CALLBACK(libro_texto_de_busqueda),
					 abreviaturas);
	elim_dropdown_enable_search_with(GTK_DROP_DOWN(combo_b),
					 G_CALLBACK(libro_texto_de_busqueda),
					 abreviaturas);
}

static gchar *
html_escape(const gchar *s)
{
	return g_markup_escape_text(s ? s : "", -1);
}

gchar *
gui_nube_palabras_html(NUBE_CONTEO *c, gboolean comparar)
{
	const char *bg = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	const char *fg = settings.bible_text_color ? settings.bible_text_color : "#222222";
	gboolean oscuro = color_es_oscuro(bg);
	const char **paleta = oscuro ? COLORES_OSCURO : COLORES_CLARO;
	const char *color_a = oscuro ? "#90e0ef" : "#1d4e89";
	const char *color_b = oscuro ? "#f4a261" : "#9c2c0d";
	const char *color_eq = oscuro ? "#ced4da" : "#6c757d";

	gint max_c = 1;
	gint min_c = G_MAXINT;
	guint n = c->palabras->len < NUBE_MAX_NUBE ? c->palabras->len : NUBE_MAX_NUBE;
	for (guint i = 0; i < n; i++) {
		NUBE_PALABRA *w = g_ptr_array_index(c->palabras, i);
		gint m = comparar ? MAX(w->cuenta, w->cuenta_b) : w->cuenta;
		if (m > max_c)
			max_c = m;
		if (m > 0 && m < min_c)
			min_c = m;
	}
	if (min_c == G_MAXINT)
		min_c = 1;

	gchar *libro_esc = html_escape(c->libro);
	gchar *libro_b_esc = html_escape(c->libro_b);
	GString *s = g_string_new(NULL);
	g_string_append_printf(s,
			       "<!DOCTYPE html><html><head>"
			       "<meta charset=\"utf-8\"/>"
			       "<style>"
			       "html,body{margin:0;padding:0;background:%s;color:%s;"
			       "font-family:'Noto Sans','DejaVu Sans',sans-serif;}"
			       ".wrap{padding:14px 16px 20px;}"
			       ".titulo{text-align:center;font-size:13px;opacity:.75;"
			       "margin-bottom:10px;letter-spacing:.02em;}"
			       ".leyenda{text-align:center;font-size:12px;margin-bottom:12px;}"
			       ".dot{display:inline-block;width:.7em;height:.7em;border-radius:50%%;"
			       "margin:0 .35em 0 .8em;vertical-align:middle;}"
			       ".nube{text-align:center;line-height:2.35;min-height:220px;}"
			       ".nube span{display:inline-block;line-height:1.08;font-weight:650;"
			       "white-space:nowrap;margin:4px 8px;padding:2px 4px;"
			       "border-bottom:1px solid currentColor;border-radius:3px;}"
			       "</style></head><body><div class=\"wrap\">",
			       bg, fg);

	if (comparar && c->libro_b) {
		g_string_append_printf(s,
				       "<div class=\"titulo\">%s · %s %s %s</div>"
				       "<div class=\"leyenda\">"
				       "<span class=\"dot\" style=\"background:%s\"></span> %s %s"
				       "<span class=\"dot\" style=\"background:%s\"></span> %s %s"
				       "<span class=\"dot\" style=\"background:%s\"></span> %s"
				       "</div>",
				       _("Palabras más usadas"),
				       libro_esc, _("y"), libro_b_esc,
				       color_a, _("más en"), libro_esc,
				       color_b, _("más en"), libro_b_esc,
				       color_eq, _("similar"));
	} else {
		g_string_append_printf(s,
				       "<div class=\"titulo\">%s %s</div>",
				       _("Palabras más usadas en"),
				       libro_esc);
	}

	g_string_append(s, "<p style=\"text-align:center\">");
	for (guint i = 0; i < n; i++) {
		NUBE_PALABRA *w = g_ptr_array_index(c->palabras, i);
		gint m = comparar ? MAX(w->cuenta, w->cuenta_b) : w->cuenta;
		if (m < 1)
			continue;
		/* The native GtkTextView renderer supports font size attributes,
		 * not CSS font-size. Relative steps map to 0.67x through 3x. */
		double ratio = (max_c == min_c) ? 1.0 :
			(double)(m - min_c) / (double)(max_c - min_c);
		int size = (int)round(-4.0 + pow(ratio, 0.72) * 28.0);
		const char *color;
		if (comparar) {
			if (w->dif_pct > 0.08)
				color = color_a;
			else if (w->dif_pct < -0.08)
				color = color_b;
			else
				color = color_eq;
		} else {
			color = paleta[g_str_hash(w->palabra) % G_N_ELEMENTS(COLORES_CLARO)];
		}
		gchar *etiqueta = cloud_label(w);
		gchar *pw = html_escape(etiqueta);
		g_free(etiqueta);
		g_string_append_printf(s,
				       "<font size=\"%+d\" color=\"%s\" title=\"%s: %d\">"
				       "%s</font> &#8194; ",
				       size, color, pw, m, pw);
		g_free(pw);
	}
	g_string_append(s, "</p></div></body></html>");
	g_free(libro_esc);
	g_free(libro_b_esc);
	return g_string_free(s, FALSE);
}

static void
mostrar_mensaje(const gchar *mensaje)
{
	gtk_label_set_text(GTK_LABEL(ui->mensaje), mensaje);
	gtk_stack_set_visible_child_name(GTK_STACK(ui->cloud_stack), "message");
}

/* The title over a cloud: badge, book in bold, word count dimmed. The
 * markup is returned (owned by the caller) for downloads. */
static gchar *
poner_titulo(GtkWidget *label, const char *badge, const char *libro, gint total)
{
	/* LRMs around the Hebrew badge keep the line LTR and «1 Reyes» whole. */
	gchar *markup = g_markup_printf_escaped(
	    "\u200E<span weight=\"bold\" size=\"large\">%s</span>\u200E  "
	    "<span weight=\"bold\" size=\"large\">%s</span>  "
	    "<span alpha=\"60%%\">%d %s</span>",
	    badge, libro, total, _("palabras"));
	gtk_label_set_markup(GTK_LABEL(label), markup);
	return markup;
}

static gboolean
lectura_oscura(void)
{
	return color_es_oscuro(settings.bible_bg_color ? settings.bible_bg_color : "#ffffff");
}

/* The same blue/orange the paired clouds and the legend use. */
static const char *
color_mas_a(void)
{
	return lectura_oscura() ? "#9fcaff" : "#315c91";
}

static const char *
color_mas_b(void)
{
	return lectura_oscura() ? "#ffb47e" : "#b34412";
}

static void
setup_tree(void)
{
	gtk_widget_set_name(ui->tree, "cloud-statistics");
	/* The table wears the reading colours, like the clouds above it. */
	const char *bg = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	const char *fg = settings.bible_text_color ? settings.bible_text_color : "#222222";
	gboolean oscuro = lectura_oscura();
	const char *linea = oscuro ? "rgba(255,255,255,0.10)" : "rgba(0,0,0,0.08)";
	const char *cabecera = oscuro ? "rgba(255,255,255,0.05)" : "rgba(0,0,0,0.035)";
	const char *seleccion = oscuro ? "#2d4a6b" : "#dbe6f3";
	gchar *reglas = g_strdup_printf(
		"#cloud-statistics, #cloud-statistics.view { background-color:%s; color:%s;"
		"font-feature-settings:'tnum'; }"
		"#cloud-statistics.view:selected { background-color:%s; color:%s; }"
		"#cloud-statistics header button { background-image:none; background-color:%s;"
		"box-shadow:none; border:none; border-bottom:1px solid %s;"
		"border-radius:0; padding:6px 10px; }"
		"#cloud-statistics header button label { color:%s; font-weight:bold; }"
		"#cloud-message { font-size:15px; }",
		bg, fg, seleccion, fg, cabecera, linea, fg);
	GtkCssProvider *css = gtk_css_provider_new();
	gtk_css_provider_load_from_string(css, reglas);
	g_free(reglas);
	gtk_style_context_add_provider_for_display(gtk_widget_get_display(ui->tree),
		GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
	g_object_set_data_full(G_OBJECT(ui->tree), "matrix-css", css, g_object_unref);
	ui->tabla = elim_table_new();
	elim_table_setup_sortable(ui->tree, ui->tabla);
	ElimTextColumn palabra = elim_text_column(TCOL_PALABRA);
	ElimTextColumn entero = elim_text_column(TCOL_CUENTA_A);
	ElimTextColumn diferencia = elim_text_column(TCOL_DIF);
	ElimTextColumn porcentaje = elim_text_column(TCOL_PCT_A);

	palabra.expand = TRUE;
	entero.xalign = diferencia.xalign = porcentaje.xalign = 1.0f;
	diferencia.foreground_column = TCOL_DIF_COLOR;
	elim_table_add_sortable_column(ui->tree, _("מילה · Palabra"), &palabra,
					       TCOL_PALABRA, ELIM_TABLE_SORT_STRING);
	ui->col_a = elim_table_add_sortable_column(ui->tree, _("Cantidad"),
					       &entero, TCOL_CUENTA_A, ELIM_TABLE_SORT_INT);
	entero.column = TCOL_CUENTA_B;
	ui->col_b = elim_table_add_sortable_column(ui->tree, _("Libro B"),
					       &entero, TCOL_CUENTA_B, ELIM_TABLE_SORT_INT);
	ui->col_dif = elim_table_add_sortable_column(ui->tree, _("Dif. cantidad"),
						 &diferencia, TCOL_DIF_VALOR, ELIM_TABLE_SORT_INT);
	porcentaje.column = TCOL_PCT_A;
	ui->col_pct_a = elim_table_add_sortable_column(ui->tree, _("%"), &porcentaje,
						   TCOL_PCT_A_VALOR, ELIM_TABLE_SORT_DOUBLE);
	porcentaje.column = TCOL_PCT_B;
	ui->col_pct_b = elim_table_add_sortable_column(ui->tree, _("% B"), &porcentaje,
						   TCOL_PCT_B_VALOR, ELIM_TABLE_SORT_DOUBLE);
	porcentaje.column = TCOL_DIF_PCT;
	porcentaje.foreground_column = TCOL_DIF_PCT_COLOR;
	ui->col_dif_pct = elim_table_add_sortable_column(ui->tree, _("Dif. %"),
						      &porcentaje, TCOL_DIF_PCT_VALOR,
						      ELIM_TABLE_SORT_DOUBLE);
	/* Comparison columns appear only with a comparison. */
	gtk_column_view_column_set_visible(ui->col_b, FALSE);
	gtk_column_view_column_set_visible(ui->col_dif, FALSE);
	gtk_column_view_column_set_visible(ui->col_pct_b, FALSE);
	gtk_column_view_column_set_visible(ui->col_dif_pct, FALSE);
}

static void
set_column_visible(GtkColumnViewColumn *col, gboolean vis)
{
	gtk_column_view_column_set_visible(col, vis);
}

static void
llenar_tabla(NUBE_CONTEO *c, gboolean comparar)
{
	g_list_store_remove_all(ui->tabla);
	gtk_column_view_column_set_title(ui->col_a,
				       comparar ? c->libro : _("Cantidad"));
	if (comparar && c->libro_b)
		gtk_column_view_column_set_title(ui->col_b, c->libro_b);
	gtk_column_view_column_set_title(ui->col_pct_a,
				       comparar ? _("% A") : _("%"));

	set_column_visible(ui->col_b, comparar);
	set_column_visible(ui->col_dif, comparar);
	set_column_visible(ui->col_pct_b, comparar);
	set_column_visible(ui->col_dif_pct, comparar);

	for (guint i = 0; i < c->palabras->len; i++) {
		NUBE_PALABRA *w = g_ptr_array_index(c->palabras, i);
		ElimRow *row = elim_row_new(N_TABLA_COLS);
		gchar *label = cloud_label(w);
		gchar *dif = g_strdup_printf("%+d", w->diferencia);
		gchar *pct_a = g_strdup_printf("%.2f %%", w->pct);
		gchar *pct_b = g_strdup_printf("%.2f %%", w->pct_b);
		gchar *dif_pct = g_strdup_printf("%+.2f %%", w->dif_pct);
		const char *dif_color = w->diferencia > 0 ? color_mas_a() :
			w->diferencia < 0 ? color_mas_b() : "";
		const char *dif_pct_color = w->dif_pct > 0.001 ? color_mas_a() :
			w->dif_pct < -0.001 ? color_mas_b() : "";
		elim_row_set_string(row, TCOL_PALABRA, label);
		elim_row_set_int(row, TCOL_CUENTA_A, w->cuenta);
		elim_row_set_int(row, TCOL_CUENTA_B, w->cuenta_b);
		elim_row_set_string(row, TCOL_DIF, dif);
		elim_row_set_string(row, TCOL_PCT_A, pct_a);
		elim_row_set_string(row, TCOL_PCT_B, pct_b);
		elim_row_set_string(row, TCOL_DIF_PCT, dif_pct);
		elim_row_set_int(row, TCOL_DIF_VALOR, w->diferencia);
		elim_row_set_double(row, TCOL_PCT_A_VALOR, w->pct);
		elim_row_set_double(row, TCOL_PCT_B_VALOR, w->pct_b);
		elim_row_set_double(row, TCOL_DIF_PCT_VALOR, w->dif_pct);
		elim_row_set_string(row, TCOL_DIF_COLOR, dif_color);
		elim_row_set_string(row, TCOL_DIF_PCT_COLOR, dif_pct_color);
		g_list_store_append(ui->tabla, row);
		g_object_unref(row);
		g_free(dif);
		g_free(pct_a);
		g_free(pct_b);
		g_free(dif_pct);
		g_free(label);
	}
}

/* The canonical name of COMBO's book, or NULL. AVISAR warns about an empty
 * or unknown book; automatic updates stay silent. */
static gchar *
resolver_libro(GtkWidget *combo, gboolean avisar)
{
	const gchar *texto = combo_texto(combo);
	if (!texto || !*texto) {
		if (avisar)
			gui_generic_warning(_("Elige un libro de la Biblia."));
		return NULL;
	}
	char *nombre = main_nube_resolver_libro(settings.MainWindowModule, texto);
	if (!nombre) {
		if (avisar)
			gui_generic_warning(_("No se encontró ese libro de la Biblia."));
		return NULL;
	}
	if (g_strcmp0(texto, nombre) != 0)
		combo_set_texto(combo, nombre);
	return nombre;
}

static gboolean on_pendiente(gpointer user_data);

/* Put a random book other than EXCLUDE in COMBO. The versification lists
 * books a module may lack, so an empty random book is replaced a few
 * times; the last try is the book being read. */
static void
poner_al_azar(GtkWidget *combo, const gchar *exclude, gboolean ultimo_actual)
{
	gchar *libro = NULL;
	if (ultimo_actual && settings.MainWindowModule && settings.currentverse)
		libro = main_nube_libro_de_clave(settings.MainWindowModule,
						 settings.currentverse);
	if (!libro || (exclude && g_utf8_collate(libro, exclude) == 0)) {
		g_free(libro);
		libro = cloud_random_book(GTK_TREE_MODEL(ui->libros), COL_NOMBRE, exclude);
	}
	if (libro) {
		ui->poniendo = TRUE;
		combo_set_texto(combo, libro);
		ui->poniendo = FALSE;
		if (combo == ui->combo_libro_b)
			ui->libro_b_puesto = TRUE;
	}
	g_free(libro);
}

/* The shape of one cloud panel (width / height), from the cloud area:
 * one or two panels 16 px apart under a title line. 0 when unknown. */
static double
aspecto_panel(gboolean pair)
{
	int area_w = gtk_widget_get_allocated_width(ui->cloud_stack);
	int area_h = gtk_widget_get_allocated_height(ui->cloud_stack) - 44;
	if (area_w <= 1 || area_h <= 1)
		return 0;
	return (pair ? (area_w - 16) / 2.0 : area_w) / (double)area_h;
}

/* Lay out and show the clouds of the current count for the panels' shape
 * (the canvases may not be allocated yet, so the shape comes from the
 * cloud area). */
static void
dibujar_nubes(void)
{
	NUBE_CONTEO *c = ui->conteo;
	const char *background = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	CloudLayout *cloud, *cloud_b = NULL;
	ui->aspecto = aspecto_panel(ui->par);
	if (ui->par)
		cloud_build_pair(ui->canvas, ui->canvas_b, c, background, ui->aspecto,
				 &cloud, &cloud_b);
	else
		cloud = cloud_build(ui->canvas, c, FALSE, background, ui->aspecto);
	g_free(ui->titulo_a);
	g_free(ui->titulo_b);
	ui->titulo_a = poner_titulo(ui->title_a, "א", c->libro, c->total);
	ui->titulo_b = ui->par ? poner_titulo(ui->title_b, "ב", c->libro_b, c->total_b) : NULL;
	gtk_widget_set_visible(ui->panel_b, ui->par);
	g_object_set_data_full(G_OBJECT(ui->canvas_b), "cloud", cloud_b, cloud_free);
	g_object_set_data_full(G_OBJECT(ui->canvas), "cloud", cloud, cloud_free);
	gtk_stack_set_visible_child_name(GTK_STACK(ui->cloud_stack), "cloud");
	gtk_widget_queue_draw(ui->canvas);
	gtk_widget_queue_draw(ui->canvas_b);
}

/* Count and show the cloud(s) for the books in the dialog. Nothing is
 * recounted when the same books are already shown. */
static void
actualizar(gboolean avisar)
{
	if (!ui || ui->ocupado)
		return;
	if (!settings.MainWindowModule || !settings.havebible) {
		if (avisar)
			gui_generic_warning(_("Abre un texto bíblico para usar la nube de palabras."));
		return;
	}
	/* The layout takes the shape of the cloud area: wait for its size. */
	if (gtk_widget_get_allocated_width(ui->cloud_stack) <= 1) {
		ui->pendiente = TRUE;
		return;
	}
	ui->pendiente = FALSE;

	gboolean comparar = gtk_check_button_get_active(
	    GTK_CHECK_BUTTON(ui->chk_comparar));

	gchar *libro_a = resolver_libro(ui->combo_libro, avisar);
	if (!libro_a)
		return;

	gchar *libro_b = NULL;
	if (comparar) {
		libro_b = resolver_libro(ui->combo_libro_b, avisar);
		if (!libro_b) {
			g_free(libro_a);
			return;
		}
		if (g_utf8_collate(libro_a, libro_b) == 0) {
			if (avisar)
				gui_generic_warning(_("Elige dos libros distintos para comparar."));
			g_free(libro_a);
			g_free(libro_b);
			return;
		}
	}

	gchar *clave = libro_b ? g_strdup_printf("%s\n%s", libro_a, libro_b) :
				 g_strdup(libro_a);
	if (g_strcmp0(clave, ui->mostrado) == 0) {
		g_free(clave);
		g_free(libro_a);
		g_free(libro_b);
		return;
	}

	/* Counts are kept per module and books: going back to a pair, or
	 * unticking «Comparar», shows again without recounting. */
	gchar *clave_cache = g_strconcat(settings.MainWindowModule, "\n", clave, NULL);
	NUBE_CONTEO *c = g_hash_table_lookup(ui->cache, clave_cache);
	if (c) {
		g_free(clave_cache);
		clave_cache = NULL;
	} else {
		ui->ocupado = TRUE;
		gtk_label_set_text(GTK_LABEL(ui->lbl_resumen),
				   _("Contando palabras…"));
		while (g_main_context_pending(NULL))
			g_main_context_iteration(NULL, FALSE);
		if (!ui) {	/* closed while counting */
			g_free(clave_cache);
			g_free(clave);
			g_free(libro_a);
			g_free(libro_b);
			return;
		}
		c = main_nube_contar(settings.MainWindowModule,
				     libro_a, libro_b, NUBE_LIMITE);
		ui->ocupado = FALSE;
	}
	gboolean vacio_a = !c || c->total == 0;
	gboolean vacio_b = c && comparar && c->total_b == 0;
	if ((vacio_a || vacio_b) && ui->reintentos > 0) {
		/* A random book without text here: draw another one. */
		ui->reintentos--;
		if (vacio_a)
			poner_al_azar(ui->combo_libro, libro_b, ui->reintentos == 0);
		else
			poner_al_azar(ui->combo_libro_b, libro_a, FALSE);
		ui->pendiente = TRUE;
		g_idle_add(on_pendiente, NULL);
		if (clave_cache)
			main_nube_conteo_free(c);
		g_free(clave_cache);
		g_free(clave);
		g_free(libro_a);
		g_free(libro_b);
		return;
	}
	if (vacio_a || vacio_b) {
		if (avisar)
			gui_generic_warning(_("No se pudo leer el texto de ese libro."));
		mostrar_mensaje(_("No hay palabras para mostrar."));
		g_list_store_remove_all(ui->tabla);
		gtk_label_set_text(GTK_LABEL(ui->lbl_resumen), "");
		gtk_widget_set_sensitive(ui->btn_descargar, FALSE);
		g_free(ui->mostrado);
		ui->mostrado = NULL;
		ui->conteo = NULL;
		if (clave_cache)
			main_nube_conteo_free(c);
		g_free(clave_cache);
		g_free(clave);
		g_free(libro_a);
		g_free(libro_b);
		return;
	}

	gchar *resumen;
	if (comparar && c->libro_b) {
		resumen = g_strdup_printf(
		    _("%s: %d palabras · %s: %d palabras  (sin artículos ni palabras vacías)"),
		    c->libro, c->total, c->libro_b, c->total_b);
	} else {
		resumen = g_strdup_printf(
		    _("%s: %d palabras, %d distintas  (sin artículos ni palabras vacías)"),
		    c->libro, c->total, c->unicas);
	}
	gtk_label_set_text(GTK_LABEL(ui->lbl_resumen), resumen);
	g_free(resumen);

	if (clave_cache) {
		if (g_hash_table_size(ui->cache) >= 16)
			g_hash_table_remove_all(ui->cache);
		g_hash_table_insert(ui->cache, clave_cache, c);
	}
	ui->conteo = c;
	ui->par = comparar && c->libro_b;
	dibujar_nubes();
	llenar_tabla(c, ui->par);
	gtk_widget_set_sensitive(ui->btn_descargar, TRUE);
	g_free(ui->mostrado);
	ui->mostrado = clave;

	g_free(libro_a);
	g_free(libro_b);
}

/* TEXT names a book of the list exactly (ignoring case). */
static gboolean
es_libro(const gchar *texto)
{
	if (!texto || !*texto)
		return FALSE;
	gchar *buscado = g_utf8_casefold(texto, -1);
	gboolean hallado = FALSE;
	GtkTreeModel *model = GTK_TREE_MODEL(ui->libros);
	GtkTreeIter iter;
	for (gboolean ok = gtk_tree_model_get_iter_first(model, &iter); ok && !hallado;
	     ok = gtk_tree_model_iter_next(model, &iter)) {
		gchar *nombre = NULL;
		gtk_tree_model_get(model, &iter, COL_NOMBRE, &nombre, -1);
		gchar *plegado = nombre ? g_utf8_casefold(nombre, -1) : NULL;
		hallado = plegado && g_utf8_collate(plegado, buscado) == 0;
		g_free(plegado);
		g_free(nombre);
	}
	g_free(buscado);
	return hallado;
}

static gboolean
on_espera(gpointer user_data)
{
	(void)user_data;
	if (!ui)
		return G_SOURCE_REMOVE;
	if (ui->ocupado)
		return G_SOURCE_CONTINUE;
	ui->espera = 0;
	gboolean comparar = gtk_check_button_get_active(
	    GTK_CHECK_BUTTON(ui->chk_comparar));
	if (es_libro(combo_texto(ui->combo_libro)) &&
	    (!comparar || es_libro(combo_texto(ui->combo_libro_b))))
		actualizar(FALSE);
	return G_SOURCE_REMOVE;
}

/* A book chosen from the list updates the cloud shortly after. */
static void
on_entry_changed(GObject *combo, GParamSpec *pspec, gpointer user_data)
{
	(void)pspec;
	(void)user_data;
	if (!ui->poniendo) {
		ui->reintentos = 0;	/* the reader's own choice stands */
		if ((GtkWidget *)combo == ui->combo_libro_b)
			ui->libro_b_puesto = TRUE;
	}
	if (ui->espera)
		g_source_remove(ui->espera);
	ui->espera = g_timeout_add(350, on_espera, NULL);
}

static void
on_comparar_toggled(GtkCheckButton *btn, gpointer user_data)
{
	(void)user_data;
	gboolean on = gtk_check_button_get_active(btn);
	gtk_widget_set_sensitive(ui->combo_libro_b, on);
	if (on) {
		/* Start from a random book other than book A; the reader can
		 * still pick another one. */
		const gchar *actual = combo_texto(ui->combo_libro_b);
		const gchar *libro_a = combo_texto(ui->combo_libro);
		if (!ui->libro_b_puesto || !actual || !*actual ||
		    g_utf8_collate(actual, libro_a) == 0) {
			poner_al_azar(ui->combo_libro_b, libro_a, FALSE);
			ui->reintentos = 8;
		}
	}
	actualizar(FALSE);
}

static gboolean
on_pendiente(gpointer user_data)
{
	(void)user_data;
	if (ui && ui->pendiente)
		actualizar(FALSE);
	return G_SOURCE_REMOVE;
}

static gboolean
on_espera_tam(gpointer user_data)
{
	(void)user_data;
	if (!ui)
		return G_SOURCE_REMOVE;
	ui->espera_tam = 0;
	if (ui->conteo && !ui->ocupado)
		dibujar_nubes();
	return G_SOURCE_REMOVE;
}

/* The first cloud waits for the dialog to give the cloud area a size.
 * Later, a clearly different panel shape lays the clouds out again (from
 * the kept count) instead of only scaling them. */
static void
on_area_allocate(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data)
{
	(void)widget;
	(void)user_data;
	if (!ui || allocation->width <= 1)
		return;
	if (ui->pendiente) {
		g_idle_add(on_pendiente, NULL);
		return;
	}
	double aspecto = aspecto_panel(ui->par);
	if (ui->conteo && ui->aspecto > 0 && aspecto > 0 &&
	    fabs(aspecto / ui->aspecto - 1) > 0.12) {
		if (ui->espera_tam)
			g_source_remove(ui->espera_tam);
		ui->espera_tam = g_timeout_add(250, on_espera_tam, NULL);
	}
}

/* A file name without characters file systems reject. */
static gchar *
nombre_archivo(const gchar *base, const gchar *extension)
{
	gchar *limpio = g_strdup(base);
	g_strdelimit(limpio, "/\\:*?\"<>|", '-');
	gchar *nombre = g_strconcat(limpio, extension, NULL);
	g_free(limpio);
	return nombre;
}

static void
on_descargar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	CloudLayout *a = g_object_get_data(G_OBJECT(ui->canvas), "cloud");
	CloudLayout *b = gtk_widget_get_visible(ui->panel_b) ?
		g_object_get_data(G_OBJECT(ui->canvas_b), "cloud") : NULL;
	if (!a || !ui->mostrado)
		return;

	GtkWidget *chooser = gtk_file_chooser_dialog_new(
	    _("Descargar nube de palabras"), GTK_WINDOW(ui->dialog),
	    GTK_FILE_CHOOSER_ACTION_SAVE,
	    _("_Cancelar"), GTK_RESPONSE_CANCEL,
	    _("_Guardar"), GTK_RESPONSE_ACCEPT, NULL);
	GtkFileChooser *fc = GTK_FILE_CHOOSER(chooser);
	const gchar *imagenes = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
	gui_file_chooser_set_current_folder(fc, imagenes ? imagenes : g_get_home_dir());
	GtkFileFilter *png = gtk_file_filter_new();
	gtk_file_filter_set_name(png, _("Imagen PNG"));
	gtk_file_filter_add_pattern(png, "*.png");
	gtk_file_chooser_add_filter(fc, png);
	GtkFileFilter *svg = gtk_file_filter_new();
	gtk_file_filter_set_name(svg, _("Imagen vectorial SVG"));
	gtk_file_filter_add_pattern(svg, "*.svg");
	gtk_file_chooser_add_filter(fc, svg);

	gchar **libros = g_strsplit(ui->mostrado, "\n", 2);
	gchar *base = libros[1] ?
		g_strdup_printf(_("Nube de palabras - %s y %s"), libros[0], libros[1]) :
		g_strdup_printf(_("Nube de palabras - %s"), libros[0]);
	g_strfreev(libros);
	gchar *sugerido = nombre_archivo(base, ".png");
	gtk_file_chooser_set_current_name(fc, sugerido);
	g_free(sugerido);
	g_free(base);

	gui_fit_dialog_to_screen(GTK_WINDOW(chooser));
	if (gui_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *ruta = gui_file_chooser_get_filename(fc);
		gboolean es_svg = gtk_file_chooser_get_filter(fc) == svg;
		gchar *lower = g_ascii_strdown(ruta, -1);
		if (!g_str_has_suffix(lower, ".png") && !g_str_has_suffix(lower, ".svg")) {
			gchar *con = g_strconcat(ruta, es_svg ? ".svg" : ".png", NULL);
			g_free(ruta);
			ruta = con;
		}
		g_free(lower);
		GError *error = NULL;
		const char *texto = settings.bible_text_color ?
			settings.bible_text_color : "#222222";
		if (!cloud_export(ruta, a, ui->titulo_a, b, b ? ui->titulo_b : NULL,
				  texto, &error)) {
			gchar *msg = g_strdup_printf(_("No se pudo guardar la imagen: %s"),
						     error ? error->message : "");
			gui_generic_warning(msg);
			g_free(msg);
			g_clear_error(&error);
		}
		g_free(ruta);
	}
	gui_widget_destroy(chooser);
}

static void
on_cerrar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	if (ui && ui->dialog)
		gui_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	(void)user_data;
	if (!ui)
		return;
	if (ui->espera)
		g_source_remove(ui->espera);
	if (ui->espera_tam)
		g_source_remove(ui->espera_tam);
	g_hash_table_destroy(ui->cache);
	g_free(ui->mostrado);
	g_free(ui->titulo_a);
	g_free(ui->titulo_b);
	GtkCssProvider *css = g_object_get_data(G_OBJECT(ui->tree), "matrix-css");
	if (css)
		gtk_style_context_remove_provider_for_display(gtk_widget_get_display(ui->tree),
			GTK_STYLE_PROVIDER(css));
	if (ui->libros)
		g_object_unref(ui->libros);
	g_clear_pointer(&ui->abreviaturas, g_hash_table_destroy);
	if (ui->tabla)
		g_object_unref(ui->tabla);
	g_free(ui);
	ui = NULL;
}

static void
crear_dialogo(void)
{
	GtkBuilder *gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(gxml,
					   "/org/xiphos/ui/nube-palabras.gtkbuilder",
					   NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el diálogo Nube de palabras."));
		return;
	}

	ui = g_new0(NUBE_UI, 1);
	ui->cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
					  (GDestroyNotify)main_nube_conteo_free);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_nube_palabras");
	ui->combo_libro = UI_GET_ITEM(gxml, "combo_libro");
	ui->combo_libro_b = UI_GET_ITEM(gxml, "combo_libro_b");
	ui->chk_comparar = UI_GET_ITEM(gxml, "chk_comparar");
	ui->btn_descargar = UI_GET_ITEM(gxml, "btn_descargar");
	ui->btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");
	ui->lbl_resumen = UI_GET_ITEM(gxml, "lbl_resumen");
	ui->box_nube = UI_GET_ITEM(gxml, "box_nube");
	ui->tree = UI_GET_ITEM(gxml, "tree_palabras");
	ui->scroll_tabla = UI_GET_ITEM(gxml, "scroll_tabla");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    widgets.app ? GTK_WINDOW(widgets.app) : NULL);
	/* Wayland owns placement. Fit inside the already allocated parent,
	 * whose height respects the compositor's bar and workspace gaps. */
	GdkDisplay *display = gtk_widget_get_display(ui->dialog);
	GdkSurface *parent_surface = widgets.app && gtk_widget_get_realized(widgets.app)
		? gtk_native_get_surface(GTK_NATIVE(widgets.app)) : NULL;
	GdkMonitor *monitor = parent_surface
		? gdk_display_get_monitor_at_surface(display, parent_surface) : NULL;
	GdkRectangle area = { 0, 0, 1024, 768 };
	if (monitor) gdk_monitor_get_geometry(monitor, &area);
	if (widgets.app && gtk_widget_get_height(widgets.app) > 1)
		area.height = MIN(area.height, gtk_widget_get_height(widgets.app));
	int dialog_height = MIN(720, (int)(area.height * 0.90));
	gtk_window_set_default_size(GTK_WINDOW(ui->dialog), MIN(960, (int)(area.width * 0.90)), dialog_height);
	gtk_window_set_resizable(GTK_WINDOW(ui->dialog), TRUE);
	gtk_paned_set_position(GTK_PANED(UI_GET_ITEM(gxml, "paned")), MAX(160, dialog_height - 310));

	ui->libros = gtk_list_store_new(N_LIBRO_COLS,
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	ui->abreviaturas = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
						 g_free);
	poblar_libros(ui->libros, ui->abreviaturas, ui->combo_libro,
		      ui->combo_libro_b);

	/* Open on a random book, already drawn (see on_area_allocate). */
	poner_al_azar(ui->combo_libro, NULL, FALSE);
	ui->reintentos = 8;
	ui->pendiente = TRUE;

	ui->cloud_stack = gtk_stack_new();
	gtk_widget_show(ui->cloud_stack);
	gui_box_pack(GTK_BOX(ui->box_nube), ui->cloud_stack, TRUE, TRUE, 0);
	ui->canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(ui->canvas, 240, 160);
	gtk_widget_set_hexpand(ui->canvas, TRUE);
	gtk_widget_set_vexpand(ui->canvas, TRUE);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ui->canvas), cloud_draw,
				       NULL, NULL);
	gtk_widget_show(ui->canvas);
	GtkWidget *panels = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_box_set_homogeneous(GTK_BOX(panels), TRUE);
	GtkWidget *panel_a = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	ui->panel_b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	ui->title_a = gtk_label_new(NULL);
	ui->title_b = gtk_label_new(NULL);
	gui_box_pack(GTK_BOX(panel_a), ui->title_a, FALSE, FALSE, 8);
	gui_box_pack(GTK_BOX(panel_a), ui->canvas, TRUE, TRUE, 0);
	ui->canvas_b = gtk_drawing_area_new();
	gtk_widget_set_size_request(ui->canvas_b, 240, 160);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ui->canvas_b), cloud_draw,
				       NULL, NULL);
	gui_box_pack(GTK_BOX(ui->panel_b), ui->title_b, FALSE, FALSE, 8);
	gui_box_pack(GTK_BOX(ui->panel_b), ui->canvas_b, TRUE, TRUE, 0);
	gui_box_pack(GTK_BOX(panels), panel_a, TRUE, TRUE, 0);
	gui_box_pack(GTK_BOX(panels), ui->panel_b, TRUE, TRUE, 0);
	gtk_widget_show(panels);
	gtk_widget_hide(ui->panel_b);
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), panels, "cloud");
	ui->mensaje = gtk_label_new(NULL);
	gtk_widget_set_name(ui->mensaje, "cloud-message");
	gtk_label_set_wrap(GTK_LABEL(ui->mensaje), TRUE);
	gtk_label_set_justify(GTK_LABEL(ui->mensaje), GTK_JUSTIFY_CENTER);
	gtk_widget_add_css_class(ui->mensaje, "dim-label");
	gtk_widget_show(ui->mensaje);
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), ui->mensaje, "message");

	setup_tree();
	mostrar_mensaje(_("Elige un libro de la Biblia para ver sus palabras más usadas."));
	gtk_label_set_text(GTK_LABEL(ui->lbl_resumen), "");
	gtk_widget_add_css_class(ui->lbl_resumen, "dim-label");
	gtk_widget_set_sensitive(ui->btn_descargar, FALSE);

	g_signal_connect(ui->btn_descargar, "clicked", G_CALLBACK(on_descargar), NULL);
	gui_widget_watch_size(ui->cloud_stack, on_area_allocate, NULL);
	g_signal_connect(ui->combo_libro, "notify::selected",
			 G_CALLBACK(on_entry_changed), NULL);
	g_signal_connect(ui->combo_libro_b, "notify::selected",
			 G_CALLBACK(on_entry_changed), NULL);
	g_signal_connect(ui->btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->chk_comparar, "toggled", G_CALLBACK(on_comparar_toggled), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);
}

void
gui_nube_palabras_dialog(void)
{
	if (ui && ui->dialog) {
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}
	crear_dialogo();
	if (ui && ui->dialog)
		gtk_widget_show(ui->dialog);
}
