/*
 * Xiphos Bible Study Tool
 * import_modfile.h - convert free/unencrypted MySword and theWord Bible
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

#ifndef __IMPORT_MODFILE_H__
#define __IMPORT_MODFILE_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * Name
 *   modfile_mysword_to_imp
 *
 * Synopsis
 *   #include "gui/import_modfile.h"
 *
 *   gboolean modfile_mysword_to_imp(const gchar *sqlite_path,
 *                                   const gchar *imp_out_path,
 *                                   gchar **out_description,
 *                                   gchar **out_lang,
 *                                   gint *n_verses,
 *                                   GError **error)
 *
 * Description
 *   Reads the "Bible" table of a MySword .bbl.mybible module (a plain
 *   SQLite database: Book/Chapter/Verse/Scripture columns, 1-66 book
 *   numbering) and writes it out as a SWORD .imp file, ready for
 *   imp2vs. A module that fails to open as SQLite, or whose Bible
 *   table can't be read, is reported as an error rather than guessed
 *   at - this covers modules that are encrypted/locked to MySword
 *   itself, which this function will not attempt to unlock.
 *
 * Return value
 *   TRUE on success (imp_out_path is written; *out_description,
 *   *out_lang, *n_verses set when available); FALSE on failure (error
 *   is set, if provided).
 */
gboolean modfile_mysword_to_imp(const gchar *sqlite_path,
				const gchar *imp_out_path,
				gchar **out_description,
				gchar **out_lang,
				gint *n_verses,
				GError **error);

/******************************************************************************
 * Name
 *   modfile_theword_to_imp
 *
 * Synopsis
 *   #include "gui/import_modfile.h"
 *
 *   gboolean modfile_theword_to_imp(const gchar *src_path,
 *                                   const gchar *start_osis_ref,
 *                                   const gchar *imp_out_path,
 *                                   gint *n_verses,
 *                                   GError **error)
 *
 * Description
 *   Reads a theWord plain-text Bible module (.ot/.nt/.ont - never the
 *   encrypted .otx/.ntx/.ontx variants, which the caller should refuse
 *   before calling this) and writes it out as a SWORD .imp file. These
 *   files carry no per-line reference: line position alone is the
 *   verse, in strict KJV canonical order starting at start_osis_ref
 *   ("Gen.1.1" for .ot/.ont, "Matt.1.1" for .nt). Rather than a
 *   hand-maintained 31,102-entry canon table, this walks libsword's
 *   own VerseKey the same number of steps as there are lines, so the
 *   ordering is exactly what SWORD itself considers canonical.
 *
 * Return value
 *   TRUE on success (imp_out_path is written, *n_verses set); FALSE on
 *   failure (error is set, if provided).
 */
gboolean modfile_theword_to_imp(const gchar *src_path,
				const gchar *start_osis_ref,
				const gchar *imp_out_path,
				gint *n_verses,
				GError **error);

#ifdef __cplusplus
}
#endif

#endif /* __IMPORT_MODFILE_H__ */
