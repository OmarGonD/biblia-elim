/*
 * Biblia Elim
 * interl_enriq.c - ficha de estudio enriquecida de una palabra del interlineal (lee fichas.sqlite)
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
#include <sqlite3.h>

#include "main/interl_enriq.h"

#define FORMATO "fichas-v1"
#define COLUMNAS                                                                 \
	"posicion,strong,ref_estandar,glosa_interlineal,rango_semantico,"          \
	"construccion,sentido_en_contexto,matiz,variantes_textuales,"              \
	"notas_traduccion,otros_usos,nivel_certeza"

typedef struct {
	gint pos;
	gchar *strong, *ref_estandar, *rango, *construccion, *sentido, *matiz;
	gchar *variantes, *notas, *otros, *certeza;
} Ficha;

static sqlite3 *db;
static gboolean intentado;
/* Caché de UN versículo: el último consultado. */
static gchar *verso_ref;
static GPtrArray *verso_fichas;

static void
ficha_libre(gpointer p)
{
	Ficha *f = p;

	g_free(f->strong);
	g_free(f->ref_estandar);
	g_free(f->rango);
	g_free(f->construccion);
	g_free(f->sentido);
	g_free(f->matiz);
	g_free(f->variantes);
	g_free(f->notas);
	g_free(f->otros);
	g_free(f->certeza);
	g_free(f);
}

static void
olvidar_verso(void)
{
	g_free(verso_ref);
	verso_ref = NULL;
	if (verso_fichas)
		g_ptr_array_free(verso_fichas, TRUE);
	verso_fichas = NULL;
}

void
main_interl_enriq_liberar(void)
{
	olvidar_verso();
	if (db)
		sqlite3_close_v2(db);
	db = NULL;
	intentado = TRUE;	/* tras liberar no se vuelve a buscar sola */
}

static gchar *
texto(sqlite3_stmt *s, int col)
{
	const unsigned char *t = sqlite3_column_text(s, col);

	return t && *t ? g_strdup((const char *)t) : NULL;
}

/* Devuelve el número de fichas de la base ya abierta, o -1 si no es una base de fichas de este formato. */
static gint
validar(sqlite3 *base)
{
	sqlite3_stmt *s = NULL;
	gint n = -1;
	gboolean ok = FALSE;

	if (sqlite3_prepare_v2(base, "PRAGMA user_version", -1, &s, NULL) != SQLITE_OK)
		return -1;
	ok = sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 0) == 1;
	sqlite3_finalize(s);
	if (!ok || sqlite3_prepare_v2(base, "SELECT key,value FROM metadata", -1, &s, NULL) != SQLITE_OK)
		return -1;
	ok = FALSE;
	n = 0;
	while (sqlite3_step(s) == SQLITE_ROW) {
		const char *k = (const char *)sqlite3_column_text(s, 0);
		const char *v = (const char *)sqlite3_column_text(s, 1);

		if (!k || !v)
			continue;
		if (!strcmp(k, "formato") && !strcmp(v, FORMATO))
			ok = TRUE;
		else if (!strcmp(k, "fichas"))
			n = (gint)g_ascii_strtoll(v, NULL, 10);
	}
	sqlite3_finalize(s);
	return ok ? n : -1;
}

gint
main_interl_enriq_cargar(const char *ruta)
{
	sqlite3 *nueva = NULL;
	gint n;

	intentado = TRUE;
	if (!ruta || !g_file_test(ruta, G_FILE_TEST_IS_REGULAR))
		return -1;
	if (sqlite3_open_v2(ruta, &nueva, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL) != SQLITE_OK) {
		sqlite3_close_v2(nueva);
		return -1;
	}
	n = validar(nueva);
	if (n < 0) {
		sqlite3_close_v2(nueva);
		return -1;
	}
	olvidar_verso();
	if (db)
		sqlite3_close_v2(db);
	db = nueva;
	return n;
}

gint
main_interl_enriq_cargar_predeterminada(void)
{
	const char *forzada = g_getenv("BIBLIA_ELIM_FICHAS");
	gint n = 0;

	intentado = TRUE;
	/* Una ruta explícita (pruebas, ejecuciones de desarrollo) se mira sola: no cae a otra copia. */
	if (forzada && *forzada)
		return g_file_test(forzada, G_FILE_TEST_IS_REGULAR) ? main_interl_enriq_cargar(forzada) : 0;
#ifdef XIPHOS_BUILD_FICHAS
	if (g_file_test(XIPHOS_BUILD_FICHAS, G_FILE_TEST_IS_REGULAR))
		n = main_interl_enriq_cargar(XIPHOS_BUILD_FICHAS);
#endif
#ifdef SHARE_DIR
	if (n <= 0) {
		gchar *r = g_build_filename(SHARE_DIR, "fichas.sqlite", NULL);

		n = g_file_test(r, G_FILE_TEST_IS_REGULAR) ? main_interl_enriq_cargar(r) : 0;
		g_free(r);
	}
#endif
	return n < 0 ? 0 : n;
}

/* Lee las fichas de UN versículo (consulta indexada por ref_tisch); recuerda solo el último. */
static GPtrArray *
fichas_del_verso(const char *ref)
{
	sqlite3_stmt *s = NULL;
	GPtrArray *a;

	if (verso_ref && !strcmp(verso_ref, ref))
		return verso_fichas;
	olvidar_verso();
	if (sqlite3_prepare_v2(db, "SELECT " COLUMNAS " FROM fichas WHERE ref_tisch=?1 ORDER BY posicion", -1, &s, NULL) != SQLITE_OK)
		return NULL;
	sqlite3_bind_text(s, 1, ref, -1, SQLITE_STATIC);
	a = g_ptr_array_new_with_free_func(ficha_libre);
	while (sqlite3_step(s) == SQLITE_ROW) {
		Ficha *f = g_new0(Ficha, 1);

		f->pos = sqlite3_column_int(s, 0);
		f->strong = texto(s, 1);
		f->ref_estandar = texto(s, 2);
		f->rango = texto(s, 4);
		f->construccion = texto(s, 5);
		f->sentido = texto(s, 6);
		f->matiz = texto(s, 7);
		f->variantes = texto(s, 8);
		f->notas = texto(s, 9);
		f->otros = texto(s, 10);
		f->certeza = texto(s, 11);
		g_ptr_array_add(a, f);
	}
	sqlite3_finalize(s);
	verso_ref = g_strdup(ref);
	verso_fichas = a;
	return a;
}

static const Ficha *
buscar(GPtrArray *a, gint pos, const char *strong)
{
	const Ficha *hallada = NULL;
	guint i, n = 0;

	for (i = 0; a && i < a->len; i++) {
		const Ficha *f = g_ptr_array_index(a, i);

		if (!f->strong || g_ascii_strcasecmp(f->strong, strong))
			continue;
		if (pos > 0) {
			if (f->pos == pos)
				return f;
			continue;
		}
		hallada = f;
		n++;
	}
	return pos > 0 || n != 1 ? NULL : hallada;	/* varias apariciones: no se adivina */
}

/* <p class="clase"><b>Título</b> texto</p>; no hace nada sin texto. */
static void
parrafo(GString *h, const char *clase, const char *titulo, const char *texto)
{
	gchar *t, *x;

	if (!texto || !*texto)
		return;
	t = g_markup_escape_text(titulo, -1);
	x = g_markup_escape_text(texto, -1);
	g_string_append_printf(h, "<p class=\"%s\"><b>%s</b> %s</p>", clase, t, x);
	g_free(t);
	g_free(x);
}

/* Lista <ul> desde un arreglo JSON de cadenas. */
static void
lista(GString *h, const char *clase, const char *titulo, const char *json)
{
	GString *items;
	JsonParser *p;
	JsonNode *raiz;
	JsonArray *a;
	guint i;

	if (!json || !*json)
		return;
	p = json_parser_new();
	if (!json_parser_load_from_data(p, json, -1, NULL) || !(raiz = json_parser_get_root(p)) ||
	    !JSON_NODE_HOLDS_ARRAY(raiz)) {
		g_object_unref(p);
		return;
	}
	a = json_node_get_array(raiz);
	items = g_string_new(NULL);
	for (i = 0; i < json_array_get_length(a); i++) {
		JsonNode *n = json_array_get_element(a, i);
		const char *s = n && json_node_get_value_type(n) == G_TYPE_STRING ? json_node_get_string(n) : NULL;
		gchar *e;

		if (!s || !*s)
			continue;
		e = g_markup_escape_text(s, -1);
		g_string_append_printf(items, "<li>%s</li>", e);
		g_free(e);
	}
	if (items->len) {
		gchar *t = g_markup_escape_text(titulo, -1);

		g_string_append_printf(h, "<p class=\"%s\"><b>%s</b></p><ul>%s</ul>", clase, t, items->str);
		g_free(t);
	}
	g_string_free(items, TRUE);
	g_object_unref(p);
}

/* Citas de SpaRV y TorresAmat en la numeración estándar. TorresAmat se omite en versículos con restos de OCR. */
static void
citas(GString *h, const char *ref_estandar)
{
	sqlite3_stmt *s = NULL;
	GString *items;

	if (!ref_estandar || !*ref_estandar ||
	    sqlite3_prepare_v2(db, "SELECT version,texto FROM citas WHERE ref_estandar=?1 AND ocr_sospechoso=0 ORDER BY version",
			       -1, &s, NULL) != SQLITE_OK)
		return;
	sqlite3_bind_text(s, 1, ref_estandar, -1, SQLITE_STATIC);
	items = g_string_new(NULL);
	while (sqlite3_step(s) == SQLITE_ROW) {
		gchar *v = texto(s, 0), *x = texto(s, 1);

		if (v && x) {
			gchar *ev = g_markup_escape_text(v, -1), *ex = g_markup_escape_text(x, -1);

			g_string_append_printf(items, "<li><b>%s:</b> %s</li>", ev, ex);
			g_free(ev);
			g_free(ex);
		}
		g_free(v);
		g_free(x);
	}
	sqlite3_finalize(s);
	if (items->len)
		g_string_append_printf(h, "<p class=\"enr-t\"><b>%s</b></p><ul>%s</ul>",
				       _("Cómo la traducen las versiones"), items->str);
	g_string_free(items, TRUE);
}

gchar *
main_interl_enriq_html(const char *ref_tisch, gint posicion, const char *strong)
{
	const Ficha *f;
	GString *h;
	gsize base;

	if (!ref_tisch || !*ref_tisch || !strong || !*strong)
		return NULL;
	if (!db && !intentado)
		main_interl_enriq_cargar_predeterminada();
	if (!db)
		return NULL;
	f = buscar(fichas_del_verso(ref_tisch), posicion, strong);
	if (!f)
		return NULL;

	h = g_string_new("<div class=\"enr\">");
	base = h->len;
	parrafo(h, "enr-ctx", _("En este versículo:"), f->sentido);
	parrafo(h, "enr-con", _("Construcción:"), f->construccion);
	parrafo(h, "enr-mat", _("Matiz:"), f->matiz);
	parrafo(h, "enr-var", _("Variantes textuales:"), f->variantes);
	lista(h, "enr-t", _("Significados en el Nuevo Testamento"), f->rango);
	if (h->len > base)	/* las citas solas no hacen una ficha */
		citas(h, f->ref_estandar);
	parrafo(h, "enr-nt", _("Sobre las diferencias:"), f->notas);
	lista(h, "enr-t", _("Otros usos"), f->otros);
	parrafo(h, "enr-cz", _("Certeza:"), f->certeza);
	g_string_append(h, "</div>");
	if (h->len == base + strlen("</div>")) {
		g_string_free(h, TRUE);
		return NULL;
	}
	return g_string_free(h, FALSE);
}
