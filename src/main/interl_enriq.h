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

/* Las fichas viven en una base SQLite de solo lectura (fichas.sqlite), generada en el build por
 * tools/construir_fichas_sqlite.py desde los JSON versionados. La app NO carga todo al arrancar: la base se
 * abre en la primera consulta y se lee un versículo a la vez (índice por ref_tisch).
 *
 * Clave de una ficha: ref_tisch (OSIS en la numeración del módulo Tisch, "John.1.39") + posición de la palabra
 * en ese versículo (1..n) + Strong de Tisch. */

/* Abre `ruta` (solo lectura) y reemplaza la base anterior. Devuelve el número de fichas, o -1 si el archivo no
 * existe o no es una base de fichas de este formato (en ese caso la base anterior sigue abierta). */
gint main_interl_enriq_cargar(const char *ruta);
void main_interl_enriq_liberar(void);

/* Busca la base instalada: $BIBLIA_ELIM_FICHAS (exclusiva), la del árbol de build y SHARE_DIR/fichas.sqlite.
 * Devuelve igual que main_interl_enriq_cargar, o 0 si no hay base. Se invoca sola, una vez, en la primera
 * consulta. */
gint main_interl_enriq_cargar_predeterminada(void);

/* Bloque HTML (sin <html>) con la ficha de esa palabra, o NULL si no hay (o no se puede identificar).
 * posicion > 0: se busca esa posición y el Strong debe coincidir con el de la ficha.
 * posicion == 0 (se ignora la posición): se busca por Strong solo si es ÚNICO en el versículo; con varias
 * apariciones no se adivina y devuelve NULL (el llamador muestra la ficha básica). Todo texto va escapado.
 * Liberar con g_free. */
gchar *main_interl_enriq_html(const char *ref_tisch, gint posicion, const char *strong);

#ifdef __cplusplus
}
#endif
#endif
