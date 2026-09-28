/*
 * Biblia Elim
 * entry_suggest.h - suggestions under a text entry, in place of the
 *                   deprecated GtkEntryCompletion (which needs a
 *                   GtkListStore)
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

#ifndef GUI_ENTRY_SUGGEST_H
#define GUI_ENTRY_SUGGEST_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether CANDIDATE is a suggestion for what the reader has typed, KEY. */
typedef gboolean (*ElimSuggestMatchFunc)(const char *key, const char *candidate,
					 gpointer data);

typedef struct _ElimEntrySuggest ElimEntrySuggest;

/* While the reader types in ENTRY (a GtkEntry or anything else that is a
 * GtkEditable) a popover under it lists the candidates that MATCH the text,
 * at most a dozen. Clicking one puts it in the entry; Up and Down pick one
 * from the keyboard, Enter takes it (with none picked, Enter is the entry's
 * "activate" as ever) and Escape closes the list. Nothing is suggested for an
 * empty entry. The object goes with the entry. */
ElimEntrySuggest *elim_entry_suggest_new(GtkWidget *entry,
					 ElimSuggestMatchFunc match,
					 gpointer data);

/* The candidates: a GList of strings, copied. Replaces the previous ones. */
void elim_entry_suggest_set_candidates(ElimEntrySuggest *suggest,
				       const GList *candidates);

/* What is showing now, for the tests: the list view, and whether it is up. */
GtkWidget *elim_entry_suggest_get_list(ElimEntrySuggest *suggest);
gboolean elim_entry_suggest_is_open(ElimEntrySuggest *suggest);

#ifdef __cplusplus
}
#endif

#endif /* GUI_ENTRY_SUGGEST_H */
