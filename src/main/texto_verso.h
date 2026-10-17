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

/* Lee el cuerpo del versículo mediante BibleBackend (SQLite o SWORD),
 * sin incluir notas/comentarios ni cambiar la referencia de la lectura.
 *
 * Devuelve el texto sin marcas y sin espacios sobrantes, o NULL si no
 * hay Biblia abierta o el módulo no da ese pasaje. g_free(). */
gchar *main_texto_de(const char *clave);

#ifdef __cplusplus
}
#endif
#endif
