/*
 * Biblia Elim
 * interl_enriq.h - ficha de estudio enriquecida de una palabra del interlineal
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __INTERL_ENRIQ_H__
#define __INTERL_ENRIQ_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Las fichas se generan fuera de la app (una por palabra y pasaje) y se
 * guardan en un JSON: un arreglo de objetos con "ref" (OSIS, "John.1.2"),
 * "strong" ("G4314") y los campos de la ficha: glosa_interlineal,
 * rango_semantico, construccion, sentido_en_contexto, matiz,
 * traducciones_comparadas, notas_traduccion, otros_usos, nivel_certeza.
 * Todo campo salvo ref y strong es opcional; null o vacío se omite.
 *
 * Devuelve el número de fichas válidas cargadas, o -1 si el archivo no se
 * pudo leer. Una carga nueva reemplaza a la anterior. */
gint main_interl_enriq_cargar(const char *ruta);
void main_interl_enriq_liberar(void);

/* Carga <dir>/interlineal_enriquecido.json si existe. Devuelve igual que
 * main_interl_enriq_cargar, o 0 si el archivo no existe. */
gint main_interl_enriq_cargar_de(const char *dir);

/* Bloque HTML (sin <html>) con la ficha de esa palabra en ese pasaje, o
 * NULL si no hay. Todo el texto va escapado. Liberar con g_free. */
gchar *main_interl_enriq_html(const char *ref, const char *strong);

#ifdef __cplusplus
}
#endif
#endif
