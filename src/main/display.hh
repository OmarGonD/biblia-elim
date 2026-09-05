/*
 * Xiphos Bible Study Tool
 * display.hh -
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
#include <gtk/gtk.h>
#include <swmgr.h>
#include <swdisp.h>
#include "main/gtk_compat.h"
#include "main/global_ops.hh"
#include "backend/sword_main.hh"
#include "gui/utilities.h"

using namespace sword;

class GTKEntryDisp : public SWDisplay
{
      public:
	GTKEntryDisp(GtkWidget *_gtkText,
		     BackEnd *_be)
	    : gtkText(_gtkText),
	      be(_be),
	      swbuf(""),
	      mf(NULL),
	      ops(NULL),
	      buf(NULL),
	      mod_column_count(NULL),
	      is_rtol(FALSE),
	      strongs_or_morph(FALSE),
	      strongs_and_morph(FALSE),
	      cache_flags(0)
	{
	}
	virtual char display(SWModule &imodule);
	virtual char displayByChapter(SWModule &imodule,
				      int columns);

      protected:
	GtkWidget *gtkText;
	BackEnd *be;
	SWBuf swbuf;
	MOD_FONT *mf;
	GLOBAL_OPS *ops;

	gboolean is_rtol;

	gboolean strongs_or_morph;
	gboolean strongs_and_morph;

	int cache_flags;

	gchar *buf, *mod_column_count;
};

class GTKChapDisp : public GTKEntryDisp
{
      public:
	GTKChapDisp(GtkWidget *_gtkText,
		    BackEnd *_be)
	    : GTKEntryDisp(_gtkText, _be)
	{
	}
	virtual char display(SWModule &imodule);
	virtual GString *introMaterial(SWModule &imodule, int chapter);
	virtual void getVerseBefore(SWModule &imodule);
	virtual void getVerseAfter(SWModule &imodule);
	virtual void RenderOneChapter(SWModule &imodule, int chapter);
	virtual void RenderWholeBook(SWModule &imodule);

      private:
	int curTest, curBook, curChapter, curVerse;
	VerseKey *key;
};

class DialogEntryDisp : public SWDisplay
{
      public:
	DialogEntryDisp(GtkWidget *_gtkText,
			DIALOG_DATA *_d,
			BackEnd *_be)
	    : gtkText(_gtkText),
	      d(_d),
	      be(_be),
	      swbuf(""),
	      mf(NULL),
	      ops(NULL),
	      buf(NULL),
	      mod_column_count(NULL),
	      is_rtol(FALSE),
	      strongs_or_morph(FALSE),
	      strongs_and_morph(FALSE),
	      cache_flags(0)
	{
	}
	virtual char display(SWModule &imodule);
	virtual char displayByChapter(SWModule &imodule,
				      int columns);

      protected:
	GtkWidget *gtkText;
	DIALOG_DATA *d;
	BackEnd *be;
	SWBuf swbuf;
	MOD_FONT *mf;
	GLOBAL_OPS *ops;

	gboolean is_rtol;

	gboolean strongs_or_morph;
	gboolean strongs_and_morph;

	int cache_flags;

	gchar *buf, *mod_column_count;
};

class DialogChapDisp : public DialogEntryDisp
{
      public:
	DialogChapDisp(GtkWidget *_gtkText,
		       DIALOG_DATA *_d,
		       BackEnd *_be)
	    : DialogEntryDisp(_gtkText, _d, _be)
	{
	}
	virtual char display(SWModule &imodule);

      private:
	int curTest, curBook, curChapter, curVerse;
	VerseKey *key;
};

class GTKPrintEntryDisp : public SWDisplay
{
      public:
	GTKPrintEntryDisp(GtkWidget *_gtkText,
			  BackEnd *_be)
	    : gtkText(_gtkText),
	      be(_be)
	{
	}
	virtual char display(SWModule &imodule);

      protected:
	GtkWidget *gtkText;
	BackEnd *be;
};

class GTKPrintChapDisp : public GTKPrintEntryDisp
{
      public:
	GTKPrintChapDisp(GtkWidget *_gtkText,
			 BackEnd *_be)
	    : GTKPrintEntryDisp(_gtkText, _be)
	{
	}
	virtual char display(SWModule &imodule);
	MOD_FONT *mf;
};

extern "C" {
#endif /* __cplusplus */

/* Unified note/highlight cache fill -- one XML section ("osisrefnotes")
 * for both whole-verse "Mark Verse" annotations and phrase-level
 * Kindle-style highlights (see the NoteElement comment in display.cc). */
void notesCacheFill(const gchar *modname, gchar *key);

/* arbitrary text-selection highlights (Kindle-style), separate from the
 * whole-verse "Mark Verse" annotation system above -- see src/gtk/bibletext.c
 * for the WebKit selection bridge that drives these. One selection may
 * span several consecutive verses; the caller (bibletext.c) splits it
 * into one HighlightSegment per verse touched (usually just one) and
 * hands the whole list to highlight_create_group(), which ties them
 * together under one group id so later edits apply to all of them. */
typedef struct
{
	gchar *osisref; /* "Book.C.V" */
	gchar *text;	/* this verse's portion of the selected text */
	gint pos;	/* UTF-8 offset of `text` relative to the verse's own
			 * start, in the live GtkTextBuffer at selection time
			 * (see wk_html_anchor_bounds()); -1 if it couldn't be
			 * computed. Lets re-renders relocate the highlight
			 * exactly instead of guessing via substring search. */
} HighlightSegment;

char *highlight_find_overlapping(const gchar *module, const gchar *osisref, const gchar *text);
char *highlight_create_group(const gchar *module, GList *segments,
			     const gchar *color);
void highlight_set_color(const gchar *group_id, const gchar *color);
void highlight_set_note(const gchar *group_id, const gchar *note);
void highlight_remove(const gchar *group_id);
char *highlight_get_color(const gchar *group_id);
char *highlight_get_note(const gchar *group_id);

typedef struct {
	gchar *group_id; /* NULL = anotación de versículo entero */
	gchar *module;   /* la versión en la que se escribió */
	gchar *osisref;  /* "Book.C.V" */
	gchar *text;     /* frase subrayada, o NULL */
	gchar *note;
	gchar *color;
	gchar *note_key; /* "HL:<group_id>" o "MV:<osisref>", identidad estable
			  * de la nota, usada para enlazarla con otras. */
	int chapter_verse;
	guint32 orden;   /* posición canónica, para ordenar entre libros */
} HighlightNote;

void highlight_note_free(HighlightNote *n);
/* osis_prefix NULL = libro actual; "Book.C" = capítulo; "Book.C.V" = versículo. */
GList *highlight_list_notes(const gchar *osis_prefix);

/* Todas las notas escritas, de cualquier libro y cualquier versión, en
 * orden canónico. highlight_list_notes() no vale para esto: mira la
 * caché, y la caché solo tiene el libro que está abierto. Esta va al
 * XML entero, así que no se llama en un redibujo -- es para buscar. */
GList *highlight_all_notes(void);
int highlight_count_notes(const gchar *osis_prefix);
int highlight_count_notes_at(int chapter_verse);

/* Notas de versículo completo (sin selección de texto) -- comparten
 * almacenamiento unificado ("osisrefnotes") con los resaltados de frase.
 * note_set_whole_verse()/note_remove_whole_verse() son el setter genérico
 * (usado por bookmark_dialog.c, que también deja elegir color);
 * highlight_set_verse_note() es un wrapper fino sobre el primero que
 * preserva el color existente, para notas_verso.c (que nunca elige
 * color). */
void note_set_whole_verse(const gchar *module, const gchar *osisref,
			  const gchar *note, const gchar *color);
void note_remove_whole_verse(const gchar *module, const gchar *osisref);
char *note_get_whole_verse_color(const gchar *module, const gchar *osisref);
void highlight_set_verse_note(const gchar *module, const gchar *osisref,
			      const gchar *note);
char *highlight_get_verse_note(const gchar *module, const gchar *osisref);

/* Identidad estable de una nota, para poder enlazarla con otras. */
char *highlight_note_key_group(const gchar *group_id);
char *highlight_note_key_verse(const gchar *osisref);
/* "Book.C.V" al que corresponde una note_key, sea "HL:" o "MV:". */
char *highlight_note_key_osisref(const gchar *note_key);

/* Enlaces (no dirigidos) entre notas de distintos versículos. */
void highlight_link_notes(const gchar *key_a, const gchar *key_b);
void highlight_unlink_notes(const gchar *key_a, const gchar *key_b);
/* GList de gchar* note_key enlazadas a `key` (llamador libera con
 * g_list_free_full(..., g_free)). */
GList *highlight_list_linked_notes(const gchar *key);

#ifdef __cplusplus
}
#endif
