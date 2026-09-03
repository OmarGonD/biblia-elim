/*
 * Biblia Elim
 * plan_personal.h - diálogo del plan de lectura que arma el lector
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___PLAN_PERSONAL_H_
#define ___PLAN_PERSONAL_H_

#include <gtk/gtk.h>

#include "main/planes_lectura.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Abre el diálogo del plan personalizado y espera. Con plan == NULL
 * crea uno nuevo; si no, edita ese. Devuelve el plan guardado (el mismo
 * puntero cuando se edita) o NULL si el lector canceló. */
const PL_PLAN *gui_plan_personal_dialog(GtkWindow *padre,
					const PL_PLAN *plan);

#ifdef __cplusplus
}
#endif
#endif
