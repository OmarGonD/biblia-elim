/*
 * Biblia Elim
 * pulpito.h - la ventana de púlpito
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___PULPITO_H_
#define ___PULPITO_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Abre el bosquejo en la vista de entrega, a pantalla completa y en su
 * propia ventana. No toca el modo lectura ni la ventana principal: es
 * otra pantalla, no el modo lectura con la letra más grande. */
void gui_pulpito_abrir(const char *modulo);

/* El diálogo de elegir qué bosquejo llevar al púlpito, para el menú. */
void gui_pulpito_elegir(GtkWindow *padre);

#ifdef __cplusplus
}
#endif
#endif
