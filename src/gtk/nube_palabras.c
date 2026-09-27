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
	GtkListStore *tabla;
	GtkTreeViewColumn *col_a;
	GtkTreeViewColumn *col_b;
	GtkTreeViewColumn *col_dif;
	GtkTreeViewColumn *col_pct_a;
	GtkTreeViewColumn *col_pct_b;
	GtkTreeViewColumn *col_dif_pct;
	guint espera;		/* debounce after typing a book name */
	gboolean ocupado;	/* counting; the loop below may re-enter */
	gboolean pendiente;	/* update once the cloud area has a size */
	gchar *mostrado;	/* what is shown: "A\nB" or "A" */
	gchar *titulo_a;	/* title markup, reused for downloads */
	gchar *titulo_b;
	gint reintentos;	/* random picks left if a random book is empty */
	gboolean poniendo;	/* the dialog, not the reader, sets a book */
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

static GtkWidget *
combo_entry(GtkWidget *combo)
{
	return gtk_bin_get_child(GTK_BIN(combo));
}

static const gchar *
combo_texto(GtkWidget *combo)
{
	GtkWidget *entry = combo_entry(combo);
	return gtk_entry_get_text(GTK_ENTRY(entry));
}

static void
combo_set_texto(GtkWidget *combo, const gchar *texto)
{
	gtk_entry_set_text(GTK_ENTRY(combo_entry(combo)), texto ? texto : "");
}

static gboolean
completion_match(GtkEntryCompletion *completion,
		 const gchar *key,
		 GtkTreeIter *iter,
		 gpointer user_data)
{
	GtkTreeModel *model = gtk_entry_completion_get_model(completion);
	gchar *nombre = NULL;
	gchar *abrev = NULL;
	gtk_tree_model_get(model, iter,
			   COL_NOMBRE, &nombre,
			   COL_ABREV, &abrev,
			   -1);
	gboolean ok = main_nube_texto_coincide(nombre, key) ||
		      main_nube_texto_coincide(abrev, key);
	g_free(nombre);
	g_free(abrev);
	return ok;
}

static void
poblar_libros(GtkListStore *store, GtkWidget *combo)
{
	gtk_list_store_clear(store);
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
	}
	main_nube_lista_libros_free(lista);

	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), COL_NOMBRE);

	gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
				       "text", COL_NOMBRE, NULL);

	GtkWidget *entry = combo_entry(combo);
	GtkEntryCompletion *comp = gtk_entry_completion_new();
	gtk_entry_completion_set_model(comp, GTK_TREE_MODEL(store));
	gtk_entry_completion_set_text_column(comp, COL_NOMBRE);
	gtk_entry_completion_set_minimum_key_length(comp, 1);
	gtk_entry_completion_set_popup_completion(comp, TRUE);
	gtk_entry_completion_set_inline_completion(comp, FALSE);
	gtk_entry_completion_set_match_func(comp, completion_match, NULL, NULL);
	gtk_entry_set_completion(GTK_ENTRY(entry), comp);
	g_object_unref(comp);
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
int_cell(GtkTreeViewColumn *col,
	 GtkCellRenderer *cell,
	 GtkTreeModel *model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	gint column = GPOINTER_TO_INT(data);
	gint v = 0;
	gtk_tree_model_get(model, iter, column, &v, -1);
	gchar *t = g_strdup_printf("%d", v);
	g_object_set(cell, "text", t, "xalign", 1.0, NULL);
	g_free(t);
}

static void
dif_cell(GtkTreeViewColumn *col,
	 GtkCellRenderer *cell,
	 GtkTreeModel *model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	gint v = 0;
	gtk_tree_model_get(model, iter, TCOL_DIF, &v, -1);
	gchar *t = g_strdup_printf("%+d", v);
	const char *fg = NULL;
	if (v > 0)
		fg = color_mas_a();
	else if (v < 0)
		fg = color_mas_b();
	g_object_set(cell, "text", t, "xalign", 1.0, "foreground", fg, NULL);
	g_free(t);
}

static void
pct_cell(GtkTreeViewColumn *col,
	 GtkCellRenderer *cell,
	 GtkTreeModel *model,
	 GtkTreeIter *iter,
	 gpointer data)
{
	gint column = GPOINTER_TO_INT(data);
	gdouble v = 0;
	gtk_tree_model_get(model, iter, column, &v, -1);
	gchar *t;
	if (column == TCOL_DIF_PCT)
		t = g_strdup_printf("%+.2f %%", v);
	else
		t = g_strdup_printf("%.2f %%", v);
	const char *fg = NULL;
	if (column == TCOL_DIF_PCT) {
		if (v > 0.001)
			fg = color_mas_a();
		else if (v < -0.001)
			fg = color_mas_b();
	}
	g_object_set(cell, "text", t, "xalign", 1.0, "foreground", fg, NULL);
	g_free(t);
}

static GtkTreeViewColumn *
add_col(GtkTreeView *view,
	const gchar *title,
	gint sort_id,
	GtkTreeCellDataFunc func,
	gpointer func_data)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
	    title, cell, NULL);
	gtk_tree_view_column_set_cell_data_func(col, cell, func, func_data, NULL);
	gtk_tree_view_column_set_sort_column_id(col, sort_id);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_column_set_expand(col, FALSE);
	gtk_tree_view_append_column(view, col);
	return col;
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
	gtk_css_provider_load_from_data(css, reglas, -1, NULL);
	g_free(reglas);
	gtk_style_context_add_provider_for_screen(gtk_widget_get_screen(ui->tree),
		GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1);
	g_object_set_data_full(G_OBJECT(ui->tree), "matrix-css", css, g_object_unref);
	ui->tabla = gtk_list_store_new(N_TABLA_COLS,
				       G_TYPE_STRING,
				       G_TYPE_INT,
				       G_TYPE_INT,
				       G_TYPE_INT,
				       G_TYPE_DOUBLE,
				       G_TYPE_DOUBLE,
				       G_TYPE_DOUBLE);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree), GTK_TREE_MODEL(ui->tabla));

	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
	    _("מילה · Palabra"), cell, "text", TCOL_PALABRA, NULL);
	gtk_tree_view_column_set_sort_column_id(col, TCOL_PALABRA);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_column_set_resizable(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree), col);

	ui->col_a = add_col(GTK_TREE_VIEW(ui->tree), _("Cantidad"),
			    TCOL_CUENTA_A, int_cell, GINT_TO_POINTER(TCOL_CUENTA_A));
	ui->col_b = add_col(GTK_TREE_VIEW(ui->tree), _("Libro B"),
			    TCOL_CUENTA_B, int_cell, GINT_TO_POINTER(TCOL_CUENTA_B));
	ui->col_dif = add_col(GTK_TREE_VIEW(ui->tree), _("Dif. cantidad"),
			      TCOL_DIF, dif_cell, NULL);
	ui->col_pct_a = add_col(GTK_TREE_VIEW(ui->tree), _("%"),
				TCOL_PCT_A, pct_cell, GINT_TO_POINTER(TCOL_PCT_A));
	ui->col_pct_b = add_col(GTK_TREE_VIEW(ui->tree), _("% B"),
				TCOL_PCT_B, pct_cell, GINT_TO_POINTER(TCOL_PCT_B));
	ui->col_dif_pct = add_col(GTK_TREE_VIEW(ui->tree), _("Dif. %"),
				  TCOL_DIF_PCT, pct_cell, GINT_TO_POINTER(TCOL_DIF_PCT));
	/* Comparison columns appear only with a comparison. */
	gtk_tree_view_column_set_visible(ui->col_b, FALSE);
	gtk_tree_view_column_set_visible(ui->col_dif, FALSE);
	gtk_tree_view_column_set_visible(ui->col_pct_b, FALSE);
	gtk_tree_view_column_set_visible(ui->col_dif_pct, FALSE);
}

static void
set_column_visible(GtkTreeViewColumn *col, gboolean vis)
{
	gtk_tree_view_column_set_visible(col, vis);
}

static void
llenar_tabla(NUBE_CONTEO *c, gboolean comparar)
{
	gtk_list_store_clear(ui->tabla);
	gtk_tree_view_column_set_title(ui->col_a,
				       comparar ? c->libro : _("Cantidad"));
	if (comparar && c->libro_b)
		gtk_tree_view_column_set_title(ui->col_b, c->libro_b);
	gtk_tree_view_column_set_title(ui->col_pct_a,
				       comparar ? _("% A") : _("%"));

	set_column_visible(ui->col_b, comparar);
	set_column_visible(ui->col_dif, comparar);
	set_column_visible(ui->col_pct_b, comparar);
	set_column_visible(ui->col_dif_pct, comparar);

	for (guint i = 0; i < c->palabras->len; i++) {
		NUBE_PALABRA *w = g_ptr_array_index(c->palabras, i);
		GtkTreeIter iter;
		gchar *label = cloud_label(w);
		gtk_list_store_append(ui->tabla, &iter);
		gtk_list_store_set(ui->tabla, &iter,
				   TCOL_PALABRA, label,
				   TCOL_CUENTA_A, w->cuenta,
				   TCOL_CUENTA_B, w->cuenta_b,
				   TCOL_DIF, w->diferencia,
				   TCOL_PCT_A, w->pct,
				   TCOL_PCT_B, w->pct_b,
				   TCOL_DIF_PCT, w->dif_pct,
				   -1);
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
			gui_generic_warning(_("Escribe o elige un libro de la Biblia."));
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

	gboolean comparar = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_comparar));

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
		while (gtk_events_pending())
			gtk_main_iteration();
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
		gtk_list_store_clear(ui->tabla);
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
	gboolean comparar = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_comparar));
	if (es_libro(combo_texto(ui->combo_libro)) &&
	    (!comparar || es_libro(combo_texto(ui->combo_libro_b))))
		actualizar(FALSE);
	return G_SOURCE_REMOVE;
}

/* A book chosen from the list, completed or typed in full updates the
 * cloud shortly after; partial typing does not. */
static void
on_entry_changed(GtkEditable *editable, gpointer user_data)
{
	(void)editable;
	(void)user_data;
	if (!ui->poniendo)
		ui->reintentos = 0;	/* the reader's own choice stands */
	if (ui->espera)
		g_source_remove(ui->espera);
	ui->espera = g_timeout_add(350, on_espera, NULL);
}

static void
on_comparar_toggled(GtkToggleButton *btn, gpointer user_data)
{
	(void)user_data;
	gboolean on = gtk_toggle_button_get_active(btn);
	gtk_widget_set_sensitive(ui->combo_libro_b, on);
	if (on) {
		/* Start from a random book other than book A; the reader can
		 * still type or pick another one. */
		const gchar *actual = combo_texto(ui->combo_libro_b);
		const gchar *libro_a = combo_texto(ui->combo_libro);
		if (!actual || !*actual || g_utf8_collate(actual, libro_a) == 0) {
			poner_al_azar(ui->combo_libro_b, libro_a, FALSE);
			ui->reintentos = 8;
		}
	}
	actualizar(FALSE);
}

static void
on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	(void)entry;
	(void)user_data;
	actualizar(TRUE);
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
	gtk_file_chooser_set_do_overwrite_confirmation(fc, TRUE);
	const gchar *imagenes = g_get_user_special_dir(G_USER_DIRECTORY_PICTURES);
	gtk_file_chooser_set_current_folder(fc, imagenes ? imagenes : g_get_home_dir());
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
	if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *ruta = gtk_file_chooser_get_filename(fc);
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
	gtk_widget_destroy(chooser);
}

static void
on_cerrar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	if (ui && ui->dialog)
		gtk_widget_destroy(ui->dialog);
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
		gtk_style_context_remove_provider_for_screen(gtk_widget_get_screen(ui->tree),
			GTK_STYLE_PROVIDER(css));
	if (ui->libros)
		g_object_unref(ui->libros);
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
	GdkWindow *parent_window = widgets.app ? gtk_widget_get_window(widgets.app) : NULL;
	GdkMonitor *monitor = parent_window ? gdk_display_get_monitor_at_window(display, parent_window) :
		gdk_display_get_monitor(display, 0);
	GdkRectangle area = { 0, 0, 1024, 768 };
	if (monitor) gdk_monitor_get_workarea(monitor, &area);
	if (widgets.app && gtk_widget_get_allocated_height(widgets.app) > 1)
		area.height = MIN(area.height, gtk_widget_get_allocated_height(widgets.app));
	int dialog_height = MIN(720, (int)(area.height * 0.90));
	gtk_window_set_default_size(GTK_WINDOW(ui->dialog), MIN(960, (int)(area.width * 0.90)), dialog_height);
	gtk_window_set_resizable(GTK_WINDOW(ui->dialog), TRUE);
	gtk_paned_set_position(GTK_PANED(UI_GET_ITEM(gxml, "paned")), MAX(160, dialog_height - 310));

	ui->libros = gtk_list_store_new(N_LIBRO_COLS,
					G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	poblar_libros(ui->libros, ui->combo_libro);
	/* Reutiliza el mismo modelo: no volver a poblar (eso vaciaría la lista). */
	{
		GtkListStore *store = ui->libros;
		GtkWidget *combo = ui->combo_libro_b;
		gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
		gtk_combo_box_set_entry_text_column(GTK_COMBO_BOX(combo), COL_NOMBRE);
		gtk_cell_layout_clear(GTK_CELL_LAYOUT(combo));
		GtkCellRenderer *cell = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), cell, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), cell,
					       "text", COL_NOMBRE, NULL);
		GtkWidget *entry = combo_entry(combo);
		GtkEntryCompletion *comp = gtk_entry_completion_new();
		gtk_entry_completion_set_model(comp, GTK_TREE_MODEL(store));
		gtk_entry_completion_set_text_column(comp, COL_NOMBRE);
		gtk_entry_completion_set_minimum_key_length(comp, 1);
		gtk_entry_completion_set_popup_completion(comp, TRUE);
		gtk_entry_completion_set_inline_completion(comp, FALSE);
		gtk_entry_completion_set_match_func(comp, completion_match, NULL, NULL);
		gtk_entry_set_completion(GTK_ENTRY(entry), comp);
		g_object_unref(comp);
	}

	gtk_entry_set_placeholder_text(GTK_ENTRY(combo_entry(ui->combo_libro)),
				       _("Escribe o elige un libro"));
	gtk_entry_set_placeholder_text(GTK_ENTRY(combo_entry(ui->combo_libro_b)),
				       _("Libro para comparar"));

	/* Open on a random book, already drawn (see on_area_allocate). */
	poner_al_azar(ui->combo_libro, NULL, FALSE);
	ui->reintentos = 8;
	ui->pendiente = TRUE;

	ui->cloud_stack = gtk_stack_new();
	gtk_widget_show(ui->cloud_stack);
	gtk_box_pack_start(GTK_BOX(ui->box_nube), ui->cloud_stack, TRUE, TRUE, 0);
	ui->canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(ui->canvas, 240, 160);
	gtk_widget_set_hexpand(ui->canvas, TRUE);
	gtk_widget_set_vexpand(ui->canvas, TRUE);
	g_signal_connect(ui->canvas, "draw", G_CALLBACK(cloud_draw), NULL);
	gtk_widget_show(ui->canvas);
	GtkWidget *panels = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
	gtk_box_set_homogeneous(GTK_BOX(panels), TRUE);
	GtkWidget *panel_a = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	ui->panel_b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	ui->title_a = gtk_label_new(NULL);
	ui->title_b = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(panel_a), ui->title_a, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(panel_a), ui->canvas, TRUE, TRUE, 0);
	ui->canvas_b = gtk_drawing_area_new();
	gtk_widget_set_size_request(ui->canvas_b, 240, 160);
	g_signal_connect(ui->canvas_b, "draw", G_CALLBACK(cloud_draw), NULL);
	gtk_box_pack_start(GTK_BOX(ui->panel_b), ui->title_b, FALSE, FALSE, 8);
	gtk_box_pack_start(GTK_BOX(ui->panel_b), ui->canvas_b, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(panels), panel_a, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(panels), ui->panel_b, TRUE, TRUE, 0);
	gtk_widget_show_all(panels);
	gtk_widget_hide(ui->panel_b);
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), panels, "cloud");
	ui->mensaje = gtk_label_new(NULL);
	gtk_widget_set_name(ui->mensaje, "cloud-message");
	gtk_label_set_line_wrap(GTK_LABEL(ui->mensaje), TRUE);
	gtk_label_set_justify(GTK_LABEL(ui->mensaje), GTK_JUSTIFY_CENTER);
	gtk_style_context_add_class(gtk_widget_get_style_context(ui->mensaje), "dim-label");
	gtk_widget_show(ui->mensaje);
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), ui->mensaje, "message");

	setup_tree();
	mostrar_mensaje(_("Elige un libro de la Biblia para ver sus palabras más usadas."));
	gtk_label_set_text(GTK_LABEL(ui->lbl_resumen), "");
	gtk_style_context_add_class(gtk_widget_get_style_context(ui->lbl_resumen), "dim-label");
	gtk_widget_set_sensitive(ui->btn_descargar, FALSE);

	g_signal_connect(ui->btn_descargar, "clicked", G_CALLBACK(on_descargar), NULL);
	g_signal_connect(ui->cloud_stack, "size-allocate", G_CALLBACK(on_area_allocate), NULL);
	g_signal_connect(combo_entry(ui->combo_libro), "changed",
			 G_CALLBACK(on_entry_changed), NULL);
	g_signal_connect(combo_entry(ui->combo_libro_b), "changed",
			 G_CALLBACK(on_entry_changed), NULL);
	g_signal_connect(ui->btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->chk_comparar, "toggled", G_CALLBACK(on_comparar_toggled), NULL);
	g_signal_connect(combo_entry(ui->combo_libro), "activate",
			 G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(combo_entry(ui->combo_libro_b), "activate",
			 G_CALLBACK(on_entry_activate), NULL);
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
