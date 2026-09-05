/*
 * Biblia Elim
 * buscar_notas.c - diálogo «Buscar en mis notas»
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Las notas se guardan en settings.xml y hasta ahora no había manera de
 * mirarlas todas juntas: se veían una a una, estando en su versículo.
 * Después de un año usando la aplicación eso es un archivo muerto.
 *
 * Se leen todas de una vez al abrir el cuadro (highlight_all_notes(), que
 * va al XML y no a la caché del libro abierto) y se buscan en memoria a
 * cada tecla: son notas de una persona, no un índice de la Biblia, así
 * que caben de sobra y buscar es instantáneo.
 *
 * Con el cuadro vacío salen todas, en orden bíblico, que es la otra cosa
 * que hacía falta: un índice de lo que uno lleva escrito.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/buscar_notas.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/buscar_notas.h"
#include "main/display.hh"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"

/* Lo que se espera desde la última tecla antes de buscar. Corto: la
 * búsqueda es en memoria y el resultado tiene que ir siguiendo a lo que
 * se escribe. */
#define TECLEO_MS 180

enum {
	COL_PASAJE = 0,
	COL_EXTRACTO,	/* con marcado: lo hallado en negrita */
	COL_VERSION,
	COL_OSISREF,
	COL_MODULO,
	N_COLS
};

typedef struct {
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *chk_regex;
	GtkWidget *chk_mayusculas;
	GtkWidget *lbl_estado;
	GtkWidget *tree;
	GtkWidget *btn_ir;
	GtkListStore *modelo;

	GList *notas;	 /* HighlightNote*, todas, leídas una vez */
	GList *entrada;	 /* BN_NOTA*, la misma lista en lo que espera el buscador */
	guint tecleo;
} BNUI;

static BNUI *ui = NULL;

static void buscar(void);

/* --------------------------------------------------------------------
 * Las notas, leídas una vez
 * ------------------------------------------------------------------ */

static void
cargar_notas(void)
{
	GList *l;

	ui->notas = highlight_all_notes();

	for (l = ui->notas; l; l = l->next) {
		HighlightNote *h = (HighlightNote *)l->data;
		BN_NOTA *n = g_new0(BN_NOTA, 1);

		n->modulo = h->module;
		n->osisref = h->osisref;
		n->note_key = h->note_key;
		n->frase = h->text;
		n->nota = h->note;
		ui->entrada = g_list_append(ui->entrada, n);
	}
}

/* --------------------------------------------------------------------
 * Pintar
 * ------------------------------------------------------------------ */

/* «Eph.2.1» no se le enseña a nadie: se pide el nombre del libro en el
 * idioma del módulo en el que se escribió la nota. */
static gchar *
pasaje_legible(const gchar *modulo, const gchar *osisref)
{
	const char *mod = (modulo && *modulo) ? modulo
					      : settings.MainWindowModule;
	char *valida;
	gchar *out;

	if (!osisref || !*osisref)
		return g_strdup("");
	if (!mod || !*mod || !main_is_module((char *)mod))
		return g_strdup(osisref);

	valida = (char *)main_get_valid_key(mod, osisref);
	out = g_strdup((valida && *valida) ? valida : osisref);
	free(valida);
	return out;
}

/* El extracto con lo hallado en negrita. Se parte en tres y se escapa
 * cada trozo por su cuenta: escapar primero y meter las etiquetas
 * después movería las posiciones. */
static gchar *
extracto_marcado(const BN_RESULTADO *r)
{
	gchar *antes, *medio, *despues, *out;
	gint largo = (gint)strlen(r->extracto);
	gint ini = CLAMP(r->ini, 0, largo);
	gint fin = CLAMP(r->fin, ini, largo);

	antes = g_markup_escape_text(r->extracto, ini);
	medio = g_markup_escape_text(r->extracto + ini, fin - ini);
	despues = g_markup_escape_text(r->extracto + fin, largo - fin);

	/* Lo que salió por la frase subrayada y no por la nota se enseña
	 * entrecomillado y en cursiva, para que se vea de dónde viene. */
	if (r->en_frase)
		out = g_strdup_printf("<i>“%s<b>%s</b>%s”</i>", antes, medio,
				      despues);
	else
		out = g_strdup_printf("%s<b>%s</b>%s", antes, medio, despues);

	g_free(antes);
	g_free(medio);
	g_free(despues);
	return out;
}

/* Sin nada escrito, la lista entera: es el índice de lo que uno lleva
 * escrito, y para eso también se abre este cuadro. */
static void
poner_todas(void)
{
	GList *l;
	int n = 0;

	for (l = ui->notas; l; l = l->next) {
		HighlightNote *h = (HighlightNote *)l->data;
		GtkTreeIter it;
		gchar *pasaje = pasaje_legible(h->module, h->osisref);
		gchar *renglon = g_strdup(h->note ? h->note : "");
		gchar *marcado;

		g_strdelimit(renglon, "\n\r\t", ' ');
		if (g_utf8_strlen(renglon, -1) > 160) {
			gchar *corte = g_utf8_substring(renglon, 0, 157);

			g_free(renglon);
			renglon = g_strconcat(corte, "…", NULL);
			g_free(corte);
		}
		marcado = g_markup_escape_text(renglon, -1);

		gtk_list_store_append(ui->modelo, &it);
		gtk_list_store_set(ui->modelo, &it,
				   COL_PASAJE, pasaje,
				   COL_EXTRACTO, marcado,
				   COL_VERSION, h->module ? h->module : "",
				   COL_OSISREF, h->osisref ? h->osisref : "",
				   COL_MODULO, h->module ? h->module : "",
				   -1);
		g_free(marcado);
		g_free(renglon);
		g_free(pasaje);
		n++;
	}

	if (n == 0)
		gtk_label_set_text(
		    GTK_LABEL(ui->lbl_estado),
		    _("Todavía no has escrito ninguna nota. Se escriben en la "
		      "ficha de debajo del versículo, o subrayando una frase."));
	else {
		gchar *m = g_strdup_printf(
		    ngettext("%d nota escrita.", "%d notas escritas.", n), n);

		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
	}
}

static void
buscar(void)
{
	const gchar *consulta;
	BN_MODO modo;
	gboolean mayus;
	GError *error = NULL;
	GList *r, *l;
	int n = 0;

	if (!ui)
		return;

	gtk_list_store_clear(ui->modelo);
	gtk_widget_set_sensitive(ui->btn_ir, FALSE);

	consulta = gtk_entry_get_text(GTK_ENTRY(ui->entry));
	if (!consulta || !*consulta) {
		poner_todas();
		return;
	}

	modo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->chk_regex))
		   ? BN_REGEX
		   : BN_TEXTO;
	mayus = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_mayusculas));

	r = main_buscar_notas(ui->entrada, consulta, modo, mayus, &error);

	/* Una expresión a medio escribir da error casi siempre -- «(» sin
	 * cerrar --, así que el motivo se dice aquí abajo y no en un
	 * cuadro de diálogo que habría que cerrar en cada tecla. */
	if (error) {
		gchar *m = g_strdup_printf(_("Expresión regular: %s"),
					   error->message);

		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
		g_error_free(error);
		return;
	}

	for (l = r; l; l = l->next) {
		BN_RESULTADO *res = (BN_RESULTADO *)l->data;
		GtkTreeIter it;
		gchar *pasaje = pasaje_legible(res->modulo, res->osisref);
		gchar *marcado = extracto_marcado(res);

		gtk_list_store_append(ui->modelo, &it);
		gtk_list_store_set(ui->modelo, &it,
				   COL_PASAJE, pasaje,
				   COL_EXTRACTO, marcado,
				   COL_VERSION, res->modulo,
				   COL_OSISREF, res->osisref,
				   COL_MODULO, res->modulo,
				   -1);
		g_free(marcado);
		g_free(pasaje);
		n++;
	}
	main_buscar_notas_libre(r);

	if (n == 0) {
		gchar *m = g_strdup_printf(
		    _("Nada en tus notas con «%s»."), consulta);

		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
	} else {
		gchar *m = g_strdup_printf(
		    ngettext("%d nota.", "%d notas.", n), n);

		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
	}
}

/* --------------------------------------------------------------------
 * Ir a la nota
 * ------------------------------------------------------------------ */

static void
ir_a_la_seleccionada(void)
{
	GtkTreeSelection *sel =
	    gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->tree));
	GtkTreeModel *modelo;
	GtkTreeIter it;
	gchar *osisref = NULL, *modulo = NULL, *url;

	if (!gtk_tree_selection_get_selected(sel, &modelo, &it))
		return;

	gtk_tree_model_get(modelo, &it, COL_OSISREF, &osisref, COL_MODULO,
			   &modulo, -1);
	if (osisref && *osisref) {
		/* El mismo camino que usan las notas enlazadas: lleva a la
		 * versión en la que se escribió, que es donde la nota se
		 * ve. */
		url = g_strdup_printf(
		    "passagestudy.jsp?action=showBookmark&type=currentTab&"
		    "value=%s&module=%s",
		    osisref,
		    (modulo && *modulo) ? modulo : settings.MainWindowModule);
		main_url_handler(url, TRUE);
		g_free(url);
	}
	g_free(osisref);
	g_free(modulo);
}

/* --------------------------------------------------------------------
 * Señales
 * ------------------------------------------------------------------ */

static gboolean
on_tecleo(gpointer datos)
{
	(void)datos;
	if (!ui)
		return G_SOURCE_REMOVE;
	ui->tecleo = 0;
	buscar();
	return G_SOURCE_REMOVE;
}

static void
on_cambio(GtkEditable *entry, gpointer datos)
{
	(void)entry;
	(void)datos;
	if (!ui)
		return;
	if (ui->tecleo)
		g_source_remove(ui->tecleo);
	ui->tecleo = g_timeout_add(TECLEO_MS, on_tecleo, NULL);
}

static void
on_opcion(GtkToggleButton *b, gpointer datos)
{
	(void)b;
	(void)datos;
	buscar();
}

static void
on_seleccion(GtkTreeSelection *sel, gpointer datos)
{
	(void)datos;
	gtk_widget_set_sensitive(ui->btn_ir,
				 gtk_tree_selection_get_selected(sel, NULL,
								 NULL));
}

static void
on_fila_activada(GtkTreeView *tree, GtkTreePath *path,
		 GtkTreeViewColumn *col, gpointer datos)
{
	(void)tree;
	(void)path;
	(void)col;
	(void)datos;
	ir_a_la_seleccionada();
}

static void
on_ir(GtkButton *b, gpointer datos)
{
	(void)b;
	(void)datos;
	ir_a_la_seleccionada();
}

static void
on_cerrar(GtkButton *b, gpointer datos)
{
	(void)b;
	(void)datos;
	if (ui && ui->dialog)
		gtk_widget_destroy(ui->dialog);
}

static void
on_destroy(GtkWidget *w, gpointer datos)
{
	(void)w;
	(void)datos;
	if (!ui)
		return;
	if (ui->tecleo)
		g_source_remove(ui->tecleo);
	if (ui->modelo)
		g_object_unref(ui->modelo);
	g_list_free_full(ui->entrada, g_free);
	g_list_free_full(ui->notas, (GDestroyNotify)highlight_note_free);
	g_free(ui);
	ui = NULL;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

/* `minimo` en píxeles: sin él, GTK reparte a ojo y la columna del pasaje
 * sale como «Salm…», que es justo el dato por el que uno mira la fila. */
static void
columna(const char *titulo, int col, gboolean markup, gboolean expande,
	int minimo)
{
	GtkCellRenderer *celda = gtk_cell_renderer_text_new();
	GtkTreeViewColumn *c;

	g_object_set(celda, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	c = gtk_tree_view_column_new_with_attributes(
	    titulo, celda, markup ? "markup" : "text", col, NULL);
	gtk_tree_view_column_set_expand(c, expande);
	gtk_tree_view_column_set_resizable(c, TRUE);
	if (minimo > 0)
		gtk_tree_view_column_set_min_width(c, minimo);
	gtk_tree_view_append_column(GTK_TREE_VIEW(ui->tree), c);
}

void
gui_buscar_notas_dialog(GtkWindow *padre)
{
	GtkBuilder *gxml;
	GtkWidget *btn_cerrar;
	GtkTreeSelection *sel;

	if (ui && ui->dialog) {
		gtk_window_present(GTK_WINDOW(ui->dialog));
		return;
	}

	gxml = elim_gtk_builder_new();
	if (!gtk_builder_add_from_resource(
		gxml, "/org/xiphos/ui/buscar-notas.gtkbuilder", NULL)) {
		g_object_unref(gxml);
		gui_generic_warning(_("No se pudo abrir «Buscar en mis notas»."));
		return;
	}

	ui = g_new0(BNUI, 1);
	ui->dialog = UI_GET_ITEM(gxml, "dialog_buscar_notas");
	ui->entry = UI_GET_ITEM(gxml, "entry_consulta");
	ui->chk_regex = UI_GET_ITEM(gxml, "chk_regex");
	ui->chk_mayusculas = UI_GET_ITEM(gxml, "chk_mayusculas");
	ui->lbl_estado = UI_GET_ITEM(gxml, "lbl_estado");
	ui->tree = UI_GET_ITEM(gxml, "tree_resultados");
	ui->btn_ir = UI_GET_ITEM(gxml, "btn_ir");
	btn_cerrar = UI_GET_ITEM(gxml, "btn_cerrar");

	gui_prepare_floating_dialog(
	    GTK_WINDOW(ui->dialog),
	    padre ? padre : (widgets.app ? GTK_WINDOW(widgets.app) : NULL));

	ui->modelo = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree),
				GTK_TREE_MODEL(ui->modelo));
	columna(_("Pasaje"), COL_PASAJE, FALSE, FALSE, 170);
	columna(_("En la nota"), COL_EXTRACTO, TRUE, TRUE, 0);
	columna(_("Versión"), COL_VERSION, FALSE, FALSE, 90);

	cargar_notas();
	poner_todas();

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->tree));
	g_signal_connect(sel, "changed", G_CALLBACK(on_seleccion), NULL);
	g_signal_connect(ui->entry, "changed", G_CALLBACK(on_cambio), NULL);
	g_signal_connect(ui->chk_regex, "toggled", G_CALLBACK(on_opcion), NULL);
	g_signal_connect(ui->chk_mayusculas, "toggled", G_CALLBACK(on_opcion),
			 NULL);
	g_signal_connect(ui->tree, "row-activated",
			 G_CALLBACK(on_fila_activada), NULL);
	g_signal_connect(ui->btn_ir, "clicked", G_CALLBACK(on_ir), NULL);
	g_signal_connect(btn_cerrar, "clicked", G_CALLBACK(on_cerrar), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
	gtk_widget_grab_focus(ui->entry);
}
