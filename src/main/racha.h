/*
 * Biblia Elim
 * racha.h - los días que el lector de verdad leyó
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __RACHA_H__
#define __RACHA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* El progreso de un plan dice qué días del plan están hechos, pero no en
 * qué fecha se marcaron: con eso no se puede saber si el lector leyó
 * cinco días seguidos o los cinco de una sentada un sábado. Así que las
 * fechas se apuntan aparte, según pasan, en settings.xml:
 *
 *   <racha><dia label="2026-09-03" list="1"/></racha>
 *
 * Solo cuenta marcar una lectura de verdad. Ponerse al día dando por
 * leído lo atrasado no apunta nada: es justo la declaración de que esos
 * días no se leyeron, y una racha que se puede rellenar hacia atrás no
 * mide nada. */

void main_racha_apuntar(const char *fecha);
void main_racha_apuntar_hoy(void);

/* Si ese día ("AAAA-MM-DD") hubo lectura. */
gboolean main_racha_dia(const char *fecha);

/* Días seguidos hasta hoy. Si hoy todavía no se ha leído pero ayer sí,
 * la racha sigue viva y se cuenta desde ayer: el día no ha terminado y
 * darla por rota a las nueve de la mañana sería mentir. */
int main_racha_actual(void);
/* Si la racha actual se apoya en ayer y no en hoy: hoy está pendiente. */
gboolean main_racha_hoy_pendiente(void);

/* La racha más larga que haya habido nunca, y el total de días. */
int main_racha_mejor(void);
int main_racha_total(void);

/* La fecha más antigua apuntada, o NULL si no hay ninguna. No liberar. */
const char *main_racha_desde(void);

#ifdef __cplusplus
}
#endif
#endif
