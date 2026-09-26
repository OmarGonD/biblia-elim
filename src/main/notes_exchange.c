/*
 * Biblia Elim
 * notes_exchange.c - exportar e importar las notas (ver notes_exchange.h)
 */
#include "main/notes_exchange.h"

#include <string.h>

#include <glib/gi18n.h>
#include <json-glib/json-glib.h>

#include "main/note_value.h"
#include "main/notes_store.h"

static void member_string_or_null(JsonBuilder *b, const gchar *name,
				  const gchar *value)
{
	json_builder_set_member_name(b, name);
	if (value)
		json_builder_add_string_value(b, value);
	else
		json_builder_add_null_value(b);
}

static void add_dates(JsonBuilder *b, const NotesEntry *e)
{
	gchar *c = notes_time_to_iso(e->created), *m = notes_time_to_iso(e->modified);
	member_string_or_null(b, "created", c);
	member_string_or_null(b, "modified", m);
	g_free(c);
	g_free(m);
}

gchar *notes_export_json(void)
{
	JsonBuilder *b = json_builder_new();
	JsonGenerator *gen;
	JsonNode *root;
	GPtrArray *entries;
	gchar *now = notes_time_to_iso(g_get_real_time() / G_USEC_PER_SEC);
	gchar *out;

	json_builder_begin_object(b);
	json_builder_set_member_name(b, "format");
	json_builder_add_string_value(b, NOTES_EXCHANGE_FORMAT);
	json_builder_set_member_name(b, "version");
	json_builder_add_int_value(b, NOTES_EXCHANGE_VERSION);
	member_string_or_null(b, "exported", now);

	json_builder_set_member_name(b, "notes");
	json_builder_begin_array(b);
	entries = notes_store_entries(NOTES_SECTION_NOTES);
	for (guint i = 0; i < entries->len; i++) {
		NotesEntry *e = g_ptr_array_index(entries, i);
		gchar *mod, *osis, *id, *color = NULL, *text = NULL, *note = NULL;
		gint pos = -1;

		if (!note_label_split(e->label, &mod, &osis, &id))
			continue; /* no es una nota (marca interna) */
		json_builder_begin_object(b);
		member_string_or_null(b, "label", e->label);
		member_string_or_null(b, "value", e->value);
		add_dates(b, e);
		/* Lo mismo, legible; al importar no se usa. */
		member_string_or_null(b, "module", mod);
		member_string_or_null(b, "osisref", osis);
		if (decode_note_value(e->value, &color, &text, &note, &pos)) {
			member_string_or_null(b, "text", (text && *text) ? text : NULL);
			member_string_or_null(b, "note", note);
			member_string_or_null(b, "color", color);
		}
		json_builder_end_object(b);
		g_free(mod);
		g_free(osis);
		g_free(id);
		g_free(color);
		g_free(text);
		g_free(note);
	}
	g_ptr_array_unref(entries);
	json_builder_end_array(b);

	json_builder_set_member_name(b, "links");
	json_builder_begin_array(b);
	entries = notes_store_entries(NOTES_SECTION_LINKS);
	for (guint i = 0; i < entries->len; i++) {
		NotesEntry *e = g_ptr_array_index(entries, i);
		json_builder_begin_object(b);
		member_string_or_null(b, "label", e->label);
		member_string_or_null(b, "value", e->value);
		add_dates(b, e);
		json_builder_end_object(b);
	}
	g_ptr_array_unref(entries);
	json_builder_end_array(b);
	json_builder_end_object(b);

	root = json_builder_get_root(b);
	gen = json_generator_new();
	json_generator_set_pretty(gen, TRUE);
	json_generator_set_root(gen, root);
	out = json_generator_to_data(gen, NULL);
	json_node_unref(root);
	g_object_unref(gen);
	g_object_unref(b);
	g_free(now);
	return out;
}

/* ------------------------------------------------------------------ */

typedef struct {
	gchar *label, *value;
	gint64 created, modified;
} Incoming;

static void incoming_free(gpointer p)
{
	Incoming *in = p;
	g_free(in->label);
	g_free(in->value);
	g_free(in);
}

static const gchar *member_string(JsonObject *o, const gchar *name)
{
	JsonNode *n = json_object_get_member(o, name);
	if (!n || !JSON_NODE_HOLDS_VALUE(n) ||
	    json_node_get_value_type(n) != G_TYPE_STRING)
		return NULL;
	return json_node_get_string(n);
}

/* Las entradas de `name` que tienen etiqueta y valor; `valid_note` exige
 * además la forma de una nota. */
static GPtrArray *read_entries(JsonObject *root, const gchar *name,
			       gboolean valid_note, guint *invalid)
{
	GPtrArray *out = g_ptr_array_new_with_free_func(incoming_free);
	JsonNode *n = json_object_get_member(root, name);
	JsonArray *arr;

	if (!n || !JSON_NODE_HOLDS_ARRAY(n))
		return out;
	arr = json_node_get_array(n);
	for (guint i = 0; i < json_array_get_length(arr); i++) {
		JsonNode *item = json_array_get_element(arr, i);
		JsonObject *o;
		const gchar *label, *value;

		if (!JSON_NODE_HOLDS_OBJECT(item)) {
			(*invalid)++;
			continue;
		}
		o = json_node_get_object(item);
		label = member_string(o, "label");
		value = member_string(o, "value");
		if (!label || !value || !*label) {
			(*invalid)++;
			continue;
		}
		if (valid_note) {
			gchar *mod, *osis, *id, *color = NULL, *text = NULL,
					       *note = NULL;
			gint pos;
			gboolean ok = note_label_split(label, &mod, &osis, &id) &&
				      strchr(osis, '.') &&
				      decode_note_value(value, &color, &text,
							&note, &pos) &&
				      text && note;
			g_free(mod);
			g_free(osis);
			g_free(id);
			g_free(color);
			g_free(text);
			g_free(note);
			if (!ok) {
				(*invalid)++;
				continue;
			}
		} else if (!strchr(value, '|')) {
			(*invalid)++;
			continue;
		}
		Incoming *in = g_new0(Incoming, 1);
		in->label = g_strdup(label);
		in->value = g_strdup(value);
		in->created = notes_time_from_iso(member_string(o, "created"));
		in->modified = notes_time_from_iso(member_string(o, "modified"));
		g_ptr_array_add(out, in);
	}
	return out;
}

static gboolean is_whole_verse(const gchar *value)
{
	gchar *color = NULL, *text = NULL, *note = NULL;
	gint pos;
	gboolean whole = decode_note_value(value, &color, &text, &note, &pos) &&
			 text && !*text;
	g_free(color);
	g_free(text);
	g_free(note);
	return whole;
}

/* La misma nota ya está en ese versículo de esa versión, con esta u otra
 * etiqueta: importar dos veces la misma copia no duplica nada. */
static gboolean same_note_exists(const gchar *label, const gchar *value)
{
	gchar *mod, *osis, *id;
	GPtrArray *entries;
	gboolean found = FALSE;

	if (!note_label_split(label, &mod, &osis, &id))
		return FALSE;
	entries = notes_store_entries(NOTES_SECTION_NOTES);
	for (guint i = 0; i < entries->len && !found; i++) {
		NotesEntry *e = g_ptr_array_index(entries, i);
		gchar *m2, *o2, *i2;
		if (strcmp(e->value, value) ||
		    !note_label_split(e->label, &m2, &o2, &i2))
			continue;
		found = !strcmp(m2, mod) && !strcmp(o2, osis);
		g_free(m2);
		g_free(o2);
		g_free(i2);
	}
	g_ptr_array_unref(entries);
	g_free(mod);
	g_free(osis);
	g_free(id);
	return found;
}

/* Una etiqueta libre para otra nota del mismo versículo: la misma forma
 * que usa highlight_add_verse_note(), «#MV<número>». */
static gchar *extra_label(const gchar *label)
{
	gchar *mod, *osis, *id, *out = NULL;
	gint64 base = g_get_real_time();

	if (!note_label_split(label, &mod, &osis, &id))
		return NULL;
	for (gint64 k = 0; !out; k++) {
		gchar *cand = g_strdup_printf("%s %s#MV%" G_GINT64_FORMAT, mod,
					      osis, base + k);
		gchar *there = notes_store_get(NOTES_SECTION_NOTES, cand);
		if (!there)
			out = cand;
		else
			g_free(cand);
		g_free(there);
	}
	g_free(mod);
	g_free(osis);
	g_free(id);
	return out;
}

gboolean notes_import_json(const gchar *json, NotesImportResult *result,
			   GError **error)
{
	JsonParser *parser;
	JsonNode *root;
	JsonObject *obj;
	const gchar *format;
	JsonNode *version;
	GPtrArray *notes, *links;
	guint invalid = 0;

	memset(result, 0, sizeof(*result));
	if (!notes_store_is_open()) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
			    _("Las notas no están abiertas."));
		return FALSE;
	}
	parser = json_parser_new();
	if (!json || !json_parser_load_from_data(parser, json, -1, error)) {
		if (error && !*error)
			g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
				    _("El archivo está vacío."));
		g_object_unref(parser);
		return FALSE;
	}
	root = json_parser_get_root(parser);
	obj = (root && JSON_NODE_HOLDS_OBJECT(root)) ? json_node_get_object(root)
						     : NULL;
	format = obj ? member_string(obj, "format") : NULL;
	version = obj ? json_object_get_member(obj, "version") : NULL;
	if (!format || strcmp(format, NOTES_EXCHANGE_FORMAT)) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
			    _("No es una copia de notas de Biblia Elim."));
		g_object_unref(parser);
		return FALSE;
	}
	if (!version || !JSON_NODE_HOLDS_VALUE(version) ||
	    json_node_get_value_type(version) != G_TYPE_INT64 ||
	    json_node_get_int(version) < 1 ||
	    json_node_get_int(version) > NOTES_EXCHANGE_VERSION) {
		g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
			    _("La copia es de una versión de Biblia Elim más "
			      "nueva que esta."));
		g_object_unref(parser);
		return FALSE;
	}

	/* Todo leído y comprobado antes de tocar el almacén. */
	notes = read_entries(obj, "notes", TRUE, &invalid);
	links = read_entries(obj, "links", FALSE, &invalid);
	g_object_unref(parser);

	for (guint i = 0; i < notes->len; i++) {
		Incoming *in = g_ptr_array_index(notes, i);
		gchar *here = notes_store_get(NOTES_SECTION_NOTES, in->label);
		if ((!here || strcmp(here, in->value)) &&
		    is_whole_verse(in->value) &&
		    same_note_exists(in->label, in->value)) {
			result->identical++;
		} else if (!here) {
			notes_store_set_full(NOTES_SECTION_NOTES, in->label,
					     in->value, in->created, in->modified);
			result->added++;
		} else if (!strcmp(here, in->value)) {
			result->identical++;
		} else if (is_whole_verse(in->value)) {
			/* Dos notas distintas sobre el mismo versículo: las
			 * dos se quedan. */
			gchar *label = extra_label(in->label);
			if (label) {
				notes_store_set_full(NOTES_SECTION_NOTES, label,
						     in->value, in->created,
						     in->modified);
				result->extra++;
			}
			g_free(label);
		} else {
			result->conflicts++;
		}
		g_free(here);
	}
	for (guint i = 0; i < links->len; i++) {
		Incoming *in = g_ptr_array_index(links, i);
		gchar *here = notes_store_get(NOTES_SECTION_LINKS, in->label);
		if (!here) {
			notes_store_set_full(NOTES_SECTION_LINKS, in->label,
					     in->value, in->created, in->modified);
			result->links_added++;
		} else if (strcmp(here, in->value)) {
			result->conflicts++;
		}
		g_free(here);
	}
	result->invalid = invalid;
	g_ptr_array_unref(notes);
	g_ptr_array_unref(links);
	return TRUE;
}

/* ------------------------------------------------------------------ */

/* Una cita en Markdown: cada renglón con su «> ». */
static void append_quote(GString *md, const gchar *text)
{
	gchar **lines = g_strsplit(text, "\n", -1);
	for (gchar **l = lines; *l; l++)
		g_string_append_printf(md, ">%s%s\n", **l ? " " : "", *l);
	g_strfreev(lines);
}

gchar *notes_export_markdown(GList *items, const gchar *fecha)
{
	GString *md = g_string_new(NULL);
	const gchar *libro = NULL;
	guint n = g_list_length(items);

	g_string_append_printf(md, "# %s\n\n", _("Mis notas"));
	g_string_append_printf(
	    md, ngettext("Exportadas de Biblia Elim el %s · %u nota.\n",
			 "Exportadas de Biblia Elim el %s · %u notas.\n", n),
	    fecha ? fecha : "", n);

	for (GList *l = items; l; l = l->next) {
		const NotesMdItem *it = l->data;
		if (!libro || g_strcmp0(libro, it->libro)) {
			libro = it->libro;
			g_string_append_printf(md, "\n## %s\n", libro ? libro : "");
		}
		g_string_append_printf(md, "\n### %s", it->pasaje ? it->pasaje : "");
		if (it->modulo && *it->modulo)
			g_string_append_printf(md, " (%s)", it->modulo);
		g_string_append(md, "\n\n");
		if (it->frase && *it->frase) {
			gchar *q = g_strdup_printf("«%s»", it->frase);
			append_quote(md, q);
			g_string_append(md, "\n");
			g_free(q);
		}
		if (it->nota && *it->nota)
			g_string_append_printf(md, "%s\n", it->nota);
		if (it->fechas && *it->fechas)
			g_string_append_printf(md, "\n*%s*\n", it->fechas);
	}
	return g_string_free(md, FALSE);
}
