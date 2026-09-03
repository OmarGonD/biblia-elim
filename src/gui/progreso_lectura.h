/*
 * Biblia Elim
 * progreso_lectura.h - diálogo Tu progreso
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___PROGRESO_LECTURA_H_
#define ___PROGRESO_LECTURA_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Cuánto lleva leído el lector: de la Biblia entera, de cada libro y de
 * cada plan. `padre` puede ser NULL. */
void gui_progreso_lectura_dialog(GtkWindow *padre);

#ifdef __cplusplus
}
#endif
#endif
