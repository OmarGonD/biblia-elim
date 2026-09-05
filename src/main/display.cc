/*
 * Xiphos Bible Study Tool
 * display.cc -
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

#include <glib.h>
#include <treekeyidx.h>

#include <osisxhtml.h>
#include <thmlxhtml.h>
#include <gbfxhtml.h>
#include <teixhtml.h>

#include <osisvariants.h>
#include <thmlvariants.h>
#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>

#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "xiphos_html/xiphos_html.h"

#include "main/display.hh"
#include "main/settings.h"
#include "main/global_ops.hh"
#include "main/sword.h"
#include "main/interlineal.h"
#include "main/modulecache.hh"
#include "main/xml.h"

#include "gui/utilities.h"
#include "gui/bookmarks_treeview.h"
#include "gui/widgets.h"
#include "gui/dialog.h"

#include "backend/sword_main.hh"

#include "gui/debug_glib_null.h"

#include <glib/gstdio.h>

// for one-time content rendering.
extern ModuleCache::CacheMap ModuleMap;

// Unified per-verse note/highlight entry -- replaces the old separate
// marked_element ("Mark Verse" whole-verse annotations) and
// highlight_element (phrase-level Kindle-style highlights) structs; both
// now live in one "osisrefnotes" XML section and one cache. text empty =
// whole-verse scope (at most one per verse); text set = phrase scope
// (several allowed per verse, distinguished by the "#<gid>" label suffix).
typedef struct
{
	gchar *label;   // full XML label: "<mod> <osisref>" or "<mod> <osisref>#<gid>"
	gchar *osisref; // "Book.C.V"
	gchar *text;    // highlighted phrase; empty for whole-verse entries
	gchar *color;   // "#RRGGBB", or NULL for the global default
	gchar *note;    // may be empty, never NULL
	gint pos;       // UTF-8 offset of `text` relative to verse start, in the
			// rendered HTML; -1 if unknown (whole-verse entries, or
			// phrase entries migrated from before this existed).
} NoteElement;

typedef std::map<int, GList *> NoteCache; // chapter_verse -> GList<NoteElement*>
static NoteCache note_cache;
static gchar *note_cache_modname = NULL, *note_cache_book = NULL;

int footnote, xref;

// flag for having discovered (in)valid bible keys.
// used for e.g. navigating from a bible w/apocrypha
// to a tab with a non-apocrypha bible.
gboolean valid_scripture_key = TRUE;
const gchar *no_content =
    N_("<br/><br/><center><i>This module has no content at this point.</i></center>");

#define HTML_START \
	"<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" /> \
<style type=\"text/css\"><!-- \
A { text-decoration:none } \
*[dir=rtl] { text-align: right; } \
@keyframes xiphos-fade-in { from { opacity: 0; } to { opacity: 1; } } \
body { background-color:%s; color:%s; -webkit-column-count: %d ; margin-top: 0.1cm ; animation: xiphos-fade-in 120ms ease-out; %s } \
td { %s } \
.introMaterial { font-style: italic; } \
a:link{ color:%s } %s %s \
.tagcolor a:link{ color:inherit !important } \
h3 { font-style: %s } --> \
%s</style> %s </head><body>"
// 11 interpolable values: bg/txt/link colors, columns, justify (2x, body+td),
// block, renderHeader, italic heading, mod.columns, external css.

#define	JUSTIFY_SELECT	(settings.justify_margins ? " text-align: justify ;" : "")
#define	ITALIC_SELECT	(ops->italic_headings ? "italic" : "bold" )

// CSS style blocks to control blocked strongs+morph output
// BOTH is when the user wants to see both types of markup.
// ONE is when he wants one or the other, so we can use a single
// specification which overlays both on top of one another.
#define CSS_BLOCK_BOTH                                                                            \
	" *        { line-height: 3.5em; }"                                                       \
	" .word    { position: relative; top:  0.0em; left: 0; }"                                 \
	" .strongs { position: absolute; top:  0.4em; left: 0; white-space: nowrap; z-index: 2 }" \
	" .morph   { position: absolute; top:  0.9em; left: 0; white-space: nowrap; z-index: 1 }" \
	" *[dir=rtl] .word { padding-left: 0.8em; }"                               \
	" *[dir=rtl] .strongs { top: 0.4em; left: auto; right: 0; }"                             \
	" *[dir=rtl] .morph { left: auto; right: 0; }"
#define CSS_BLOCK_ONE                                                                  \
	" *        { line-height: 2.7em; }"                                            \
	" .word    { position: relative; top:  0.0em; left: 0; }"                      \
	" .strongs { position: absolute; top:  0.4em; left: 0; white-space: nowrap; }" \
	" .morph   { position: absolute; top:  0.4em; left: 0; white-space: nowrap; }" \
	" *[dir=rtl] .word { padding-left: 0.8em; }"                               \
	" *[dir=rtl] .strongs { left: auto; right: 0; }"                               \
	" *[dir=rtl] .morph { left: auto; right: 0; }"

#define DOUBLE_SPACE " * { line-height: 2em ! important; }"

#define MAX_COLUMNS	8
#define CURRENT_COLUMNS	(((mf->columns_value >= 1) && (mf->columns_value <= MAX_COLUMNS)) ? mf->columns_value : settings.display_columns)

using namespace sword;
using namespace std;


//
// user annotation cache filling.
//

#define NUM_REPLACE 6

struct replace
{
	gchar c;
	gchar *s;
} replacement[NUM_REPLACE] = {
      // & must be first to avoid double-encoding
      {'&', (gchar *)"&amp;"},
      {'<', (gchar *)"&lt;"},
      {'>', (gchar *)"&gt;"},
      {'\n', (gchar *)"<br />"},
      {'"', (gchar *)"&quot;"},
      {'\'', (gchar *)"&apos;"},
};

// a macro to substitute the visually ugly presentation below.
// this puts all the decoration possible around a verse number.
#define PRETTYPRINT(v)                                             \
	(settings.verse_num_superscript ? superscript_start : ""), \
	    (settings.verse_num_bracket ? "[" : ""),               \
	    (settings.verse_num_bold ? bold_start : ""),           \
	    (v),                                                   \
	    (settings.verse_num_bold ? bold_end : ""),             \
	    (settings.verse_num_bracket ? "]" : ""),               \
	    (settings.verse_num_superscript ? superscript_end : "")

const char *bold_start = "<b>",
	   *bold_end = "</b>",
	   *superscript_start = "<sup>",
	   *superscript_end = "</sup>";

static void
append_verse_tools(SWBuf &swbuf, const char *key, gboolean has_note)
{
	gchar *esc = g_markup_escape_text(key ? key : "", -1);
	swbuf.appendFormatted("<span class=\"vtools\" data-key=\"%s\"%s> </span>",
			      esc, has_note ? " data-has-note=\"1\"" : "");
	g_free(esc);
}

enum { COLOR_NONE, COLOR_TEXT, COLOR_BOTH } color_choices;
gchar    *color_chosen_fg, *color_chosen_bg;

#define DEFAULT_HIGHLIGHT_COLOR "#FFEB3B" /* Kindle-style default yellow */
#define NOTES_MIGRATED_MARK "__migrated__"

static void
free_note_element(NoteElement *e)
{
	g_free(e->label);
	g_free(e->osisref);
	g_free(e->text);
	g_free(e->color);
	g_free(e->note);
	g_free(e);
}

static void
free_note_cache(void)
{
	NoteCache::iterator it;
	for (it = note_cache.begin(); it != note_cache.end(); ++it) {
		for (GList *n = (*it).second; n; n = n->next)
			free_note_element((NoteElement *)n->data);
		g_list_free((*it).second);
	}
	note_cache.clear();
}

// "color|<uri-escaped text>|<uri-escaped note>|<pos or empty>" -- '|' is
// always safe as a delimiter since g_uri_escape_string() percent-encodes
// everything outside the RFC3986 unreserved set. text empty => whole-verse
// entry. pos < 0 => not stored (whole-verse entries, or phrase entries
// with no anchor yet).
static gchar *
encode_note_value(const gchar *color, const gchar *text, const gchar *note, gint pos)
{
	gchar *etext = g_uri_escape_string(text ? text : "", NULL, TRUE);
	gchar *enote = g_uri_escape_string(note ? note : "", NULL, TRUE);
	gchar *value = (pos >= 0)
			   ? g_strdup_printf("%s|%s|%s|%d", color ? color : "", etext, enote, pos)
			   : g_strdup_printf("%s|%s|%s|", color ? color : "", etext, enote);
	g_free(etext);
	g_free(enote);
	return value;
}

static gboolean
decode_note_value(const gchar *value, gchar **color, gchar **text, gchar **note, gint *pos)
{
	gchar **parts = g_strsplit(value, "|", 4);
	if (!parts[0] || !parts[1] || !parts[2]) {
		g_strfreev(parts);
		return FALSE;
	}
	*color = (*parts[0]) ? g_strdup(parts[0]) : NULL;
	*text = g_uri_unescape_string(parts[1], NULL);
	*note = g_uri_unescape_string(parts[2], NULL);
	*pos = (parts[3] && *parts[3]) ? atoi(parts[3]) : -1;
	g_strfreev(parts);
	return TRUE;
}

// Same HTML-escaping markedCacheFill() used to apply to whole-verse
// annotation text before it could reach any HTML context downstream
// (note popups etc.) -- ported as-is, still scoped to whole-verse notes
// only, since phrase-highlight notes never went through this.
static void
html_escape_note(gchar **note)
{
	GString *g = g_string_new(*note);
	for (int i = 0; i < NUM_REPLACE; ++i) {
		for (gchar *s = strchr(g->str, replacement[i].c);
		     s;
		     s = strchr(s + 1, replacement[i].c)) {
			(void)g_string_erase(g, s - g->str, 1);
			(void)g_string_insert(g, s - g->str, replacement[i].s);
		}
	}
	g_free(*note);
	*note = g_string_free(g, FALSE);
}

// One-time, non-destructive import of the old osisrefhighlights /
// osisrefmarkedverses / osisrefmarkedcolors sections into osisrefnotes.
// The legacy sections are left untouched (never deleted) -- if anything
// about the new format turns out wrong, the original data is still there
// to recover from. A sentinel label marks "already migrated" (an
// osisrefnotes section that exists but happens to be empty is otherwise
// indistinguishable from "never migrated", since xml_set_section_ptr()
// only reports sections with at least one child).
static void
migrate_legacy_notes_if_needed(void)
{
	static gboolean checked = FALSE;

	if (checked)
		return;
	checked = TRUE;

	if (xml_set_section_ptr("osisrefnotes"))
		return; // sentinel or real data already present

	// phrase highlights: "color|text|note" (3 fields) -> add empty pos.
	if (xml_set_section_ptr("osisrefhighlights") && xml_get_label()) {
		do {
			gchar *label = xml_get_label();
			gchar *value = xml_get_list();
			if (label && value) {
				gchar **parts = g_strsplit(value, "|", 3);
				if (parts[0] && parts[1] && parts[2]) {
					gchar *newval = g_strdup_printf("%s|%s|%s|",
									parts[0], parts[1], parts[2]);
					xml_set_list_item("osisrefnotes", "note", label, newval);
					g_free(newval);
				}
				g_strfreev(parts);
			}
			g_free(label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}

	// whole-verse marks: value was the raw note text (or the "user
	// content" placeholder for "marked, no note"); color lived in a
	// separate osisrefmarkedcolors section under the same label.
	if (xml_set_section_ptr("osisrefmarkedverses") && xml_get_label()) {
		do {
			gchar *label = xml_get_label();
			gchar *value = xml_get_list();
			if (label && value) {
				const gchar *note_text = strcmp(value, "user content") ? value : "";
				gchar *color = xml_get_list_from_label("osisrefmarkedcolors",
								       "markedcolor", label);
				gchar *newval = encode_note_value(color, "", note_text, -1);
				xml_set_list_item("osisrefnotes", "note", label, newval);
				g_free(newval);
				g_free(color);
			}
			g_free(label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}

	xml_set_list_item("osisrefnotes", "note", NOTES_MIGRATED_MARK, "1");
	xml_save_settings_doc(settings.fnconfigure);
}

// "<mod> <osisref>" or "<mod> <osisref>#<gid>" -> "Book.C.V", for either
// label shape.
static gchar *
osis_from_note_label(const gchar *label)
{
	const gchar *space, *hash;
	if (!label)
		return NULL;
	space = strchr(label, ' ');
	if (!space)
		return NULL;
	hash = strrchr(label, '#');
	if (hash && hash > space + 1)
		return g_strndup(space + 1, (gsize)(hash - space - 1));
	return g_strdup(space + 1);
}

void
notesCacheFill(const gchar *modname, gchar *key)
{
	migrate_legacy_notes_if_needed();
	free_note_cache();

	char *key_book = g_strdup(main_get_osisref_from_key((const char *)modname,
							    (const char *)key));
	gchar *s, *t;
	*(s = strrchr(key_book, '.')) = '\0';
	*(t = strrchr(key_book, '.')) = '\0';

	g_free(note_cache_modname);
	g_free(note_cache_book);
	note_cache_modname = g_strdup(modname);
	note_cache_book = g_strdup(key_book);

	if (xml_set_section_ptr("osisrefnotes") && xml_get_label()) {
		do {
			gchar *full_label = xml_get_label(); // "<mod> <osisref>[#<gid>]"
			gchar *value = xml_get_list();

			if (full_label && value && strcmp(full_label, NOTES_MIGRATED_MARK)) {
				gchar *space = strchr(full_label, ' ');
				gchar *hash = strrchr(full_label, '#');
				if (space && (!hash || hash > space + 1)) {
					gsize oref_len = (hash && hash > space + 1)
							     ? (gsize)(hash - space - 1)
							     : strlen(space + 1);
					gchar *mod = g_strndup(full_label, space - full_label);
					gchar *osisref = g_strndup(space + 1, oref_len);
					gchar *bhold = g_strdup(osisref);
					gchar *dot1 = strrchr(bhold, '.');

					if (dot1) {
						*dot1 = '\0';
						gchar *dot2 = strrchr(bhold, '.');
						if (dot2) {
							int chapter_verse =
							    (1000 * atoi(dot2 + 1)) + atoi(dot1 + 1);
							*dot2 = '\0'; // bhold now just the book

							if (!((*mod && strcasecmp(mod, modname) != 0) ||
							      strcasecmp(bhold, key_book) != 0)) {
								gchar *color = NULL, *text = NULL, *note = NULL;
								gint pos = -1;
								if (decode_note_value(value, &color, &text, &note, &pos)) {
									if (!*text)
										html_escape_note(&note);
									NoteElement *ne = g_new0(NoteElement, 1);
									ne->label = g_strdup(full_label);
									ne->osisref = g_strdup(osisref);
									ne->text = text;
									ne->color = color;
									ne->note = note;
									ne->pos = pos;
									note_cache[chapter_verse] =
									    g_list_append(note_cache[chapter_verse], ne);
								}
							}
						}
					}
					g_free(bhold);
					g_free(mod);
					g_free(osisref);
				}
			}
			g_free(full_label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}
	g_free(key_book);
}

// The (at most one) entry for this verse whose text is empty --
// whole-verse scope, replaces the old markedCacheCheck().
static NoteElement *
whole_verse_note(int chapter_verse)
{
	NoteCache::iterator it = note_cache.find(chapter_verse);
	if (it == note_cache.end())
		return NULL;
	for (GList *n = (*it).second; n; n = n->next) {
		NoteElement *e = (NoteElement *)n->data;
		if (!e->text || !*e->text)
			return e;
	}
	return NULL;
}

// Highlights are identified by a group id shared across every verse a
// single selection touches (see highlight_create_group() below) --
// "<mod> <osisref>#<group_id>". A selection that only touches one verse
// is simply a group of one. Finding every XML entry for a group means
// walking the whole section, same as highlightCacheFill() already does,
// collecting every label whose "#<group_id>" suffix matches.
static GList *
find_labels_by_group(const gchar *group_id)
{
	GList *labels = NULL;
	gchar *suffix;

	if (!group_id || !*group_id)
		return NULL;
	suffix = g_strdup_printf("#%s", group_id);

	if (xml_set_section_ptr("osisrefnotes") && xml_get_label()) {
		do {
			gchar *label = xml_get_label();
			if (label && g_str_has_suffix(label, suffix))
				labels = g_list_append(labels, g_strdup(label));
			g_free(label);
		} while (xml_next_item() && xml_get_label());
	}
	g_free(suffix);
	return labels;
}

// A word/sentence/verse should only ever carry one highlight color: if
// the new selection is the same as, contained within, or contains an
// already-highlighted phrase in this verse, return that highlight's
// *group id* so the caller edits the whole thing in place (every verse
// it spans) instead of stacking a duplicate, differently-colored
// highlight on top of it.
extern "C" char *
highlight_find_overlapping(const gchar *module, const gchar *osisref, const gchar *text)
{
	(void)module; // the cache is already scoped to the current module/book.

	gchar *copy = g_strdup(osisref); // "Book.C.V"
	gchar *dot1 = strrchr(copy, '.');
	if (!dot1) {
		g_free(copy);
		return NULL;
	}
	*dot1 = '\0';
	gchar *dot2 = strrchr(copy, '.');
	if (!dot2) {
		g_free(copy);
		return NULL;
	}
	int chapter_verse = (1000 * atoi(dot2 + 1)) + atoi(dot1 + 1);
	g_free(copy);

	NoteCache::iterator it = note_cache.find(chapter_verse);
	if (it == note_cache.end())
		return NULL;

	for (GList *n = (*it).second; n; n = n->next) {
		NoteElement *h = (NoteElement *)n->data;
		if (!h->text || !*h->text)
			continue; // whole-verse entry, not a phrase highlight
		if (strstr(h->text, text) || strstr(text, h->text)) {
			gchar *hash = strrchr(h->label, '#');
			return hash ? g_strdup(hash + 1) : g_strdup(h->label);
		}
	}
	return NULL;
}

static void
highlight_persist(gboolean rerender)
{
	xml_save_settings_doc(settings.fnconfigure);
	notesCacheFill(settings.MainWindowModule, settings.currentverse);
	if (rerender)
		main_display_bible(NULL, settings.currentverse);
}

// segments: GList of HighlightSegment*, one per verse the selection
// touches (almost always just one). All share one freshly-minted group
// id, so a later color change, note, or delete affects every verse the
// original selection spanned, together.
extern "C" char *
highlight_create_group(const gchar *module, GList *segments, const gchar *color)
{
	gchar *group_id = g_strdup_printf("%" G_GINT64_FORMAT, g_get_monotonic_time());
	const gchar *c = (color && *color) ? color : DEFAULT_HIGHLIGHT_COLOR;

	for (GList *n = segments; n; n = n->next) {
		HighlightSegment *seg = (HighlightSegment *)n->data;
		gchar *label = g_strdup_printf("%s %s#%s", module, seg->osisref, group_id);
		gchar *value = encode_note_value(c, seg->text, "", seg->pos);
		xml_set_list_item("osisrefnotes", "note", label, value);
		g_free(value);
		g_free(label);
	}

	highlight_persist(FALSE);
	return group_id; // caller owns
}

extern "C" void
highlight_set_color(const gchar *group_id, const gchar *color)
{
	GList *labels = find_labels_by_group(group_id);
	for (GList *n = labels; n; n = n->next) {
		gchar *label = (gchar *)n->data;
		gchar *value = xml_get_list_from_label("osisrefnotes", "note", label);
		if (value) {
			gchar *old_color = NULL, *text = NULL, *note = NULL;
			gint pos = -1;
			if (decode_note_value(value, &old_color, &text, &note, &pos)) {
				gchar *newval = encode_note_value(color, text, note, pos);
				xml_set_list_item("osisrefnotes", "note", label, newval);
				g_free(newval);
				g_free(old_color);
				g_free(text);
				g_free(note);
			}
			g_free(value);
		}
	}
	g_list_free_full(labels, g_free);
	highlight_persist(FALSE);
}

extern "C" void
highlight_set_note(const gchar *group_id, const gchar *note)
{
	GList *labels = find_labels_by_group(group_id);
	for (GList *n = labels; n; n = n->next) {
		gchar *label = (gchar *)n->data;
		gchar *value = xml_get_list_from_label("osisrefnotes", "note", label);
		if (value) {
			gchar *color = NULL, *text = NULL, *old_note = NULL;
			gint pos = -1;
			if (decode_note_value(value, &color, &text, &old_note, &pos)) {
				gchar *newval = encode_note_value(color, text, note, pos);
				xml_set_list_item("osisrefnotes", "note", label, newval);
				g_free(newval);
				g_free(color);
				g_free(text);
				g_free(old_note);
			}
			g_free(value);
		}
	}
	g_list_free_full(labels, g_free);
	highlight_persist(FALSE);
}

extern "C" void
highlight_remove(const gchar *group_id)
{
	GList *labels = find_labels_by_group(group_id);
	for (GList *n = labels; n; n = n->next)
		xml_remove_node("osisrefnotes", "note", (gchar *)n->data);
	g_list_free_full(labels, g_free);
	highlight_persist(FALSE);
}

// the color a highlight group is currently stored with, e.g. to paint
// the toolbar's "Subrayado" button as a circle in that same color.
// All verses in a group share one color, so the first entry found is
// authoritative.
extern "C" char *
highlight_get_color(const gchar *group_id)
{
	GList *labels = find_labels_by_group(group_id);
	gchar *result = NULL;
	if (labels) {
		gchar *value = xml_get_list_from_label("osisrefnotes", "note",
						       (gchar *)labels->data);
		if (value) {
			gchar *text = NULL, *note = NULL;
			gint pos = -1;
			if (decode_note_value(value, &result, &text, &note, &pos)) {
				g_free(text);
				g_free(note);
			}
			g_free(value);
		}
	}
	g_list_free_full(labels, g_free);
	return result;
}

extern "C" char *
highlight_get_note(const gchar *group_id)
{
	GList *labels = find_labels_by_group(group_id);
	gchar *result = NULL;
	if (labels) {
		gchar *value = xml_get_list_from_label("osisrefnotes", "note",
						       (gchar *)labels->data);
		if (value) {
			gchar *color = NULL, *text = NULL;
			gint pos = -1;
			if (decode_note_value(value, &color, &text, &result, &pos)) {
				g_free(color);
				g_free(text);
			}
			g_free(value);
		}
	}
	g_list_free_full(labels, g_free);
	if (result && !*result) {
		g_free(result);
		result = NULL;
	}
	return result;
}

static gboolean
osis_matches_prefix(const gchar *osis, const gchar *prefix)
{
	gsize n;
	gchar next;
	if (!prefix || !*prefix)
		return TRUE;
	if (!osis)
		return FALSE;
	n = strlen(prefix);
	if (strncmp(osis, prefix, n) != 0)
		return FALSE;
	next = osis[n];
	return next == '\0' || next == '.';
}

extern "C" void
highlight_note_free(HighlightNote *n)
{
	if (!n)
		return;
	g_free(n->group_id);
	g_free(n->module);
	g_free(n->osisref);
	g_free(n->text);
	g_free(n->note);
	g_free(n->color);
	g_free(n->note_key);
	g_free(n);
}

extern "C" char *
highlight_note_key_group(const gchar *group_id)
{
	return group_id ? g_strdup_printf("HL:%s", group_id) : NULL;
}

extern "C" char *
highlight_note_key_verse(const gchar *osisref)
{
	return osisref ? g_strdup_printf("MV:%s", osisref) : NULL;
}

extern "C" char *
highlight_note_key_osisref(const gchar *note_key)
{
	if (!note_key)
		return NULL;
	if (g_str_has_prefix(note_key, "MV:"))
		return g_strdup(note_key + 3);
	if (g_str_has_prefix(note_key, "HL:")) {
		GList *labels = find_labels_by_group(note_key + 3);
		gchar *osis = NULL;
		if (labels)
			osis = osis_from_note_label((const gchar *)labels->data);
		g_list_free_full(labels, g_free);
		return osis;
	}
	return NULL;
}

// Whole-verse note+color, shared by notas_verso.c's simple note panel and
// bookmark_dialog.c's "Mark Verse" dialog (which also lets the user pick
// a color -- notas_verso.c never does, it always passes color=NULL and
// note_set_whole_verse() keeps the earlier one alone in that case, same
// as the two features never touching each other's data used to work).
extern "C" void
note_set_whole_verse(const gchar *module, const gchar *osisref,
		     const gchar *note, const gchar *color)
{
	gchar *label = g_strdup_printf("%s %s", module, osisref);
	gchar *value = encode_note_value(color, "", note, -1);
	xml_set_list_item("osisrefnotes", "note", label, value);
	g_free(value);
	g_free(label);
	xml_save_settings_doc(settings.fnconfigure);
	notesCacheFill(settings.MainWindowModule, settings.currentverse);
}

extern "C" void
note_remove_whole_verse(const gchar *module, const gchar *osisref)
{
	gchar *label = g_strdup_printf("%s %s", module, osisref);
	xml_remove_node("osisrefnotes", "note", label);
	g_free(label);
	xml_save_settings_doc(settings.fnconfigure);
	notesCacheFill(settings.MainWindowModule, settings.currentverse);
}

extern "C" char *
note_get_whole_verse_color(const gchar *module, const gchar *osisref)
{
	gchar *label = g_strdup_printf("%s %s", module, osisref);
	gchar *value = xml_get_list_from_label("osisrefnotes", "note", label);
	gchar *result = NULL;
	g_free(label);
	if (value) {
		gchar *text = NULL, *note = NULL;
		gint pos = -1;
		decode_note_value(value, &result, &text, &note, &pos);
		g_free(text);
		g_free(note);
		g_free(value);
	}
	return result;
}

extern "C" void
highlight_set_verse_note(const gchar *module, const gchar *osisref, const gchar *note)
{
	gchar *existing_color = note_get_whole_verse_color(module, osisref);
	note_set_whole_verse(module, osisref, note, existing_color);
	g_free(existing_color);
}

extern "C" char *
highlight_get_verse_note(const gchar *module, const gchar *osisref)
{
	gchar *label = g_strdup_printf("%s %s", module, osisref);
	gchar *value = xml_get_list_from_label("osisrefnotes", "note", label);
	gchar *result = NULL;
	g_free(label);
	if (value) {
		gchar *color = NULL, *text = NULL;
		gint pos = -1;
		decode_note_value(value, &color, &text, &result, &pos);
		g_free(color);
		g_free(text);
		g_free(value);
	}
	if (result && !*result) {
		g_free(result);
		result = NULL;
	}
	return result;
}

// Undirected links between two notes' stable identities ("HL:<gid>" or
// "MV:<osisref>"), stored as "<keyA>|<keyB>" pairs under their own XML
// section, same linear-scan approach as find_labels_by_group() above --
// link volume per user is tiny, so this stays simple.
extern "C" void
highlight_link_notes(const gchar *key_a, const gchar *key_b)
{
	GList *existing;

	if (!key_a || !key_b || !*key_a || !*key_b || !strcmp(key_a, key_b))
		return;

	existing = highlight_list_linked_notes(key_a);
	for (GList *n = existing; n; n = n->next) {
		if (!strcmp((const gchar *)n->data, key_b)) {
			g_list_free_full(existing, g_free);
			return;
		}
	}
	g_list_free_full(existing, g_free);

	gchar *id = g_strdup_printf("%" G_GINT64_FORMAT, g_get_monotonic_time());
	gchar *value = g_strdup_printf("%s|%s", key_a, key_b);
	xml_set_list_item("osisrefnotelinks", "notelink", id, value);
	g_free(value);
	g_free(id);
	xml_save_settings_doc(settings.fnconfigure);
}

extern "C" void
highlight_unlink_notes(const gchar *key_a, const gchar *key_b)
{
	GList *to_remove = NULL;

	if (!key_a || !key_b)
		return;

	if (xml_set_section_ptr("osisrefnotelinks") && xml_get_label()) {
		do {
			gchar *label = xml_get_label();
			gchar *value = xml_get_list();
			if (value) {
				gchar **parts = g_strsplit(value, "|", 2);
				if (parts[0] && parts[1] &&
				    ((!strcmp(parts[0], key_a) && !strcmp(parts[1], key_b)) ||
				     (!strcmp(parts[0], key_b) && !strcmp(parts[1], key_a))))
					to_remove = g_list_append(to_remove, g_strdup(label));
				g_strfreev(parts);
			}
			g_free(label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}

	for (GList *n = to_remove; n; n = n->next)
		xml_remove_node("osisrefnotelinks", "notelink", (gchar *)n->data);
	if (to_remove)
		xml_save_settings_doc(settings.fnconfigure);
	g_list_free_full(to_remove, g_free);
}

extern "C" GList *
highlight_list_linked_notes(const gchar *key)
{
	GList *out = NULL;

	if (!key)
		return NULL;

	if (xml_set_section_ptr("osisrefnotelinks") && xml_get_label()) {
		do {
			gchar *label = xml_get_label();
			gchar *value = xml_get_list();
			if (value) {
				gchar **parts = g_strsplit(value, "|", 2);
				if (parts[0] && parts[1]) {
					if (!strcmp(parts[0], key))
						out = g_list_append(out, g_strdup(parts[1]));
					else if (!strcmp(parts[1], key))
						out = g_list_append(out, g_strdup(parts[0]));
				}
				g_strfreev(parts);
			}
			g_free(label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}
	return out;
}

extern "C" int
highlight_count_notes_at(int chapter_verse)
{
	int n = 0;
	NoteCache::iterator it = note_cache.find(chapter_verse);
	if (it != note_cache.end()) {
		for (GList *l = (*it).second; l; l = l->next) {
			NoteElement *h = (NoteElement *)l->data;
			if (h->note && *h->note)
				n++;
		}
	}
	return n;
}

extern "C" GList *
highlight_list_notes(const gchar *osis_prefix)
{
	GList *out = NULL;
	GHashTable *seen = g_hash_table_new(g_str_hash, g_str_equal);
	NoteCache::iterator it;

	for (it = note_cache.begin(); it != note_cache.end(); ++it) {
		for (GList *l = (*it).second; l; l = l->next) {
			NoteElement *h = (NoteElement *)l->data;
			gboolean whole_verse = !h->text || !*h->text;
			gchar *hash, *gid = NULL;
			HighlightNote *n;

			if (!h->note || !*h->note)
				continue;
			if (!whole_verse) {
				hash = strrchr(h->label, '#');
				gid = (hash && hash[1]) ? hash + 1 : h->label;
				if (g_hash_table_contains(seen, gid))
					continue;
				g_hash_table_add(seen, gid);
			}
			if (!osis_matches_prefix(h->osisref, osis_prefix))
				continue;

			n = g_new0(HighlightNote, 1);
			n->group_id = whole_verse ? NULL : g_strdup(gid);
			{
				gchar *space = strchr(h->label, ' ');

				n->module = space
					? g_strndup(h->label, space - h->label)
					: g_strdup(note_cache_modname);
			}
			n->osisref = g_strdup(h->osisref);
			n->text = whole_verse ? NULL : g_strdup(h->text);
			n->note = g_strdup(h->note);
			n->color = h->color ? g_strdup(h->color) : NULL;
			n->note_key = whole_verse
					  ? highlight_note_key_verse(h->osisref)
					  : highlight_note_key_group(gid);
			n->chapter_verse = (*it).first;
			out = g_list_append(out, n);
		}
	}

	g_hash_table_destroy(seen);
	return out;
}

// Canonical position of an osisref, so notes from different books sort
// the way a Bible reads instead of the way the XML happens to be
// ordered. VerseKey::getIndex() restarts at each testament, so the
// testament goes in the high bits.
static guint32
osis_orden(const gchar *osisref)
{
	VerseKey vk;

	vk.setAutoNormalize(1);
	vk.setText(osisref);
	if (vk.popError())
		return G_MAXUINT32; // lo que no se entiende, al final
	return ((guint32)vk.getTestament() << 24) | (guint32)vk.getIndex();
}

static gint
por_orden(gconstpointer a, gconstpointer b)
{
	guint32 oa = ((const HighlightNote *)a)->orden;
	guint32 ob = ((const HighlightNote *)b)->orden;

	return (oa < ob) ? -1 : (oa > ob) ? 1 : 0;
}

// Every note ever written, from the XML itself: the cache only holds the
// open book, and a search has to see all of them. One group of
// highlighted phrases spans one entry per verse it touched, and each of
// those carries the same note -- so they are folded into one result, the
// same way highlight_list_notes() does it.
extern "C" GList *
highlight_all_notes(void)
{
	GList *out = NULL;
	GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal,
						 g_free, NULL);

	migrate_legacy_notes_if_needed();

	if (xml_set_section_ptr("osisrefnotes") && xml_get_label()) {
		do {
			gchar *full_label = xml_get_label();
			gchar *value = xml_get_list();
			gchar *space = full_label ? strchr(full_label, ' ') : NULL;
			gchar *hash = full_label ? strrchr(full_label, '#') : NULL;

			if (full_label && value &&
			    strcmp(full_label, NOTES_MIGRATED_MARK) && space &&
			    (!hash || hash > space + 1)) {
				gsize oref_len = (hash && hash > space + 1)
						     ? (gsize)(hash - space - 1)
						     : strlen(space + 1);
				gchar *mod = g_strndup(full_label,
						       space - full_label);
				gchar *osisref = g_strndup(space + 1, oref_len);
				gchar *color = NULL, *text = NULL, *note = NULL;
				gint pos = -1;

				if (decode_note_value(value, &color, &text,
						      &note, &pos) &&
				    note && *note) {
					gboolean whole = !text || !*text;
					const gchar *gid = (hash && hash[1])
							       ? hash + 1
							       : full_label;

					// del grupo, un solo resultado
					if (whole || !g_hash_table_contains(seen, gid)) {
						HighlightNote *n =
						    g_new0(HighlightNote, 1);

						if (!whole)
							g_hash_table_add(seen, g_strdup(gid));
						n->group_id = whole ? NULL : g_strdup(gid);
						n->module = g_strdup(mod);
						n->osisref = g_strdup(osisref);
						n->text = whole ? NULL : g_strdup(text);
						n->note = g_strdup(note);
						n->color = color ? g_strdup(color) : NULL;
						n->note_key =
						    whole ? highlight_note_key_verse(osisref)
							  : highlight_note_key_group(gid);
						n->orden = osis_orden(osisref);
						out = g_list_prepend(out, n);
					}
				}
				g_free(color);
				g_free(text);
				g_free(note);
				g_free(mod);
				g_free(osisref);
			}
			g_free(full_label);
			g_free(value);
		} while (xml_next_item() && xml_get_label());
	}

	g_hash_table_destroy(seen);
	return g_list_sort(g_list_reverse(out), por_orden);
}

extern "C" int
highlight_count_notes(const gchar *osis_prefix)
{
	GList *list = highlight_list_notes(osis_prefix);
	int n = (int)g_list_length(list);
	g_list_free_full(list, (GDestroyNotify)highlight_note_free);
	return n;
}

static void
append_verse_note_marker(SWBuf &swbuf, int chapter_verse,
			 const char *module, const char *passage)
{
	int n = highlight_count_notes_at(chapter_verse);
	gchar *lab;
	if (n <= 0)
		return;
	lab = (n == 1) ? g_strdup("n") : g_strdup_printf("n%d", n);
	swbuf.appendFormatted("<sup class=\"hl-note-count\">"
			      "<a href=\"passagestudy.jsp?action=showHlNotes&"
			      "module=%s&passage=%s\">%s</a>"
			      "</sup>&nbsp;",
			      module, passage, lab);
	g_free(lab);
}

// Byte offset in `rework_str` of the `n`th plain (non-"<tag>") UTF-8
// character, skipping tags transparently -- translates the verse-relative
// plain-text character offset saved at highlight-creation time (measured
// against the plain GtkTextBuffer, no tags) into a position in this
// freshly re-rendered HTML string. -1 if the string runs out first.
static gssize
plain_char_offset(const gchar *rework_str, gint n)
{
	const gchar *s = rework_str;
	gint count = 0;
	while (*s && count < n) {
		if (*s == '<') {
			while (*s && *s != '>')
				s++;
			if (*s == '>')
				s++;
			continue;
		}
		s = g_utf8_next_char(s);
		count++;
	}
	return (count == n) ? (gssize)(s - rework_str) : -1;
}

// From byte offset `start_offset`, checks whether the next plain
// (non-tag) characters equal `text`, skipping any "<tag>" transparently
// on both sides (the anchor was computed against plain buffer text, this
// walks the equivalent HTML string). On success returns TRUE and sets
// *end_offset to the byte offset right after the match, for the closing
// tag to go there.
static gboolean
verify_plain_match(const gchar *rework_str, gssize start_offset,
		   const gchar *text, gssize *end_offset)
{
	const gchar *s = rework_str + start_offset;
	const gchar *t = text;
	while (*t) {
		while (*s == '<') {
			while (*s && *s != '>')
				s++;
			if (*s == '>')
				s++;
		}
		if (!*s || g_utf8_get_char(s) != g_utf8_get_char(t))
			return FALSE;
		s = g_utf8_next_char(s);
		t = g_utf8_next_char(t);
	}
	*end_offset = s - rework_str;
	return TRUE;
}

// defined further below; forward-declared so apply_verse_notes() (which
// runs earlier in the per-verse render, before the whole-verse-annotation
// wrapping code that also calls this) can use it.
static const char *text_color_for_bg(const gchar *hex_color);

// Re-applies stored notes/highlights for this verse onto its
// already-rendered HTML, before it's wrapped by any tag-color
// colorization below. Replaces the old apply_selection_highlights()
// (phrase highlights only) + the markedCacheCheck()-based whole-verse
// block that used to live inline in the two callers below -- one unified
// cache, one pass. Sets color_choices/color_chosen_fg/color_chosen_bg
// (same globals the callers already used) for the whole-verse case; the
// callers still do the actual <span>/<font> wrapping around `rework`
// themselves, unchanged.
static void
apply_verse_notes(GString *rework, int chapter_verse)
{
	NoteCache::iterator it = note_cache.find(chapter_verse);
	NoteElement *verse_note = NULL;

	color_choices   = COLOR_NONE;
	color_chosen_fg = NULL;
	color_chosen_bg = NULL;

	if (it != note_cache.end()) {
		int n_hl_notes = 0, idx = 0;
		for (GList *n = (*it).second; n; n = n->next) {
			NoteElement *h = (NoteElement *)n->data;
			if (!h->text || !*h->text) {
				if (!verse_note)
					verse_note = h;
				continue;
			}
			if (h->note && *h->note)
				n_hl_notes++;
		}

		for (GList *n = (*it).second; n; n = n->next) {
			NoteElement *h = (NoteElement *)n->data;
			gchar *close_tag, *open_tag, *hash;
			const char *gid;
			gssize offset = -1, end_offset = 0;

			if (!h->text || !*h->text)
				continue; // whole-verse entry, handled above/below

			if (h->pos >= 0) {
				gssize cand = plain_char_offset(rework->str, h->pos);
				if (cand >= 0 && verify_plain_match(rework->str, cand, h->text, &end_offset))
					offset = cand;
			}
			if (offset < 0) {
				// no anchor, or content drifted since it was
				// saved -- same best-effort fallback as before.
				gchar *found = strstr(rework->str, h->text);
				if (!found)
					continue;
				offset = found - rework->str;
				end_offset = offset + strlen(h->text);
			}

			hash = strrchr(h->label, '#');
			gid = (hash && hash[1]) ? hash + 1 : "0";
			open_tag = g_strdup_printf(
			    "<span class=\"xiphos-hl\" data-hl-id=\"%s\" "
			    "style=\"background-color: %s; text-decoration: underline; cursor: pointer;\">",
			    gid, h->color ? h->color : DEFAULT_HIGHLIGHT_COLOR);

			if (h->note && *h->note) {
				idx++;
				if (n_hl_notes == 1)
					close_tag = g_strdup_printf(
					    "</span><sup class=\"hl-note\">"
					    "<a href=\"passagestudy.jsp?action=showHlNote&value=%s\">n</a></sup>",
					    gid);
				else
					close_tag = g_strdup_printf(
					    "</span><sup class=\"hl-note\">"
					    "<a href=\"passagestudy.jsp?action=showHlNote&value=%s\">n%d</a></sup>",
					    gid, idx);
			} else
				close_tag = g_strdup("</span>");

			g_string_insert(rework, end_offset, close_tag);
			g_string_insert(rework, offset, open_tag);
			g_free(open_tag);
			g_free(close_tag);
		}
	}

	if (settings.annotate_highlight && verse_note) {
		color_choices = COLOR_BOTH;
		if (verse_note->color) {
			// per-verse highlight color chosen when this verse was
			// marked: auto-contrast the text instead of requiring
			// the user to pick a fg/bg pair.
			color_chosen_bg = verse_note->color;
			color_chosen_fg = (gchar *)text_color_for_bg(verse_note->color);
		} else {
			color_chosen_fg = settings.highlight_fg;
			color_chosen_bg = settings.highlight_bg;
		}
	}
}

//
// utility function to blank `<img src="foo.jpg" />' content from text.
//
void
ClearImages(gchar *text)
{
	gchar *s;

	for (s = strstr(text, "<img "); s; s = strstr(s, "<img ")) {
		gchar *t;
		if ((t = strchr(s + 4, '>'))) {
			while (s <= t)
				*(s++) = ' ';
		} else {
			XI_message(("ClearImages: no img end: %s\n", s));
			return;
		}
	}
}

//
// utility function to blank font names `<font face="..." />' in text.
//

#define	CLEAR_FONT_NAME		"<font face=\""
#define	CLEAR_FONT_NAME_LENGTH	12

void
ClearFontFaces(gchar *text)
{
	gchar *s;

	// huge assumption: no nested <font> specs: <font face="">
	// is never followed by another <font anything> before </font>.
	for (s = strstr(text, CLEAR_FONT_NAME); s; s = strstr(s, CLEAR_FONT_NAME)) {
		gchar *t;
		s += CLEAR_FONT_NAME_LENGTH;
		if ((t = strchr(s, '"'))) {
			while (s < t)
				*(s++) = '.';
		} else {
			XI_message(("ClearFontFaces: no closing double quote: %s\n", s));
			return;
		}
	}
}

//
// retrieve the content of the module's personal css
//
static const char *stylefile =
    "style.css"; // default name, per module.
static const char *default_stylefile =
    "default-style.css"; // default name, overall.

const gchar *get_css_references(SWModule &module)
{
	static string css; // static -> safe to return it

	// assume nothing will be available.
	css = "";

	// non-specific CSS for all module displays.
	char *css_file = g_build_filename(settings.gSwordDir,
					  default_stylefile, NULL);

	if (g_file_test(css_file, G_FILE_TEST_EXISTS)) {
		css += (string) "<link rel=\"stylesheet\" type=\"text/css\" href=\""
#ifdef WIN32
		       + "http://127.0.0.1:7878/" // see main.c (sob)
#else
		       + "file:"
#endif
		       + css_file + "\" />";
	}
	g_free(css_file);

	// construct path to module's CSS.
	char *datapath = main_get_mod_config_entry(module.getName(), "AbsoluteDataPath");
	char *prefcss = main_get_mod_config_entry(module.getName(), "PreferredCSSXHTML");

	// module-specific CSS.
	css_file = g_build_filename(datapath, (prefcss ? prefcss : stylefile), NULL);

	if (g_file_test(css_file, G_FILE_TEST_EXISTS)) {
		css += (string) "<link rel=\"stylesheet\" type=\"text/css\" href=\""
#ifdef WIN32
		       + "http://127.0.0.1:7878/" // see main.c (sob)
#else
		       + "file:"
#endif
		       + css_file + "\" />";
	}
	g_free(css_file); // get rid of old one first.

	return css.c_str();
}

//
// utility function for block_render() below.
// having a word + annotation in hand, stuff them into the buffer.
// span class names are from CSS_BLOCK macros.
// we garbage-collect here so block_render doesn't have to.
//

#define ALIGN_WORD "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"

void
block_dump(SWBuf &rendered,
	   const char **word,
	   const char **strongs,
	   const char **morph)
{
	int wlen, min_length, slen, mlen;
	char *s, *s0, *t;

	// unannotated words need no help.
	if (*word && (*strongs == NULL) && (*morph == NULL)) {
		rendered += "<span class=\"word\">";
		rendered += *word;
		rendered += "</span>";
		g_free((char *)*word);
		*word = NULL;
		rendered += " ";
		return;
	}

	if (*word == NULL) {
		// we need to cobble up something to take the place of
		// a word, in order that the strongs/morph not overlay.
		*word = g_strdup(ALIGN_WORD);
	}

	rendered += "<span class=\"word\">";
	if (*strongs) {
		s = g_strrstr(*strongs, "</a>");
		*s = '\0';
		t = (char *)strrchr(*strongs, '>') + 1;
		// correct weird NASB lexicon references.
		if ((s0 = (char *)strchr(*strongs, '!'))) {
			do {
				*s0 = *(s0 + 1);
				++s0;
			} while (*s0 != '"');
			*s0 = ' ';
		}
		*s = '<';
		slen = s - t;
		s = (char *)strstr(*strongs, "&lt;");
		*s = *(s + 1) = *(s + 2) = *(s + 3) = ' ';
		s = strstr(s, "&gt;");
		*s = *(s + 1) = *(s + 2) = *(s + 3) = ' ';

		// gross hack needed to handle new class="..." in sword -r2512.
		if ((s = (char *)strstr(*strongs, " class=\"strongs\">"))) {
			memcpy(s, ">                ", 17);
			if ((s = (char *)strstr(s, " class=\"strongs\">")))
				memcpy(s, ">                ", 17);
		}
	} else
		slen = 0;

	if (*morph) {
		s = s0 = (char *)g_strrstr(*morph, "\">") + 2;
		t = strchr(s, '<');
		for (/* */; s < t; ++s)
			if (isupper(*s))
				*s = tolower(*s);
		for (s = strchr(s0, ' '); s && (s < t); s = strchr(s, ' '))
			*s = '-';
		s = g_strrstr(*morph, "</a>");
		*s = '\0';
		t = (char *)strrchr(*morph, '>') + 1;
		*s = '<';
		mlen = s - t;
		s = (char *)strchr(*morph, '(');
		*s = ' ';
		s = strrchr(s, ')');
		*s = ' ';

		// gross hack needed to handle new class="..." in sword -r2512.
		if ((s = (char *)strstr(*morph, " class=\"morph\">"))) {
			memcpy(s, ">              ", 15);
			if ((s = (char *)strstr(s, " class=\"morph\">")))
				memcpy(s, ">              ", 15);
		}
	} else
		mlen = 0;

	min_length = 2 + max(slen, mlen);

	rendered += *word;

	for (wlen = strlen(*word); wlen <= min_length; ++wlen)
		rendered += "&nbsp;";

	g_free((char *)*word);
	*word = NULL;

	rendered += "<span class=\"strongs\"><sup>";
	rendered += (*strongs ? *strongs : "&nbsp;");
	rendered += "</sup></span>";
	if (*strongs)
		g_free((char *)*strongs);
	*strongs = NULL;

/* truncate long Hebrew morph codes: remove pronominal suffix segment */
	if (*morph) {
		char *anchor_start = (char *)g_strrstr(*morph, "\">") + 2;
		char *anchor_end = (char *)strstr(anchor_start, "</a>");
		if (anchor_start && anchor_end) {
			/* count slashes */
			char *first_slash = strchr(anchor_start, '/');
			if (first_slash && first_slash < anchor_end) {
				char *second_slash = strchr(first_slash + 1, '/');
				if (second_slash && second_slash < anchor_end) {
					/* 2 segments: hr/ncmsc/sp2mp → truncate at second slash */
					memmove(second_slash, anchor_end,
						strlen(anchor_end) + 1);
				} else {
					/* 1 segment: check if suffix is pronominal (sp/sd) */
					char *seg = first_slash + 1;
					if ((seg[0] == 's' && (seg[1] == 'p' || seg[1] == 'd'))
					    || (seg[0] == 'S' && (seg[1] == 'p' || seg[1] == 'd'))) {
						memmove(first_slash, anchor_end,
							strlen(anchor_end) + 1);
					}
				}
			}
		}
	}

	rendered += "<span class=\"morph\"><sup>";
	rendered += (*morph ? *morph : "&nbsp;");
	rendered += "</sup></span>";
	if (*morph)
		g_free((char *)*morph);
	*morph = NULL;

	rendered += "</span> <span class=\"word\">&nbsp;</span>";
}

//
// re-process a block of text so as to envelope its strong's and morph
// references in <span> blocks which will be interpreted by the CSS
// directives to put each ref immediately below the word it annotates.
// this keeps the text linearly readable while providing annotation.
//
// secondary interface.
// text destination is provided, ready to go.
// this means we are able to recurse when needed.

#define EMPTY_WORD ""

void
block_render_secondary(const char *text,
		       SWBuf &rendered)
{
	const char *word = NULL,
		   *strongs = NULL,
		   *morph = NULL;
	int bracket;
	const char *s;
	char *t, *u;

	for (s = text; *s; ++s) {
		switch (*s) {
		case ' ':
		case '\t':
			break;

		case '<':
			// <token> causes a lot of grief, because we must
			// keep it bound with its </token> terminator,
			// esp. because anchors contain <small></small>. *sigh*
			// i believe we see only anchors + font switches here.
			if ((((*(s + 1) == 'a') || (*(s + 1) == 'A')) && (*(s + 2) == ' ')) ||
			    (((*(s + 1) == 'b') || (*(s + 1) == 'B') ||
			      (*(s + 1) == 'i') || (*(s + 1) == 'I') ||
			      (*(s + 1) == 'u') || (*(s + 1) == 'U')) &&
			     (*(s + 2) == '>'))) {
				if (word)
					block_dump(rendered, &word, &strongs, &morph);

				static char end[5] = "</X>";
				end[2] = *(s + 1);
			again:
				if ((t = strstr((char *)s, end)) == NULL) {
					XI_warning(("No %s in %s\n", end, s));
					break;
				}

				// yet another nightmare:
				// if the markup results in e.g. doubled <i>
				// (bogus "<hi><hi>word</hi></hi>"),
				// then we will mis-assess termination.
				// so we search for the same markup embedded within.
				// if we find an internal set, we just wipe it out.
				static char embedded[4] = "<X>";
				embedded[1] = *(s + 1);
				if ((u = g_strstr_len(s + 3, t - (s + 3), embedded))) {
					*u = *(u + 1) = *(u + 2) =
					    *t = *(t + 1) = *(t + 2) = *(t + 3) = ' ';
					goto again; // yuck, yes, i know...
				}

				// nasb eph 5:31: whole verse is an italicized
				// quotation of gen 2:24...containing strongs.
				// in event of font switch, output bare switch
				// controls but recurse on content within.
				if ((*(s + 1) == 'a') || (*(s + 1) == 'A')) {
					// <a href> anchor -- uninteresting.
					t += 4;
					word = g_strndup(s, t - s);
					s = t - 1;
				} else {
					// font switch nightmare.
					word = g_strndup(s, 3);
					rendered += word;
					g_free((char *)word);
					word = g_strndup(s + 3, t - (s + 3));
					block_render_secondary(word, rendered);
					g_free((char *)word);
					word = NULL;
					rendered += end;
					s = t + 3;
				}
				break;
			} else if (!strncmp(s + 1, "small>", 6)) {
				// strongs and morph are bounded by "<small>".
				if ((t = strstr((char *)s, "</small>")) == NULL) {
					XI_warning(("No </small> in %s\n", s));
					break;
				}
				t += 8;
				// this is very dicey -- phenomenological/
				// observable about Sword filters' provision.
				// strongs: "<em>&lt;...&gt;</em>"
				// morph:   "<em>(...)</em>"
				// if Sword ever changes this, we're dead.
				const char *u = s + 11;
				while ((*u != '(') && (*u != '&'))
					++u; // it has to be one or the other.
				if (*u == '(') {
					if (morph) {
						block_dump(rendered, &word, &strongs, &morph);
						word = g_strdup(EMPTY_WORD);
					}
					morph = g_strndup(s, t - s);
				} else {
					if (strongs) {
						block_dump(rendered, &word, &strongs, &morph);
						word = g_strdup(EMPTY_WORD);
					}
					strongs = g_strndup(s, t - s);
				}
				s = t - 1;
				break;
			}
		// ...fall through to ordinary text...
		// (includes other "<>"-bounded markup.)

		default:
			if (word)
				block_dump(rendered, &word, &strongs, &morph);

			// here's an unfortunate problem.  consider:
			// L<font size="-1">ORD</font> followed by strongs.
			// the SPC breaks it into 2 "words", very badly.
			// we need to track <> use to get it as *1* word,
			// before we capture the strongs, or just the latter
			// half of it ("size=...") goes inside the <span>.
			bracket = 0;
			word = s;
			do {
				while ((*s == ' ') || (*s == '\t'))
					s++;
				for (/* */;
				     *s && (*s != ' ') && (*s != '\t');
				     ++s) {
					if (*s == '<') {
						if (!strncmp(s + 1, "small>", 6) ||
						    ((*(s + 1) == 'a') &&
						     (*(s + 2) == ' '))) {
							// "break 2;"
							goto outword;
						}
						bracket++;
					} else if (*s == '>')
						bracket--;
					assert(bracket >= 0);
				}
			} while (bracket != 0);
		outword:
			word = g_strndup(word, s - word);
			s--;
		}
	}
	if (word || strongs || morph)
		block_dump(rendered, &word, &strongs, &morph);
}

// entry interface.
// initializes for secondary interface.
const char *
block_render(const char *text)
{
	static SWBuf rendered;

	rendered = "";
	block_render_secondary(text, rendered);
	return rendered.c_str();
}

//
// in-place removal of inconvenient-to-the-user content, and note/xref marking.
//
GString *
CleanupContent(GString *text,
	       GLOBAL_OPS *ops,
	       const char *name,
	       bool reset = true)
{
	if (ops->image_content == 0)
		ClearImages((gchar *)text->str);
	else if ((ops->image_content == -1) && // "unknown"
		 (strcasestr(text->str, "<img ") != NULL)) {
		ops->image_content = 1; // now known.
		main_save_module_options(name, "Image Content", 1);
	}
	if (ops->respect_font_faces == 0)
		ClearFontFaces((gchar *)text->str);
	else if ((ops->respect_font_faces == -1) && // "unknown"
		 (strcasestr(text->str, CLEAR_FONT_NAME) != NULL)) {
		ops->respect_font_faces = 1; // now known.
		main_save_module_options(name, "Respect Font Faces", 1);
	}

	gint pos;
	gchar value[50], *reported, *s = text->str;

	// test for any 'n="X"' content.  if so, use it directly.
	if ((reported = backend->get_entry_attribute("Footnote", "1", "n", false))) {
		g_free(reported); // dispose of test junk.
	}
	// otherwise we simply count notes & xrefs through the verse.
	else if (ops->xrefnotenumbers) {
		while ((s = strstr(s, "*n"))) {
			g_snprintf(value, 5, "%d", ++footnote);
			pos = s - (text->str) + 2;
			text = g_string_insert(text, pos, value);
			s = text->str + pos + 1;
		}

		s = text->str;
		while ((s = strstr(s, "*x"))) {
			g_snprintf(value, 5, "%d", ++xref);
			pos = s - (text->str) + 2;
			text = g_string_insert(text, pos, value);
			s = text->str + pos + 1;
		}
	}

	return text;
}

//
// utility function to fill headers from verses.
//
void
CacheHeader(ModuleCache::CacheVerse &cVerse,
	    SWModule &mod,
	    GLOBAL_OPS *ops, BackEnd *be, bool already_rendered)
{
	int x = 0;
	gchar heading[32];
	const gchar *preverse;
	SWBuf preverse2;
	GString *text = g_string_new("");

	cVerse.SetHeader("");

	/* get_entry_attribute() renders the entry itself unless told
	 * otherwise, and this loop asks it once per heading level per
	 * verse -- so a caller that had already rendered the text was
	 * paying to render the whole book a second time, verse by verse.
	 * Render once here when the caller has not, then read the
	 * attributes without rendering again. */
	if (!already_rendered)
		mod.renderText();

	sprintf(heading, "%d", x);
	while ((preverse = be->get_entry_attribute("Heading", "Preverse",
						   heading, false)) != NULL) {
		preverse2 = mod.renderText(preverse);
		g_string_printf(text,
				"%s",
				(((ops->strongs || ops->lemmas) ||
				  ops->morphs)
				     ? block_render(preverse2.c_str())
				     : preverse2.c_str()));
		text = CleanupContent(text, ops, mod.getName(), false);

		cVerse.AppendHeader(text->str);
		g_free((gchar *)preverse);
		++x;
		sprintf(heading, "%d", x);
	}
	g_string_free(text, TRUE);
}

void
set_morph_order(SWModule &imodule)
{
	for (FilterList::const_iterator it =
		 imodule.getRenderFilters().begin();
	     it != imodule.getRenderFilters().end();
	     ++it) {
		OSISXHTML *f = dynamic_cast<OSISXHTML *>(*it);
		if (f)
			f->setMorphFirst();
	}
}

void
set_render_numbers(SWModule &imodule, GLOBAL_OPS *ops)
{
	// if we have not yet determined options, don't bother.
	if (!ops)
		return;

	for (FilterList::const_iterator it =
		 imodule.getRenderFilters().begin();
	     it != imodule.getRenderFilters().end();
	     ++it) {
		OSISXHTML *f1 = dynamic_cast<OSISXHTML *>(*it);
		if (f1)
			f1->setRenderNoteNumbers((ops->xrefnotenumbers != 0));
		ThMLXHTML *f2 = dynamic_cast<ThMLXHTML *>(*it);
		if (f2)
			f2->setRenderNoteNumbers((ops->xrefnotenumbers != 0));
		GBFXHTML *f3 = dynamic_cast<GBFXHTML *>(*it);
		if (f3)
			f3->setRenderNoteNumbers((ops->xrefnotenumbers != 0));
		TEIXHTML *f4 = dynamic_cast<TEIXHTML *>(*it);
		if (f4)
			f4->setRenderNoteNumbers((ops->xrefnotenumbers != 0));
	}
}

//
// display of commentary by chapter.
//
char
GTKEntryDisp::displayByChapter(SWModule &imodule, int columns)
{
	imodule.setSkipConsecutiveLinks(true);

	VerseKey *key = (VerseKey *)(SWKey *) imodule;
	bool before_curVerse(true);
	int curVerse = key->getVerse();
	int curChapter = key->getChapter();
	int curBook = key->getBook();
	int curTest = key->getTestament();
	gchar *buf;
	const char *ModuleName = imodule.getName();
	GString *rework; // for image size analysis rework.
	footnote = xref = 0;

	// we come into this routine with swbuf init'd with
	// boilerplate html startup, plus ops and mf ready.

	cache_flags = ConstructFlags(ops);
	is_rtol = main_is_mod_rtol(ModuleName);

	strongs_and_morph = ((ops->strongs || ops->lemmas) &&
			     ops->morphs);
	strongs_or_morph  = ((ops->strongs || ops->lemmas) ||
			     ops->morphs);
	if (strongs_and_morph)
		set_morph_order(imodule);
	set_render_numbers(imodule, ops);

	// open the table.
	if (settings.showversenum) {
		swbuf.appendFormatted("<font face=\"%s\"><table border=\"0\""
				      " cellpadding=\"5\" cellspacing=\"0\">",
				      ((mf->old_font) ? mf->old_font : ""));
	}

	for (key->setVerse(1);
	     (key->getBook() == curBook) && (key->getChapter() == curChapter) && !imodule.popError();
	     imodule++) {

		ModuleCache::CacheVerse &cVerse =
			ModuleMap[ModuleName][curTest][curBook][curChapter][key->getVerse()];

		// use the module cache rather than re-accessing Sword.
		// but editable personal commentaries don't use the cache.
		if (!cVerse.CacheIsValid(cache_flags) &&
		    (backend->module_type(imodule.getName()) != PERCOM_TYPE)) {
			rework = g_string_new(strongs_or_morph
						  ? block_render(imodule.renderText().c_str())
						  : imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
		} else {
			if (backend->module_type(imodule.getName()) == PERCOM_TYPE)
				rework = g_string_new(strongs_or_morph
							  ? block_render(imodule.getRawEntry())
							  : imodule.getRawEntry());
			else
				rework = g_string_new(cVerse.GetText());
		}

		if (!cVerse.HeaderIsValid())
			CacheHeader(cVerse, imodule, ops, backend, false);
		if (cache_flags & ModuleCache::Headings) {
			swbuf.append(settings.imageresize
					 ? AnalyzeForImageSize(cVerse.GetHeader(), CURRENT_COLUMNS,
							       GDK_WINDOW(gtk_widget_get_window(gtkText)))
					 : cVerse.GetHeader() /* left as-is */);
		} else
			cVerse.InvalidateHeader();

		// add an anchor for where in the chapter we are.
		// (commentaries can have big sections on 1 verse [<hr>],
		// and many missing verses [<a name>].)
		if ((curVerse != 1) && // not at top of pane
		    ((curVerse == key->getVerse()) ||
		     (before_curVerse && (key->getVerse() > curVerse)))) {
			buf = NULL;
			swbuf.appendFormatted("<tr><td>%s<hr/></td><td><hr/></td></tr>",
					      // repeated conditional check here
					      ((before_curVerse &&
						(key->getVerse() > curVerse))
					       ? (buf = g_strdup_printf(
							  "<a name=\"%d\"> </a>", curVerse))
					       : ""));
			g_free(buf);
		}

		swbuf.append("<tr>");

		// insert verse numbers
		char *num = main_format_number(key->getVerse());

		swbuf.appendFormatted((settings.showversenum
				       ? "<td valign=\"top\" align=\"right\">"
				       "<a name=\"%d\" href=\"sword:///%s\">"
				       "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font></a></td>"
				       : "<p/><a name=\"%d\"> </a>"),
				      key->getVerse(),
				      (char *)key->getText(),
				      settings.verse_num_font_size + settings.base_font_size,
				      settings.bible_verse_num_color,
				      PRETTYPRINT(num));
		g_free(num);

		if (settings.showversenum) {
			swbuf.appendFormatted("<td><font size=\"%+d\">",
					      mf->old_font_size_value);
		}
		swbuf.append(settings.imageresize
				 ? AnalyzeForImageSize(rework->str, columns,
						       GDK_WINDOW(gtk_widget_get_window(gtkText)))
				 : rework->str /* left as-is */);
		if (settings.showversenum)
			swbuf.append("</font></td>");

		swbuf.append("</tr>");
		before_curVerse = (key->getVerse() < curVerse);
	}

	// if we haven't gotten around to placing the anchor, do so now.
	if (before_curVerse) {
		swbuf.appendFormatted("<tr><td><a name=\"%d\"> </a><hr/></td><td><hr/></td></tr>",
				      curVerse);
	}

	// close the table.
	if (settings.showversenum)
		swbuf.append("</table></font>");
	swbuf.append("</div></font></body></html>");

	buf = g_strdup_printf("%d", curVerse);
	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, buf);
	g_free(buf);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

//
// general display of entries: commentary, genbook, lexdict
//
static void
_render_display_level(SWModule &imodule, unsigned long offset,
                      int max_level, int cur_level, SWBuf &combined)
{
	TreeKeyIdx *treekey = (TreeKeyIdx *)imodule.getKey();
	treekey->setOffset(offset);
	imodule.getRawEntry(); // snap to entry

	const char *raw = imodule.getRawEntry();
	if (raw && *raw) {
		if (combined.length() > 0)
			combined += "<br/><hr/>";
		combined += imodule.renderText().c_str();
	}

	if (cur_level < max_level && treekey->hasChildren()) {
		treekey->firstChild();
		unsigned long child_offset = treekey->getOffset();
		bool has_next = true;
		while (has_next) {
			_render_display_level(imodule, child_offset,
					      max_level, cur_level + 1,
					      combined);
			// reposition after recursion
			treekey->setOffset(child_offset);
			imodule.getRawEntry();
			has_next = treekey->nextSibling() && !treekey->popError();
			if (has_next)
				child_offset = treekey->getOffset();
		}
		// restore to current node
		treekey->setOffset(offset);
		imodule.getRawEntry();
	}
}

char
GTKEntryDisp::display(SWModule &imodule)
{
	if (!gtk_widget_get_realized(GTK_WIDGET(gtkText)))
		gtk_widget_realize(gtkText);

	const char *abbreviation = main_name_to_abbrev(imodule.getName());
	buf = mod_column_count = NULL;
	mf = get_font(imodule.getName());
	swbuf = "";
	footnote = xref = 0;

	ops = main_new_globals(imodule.getName());

	GString *rework; // for image size analysis rework.

	imodule.getRawEntry(); // snap to entry
	main_set_global_options(ops);

	strongs_and_morph = ((ops->strongs || ops->lemmas) &&
			     ops->morphs);
	strongs_or_morph  = ((ops->strongs || ops->lemmas) ||
			     ops->morphs);
	if (strongs_and_morph)
		set_morph_order(imodule);
	set_render_numbers(imodule, ops);

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START // //bgcolor=\"%s\" text=\"%s\" link=\"%s\">"
			      "<font face=\"%s\" size=\"%+d\">"
			      "[<a href=\"passagestudy.jsp?action=showModInfo&value=%s&module=%s\">"
			      "<font color=\"%s\">*%s*</font></a>]<br/>",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      (strongs_and_morph // both
				   ? CSS_BLOCK_BOTH
				   : (strongs_or_morph // either
					  ? CSS_BLOCK_ONE
					  : (ops->doublespace // neither
						 ? DOUBLE_SPACE
						 : ""))),
			      imodule.getRenderHeader(),
			      ITALIC_SELECT,
			      (mod_column_count ? mod_column_count : ""),
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value,
			      imodule.getDescription(),
			      imodule.getName(),
			      settings.bible_verse_num_color,
			      (abbreviation ? abbreviation : imodule.getName()));

	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	swbuf.appendFormatted("<div dir=%s>",
			      ((is_rtol && !ops->transliteration)
				   ? "rtl"
				   : "ltr"));

	if (!valid_scripture_key) {
		swbuf.append(no_content);
		swbuf.append("</div></font></body></html>");
		HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);
		free_font(mf);
		mf = NULL;
		g_free(ops);
		ops = NULL;
		return 0;
	}

	//
	// the rest of this routine is irrelevant if we are
	// instead heading off to show a whole chapter
	// (this option can be enabled only in commentaries.)
	//
	if (ops->commentary_by_chapter)
		return displayByChapter(imodule, CURRENT_COLUMNS);

	// we will use the module cache for regular commentaries,
	// which navigate/change a lot, whereas pers.comms, lexdicts,
	// and genbooks still do fresh access every time -- the nature
	// of those modules' use won't buy much with a module cache.

	// there is some unfortunate but unavoidable code duplication
	// for handling potential clearing of images, due to the
	// difference in how modules are being accessed.

	int modtype = backend->module_type(imodule.getName());
	if (modtype == COMMENTARY_TYPE) {
		VerseKey *key = (VerseKey *)(SWKey *) imodule;
		cache_flags = ConstructFlags(ops);
		const char *ModuleName = imodule.getName();

		ModuleCache::CacheVerse &cVerse = ModuleMap
		    [ModuleName]
		    [key->getTestament()]
		    [key->getBook()]
		    [key->getChapter()]
		    [key->getVerse()];

		// use the module cache rather than re-accessing Sword.
		if (!cVerse.CacheIsValid(cache_flags)) {
			rework = g_string_new(strongs_or_morph
						  ? block_render(imodule.renderText().c_str())
						  : imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
		} else
			rework = g_string_new(cVerse.GetText());
	} else {
		if ((modtype == PERCOM_TYPE) ||
		    (modtype == PRAYERLIST_TYPE))
			rework = g_string_new(strongs_or_morph
					      ? block_render(imodule.getRawEntry())
					      : imodule.getRawEntry());
		else {
			// respect DisplayLevel for genbooks (BOOK_TYPE):
			const char *dl_str = imodule.getConfigEntry("DisplayLevel");
			int display_level = dl_str ? atoi(dl_str) : 1;
			if (display_level <= 1) {
				rework = g_string_new(strongs_or_morph
							  ? block_render(imodule.renderText().c_str())
							  : imodule.renderText().c_str());
			} else {
				SWMgr *mgr = backend->get_mgr();
				SWModule *mod = mgr->Modules[imodule.getName()];
				TreeKeyIdx *treekey = dynamic_cast<TreeKeyIdx *>(mod->getKey());
				if (!treekey) {
					rework = g_string_new(imodule.renderText().c_str());
				} else {
					TreeKeyIdx saved = *treekey;
					SWBuf combined = "";
					_render_display_level(*mod, saved.getOffset(),
							      display_level, 1, combined);
					treekey->setOffset(saved.getOffset());
					mod->getRawEntry();
					rework = g_string_new(strongs_or_morph
								  ? block_render(combined.c_str())
								  : combined.c_str());
				}
			}
		}
		rework = CleanupContent(rework, ops, imodule.getName());
	}

	swbuf.append(settings.imageresize
			 ? AnalyzeForImageSize(rework->str, CURRENT_COLUMNS,
					       GDK_WINDOW(gtk_widget_get_window(gtkText)))
			 : rework->str /* left as-is */);

	swbuf.append("</div></font></body></html>");

	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

GString *
GTKChapDisp::introMaterial(SWModule &imodule, int thisChapter)
{
	GString *intro = g_string_new(NULL);

	//
	// displayable content at 0:0 and n:0.
	//
	char oldAutoNorm = key->isAutoNormalize();
	key->setAutoNormalize(0);

	bool started_intro = false;

	for (int i = 0; i < 2; ++i) {
		// Get chapter 0 iff we're in chapter 1.
		if ((i == 0) && (thisChapter != 1))
			continue;

		key->setChapter(i * thisChapter);
		key->setVerse(0);

		buf = g_strdup_printf("%s", strongs_or_morph
				      ? block_render(imodule.renderText().c_str())
				      : imodule.renderText().c_str());

		if ((buf != NULL) && (strlen(buf) > 0))
		{
			if (!started_intro)
			{
				g_string_append(intro, "<div class=\"introMaterial\">");
				started_intro = true;
			}

			g_string_append(intro,
					(settings.imageresize
					 ? AnalyzeForImageSize(buf, CURRENT_COLUMNS,
							       GDK_WINDOW(gtk_widget_get_window(gtkText)))
					 : buf));
			g_string_append(intro, "<br />");
			g_free(buf);
		}
	}

	if (started_intro)
		g_string_append(intro, "</div>");		// finish what we started.

	key->setAutoNormalize(oldAutoNorm);

	key->setTestament(curTest);
	key->setBook(curBook);
	key->setChapter(curChapter);
	key->setVerse(curVerse);

	return intro;
}

void
GTKChapDisp::getVerseBefore(SWModule &imodule)
{
	char *num;

	key->setVerse(1);
	imodule--;

	if (imodule.popError()) {
		imodule++;	// restore position because we're at beginning

		swbuf.appendFormatted("<a name=\"TOP\"></a><div style=\"text-align: center\">"
				      "<p><b><font size=\"%+d\">%s</font></b></p></div>",
				      1 + mf->old_font_size_value,
				      imodule.getDescription());
	} else {

		if (strongs_and_morph)
			set_morph_order(imodule);
		set_render_numbers(imodule, ops);

		num = main_format_number(key->getVerse());
		swbuf.appendFormatted((settings.showversenum
				       ? "&nbsp;<a name=\"0\" href=\"sword:///%s\">"
				       "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font>&nbsp;"
				       : "&nbsp;<a name=\"0\"> </a>"),
				      (char *)key->getText(),
				      (settings.versestyle
				       ? settings.verse_num_font_size + settings.base_font_size
				       : settings.base_font_size - 2),
				      settings.bible_verse_num_color,
				      PRETTYPRINT(num));
		g_free(num);

		swbuf.appendFormatted("<font color=\"%s\">%s</font>%s%s<br/><a name=\"TOP\"></a>%s",
				      settings.bible_text_color,
				      (strongs_or_morph
				       ? block_render(imodule.renderText().c_str())
				       : imodule.renderText().c_str()),
				      (settings.showversenum ? "</a>" : ""),
				      // extra break when excess strongs/morph space.
				      (strongs_or_morph ? "<br/>" : ""),
				      (ops->headings ? "<hr/>" : ""));

		imodule++;	// restore position after getting "before" verse
	}

	buf = NULL;
}

void
GTKChapDisp::getVerseAfter(SWModule &imodule)
{
	/* Marks where the last real verse's content ends and the trailing
	 * chrome (the <hr/> + "Chapter N" heading below) begins. Without
	 * this, wk_html_anchor_bounds() has no anchor to stop at until
	 * "0next" further down (on the next-chapter preview verse), so the
	 * reading-focus band for the chapter's real last verse swallows the
	 * <hr/> and heading too -- the text-heuristic trims
	 * (trim_trailing_heading() et al.) try to compensate but don't
	 * reliably recognize this specific block. A real anchor sidesteps
	 * the heuristics entirely, the same fix already applied for "0next"
	 * itself. Inert like "0"/"0next" -- see the sentinel checks in
	 * wk_html_anchor_at(), find_focus_anchor(), and
	 * gui_bibletext_lectura_sync_focus_refresh(). */
	swbuf.appendFormatted("<a name=\"0hdr\"></a>");

	imodule++;
	if (imodule.popError()) {
		swbuf.appendFormatted("%s<hr/><div style=\"text-align: center\"><p><b>%s</b></p></div>",
				      // extra break when excess strongs/morph space.
				      (strongs_or_morph ? "<br/><br/>" : ""),
				      imodule.getDescription());
	} else {
		char *num = main_format_number(key->getChapter());

		swbuf.appendFormatted((ops->display_chapter_N
				       ? "%s%s<div style=\"text-align: center\"><b>%s %s</b></div>"
				       : "%s%s" ),
				      (strongs_or_morph ? "<br/><br/>" : ""),
				      (ops->headings ? "<hr/>" : ""),
				      _("Chapter"), num);
		g_free(num);

		num = main_format_number(key->getVerse());
		/* "0next", not "0" -- getVerseBefore() above already placed an
		 * <a name="0"> for its own preview snippet earlier in this same
		 * buffer. place_anchor() (wk-html.c) dedupes by name, so a
		 * second "0" here would be silently dropped, leaving this
		 * trailing next-chapter preview with no anchor at all -- which
		 * is exactly what made the reading-focus band for the chapter's
		 * real last verse swallow this preview too (wk_html_anchor_bounds()
		 * falls back to "rest of the buffer" when it can't find a next
		 * anchor to stop at). Every "is this a real verse" sentinel
		 * check elsewhere already treats non-numeric anchor names as
		 * atol()==0 too, so this stays just as inert as "0" was. */
		swbuf.appendFormatted((settings.showversenum
				       ? "&nbsp;<a name=\"0next\" href=\"sword:///%s\">"
				       "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font>&nbsp;"
				       : "&nbsp;<a name=\"0next\"> </a>"),
				      (char *)key->getText(),
				      (settings.versestyle
				       ? settings.verse_num_font_size + settings.base_font_size
				       : settings.base_font_size - 2),
				      settings.bible_verse_num_color,
				      PRETTYPRINT(num));
		g_free(num);

		if (strongs_and_morph)
			set_morph_order(imodule);
		set_render_numbers(imodule, ops);

		swbuf.appendFormatted("<font color=\"%s\">%s</font>%s",
				      settings.bible_text_color,
				      (strongs_or_morph
				       ? block_render(imodule.renderText().c_str())
				       : imodule.renderText().c_str()),
				      (settings.showversenum ? "</a>" : ""));

		imodule--;
	}

	buf = NULL;
}

/* Returns white or black depending on background luminance */
static const char *text_color_for_bg(const gchar *hex_color)
{
	if (!hex_color || hex_color[0] != '#' || strlen(hex_color) < 7)
		return "#000000";
	int r = 0, g = 0, b = 0;
	sscanf(hex_color + 1, "%02x%02x%02x", &r, &g, &b);
	/* relative luminance (ITU-R BT.709) */
	double lum = 0.2126 * r + 0.7152 * g + 0.0722 * b;
	return (lum < 128.0) ? "#FFFFFF" : "#000000";
}

/* Tag-color lookup cache: built once per GTKChapDisp::display() call
 * (i.e. once per book in render_whole_books mode, not once per
 * verse). The previous approach called parse_verse_list() from
 * inside the per-verse render loop, once per bookmark, for every
 * single verse rendered. Since parse_verse_list() repositions the
 * SAME SWModule key object that the render loop is simultaneously
 * using for content, doing that dozens of times per chapter left
 * residual/corrupted key state which could cause bookmarks to appear
 * to match verses they don't reference at all (reported: stray
 * highlights in Genesis 1 despite no bookmarks there). Building the
 * map once, up front, with the key fully restored afterwards, and
 * then doing a plain hash-table lookup per verse during rendering,
 * avoids touching the key at all while the chapter is being drawn. */
static GHashTable *tag_color_map = NULL;

static void free_tag_color_map(void)
{
	if (tag_color_map) {
		g_hash_table_destroy(tag_color_map);
		tag_color_map = NULL;
	}
}

static void build_tag_color_map(VerseKey *vk)
{
	free_tag_color_map();
	tag_color_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	extern GtkTreeStore *model;
	if (!settings.tag_colorize || !model || !vk)
		return;

	GtkTreeIter root;
	if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(model), &root))
		return;

	/* Resolve everything against a saved copy of the key's text, and
	 * restore it when done, so the live render position is never
	 * left disturbed once building the map is finished. */
	gchar *saved_pos = g_strdup((const char *)vk->getText());

	GQueue *stack = g_queue_new();
	GQueue *colors = g_queue_new();
	GtkTreeIter child;
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model), &child, &root)) {
		do {
			GtkTreeIter *copy = g_new(GtkTreeIter, 1);
			*copy = child;
			g_queue_push_tail(stack, copy);
			g_queue_push_tail(colors, NULL);
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(model), &child));
	}

	while (!g_queue_is_empty(stack)) {
		GtkTreeIter *iter = (GtkTreeIter *)g_queue_pop_head(stack);
		gchar *inherited = (gchar *)g_queue_pop_head(colors);
		gchar *node_color = NULL, *node_key = NULL, *node_module = NULL, *node_label = NULL;
		gtk_tree_model_get(GTK_TREE_MODEL(model), iter,
				   COL_COLOR,       &node_color,
				   COL_KEY,         &node_key,
				   COL_MODULE,      &node_module,
				   COL_DESCRIPTION, &node_label,
				   -1);
		gchar *escaped_label = (node_label
					? g_uri_escape_string(node_label, NULL, TRUE)
					: NULL);

		const gchar *effective = (node_color && *node_color)
			? node_color : inherited;
		if (node_key) {
			/* Let Sword resolve the (possibly multi-reference, possibly
			 * partial) bookmark key into individual, fully-qualified
			 * verses -- same mechanism used for the verse-list popup.
			 * Only do this for bookmarks that actually target Bible
			 * text (an empty module means "applies to all Bible
			 * modules", the long-standing convention here) or a
			 * commentary (also verse-keyed). Bookmarks pointing at a
			 * dictionary or genbook module (e.g. Josephus section
			 * numbers, Strong's numbers) have keys that are NOT verse
			 * references at all; feeding them to Sword's verse-list
			 * parser doesn't fail cleanly -- it silently reinterprets
			 * the bare number/text as some arbitrary chapter:verse,
			 * producing spurious highlighted verses elsewhere in the
			 * Bible (reported: stray highlights in Genesis 1). */
			gboolean is_scripture_key = TRUE;
			if (node_module && *node_module) {
				int mtype = backend->module_type(node_module);
				if ((mtype != TEXT_TYPE) && (mtype != COMMENTARY_TYPE))
					is_scripture_key = FALSE;
			}
			if (effective && is_scripture_key) {
				// footnote elimination, like "Song.2.4#n17".
				// remove thml (#) and osis (!) subverse indicators.
				// must do this before Sword parse gets hold of it.
				gchar *punc;
				while ((punc = strpbrk(node_key, "#!"))) {
					*(punc++) = ' ';
					while ((punc = strpbrk(punc, "n0123456789"))) {
						*(punc++) = ' ';
					}
				}

				GList *verses = backend->parse_verse_list(
					settings.MainWindowModule, node_key,
					(char *)settings.currentverse);
				for (GList *l = verses; l; l = l->next) {
					VerseKey vk2;
					vk2.setLocale(vk->getLocale());
					vk2.setText((const char *)l->data);
					gchar *ref2 = g_strdup_printf("%s.%d.%d",
						vk2.getOSISBookName(),
						vk2.getChapter(),
						vk2.getVerse());

					/* first color wins for display of a given verse
					 * but all other occurrences will be appended. */
					const char *tc_fg = text_color_for_bg(effective);
					gchar *element = (gchar *)g_hash_table_lookup(tag_color_map, ref2);
					gchar *str = g_strconcat((element ? element : ""),
								 effective,
								 "-",		// arbitrary separators.
								 tc_fg,
								 "-",
								 (escaped_label ? escaped_label : ""),
								 "@:@:@",	// magic string, see main_info_viewer().
								 NULL);
					// insertion may supercede previous instance, freeing it.
					// ownership of newly-allocated str is given to the map.
					// (parse_verse_list() itself now expands hyphenated
					// ranges like "15:1-4" into one list element per verse,
					// so no further manual range expansion is needed here
					// -- doing so used to double up entries for every verse
					// after the first one in a range.)
					g_hash_table_insert(tag_color_map, ref2, str);
				}
				for (GList *l = verses; l; l = l->next)
					g_free(l->data);
				g_list_free(verses);
			}
			g_free(node_key);
		} else {
			if (gtk_tree_model_iter_children(GTK_TREE_MODEL(model),
							 &child, iter)) {
				do {
					GtkTreeIter *copy = g_new(GtkTreeIter, 1);
					*copy = child;
					g_queue_push_tail(stack, copy);
					g_queue_push_tail(colors,
						effective ? g_strdup(effective) : NULL);
				} while (gtk_tree_model_iter_next(
						GTK_TREE_MODEL(model), &child));
			}
		}
		g_free(escaped_label);
		g_free(node_label);
		g_free(node_color);
		g_free(node_module);
		g_free(inherited);
		g_free(iter);
	}
	g_queue_free_full(stack, g_free);
	g_queue_free_full(colors, g_free);

	vk->setText(saved_pos);
	g_free(saved_pos);
}

/* Cheap per-verse lookup against the map built by build_tag_color_map().
 * Does NOT touch the module's key/position at all. */
static gchar *get_tag_color_for_versekey(VerseKey *vk)
{
	if (!settings.tag_colorize || !tag_color_map || !vk)
		return NULL;
	gchar *osisref = g_strdup_printf("%s.%d.%d",
		vk->getOSISBookName(),
		vk->getChapter(),
		vk->getVerse());
	gchar *color = (gchar *)g_hash_table_lookup(tag_color_map, osisref);
	g_free(osisref);
	return color ? g_strdup(color) : NULL;
}

// with/without a useless space, because it's what we've seen in the field. *sigh*
/* Chapter range the Bible pane currently holds, published for the
 * in-place shortcut in main_display_bible(). */
int main_rendered_first_chapter = 0, main_rendered_last_chapter = 0;

const gchar *para_endings[] = { "<p/>", "<p />" };

void
GTKChapDisp::RenderOneChapter(SWModule &imodule,
			      int thisChapter)
{
	char *num;
	GString *rework;				// for image size analysis rework.
	GString *intro;
	const char *ModuleName = imodule.getName();

	/* Issue #921: this anchor must stay self-closed and must NOT wrap
	 * the chapter title / intro material below, since that content
	 * legitimately contains block-level elements (<div>, <h2> from
	 * introMaterial()). An <a> is an inline "formatting element" in
	 * the HTML5 spec; wrapping block content inside it forces the
	 * parser's adoption agency algorithm to silently fragment/
	 * duplicate the <a> in the resulting DOM, which breaks WebKit's
	 * keyboard caret navigation (Down/Page_Down) and can also leak
	 * inline styles (e.g. italics) past their intended scope. */
	swbuf.appendFormatted("<a name=\"%d\"></a>", (thisChapter * 1000));

	if (ops->display_chapter_N) {
		num = main_format_number(thisChapter);
		swbuf.appendFormatted("<div style=\"text-align: center\"><b>%s %s</b></div>",
				      _("Chapter"), num);
		g_free(num);
	}

	if (ops->headings) {
		intro = GTKChapDisp::introMaterial(imodule, thisChapter);
		swbuf.append(intro->str);
		g_string_free(intro, TRUE);
	}

	key->setTestament(curTest);
	key->setBook(curBook);
	key->setChapter(thisChapter);

	/* At most one verse in this chapter carries the interlinear
	 * apparatus, so resolve which one up front instead of asking
	 * main_interlineal_html_original() for every verse -- that call
	 * ran VerseKey::setText() over *both* keys just to answer "no",
	 * measured at 36 us a verse (~6 ms across a long psalm).
	 *
	 * The probe is a copy of the module's own key, so the anchor is
	 * parsed in the module's versification and compared against the
	 * numbering the loop below is actually walking. That matters:
	 * modules here use KJV, Vulg (SpaPlatense) and Leningrad (WLC),
	 * and resolving the anchor in one versification while walking
	 * another lands on the wrong verse. */
	int il_verse_here = -1;
	{
		const char *il_open = main_interlineal_verso_abierto();
		if (il_open && *il_open) {
			VerseKey probe = *key;
			probe.setAutoNormalize(1);
			probe.setText(il_open);
			if (!probe.popError() &&
			    (probe.getTestament() == curTest) &&
			    (probe.getBook() == curBook) &&
			    (probe.getChapter() == thisChapter))
				il_verse_here = probe.getVerse();
		}
	}

	for (int k = 1 ; k <= key->getVerseMax() ; ++k) {

		key->setVerse(k);

		ModuleCache::CacheVerse &cVerse =
		    ModuleMap[ModuleName][curTest][curBook][thisChapter][k];

		/* Text first, header second. SetText() deliberately drops
		 * any cached header ("we are setting from scratch:
		 * neutralize header"), so computing the header before it
		 * meant throwing the header away moments later, on every
		 * render, and recomputing it on the next one -- the header
		 * cache never served a single hit. CacheHeader() asks Sword
		 * for a Preverse attribute per verse, which measured 185 ms
		 * of the 220 ms this loop took for Psalms.
		 *
		 * The output order is unchanged: the heading is still
		 * appended to swbuf before the verse it introduces. */
		bool just_rendered = false;
		if (!cVerse.CacheIsValid(cache_flags)) {
			rework = g_string_new(strongs_or_morph
						  ? block_render(imodule.renderText().c_str())
						  : imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
			just_rendered = true;
		} else
			rework = g_string_new(cVerse.GetText());

		if (!cVerse.HeaderIsValid())
			CacheHeader(cVerse, imodule, ops, be, just_rendered);

		if (cache_flags & ModuleCache::Headings) {
			swbuf.append(settings.imageresize
					 ? AnalyzeForImageSize(cVerse.GetHeader(), CURRENT_COLUMNS,
							       GDK_WINDOW(gtk_widget_get_window(gtkText)))
					 : cVerse.GetHeader() /* left as-is */);
		} else
			cVerse.InvalidateHeader();

		if (*rework->str == '\0')
			continue;		// no verse content there.

		// tag-group (bookmark folder) color highlight -- wraps the
		// verse number, user-note reference, and verse text below.
		gchar *tag_color = get_tag_color_for_versekey(key);
		if (tag_color) {
			// we own the tag_color data, so now we can mangle it for local purposes.
			// color is 1st 7 characters (#ABCDEF). 8th becomes its terminator.
			// similar foreground per luminance test, bytes 9-16.
			// from tag_color+16 onward, it's bookmark reference data.
			// this content was encoded when created, so we're safe pasting it in.
			// format has bg+fg+label formatted as "#123456-#9ABCDE-LabelData".
			//                                      0......78......FLabelData

			*(tag_color + 7)  = '\0';		// terminate color strings
			*(tag_color + 15) = '\0';

			swbuf.appendFormatted("&nbsp;<span class=\"tagcolor\" style=\"background-color: %s; "
					      "color: %s; \">",
					      tag_color, tag_color + 8);
		}

		// Anchor first so the next verse's tools chip is not
		// inside this verse's reading-focus bounds.
		swbuf.appendFormatted("<p class=\"verse\"><a name=\"%d\"></a>",
				      (thisChapter * 1000) + key->getVerse());
		append_verse_tools(swbuf, key->getText(),
				  highlight_count_notes_at((thisChapter * 1000) + key->getVerse()) > 0);

		gchar *num = main_format_number(key->getVerse());
		if (settings.showversenum)
			swbuf.appendFormatted("&nbsp;<span class=\"word\"><a href=\"sword:///%s\">"
					     "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font></a></span>&nbsp;",
					     (char *)key->getText(),
					     settings.verse_num_font_size + settings.base_font_size,
					     (tag_color ? (tag_color + 8) : settings.bible_verse_num_color),
					     PRETTYPRINT(num));
		g_free(num);

		if (tag_color) {
			*(tag_color + 7)  = '-';		// restore format
			*(tag_color + 15) = '-';		// for use in link data

			swbuf.appendFormatted("<span class=\"bookmarkref\">"
					      "<a href=\"passagestudy.jsp?action=showBookmarkSource&"
					      "module=%s&passage=%s&value=%s\">"
					      "<small><sup>*b</sup></small></a></span>&nbsp;",
					      settings.MainWindowModule,
					      (char *)key->getShortText(),
					      tag_color);	// includes all data, bg+fg+label
		}

		append_verse_note_marker(swbuf, (thisChapter * 1000) + k,
					 settings.MainWindowModule,
					 (char *)key->getOSISRef());

		// Applies any stored phrase highlights to `rework` and sets
		// color_choices/color_chosen_fg/color_chosen_bg for a
		// whole-verse note, if any. (The current verse itself gets a
		// native full-line band after render,
		// wk_html_reading_focus_set(), not a green font color here.)
		apply_verse_notes(rework, (thisChapter * 1000) + k);

		// ugly ... ugly ... ugly.
		// text containing <p/> in the middle of a <span> or <font> block
		// induces a premature closure of the <span> or <font> content.
		// this has follow-on effects, likely a webkit bug, where such
		// background colorization is re-introduced in psychotic ways beyond
		// the end of the </span> or </font>.
		// solution is ... be still, my wretching stomach ...
		// within any form of background colorization, hunt down all <p/>
		// so as to replace them with <br/><br/>. just keep telling yourself,
		// we do this for fun, we do this for fun, we do this for fun, we do...

		if ((color_choices == COLOR_BOTH) || tag_color) {
			for (int i = 0; i < 2; ++i) {
				for (gchar *s = strstr(rework->str, para_endings[i]);
				     s; 
				     s = strstr(s + 1, para_endings[i])) {
					// 4- & 5-char strings.
					(void)g_string_erase(rework, s - rework->str, 4+i);
					(void)g_string_insert(rework, s - rework->str, "<br/><br/>");
				}
			}
		}

		if (color_choices == COLOR_BOTH) {
			swbuf.appendFormatted("<span style=\"background-color: %s\">", color_chosen_bg);
		}

		if (color_choices != COLOR_NONE) {
			swbuf.appendFormatted("<font color=\"%s\">", color_chosen_fg);
		}

		swbuf.append(settings.imageresize
				 ? AnalyzeForImageSize(rework->str, CURRENT_COLUMNS,
						       GDK_WINDOW(gtk_widget_get_window(gtkText)))
				 : rework->str /* left as-is */);

		if (color_choices != COLOR_NONE) {
			swbuf.append("</font>");
			ReadAloud(curVerse, rework->str);
		}

		if (color_choices == COLOR_BOTH)
			swbuf.append("</span>");

		// close tag-group color span
		if (tag_color) {
			swbuf.append("</span>");
			g_free(tag_color);
			tag_color = NULL;
		}

		swbuf.append("</p>");

		if (k == il_verse_here) {
			gchar *ilhtml = main_interlineal_html_original(key->getText());
			if (ilhtml) {
				swbuf.append(ilhtml);
				g_free(ilhtml);
			}
		}
	}
}

void
GTKChapDisp::RenderWholeBook(SWModule &imodule)
{
	int thisChapter, first_chapter, last_chapter;

	// pre-Genesis, name the Bible.
	if (curBook == 1 && (settings.reading_mode_window <= 0 ||
			     curChapter - settings.reading_mode_window <= 1)) {
		swbuf.appendFormatted("<a name=\"TOP\"></a><div style=\"text-align: center\">"
				      "<p><b><font size=\"%+d\">%s</font></b></p></div>",
				      1 + mf->old_font_size_value,
				      imodule.getDescription());
	}

	/* A window of chapters around the one being read, rather than the
	 * whole book. Rendering is Sword's cost and it is not reducible:
	 * measured on this machine, renderText() over the 2461 verses of
	 * Psalms takes 146 ms inside Sword itself, against 18 ms to read
	 * the same entries raw. The only lever left is rendering fewer
	 * verses, and a reader looking at one chapter has no use for the
	 * other 149.
	 *
	 * The window is what makes scrolling continuous: it is wide enough
	 * to read through without hitting an edge, and moving beyond it
	 * lays out a new one, which costs a fraction of the book.
	 * settings.reading_mode_window is the number of chapters kept
	 * either side; 0 restores rendering the entire book. */
	first_chapter = 1;
	last_chapter = key->getChapterMax();
	if (settings.reading_mode_window > 0) {
		first_chapter = curChapter - settings.reading_mode_window;
		last_chapter = curChapter + settings.reading_mode_window;
		if (first_chapter < 1)
			first_chapter = 1;
		if (last_chapter > key->getChapterMax())
			last_chapter = key->getChapterMax();
	}

	/* what the pane will actually hold, for main_display_bible()'s
	 * in-place shortcut to check against. */
	main_rendered_first_chapter = first_chapter;
	main_rendered_last_chapter = last_chapter;

	for (thisChapter = first_chapter; thisChapter <= last_chapter; ++thisChapter) {
		RenderOneChapter(imodule, thisChapter);
		swbuf.appendFormatted("%s%s",
				      // extra break when excess strongs/morph space.
				      (strongs_or_morph ? "<br/><br/>" : ""),
				      (ops->headings ? "<hr/>" : ""));
	}

	// post-Revelation, name the Bible.
	if (curBook == key->getBookMax() &&
	    (settings.reading_mode_window <= 0 ||
	     curChapter + settings.reading_mode_window >= key->getChapterMax())) {
		swbuf.appendFormatted("%s<hr/><div style=\"text-align: center\"><p><b>%s</b></p></div>",
				      // extra break when excess strongs/morph space.
				      (strongs_or_morph ? "<br/><br/>" : ""),
				      imodule.getDescription());
	}
}

char
GTKChapDisp::display(SWModule &imodule)
{
	if (!gtk_widget_get_realized(GTK_WIDGET(gtkText)))
		return 0;

	// following line ensures linked verses work correctly
	// it does not solve the problem of marking groups of verses (1-4), etc
	imodule.setSkipConsecutiveLinks(true);

	const char *ModuleName = imodule.getName();

	ops         = main_new_globals(ModuleName);
	/* Strong's superscripts belong only to the α interlinear toggle,
	 * never to the plain Bible pane. */
	if (!settings.show_interlineal)
		ops->strongs = 0;
	cache_flags = ConstructFlags(ops);

	key        = (VerseKey *)(SWKey *) imodule;
	curTest    = key->getTestament();
	curBook    = key->getBook();
	curChapter = key->getChapter();
	curVerse   = key->getVerse();

	is_rtol = main_is_mod_rtol(ModuleName);
	mf = get_font(ModuleName);

	strongs_and_morph = ((ops->strongs || ops->lemmas) &&
			     ops->morphs);
	strongs_or_morph  = ((ops->strongs || ops->lemmas) ||
			     ops->morphs);
	if (strongs_and_morph)
		set_morph_order(imodule);
	set_render_numbers(imodule, ops);

	settings.versestyle = ops->verse_per_line;

	// if we are no longer where notes/highlights were current, re-load.
	if (strcasecmp(ModuleName,
		       (note_cache_modname ? note_cache_modname : "")) ||
	    strcasecmp(key->getBookAbbrev(), note_cache_book))
		notesCacheFill(ModuleName, settings.currentverse);

	swbuf = "";
	footnote = xref = 0;

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START // "<body bgcolor=\"%s\" text=\"%s\" link=\"%s\">"
			      "<font face=\"%s\" size=\"%+d\">",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      // strongs & morph specs win over dblspc.
			      (strongs_and_morph // both
				   ? CSS_BLOCK_BOTH
				   : (strongs_or_morph // either
					  ? CSS_BLOCK_ONE
					  : (ops->doublespace // neither
						 ? DOUBLE_SPACE
						 : ""))),
			      imodule.getRenderHeader(),
			      ITALIC_SELECT,
			      (mod_column_count ? mod_column_count : ""),
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value);

	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	swbuf.appendFormatted("<div dir=%s>",
			      ((is_rtol && !ops->transliteration)
				   ? "rtl"
				   : "ltr"));

	main_set_global_options(ops);

	if (!valid_scripture_key) {
		swbuf.append(no_content);
		swbuf.append("</div></font></body></html>");
		HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);
		free_font(mf);
		mf = NULL;
		g_free(ops);
		ops = NULL;
		return 0;
	}

	/* Build the tag-color lookup table once, up front, covering the
	 * whole book (whole-book mode) or the whole chapter (chapter
	 * mode) about to be rendered -- see build_tag_color_map() for why
	 * this must not be done per-verse inside the render loop. */
	build_tag_color_map(key);

	/* The chapter-at-a-time render ends with a single teaser verse of
	 * the next chapter (getVerseAfter()), so scrolling to the bottom
	 * stops dead one verse in. Rendering the whole book instead makes
	 * the scroll continuous, which is what reading wants -- but it is
	 * not free: measured at 1.9 s for Psalms (2461 verses), paid again
	 * on every move to another chapter of that book. Too slow to
	 * impose, so reading_mode_whole_book turns it on for those who
	 * want continuous reading more than they want fast navigation.
	 * Never for the comparison: a whole book of nested per-cell views
	 * would be far worse (~1.3 ms a cell). */
	if (settings.render_whole_books ||
	    (settings.reading_mode && settings.reading_mode_whole_book &&
	     !settings.reading_compare)) {
#ifdef CHATTY
		GTimer *t;
		double d;
		t = g_timer_new();
#endif
		RenderWholeBook(imodule);
#ifdef CHATTY
		g_timer_stop(t);
		d = g_timer_elapsed(t, NULL);
		g_timer_destroy(t);
		XI_message(("main render time = %f", d));
#endif
	}
	else
	{
		getVerseBefore(imodule);
		RenderOneChapter(imodule, curChapter);
		getVerseAfter(imodule);
	}

	// Reset the Bible location before GTK gets access:
	// Mouse activity destroys this key, so we must be finished with it.
	key->setTestament(curTest);
	key->setBook(curBook);
	key->setChapter(curChapter);
	key->setVerse(curVerse);

	swbuf.append("</div></font></body></html>");

	/* Native GtkTextView: always jump to the requested verse. The old
	 * WebKit "display_boundary" offset (scroll to verse-N) left the
	 * navbar saying e.g. 9:7 while verse 3 sat at the top. */
	buf = g_strdup_printf("%d", (curChapter * 1000) + curVerse);
	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, buf);
	if (buf)
		g_free(buf);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

//
// display of commentary by chapter.
//
char
DialogEntryDisp::displayByChapter(SWModule &imodule, int columns)
{
	imodule.setSkipConsecutiveLinks(true);
	VerseKey *key = (VerseKey *)(SWKey *) imodule;
	int curVerse = key->getVerse();
	int curChapter = key->getChapter();
	int curBook = key->getBook();
	int curTest = key->getTestament();
	gchar *buf;
	const char *ModuleName = imodule.getName();
	GString *rework; // for image size analysis rework.
	footnote = xref = 0;

	// we come into this routine with swbuf init'd with
	// boilerplate html startup, plus ops and mf ready.

	cache_flags = ConstructFlags(ops);
	is_rtol = main_is_mod_rtol(ModuleName);

	strongs_and_morph = ((ops->strongs || ops->lemmas) &&
			     ops->morphs);
	strongs_or_morph  = ((ops->strongs || ops->lemmas) ||
			     ops->morphs);
	if (strongs_and_morph)
		set_morph_order(imodule);
	set_render_numbers(imodule, ops);

	swbuf.appendFormatted("<div dir=%s>",
			      ((is_rtol && !ops->transliteration)
				   ? "rtl"
				   : "ltr"));

	for (key->setVerse(1);
	     (key->getBook() == curBook) && (key->getChapter() == curChapter) && !imodule.popError();
	     imodule++) {

		ModuleCache::CacheVerse &cVerse =
			ModuleMap[ModuleName][curTest][curBook][curChapter][key->getVerse()];

		// use the module cache rather than re-accessing Sword.
		if (!cVerse.CacheIsValid(cache_flags)) {
			rework = g_string_new(strongs_or_morph
						  ? block_render(imodule.renderText().c_str())
						  : imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
		} else
			rework = g_string_new(cVerse.GetText());

		swbuf.appendFormatted("<p /><a name=\"%d\"> </a>",
				      key->getVerse());
		swbuf.append(settings.imageresize
				 ? AnalyzeForImageSize(rework->str, CURRENT_COLUMNS,
						       GDK_WINDOW(gtk_widget_get_window(gtkText)))
				 : rework->str /* left as-is */);
	}

	swbuf.append("</div></font></body></html>");

	buf = g_strdup_printf("%d", curVerse);
	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, buf);
	g_free(buf);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

char
DialogEntryDisp::display(SWModule &imodule)
{
	swbuf = "";
	char *mod_column_count = NULL;
	mf = get_font(imodule.getName());
	ops = main_new_globals(imodule.getName());
	main_set_global_options(ops);
	GString *rework; // for image size analysis rework.
	footnote = xref = 0;

	imodule.getRawEntry(); // snap to entry

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START
			      "<font face=\"%s\" size=\"%+d\">"
			      "<font color=\"%s\">"
			      "<a href=\"passagestudy.jsp?action=showModInfo&value=%s&module=%s\">"
			      "[*%s*]</a></font><br/>",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      (ops->doublespace ? DOUBLE_SPACE : ""),
			      imodule.getRenderHeader(),
			      (mod_column_count ? mod_column_count : ""),
			      ITALIC_SELECT,
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value,
			      settings.bible_verse_num_color,
			      imodule.getDescription(),
			      imodule.getName(),
			      imodule.getName());
	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	if (!valid_scripture_key) {
		swbuf.append(no_content);
		swbuf.append("</div></font></body></html>");
		HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);
		free_font(mf);
		mf = NULL;
		g_free(ops);
		ops = NULL;
		return 0;
	}

	//
	// the rest of this routine is irrelevant if we are
	// instead heading off to show a whole chapter
	//
	if (ops->commentary_by_chapter)
		return displayByChapter(imodule, CURRENT_COLUMNS);

	if (be->module_type(imodule.getName()) == COMMENTARY_TYPE) {
		VerseKey *key = (VerseKey *)(SWKey *) imodule;
		cache_flags = ConstructFlags(ops);
		const char *ModuleName = imodule.getName();

		ModuleCache::CacheVerse &cVerse = ModuleMap
		    [ModuleName]
		    [key->getTestament()]
		    [key->getBook()]
		    [key->getChapter()]
		    [key->getVerse()];

		// use the module cache rather than re-accessing Sword.
		if (!cVerse.CacheIsValid(cache_flags)) {
			rework = g_string_new(imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
		} else
			rework = g_string_new(cVerse.GetText());

	} else {
		if ((be->module_type(imodule.getName()) == PERCOM_TYPE) ||
		    (be->module_type(imodule.getName()) == PRAYERLIST_TYPE))
			rework = g_string_new(imodule.getRawEntry());
		else
			rework = g_string_new(imodule.renderText().c_str());
		rework = CleanupContent(rework, ops, imodule.getName());
	}

	swbuf.append(settings.imageresize
			 ? AnalyzeForImageSize(rework->str, CURRENT_COLUMNS,
					       GDK_WINDOW(gtk_widget_get_window(gtkText)))
			 : rework->str /* left as-is */);

	swbuf.append("</font></body></html>");

	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

char
DialogChapDisp::display(SWModule &imodule)
{
	imodule.setSkipConsecutiveLinks(true);
	key = (VerseKey *)(SWKey *) imodule;
	curVerse = key->getVerse();
	curChapter = key->getChapter();
	curBook = key->getBook();
	curTest = key->getTestament();

	buf = mod_column_count = NULL;
	GString *rework; // for image size analysis rework.
	const char *ModuleName = imodule.getName();
	ops = main_new_globals(ModuleName);
	cache_flags = ConstructFlags(ops);

	is_rtol = main_is_mod_rtol(ModuleName);
	mf = get_font(ModuleName);

	strongs_and_morph = ((ops->strongs || ops->lemmas) &&
			     ops->morphs);
	strongs_or_morph  = ((ops->strongs || ops->lemmas) ||
			     ops->morphs);
	if (strongs_and_morph)
		set_morph_order(imodule);
	set_render_numbers(imodule, ops);

	// if we are no longer where notes/highlights were current, re-load.
	if (strcasecmp(ModuleName,
		       (note_cache_modname ? note_cache_modname : "")) ||
	    strcasecmp(key->getBookName(), note_cache_book))
		notesCacheFill(ModuleName, (gchar *)key->getShortText());

	gint versestyle = ops->verse_per_line;

	main_set_global_options(ops);

	swbuf = "";
	footnote = xref = 0;

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START
			      "<font face=\"%s\" size=\"%+d\">",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      (strongs_and_morph
				   ? CSS_BLOCK_BOTH
				   : (strongs_or_morph
					  ? CSS_BLOCK_ONE
					  : (ops->doublespace
						 ? DOUBLE_SPACE
						 : ""))),
			      imodule.getRenderHeader(),
			      (mod_column_count ? mod_column_count : ""),
			      ITALIC_SELECT,
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value);

	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	swbuf.appendFormatted("<div dir=%s>",
			      ((is_rtol && !ops->transliteration)
				   ? "rtl"
				   : "ltr"));

	if (!valid_scripture_key) {
		swbuf.append(no_content);
		swbuf.append("</div></font></body></html>");
		HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);
		free_font(mf);
		mf = NULL;
		g_free(ops);
		ops = NULL;
		return 0;
	}

	for (int k = 1; k <= key->getVerseMax(); ++k) {

		key->setVerse(k);

		ModuleCache::CacheVerse &cVerse =
		    ModuleMap[ModuleName][curTest][curBook][curChapter][k];

		// use the module cache rather than re-accessing Sword.
		if (!cVerse.CacheIsValid(cache_flags)) {
			rework = g_string_new(strongs_or_morph
						  ? block_render(imodule.renderText().c_str())
						  : imodule.renderText().c_str());
			rework = CleanupContent(rework, ops, imodule.getName());
			cVerse.SetText(rework->str, cache_flags);
		} else
			rework = g_string_new(cVerse.GetText());

		if (!cVerse.HeaderIsValid())
			CacheHeader(cVerse, imodule, ops, be, false);

		if (cache_flags & ModuleCache::Headings)
			swbuf.append(settings.imageresize
					 ? AnalyzeForImageSize(cVerse.GetHeader(), CURRENT_COLUMNS,
							       GDK_WINDOW(gtk_widget_get_window(gtkText)))
					 : cVerse.GetHeader() /* left as-is */);
		else
			cVerse.InvalidateHeader();

		if (*rework->str == '\0')
			continue;		// no verse content there.

		gchar *num = main_format_number(key->getVerse());
		swbuf.appendFormatted((settings.showversenum
				       ? "&nbsp;<span class=\"word\"><a name=\"%d\" href=\"sword:///%s\">"
				       "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font></a></span>&nbsp;"
				       : "&nbsp;<a name=\"%d\"> </a>"),
				      (curChapter * 1000) + key->getVerse(),
				      (char *)key->getText(),
				      settings.verse_num_font_size + settings.base_font_size,
				      settings.bible_verse_num_color,
				      PRETTYPRINT(num));
		g_free(num);

		append_verse_note_marker(swbuf, (curChapter * 1000) + k,
					 settings.MainWindowModule,
					 (char *)key->getOSISRef());

		// Applies any stored phrase highlights to `rework` and sets
		// color_choices/color_chosen_fg/color_chosen_bg for a
		// whole-verse note, if any (see GTKChapDisp::RenderOneChapter
		// above for the shared apply_verse_notes()).
		apply_verse_notes(rework, (curChapter * 1000) + k);
		if (curVerse == k) {
			/* Current verse: native band after render, not green ink. */
		}

		// ugly ... ugly ... ugly.
		// text containing <p/> in the middle of a <span> or <font> block
		// induces a premature closure of the <span> or <font> content.
		// this has follow-on effects, likely a webkit bug, where such
		// background colorization is re-introduced in psychotic ways beyond
		// the end of the </span> or </font>.
		// solution is ... be still, my wretching stomach ...
		// within any form of background colorization, hunt down all <p/>
		// so as to replace them with <br/><br/>. just keep telling yourself,
		// we do this for fun, we do this for fun, we do this for fun, we do...

		if (color_choices == COLOR_BOTH) {
			for (int i = 0; i < 2; ++i) {
				for (gchar *s = strstr(rework->str, para_endings[i]);
				     s; 
				     s = strstr(s + 1, para_endings[i])) {
					// 4- & 5-char strings.
					(void)g_string_erase(rework, s - rework->str, 4+i);
					(void)g_string_insert(rework, s - rework->str, "<br/><br/>");
				}
			}
		}

		if (color_choices == COLOR_BOTH) {
			swbuf.appendFormatted("<span style=\"background-color: %s\">", color_chosen_bg);
		}

		if (color_choices != COLOR_NONE) {
			swbuf.appendFormatted("<font color=\"%s\">", color_chosen_fg);
		}

		swbuf.append(settings.imageresize
				 ? AnalyzeForImageSize(rework->str, CURRENT_COLUMNS,
						       GDK_WINDOW(gtk_widget_get_window(gtkText)))
				 : rework->str /* left as-is */);

		if (color_choices != COLOR_NONE) {
			swbuf.append("</font>");
			ReadAloud(curVerse, rework->str);
		}

		if (color_choices == COLOR_BOTH)
			swbuf.append("</span>");

		if (versestyle) {
			if ((key->getVerse() != curVerse) ||
			    (!settings.versehighlight &&
			     (color_choices != COLOR_BOTH)))
				swbuf.append("<br/>");
			else if (key->getVerse() == curVerse)
				swbuf.append("<br/>");
		}
	}

	// Reset the Bible location before GTK gets access:
	// Mouse activity destroys this key, so we must be finished with it.
	key->setBook(curBook);
	key->setChapter(curChapter);
	key->setVerse(curVerse);

	swbuf.append("</div></font></body></html>");

	buf = g_strdup_printf("%d", (curChapter * 1000) + curVerse);
	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, buf);
	if (buf)
		g_free(buf);

	free_font(mf);
	mf = NULL;
	g_free(ops);
	ops = NULL;
	return 0;
}

char
GTKPrintEntryDisp::display(SWModule &imodule)
{
	gchar *keytext = NULL;
	gchar *buf, *mod_column_count = NULL;
	SWBuf swbuf = "";
	gint mod_type;
	MOD_FONT *mf = get_font(imodule.getName());

	GLOBAL_OPS *ops = main_new_globals(imodule.getName());

	imodule.getRawEntry(); // snap to entry
	XI_message(("%s", (const char *)imodule.getRawEntry()));
	main_set_global_options(ops);
	mod_type = backend->module_type(imodule.getName());

	if (mod_type == BOOK_TYPE)
		keytext = strdup(backend->treekey_get_local_name(
		    settings.book_offset));
	else if (mod_type == DICTIONARY_TYPE)
		keytext = g_strdup((char *)imodule.getKeyText());
	else
		keytext = strdup((char *)imodule.getKeyText());

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START
			      "<font face=\"%s\" size=\"%+d\">"
			      "<font color=\"%s\">"
			      "<a href=\"passagestudy.jsp?action=showModInfo&value=%s&module=%s\">"
			      "[*%s*]</a></font>[%s]<br/>",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      (ops->doublespace ? DOUBLE_SPACE : ""),
			      imodule.getRenderHeader(),
			      (mod_column_count ? mod_column_count : ""),
			      ITALIC_SELECT,
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value,
			      settings.bible_verse_num_color,
			      imodule.getDescription(),
			      imodule.getName(),
			      imodule.getName(),
			      (gchar *)keytext);
	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	swbuf.append(imodule.renderText());
	swbuf.append("</font></body></html>");

	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);
	free_font(mf);
	g_free(ops);
	if (keytext)
		g_free(keytext);

	return 0;
}

char
GTKPrintChapDisp::display(SWModule &imodule)
{
	imodule.setSkipConsecutiveLinks(true);
	VerseKey *key = (VerseKey *)(SWKey *) imodule;
	int curVerse = key->getVerse();
	int curChapter = key->getChapter();
	int curBook = key->getBook();
	gchar *buf, *mod_column_count = NULL;
	gchar heading[32];
	SWBuf swbuf;

	GLOBAL_OPS *ops = main_new_globals(imodule.getName());
	gboolean is_rtol = main_is_mod_rtol(imodule.getName());
	mf = get_font(imodule.getName());

	swbuf = "";

	if (mf->columns_value != -1) {
		mf->columns_value = CURRENT_COLUMNS;	// restrict [ 1..MAX_COLUMNS ].
		mod_column_count = g_strdup_printf(" body { -webkit-column-count: %d } ", mf->columns_value);
	}

	swbuf.appendFormatted(HTML_START
			      "<font face=\"%s\" size=\"%+d\">",
			      settings.bible_bg_color,
			      settings.bible_text_color,
			      settings.display_columns,
			      JUSTIFY_SELECT, JUSTIFY_SELECT,
			      settings.link_color,
			      (ops->doublespace ? DOUBLE_SPACE : ""),
			      imodule.getRenderHeader(),
			      (mod_column_count ? mod_column_count : ""),
			      ITALIC_SELECT,
			      get_css_references(imodule),
			      ((mf->old_font) ? mf->old_font : ""),
			      mf->old_font_size_value);
	if (mod_column_count)	/* not empty => we created it, so free it. */
		g_free(mod_column_count);

	swbuf.appendFormatted("<div dir=%s>",
			      ((is_rtol && !ops->transliteration)
				   ? "rtl"
				   : "ltr"));

	main_set_global_options(ops);

	for (key->setVerse(1);
	     (key->getBook() == curBook) && (key->getChapter() == curChapter) && !imodule.popError();
	     imodule++) {
		int x = 0;
		gchar *preverse;
		sprintf(heading, "%d", x);

		while ((preverse = backend->get_entry_attribute("Heading", "Preverse",
								heading)) != NULL) {
			SWBuf preverse2 = imodule.renderText(preverse);
			swbuf.appendFormatted("%s", preverse2.c_str());
			g_free(preverse);
			++x;
			sprintf(heading, "%d", x);
		}

		gchar *num = main_format_number(key->getVerse());
		swbuf.appendFormatted(settings.showversenum
				      ? "&nbsp;<a name=\"%d\" href=\"sword:///%s\">"
				      "<font size=\"%+d\" color=\"%s\">%s%s%s%s%s%s%s</font></a>&nbsp;"
				      : "&nbsp;<a name=\"%d\"> </a>",
				      key->getVerse(),
				      (char *)key->getText(),
				      settings.verse_num_font_size + settings.base_font_size,
				      settings.bible_verse_num_color,
				      PRETTYPRINT(num));
		g_free(num);

		buf = g_strdup_printf("%s", imodule.renderText().c_str());

		if (settings.versestyle) {
			swbuf.append("<br/>");
		}
	}

	// Reset the Bible location before GTK gets access:
	// Mouse activity destroys this key, so we must be finished with it.
	key->setBook(curBook);
	key->setChapter(curChapter);
	key->setVerse(curVerse);

	swbuf.append("</div></font></body></html>");

	HtmlOutput((char *)swbuf.c_str(), gtkText, mf, NULL);

	free_font(mf);
	g_free(ops);

	return 0;
}
