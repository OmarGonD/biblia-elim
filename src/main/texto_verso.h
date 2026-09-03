/*
 * Biblia Elim
 * texto_verso.h - leer un pasaje de paso, sin moverle la clave al módulo
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __TEXTO_VERSO_H__
#define __TEXTO_VERSO_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* main_get_striptext() le mueve la clave al módulo, y esa es la misma
 * que el lector tiene puesta en la ventana: pedirle un pasaje de paso
 * -- el versículo del día, el tiempo de la lectura, una tarjeta de
 * memorización -- le arrastraba la lectura hasta allí. Esto lo pide y
 * se la devuelve donde estaba.
 *
 * Devuelve el texto sin marcas y sin espacios sobrantes, o NULL si no
 * hay Biblia abierta o el módulo no da ese pasaje. g_free(). */
gchar *main_texto_de(const char *clave);

#ifdef __cplusplus
}
#endif
#endif
