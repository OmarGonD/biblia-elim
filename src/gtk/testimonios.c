/*
 * Biblia Elim
 * testimonios.c - diálogo «Jesús en la historia»
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * A la izquierda, las fuentes agrupadas por su clase -- judías, romanas,
 * paganas, lo que ha salido de la tierra, los manuscritos -- y a la
 * derecha la ficha de la que se elija: el texto de la fuente traducido,
 * qué sostiene y qué no.
 *
 * Los pasajes bíblicos de cada ficha van como enlaces sword://, que es
 * lo mismo que usan las referencias del comentario: se abren en la
 * ventana principal, en la Biblia que el lector tenga puesta. Se
 * escriben en castellano porque el motor los interpreta con el idioma
 * del módulo abierto, que en esta aplicación es el español.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include <glib/gi18n.h>

#include "gui/testimonios.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "gui/table_helpers.h"
#include "main/testimonios.h"
#include "main/settings.h"
#include "main/sword.h"

#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

enum {
	COL_ETIQUETA = 0,
	COL_ID,	     /* vacío en las filas de grupo */
	N_COLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *box_html;
	GtkWidget *html;
	GtkWidget *tree;
	GtkWidget *lbl_pie;
	GListStore *modelo;	/* the sources, in groups */
} TestUI;

static TestUI *ui = NULL;

/* --------------------------------------------------------------------
 * Pintar la ficha
 * ------------------------------------------------------------------ */

static const char *
color_fondo(void)
{
	return settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
}

static const char *
color_texto(void)
{
	return settings.bible_text_color ? settings.bible_text_color
					 : "#222222";
}

static const char *
color_enlace(void)
{
	return settings.link_color ? settings.link_color : "#2C4A6E";
}

static void
escribir(const gchar *html)
{
	if (!ui || !ui->html)
		return;
	XIPHOS_HTML_OPEN_STREAM(ui->html, "text/html");
	XIPHOS_HTML_WRITE(ui->html, html, strlen(html));
	XIPHOS_HTML_CLOSE(ui->html);
}

/* Abre y cierra el documento con los colores del tema, que es lo que
 * hace que la ficha no salga blanca sobre un lector oscuro. */
static void
volcar_documento(const gchar *cuerpo)
{
	gchar *html = g_strdup_printf(
	    "<html><body bgcolor=\"%s\" style=\"color:%s\">%s</body></html>",
	    color_fondo(), color_texto(), cuerpo);

	escribir(html);
	g_free(html);
}

/* Un apartado con su rótulo, que se calla entero si no hay nada que
 * decir: una fuente sin cautelas no debe enseñar un título vacío. */
static void
apartado(GString *out, const char *rotulo, const char *texto)
{
	gchar *r, *t;

	if (!texto || !*texto)
		return;

	r = g_markup_escape_text(rotulo, -1);
	t = g_markup_escape_text(texto, -1);
	g_string_append_printf(out, "<h3>%s</h3><p>%s</p>", r, t);
	g_free(r);
	g_free(t);
}

/* El renderizador colapsa los saltos de línea, y en las fichas que
 * enumeran casos -- la de los materiales que no sirven, sin ir más
 * lejos -- esos saltos son lo que separa un caso del siguiente: aquí se
 * convierten en párrafos de verdad. */
static void
anadir_parrafos(GString *out, const char *texto, gboolean cursiva)
{
	gchar **trozos;
	gchar **p;

	trozos = g_strsplit(texto, "\n\n", -1);
	for (p = trozos; p && *p; p++) {
		gchar *t;

		g_strstrip(*p);
		if (!**p)
			continue;
		t = g_markup_escape_text(*p, -1);
		if (cursiva)
			g_string_append_printf(out, "<p><i>%s</i></p>", t);
		else
			g_string_append_printf(out, "<p>%s</p>", t);
		g_free(t);
	}
	g_strfreev(trozos);
}

static void
apartado_parrafos(GString *out, const char *rotulo, const char *texto)
{
	gchar *r;

	if (!texto || !*texto)
		return;

	r = g_markup_escape_text(rotulo, -1);
	g_string_append_printf(out, "<h3>%s</h3>", r);
	g_free(r);
	anadir_parrafos(out, texto, FALSE);
}

static void
mostrar_testimonio(const Testimonio *t)
{
	GString *out;
	GString *meta;
	gchar **refs;
	gchar *esc;

	if (!t)
		return;

	out = g_string_new(NULL);

	esc = g_markup_escape_text(t->titulo, -1);
	g_string_append_printf(out, "<h1>%s</h1>", esc);
	g_free(esc);

	/* La obra, quién la escribió y cuándo, en una sola línea: es lo
	 * primero que pregunta cualquiera que oiga una cita así. */
	meta = g_string_new(NULL);
	if (t->obra && *t->obra) {
		esc = g_markup_escape_text(t->obra, -1);
		g_string_append_printf(meta, "<b>%s</b>", esc);
		g_free(esc);
	}
	if (t->autor && *t->autor) {
		esc = g_markup_escape_text(t->autor, -1);
		g_string_append_printf(meta, "%s%s", meta->len ? " · " : "",
				       esc);
		g_free(esc);
	}
	if (t->fecha && *t->fecha && g_strcmp0(t->fecha, "-")) {
		esc = g_markup_escape_text(t->fecha, -1);
		g_string_append_printf(meta, "%s%s", meta->len ? " · " : "",
				       esc);
		g_free(esc);
	}
	if (meta->len)
		g_string_append_printf(out, "<p><small>%s</small></p>",
				       meta->str);
	g_string_free(meta, TRUE);

	if (t->postura && *t->postura) {
		esc = g_markup_escape_text(t->postura, -1);
		g_string_append_printf(
		    out, "<p><small style=\"color:%s\"><b>%s</b></small></p>",
		    color_enlace(), esc);
		g_free(esc);
	}

	/* El texto de la fuente, aparte y en cursiva: lo que sigue es
	 * comentario nuestro, y el lector tiene que ver dónde acaba uno y
	 * empieza el otro. */
	if (t->cita && *t->cita) {
		g_string_append(out, "<hr><blockquote>");
		anadir_parrafos(out, t->cita, TRUE);
		g_string_append(out, "</blockquote>");
	}

	apartado(out, _("Qué muestra"), t->muestra);
	apartado_parrafos(out, _("Con cuidado"), t->cautela);

	refs = main_testimonios_referencias(t);
	if (refs && *refs) {
		gchar **p;
		gboolean primera = TRUE;

		g_string_append_printf(out, "<h3>%s</h3><p>", _("En la Biblia"));
		for (p = refs; *p; p++) {
			gchar *texto, *url;

			if (!**p)
				continue;
			texto = g_markup_escape_text(*p, -1);
			url = g_markup_escape_text(*p, -1);
			g_string_append_printf(
			    out,
			    "%s<a href=\"sword:///%s\" style=\"color:%s\">%s</a>",
			    primera ? "" : " · ", url, color_enlace(), texto);
			primera = FALSE;
			g_free(texto);
			g_free(url);
		}
		g_string_append(out, "</p>");
	}
	g_strfreev(refs);

	volcar_documento(out->str);
	g_string_free(out, TRUE);
}

static void
mostrar_portada(void)
{
	GString *out = g_string_new(NULL);
	gchar *cuantas;

	g_string_append_printf(out, "<h1>%s</h1>", _("Jesús en la historia"));
	g_string_append_printf(
	    out, "<p>%s</p>",
	    _("Que Jesús de Nazaret existió no se sostiene solo con la "
	      "Biblia. Lo dan por hecho un historiador judío que escribía "
	      "para Roma, un senador romano que despreciaba a los "
	      "cristianos, un gobernador que los interrogó bajo tortura y "
	      "un filósofo que escribió un libro entero contra ellos. "
	      "Aquí están sus palabras, traducidas, con la fecha y la obra "
	      "de donde salen."));
	g_string_append_printf(
	    out, "<p>%s</p>",
	    _("Cada ficha dice también qué no demuestra. Hay fuentes "
	      "discutidas, otras tardías y algunas que circulan mucho y no "
	      "valen nada: están señaladas una por una. Un argumento que se "
	      "cae al primer empujón se lleva por delante todo lo demás, "
	      "que era lo bueno."));
	g_string_append_printf(out, "<p><small>%s</small></p>",
			       _("Elige una fuente en la lista de la "
				 "izquierda. Los pasajes bíblicos de cada "
				 "ficha se abren en la ventana principal, en "
				 "tu versión."));
	/* Josefo entero existe como módulo, pero en inglés y con casi
	 * cinco megas: no se instala solo, por lo mismo que no se instala
	 * un comentario en inglés. Quien lo quiera, que sepa que está. */
	g_string_append_printf(
	    out, "<p><small>%s</small></p>",
	    _("Las obras completas de Josefo se pueden instalar aparte, en "
	      "inglés y de dominio público, desde el gestor de módulos: "
	      "«Josephus», en la traducción de Whiston."));

	cuantas = g_strdup_printf(
	    ngettext("%u fuente, y funciona sin internet.",
		     "%u fuentes, y funcionan sin internet.",
		     main_testimonios_cuantos()),
	    main_testimonios_cuantos());
	g_string_append_printf(out, "<p><small>%s</small></p>", cuantas);
	g_free(cuantas);

	volcar_documento(out->str);
	g_string_free(out, TRUE);
}

/* --------------------------------------------------------------------
 * La lista
 * ------------------------------------------------------------------ */

static void
llenar_lista(void)
{
	const GList *g;

	g_list_store_remove_all(ui->modelo);

	for (g = main_testimonios_grupos(); g; g = g->next) {
		TestimonioGrupo *grupo = (TestimonioGrupo *)g->data;
		ElimRow *padre;
		GList *l;

		padre = elim_tree_append(ui->modelo, NULL, N_COLS);
		elim_row_set_string(padre, COL_ETIQUETA, grupo->titulo);
		elim_row_set_string(padre, COL_ID, "");

		for (l = grupo->testimonios; l; l = l->next) {
			Testimonio *t = (Testimonio *)l->data;
			ElimRow *fila = elim_tree_append(ui->modelo, padre, N_COLS);

			elim_row_set_string(fila, COL_ETIQUETA, t->titulo);
			elim_row_set_string(fila, COL_ID, t->id);
		}
	}
	elim_tree_expand_all(ui->tree);
}

static void
on_seleccion(GObject *seleccion, GParamSpec *pspec, gpointer datos)
{
	ElimRow *fila;
	const char *id;

	(void)seleccion;
	(void)pspec;
	(void)datos;
	fila = ui ? elim_table_get_selected(ui->tree) : NULL;
	if (!fila)
		return;

	id = elim_row_get_string(fila, COL_ID);
	if (*id)
		mostrar_testimonio(main_testimonios_por_id(id));
	else
		mostrar_portada();
}

static void
on_fila_activada(GtkWidget *tree, guint posicion, gpointer datos)
{
	ElimRow *fila = elim_table_row_at(tree, posicion);

	(void)datos;
	if (!fila)
		return;

	/* Un doble clic en el nombre de un grupo lo pliega, que es lo que
	 * espera cualquiera; en una fuente ya lo ha hecho la selección. */
	if (!*elim_row_get_string(fila, COL_ID)) {
		if (elim_tree_row_expanded(tree, fila))
			elim_tree_collapse_row(tree, fila);
		else
			elim_tree_expand_row(tree, fila, FALSE);
	}
}

/* Deja elegida la fila de un id, abriendo su grupo. */
static gboolean
seleccionar_id(const char *id)
{
	guint g, f;

	if (!ui || !id || !*id)
		return FALSE;

	for (g = 0; g < g_list_model_get_n_items(G_LIST_MODEL(ui->modelo)); g++) {
		ElimRow *grupo = elim_table_get(ui->modelo, g);

		for (f = 0; f < elim_row_n_children(grupo); f++) {
			ElimRow *fila = elim_row_get_child(grupo, f);

			if (!g_strcmp0(elim_row_get_string(fila, COL_ID), id))
				return elim_tree_select_row(ui->tree, fila, TRUE);
		}
	}
	return FALSE;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
on_cerrar(GtkButton *boton, gpointer datos)
{
	(void)boton;
	(void)datos;
	if (ui && ui->dialog)
		gui_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *widget, gpointer datos)
{
	(void)widget;
	(void)datos;
	if (!ui)
		return;
	if (ui->modelo)
		g_object_unref(ui->modelo);
	g_free(ui);
	ui = NULL;
}

static void
crear_dialogo(GtkWindow *padre)
{
	GtkBuilder *gxml;
	GtkWidget *btn_cerrar;
	ElimTextColumn columna = elim_text_column(COL_ETIQUETA);

	gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(
		gxml, "/org/xiphos/ui/testimonios.gtkbuilder", NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(
		    _("No se pudo abrir «Jesús en la historia»."));
		return;
	}

	ui = g_new0(TestUI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_testimonios");
	ui->box_html = UI_GET_ITEM(gxml, "box_html");
	ui->tree = UI_GET_ITEM(gxml, "tree_fuentes");
	ui->lbl_pie = UI_GET_ITEM(gxml, "lbl_pie");
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(
	    GTK_WINDOW(ui->dialog),
	    padre ? padre : (widgets.app ? GTK_WINDOW(widgets.app) : NULL));

	ui->html = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
	gtk_widget_show(ui->html);
	gui_box_pack(GTK_BOX(ui->box_html), ui->html, TRUE, TRUE, 0);

	ui->modelo = elim_table_new();
	columna.expand = TRUE;
	elim_tree_setup_list(ui->tree, ui->modelo, &columna);

	llenar_lista();

	gtk_label_set_text(
	    GTK_LABEL(ui->lbl_pie),
	    _("Los originales son de dominio público; las traducciones al "
	      "castellano se hicieron para esta aplicación."));

	g_signal_connect(elim_table_selection(ui->tree), "notify::selected-item",
			 G_CALLBACK(on_seleccion), NULL);
	g_signal_connect(ui->tree, "activate", G_CALLBACK(on_fila_activada), NULL);
	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	mostrar_portada();
	g_object_unref(gxml);
}

void
gui_testimonios_dialog(GtkWindow *padre)
{
	if (ui && ui->dialog) {
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}
	crear_dialogo(padre);
	if (ui && ui->dialog)
		gtk_widget_show(ui->dialog);
}

void
gui_testimonios_mostrar(const char *id)
{
	gui_testimonios_dialog(NULL);
	if (!ui)
		return;
	if (!seleccionar_id(id))
		mostrar_portada();
}
