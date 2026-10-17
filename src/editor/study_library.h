#ifndef XIPHOS_STUDY_LIBRARY_H
#define XIPHOS_STUDY_LIBRARY_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	gchar *path;
	gchar *title;
	gchar *folder;
	gchar *tags;
	gboolean favorite;
	gint64 modified;
} StudyLibraryEntry;

void study_library_entry_clear(StudyLibraryEntry *entry);
gboolean study_library_get(const gchar *root, const gchar *path,
			   StudyLibraryEntry *entry, GError **error);
gboolean study_library_put(const gchar *root, const StudyLibraryEntry *entry,
			   GError **error);
GPtrArray *study_library_list(const gchar *root, GError **error);

/* Version snapshots are stored in the user's app data directory, keyed by
 * the document path. The returned array owns its path strings and is newest
 * first. */
gboolean study_history_snapshot(const gchar *path, GError **error);
GPtrArray *study_history_list(const gchar *path);

G_END_DECLS
#endif
