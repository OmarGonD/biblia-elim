/*
 * Biblia Elim
 * busqueda_tildes.h - buscar en español sin pelearse con las tildes
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

#ifndef __BUSQUEDA_TILDES_H__
#define __BUSQUEDA_TILDES_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Devuelve la palabra convertida en expresión regular, con cada vocal
 * abierta a su forma con tilde y la n abierta a ñ, de modo que
 * «espiritu» encuentre «Espíritu» y al revés. Se libera con g_free().
 * Devuelve NULL si la palabra no tiene ninguna letra que abrir. */
gchar *elim_tildes_regex_palabra(const gchar *palabra);

/* Parte la consulta en palabras y devuelve la expresión regular de cada
 * una, en un vector terminado en NULL que se libera con g_strfreev().
 *
 * Devuelve NULL cuando no procede tocar lo escrito: si la consulta trae
 * caracteres propios de una expresión regular (la escribió quien sabe lo
 * que hace) o si ninguna palabra gana nada con abrirse. */
gchar **elim_tildes_regex_consulta(const gchar *consulta);

/* Reduce el texto a su forma neutra -- sin tildes y en minúsculas -- y,
 * si `mapa` no es NULL, va anotando en él a qué carácter del original
 * corresponde cada carácter del resultado, más un último apunte con el
 * total. `mapa` ha de ser un GArray de gint ya creado y vacío.
 *
 * Sirve para buscar dentro de un texto ya cargado sabiendo dónde cae
 * cada coincidencia. Se libera con g_free(). */
gchar *elim_tildes_neutro(const gchar *texto, GArray *mapa);

#ifdef __cplusplus
}
#endif

#endif /* __BUSQUEDA_TILDES_H__ */
