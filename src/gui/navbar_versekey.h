/*
 * Xiphos Bible Study Tool
 * navbar_versekey.h - navigation bar for bible references
 *
 * Copyright (C) 2007-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ___NAVBAR_VERSEKEY_H_
#define ___NAVBAR_VERSEKEY_H_

#include "main/navbar_versekey.h"

#ifdef __cplusplus
extern "C" {
#endif

GtkWidget *gui_navbar_versekey_new(void);

/* Re-sincroniza el combo de versión (widgets.combo_bible_version) con
 * settings.MainWindowModule. Llamar cada vez que el módulo activo de
 * la ventana principal cambie por una vía que no sea el propio combo
 * (árbol de módulos del panel lateral, reemplazo automático de un
 * módulo desinstalado, etc.) para que el selector no quede
 * desactualizado. */
void gui_navbar_version_combo_sync(void);
/* Llena el picker de versiones. Lo llama main() cuando Sword ya está
 * levantado: antes de eso no se le puede preguntar el idioma a un
 * módulo. */
void gui_navbar_version_combo_refill(void);

#ifdef __cplusplus
}
#endif

#endif
