/*
 * Biblia Elim
 * planes_lectura.h - diálogo Planes de lectura
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___PLANES_LECTURA_H_
#define ___PLANES_LECTURA_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_planes_lectura_dialog(void);

/* La lectura de hoy del plan en curso, abierta en la ventana principal
 * de un tirón. Sin plan empezado no hay lectura de hoy, así que abre el
 * diálogo para elegir uno. */
void gui_planes_lectura_hoy(void);

/* Qué toca hoy, en dos renglones, para el botón de la ventana
 * principal: "La Biblia en un año · día 12 de 365\nGénesis 30-31 ·
 * Mateo 9". NULL si no hay plan en curso. Hay que liberarlo. */
gchar *gui_planes_lectura_resumen_hoy(void);

/* Cómo está la lectura de hoy, para el botón de marcar de la ventana
 * principal. `detalle`, si no es NULL, recibe una línea que dice qué se
 * marcaría o por qué no hay nada que marcar (hay que liberarla). */
typedef enum {
	PL_HOY_SIN_PLAN = 0,	/* no hay plan en curso */
	PL_HOY_PENDIENTE,	/* queda lectura por marcar */
	PL_HOY_TERMINADO	/* el plan está entero */
} PL_HOY;

PL_HOY gui_planes_lectura_estado_hoy(gchar **detalle);

/* Marca como leído el día que toca del plan en curso. */
void gui_planes_lectura_marcar_hoy(void);

#ifdef __cplusplus
}
#endif
#endif
