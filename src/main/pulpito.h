/*
 * Biblia Elim
 * pulpito.h - la vista de púlpito: entregar el sermón, no estudiarlo
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PULPITO_H__
#define __PULPITO_H__

#include <glib.h>

#include "main/pulpito_pasos.h"

#ifdef __cplusplus
extern "C" {
#endif

/* El sermón no es una entidad nueva: es el bosquejo que la aplicación ya
 * sabe crear y editar (en la barra lateral, crear > Esquema), que por
 * dentro es un genbook de Sword con un árbol de puntos. Aquí se
 * lee ese árbol y se aplana en la lista de pasos que se recorren en el
 * púlpito; el reparto en pasos está en pulpito_pasos.c.
 *
 * Lo que se guarda aparte, en settings.xml, es solo el estado de la
 * entrega: por dónde iba, con qué versión se predica y si ya se predicó.
 */

/* Los bosquejos que hay: nombres de módulo. g_list_free_full(l, g_free). */
GList *main_pulpito_sermones(void);
/* TRUE si ese módulo es un bosquejo que se puede llevar al púlpito. */
gboolean main_pulpito_es_sermon(const char *modulo);

/* Abre el bosquejo y resuelve de una vez el texto de todos los pasos:
 * en el púlpito no se va a la base de datos en cada pulsación. NULL si
 * el módulo no existe. */
PU_SERMON *main_pulpito_abrir(const char *modulo);
void main_pulpito_cerrar(PU_SERMON *sermon);

/* El texto de una referencia, versículo o rango, con los números de
 * versículo si son varios. NULL si no se pudo. Para las capas, que sí
 * piden texto en caliente. */
gchar *main_pulpito_texto(const char *version, const char *ref);
/* "Efesios 2:5" -> "Efesios 2:1-22": el capítulo entero de esa cita. */
gchar *main_pulpito_capitulo(const char *version, const char *ref);

/* --- estado de la entrega, en settings.xml --- */

int main_pulpito_ultimo_paso(const char *modulo);
void main_pulpito_guardar_paso(const char *modulo, int paso);
gboolean main_pulpito_predicado(const char *modulo);
void main_pulpito_marcar_predicado(const char *modulo, gboolean predicado);

/* Qué se manda a la segunda pantalla, si hay. El orden es el de menos a
 * más: nada, el versículo, el enunciado del punto, y los dos. */
typedef enum {
	PU2_NADA = 0,
	PU2_VERSO,
	PU2_PUNTO,
	PU2_AMBOS
} PU_SEGUNDA;

PU_SEGUNDA main_pulpito_segunda(void);
void main_pulpito_segunda_poner(PU_SEGUNDA que);

/* La duración prevista, en minutos, para el aviso del tiempo: el reloj
 * del atril se pone ámbar cerca del final y rojo al pasarse. 0 es no
 * querer aviso, que es como viene de fábrica: quien no lo pida no verá
 * un reloj cambiándole de color a media predicación. */
int main_pulpito_objetivo(void);
void main_pulpito_objetivo_poner(int minutos);

/* El zoom del púlpito va aparte del de lectura a propósito: el tamaño
 * que se lee de cerca en el escritorio no es el que se ve desde el
 * atril. Es un porcentaje, 100 = el tamaño de partida. */
int main_pulpito_zoom(void);
void main_pulpito_zoom_poner(int porciento);

#ifdef __cplusplus
}
#endif
#endif
