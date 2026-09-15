/*
 * Xiphos Bible Study Tool
 * bibletext.h - gui for Bible text modules
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

#ifndef ___BIBLETEXT_H_
#define ___BIBLETEXT_H_

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

void gui_setup_bibletext(void);
gboolean gui_text_button_release_event(GtkWidget *widget,
				       GdkEventButton *event,
				       gpointer data);
void gui_popup_pm_text(void);
GtkWidget *gui_create_bible_pane(void);

/* Kindle-style selection-highlight notes: open the view/edit popover
 * for a given highlight group id (used by the showHlNote passagestudy
 * action in main/url.cc). */
void gui_open_highlight_note_by_id(const gchar *group_id);

/* All notes (highlight + whole-verse) touching one verse: view, edit,
 * link to other verses, or add a brand new whole-verse note (used by
 * the showHlNotes passagestudy action in main/url.cc). */
void gui_show_verse_notes_dialog(const gchar *module, const gchar *passage);

/* Mark the navigated verse with the current-verse band (always). */
void gui_bibletext_mark_current_verse(void);
/* "Comparar" reading focus: focuses the navigated-to verse
 * (settings.currentverse) -- used when the panel opens and whenever
 * the navigated verse changes while it's open. */
void gui_bibletext_lectura_sync_focus_current(void);
/* Runs a reading-focus update still waiting for the next frame after a
 * user scroll, so a navigation that follows starts from the verse the
 * reader scrolled to (see on_up/down_eventbox_button_release_event). */
void gui_bibletext_reading_focus_flush(void);
/* The Bible pane's text view (NULL before it exists): where the keyboard
 * focus goes when a navbar picker has nothing else to give it back to. */
GtkWidget *gui_bibletext_view(void);
/* Clears the reading-focus highlight (used when the panel closes). */
void gui_bibletext_lectura_sync_clear_focus(void);
/* Vuelca la posición de lectura seguida durante la sesión (dónde quedó
 * la vista tras el último desplazamiento) a keys/verse y a la pestaña
 * actual, para retomar ahí en el próximo arranque. Se llama al cerrar,
 * antes de guardar settings.xml. */
void gui_bibletext_guardar_posicion_lectura(void);

#ifdef __cplusplus
}
#endif

#endif
