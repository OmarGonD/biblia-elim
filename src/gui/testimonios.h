/*
 * Biblia Elim
 * testimonios.h - diálogo «Jesús en la historia»
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___GUI_TESTIMONIOS_H_
#define ___GUI_TESTIMONIOS_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_testimonios_dialog(GtkWindow *padre);

/* Abre el diálogo con una fuente concreta ya elegida ("tacito"). */
void gui_testimonios_mostrar(const char *id);

#ifdef __cplusplus
}
#endif
#endif
