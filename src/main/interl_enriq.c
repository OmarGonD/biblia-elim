/*
 * Biblia Elim
 * interl_enriq.c - ficha de estudio enriquecida de una palabra del interlineal
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
#include <glib.h>
#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "main/interl_enriq.h"

#define ARCHIVO "interlineal_enriquecido.json"

/* "ref|strong" -> JsonObject* (propiedad de tabla). */
static GHashTable *tabla;

static gchar *
clave(const char *ref, const char *strong)
{
	return g_strdup_printf("%s|%s", ref, strong);
}

static const char *
cadena(JsonObject *o, const char *campo)
{
	JsonNode *n;
	const char *s;

	if (!json_object_has_member(o, campo))
		return NULL;
	n = json_object_get_member(o, campo);
	if (!n || json_node_get_value_type(n) != G_TYPE_STRING)
		return NULL;
	s = json_node_get_string(n);
	return s && *s ? s : NULL;
}

static JsonArray *
arreglo(JsonObject *o, const char *campo)
{
	JsonNode *n;

	if (!json_object_has_member(o, campo))
		return NULL;
	n = json_object_get_member(o, campo);
	if (!n || !JSON_NODE_HOLDS_ARRAY(n))
		return NULL;
	return json_node_get_array(n);
}

void
main_interl_enriq_liberar(void)
{
	if (tabla)
		g_hash_table_destroy(tabla);
	tabla = NULL;
}

gint
main_interl_enriq_cargar(const char *ruta)
{
	JsonParser *p;
	JsonNode *raiz;
	JsonArray *arr;
	GHashTable *nueva;
	guint i;

	if (!ruta)
		return -1;
	p = json_parser_new();
	if (!json_parser_load_from_file(p, ruta, NULL)) {
		g_object_unref(p);
		return -1;
	}
	raiz = json_parser_get_root(p);
	if (!raiz || !JSON_NODE_HOLDS_ARRAY(raiz)) {
		g_object_unref(p);
		return -1;
	}
	arr = json_node_get_array(raiz);
	nueva = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
				      (GDestroyNotify)json_object_unref);
	for (i = 0; i < json_array_get_length(arr); i++) {
		JsonNode *n = json_array_get_element(arr, i);
		JsonObject *o;
		const char *ref, *strong;

		if (!n || !JSON_NODE_HOLDS_OBJECT(n))
			continue;
		o = json_node_get_object(n);
		ref = cadena(o, "ref");
		strong = cadena(o, "strong");
		if (!ref || !strong)
			continue;
		g_hash_table_replace(nueva, clave(ref, strong),
				     json_object_ref(o));
	}
	g_object_unref(p);
	main_interl_enriq_liberar();
	tabla = nueva;
	return (gint)g_hash_table_size(tabla);
}

gint
main_interl_enriq_cargar_de(const char *dir)
{
	gchar *ruta;
	gint n;

	if (!dir)
		return 0;
	ruta = g_build_filename(dir, ARCHIVO, NULL);
	n = g_file_test(ruta, G_FILE_TEST_IS_REGULAR)
		? main_interl_enriq_cargar(ruta) : 0;
	g_free(ruta);
	return n;
}

/* <p class="clase"><b>Título</b> texto</p>; no hace nada sin texto. */
static void
parrafo(GString *h, const char *clase, const char *titulo, const char *texto)
{
	gchar *t, *x;

	if (!texto)
		return;
	t = g_markup_escape_text(titulo, -1);
	x = g_markup_escape_text(texto, -1);
	g_string_append_printf(h, "<p class=\"%s\"><b>%s</b> %s</p>", clase, t, x);
	g_free(t);
	g_free(x);
}

static void
lista(GString *h, const char *clase, const char *titulo, JsonArray *a)
{
	GString *items = g_string_new(NULL);
	guint i;
	gchar *t;

	for (i = 0; a && i < json_array_get_length(a); i++) {
		JsonNode *n = json_array_get_element(a, i);
		const char *s = n && json_node_get_value_type(n) == G_TYPE_STRING
				    ? json_node_get_string(n) : NULL;
		gchar *e;

		if (!s || !*s)
			continue;
		e = g_markup_escape_text(s, -1);
		g_string_append_printf(items, "<li>%s</li>", e);
		g_free(e);
	}
	if (items->len) {
		t = g_markup_escape_text(titulo, -1);
		g_string_append_printf(h, "<p class=\"%s\"><b>%s</b></p><ul>%s</ul>",
				       clase, t, items->str);
		g_free(t);
	}
	g_string_free(items, TRUE);
}

static void
traducciones(GString *h, JsonArray *a)
{
	GString *items = g_string_new(NULL);
	guint i;

	for (i = 0; a && i < json_array_get_length(a); i++) {
		JsonNode *n = json_array_get_element(a, i);
		JsonObject *o;
		const char *v, *x, *nota;
		gchar *ev, *ex;

		if (!n || !JSON_NODE_HOLDS_OBJECT(n))
			continue;
		o = json_node_get_object(n);
		v = cadena(o, "version");
		x = cadena(o, "texto");
		if (!v || !x)
			continue;
		nota = cadena(o, "nota");
		ev = g_markup_escape_text(v, -1);
		ex = g_markup_escape_text(x, -1);
		g_string_append_printf(items, "<li><b>%s:</b> %s", ev, ex);
		if (nota) {
			gchar *en = g_markup_escape_text(nota, -1);

			g_string_append_printf(items,
					       " <span class=\"nota\">(%s)</span>", en);
			g_free(en);
		}
		g_string_append(items, "</li>");
		g_free(ev);
		g_free(ex);
	}
	if (items->len)
		g_string_append_printf(h,
				       "<p class=\"enr-t\"><b>%s</b></p><ul>%s</ul>",
				       _("Cómo la traducen las versiones"), items->str);
	g_string_free(items, TRUE);
}

gchar *
main_interl_enriq_html(const char *ref, const char *strong)
{
	JsonObject *o;
	GString *h;
	gchar *k;
	const char *certeza;

	if (!tabla || !ref || !*ref || !strong || !*strong)
		return NULL;
	k = clave(ref, strong);
	o = g_hash_table_lookup(tabla, k);
	g_free(k);
	if (!o)
		return NULL;

	h = g_string_new("<div class=\"enr\">");
	parrafo(h, "enr-ctx", _("En este versículo:"),
		cadena(o, "sentido_en_contexto"));
	parrafo(h, "enr-con", _("Construcción:"), cadena(o, "construccion"));
	parrafo(h, "enr-mat", _("Matiz:"), cadena(o, "matiz"));
	lista(h, "enr-t", _("Significados en el Nuevo Testamento"),
	      arreglo(o, "rango_semantico"));
	traducciones(h, arreglo(o, "traducciones_comparadas"));
	parrafo(h, "enr-nt", _("Sobre las diferencias:"),
		cadena(o, "notas_traduccion"));
	lista(h, "enr-t", _("Otros usos"), arreglo(o, "otros_usos"));
	certeza = cadena(o, "nivel_certeza");
	parrafo(h, "enr-cz", _("Certeza:"), certeza);
	g_string_append(h, "</div>");

	/* Sin ningún campo útil no hay ficha que enseñar. */
	if (!strcmp(h->str, "<div class=\"enr\"></div>")) {
		g_string_free(h, TRUE);
		return NULL;
	}
	return g_string_free(h, FALSE);
}
