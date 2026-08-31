/*
 * Biblia Elim — diálogo Diccionario / Léxico (offline)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/diccionario.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/diccionario.h"
#include "main/nube_palabras.h"
#include "main/settings.h"
#include "main/sword.h"

#include "xiphos_html/xiphos_html.h"

enum {
	COL_TITULO = 0,
	COL_AUTOR,
	COL_MODULO,
	COL_TIPO, /* 0 carpeta autor, 1 estudio, 2 comentario Sword */
	COL_TEXTO,
	N_COLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *btn_buscar;
	GtkWidget *btn_cerrar;
	GtkWidget *box_html;
	GtkWidget *html;
	GtkWidget *tree;
	GtkWidget *box_comentarios;
	GtkListStore *completar;
	GtkTreeStore *comentarios;
	const DiccEntrada *actual;
} DiccUI;

static DiccUI *ui = NULL;

static gchar *
html_esc(const gchar *s)
{
	return g_markup_escape_text(s ? s : "", -1);
}

static void
escribir_html(const gchar *html)
{
	if (!ui || !ui->html)
		return;
	XIPHOS_HTML_OPEN_STREAM(ui->html, "text/html");
	XIPHOS_HTML_WRITE(ui->html, html, strlen(html));
	XIPHOS_HTML_CLOSE(ui->html);
}

static void
mostrar_html(const gchar *titulo, const gchar *cuerpo, const gchar *refs, const gchar *extra)
{
	const char *bg = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	const char *fg = settings.bible_text_color ? settings.bible_text_color : "#222222";
	gchar *t = html_esc(titulo);
	gchar *c = html_esc(cuerpo);
	gchar *r = html_esc(refs);
	gchar *e = extra ? html_esc(extra) : NULL;
	gchar *html = g_strdup_printf(
	    "<html><head><meta charset=\"utf-8\"/><style>"
	    "body{margin:0;padding:16px 18px;background:%s;color:%s;"
	    "font-family:'Noto Sans','DejaVu Sans',sans-serif;line-height:1.45;}"
	    "h1{font-size:1.45em;margin:0 0 .4em;}"
	    "p{margin:.4em 0 0.8em;}"
	    ".refs{opacity:.8;font-size:.92em;}"
	    ".extra{margin-top:1.2em;padding-top:.8em;border-top:1px solid alpha(currentColor,.2);}"
	    "</style></head><body><h1>%s</h1><p>%s</p>"
	    "%s%s%s%s%s</body></html>",
	    bg, fg, t, c,
	    (refs && *refs) ? "<p class=\"refs\"><b>" : "",
	    (refs && *refs) ? _("Referencias: ") : "",
	    (refs && *refs) ? r : "",
	    (refs && *refs) ? "</b></p>" : "",
	    (e && *e) ? e : "");
	escribir_html(html);
	g_free(html);
	g_free(t);
	g_free(c);
	g_free(r);
	g_free(e);
}

static GtkTreeIter
ensure_autor(const gchar *autor)
{
	GtkTreeIter iter, child;
	gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(ui->comentarios), &iter);
	while (valid) {
		gchar *a = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(ui->comentarios), &iter, COL_AUTOR, &a, -1);
		gboolean match = (a && autor && !g_utf8_collate(a, autor));
		g_free(a);
		if (match)
			return iter;
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(ui->comentarios), &iter);
	}
	gtk_tree_store_append(ui->comentarios, &child, NULL);
	gtk_tree_store_set(ui->comentarios, &child,
			   COL_TITULO, autor,
			   COL_AUTOR, autor,
			   COL_MODULO, "",
			   COL_TIPO, 0,
			   COL_TEXTO, "",
			   -1);
	return child;
}

static void
llenar_comentarios(const DiccEntrada *e)
{
	gtk_tree_store_clear(ui->comentarios);

	if (e) {
		for (GList *l = e->estudios; l; l = l->next) {
			DiccEstudio *es = (DiccEstudio *)l->data;
			GtkTreeIter parent = ensure_autor(es->autor);
			GtkTreeIter row;
			gtk_tree_store_append(ui->comentarios, &row, &parent);
			gtk_tree_store_set(ui->comentarios, &row,
					   COL_TITULO, es->titulo,
					   COL_AUTOR, es->autor,
					   COL_MODULO, "",
					   COL_TIPO, 1,
					   COL_TEXTO, es->texto,
					   -1);
		}
	}

	GList *comms = main_diccionario_comentarios(settings.currentverse);
	for (GList *l = comms; l; l = l->next) {
		DiccComentario *c = (DiccComentario *)l->data;
		GtkTreeIter parent = ensure_autor(c->autor);
		GtkTreeIter row;
		gchar *titulo = g_strdup_printf("%s — %s",
						c->descripcion ? c->descripcion : c->modulo,
						settings.currentverse ? settings.currentverse : "");
		gtk_tree_store_append(ui->comentarios, &row, &parent);
		gtk_tree_store_set(ui->comentarios, &row,
				   COL_TITULO, titulo,
				   COL_AUTOR, c->autor,
				   COL_MODULO, c->modulo,
				   COL_TIPO, 2,
				   COL_TEXTO, c->extracto,
				   -1);
		g_free(titulo);
	}
	gboolean hay = (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(ui->comentarios), NULL) > 0);
	gtk_widget_set_visible(ui->box_comentarios, hay);
	if (hay)
		gtk_tree_view_expand_all(GTK_TREE_VIEW(ui->tree));
	main_diccionario_comentarios_free(comms);
}

static void
mostrar_entrada(const DiccEntrada *e, const char *buscado)
{
	ui->actual = e;
	if (!e) {
		gchar *msg = g_strdup_printf(
		    _("No se encontró «%s» en el diccionario offline."),
		    buscado ? buscado : "");
		mostrar_html(_("Sin resultado"), msg, NULL, NULL);
		g_free(msg);
		llenar_comentarios(NULL);
		return;
	}
	mostrar_html(e->titulo, e->definicion, e->referencias, NULL);
	llenar_comentarios(e);
}

static void
on_buscar(GtkButton *button, gpointer user_data)
{
	(void)button;
	(void)user_data;
	const gchar *q = gtk_entry_get_text(GTK_ENTRY(ui->entry));
	if (!q || !*q) {
		gui_generic_warning(_("Escribe una palabra para buscar, por ejemplo Adonai."));
		return;
	}
	mostrar_entrada(main_diccionario_buscar(q), q);
}

static void
on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	(void)entry;
	(void)user_data;
	on_buscar(NULL, NULL);
}

static gboolean
completion_match(GtkEntryCompletion *comp, const gchar *key,
		 GtkTreeIter *iter, gpointer data)
{
	(void)data;
	gchar *titulo = NULL;
	gtk_tree_model_get(gtk_entry_completion_get_model(comp), iter, 0, &titulo, -1);
	gboolean ok = main_nube_texto_coincide(titulo, key);
	g_free(titulo);
	return ok;
}

static void
on_comentario_activado(GtkTreeView *tree, GtkTreePath *path,
		       GtkTreeViewColumn *col, gpointer user_data)
{
	(void)col;
	(void)user_data;
	GtkTreeIter iter;
	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(ui->comentarios), &iter, path))
		return;
	gint tipo = 0;
	gchar *titulo = NULL, *autor = NULL, *modulo = NULL, *texto = NULL;
	gtk_tree_model_get(GTK_TREE_MODEL(ui->comentarios), &iter,
			   COL_TITULO, &titulo,
			   COL_AUTOR, &autor,
			   COL_MODULO, &modulo,
			   COL_TIPO, &tipo,
			   COL_TEXTO, &texto,
			   -1);
	if (tipo == 0) {
		if (gtk_tree_view_row_expanded(tree, path))
			gtk_tree_view_collapse_row(tree, path);
		else
			gtk_tree_view_expand_row(tree, path, FALSE);
	} else if (tipo == 1) {
		gchar *head = g_strdup_printf("%s — %s", autor ? autor : "", titulo ? titulo : "");
		mostrar_html(head, texto ? texto : "", NULL, NULL);
		g_free(head);
	} else if (tipo == 2 && modulo && *modulo) {
		char *full = main_get_rendered_text(modulo, settings.currentverse);
		if (!full)
			full = main_get_striptext((char *)modulo, settings.currentverse);
		gchar *head = g_strdup_printf("%s", autor ? autor : modulo);
		mostrar_html(head, full ? full : (texto ? texto : ""),
			     settings.currentverse, NULL);
		g_free(head);
		g_free(full);
		if (settings.havecomm)
			main_display_commentary(modulo, settings.currentverse);
	}
	g_free(titulo);
	g_free(autor);
	g_free(modulo);
	g_free(texto);
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
on_destroy(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	if (!ui)
		return;
	if (ui->completar)
		g_object_unref(ui->completar);
	if (ui->comentarios)
		g_object_unref(ui->comentarios);
	g_free(ui);
	ui = NULL;
}

static void
poblar_completion(void)
{
	GList *sugs = main_diccionario_sugerencias("");
	gtk_list_store_clear(ui->completar);
	for (GList *l = sugs; l; l = l->next) {
		GtkTreeIter it;
		gtk_list_store_append(ui->completar, &it);
		gtk_list_store_set(ui->completar, &it, 0, (gchar *)l->data, -1);
	}
	g_list_free_full(sugs, g_free);
}

static void
crear_dialogo(void)
{
	GtkBuilder *gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/diccionario.gtkbuilder", NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir el Diccionario."));
		return;
	}

	ui = g_new0(DiccUI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_diccionario");
	ui->entry = UI_GET_ITEM(gxml, "entry_palabra");
	ui->btn_buscar = UI_GET_ITEM(gxml, "btn_buscar");
	ui->btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");
	ui->box_html = UI_GET_ITEM(gxml, "box_html");
	ui->tree = UI_GET_ITEM(gxml, "tree_comentarios");
	ui->box_comentarios = UI_GET_ITEM(gxml, "box_comentarios");

	gui_prepare_floating_dialog(GTK_WINDOW(ui->dialog),
				    widgets.app ? GTK_WINDOW(widgets.app) : NULL);

	ui->html = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	gtk_widget_show(ui->html);
#ifdef USE_WEBKIT2
	gtk_box_pack_start(GTK_BOX(ui->box_html), ui->html, TRUE, TRUE, 0);
#else
	GtkWidget *sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(sw);
	gtk_container_add(GTK_CONTAINER(sw), ui->html);
	gtk_box_pack_start(GTK_BOX(ui->box_html), sw, TRUE, TRUE, 0);
#endif

	ui->completar = gtk_list_store_new(1, G_TYPE_STRING);
	poblar_completion();
	GtkEntryCompletion *comp = gtk_entry_completion_new();
	gtk_entry_completion_set_model(comp, GTK_TREE_MODEL(ui->completar));
	gtk_entry_completion_set_text_column(comp, 0);
	gtk_entry_completion_set_minimum_key_length(comp, 1);
	gtk_entry_completion_set_match_func(comp, completion_match, NULL, NULL);
	gtk_entry_set_completion(GTK_ENTRY(ui->entry), comp);
	g_object_unref(comp);

	ui->comentarios = gtk_tree_store_new(N_COLS,
					     G_TYPE_STRING, G_TYPE_STRING,
					     G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree), GTK_TREE_MODEL(ui->comentarios));
	GtkCellRenderer *cell = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes(
	    _("Autor / estudio"), cell, "text", COL_TITULO, NULL);
	gtk_tree_view_column_set_expand(col, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree), col);

	mostrar_html(_("Diccionario"),
		     _("Escribe una palabra (por ejemplo Adonai) y pulsa Buscar. "
		       "Todo el léxico está incluido en la aplicación y funciona sin internet. "
		       "Si hay estudios o comentarios para el pasaje actual, aparecen abajo agrupados por autor."),
		     NULL, NULL);
	llenar_comentarios(NULL);

	g_signal_connect(ui->btn_buscar, "clicked", G_CALLBACK(on_buscar), NULL);
	g_signal_connect(ui->btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->entry, "activate", G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(ui->tree, "row-activated", G_CALLBACK(on_comentario_activado), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);
	gtk_widget_set_can_default(ui->btn_buscar, TRUE);
	gtk_widget_grab_default(ui->btn_buscar);
	gtk_widget_grab_focus(ui->entry);
}

void
gui_diccionario_dialog(void)
{
	if (ui && ui->dialog) {
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}
	crear_dialogo();
	if (ui && ui->dialog)
		gtk_widget_show(ui->dialog);
}

void
gui_diccionario_mostrar(const char *palabra)
{
	gui_diccionario_dialog();
	if (!ui)
		return;
	if (palabra && *palabra)
		gtk_entry_set_text(GTK_ENTRY(ui->entry), palabra);
	if (palabra && *palabra)
		mostrar_entrada(main_diccionario_buscar(palabra), palabra);
}
