/*
 * Biblia Elim
 * buscar_notas.c - buscar dentro de lo que uno mismo ha escrito
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Dos modos, y los dos hacen falta.
 *
 * El de texto es el que va a usar todo el mundo: escribe «espiritu» y
 * encuentra «Espíritu», porque por dentro reduce las dos cosas a su
 * forma neutra con elim_tildes_neutro(), igual que la búsqueda del
 * lector. El mapa que devuelve esa función es lo que permite subrayar
 * después el trozo exacto del texto original.
 *
 * El de expresión regular es para quien lo pide: buscar «^Apóstol» solo
 * al principio de la nota, o «gracia|misericordia» de una vez, o
 * «\bfe\b» sin que salgan «fe» dentro de «fecha». Ahí no se tocan las
 * tildes -- quien escribe una expresión regular decide él lo que quiere
 * -- y una expresión mal escrita se devuelve como error para enseñarlo
 * debajo del cuadro, no como un cuadro de diálogo de error.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "main/buscar_notas.h"
#include "main/busqueda_tildes.h"

/* Cuánto texto se enseña de una nota larga, en caracteres, y cuánto se
 * deja delante de lo encontrado para que se lea en contexto. */
#define BN_EXTRACTO 150
#define BN_ANTES 45

/* --------------------------------------------------------------------
 * El trozo que se enseña
 * ------------------------------------------------------------------ */

/* Los saltos de línea se cambian por espacios y no se quitan: así el
 * extracto cabe en un renglón y, como un salto ocupa lo mismo que un
 * espacio, las posiciones de lo encontrado siguen valiendo. */
static gchar *
en_un_renglon(const gchar *texto)
{
	gchar *copia = g_strdup(texto ? texto : "");

	return g_strdelimit(copia, "\n\r\t", ' ');
}

/*
 * Recorta el texto alrededor de lo hallado. Entra y sale todo en
 * caracteres menos `ini_b` y `fin_b`, que salen en bytes del extracto,
 * que es como los quiere quien tiene que pintarlo.
 */
static gchar *
extracto_de(const gchar *texto, gint ini_c, gint fin_c, gint *ini_b, gint *fin_b)
{
	gchar *renglon = en_un_renglon(texto);
	glong largo = g_utf8_strlen(renglon, -1);
	gint desde, hasta;
	gchar *trozo, *salida;
	const gchar *p_desde, *p_hasta;
	gboolean corta_izq, corta_der;

	desde = MAX(0, ini_c - BN_ANTES);
	hasta = MIN((gint)largo, desde + BN_EXTRACTO);
	/* Si lo hallado se sale por la derecha, se corre la ventana: más
	 * vale perder contexto delante que no enseñar lo que se buscaba. */
	if (fin_c > hasta) {
		hasta = MIN((gint)largo, fin_c);
		desde = MAX(0, hasta - BN_EXTRACTO);
	}

	corta_izq = (desde > 0);
	corta_der = (hasta < (gint)largo);

	p_desde = g_utf8_offset_to_pointer(renglon, desde);
	p_hasta = g_utf8_offset_to_pointer(renglon, hasta);
	trozo = g_strndup(p_desde, p_hasta - p_desde);

	salida = g_strconcat(corta_izq ? "…" : "", trozo,
			     corta_der ? "…" : "", NULL);
	g_free(trozo);

	/* Y dónde queda lo hallado dentro de lo que se enseña. */
	*ini_b = (gint)(g_utf8_offset_to_pointer(salida,
						 (corta_izq ? 1 : 0) +
						     (ini_c - desde)) -
			salida);
	*fin_b = (gint)(g_utf8_offset_to_pointer(salida,
						 (corta_izq ? 1 : 0) +
						     (MIN(fin_c, hasta) - desde)) -
			salida);

	g_free(renglon);
	return salida;
}

/* --------------------------------------------------------------------
 * Buscar en un texto
 * ------------------------------------------------------------------ */

/* Devuelve el número de veces que aparece y, en `ini_c`/`fin_c`, dónde
 * cae la primera, en caracteres. Cero si no aparece. */
static gint
buscar_texto(const gchar *texto, const gchar *aguja, gboolean distinguir,
	     gint *ini_c, gint *fin_c)
{
	gint cuantas = 0;

	if (!texto || !*texto || !aguja || !*aguja)
		return 0;

	if (distinguir) {
		const gchar *p = texto;
		gsize n = strlen(aguja);
		const gchar *golpe;

		while ((golpe = strstr(p, aguja)) != NULL) {
			if (cuantas == 0) {
				*ini_c = (gint)g_utf8_pointer_to_offset(texto,
									golpe);
				*fin_c = *ini_c +
					 (gint)g_utf8_strlen(aguja, -1);
			}
			cuantas++;
			p = golpe + n;
		}
		return cuantas;
	}

	/* Sin distinguir: las dos cosas se reducen a su forma neutra, y el
	 * mapa dice a qué carácter del original corresponde cada uno. */
	{
		GArray *mapa = g_array_new(FALSE, FALSE, sizeof(gint));
		gchar *neutro = elim_tildes_neutro(texto, mapa);
		gchar *aguja_n = elim_tildes_neutro(aguja, NULL);
		glong largo_n = g_utf8_strlen(aguja_n, -1);
		const gchar *p = neutro;
		const gchar *golpe;
		gint desplazado = 0;

		while (largo_n > 0 && (golpe = strstr(p, aguja_n)) != NULL) {
			gint off = desplazado +
				   (gint)g_utf8_pointer_to_offset(p, golpe);

			if (off + (gint)largo_n > (gint)mapa->len - 1)
				break;
			if (cuantas == 0) {
				*ini_c = g_array_index(mapa, gint, off);
				*fin_c = g_array_index(mapa, gint,
						       off + (gint)largo_n - 1) +
					 1;
			}
			cuantas++;
			p = golpe + strlen(aguja_n);
			desplazado = off + (gint)largo_n;
		}

		g_free(neutro);
		g_free(aguja_n);
		g_array_free(mapa, TRUE);
	}
	return cuantas;
}

static gint
buscar_regex(const gchar *texto, GRegex *re, gint *ini_c, gint *fin_c)
{
	GMatchInfo *info = NULL;
	gint cuantas = 0;

	if (!texto || !*texto || !re)
		return 0;

	if (!g_regex_match(re, texto, (GRegexMatchFlags)0, &info)) {
		if (info)
			g_match_info_free(info);
		return 0;
	}

	while (g_match_info_matches(info)) {
		gint ini_b = 0, fin_b = 0;

		if (cuantas == 0 &&
		    g_match_info_fetch_pos(info, 0, &ini_b, &fin_b)) {
			*ini_c = (gint)g_utf8_pointer_to_offset(
			    texto, texto + ini_b);
			*fin_c = (gint)g_utf8_pointer_to_offset(
			    texto, texto + fin_b);
		}
		cuantas++;
		/* Una expresión que casa con la cadena vacía haría esto
		 * eterno; g_match_info_next() lo avisa por su cuenta. */
		if (!g_match_info_next(info, NULL))
			break;
	}
	g_match_info_free(info);
	return cuantas;
}

/* --------------------------------------------------------------------
 * La búsqueda
 * ------------------------------------------------------------------ */

static void
resultado_libre(BN_RESULTADO *r)
{
	if (!r)
		return;
	g_free(r->modulo);
	g_free(r->osisref);
	g_free(r->note_key);
	g_free(r->frase);
	g_free(r->nota);
	g_free(r->extracto);
	g_free(r);
}

void
main_buscar_notas_libre(GList *resultados)
{
	g_list_free_full(resultados, (GDestroyNotify)resultado_libre);
}

GList *
main_buscar_notas(GList *notas, const gchar *consulta, BN_MODO modo,
		  gboolean distinguir_mayusculas, GError **error)
{
	GList *out = NULL;
	GList *l;
	GRegex *re = NULL;

	if (!consulta || !*consulta)
		return NULL;

	if (modo == BN_REGEX) {
		GRegexCompileFlags banderas = G_REGEX_OPTIMIZE;

		if (!distinguir_mayusculas)
			banderas = (GRegexCompileFlags)(banderas |
							G_REGEX_CASELESS);
		re = g_regex_new(consulta, banderas, (GRegexMatchFlags)0,
				 error);
		if (!re)
			return NULL;
	}

	for (l = notas; l; l = l->next) {
		BN_NOTA *n = (BN_NOTA *)l->data;
		gint ini_c = 0, fin_c = 0, cuantas;
		gboolean en_frase = FALSE;
		const gchar *donde;
		BN_RESULTADO *r;

		if (!n)
			continue;

		donde = n->nota;
		cuantas = (modo == BN_REGEX)
			      ? buscar_regex(n->nota, re, &ini_c, &fin_c)
			      : buscar_texto(n->nota, consulta,
					     distinguir_mayusculas, &ini_c,
					     &fin_c);

		/* Si no está en la nota, se mira la frase subrayada: es
		 * texto del lector igual que la nota, y quien busca «no
		 * temas» espera encontrar también lo que subrayó. */
		if (cuantas == 0) {
			cuantas = (modo == BN_REGEX)
				      ? buscar_regex(n->frase, re, &ini_c,
						     &fin_c)
				      : buscar_texto(n->frase, consulta,
						     distinguir_mayusculas,
						     &ini_c, &fin_c);
			if (cuantas == 0)
				continue;
			en_frase = TRUE;
			donde = n->frase;
		}

		r = g_new0(BN_RESULTADO, 1);
		r->modulo = g_strdup(n->modulo ? n->modulo : "");
		r->osisref = g_strdup(n->osisref ? n->osisref : "");
		r->note_key = g_strdup(n->note_key ? n->note_key : "");
		r->frase = n->frase ? g_strdup(n->frase) : NULL;
		r->nota = g_strdup(n->nota ? n->nota : "");
		r->en_frase = en_frase;
		r->cuantas = cuantas;
		r->extracto = extracto_de(donde, ini_c, fin_c, &r->ini,
					  &r->fin);
		out = g_list_prepend(out, r);
	}

	if (re)
		g_regex_unref(re);
	return g_list_reverse(out);
}
