/*
 * Xiphos Bible Study Tool
 * import_modfile.cc - convert free/unencrypted MySword and theWord Bible
 * modules into SWORD's plain-text .imp import format.
 *
 * Copyright (C) 2026 Xiphos Developer Team
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

#include <cstdio>
#include <cstring>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <sqlite3.h>
#include <versekey.h>

#include "gui/import_modfile.h"

using sword::VerseKey;

/* Standard 66-book OSIS ids, in canonical order; index 0 is unused so
 * that MySword's 1-based Book column can index directly. MySword and
 * theWord both only cover this base canon in their free modules; books
 * beyond 66 (Apocrypha, in some MySword modules) are skipped rather
 * than guessed at. */
static const char *const OSIS_BOOK[67] = {
    NULL,
    "Gen", "Exod", "Lev", "Num", "Deut", "Josh", "Judg", "Ruth", "1Sam", "2Sam",
    "1Kgs", "2Kgs", "1Chr", "2Chr", "Ezra", "Neh", "Esth", "Job", "Ps", "Prov",
    "Eccl", "Song", "Isa", "Jer", "Lam", "Ezek", "Dan", "Hos", "Joel", "Amos",
    "Obad", "Jonah", "Mic", "Nah", "Hab", "Zeph", "Hag", "Zech", "Mal",
    "Matt", "Mark", "Luke", "John", "Acts", "Rom", "1Cor", "2Cor", "Gal", "Eph",
    "Phil", "Col", "1Thess", "2Thess", "1Tim", "2Tim", "Titus", "Phlm", "Heb",
    "Jas", "1Pet", "2Pet", "1John", "2John", "3John", "Jude", "Rev"};

extern "C" gboolean modfile_mysword_to_imp(const gchar *sqlite_path,
					   const gchar *imp_out_path,
					   gchar **out_description,
					   gchar **out_lang,
					   gint *n_verses,
					   GError **error) {
	g_return_val_if_fail(sqlite_path != NULL, FALSE);
	g_return_val_if_fail(imp_out_path != NULL, FALSE);

	if (out_description)
		*out_description = NULL;
	if (out_lang)
		*out_lang = NULL;
	if (n_verses)
		*n_verses = 0;

	sqlite3 *db = NULL;
	if (sqlite3_open_v2(sqlite_path, &db, SQLITE_OPEN_READONLY, NULL) !=
	    SQLITE_OK) {
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    1, _("No se pudo abrir '%s': %s"), sqlite_path,
			    db ? sqlite3_errmsg(db) : _("error desconocido"));
		if (db)
			sqlite3_close(db);
		return FALSE;
	}

	/* Metadata is best-effort: an absent or oddly-named Details table
	 * should not stop the actual verse text from importing. */
	sqlite3_stmt *det_st = NULL;
	if (sqlite3_prepare_v2(db,
			       "SELECT Description, Language FROM Details LIMIT 1",
			       -1, &det_st, NULL) == SQLITE_OK) {
		if (sqlite3_step(det_st) == SQLITE_ROW) {
			const unsigned char *desc = sqlite3_column_text(det_st, 0);
			const unsigned char *lang = sqlite3_column_text(det_st, 1);
			if (out_description && desc && *desc)
				*out_description = g_strdelimit(
				    g_strdup((const gchar *)desc), "\n\r", ' ');
			if (out_lang && lang && *lang)
				*out_lang = g_strdup((const gchar *)lang);
		}
	}
	sqlite3_finalize(det_st);

	sqlite3_stmt *bib_st = NULL;
	if (sqlite3_prepare_v2(db,
			       "SELECT Book, Chapter, Verse, Scripture FROM Bible "
			       "ORDER BY Book, Chapter, Verse",
			       -1, &bib_st, NULL) != SQLITE_OK) {
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    2, _("'%s' no tiene una tabla \"Bible\" legible. "
				 "Puede que no sea un modulo de Biblia de "
				 "MySword, o que este protegido."),
			    sqlite_path);
		sqlite3_close(db);
		return FALSE;
	}

	FILE *out = g_fopen(imp_out_path, "wb");
	if (!out) {
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    3, _("No se pudo crear el archivo temporal '%s'."),
			    imp_out_path);
		sqlite3_finalize(bib_st);
		sqlite3_close(db);
		return FALSE;
	}

	gint written = 0;
	while (sqlite3_step(bib_st) == SQLITE_ROW) {
		int book = sqlite3_column_int(bib_st, 0);
		int chapter = sqlite3_column_int(bib_st, 1);
		int verse = sqlite3_column_int(bib_st, 2);
		const unsigned char *scripture = sqlite3_column_text(bib_st, 3);

		if (book < 1 || book > 66 || !scripture)
			continue;

		fprintf(out, "$$$%s.%d.%d\n%s\n\n", OSIS_BOOK[book], chapter,
			verse, (const char *)scripture);
		written++;
	}
	sqlite3_finalize(bib_st);
	sqlite3_close(db);
	fclose(out);

	if (written == 0) {
		g_unlink(imp_out_path);
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    4, _("No se encontro texto biblico legible en '%s'."),
			    sqlite_path);
		return FALSE;
	}

	if (n_verses)
		*n_verses = written;
	return TRUE;
}

extern "C" gboolean modfile_theword_to_imp(const gchar *src_path,
					   const gchar *start_osis_ref,
					   const gchar *imp_out_path,
					   gint *n_verses,
					   GError **error) {
	g_return_val_if_fail(src_path != NULL, FALSE);
	g_return_val_if_fail(start_osis_ref != NULL, FALSE);
	g_return_val_if_fail(imp_out_path != NULL, FALSE);

	if (n_verses)
		*n_verses = 0;

	gchar *raw = NULL;
	gsize raw_len = 0;
	GError *read_err = NULL;
	if (!g_file_get_contents(src_path, &raw, &raw_len, &read_err)) {
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    1, _("No se pudo leer '%s': %s"), src_path,
			    read_err ? read_err->message : _("error desconocido"));
		if (read_err)
			g_error_free(read_err);
		return FALSE;
	}

	/* Older theWord modules are often Windows-1252/Latin-1 rather than
	 * UTF-8; fall back to that before giving up. */
	gchar *text;
	if (g_utf8_validate(raw, (gssize)raw_len, NULL)) {
		text = raw;
	} else {
		GError *conv_err = NULL;
		text = g_convert(raw, (gssize)raw_len, "UTF-8", "WINDOWS-1252",
				 NULL, NULL, &conv_err);
		g_free(raw);
		if (!text) {
			g_set_error(error, g_quark_from_static_string("modfile-import"),
				    2, _("'%s' no esta en UTF-8 ni en una "
					 "codificacion reconocida: %s"),
				    src_path,
				    conv_err ? conv_err->message : _("error desconocido"));
			if (conv_err)
				g_error_free(conv_err);
			return FALSE;
		}
	}

	gchar **lines = g_strsplit(text, "\n", -1);
	g_free(text);

	gint n_lines = (gint)g_strv_length(lines);
	/* a trailing newline at EOF produces one bogus empty final line */
	if (n_lines > 0 && !*lines[n_lines - 1])
		n_lines--;

	if (n_lines == 0) {
		g_strfreev(lines);
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    3, _("'%s' esta vacio."), src_path);
		return FALSE;
	}

	FILE *out = g_fopen(imp_out_path, "wb");
	if (!out) {
		g_strfreev(lines);
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    4, _("No se pudo crear el archivo temporal '%s'."),
			    imp_out_path);
		return FALSE;
	}

	/* theWord's plain Bible modules carry no per-line reference at all:
	 * position in the file IS the reference, in strict KJV verse order.
	 * Rather than hand-maintain a 31,102-entry canon table, walk
	 * libsword's own VerseKey the same number of steps - it already
	 * knows this order authoritatively. */
	VerseKey vk(start_osis_ref);
	gint written = 0;
	for (gint i = 0; i < n_lines; i++) {
		gchar *line = lines[i];
		gsize l = strlen(line);
		if (l > 0 && line[l - 1] == '\r')
			line[l - 1] = '\0';

		const char *ref = vk.getOSISRef();
		if (!ref || !*ref)
			break;

		fprintf(out, "$$$%s\n%s\n\n", ref, line);
		written++;

		if (i + 1 < n_lines) {
			vk.increment(1);
			if (vk.popError())
				break;
		}
	}
	fclose(out);
	g_strfreev(lines);

	if (written == 0) {
		g_unlink(imp_out_path);
		g_set_error(error, g_quark_from_static_string("modfile-import"),
			    5, _("No se pudo convertir ningun versiculo de '%s'."),
			    src_path);
		return FALSE;
	}

	if (n_verses)
		*n_verses = written;
	return TRUE;
}
