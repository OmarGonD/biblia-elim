/*
 * Biblia Elim
 * pulpito_pasos.c - el bosquejo convertido en pasos de entrega
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

#include "main/pulpito_pasos.h"

/* --------------------------------------------------------------------
 * Los pasos
 * ------------------------------------------------------------------ */

static void
paso_libre(gpointer p)
{
	PU_PASO *s = (PU_PASO *)p;

	if (!s)
		return;
	g_free(s->titulo);
	g_free(s->ref);
	g_free(s->texto);
	g_free(s->ilustracion);
	g_free(s->nota);
	if (s->vinetas)
		g_ptr_array_free(s->vinetas, TRUE);
	g_free(s);
}

static PU_PASO *
paso_nuevo(PU_TIPO tipo, int nivel, int titulo_de)
{
	PU_PASO *s = g_new0(PU_PASO, 1);

	s->tipo = tipo;
	s->nivel = nivel;
	s->titulo_de = titulo_de;
	return s;
}

PU_SERMON *
pu_sermon_nuevo(const char *modulo, const char *titulo, const char *version)
{
	PU_SERMON *sermon = g_new0(PU_SERMON, 1);

	sermon->modulo = g_strdup(modulo ? modulo : "");
	sermon->titulo = g_strdup(titulo ? titulo : "");
	sermon->version = version ? g_strdup(version) : NULL;
	sermon->pasos = g_ptr_array_new_with_free_func(paso_libre);
	return sermon;
}

void
pu_sermon_libre(PU_SERMON *sermon)
{
	if (!sermon)
		return;
	g_free(sermon->modulo);
	g_free(sermon->titulo);
	g_free(sermon->ref_base);
	g_free(sermon->version);
	if (sermon->pasos)
		g_ptr_array_free(sermon->pasos, TRUE);
	g_free(sermon);
}

int
pu_sermon_titulo(PU_SERMON *sermon, const char *enunciado, int nivel)
{
	int i = (int)sermon->pasos->len;
	PU_PASO *s = paso_nuevo(PU_TITULO, nivel, i);

	s->titulo = g_strdup(enunciado ? enunciado : "");
	g_ptr_array_add(sermon->pasos, s);
	return i;
}

/* --------------------------------------------------------------------
 * Del bosquejo a texto plano
 *
 * El contenido de cada punto es HTML del editor. Aquí no se interpreta:
 * se quitan las etiquetas y se parte en líneas, que es lo que hace falta
 * para repartirlas en pasos.
 * ------------------------------------------------------------------ */

/* Las etiquetas que separan bloques dejan un salto de línea; el resto
 * desaparecen sin más. */
static gboolean
etiqueta_de_bloque(const char *nombre)
{
	static const char *bloques[] = {
	    "br", "p", "/p", "li", "/li", "div", "/div", "tr", "/tr",
	    "ul", "/ul", "ol", "/ol", "h1", "/h1", "h2", "/h2", "h3",
	    "/h3", "blockquote", "/blockquote", NULL
	};
	int i;

	for (i = 0; bloques[i]; ++i)
		if (!g_strcmp0(nombre, bloques[i]))
			return TRUE;
	return FALSE;
}

gchar *
pu_sin_etiquetas(const char *html)
{
	GString *out;
	const char *p = html;

	if (!html)
		return g_strdup("");

	out = g_string_new(NULL);
	while (*p) {
		if (*p == '<') {
			const char *fin = strchr(p, '>');
			gchar *etiqueta;
			gchar *nombre;
			char *espacio;

			if (!fin)
				break;
			etiqueta = g_ascii_strdown(p + 1, fin - p - 1);
			/* "br /" y "p class=x" son la misma etiqueta que
			 * "br" y "p": el nombre es lo que va delante del
			 * primer espacio o barra final. */
			nombre = g_strdup(etiqueta);
			espacio = strpbrk(nombre, " \t\n");
			if (espacio)
				*espacio = '\0';
			if (*nombre && nombre[strlen(nombre) - 1] == '/')
				nombre[strlen(nombre) - 1] = '\0';
			if (etiqueta_de_bloque(nombre))
				g_string_append_c(out, '\n');
			g_free(nombre);
			g_free(etiqueta);
			p = fin + 1;
			continue;
		}
		if (*p == '&') {
			/* Las que de verdad salen del editor; cualquier
			 * otra se deja como está. */
			if (!g_ascii_strncasecmp(p, "&amp;", 5)) {
				g_string_append_c(out, '&');
				p += 5;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&lt;", 4)) {
				g_string_append_c(out, '<');
				p += 4;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&gt;", 4)) {
				g_string_append_c(out, '>');
				p += 4;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&nbsp;", 6)) {
				g_string_append_c(out, ' ');
				p += 6;
				continue;
			}
			if (!g_ascii_strncasecmp(p, "&quot;", 6)) {
				g_string_append_c(out, '"');
				p += 6;
				continue;
			}
		}
		g_string_append_c(out, *p);
		++p;
	}
	return g_string_free(out, FALSE);
}

/* --------------------------------------------------------------------
 * Repartir el contenido de un punto
 * ------------------------------------------------------------------ */

/* "Apoyo:", "Ilustración:", "Nota:". Devuelve lo que va detrás de los
 * dos puntos, o NULL. La comparación es byte a byte y sin distinguir
 * mayúsculas en lo que es ASCII, que es cuanto hace falta. */
static const char *
tras_marca(const char *linea, const char *marca)
{
	size_t n = strlen(marca);

	if (g_ascii_strncasecmp(linea, marca, n))
		return NULL;
	return linea + n;
}

static const char *
marca_ilustracion(const char *linea)
{
	const char *resto = tras_marca(linea, "ilustración:");

	if (!resto)
		resto = tras_marca(linea, "ilustracion:");
	return resto;
}

/* La nota que está esperando se pega al paso que acaba de nacer. */
static void
soltar_nota(GString **pendiente, PU_PASO *destino)
{
	if (!*pendiente)
		return;
	if (destino) {
		if (destino->nota) {
			gchar *junto = g_strconcat(destino->nota, "\n",
						   (*pendiente)->str, NULL);

			g_free(destino->nota);
			destino->nota = junto;
		} else
			destino->nota = g_strdup((*pendiente)->str);
	}
	g_string_free(*pendiente, TRUE);
	*pendiente = NULL;
}

void
pu_sermon_contenido(PU_SERMON *sermon, const char *html, int nivel,
		    int titulo_de, const PU_RESOLUTOR *res)
{
	gchar *plano = pu_sin_etiquetas(html);
	gchar **lineas = g_strsplit(plano, "\n", -1);
	PU_PASO *vinetas = NULL;
	/* Lo apuntado con "Nota:" mientras no haya un paso al que pegarlo. */
	GString *nota = NULL;
	PU_PASO *ultimo = NULL;
	int i;

	for (i = 0; lineas[i]; ++i) {
		gchar *linea = g_strstrip(lineas[i]);
		const char *resto;
		gchar *ref;

		if (!*linea)
			continue;

		/* Lo apuntado para uno mismo espera al paso siguiente. */
		resto = tras_marca(linea, "nota:");
		if (resto) {
			gchar *solo = g_strdup(resto);

			g_strstrip(solo);
			if (*solo) {
				if (!nota)
					nota = g_string_new(solo);
				else {
					g_string_append_c(nota, '\n');
					g_string_append(nota, solo);
				}
			}
			/* Corta la tanda de viñetas: lo que venga detrás de
			 * una nota empieza un paso nuevo, que es el que se
			 * la queda. */
			vinetas = NULL;
			g_free(solo);
			continue;
		}

		resto = tras_marca(linea, "apoyo:");
		if (resto) {
			gchar *solo = g_strdup(resto);

			g_strstrip(solo);
			ref = res->ref_valida ? res->ref_valida(solo, res->datos)
					      : NULL;
			if (ref) {
				PU_PASO *s = paso_nuevo(PU_APOYO, nivel,
							titulo_de);

				s->ref = ref;
				s->texto = res->texto_de
					       ? res->texto_de(ref, res->datos)
					       : NULL;
				g_ptr_array_add(sermon->pasos, s);
				soltar_nota(&nota, s);
				ultimo = s;
				vinetas = NULL;
				g_free(solo);
				continue;
			}
			/* Si no era una referencia, la línea sigue su
			 * camino y acaba de viñeta. */
			g_free(solo);
		}

		resto = marca_ilustracion(linea);
		if (resto) {
			PU_PASO *s = paso_nuevo(PU_ILUSTRACION, nivel,
						titulo_de);

			s->ilustracion = g_strdup(resto);
			g_strstrip(s->ilustracion);
			g_ptr_array_add(sermon->pasos, s);
			soltar_nota(&nota, s);
			ultimo = s;
			vinetas = NULL;
			continue;
		}

		ref = res->ref_valida ? res->ref_valida(linea, res->datos)
				      : NULL;
		if (ref) {
			PU_PASO *s = paso_nuevo(PU_VERSO, nivel, titulo_de);

			s->ref = ref;
			s->texto = res->texto_de
				       ? res->texto_de(ref, res->datos)
				       : NULL;
			g_ptr_array_add(sermon->pasos, s);
			if (!sermon->ref_base)
				sermon->ref_base = g_strdup(ref);
			soltar_nota(&nota, s);
			ultimo = s;
			vinetas = NULL;
			continue;
		}

		/* Las viñetas seguidas se juntan en un paso: son para
		 * verlas de golpe, no de una en una. */
		if (!vinetas) {
			vinetas = paso_nuevo(PU_VINETAS, nivel, titulo_de);
			vinetas->vinetas =
			    g_ptr_array_new_with_free_func(g_free);
			g_ptr_array_add(sermon->pasos, vinetas);
			soltar_nota(&nota, vinetas);
			ultimo = vinetas;
		}
		g_ptr_array_add(vinetas->vinetas, g_strdup(linea));
	}

	/* La nota que quedó al final no se tira: es de lo último que se
	 * dijo en el punto, o del punto entero si no llegó a tener pasos
	 * ("1. El perdón" y debajo solo "Nota: contar lo de Ana"). */
	if (nota)
		soltar_nota(&nota, ultimo ? ultimo
					  : pu_paso(sermon, titulo_de));

	g_strfreev(lineas);
	g_free(plano);
}

/* --------------------------------------------------------------------
 * Recorrer
 * ------------------------------------------------------------------ */

int
pu_total(PU_SERMON *sermon)
{
	if (!sermon || !sermon->pasos)
		return 0;
	return (int)sermon->pasos->len;
}

PU_PASO *
pu_paso(PU_SERMON *sermon, int i)
{
	if (i < 0 || i >= pu_total(sermon))
		return NULL;
	return (PU_PASO *)g_ptr_array_index(sermon->pasos, i);
}

int
pu_primero(PU_SERMON *sermon)
{
	(void)sermon;
	return 0;
}

int
pu_ultimo(PU_SERMON *sermon)
{
	int n = pu_total(sermon);

	return (n > 0) ? n - 1 : 0;
}

int
pu_siguiente(PU_SERMON *sermon, int i)
{
	return (i + 1 <= pu_ultimo(sermon)) ? i + 1 : pu_ultimo(sermon);
}

int
pu_anterior(PU_SERMON *sermon, int i)
{
	(void)sermon;
	return (i > 0) ? i - 1 : 0;
}

int
pu_titulo_cercano(PU_SERMON *sermon, int i, int direccion)
{
	int j = i + direccion;

	while (j >= 0 && j < pu_total(sermon)) {
		PU_PASO *s = pu_paso(sermon, j);

		if (s && s->tipo == PU_TITULO)
			return j;
		j += direccion;
	}
	return i;	/* no hay otro punto: se queda donde está */
}

int
pu_verso_visible(PU_SERMON *sermon, int i)
{
	int j;

	for (j = MIN(i, pu_ultimo(sermon)); j >= 0; --j) {
		PU_PASO *s = pu_paso(sermon, j);

		if (s && (s->tipo == PU_VERSO || s->tipo == PU_APOYO))
			return j;
	}
	return -1;
}

GArray *
pu_apoyos_de(PU_SERMON *sermon, int i)
{
	GArray *apoyos = g_array_new(FALSE, FALSE, sizeof(int));
	PU_PASO *actual = pu_paso(sermon, i);
	int j;

	if (!actual)
		return apoyos;
	for (j = 0; j < pu_total(sermon); ++j) {
		PU_PASO *s = pu_paso(sermon, j);

		if (s->tipo == PU_APOYO && s->titulo_de == actual->titulo_de)
			g_array_append_val(apoyos, j);
	}
	return apoyos;
}

int
pu_ilustracion_de(PU_SERMON *sermon, int i)
{
	PU_PASO *actual = pu_paso(sermon, i);
	int j;

	if (!actual)
		return -1;
	for (j = 0; j < pu_total(sermon); ++j) {
		PU_PASO *s = pu_paso(sermon, j);

		if (s->tipo == PU_ILUSTRACION &&
		    s->titulo_de == actual->titulo_de)
			return j;
	}
	return -1;
}

gchar *
pu_resumen(PU_PASO *paso)
{
	if (!paso)
		return NULL;
	switch (paso->tipo) {
	case PU_TITULO:
		return g_strdup(paso->titulo ? paso->titulo : "");
	case PU_VERSO:
	case PU_APOYO:
		return g_strdup(paso->ref ? paso->ref : "");
	case PU_VINETAS:
		if (paso->vinetas && paso->vinetas->len)
			return g_strdup((const char *)
					    g_ptr_array_index(paso->vinetas, 0));
		return g_strdup(_("viñetas"));
	case PU_ILUSTRACION:
		return g_strdup(_("ilustración"));
	}
	return NULL;
}

/* --------------------------------------------------------------------
 * Por dónde se iba
 * ------------------------------------------------------------------ */

int
pu_paso_guardado(PU_SERMON *sermon, int guardado)
{
	if (guardado < 0)
		return 0;
	return MIN(guardado, pu_ultimo(sermon));
}

gboolean
pu_preguntar_continuar(PU_SERMON *sermon, int guardado)
{
	/* Ni al empezar ni al terminar hay nada que preguntar: en los dos
	 * casos lo que toca es el primer paso. */
	if (guardado <= 0)
		return FALSE;
	return guardado < pu_ultimo(sermon);
}
