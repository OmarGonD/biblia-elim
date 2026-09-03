/*
 * Biblia Elim
 * versiculo_dia.h - el versículo del día y la reflexión del lector
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __VERSICULO_DIA_H__
#define __VERSICULO_DIA_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* El versículo del día sale de una tabla de citas fija y de la fecha, y
 * de nada más: dos lectores en el mismo día ven la misma, sin descargar
 * nada ni preguntarle a ningún servidor. El texto no está aquí; se le
 * pide al módulo que el lector tenga abierto, así que sale en su versión
 * y no hay que citar a nadie de memoria. */

/* Cuántas citas distintas hay. La rueda tarda ese número de días en dar
 * la vuelta, que no es un año justo a propósito: así el 3 de septiembre
 * no cae siempre en el mismo versículo. */
int main_versiculo_cuantos(void);

/* La cita de esa fecha en castellano ("Juan 3:16"), y la clave con el
 * nombre OSIS del libro ("John 3:16") que es la que entiende el motor.
 * Las dos hay que liberarlas con g_free(). */
gchar *main_versiculo_cita(GDateTime *fecha);
gchar *main_versiculo_clave(GDateTime *fecha);

/* --- la reflexión ---
 *
 * Una por día, guardada en settings.xml junto al resto:
 *   <versiculo><reflexion label="2026-09-03" list="..."/></versiculo>
 * El texto va escapado como el de las notas de versículo, porque puede
 * llevar saltos de línea y comillas y va dentro de un atributo. */

/* Fecha "AAAA-MM-DD" de un GDateTime, que es como se guarda todo aquí.
 * Hay que liberarla con g_free(). */
gchar *main_versiculo_fecha(GDateTime *fecha);

/* Lo escrito ese día, o NULL si no hay nada. g_free(). */
gchar *main_versiculo_reflexion(const char *fecha);
/* Guarda, o borra la entrada si el texto queda vacío. */
void main_versiculo_reflexion_poner(const char *fecha, const char *texto);

/* Cuántos días llevan algo escrito. */
int main_versiculo_reflexiones_cuantas(void);

#ifdef __cplusplus
}
#endif
#endif
