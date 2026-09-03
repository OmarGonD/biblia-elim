/*
 * Biblia Elim
 * tiempo_lectura.h - cuánto se tarda en leer lo de hoy
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __TIEMPO_LECTURA_H__
#define __TIEMPO_LECTURA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Las palabras se cuentan del módulo que el lector tenga abierto, no de
 * una tabla: cada versión tiene su extensión, y la Platense no se lee en
 * el mismo rato que la Reina-Valera. Eso obliga a hablar con el motor,
 * así que esto vive aparte de main/planes_lectura.c, que es de glib y
 * XML y nada más -- es lo que le permite al aviso de systemd calcular la
 * lectura del día sin arrancar Sword. */

/* Palabras que suman esos capítulos ("John 3", "Matt 1"...), o 0 si no
 * se pudo leer ninguno. La lista es la de main_planes_referencias(). */
int main_tiempo_palabras(GList *referencias);

/* Lo mismo en palabras del idioma: "8–12 min", o "unos 5 min" cuando la
 * franja se queda en un solo número. NULL si no hay nada que medir. Hay
 * que liberarlo con g_free(). */
gchar *main_tiempo_texto(GList *referencias);

#ifdef __cplusplus
}
#endif
#endif
