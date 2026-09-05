/*
 * Biblia Elim
 * glosa.c - la ficha del término, sin repetirse
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

#include "main/glosa.h"

#define PU_MARCA_RV "En Reina-Valera 1909:"

/* Dónde empieza la coletilla de la Reina-Valera, o NULL. */
static const char *
marca_rv(const char *definicion)
{
	if (!definicion)
		return NULL;
	return strstr(definicion, PU_MARCA_RV);
}

gchar *
main_glosa_definicion(const char *definicion, const char *glosa)
{
	const char *rv;
	gchar *cuerpo;
	gchar *sin_glosa;

	if (!definicion || !*definicion)
		return g_strdup("");

	rv = marca_rv(definicion);
	cuerpo = rv ? g_strndup(definicion, rv - definicion)
		    : g_strdup(definicion);
	g_strstrip(cuerpo);

	/* "Vencido. Hacer peor…" con la glosa "vencido" delante: fuera, que
	 * ya está arriba en grande. Solo si detrás queda algo: cuando la
	 * definición es únicamente la glosa, se deja como está. */
	sin_glosa = NULL;
	if (glosa && *glosa) {
		gsize n = strlen(glosa);
		gchar *cabeza = g_utf8_casefold(cuerpo, (gssize)n);
		gchar *g_baja = g_utf8_casefold(glosa, -1);

		if (!g_strcmp0(cabeza, g_baja) && cuerpo[n] == '.') {
			const char *resto = cuerpo + n + 1;

			while (*resto == ' ')
				resto++;
			if (*resto)
				sin_glosa = g_strdup(resto);
		}
		g_free(cabeza);
		g_free(g_baja);
	}
	if (sin_glosa) {
		g_free(cuerpo);
		cuerpo = sin_glosa;
	}

	g_strstrip(cuerpo);
	/* Un punto suelto al empezar, de recortes anteriores. */
	while (*cuerpo == '.' || *cuerpo == ' ') {
		gchar *t = g_strdup(cuerpo + 1);

		g_free(cuerpo);
		cuerpo = t;
	}
	return cuerpo;
}

gchar *
main_glosa_rv1909(const char *definicion)
{
	const char *rv = marca_rv(definicion);
	gchar *lista;
	gsize n;

	if (!rv)
		return NULL;
	lista = g_strdup(rv + strlen(PU_MARCA_RV));
	g_strstrip(lista);
	n = strlen(lista);
	while (n && (lista[n - 1] == '.' || lista[n - 1] == ' '))
		lista[--n] = '\0';
	if (!*lista) {
		g_free(lista);
		return NULL;
	}
	return lista;
}
