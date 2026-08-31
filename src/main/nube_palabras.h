/*
 * Biblia Elim
 * nube_palabras.h - conteo de palabras por libro bíblico
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __NUBE_PALABRAS_H__
#define __NUBE_PALABRAS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _nube_libro NUBE_LIBRO;
struct _nube_libro {
	gchar *nombre;
	gchar *abrev;
	gchar *osis;
};

typedef struct _nube_palabra NUBE_PALABRA;
struct _nube_palabra {
	gchar *palabra;
	gint cuenta;
	gint cuenta_b;
	gint diferencia;
	gdouble pct;
	gdouble pct_b;
	gdouble dif_pct;
};

typedef struct _nube_conteo NUBE_CONTEO;
struct _nube_conteo {
	gchar *libro;
	gchar *libro_b;
	gint total;
	gint total_b;
	gint unicas;
	GPtrArray *palabras;
};

/* Lista de libros del módulo (cada elemento es NUBE_LIBRO *). */
GList *main_nube_lista_libros(const char *module);
void main_nube_libro_free(NUBE_LIBRO *libro);
void main_nube_lista_libros_free(GList *lista);

/* Nombre Sword del libro de la clave actual (p. ej. settings.currentverse). */
char *main_nube_libro_de_clave(const char *module, const char *key);

/* Resuelve texto tecleado (nombre, abreviatura o alias ES/EN) al nombre Sword. */
char *main_nube_resolver_libro(const char *module, const char *texto);

gchar *main_nube_normalizar(const gchar *s);
gboolean main_nube_texto_coincide(const gchar *haystack, const gchar *needle);

/*
 * Cuenta palabras de libro_a. Si libro_b no es NULL, fusiona las más
 * usadas de ambos. limite acota cuántas palabras se devuelven.
 */
NUBE_CONTEO *main_nube_contar(const char *module,
			      const char *libro_a,
			      const char *libro_b,
			      int limite);
void main_nube_conteo_free(NUBE_CONTEO *conteo);

#ifdef __cplusplus
}
#endif
#endif
