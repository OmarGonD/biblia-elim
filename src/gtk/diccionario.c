/*
 * Biblia Elim — diálogo Diccionario / Léxico (offline)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include <glib/gi18n.h>

#include "gui/diccionario.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "gui/entry_suggest.h"
#include "gui/table_helpers.h"
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
	ElimEntrySuggest *sugerir;
	GListStore *comentarios; /* por autor, y bajo cada autor sus estudios */
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

static ElimRow *
ensure_autor(const gchar *autor)
{
	ElimRow *fila;
	guint i;

	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(ui->comentarios)); i++) {
		ElimRow *carpeta = elim_table_get(ui->comentarios, i);

		if (autor && !g_utf8_collate(elim_row_get_string(carpeta, COL_AUTOR), autor))
			return carpeta;
	}
	fila = elim_tree_append(ui->comentarios, NULL, N_COLS);
	elim_row_set_string(fila, COL_TITULO, autor);
	elim_row_set_string(fila, COL_AUTOR, autor);
	elim_row_set_int(fila, COL_TIPO, 0);
	return fila;
}

static void
llenar_comentarios(const DiccEntrada *e)
{
	g_list_store_remove_all(ui->comentarios);

	if (e) {
		for (GList *l = e->estudios; l; l = l->next) {
			DiccEstudio *es = (DiccEstudio *)l->data;
			ElimRow *row = elim_tree_append(ui->comentarios,
							ensure_autor(es->autor), N_COLS);

			elim_row_set_string(row, COL_TITULO, es->titulo);
			elim_row_set_string(row, COL_AUTOR, es->autor);
			elim_row_set_int(row, COL_TIPO, 1);
			elim_row_set_string(row, COL_TEXTO, es->texto);
		}
	}

	GList *comms = main_diccionario_comentarios(settings.currentverse);
	for (GList *l = comms; l; l = l->next) {
		DiccComentario *c = (DiccComentario *)l->data;
		ElimRow *row = elim_tree_append(ui->comentarios, ensure_autor(c->autor), N_COLS);
		gchar *titulo = g_strdup_printf("%s — %s",
						c->descripcion ? c->descripcion : c->modulo,
						settings.currentverse ? settings.currentverse : "");

		elim_row_set_string(row, COL_TITULO, titulo);
		elim_row_set_string(row, COL_AUTOR, c->autor);
		elim_row_set_string(row, COL_MODULO, c->modulo);
		elim_row_set_int(row, COL_TIPO, 2);
		elim_row_set_string(row, COL_TEXTO, c->extracto);
		g_free(titulo);
	}
	gboolean hay = (g_list_model_get_n_items(G_LIST_MODEL(ui->comentarios)) > 0);
	gtk_widget_set_visible(ui->box_comentarios, hay);
	if (hay)
		elim_tree_expand_all(ui->tree);
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
	const gchar *q = gtk_editable_get_text(GTK_EDITABLE(ui->entry));
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
sugerencia_coincide(const char *key, const char *titulo, gpointer data)
{
	(void)data;
	return main_nube_texto_coincide(titulo, key);
}

static void
on_comentario_activado(GtkWidget *tree, guint posicion, gpointer user_data)
{
	ElimRow *fila = elim_table_row_at(tree, posicion);
	(void)user_data;
	if (!fila)
		return;
	gint tipo = elim_row_get_int(fila, COL_TIPO);
	/* the row may go while the text is shown */
	gchar *titulo = g_strdup(elim_row_get_string(fila, COL_TITULO));
	gchar *autor = g_strdup(elim_row_get_string(fila, COL_AUTOR));
	gchar *modulo = g_strdup(elim_row_get_string(fila, COL_MODULO));
	gchar *texto = g_strdup(elim_row_get_string(fila, COL_TEXTO));
	if (tipo == 0) {
		if (elim_tree_row_expanded(tree, fila))
			elim_tree_collapse_row(tree, fila);
		else
			elim_tree_expand_row(tree, fila, FALSE);
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
		gui_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	if (!ui)
		return;
	if (ui->comentarios)
		g_object_unref(ui->comentarios);
	g_free(ui);
	ui = NULL;
}

static void
poblar_completion(void)
{
	GList *sugs = main_diccionario_sugerencias("");

	elim_entry_suggest_set_candidates(ui->sugerir, sugs);
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
	gui_box_pack(GTK_BOX(ui->box_html), ui->html, TRUE, TRUE, 0);

	/* before the "activate" handler below: Enter takes a suggestion first */
	ui->sugerir = elim_entry_suggest_new(ui->entry, sugerencia_coincide, NULL);
	poblar_completion();

	ui->comentarios = elim_table_new();
	elim_tree_setup(ui->tree, ui->comentarios);
	ElimTextColumn col = elim_text_column(COL_TITULO);
	col.expand = TRUE;
	col.expander = TRUE;
	elim_table_add_column(ui->tree, _("Autor / estudio"), &col);

	mostrar_html(_("Diccionario"),
		     _("Escribe una palabra (por ejemplo Adonai) y pulsa Buscar. "
		       "Todo el léxico está incluido en la aplicación y funciona sin internet. "
		       "Si hay estudios o comentarios para el pasaje actual, aparecen abajo agrupados por autor."),
		     NULL, NULL);
	llenar_comentarios(NULL);

	g_signal_connect(ui->btn_buscar, "clicked", G_CALLBACK(on_buscar), NULL);
	g_signal_connect(ui->btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->entry, "activate", G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect(ui->tree, "activate", G_CALLBACK(on_comentario_activado), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);
	gtk_window_set_default_widget(GTK_WINDOW(ui->dialog), ui->btn_buscar);
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
		gtk_editable_set_text(GTK_EDITABLE(ui->entry), palabra);
	if (palabra && *palabra)
		mostrar_entrada(main_diccionario_buscar(palabra), palabra);
}
