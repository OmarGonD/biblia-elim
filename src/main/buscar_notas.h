/*
 * Biblia Elim
 * buscar_notas.h - buscar dentro de lo que uno mismo ha escrito
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __BUSCAR_NOTAS_H__
#define __BUSCAR_NOTAS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Esta parte no sabe de Sword ni de GTK: recibe las notas ya leídas y
 * devuelve las que coinciden, con el trozo que hay que enseñar. Por eso
 * se puede probar sola. */

typedef enum {
	/* Como busca el resto de la aplicación: sin pelearse con las
	 * tildes ni con las mayúsculas, que es lo que quiere quien
	 * escribió «espíritu» y busca «espiritu». */
	BN_TEXTO = 0,
	/* Expresión regular de Perl, la de GLib. Para quien sabe lo que
	 * escribe: «^Ap[oó]stol», «gracia|misericordia», «\\bfe\\b». */
	BN_REGEX
} BN_MODO;

/* Una nota, tal como se le entrega al buscador. */
typedef struct {
	const gchar *modulo;
	const gchar *osisref;	/* "Eph.2.1" */
	const gchar *note_key;
	const gchar *frase;	/* la frase subrayada, o NULL */
	const gchar *nota;
} BN_NOTA;

typedef struct {
	gchar *modulo;
	gchar *osisref;
	gchar *note_key;
	gchar *frase;
	gchar *nota;
	/* Lo encontrado estaba en la frase subrayada y no en la nota. Se
	 * dice en el resultado porque si no, quien busca no entiende por
	 * qué ha salido esa fila. */
	gboolean en_frase;
	gchar *extracto;	/* el trozo de texto que se enseña */
	gint ini, fin;		/* dónde cae lo hallado, en bytes del extracto */
	gint cuantas;		/* veces que aparece en esa nota */
} BN_RESULTADO;

/* `notas` es una GList de BN_NOTA*. Devuelve una GList de BN_RESULTADO*
 * en el mismo orden en que llegaron las notas.
 *
 * Con BN_REGEX, una expresión mal escrita no es un fallo del programa
 * sino una errata de quien la escribe: se devuelve NULL y el motivo en
 * `error`, para poder enseñarlo debajo del cuadro de búsqueda. */
GList *main_buscar_notas(GList *notas, const gchar *consulta, BN_MODO modo,
			 gboolean distinguir_mayusculas, GError **error);

void main_buscar_notas_libre(GList *resultados);

#ifdef __cplusplus
}
#endif
#endif
