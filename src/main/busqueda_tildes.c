/*
 * Biblia Elim
 * busqueda_tildes.c - buscar en español sin pelearse con las tildes
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "main/busqueda_tildes.h"

/* Nadie escribe «Espíritu» con la tilde puesta cuando busca deprisa, y
 * la Reina-Valera tampoco acentúa igual en todas sus ediciones. Aquí
 * está la equivalencia: la letra desnuda y todas las formas que ha de
 * aceptar, en ambas cajas, porque Sword compara byte a byte y su
 * REG_ICASE solo entiende de ASCII. */
static const struct
{
	gunichar base;
	const gchar *formas;
} clases[] = {
	{'a', "a|á|à|ä|â|A|Á|À|Ä|Â"},
	{'e', "e|é|è|ë|ê|E|É|È|Ë|Ê"},
	{'i', "i|í|ì|ï|î|I|Í|Ì|Ï|Î"},
	{'o', "o|ó|ò|ö|ô|O|Ó|Ò|Ö|Ô"},
	{'u', "u|ú|ù|ü|û|U|Ú|Ù|Ü|Û"},
	{'n', "n|ñ|N|Ñ"},
	{'c', "c|ç|C|Ç"},
	{'y', "y|ý|Y|Ý"},
};

/* Lo que en una expresión regular significa algo más que sí mismo. Si
 * la consulta trae uno de estos, es que la escribió alguien que sabe lo
 * que hace, y no la tocamos. */
#define METACARACTERES "[](){}|*+?.^$\\"

/* La letra a secas: sin tilde y en minúscula. Devuelve 0 si el carácter
 * no deja nada al quitarle los adornos. */
static gunichar
letra_desnuda(gunichar c)
{
	gchar utf8[8];
	gchar *nfd;
	const gchar *p;
	gunichar base = 0;
	gint len;

	len = g_unichar_to_utf8(c, utf8);
	utf8[len] = '\0';
	nfd = g_utf8_normalize(utf8, -1, G_NORMALIZE_NFD);
	if (!nfd)
		return g_unichar_tolower(c);

	for (p = nfd; *p; p = g_utf8_next_char(p)) {
		gunichar u = g_utf8_get_char(p);
		if (g_unichar_type(u) == G_UNICODE_NON_SPACING_MARK)
			continue;
		base = g_unichar_tolower(u);
		break;
	}
	g_free(nfd);
	return base;
}

static const gchar *
formas_de(gunichar base)
{
	gsize i;

	for (i = 0; i < G_N_ELEMENTS(clases); i++)
		if (clases[i].base == base)
			return clases[i].formas;
	return NULL;
}

gchar *
elim_tildes_regex_palabra(const gchar *palabra)
{
	GString *rx;
	const gchar *p;
	gboolean abierta = FALSE;

	if (!palabra || !*palabra)
		return NULL;

	rx = g_string_new(NULL);
	for (p = palabra; *p; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		const gchar *formas = formas_de(letra_desnuda(c));

		if (formas) {
			g_string_append_c(rx, '(');
			g_string_append(rx, formas);
			g_string_append_c(rx, ')');
			abierta = TRUE;
		} else {
			g_string_append_unichar(rx, c);
		}
	}

	if (!abierta) {
		g_string_free(rx, TRUE);
		return NULL;
	}
	return g_string_free(rx, FALSE);
}

gchar **
elim_tildes_regex_consulta(const gchar *consulta)
{
	gchar **palabras;
	GPtrArray *salida;
	gboolean alguna = FALSE;
	gint i;

	if (!consulta || !*consulta)
		return NULL;
	if (strpbrk(consulta, METACARACTERES))
		return NULL;

	palabras = g_strsplit_set(consulta, " \t\n", -1);
	salida = g_ptr_array_new();

	for (i = 0; palabras[i]; i++) {
		gchar *rx;

		if (!*palabras[i])
			continue;
		rx = elim_tildes_regex_palabra(palabras[i]);
		if (rx)
			alguna = TRUE;
		else
			rx = g_strdup(palabras[i]);
		g_ptr_array_add(salida, rx);
	}
	g_strfreev(palabras);

	if (!alguna || salida->len == 0) {
		g_ptr_array_foreach(salida, (GFunc)g_free, NULL);
		g_ptr_array_free(salida, TRUE);
		return NULL;
	}

	g_ptr_array_add(salida, NULL);
	return (gchar **)g_ptr_array_free(salida, FALSE);
}

gchar *
elim_tildes_neutro(const gchar *texto, GArray *mapa)
{
	GHashTable *cache;
	GString *salida;
	const gchar *p;
	gint origen = 0;

	if (!texto)
		texto = "";

	/* La Biblia entera usa apenas un par de centenares de caracteres
	 * distintos, así que cada uno se reduce una sola vez y el resto
	 * del recorrido es mirar la tabla. */
	cache = g_hash_table_new_full(NULL, NULL, NULL, g_free);
	salida = g_string_new(NULL);

	for (p = texto; *p; p = g_utf8_next_char(p), origen++) {
		gunichar c = g_utf8_get_char(p);
		gchar *neutro = g_hash_table_lookup(cache, GUINT_TO_POINTER(c));
		const gchar *q;

		if (!neutro) {
			gunichar base = letra_desnuda(c);
			gchar utf8[8];
			gint len;

			if (base == 0) {
				neutro = g_strdup("");
			} else {
				len = g_unichar_to_utf8(base, utf8);
				utf8[len] = '\0';
				neutro = g_strdup(utf8);
			}
			g_hash_table_insert(cache, GUINT_TO_POINTER(c), neutro);
		}

		for (q = neutro; *q; q = g_utf8_next_char(q)) {
			g_string_append_unichar(salida, g_utf8_get_char(q));
			if (mapa)
				g_array_append_val(mapa, origen);
		}
	}

	/* Un apunte final con el total, para que quien mire el mapa pueda
	 * preguntar por el carácter siguiente al último sin salirse. */
	if (mapa)
		g_array_append_val(mapa, origen);

	g_hash_table_destroy(cache);
	return g_string_free(salida, FALSE);
}
