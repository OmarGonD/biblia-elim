/*
 * Biblia Elim
 * memorizacion.h - diálogo Memorización
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___MEMORIZACION_H_
#define ___MEMORIZACION_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_memorizacion_dialog(GtkWindow *padre);

/* Da de alta un versículo y avisa por la barra de estado. Es lo que
 * llama el botón «Memorizar este» del diálogo del versículo del día. */
void gui_memorizacion_anadir(const char *clave);

#ifdef __cplusplus
}
#endif
#endif
