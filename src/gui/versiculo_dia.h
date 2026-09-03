/*
 * Biblia Elim
 * versiculo_dia.h - diálogo Versículo del día
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___VERSICULO_DIA_H_
#define ___VERSICULO_DIA_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_versiculo_dia_dialog(GtkWindow *padre);

/* Vuelca la reflexión que se esté escribiendo. El diálogo ya guarda al
 * cambiar de día, al salir del cuadro y tras una pausa al teclear; esto
 * es para el cierre de la aplicación, que no pasa por ninguno. */
void gui_versiculo_dia_guardar_pendiente(void);

#ifdef __cplusplus
}
#endif
#endif
