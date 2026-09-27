/*
 * Xiphos Bible Study Tool
 * menu_popup.h - creation of (and call backs) for xiphos popup menus
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

#ifndef __MENU_POPUP__H_
#define __MENU_POPUP__H_

#ifdef __cplusplus
extern "C" {
#endif


#include "main/module_dialogs.h"
#include "xiphos_html/xiphos_html.h"

GtkWidget *gui_menu_popup(XiphosHtml *html, const gchar *mod_name,
			  DIALOG_DATA *d);

#ifdef __cplusplus
}
#endif
#endif /* __MENU_POPUP__H_ */
