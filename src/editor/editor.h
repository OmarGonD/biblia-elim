/*
 * Xiphos Bible Study Tool
 * editor.h - shared types and API of the note/book/studypad editor
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

#ifndef _EDITOR_H
#define _EDITOR_H

#include <config.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <gtk/gtk.h>

#include "main/navbar_versekey.h"

struct _tool_items
{
	/* GTK4 has no GtkToolButton hierarchy.  The GtkTextView editor
	 * stores the controls as their GTK4 base types. */
	GtkToggleButton *bold;
	GtkToggleButton *italic;
	GtkToggleButton *underline;
	GtkToggleButton *strike;
	GtkWidget *color;
	GtkButton *newdoc;
	GtkButton *open;
	GtkButton *deletedoc;
	GtkWidget *cb;
};
typedef struct _tool_items TOOL_ITEMS;

typedef struct _editor EDITOR;
struct _editor
{
	GtkWidget *window;
	GtkWidget *toolbar;
	GtkWidget *treeview;
	GtkWidget *sync_button;
	GtkWidget *html_widget;
	GtkWidget *statusbar;
	GtkWidget *text_widget;  // For GtkTextView
	GtkWidget *navbar_box;   // For navbar
	GtkWidget *box;          // For main vbox
	TOOL_ITEMS toolitems;

	NAVBAR_VERSEKEY navbar;

	gint type;

	gboolean studypad;
	gboolean noteeditor;
	gboolean bookeditor;
	gboolean is_changed;
	gboolean sync;

	gchar *filename;
	gchar *module;
	gchar *key;
};

enum {
	STUDYPAD_EDITOR,
	NOTE_EDITOR,
	BOOK_EDITOR
};

void editor_sync_with_main(void);
void editor_load_note(EDITOR *e, const gchar *module_name,
		      const gchar *key);
void editor_load_book(EDITOR *e);
gint editor_create_new(const gchar *filename, const gchar *key,
		       gint note);
void editor_save_book(EDITOR *e);
void editor_maybe_save_all(void);
GtkWidget *editor_new(const gchar *title, EDITOR *e);

#ifdef __cplusplus
}
#endif
#endif /* _EDITOR_H */
