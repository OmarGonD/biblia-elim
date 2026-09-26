/*
 * Biblia Elim
 * notes_store.c - almacén propio de las notas del lector (NOTES-STORE-101)
 *
 * Formato de notas.xml:
 *
 *   <notas version="1">
 *     <note label="SpaRV Ps.23.1" value="..."
 *           created="2026-09-25T18:04:05Z" modified="..."/>
 *     <notelink label="..." value="..."/>
 *   </notas>
 */
#include "main/notes_store.h"

#include <string.h>

#include <glib/gstdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

typedef struct {
	GPtrArray *entries;	/* NotesEntry*, orden de inserción */
	GHashTable *index;	/* label -> NotesEntry* (no dueño) */
} Section;

static gchar *store_path = NULL;
static Section sections[2];
static gboolean rotated_this_session = FALSE;
static guint64 generation = 1;

static const char *const ELEMENT[2] = {"note", "notelink"};

static void entry_free(gpointer data)
{
	NotesEntry *e = data;
	if (!e)
		return;
	g_free(e->label);
	g_free(e->value);
	g_free(e);
}

void notes_entry_free(NotesEntry *entry)
{
	entry_free(entry);
}

static NotesEntry *entry_copy(const NotesEntry *e)
{
	NotesEntry *copy = g_new0(NotesEntry, 1);
	copy->label = g_strdup(e->label);
	copy->value = g_strdup(e->value);
	copy->created = e->created;
	copy->modified = e->modified;
	return copy;
}

static gint64 now_seconds(void)
{
	return g_get_real_time() / G_USEC_PER_SEC;
}

/* ISO 8601 en UTC, «2026-09-25T18:04:05Z»; NULL para 0. */
gchar *notes_time_to_iso(gint64 t)
{
	GDateTime *dt;
	gchar *out;
	if (t <= 0)
		return NULL;
	dt = g_date_time_new_from_unix_utc(t);
	if (!dt)
		return NULL;
	out = g_date_time_format(dt, "%Y-%m-%dT%H:%M:%SZ");
	g_date_time_unref(dt);
	return out;
}

gint64 notes_time_from_iso(const gchar *text)
{
	GDateTime *dt;
	gint64 t;
	if (!text || !*text)
		return 0;
	dt = g_date_time_new_from_iso8601(text, NULL);
	if (!dt)
		return 0;
	t = g_date_time_to_unix(dt);
	g_date_time_unref(dt);
	return t > 0 ? t : 0;
}

static void section_reset(Section *s)
{
	if (s->index)
		g_hash_table_destroy(s->index);
	if (s->entries)
		g_ptr_array_unref(s->entries);
	s->entries = g_ptr_array_new_with_free_func(entry_free);
	s->index = g_hash_table_new(g_str_hash, g_str_equal);
}

static void section_put(Section *s, const gchar *label, const gchar *value,
			gint64 created, gint64 modified)
{
	NotesEntry *e = g_hash_table_lookup(s->index, label);
	if (e) {
		g_free(e->value);
		e->value = g_strdup(value ? value : "");
		e->created = created;
		e->modified = modified;
		return;
	}
	e = g_new0(NotesEntry, 1);
	e->label = g_strdup(label);
	e->value = g_strdup(value ? value : "");
	e->created = created;
	e->modified = modified;
	g_ptr_array_add(s->entries, e);
	g_hash_table_insert(s->index, e->label, e);
}

void notes_store_close(void)
{
	for (int i = 0; i < 2; i++) {
		if (sections[i].index)
			g_hash_table_destroy(sections[i].index);
		if (sections[i].entries)
			g_ptr_array_unref(sections[i].entries);
		sections[i].index = NULL;
		sections[i].entries = NULL;
	}
	g_free(store_path);
	store_path = NULL;
	rotated_this_session = FALSE;
	generation++;
}

gboolean notes_store_is_open(void)
{
	return store_path != NULL;
}

const gchar *notes_store_path(void)
{
	return store_path;
}

/* Un archivo que existe y no se entiende no se pisa nunca. */
static void set_aside_unreadable(const gchar *path)
{
	GDateTime *now = g_date_time_new_now_local();
	gchar *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
	gchar *aside = g_strdup_printf("%s.ilegible-%s", path, stamp);
	if (g_rename(path, aside) != 0)
		g_warning("notas: no se pudo apartar %s", path);
	else
		g_warning("notas: %s no se pudo leer; apartado como %s", path,
			  aside);
	g_free(aside);
	g_free(stamp);
	g_date_time_unref(now);
}

gboolean notes_store_open(const gchar *path)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;

	notes_store_close();
	store_path = g_strdup(path);
	section_reset(&sections[NOTES_SECTION_NOTES]);
	section_reset(&sections[NOTES_SECTION_LINKS]);

	if (!g_file_test(path, G_FILE_TEST_EXISTS))
		return FALSE;
	doc = xmlReadFile(path, "UTF-8", XML_PARSE_NONET | XML_PARSE_NOBLANKS |
			  XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
	root = doc ? xmlDocGetRootElement(doc) : NULL;
	if (!root || xmlStrcmp(root->name, (const xmlChar *)"notas") != 0) {
		if (doc)
			xmlFreeDoc(doc);
		set_aside_unreadable(path);
		return FALSE;
	}
	for (node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE)
			continue;
		for (int i = 0; i < 2; i++) {
			if (xmlStrcmp(node->name, (const xmlChar *)ELEMENT[i]))
				continue;
			xmlChar *label = xmlGetProp(node, (const xmlChar *)"label");
			xmlChar *value = xmlGetProp(node, (const xmlChar *)"value");
			xmlChar *created = xmlGetProp(node, (const xmlChar *)"created");
			xmlChar *modified = xmlGetProp(node, (const xmlChar *)"modified");
			if (label)
				section_put(&sections[i], (const gchar *)label,
					    (const gchar *)value,
					    notes_time_from_iso((const gchar *)created),
					    notes_time_from_iso((const gchar *)modified));
			xmlFree(label);
			xmlFree(value);
			xmlFree(created);
			xmlFree(modified);
		}
	}
	xmlFreeDoc(doc);
	return TRUE;
}

gchar *notes_store_get(NotesSection section, const gchar *label)
{
	NotesEntry *e;
	if (!store_path || !label)
		return NULL;
	e = g_hash_table_lookup(sections[section].index, label);
	return e ? g_strdup(e->value) : NULL;
}

void notes_store_set(NotesSection section, const gchar *label,
		     const gchar *value)
{
	NotesEntry *e;
	gint64 now = now_seconds();
	if (!store_path || !label)
		return;
	e = g_hash_table_lookup(sections[section].index, label);
	if (e && !g_strcmp0(e->value, value ? value : ""))
		return;
	section_put(&sections[section], label, value, e ? e->created : now,
		    now);
	generation++;
}

void notes_store_set_full(NotesSection section, const gchar *label,
			  const gchar *value, gint64 created,
			  gint64 modified)
{
	if (!store_path || !label)
		return;
	section_put(&sections[section], label, value, MAX(created, 0),
		    MAX(modified, 0));
	generation++;
}

NotesEntry *notes_store_lookup(NotesSection section, const gchar *label)
{
	NotesEntry *e;
	if (!store_path || !label)
		return NULL;
	e = g_hash_table_lookup(sections[section].index, label);
	return e ? entry_copy(e) : NULL;
}

guint64 notes_store_generation(void)
{
	return generation;
}

void notes_store_remove(NotesSection section, const gchar *label)
{
	Section *s = &sections[section];
	NotesEntry *e;
	if (!store_path || !label)
		return;
	e = g_hash_table_lookup(s->index, label);
	if (!e)
		return;
	g_hash_table_remove(s->index, label);
	g_ptr_array_remove(s->entries, e);
	generation++;
}

GPtrArray *notes_store_entries(NotesSection section)
{
	GPtrArray *out = g_ptr_array_new_with_free_func(entry_free);
	Section *s = &sections[section];
	if (!store_path)
		return out;
	for (guint i = 0; i < s->entries->len; i++) {
		g_ptr_array_add(out, entry_copy(g_ptr_array_index(s->entries, i)));
	}
	return out;
}

guint notes_store_count(NotesSection section)
{
	return store_path ? sections[section].entries->len : 0;
}

/* notas.xml.bak.1 es la copia más reciente; la más antigua se descarta. */
static void rotate_backups(void)
{
	gchar *contents = NULL;
	gsize length = 0;

	if (!g_file_get_contents(store_path, &contents, &length, NULL))
		return;
	for (int i = NOTES_STORE_BACKUPS - 1; i >= 1; i--) {
		gchar *from = g_strdup_printf("%s.bak.%d", store_path, i);
		gchar *to = g_strdup_printf("%s.bak.%d", store_path, i + 1);
		if (g_file_test(from, G_FILE_TEST_EXISTS))
			(void)g_rename(from, to);
		g_free(from);
		g_free(to);
	}
	gchar *first = g_strdup_printf("%s.bak.1", store_path);
	if (!g_file_set_contents(first, contents, (gssize)length, NULL))
		g_warning("notas: no se pudo escribir la copia %s", first);
	g_free(first);
	g_free(contents);
}

gboolean notes_store_copy_to(const gchar *dest)
{
	gchar *contents = NULL;
	gsize length = 0;
	gboolean ok;
	if (!store_path || !dest ||
	    !g_file_get_contents(store_path, &contents, &length, NULL))
		return FALSE;
	ok = g_file_set_contents_full(dest, contents, (gssize)length,
				      G_FILE_SET_CONTENTS_CONSISTENT |
					  G_FILE_SET_CONTENTS_DURABLE,
				      0644, NULL);
	g_free(contents);
	return ok;
}

gboolean notes_store_save(void)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *mem = NULL;
	int size = 0;
	GError *error = NULL;
	gboolean ok;

	if (!store_path)
		return FALSE;
	if (!rotated_this_session) {
		rotate_backups();
		rotated_this_session = TRUE;
	}
	doc = xmlNewDoc((const xmlChar *)"1.0");
	root = xmlNewNode(NULL, (const xmlChar *)"notas");
	xmlNewProp(root, (const xmlChar *)"version", (const xmlChar *)"1");
	xmlDocSetRootElement(doc, root);
	for (int i = 0; i < 2; i++) {
		Section *s = &sections[i];
		for (guint k = 0; k < s->entries->len; k++) {
			NotesEntry *e = g_ptr_array_index(s->entries, k);
			xmlNodePtr n = xmlNewChild(root, NULL,
						   (const xmlChar *)ELEMENT[i], NULL);
			xmlNewProp(n, (const xmlChar *)"label",
				   (const xmlChar *)e->label);
			xmlNewProp(n, (const xmlChar *)"value",
				   (const xmlChar *)e->value);
			for (int t = 0; t < 2; t++) {
				gchar *when = notes_time_to_iso(t ? e->modified : e->created);
				if (when)
					xmlNewProp(n, (const xmlChar *)(t ? "modified" : "created"),
						   (const xmlChar *)when);
				g_free(when);
			}
		}
	}
	xmlDocDumpFormatMemoryEnc(doc, &mem, &size, "UTF-8", 1);
	xmlFreeDoc(doc);
	ok = g_file_set_contents_full(
	    store_path, (const gchar *)mem, size,
	    G_FILE_SET_CONTENTS_CONSISTENT | G_FILE_SET_CONTENTS_DURABLE,
	    0644, &error);
	xmlFree(mem);
	if (!ok) {
		g_warning("notas: no se pudo guardar %s: %s", store_path,
			  error ? error->message : "?");
		g_clear_error(&error);
	}
	return ok;
}
