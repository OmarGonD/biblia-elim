/*
 * Biblia Elim
 * notas_verso.h - pestaña de notas del versículo enfocado, dentro del
 * panel Comentario/Libro
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef ___NOTAS_VERSO_H_
#define ___NOTAS_VERSO_H_

#ifdef __cplusplus
extern "C" {
#endif

GtkWidget *gui_create_notes_pane(void);

/* Llamar tras cada navegación/redisplay del versículo actual. Decide
 * siempre de forma determinista si el panel Comentario/Libro debe
 * estar visible según si ese versículo tiene una nota guardada --
 * sin excepción, para poder corregir cualquier visibilidad obsoleta
 * que algo externo (memoria por pestaña en tabbed_browser.c) le haya
 * pisado de por medio. El contenido de la pestaña "Notas" (texto,
 * título) solo se recarga en un cambio real de versículo, para no
 * pisar una nota que el usuario esté escribiendo todavía sin
 * guardar. */
void gui_verse_notes_panel_actualizar(void);

/* Vuelca al disco la nota que se esté escribiendo, si la hay. El panel
 * ya guarda solo al salir del cuadro, al cambiar de versículo y tras una
 * pausa al teclear; esto es para el cierre de la aplicación, que no pasa
 * por ninguno de los tres. */
void gui_verse_notes_guardar_pendiente(void);

#ifdef __cplusplus
}
#endif

#endif
