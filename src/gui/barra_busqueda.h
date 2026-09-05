/*
 * Biblia Elim
 * barra_busqueda.h - buscar dentro del texto, sin ventana que estorbe
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
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

#ifndef __BARRA_BUSQUEDA_H__
#define __BARRA_BUSQUEDA_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Crea la barra y la devuelve para empotrarla en la ventana principal.
 * Nace plegada; se despliega con gui_barra_busqueda_mostrar(). */
GtkWidget *gui_barra_busqueda_crear(void);

/* Despliega la barra y la apunta al panel dado (el texto bíblico, el
 * comentario, el libro o el diccionario). */
void gui_barra_busqueda_mostrar(GtkWidget *html);

void gui_barra_busqueda_ocultar(void);

#ifdef __cplusplus
}
#endif

#endif /* __BARRA_BUSQUEDA_H__ */
