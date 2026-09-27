/*
 * Xiphos Bible Study Tool
 * tab_history.h - add, remove and navigate history
 *
 * Copyright (C) 2005-2026 Xiphos Developer Team
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

#ifndef __TAB_HISTORY_H_
#define __TAB_HISTORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#define TABHISTORYLENGTH 50	/* history list max length */
#define TABELEMENTLENGTH 75	/* individual element content max length */
				/* some language names take absurd space */
				/* someday, this too may be found insufficient */

struct _tab_history
{
	gchar verseref[TABELEMENTLENGTH];
	gchar textmod[TABELEMENTLENGTH];
	gchar commod[TABELEMENTLENGTH];
};
typedef struct _tab_history TAB_HISTORY;

void main_update_tab_history_menu(gpointer data);
/* GTK4-PORT-101 step 2: the history drop-down is a GMenu whose items
 * call the «historial» actions installed on WIDGET (clear, to the verse
 * list, go to entry i). */
void main_tab_history_install_actions(GtkWidget *widget);
GMenuModel *main_tab_history_menu_model(gpointer data);
void main_clear_tab_history(void);
void main_add_tab_history_item(gpointer data);
void main_fake_tab_history_item(char *reference);
void main_navigate_tab_history(gint direction);
void main_change_verse_tab_history(gint historynum);

#ifdef __cplusplus
}
#endif
#endif
