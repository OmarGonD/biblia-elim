/*
 * Xiphos Bible Study Tool
 * parallel_view.h - support for displaying multiple modules
 *
 * Copyright (C) 2004-2026 Xiphos Developer Team
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

#ifndef __PARALLEL_VIEW_H_
#define __PARALLEL_VIEW_H_

#ifdef __cplusplus
extern "C" {
#endif

gchar *main_parallel_change_verse(void);
gchar *main_parallel_html(void); /* caller frees */
/* GTK4-PORT-101 step 2: appends the module options to MENU; their
 * «paralelo» actions go into ACTIONS. */
void main_parallel_options_menu(GMenu *menu, GActionMap *actions);
void main_set_parallel_options_at_start(void);
void main_load_menu_form_mod_list(GtkWidget *pmInt, gchar *label,
				  GCallback mycallback);
void main_check_parallel_modules(void);
void main_set_parallel_options_at_start(void);
void main_update_parallel_page(void);
/* One pane of the docked page: `module`'s chapters around the current
 * passage, as the main pane lays them out. FALSE when the module has no
 * such passage (the pane then says so). */
gboolean main_parallel_render_pane(GtkWidget *html, const char *module);
/* The anchor (chapter * 1000 + verse, verse 0 for the chapter's heading)
 * in `to_mod`'s pane naming the verse `anchor` names in `from_mod`'s, or
 * 0 when there is no such verse in the book that pane holds. */
/* The anchor of the current passage in `module`'s pane, 0 if none. */
gint main_parallel_current_anchor(const char *module);
/* The same, with the OSIS book it is in (g_free(), NULL if none). */
gint main_parallel_current_position(const char *module, gchar **book);
gint main_parallel_map_anchor(const char *from_mod, const char *to_mod,
			      gint anchor);
void main_update_parallel_page_detached(void);
/* Renders the verse-aligned comparison into the main reading pane. */
gboolean main_reading_compare_render(const char *key);
void main_swap_parallel_with_main(char *intmod);

void main_init_parallel_view(void);
void main_delete_parallel_view(void);

#ifdef __cplusplus
}
#endif
#endif
