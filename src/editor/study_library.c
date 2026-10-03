#include "editor/study_library.h"

#include <errno.h>

#define LIBRARY_FILE "study-library.ini"

static gchar *library_path(const gchar *root)
{
	return g_build_filename(root, LIBRARY_FILE, NULL);
}

static gchar *entry_group(const gchar *path)
{
	gchar *digest = g_compute_checksum_for_string(G_CHECKSUM_SHA256, path, -1);
	gchar *group = g_strdup_printf("study-%s", digest);
	g_free(digest);
	return group;
}

void study_library_entry_clear(StudyLibraryEntry *entry)
{
	if (!entry) return;
	g_clear_pointer(&entry->path, g_free);
	g_clear_pointer(&entry->title, g_free);
	g_clear_pointer(&entry->folder, g_free);
	g_clear_pointer(&entry->tags, g_free);
	entry->favorite = FALSE;
	entry->modified = 0;
}

static void entry_free(StudyLibraryEntry *entry)
{
	study_library_entry_clear(entry);
	g_free(entry);
}

static GKeyFile *load_key_file(const gchar *root, GError **error)
{
	GKeyFile *key = g_key_file_new();
	gchar *path = library_path(root);
	if (g_file_test(path, G_FILE_TEST_EXISTS) &&
	    !g_key_file_load_from_file(key, path, G_KEY_FILE_NONE, error)) {
		g_key_file_unref(key);
		key = NULL;
	}
	g_free(path);
	return key;
}

gboolean study_library_get(const gchar *root, const gchar *path,
			   StudyLibraryEntry *entry, GError **error)
{
	if (!root || !path || !entry) return FALSE;
	study_library_entry_clear(entry);
	entry->path = g_strdup(path);
	entry->title = g_path_get_basename(path);
	entry->folder = g_strdup("");
	entry->tags = g_strdup("");
	GKeyFile *key = load_key_file(root, error);
	if (!key) return FALSE;
	gchar *group = entry_group(path);
	if (g_key_file_has_group(key, group)) {
		g_free(entry->title);
		g_free(entry->folder);
		g_free(entry->tags);
		entry->title = g_key_file_get_string(key, group, "title", NULL);
		entry->folder = g_key_file_get_string(key, group, "folder", NULL);
		entry->tags = g_key_file_get_string(key, group, "tags", NULL);
		entry->favorite = g_key_file_get_boolean(key, group, "favorite", NULL);
		entry->modified = g_key_file_get_int64(key, group, "modified", NULL);
		if (!entry->title) entry->title = g_path_get_basename(path);
		if (!entry->folder) entry->folder = g_strdup("");
		if (!entry->tags) entry->tags = g_strdup("");
	}
	g_free(group);
	g_key_file_unref(key);
	return TRUE;
}

gboolean study_library_put(const gchar *root, const StudyLibraryEntry *entry,
			   GError **error)
{
	if (!root || !entry || !entry->path || !*entry->path) return FALSE;
	if (g_mkdir_with_parents(root, 0700) != 0) {
		g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(errno),
			"No se pudo crear la biblioteca de estudios.");
		return FALSE;
	}
	GKeyFile *key = load_key_file(root, error);
	if (!key) return FALSE;
	gchar *group = entry_group(entry->path);
	g_key_file_set_string(key, group, "path", entry->path);
	g_key_file_set_string(key, group, "title", entry->title ? entry->title : "");
	g_key_file_set_string(key, group, "folder", entry->folder ? entry->folder : "");
	g_key_file_set_string(key, group, "tags", entry->tags ? entry->tags : "");
	g_key_file_set_boolean(key, group, "favorite", entry->favorite);
	g_key_file_set_int64(key, group, "modified", entry->modified ? entry->modified : g_get_real_time());
	gsize length = 0;
	gchar *data = g_key_file_to_data(key, &length, error);
	gchar *path = library_path(root);
	gboolean ok = data && g_file_set_contents(path, data, length, error);
	g_free(path);
	g_free(data);
	g_free(group);
	g_key_file_unref(key);
	return ok;
}

GPtrArray *study_library_list(const gchar *root, GError **error)
{
	GPtrArray *entries = g_ptr_array_new_with_free_func((GDestroyNotify)entry_free);
	GKeyFile *key = load_key_file(root, error);
	if (!key) return entries;
	gsize count = 0;
	gchar **groups = g_key_file_get_groups(key, &count);
	for (gsize i = 0; i < count; ++i) {
		gchar *path = g_key_file_get_string(key, groups[i], "path", NULL);
		if (!path || !g_file_test(path, G_FILE_TEST_EXISTS)) { g_free(path); continue; }
		StudyLibraryEntry *entry = g_new0(StudyLibraryEntry, 1);
		study_library_get(root, path, entry, NULL);
		g_ptr_array_add(entries, entry);
		g_free(path);
	}
	g_strfreev(groups);
	g_key_file_unref(key);
	return entries;
}
