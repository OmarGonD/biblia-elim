/*
 * Xiphos Bible Study Tool
 * parallel_view.h - support for displaying multiple modules
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

#ifndef ___parallel_H_
#define ___parallel_H_

#ifdef __cplusplus
extern "C" {
#endif

void gui_create_parallel_page(void);
/* Lays both versions of the docked page out again around the current
 * passage -- now if the page is showing or `force`, otherwise the next
 * time it is shown. */
void gui_parallel_panes_update(gboolean force);
/* The current passage moved (navigation): when both panes already hold
 * it, they glide there and the focus bands move, nothing is laid out
 * again; otherwise as gui_parallel_panes_update(). `animate` FALSE jumps. */
void gui_parallel_panes_follow(gboolean animate);
/* The docked page was just brought to the front. */
void gui_parallel_page_shown(void);
/* The reader is reading in pane `pane` (0 left, 1 right): as it scrolls,
 * the other pane follows it. Scrolling, clicking or typing in a pane does
 * this. */
void gui_parallel_pane_take_lead(gint pane);
/* Which versions the two panes show (not owned), for tests. */
const char *gui_parallel_pane_module(gint pane);
GtkWidget *gui_parallel_pane(gint pane);
/* How many times a pane has been laid out, for tests. */
guint gui_parallel_panes_layouts(void);
void gui_create_parallel_popup(void);
void gui_popup_menu_parallel(GtkWidget *relative);
void on_undockInt_activate(gpointer unused);
void on_paratab_activate(gpointer unused);

#ifdef __cplusplus
}
#endif
#endif
