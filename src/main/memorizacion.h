/*
 * Biblia Elim
 * memorizacion.h - un versículo por semana, con repaso espaciado
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MEMORIZACION_H__
#define __MEMORIZACION_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Repaso espaciado por cajas (Leitner), que es lo que se puede explicar
 * en una frase y no falla: un versículo acertado sube de caja y tarda
 * más en volver; uno fallado baja a la primera y vuelve mañana.
 *
 * Cada caja tiene su espera: 1, 2, 4, 8, 15, 30 y 90 días. Como acertar
 * sube de caja antes de esperar, un versículo que va bien desde el
 * principio se ve a los 2 días, luego a los 4, 8, 15, 30 y 90; el día
 * de espera de la caja 1 es el que se aplica tras fallar, que es cuando
 * hace falta volver a verlo pronto.
 *
 * La última caja no es "aprendido y fuera": un versículo que no se
 * repasa nunca más se olvida, así que sigue volviendo cada tres meses.
 *
 * En settings.xml:
 *   <memoria><verso label="John 3:16"
 *                   list="caja|proximo|alta|aciertos|fallos"/></memoria>
 */

#define MEM_CAJAS 7

typedef struct {
	gchar *clave;		/* "John 3:16", como la entiende el motor */
	int caja;		/* 1..MEM_CAJAS */
	gchar *proximo;		/* "AAAA-MM-DD" en que toca repasarlo */
	gchar *alta;		/* "AAAA-MM-DD" en que se empezó */
	int aciertos;
	int fallos;
} MEM_VERSO;

/* Todos, del más urgente al menos. Liberar con main_memoria_libre(). */
GList *main_memoria_todos(void);
/* Solo los que tocan hoy o se quedaron atrás. */
GList *main_memoria_de_hoy(void);
void main_memoria_libre(GList *lista);

int main_memoria_cuantos(void);
int main_memoria_pendientes(void);
/* Cuántos hay ya en la última caja: los que se saben. */
int main_memoria_asentados(void);

gboolean main_memoria_tiene(const char *clave);
/* Lo da de alta en la caja 1, para repasarlo hoy mismo. FALSE si ya
 * estaba o si la clave no vale. */
gboolean main_memoria_anadir(const char *clave);
void main_memoria_quitar(const char *clave);

/* El repaso: acertado sube de caja, fallado vuelve a la primera. */
void main_memoria_repasar(const char *clave, gboolean acertado);

/* --- el ritmo de uno por semana ---
 *
 * La aplicación no impide dar de alta dos en la misma semana: es una
 * sugerencia, no una reja. Pero sí sabe decir si esta semana ya hay uno,
 * que es lo que hace falta para proponerlo sin dar la lata. */
int main_memoria_altas_de_esta_semana(void);

#ifdef __cplusplus
}
#endif
#endif
