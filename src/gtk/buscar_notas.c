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
 *
 * Encima de la lista, filtros por etiqueta («#oración» escrita en la
 * nota), libro y versión (NOTES-TAGS-101); debajo, exportar lo que se ve
 * a Markdown o todas las notas a una copia JSON, e importar una copia
 * (NOTES-EXPORT-101).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gstdio.h>
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
	COL_FECHA,
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
	GtkWidget *cmb_etiqueta;
	GtkWidget *cmb_libro;
	GtkWidget *cmb_version;
	GtkListStore *modelo;

	GList *notas;	 /* HighlightNote*, todas, leídas una vez */
	GList *entrada;	 /* BN_NOTA*, la misma lista en lo que espera el buscador */
	GList *visibles; /* BN_NOTA* de `entrada` que pasan los filtros */
	GList *mostradas; /* BN_NOTA* en la lista ahora mismo, en su orden */
	gboolean llenando; /* rellenando los filtros: sus «changed» no cuentan */
	guint tecleo;
} BNUI;

static BNUI *ui = NULL;

static void buscar(void);

/* --------------------------------------------------------------------
 * Las notas, leídas una vez
 * ------------------------------------------------------------------ */

static void
soltar_notas(void)
{
	g_list_free(ui->mostradas);
	g_list_free(ui->visibles);
	g_list_free_full(ui->entrada, g_free);
	g_list_free_full(ui->notas, (GDestroyNotify)highlight_note_free);
	ui->mostradas = ui->visibles = ui->entrada = ui->notas = NULL;
}

static void
cargar_notas(void)
{
	GList *l;

	soltar_notas();
	ui->notas = highlight_all_notes();

	for (l = ui->notas; l; l = l->next) {
		HighlightNote *h = (HighlightNote *)l->data;
		BN_NOTA *n = g_new0(BN_NOTA, 1);

		n->modulo = h->module;
		n->osisref = h->osisref;
		n->note_key = h->note_key;
		n->frase = h->text;
		n->nota = h->note;
		n->fecha = h->modified > 0 ? h->modified : h->created;
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
	gchar *valida;
	gchar *out;

	if (!osisref || !*osisref)
		return g_strdup("");
	if (!mod || !*mod || !main_is_module((char *)mod))
		return g_strdup(osisref);

	valida = main_get_valid_key(mod, osisref);
	out = g_strdup((valida && *valida) ? valida : osisref);
	g_free(valida);
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

/* «Salmos 23:1» -> «Salmos»: el nombre del libro en el idioma del módulo,
 * sin capítulo ni versículo. */
static gchar *
libro_legible(const gchar *modulo, const gchar *osisref)
{
	gchar *pasaje = pasaje_legible(modulo, osisref);
	gchar *espacio = strrchr(pasaje, ' ');

	if (espacio && espacio != pasaje && strchr(espacio, ':'))
		*espacio = '\0';
	return pasaje;
}

/* La fecha de la nota, corta y en el formato del lector; "" si es de
 * antes de que se guardaran fechas. */
static gchar *
fecha_corta(gint64 t)
{
	GDateTime *utc, *local;
	gchar *out;

	if (t <= 0)
		return g_strdup("");
	utc = g_date_time_new_from_unix_utc(t);
	local = utc ? g_date_time_to_local(utc) : NULL;
	out = local ? g_date_time_format(local, "%x") : g_strdup("");
	if (local)
		g_date_time_unref(local);
	if (utc)
		g_date_time_unref(utc);
	return out;
}

static void
fila(const BN_NOTA *n, const gchar *marcado)
{
	GtkTreeIter it;
	gchar *pasaje = pasaje_legible(n->modulo, n->osisref);
	gchar *fecha = fecha_corta(n->fecha);

	gtk_list_store_append(ui->modelo, &it);
	gtk_list_store_set(ui->modelo, &it,
			   COL_PASAJE, pasaje,
			   COL_EXTRACTO, marcado,
			   COL_VERSION, n->modulo ? n->modulo : "",
			   COL_FECHA, fecha,
			   COL_OSISREF, n->osisref ? n->osisref : "",
			   COL_MODULO, n->modulo ? n->modulo : "",
			   -1);
	g_free(fecha);
	g_free(pasaje);
	ui->mostradas = g_list_prepend(ui->mostradas, (gpointer)n);
}

/* --------------------------------------------------------------------
 * Filtros (NOTES-TAGS-101)
 * ------------------------------------------------------------------ */

static const gchar *
filtro(GtkWidget *cmb)
{
	const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(cmb));
	return (id && *id) ? id : NULL;
}

static gboolean
hay_filtros(void)
{
	return filtro(ui->cmb_etiqueta) || filtro(ui->cmb_libro) ||
	       filtro(ui->cmb_version);
}

static void
aplicar_filtros(void)
{
	g_list_free(ui->visibles);
	ui->visibles = main_buscar_notas_filtrar(
	    ui->entrada, filtro(ui->cmb_etiqueta), filtro(ui->cmb_libro),
	    filtro(ui->cmb_version));
}

static gint
por_nombre(gconstpointer a, gconstpointer b)
{
	return g_utf8_collate(*(const gchar *const *)a, *(const gchar *const *)b);
}

/* Cada filtro ofrece lo que hay en las notas y cuántas: una etiqueta que
 * nadie escribió no sirve de nada en la lista. Lo elegido se conserva si
 * sigue existiendo (al volver a leer las notas después de importar). */
static void
rellenar_filtros(void)
{
	GHashTable *etiquetas = g_hash_table_new_full(g_str_hash, g_str_equal,
						      g_free, NULL);
	GHashTable *libros = g_hash_table_new_full(g_str_hash, g_str_equal,
						   g_free, NULL);
	GHashTable *versiones = g_hash_table_new(g_str_hash, g_str_equal);
	GPtrArray *orden_etiquetas = g_ptr_array_new();
	GPtrArray *orden_libros = g_ptr_array_new_with_free_func(g_free);
	GPtrArray *orden_versiones = g_ptr_array_new();
	gchar *antes_e = g_strdup(filtro(ui->cmb_etiqueta));
	gchar *antes_l = g_strdup(filtro(ui->cmb_libro));
	gchar *antes_v = g_strdup(filtro(ui->cmb_version));
	GList *l;

	ui->llenando = TRUE;
	for (l = ui->entrada; l; l = l->next) {
		BN_NOTA *n = (BN_NOTA *)l->data;
		GPtrArray *et = main_notas_etiquetas(n->nota);
		gchar *libro = main_notas_libro(n->osisref);

		for (guint i = 0; i < et->len; i++) {
			const gchar *e = g_ptr_array_index(et, i);
			gpointer k;
			gpointer v;
			if (g_hash_table_lookup_extended(etiquetas, e, &k, &v)) {
				g_hash_table_insert(etiquetas, g_strdup(e),
						    GINT_TO_POINTER(GPOINTER_TO_INT(v) + 1));
			} else {
				g_hash_table_insert(etiquetas, g_strdup(e),
						    GINT_TO_POINTER(1));
				g_ptr_array_add(orden_etiquetas,
						g_hash_table_lookup_extended(
						    etiquetas, e, &k, NULL) ? k : NULL);
			}
		}
		g_ptr_array_unref(et);
		/* Las notas llegan en orden bíblico: los libros también. */
		if (*libro && !g_hash_table_contains(libros, libro)) {
			g_hash_table_add(libros, g_strdup(libro));
			g_ptr_array_add(orden_libros,
					g_strdup_printf("%s\t%s", libro, n->osisref));
		}
		g_free(libro);
		if (n->modulo && *n->modulo &&
		    !g_hash_table_contains(versiones, n->modulo)) {
			g_hash_table_add(versiones, (gpointer)n->modulo);
			g_ptr_array_add(orden_versiones, (gpointer)n->modulo);
		}
	}
	/* Las etiquetas por orden alfabético; las versiones también. */
	g_ptr_array_sort(orden_etiquetas, por_nombre);
	g_ptr_array_sort(orden_versiones, por_nombre);

	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(ui->cmb_etiqueta));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_etiqueta), "",
				  _("Todas"));
	for (guint i = 0; i < orden_etiquetas->len; i++) {
		const gchar *e = g_ptr_array_index(orden_etiquetas, i);
		gchar *txt = g_strdup_printf(
		    "#%s (%d)", e,
		    GPOINTER_TO_INT(g_hash_table_lookup(etiquetas, e)));
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_etiqueta), e,
					  txt);
		g_free(txt);
	}

	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(ui->cmb_libro));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_libro), "",
				  _("Todos"));
	for (guint i = 0; i < orden_libros->len; i++) {
		gchar **par = g_strsplit(g_ptr_array_index(orden_libros, i), "\t", 2);
		gchar *nombre = libro_legible(NULL, par[1]);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_libro), par[0],
					  nombre);
		g_free(nombre);
		g_strfreev(par);
	}

	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(ui->cmb_version));
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_version), "",
				  _("Todas"));
	for (guint i = 0; i < orden_versiones->len; i++) {
		const gchar *v = g_ptr_array_index(orden_versiones, i);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(ui->cmb_version), v, v);
	}

	if (!antes_e || !gtk_combo_box_set_active_id(GTK_COMBO_BOX(ui->cmb_etiqueta), antes_e))
		gtk_combo_box_set_active(GTK_COMBO_BOX(ui->cmb_etiqueta), 0);
	if (!antes_l || !gtk_combo_box_set_active_id(GTK_COMBO_BOX(ui->cmb_libro), antes_l))
		gtk_combo_box_set_active(GTK_COMBO_BOX(ui->cmb_libro), 0);
	if (!antes_v || !gtk_combo_box_set_active_id(GTK_COMBO_BOX(ui->cmb_version), antes_v))
		gtk_combo_box_set_active(GTK_COMBO_BOX(ui->cmb_version), 0);
	gtk_widget_set_sensitive(ui->cmb_etiqueta, orden_etiquetas->len > 0);
	ui->llenando = FALSE;

	g_free(antes_e);
	g_free(antes_l);
	g_free(antes_v);
	g_ptr_array_unref(orden_etiquetas);
	g_ptr_array_unref(orden_libros);
	g_ptr_array_unref(orden_versiones);
	g_hash_table_destroy(etiquetas);
	g_hash_table_destroy(libros);
	g_hash_table_destroy(versiones);
	aplicar_filtros();
}

/* Sin nada escrito, la lista entera: es el índice de lo que uno lleva
 * escrito, y para eso también se abre este cuadro. */
static void
poner_todas(void)
{
	GList *l;
	int n = 0;

	for (l = ui->visibles; l; l = l->next) {
		BN_NOTA *h = (BN_NOTA *)l->data;
		gchar *renglon = g_strdup(h->nota ? h->nota : "");
		gchar *marcado;

		g_strdelimit(renglon, "\n\r\t", ' ');
		if (g_utf8_strlen(renglon, -1) > 160) {
			gchar *corte = g_utf8_substring(renglon, 0, 157);

			g_free(renglon);
			renglon = g_strconcat(corte, "…", NULL);
			g_free(corte);
		}
		marcado = g_markup_escape_text(renglon, -1);
		fila(h, marcado);
		g_free(marcado);
		g_free(renglon);
		n++;
	}

	if (n == 0 && !ui->entrada)
		gtk_label_set_text(
		    GTK_LABEL(ui->lbl_estado),
		    _("Todavía no has escrito ninguna nota. Se escriben en la "
		      "ficha de debajo del versículo, o subrayando una frase."));
	else if (n == 0)
		gtk_label_set_text(GTK_LABEL(ui->lbl_estado),
				   _("Ninguna nota con esos filtros."));
	else {
		gchar *m = hay_filtros()
			       ? g_strdup_printf(
				     ngettext("%d nota con esos filtros.",
					      "%d notas con esos filtros.", n),
				     n)
			       : g_strdup_printf(
				     ngettext("%d nota escrita.",
					      "%d notas escritas.", n),
				     n);

		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
	}
}

/* El resultado de la búsqueda viene en el orden de las notas que se le
 * dieron: se empareja con ellas avanzando a la par. */
static BN_NOTA *
nota_del_resultado(GList **desde, const BN_RESULTADO *res)
{
	for (GList *l = *desde; l; l = l->next) {
		BN_NOTA *n = (BN_NOTA *)l->data;
		if (!g_strcmp0(n->note_key ? n->note_key : "", res->note_key) &&
		    !g_strcmp0(n->modulo ? n->modulo : "", res->modulo) &&
		    !g_strcmp0(n->osisref ? n->osisref : "", res->osisref)) {
			*desde = l->next;
			return n;
		}
	}
	return NULL;
}

static void
buscar(void)
{
	const gchar *consulta;
	BN_MODO modo;
	gboolean mayus;
	GError *error = NULL;
	GList *r, *l, *desde;
	int n = 0;

	if (!ui)
		return;

	gtk_list_store_clear(ui->modelo);
	g_list_free(ui->mostradas);
	ui->mostradas = NULL;
	gtk_widget_set_sensitive(ui->btn_ir, FALSE);

	consulta = gtk_entry_get_text(GTK_ENTRY(ui->entry));
	if (!consulta || !*consulta) {
		poner_todas();
		ui->mostradas = g_list_reverse(ui->mostradas);
		return;
	}

	modo = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->chk_regex))
		   ? BN_REGEX
		   : BN_TEXTO;
	mayus = gtk_toggle_button_get_active(
	    GTK_TOGGLE_BUTTON(ui->chk_mayusculas));

	r = main_buscar_notas(ui->visibles, consulta, modo, mayus, &error);

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

	desde = ui->visibles;
	for (l = r; l; l = l->next) {
		BN_RESULTADO *res = (BN_RESULTADO *)l->data;
		BN_NOTA *nota = nota_del_resultado(&desde, res);
		gchar *marcado;

		if (!nota)
			continue;
		marcado = extracto_marcado(res);
		fila(nota, marcado);
		g_free(marcado);
		n++;
	}
	main_buscar_notas_libre(r);
	ui->mostradas = g_list_reverse(ui->mostradas);

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
 * Exportar e importar (NOTES-EXPORT-101)
 * ------------------------------------------------------------------ */

static gchar *
hoy(const gchar *formato)
{
	GDateTime *ahora = g_date_time_new_now_local();
	gchar *out = g_date_time_format(ahora, formato);
	g_date_time_unref(ahora);
	return out;
}

/* Dónde guardar, o NULL si se cancela. */
static gchar *
pedir_destino(const gchar *titulo, const gchar *nombre, const gchar *patron,
	      const gchar *tipo)
{
	GtkFileChooserNative *fc = gtk_file_chooser_native_new(
	    titulo, GTK_WINDOW(ui->dialog), GTK_FILE_CHOOSER_ACTION_SAVE,
	    _("_Guardar"), _("_Cancelar"));
	GtkFileFilter *f = gtk_file_filter_new();
	gchar *out = NULL;

	gtk_file_filter_set_name(f, tipo);
	gtk_file_filter_add_pattern(f, patron);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(fc), TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(fc), nombre);
	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT)
		out = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	g_object_unref(fc);
	return out;
}

static void
avisar(GtkMessageType tipo, const gchar *titulo, const gchar *detalle)
{
	GtkWidget *d = gtk_message_dialog_new(
	    GTK_WINDOW(ui->dialog), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	    tipo, GTK_BUTTONS_CLOSE, "%s", titulo);
	if (detalle)
		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
							 "%s", detalle);
	gtk_dialog_run(GTK_DIALOG(d));
	gtk_widget_destroy(d);
}

static void
guardar_texto(const gchar *ruta, const gchar *texto)
{
	GError *error = NULL;

	if (!g_file_set_contents_full(ruta, texto, -1,
				      G_FILE_SET_CONTENTS_CONSISTENT |
					  G_FILE_SET_CONTENTS_DURABLE,
				      0644, &error)) {
		avisar(GTK_MESSAGE_ERROR, _("No se pudo guardar el archivo."),
		       error ? error->message : NULL);
		g_clear_error(&error);
		return;
	}
	{
		gchar *m = g_strdup_printf(_("Guardado en %s"), ruta);
		gtk_label_set_text(GTK_LABEL(ui->lbl_estado), m);
		g_free(m);
	}
}

/* Lo que se ve en la lista, para leerlo o imprimirlo. */
static void
exportar_markdown(void)
{
	gchar *fecha = hoy("%Y-%m-%d");
	gchar *nombre = g_strdup_printf("mis-notas-%s.md", fecha);
	gchar *ruta = pedir_destino(_("Exportar las notas mostradas"), nombre,
				    "*.md", _("Markdown"));
	GList *items = NULL, *propios = NULL;
	gchar *md, *dia;

	g_free(nombre);
	g_free(fecha);
	if (!ruta)
		return;
	for (GList *l = ui->mostradas; l; l = l->next) {
		const BN_NOTA *n = (const BN_NOTA *)l->data;
		NotesMdItem *it = g_new0(NotesMdItem, 1);
		gchar *libro = libro_legible(n->modulo, n->osisref);
		gchar *pasaje = pasaje_legible(n->modulo, n->osisref);
		const HighlightNote *h = NULL;

		for (GList *k = ui->notas; k && !h; k = k->next)
			if (((HighlightNote *)k->data)->note_key == n->note_key)
				h = (const HighlightNote *)k->data;
		gchar *fechas = h ? highlight_note_dates_text(h->created, h->modified)
				  : NULL;
		it->libro = libro;
		it->pasaje = pasaje;
		it->modulo = n->modulo;
		it->frase = n->frase;
		it->nota = n->nota;
		it->fechas = fechas;
		items = g_list_append(items, it);
		propios = g_list_prepend(propios, libro);
		propios = g_list_prepend(propios, pasaje);
		propios = g_list_prepend(propios, fechas);
	}
	dia = hoy("%x");
	md = notes_export_markdown(items, dia);
	guardar_texto(ruta, md);
	g_free(md);
	g_free(dia);
	g_list_free_full(items, g_free);
	g_list_free_full(propios, g_free);
	g_free(ruta);
}

/* Todas, con sus fechas y sus enlaces: la copia de seguridad. */
static void
exportar_json(void)
{
	gchar *fecha = hoy("%Y-%m-%d");
	gchar *nombre = g_strdup_printf("notas-biblia-elim-%s.json", fecha);
	gchar *ruta = pedir_destino(_("Copia de todas las notas"), nombre,
				    "*.json", _("Copia de notas (JSON)"));
	gchar *json;

	g_free(nombre);
	g_free(fecha);
	if (!ruta)
		return;
	json = highlight_notes_export_json();
	guardar_texto(ruta, json);
	g_free(json);
	g_free(ruta);
}

static void
on_exportar_md(GtkMenuItem *item, gpointer datos)
{
	(void)item;
	(void)datos;
	exportar_markdown();
}

static void
on_exportar_json(GtkMenuItem *item, gpointer datos)
{
	(void)item;
	(void)datos;
	exportar_json();
}

static gboolean
destruir_menu(gpointer menu)
{
	gtk_widget_destroy(GTK_WIDGET(menu));
	return G_SOURCE_REMOVE;
}

static void
on_menu_cerrado(GtkMenuShell *menu, gpointer datos)
{
	(void)datos;
	g_idle_add(destruir_menu, menu);
}

static void
on_exportar(GtkButton *b, gpointer datos)
{
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *md = gtk_menu_item_new_with_label(
	    _("Las notas mostradas, en Markdown (para leer o imprimir)…"));
	GtkWidget *js = gtk_menu_item_new_with_label(
	    _("Copia completa de todas las notas (JSON)…"));

	(void)datos;
	gtk_widget_set_sensitive(md, ui->mostradas != NULL);
	g_signal_connect(md, "activate", G_CALLBACK(on_exportar_md), NULL);
	g_signal_connect(js, "activate", G_CALLBACK(on_exportar_json), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), md);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), js);
	gtk_widget_show_all(menu);
	gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(b), NULL);
	/* Después de que el elemento elegido haya hecho lo suyo. */
	g_signal_connect(menu, "deactivate", G_CALLBACK(on_menu_cerrado), NULL);
	gtk_menu_popup_at_widget(GTK_MENU(menu), GTK_WIDGET(b),
				 GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST,
				 NULL);
}

static void
on_importar(GtkButton *b, gpointer datos)
{
	GtkFileChooserNative *fc = gtk_file_chooser_native_new(
	    _("Importar una copia de notas"), GTK_WINDOW(ui->dialog),
	    GTK_FILE_CHOOSER_ACTION_OPEN, _("_Importar"), _("_Cancelar"));
	GtkFileFilter *f = gtk_file_filter_new();
	gchar *ruta = NULL, *json = NULL, *copia = NULL;
	GError *error = NULL;
	NotesImportResult r;

	(void)b;
	(void)datos;
	gtk_file_filter_set_name(f, _("Copia de notas (JSON)"));
	gtk_file_filter_add_pattern(f, "*.json");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(fc), f);
	if (gtk_native_dialog_run(GTK_NATIVE_DIALOG(fc)) == GTK_RESPONSE_ACCEPT)
		ruta = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(fc));
	g_object_unref(fc);
	if (!ruta)
		return;

	if (!g_file_get_contents(ruta, &json, NULL, &error) ||
	    !highlight_notes_import_json(json, &r, &copia, &error)) {
		avisar(GTK_MESSAGE_ERROR, _("No se ha importado nada."),
		       error ? error->message : NULL);
		g_clear_error(&error);
		g_free(json);
		g_free(ruta);
		return;
	}

	{
		GString *d = g_string_new(NULL);
		guint nuevas = r.added + r.extra;

		g_string_append_printf(
		    d, ngettext("%u nota nueva.", "%u notas nuevas.", nuevas),
		    nuevas);
		if (r.extra)
			g_string_append_printf(
			    d, ngettext(" %u de ellas va junto a otra nota tuya del mismo versículo.",
					" %u de ellas van junto a otra nota tuya del mismo versículo.",
					r.extra),
			    r.extra);
		if (r.identical)
			g_string_append_printf(
			    d, ngettext("\n%u ya estaba.", "\n%u ya estaban.", r.identical),
			    r.identical);
		if (r.links_added)
			g_string_append_printf(
			    d, ngettext("\n%u enlace nuevo.", "\n%u enlaces nuevos.",
					r.links_added),
			    r.links_added);
		if (r.conflicts)
			g_string_append_printf(
			    d, ngettext("\n%u subrayado o enlace distinto del tuyo: se conservó el tuyo.",
					"\n%u subrayados o enlaces distintos de los tuyos: se conservaron los tuyos.",
					r.conflicts),
			    r.conflicts);
		if (r.invalid)
			g_string_append_printf(
			    d, ngettext("\n%u entrada no válida, ignorada.",
					"\n%u entradas no válidas, ignoradas.", r.invalid),
			    r.invalid);
		if (copia)
			g_string_append_printf(
			    d, _("\n\nTus notas de antes quedaron copiadas en %s"),
			    copia);
		avisar(GTK_MESSAGE_INFO, _("Copia importada."), d->str);
		g_string_free(d, TRUE);
	}
	if (copia) {
		cargar_notas();
		rellenar_filtros();
		buscar();
		main_display_bible(NULL, settings.currentverse);
	}
	g_free(copia);
	g_free(json);
	g_free(ruta);
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
on_filtro(GtkComboBox *cmb, gpointer datos)
{
	(void)cmb;
	(void)datos;
	if (!ui || ui->llenando)
		return;
	aplicar_filtros();
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
	soltar_notas();
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
	GtkWidget *btn_cerrar, *btn_exportar, *btn_importar;
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
	btn_exportar = UI_GET_ITEM(gxml, "btn_exportar");
	btn_importar = UI_GET_ITEM(gxml, "btn_importar");
	ui->cmb_etiqueta = UI_GET_ITEM(gxml, "cmb_etiqueta");
	ui->cmb_libro = UI_GET_ITEM(gxml, "cmb_libro");
	ui->cmb_version = UI_GET_ITEM(gxml, "cmb_version");

	gui_prepare_floating_dialog(
	    GTK_WINDOW(ui->dialog),
	    padre ? padre : (widgets.app ? GTK_WINDOW(widgets.app) : NULL));

	ui->modelo = gtk_list_store_new(N_COLS, G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_STRING,
					G_TYPE_STRING, G_TYPE_STRING);
	gtk_tree_view_set_model(GTK_TREE_VIEW(ui->tree),
				GTK_TREE_MODEL(ui->modelo));
	columna(_("Pasaje"), COL_PASAJE, FALSE, FALSE, 170);
	columna(_("En la nota"), COL_EXTRACTO, TRUE, TRUE, 0);
	columna(_("Versión"), COL_VERSION, FALSE, FALSE, 90);
	columna(_("Fecha"), COL_FECHA, FALSE, FALSE, 80);

	cargar_notas();
	rellenar_filtros();
	buscar();

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
	g_signal_connect(btn_exportar, "clicked", G_CALLBACK(on_exportar), NULL);
	g_signal_connect(btn_importar, "clicked", G_CALLBACK(on_importar), NULL);
	g_signal_connect(ui->cmb_etiqueta, "changed", G_CALLBACK(on_filtro), NULL);
	g_signal_connect(ui->cmb_libro, "changed", G_CALLBACK(on_filtro), NULL);
	g_signal_connect(ui->cmb_version, "changed", G_CALLBACK(on_filtro), NULL);
	g_signal_connect(ui->dialog, "destroy", G_CALLBACK(on_destroy), NULL);

	g_object_unref(gxml);
	gtk_widget_show(ui->dialog);
	gtk_widget_grab_focus(ui->entry);
}
