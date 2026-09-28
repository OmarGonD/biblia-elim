/*
 * Xiphos Bible Study Tool
 * export_passage.cc -
 *
 * Copyright (C) 2008-2026 Xiphos Developer Team
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

#include <gtk/gtk.h>
#include <swmgr.h>
#include <swmodule.h>

#include "main/backend_access.h"
#include "main/strong_interaction.h"

#include "gui/export_dialog.h"

#include "main/export_passage.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/global_ops.hh"

#define HTML_START "<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" /><style type=\"text/css\"><!-- A { text-decoration:none } *[dir=rtl] { text-align: right; } .transChangeSupplied { font-style: italic; } --></style></head><body>"

int main_get_max_verses(const char *name)
{
    BibleKeyInfo info;
    return name && main_backend_for(name).resolveKey(name, settings.currentverse, info)
        ? info.verseCount : 1;
}

int main_get_current_verse(const char *name)
{
    BibleKeyInfo info;
    return name && main_backend_for(name).resolveKey(name, settings.currentverse, info)
        ? info.reference.verse : 1;
}

/**
 * _copy_to_clipboard:
 * @text: #gchar text to copy
 * @len: #gint length (unused now)
 *
 * Copies the data to the clipboard as HTML if applicable (that is, HTML data,
 * so that smart word processors such as OO.o will preserve markup; at this point,
 * the HTML is *not* preserving the font.
 **/
static void _copy_to_clipboard(EXPORT_DATA data, char *text, int len)
{
	/* HTML where the other side takes it, the same text otherwise */
	GdkClipboard *clipboard =
	    gdk_display_get_clipboard(gdk_display_get_default());
	GBytes *html = g_bytes_new(text, strlen(text));
	GdkContentProvider *providers[] = {
		gdk_content_provider_new_for_bytes("text/html", html),
		gdk_content_provider_new_typed(G_TYPE_STRING, text),
	};
	GdkContentProvider *provider =
	    gdk_content_provider_new_union(providers, G_N_ELEMENTS(providers));

	gdk_clipboard_set_content(clipboard, provider);
	g_object_unref(provider);
	g_bytes_unref(html);
	if (data.bookheader)
		g_free(data.bookheader);
	if (data.chapterheader_book)
		g_free(data.chapterheader_book);
	if (data.chapterheader_chapter)
		g_free(data.chapterheader_chapter);
	if (data.versenumber)
		g_free(data.versenumber);
	if (data.verselayout_single_verse_ref_last)
		g_free(data.verselayout_single_verse_ref_last);
	if (data.verselayout_single_verse_ref_first)
		g_free(data.verselayout_single_verse_ref_first);
	if (data.verse_range_verse)
		g_free(data.verse_range_verse);
}

static void _save(EXPORT_DATA data, char *text, int len)
{
	XI_message(("%s", data.filename));
	g_file_set_contents(data.filename, text, len, NULL);
	if (data.filename)
		g_free(data.filename);
	if (data.bookheader)
		g_free(data.bookheader);
	if (data.chapterheader_book)
		g_free(data.chapterheader_book);
	if (data.chapterheader_chapter)
		g_free(data.chapterheader_chapter);
	if (data.versenumber)
		g_free(data.versenumber);
	if (data.verselayout_single_verse_ref_last)
		g_free(data.verselayout_single_verse_ref_last);
	if (data.verselayout_single_verse_ref_first)
		g_free(data.verselayout_single_verse_ref_first);
	if (data.verse_range_verse)
		g_free(data.verse_range_verse);
}

static std::string export_text(BibleBackend &reader, const char *module,
                               const BibleVerse &verse, bool html)
{
    auto content = reader.getVerseContent(module, verse.reference, true);
    if (!html) return content.plainText;
    if (content.renderedText != content.plainText) return content.renderedText;
    content.footnotes.clear(); content.crossReferences.clear();
    std::string result;
    for (const auto &heading : content.headings) {
        gchar *safe = g_markup_escape_text(heading.text.c_str(), -1);
        result += "<h3>" + std::string(safe) + "</h3>"; g_free(safe);
    }
    return result + renderAnnotatedVerseText(content, module, verse.key, false);
}

void main_export_content(EXPORT_DATA data, gint format)
{
    const bool html = format != 0;
    const char *module = settings.MainWindowModule;
    if (!module || !settings.currentverse) return;
    BibleBackend &reader = main_backend_for(module);
    BibleKeyInfo current;
    if (!reader.resolveKey(module, settings.currentverse, current)) return;
    // Keep export marker policy; neutral annotations are removed explicitly.
    _set_global_textual("Cross-references", "Off");
    _set_global_textual("Footnotes", "Off");
    const std::string description = reader.moduleDescription(module);
    const std::string version = data.version ? " [" + std::string(module) + "]" : "";
    GString *out = g_string_new(nullptr);
    const int passage = data.passage_type;
    std::vector<BibleKeyInfo> books;
    if (passage == BIBLE) {
        if (html) g_string_append_printf(out, "%s<h1>%s</h1>", HTML_START,
                                        data.version ? description.c_str() : "");
        else g_string_append_printf(out, "%s\n\n", data.version ? description.c_str() : "");
        for (int testament = 1; testament <= 2; ++testament) {
            const auto names = reader.bookNames(module, testament);
            for (std::size_t n = 0; n < names.size(); ++n) {
                BibleKeyInfo book;
                if (reader.resolveKey(module, reader.setBook(module, "", testament, n + 1), book)) books.push_back(book);
            }
        }
    } else books.push_back(current);
    for (const auto &book : books) {
        if (passage == BIBLE)
            g_string_append_printf(out, html ? "<h2>%s</h2>" : "\n%s\n", book.bookName.c_str());
        if (passage == BOOK) {
            if (html) g_string_append_printf(out, data.bookheader, HTML_START, data.version ? description.c_str() : "", 1);
            else g_string_append_printf(out, data.plain_bookheader, description.c_str(), 1);
        }
        if (passage == CHAPTER) {
            if (html) g_string_append_printf(out, data.chapterheader_chapter, HTML_START,
                data.version ? description.c_str() : "", book.bookName.c_str(), current.reference.chapter);
            else g_string_append_printf(out, data.plain_chapterheader_chapter, description.c_str(),
                book.bookName.c_str(), current.reference.chapter);
        }
        if (passage == VERSE_RANGE) {
            if (html) g_string_append(out, HTML_START);
            if (!data.reference_last)
                g_string_append_printf(out, html ? data.verse_range_ref_first : data.plain_verse_range_ref_first,
                    book.bookName.c_str(), current.reference.chapter, data.start_verse, data.end_verse, version.c_str());
        }
        int first = (passage == BIBLE || passage == BOOK) ? 1 : current.reference.chapter;
        int last = (passage == BIBLE || passage == BOOK) ? book.chapterCount : first;
        for (int chapter = first; chapter <= last; ++chapter) {
            if (passage == BIBLE || (passage == BOOK && chapter > 1))
                g_string_append_printf(out, html ? data.chapterheader_book : data.plain_chapterheader_book, chapter);
            BibleReference ref = book.reference; ref.chapter = chapter;
            for (const auto &verse : reader.getChapter(module, ref, false)) {
                const int number = verse.reference.verse;
                if (passage == VERSE && number != current.reference.verse) continue;
                if (passage == VERSE_RANGE && (number < data.start_verse || number > data.end_verse)) continue;
                const std::string text = export_text(reader, module, verse, html);
                if (passage == VERSE) {
                    if (html && data.reference_last)
                        g_string_append_printf(out, data.verselayout_single_verse_ref_last, HTML_START,
                            text.c_str(), book.bookName.c_str(), chapter, number, version.c_str());
                    else if (html)
                        g_string_append_printf(out, data.verselayout_single_verse_ref_first, HTML_START,
                            book.bookName.c_str(), chapter, number, version.c_str(), text.c_str());
                    else if (data.reference_last)
                        g_string_append_printf(out, data.plain_verselayout_single_verse_ref_last,
                            text.c_str(), book.bookName.c_str(), chapter, number, version.c_str());
                    else
                        g_string_append_printf(out, data.plain_verselayout_single_verse_ref_first,
                            book.bookName.c_str(), chapter, number, version.c_str(), text.c_str());
                } else {
                    if (data.verse_num)
                        g_string_append_printf(out, html ? data.versenumber : data.plain_versenumber, number);
                    const char *separator = settings.versestyle ? (html ? "<br>" : "\n") : " ";
                    g_string_append_printf(out, passage == VERSE_RANGE ? data.verse_range_verse : " %s%s",
                                          text.c_str(), separator);
                }
            }
        }
        if (passage == VERSE_RANGE && data.reference_last)
            g_string_append_printf(out, html ? data.verse_range_ref_last : data.plain_verse_range_ref_last,
                html ? "<br>" : "\n", book.bookName.c_str(), current.reference.chapter,
                data.start_verse, data.end_verse, version.c_str());
    }
    if (html && passage != VERSE) g_string_append(out, "</body></html>");
    if (data.filename) _save(data, out->str, out->len);
    else _copy_to_clipboard(data, out->str, out->len);
    g_string_free(out, TRUE);
}
