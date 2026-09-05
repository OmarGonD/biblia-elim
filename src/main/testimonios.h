/*
 * Biblia Elim
 * testimonios.h - Jesús en la historia: las fuentes de fuera de la Biblia
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __TESTIMONIOS_H__
#define __TESTIMONIOS_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _testimonio {
	gchar *id;
	gchar *titulo;
	gchar *obra;	     /* la obra y el pasaje: "Anales XV, 44" */
	gchar *autor;
	gchar *fecha;
	gchar *postura;	     /* "Testigo hostil", "Prueba material"... */
	gchar *cita;	     /* el texto de la fuente, traducido */
	gchar *muestra;	     /* qué se puede sostener con él */
	gchar *cautela;	     /* y qué no */
	gchar *referencias;  /* pasajes bíblicos, separados por ';' */
} Testimonio;

typedef struct _testimonio_grupo {
	gchar *id;
	gchar *titulo;
	GList *testimonios; /* Testimonio* */
} TestimonioGrupo;

void main_testimonios_init(void);
void main_testimonios_shutdown(void);

/* Lee el archivo suelto en vez del recurso incrustado. Es lo que usa la
 * prueba de tests/, que no tiene el binario con los recursos dentro. */
gboolean main_testimonios_cargar_archivo(const char *ruta);

/* Los grupos, en el orden del archivo. Quien llama no libera nada. */
const GList *main_testimonios_grupos(void);

/* Cuántos testimonios hay en total. */
guint main_testimonios_cuantos(void);

/* Uno por su id ("tacito"), o NULL. */
const Testimonio *main_testimonios_por_id(const char *id);

/* Trocea el campo referencias en citas sueltas ("Mateo 27:1-2"), tal como
 * están escritas. Se libera con g_strfreev(). */
gchar **main_testimonios_referencias(const Testimonio *t);

#ifdef __cplusplus
}
#endif
#endif
