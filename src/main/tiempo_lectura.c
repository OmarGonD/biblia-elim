/*
 * Biblia Elim
 * tiempo_lectura.c - cuánto se tarda en leer lo de hoy
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "main/tiempo_lectura.h"
#include "main/settings.h"
#include "main/texto_verso.h"

#include "gui/debug_glib_null.h"

/* Velocidades de lectura silenciosa, en palabras por minuto.
 *
 * La franja es ancha a posta. Un adulto lee prosa corriente a unas 240
 * ppm, pero la Biblia no es prosa corriente: hay nombres largos que
 * frenan, genealogías que se saltan y frases que se releen. Y sobre
 * todo, un lector no lee como otro. Dar un número solo -- "9 min" --
 * sería fingir una precisión que no tenemos; la franja dice la verdad,
 * que es que depende.
 *
 * Estos dos números son el único sitio donde se toca eso. */
#define PPM_RAPIDO 220
#define PPM_LENTO 150

/* Contar palabras cuesta pedirle al motor el texto de cada capítulo, y
 * el globo de la ventana principal se arma cada vez que el ratón pasa
 * por encima. Se recuerda lo último, que es siempre la lectura de hoy:
 * un solo hueco basta, no hace falta un mapa. */
static gchar *cache_clave = NULL;
static int cache_palabras = 0;

/* La clave del hueco lleva el módulo, porque el mismo capítulo en otra
 * versión son otras palabras. */
static gchar *
clave_de(GList *referencias)
{
	GString *g = g_string_new(settings.MainWindowModule
				      ? settings.MainWindowModule
				      : "");
	GList *l;

	for (l = referencias; l; l = l->next)
		g_string_append_printf(g, "|%s", (const char *)l->data);
	return g_string_free(g, FALSE);
}

/* Palabras de una cadena. Se cuentan los tramos que no son espacio; los
 * bytes de continuación de UTF-8 son todos >= 0x80, así que mirar el
 * espacio ASCII no parte ninguna palabra acentuada. */
static int
palabras_de(const char *texto)
{
	const char *p = texto;
	int n = 0;
	gboolean dentro = FALSE;

	if (!texto)
		return 0;
	for (; *p; ++p) {
		if (g_ascii_isspace((unsigned char)*p))
			dentro = FALSE;
		else if (!dentro) {
			dentro = TRUE;
			++n;
		}
	}
	return n;
}

int
main_tiempo_palabras(GList *referencias)
{
	gchar *clave;
	GList *l;
	int total = 0;

	if (!referencias || !settings.MainWindowModule ||
	    !*settings.MainWindowModule)
		return 0;

	clave = clave_de(referencias);
	if (cache_clave && !strcmp(cache_clave, clave)) {
		g_free(clave);
		return cache_palabras;
	}

	/* main_texto_de() le devuelve al módulo su clave después de cada
	 * capítulo. Es una lectura de un versículo de más por capítulo,
	 * al lado de renderizar el capítulo entero no se nota, y a cambio
	 * el cuidado de no moverle la lectura al lector vive en un solo
	 * sitio. */
	for (l = referencias; l; l = l->next) {
		gchar *texto = main_texto_de((const char *)l->data);

		if (texto) {
			total += palabras_de(texto);
			g_free(texto);
		}
	}

	g_free(cache_clave);
	cache_clave = clave;
	cache_palabras = total;
	return total;
}

/* Hacia arriba y nunca cero: decir "0 min" de algo que hay que leer no
 * ayuda a nadie. */
static int
minutos(int palabras, int ppm)
{
	int m = (palabras + ppm - 1) / ppm;

	return (m < 1) ? 1 : m;
}

gchar *
main_tiempo_texto(GList *referencias)
{
	int palabras = main_tiempo_palabras(referencias);
	int bajo, alto;

	if (palabras < 1)
		return NULL;

	bajo = minutos(palabras, PPM_RAPIDO);
	alto = minutos(palabras, PPM_LENTO);

	/* "unos 1 min" no se dice. Y con esa cuenta el pasaje se lee en
	 * menos de un minuto hasta leyéndolo despacio, que es lo que hay
	 * que decir. */
	if (alto <= 1)
		return g_strdup(_("menos de 1 min"));
	if (bajo >= alto)
		return g_strdup_printf(_("unos %d min"), alto);
	/* Raya, no guion: es una franja, no una palabra compuesta. */
	return g_strdup_printf(_("%d–%d min"), bajo, alto);
}
