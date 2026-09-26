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

#include "xiphos_html/xiphos_html.h"

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
	GtkWidget *btn_mostrar;
	GtkWidget *btn_cerrar;
	GtkWidget *lbl_resumen;
	GtkWidget *box_nube;
	GtkWidget *html;
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

static void
escribir_html(const gchar *html)
{
	XIPHOS_HTML_OPEN_STREAM(ui->html, "text/html");
	XIPHOS_HTML_WRITE(ui->html, html, strlen(html));
	XIPHOS_HTML_CLOSE(ui->html);
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
html_placeholder(const gchar *mensaje)
{
	gtk_stack_set_visible_child_name(GTK_STACK(ui->cloud_stack), "message");
	const char *bg = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	const char *fg = settings.bible_text_color ? settings.bible_text_color : "#222222";
	gchar *esc = html_escape(mensaje);
	gchar *html = g_strdup_printf(
	    "<html><head><meta charset=\"utf-8\"/><style>"
	    "body{margin:0;padding:48px 24px;background:%s;color:%s;"
	    "font-family:'Noto Sans','DejaVu Sans',sans-serif;"
	    "text-align:center;opacity:.7;font-size:15px;}"
	    "</style></head><body>%s</body></html>",
	    bg, fg, esc);
	escribir_html(html);
	g_free(html);
	g_free(esc);
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
		fg = "#82ffb0";
	else if (v < 0)
		fg = "#ffc078";
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
			fg = "#82ffb0";
		else if (v < -0.001)
			fg = "#ffc078";
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
	GtkCssProvider *css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(css,
		"#cloud-statistics { background-color:#071610; color:#b4efc8;"
		"font-family:monospace; font-size:14px; }"
		"#cloud-statistics.view { background-color:#071610; color:#b4efc8; }"
		"#cloud-statistics.view:selected { background-color:#164c36; color:#effff4; }"
		"#cloud-statistics header button { background-image:none; background-color:#10291d;"
		"color:#83f7ac; border:1px solid #28543d; border-radius:0; padding:10px 12px; }"
		"#cloud-statistics header button label { color:#83f7ac; font-weight:bold; }",
		-1, NULL);
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

static gchar *
resolver_o_avisar(GtkWidget *combo)
{
	const gchar *texto = combo_texto(combo);
	if (!texto || !*texto) {
		gui_generic_warning(_("Escribe o elige un libro de la Biblia."));
		return NULL;
	}
	char *nombre = main_nube_resolver_libro(settings.MainWindowModule, texto);
	if (!nombre) {
		gui_generic_warning(_("No se encontró ese libro de la Biblia."));
		return NULL;
	}
	combo_set_texto(combo, nombre);
	return nombre;
}

static void
on_mostrar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;

	if (!settings.MainWindowModule || !settings.havebible) {
		gui_generic_warning(_("Abre un texto bíblico para usar la nube de palabras."));
		return;
	}

	gboolean comparar = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_comparar));

	gchar *libro_a = resolver_o_avisar(ui->combo_libro);
	if (!libro_a)
		return;

	gchar *libro_b = NULL;
	if (comparar) {
		libro_b = resolver_o_avisar(ui->combo_libro_b);
		if (!libro_b) {
			g_free(libro_a);
			return;
		}
		if (g_utf8_collate(libro_a, libro_b) == 0) {
			gui_generic_warning(_("Elige dos libros distintos para comparar."));
			g_free(libro_a);
			g_free(libro_b);
			return;
		}
	}

	gtk_label_set_text(GTK_LABEL(ui->lbl_resumen),
			   _("Contando palabras…"));
	while (gtk_events_pending())
		gtk_main_iteration();

	NUBE_CONTEO *c = main_nube_contar(settings.MainWindowModule,
					  libro_a, libro_b, NUBE_LIMITE);
	if (!c || c->total == 0) {
		gui_generic_warning(_("No se pudo leer el texto de ese libro."));
		html_placeholder(_("No hay palabras para mostrar."));
		gtk_list_store_clear(ui->tabla);
		gtk_label_set_text(GTK_LABEL(ui->lbl_resumen), "");
		main_nube_conteo_free(c);
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

	const char *background = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	CloudLayout *cloud, *cloud_b = NULL;
	gboolean pair = comparar && c->libro_b;
	if (pair)
		cloud_build_pair(ui->canvas, ui->canvas_b, c, background, &cloud, &cloud_b);
	else
		cloud = cloud_build(ui->canvas, c, FALSE, background);
	gchar *title = g_strdup_printf("א · %s — %d palabras", c->libro, c->total);
	gtk_label_set_text(GTK_LABEL(ui->title_a), title);
	g_free(title);
	if (pair) {
		title = g_strdup_printf("ב · %s — %d palabras", c->libro_b, c->total_b);
		gtk_label_set_text(GTK_LABEL(ui->title_b), title);
		g_free(title);
	}
	gtk_widget_set_visible(ui->panel_b, pair);
	g_object_set_data_full(G_OBJECT(ui->canvas_b), "cloud", cloud_b, cloud_free);
	g_object_set_data_full(G_OBJECT(ui->canvas), "cloud", cloud, cloud_free);
	gtk_stack_set_visible_child_name(GTK_STACK(ui->cloud_stack), "cloud");
	gtk_widget_queue_draw(ui->canvas);
	gtk_widget_queue_draw(ui->canvas_b);
	llenar_tabla(c, comparar && c->libro_b);

	main_nube_conteo_free(c);
	g_free(libro_a);
	g_free(libro_b);
}

static void
on_comparar_toggled(GtkToggleButton *btn, gpointer user_data)
{
	(void)user_data;
	gboolean on = gtk_toggle_button_get_active(btn);
	gtk_widget_set_sensitive(ui->combo_libro_b, on);
	if (on)
		gtk_widget_grab_focus(combo_entry(ui->combo_libro_b));
}

static void
on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	(void)entry;
	(void)user_data;
	on_mostrar(NULL, NULL);
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
	ui->dialog = UI_GET_ITEM(gxml, "dialog_nube_palabras");
	ui->combo_libro = UI_GET_ITEM(gxml, "combo_libro");
	ui->combo_libro_b = UI_GET_ITEM(gxml, "combo_libro_b");
	ui->chk_comparar = UI_GET_ITEM(gxml, "chk_comparar");
	ui->btn_mostrar = UI_GET_ITEM(gxml, "btn_mostrar");
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

	if (settings.MainWindowModule && settings.currentverse) {
		char *actual = main_nube_libro_de_clave(settings.MainWindowModule,
							settings.currentverse);
		if (actual) {
			combo_set_texto(ui->combo_libro, actual);
			g_free(actual);
		}
	}

	ui->html = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	gtk_widget_show(ui->html);
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
#ifdef USE_WEBKIT2
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), ui->html, "message");
#else
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_show(sw);
	gtk_container_add(GTK_CONTAINER(sw), ui->html);
	gtk_stack_add_named(GTK_STACK(ui->cloud_stack), sw, "message");
#endif

	setup_tree();
	html_placeholder(_("Escribe o elige un libro de la Biblia para ver sus palabras más usadas."));

	g_signal_connect(ui->btn_mostrar, "clicked", G_CALLBACK(on_mostrar), NULL);
	g_signal_connect(ui->btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->chk_comparar, "toggled", G_CALLBACK(on_comparar_toggled), NULL);
	g_signal_connect(combo_entry(ui->combo_libro), "activate",
			 G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(combo_entry(ui->combo_libro_b), "activate",
			 G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	gtk_widget_set_can_default(ui->btn_mostrar, TRUE);
	gtk_widget_grab_default(ui->btn_mostrar);
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
