/*
 * Biblia Elim
 * recordatorio.h - el aviso diario de la lectura
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___RECORDATORIO_H_
#define ___RECORDATORIO_H_

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Pone en marcha el reloj que mira si toca avisar. Se llama una vez, al
 * terminar de arrancar la ventana principal. */
void gui_recordatorio_arrancar(void);

/* Manda el aviso ahora mismo, diga lo que diga el reloj: es el botón
 * «Probar» del diálogo de planes, para ver si el escritorio los
 * muestra. */
void gui_recordatorio_probar(void);

/* Pide que las unidades de systemd se pongan al día con la hora que se
 * acaba de elegir. No lo hace en el acto: los botones de la hora sueltan
 * un cambio por pulsación, y no vamos a recargar systemd en cada una.
 * Ver main/recordatorio.h. */
void gui_recordatorio_sincronizar_pronto(void);

#ifdef __cplusplus
}
#endif
#endif
