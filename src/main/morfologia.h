/*
 * Biblia Elim
 * morfologia.h - los códigos de morfología, dichos en español
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MORFOLOGIA_H__
#define __MORFOLOGIA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Lo que viene en el atributo morph= de los módulos etiquetados:
 *
 *   "robinson:V-PAI-3S"                 un verbo griego
 *   "robinson:T-NSM robinson:N-NSM"     dos palabras fundidas en una
 *   "strongMorph:TH8804"                un verbo hebreo, del KJV
 *
 * Aquí se traducen a algo que un predicador pueda leer sin haber
 * estudiado griego. Nada de esto toca Sword ni GTK: por eso se puede
 * probar solo, que es lo que hace tests/morfologia_test.c.
 *
 * Las tres devuelven memoria de g_malloc, nunca NULL, y "" si no hay
 * nada que decir.
 */

/* "Verbo · presente activo indicativo · 3ª persona singular" */
gchar *main_morf_es(const char *attr);

/* Lo mismo, apretado para que quepa en la etiqueta de una fila:
 * "v. pres.act.ind. 3sg" */
gchar *main_morf_corto(const char *attr);

/* El código sin el nombre del esquema: "V-PAI-3S". */
gchar *main_morf_codigo(const char *attr);

#ifdef __cplusplus
}
#endif
#endif
