/*
 * Biblia Elim
 * planes_lectura.h - planes de lectura listos y su progreso
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PLANES_LECTURA_H__
#define __PLANES_LECTURA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Un tramo de capítulos seguidos de un libro. cap_fin == 0 quiere decir
 * "hasta el último capítulo del libro", que es como se escriben los
 * libros completos sin repetir la cuenta en la tabla. */
typedef struct {
	short libro;
	short cap_ini;
	short cap_fin;
} PL_TRAMO;

/* Una vía es un hilo de lectura que corre en paralelo a los demás
 * durante los mismos días: el plan de un año lleva una vía de Antiguo
 * Testamento y otra de Nuevo, y Salmos/Proverbios una de cada libro.
 * Los capítulos de la vía se reparten parejo entre los días del plan. */
typedef struct {
	const char *nombre;
	const PL_TRAMO *tramos;
	int n_tramos;
} PL_VIA;

typedef struct {
	const char *id;
	const char *nombre;
	const char *descripcion;
	int dias;
	const PL_VIA *vias;
	int n_vias;
	/* Plan curado: en vez de repartir capítulos, el tramo i es la
	 * lectura del día i (con su título en titulos[i], si lo hay). */
	gboolean tramo_por_dia;
	const char *const *titulos;
} PL_PLAN;

int main_planes_cuantos(void);
const PL_PLAN *main_planes_get(int i);
const PL_PLAN *main_planes_por_id(const char *id);

/* Cuántos capítulos tiene el plan entero (para el resumen). */
int main_planes_total_capitulos(const PL_PLAN *plan);

/* Cómo queda el reparto en palabras: "3 capítulos por día", "entre 3 y
 * 4 capítulos por día". Hay que liberarlo con g_free(). */
char *main_planes_ritmo_texto(int capitulos, int dias);

/* Lectura del día (1..plan->dias) en castellano, lista para mostrar:
 * "Génesis 1-3 · Mateo 1". Devuelve NULL si el día está fuera de rango;
 * si no, una cadena que hay que liberar con g_free(). */
char *main_planes_lectura(const PL_PLAN *plan, int dia);

/* Título del día en los planes curados, o NULL. No hay que liberarlo. */
const char *main_planes_titulo(const PL_PLAN *plan, int dia);

/* Referencias de capítulo del día, en el orden en que se leen, para
 * abrirlas en la ventana principal. Lista de gchar* a liberar con
 * g_list_free_full(lista, g_free). */
GList *main_planes_referencias(const PL_PLAN *plan, int dia);

/* --- progreso, guardado en settings.xml --- */

/* Id del plan que el lector tiene en curso, o NULL. No liberar. */
const char *main_planes_activo(void);
void main_planes_activar(const PL_PLAN *plan);
void main_planes_soltar(void);

/* Fecha ("AAAA-MM-DD") en que se empezó el plan, o NULL si nunca. */
const char *main_planes_inicio(const PL_PLAN *plan);

gboolean main_planes_dia_hecho(const PL_PLAN *plan, int dia);
void main_planes_marcar(const PL_PLAN *plan, int dia, gboolean hecho);
int main_planes_dias_hechos(const PL_PLAN *plan);
/* Primer día sin marcar: lo que toca leer ahora. */
int main_planes_dia_de_hoy(const PL_PLAN *plan);
/* Día en que iría el lector si hubiera leído uno por jornada desde que
 * empezó; sirve para decirle si va al día, adelantado o atrasado. 0 si
 * el plan no se ha empezado. */
int main_planes_dia_segun_calendario(const PL_PLAN *plan);
void main_planes_reiniciar(const PL_PLAN *plan);

/* Cómo está la lectura de hoy del plan en curso. `detalle`, si no es
 * NULL, recibe una línea lista para mostrar ("Día 12 de 365 · Génesis
 * 30-31 · Mateo 9", o "La Biblia en un año · terminado") que hay que
 * liberar con g_free(). Vive aquí y no en la capa de ventanas porque el
 * aviso de systemd la necesita sin GTK por medio. */
typedef enum {
	PL_HOY_SIN_PLAN = 0,	/* no hay plan en curso */
	PL_HOY_PENDIENTE,	/* queda lectura por marcar */
	PL_HOY_TERMINADO	/* el plan está entero */
} PL_HOY;

PL_HOY main_planes_estado_hoy(gchar **detalle);

/* --- ponerse al día ---
 *
 * Dos maneras honradas de recuperar los días perdidos: dar por leído lo
 * atrasado, o correr el calendario para que hoy sea el día que toca sin
 * marcar nada que no se haya leído. */

/* Marca como leídos los días 1..dia. */
void main_planes_marcar_hasta(const PL_PLAN *plan, int dia);
/* Mueve la fecha de inicio para que el día pendiente sea el de hoy. */
void main_planes_reprogramar(const PL_PLAN *plan);
/* Días de retraso respecto al calendario, 0 si va al día o adelantado. */
int main_planes_dias_atrasados(const PL_PLAN *plan);

/* --- capítulos leídos ---
 *
 * Lo que el lector lleva marcado en todos sus planes, resuelto a
 * capítulos: es lo que permite decir cuánto lleva de cada libro y de la
 * Biblia entera aunque lo haya leído a trozos y con varios planes.
 * `por_libro`, si no es NULL, recibe un entero por libro. Devuelve el
 * total de capítulos distintos leídos. */
int main_planes_capitulos_leidos(int *por_libro);

/* --- recordatorio diario ---
 *
 * Una hora del día, guardada en este equipo y en ningún otro sitio. */

/* TRUE si está puesto; la hora sale en *hora y *minuto (pueden ser
 * NULL). Sin poner, devuelve FALSE y las 7:00 como propuesta. */
gboolean main_planes_recordatorio(int *hora, int *minuto);
void main_planes_recordatorio_poner(gboolean activo, int hora, int minuto);

/* Día del último aviso ("AAAA-MM-DD"), o NULL si todavía ninguno: es lo
 * que evita repetir el aviso cada vez que se abre la aplicación. */
const char *main_planes_recordatorio_ultimo(void);
void main_planes_recordatorio_avisado(void);

/* --- planes que arma el lector ---
 *
 * Elige libros (enteros) y cuántos días quiere; los capítulos se
 * reparten parejo entre esos días, igual que en los planes de la casa.
 * Los libros van en el vector de main_planes_libros_cuantos() casillas
 * que usan estas funciones, una por libro y en orden canónico. */

int main_planes_libros_cuantos(void);
const char *main_planes_libro_nombre(int libro);
int main_planes_libro_capitulos(int libro);
gboolean main_planes_libro_es_nt(int libro);

/* Capítulos que suman los libros marcados. */
int main_planes_capitulos_de(const gboolean *libros);

gboolean main_planes_es_personal(const PL_PLAN *plan);

/* Crea uno y lo guarda; devuelve el plan, o NULL si no se marcó ningún
 * libro. El puntero vale hasta que se borre ese plan. */
const PL_PLAN *main_planes_personal_nuevo(const char *nombre,
					  const gboolean *libros, int dias);
/* Cambia uno que ya existe, sin mover el puntero ni tocar el progreso. */
gboolean main_planes_personal_editar(const PL_PLAN *plan, const char *nombre,
				     const gboolean *libros, int dias);
/* Lo borra junto con su progreso; el puntero deja de valer. */
void main_planes_personal_borrar(const PL_PLAN *plan);
/* Marca en `libros` los que lleva el plan, para volver a editarlo. */
void main_planes_personal_libros(const PL_PLAN *plan, gboolean *libros);

#ifdef __cplusplus
}
#endif
#endif
