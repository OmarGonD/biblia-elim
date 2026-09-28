/*
 * Xiphos Bible Study Tool
 * navbar.h - state shared by the navbars
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

#ifndef _NAVBAR_H
#define _NAVBAR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <config.h>
#include <glib.h>

/* set by whichever navbar last redisplayed, read by the others */
extern gboolean do_display;
extern gboolean do_display_dict;

#ifdef __cplusplus
}
#endif
#endif /* _NAVBAR_H */
