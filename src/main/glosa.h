/*
 * Biblia Elim
 * glosa.h - la ficha del término, sin repetirse
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __GLOSA_H__
#define __GLOSA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* El léxico guarda cada entrada en una sola cadena que trae dos cosas
 * pegadas y una repetida:
 *
 *   glosa:      "vencido"
 *   definición: "Vencido. Hacer peor... En Reina-Valera 1909: vencido; son vencidos."
 *
 * La ficha ya enseña la glosa arriba, en grande, así que repetirla al
 * empezar la definición sobra (pasa en el 86 % de las entradas), y cómo
 * lo tradujo la Reina-Valera es otra cosa distinta de lo que la palabra
 * significa: va en su propio renglón (94 % de las entradas).
 *
 * Las dos devuelven memoria de g_malloc; la segunda, NULL si no hay.
 */

gchar *main_glosa_definicion(const char *definicion, const char *glosa);
gchar *main_glosa_rv1909(const char *definicion);

#ifdef __cplusplus
}
#endif
#endif
