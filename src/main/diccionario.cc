/*
 * Biblia Elim — diccionario / léxico offline
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "gui/utilities.h"
#include "main/diccionario.h"
#include "main/lists.h"
#include "main/nube_palabras.h"
#include "main/settings.h"
#include "main/sword.h"

static GList *entradas = NULL;
static gboolean loaded = FALSE;

static gchar *
xml_child_text(xmlNodePtr parent, const char *name)
{
	for (xmlNodePtr n = parent->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp(n->name, (const xmlChar *)name)) {
			xmlChar *t = xmlNodeGetContent(n);
			gchar *s = t ? g_strstrip(g_strdup((const char *)t)) : g_strdup("");
			if (t)
				xmlFree(t);
			return s;
		}
	}
	return g_strdup("");
}

static void
entrada_free(DiccEntrada *e)
{
	if (!e)
		return;
	g_free(e->titulo);
	g_free(e->definicion);
	g_free(e->referencias);
	g_strfreev(e->claves);
	for (GList *l = e->estudios; l; l = l->next) {
		DiccEstudio *es = (DiccEstudio *)l->data;
		g_free(es->autor);
		g_free(es->titulo);
		g_free(es->texto);
		g_free(es);
	}
	g_list_free(e->estudios);
	g_free(e);
}

static DiccEntrada *
parse_entrada(xmlNodePtr node)
{
	DiccEntrada *e = g_new0(DiccEntrada, 1);
	xmlChar *claves = xmlGetProp(node, (const xmlChar *)"claves");
	if (claves) {
		e->claves = g_strsplit((const char *)claves, ",", -1);
		for (gchar **p = e->claves; p && *p; p++)
			g_strstrip(*p);
		xmlFree(claves);
	}
	e->titulo = xml_child_text(node, "titulo");
	e->definicion = xml_child_text(node, "definicion");
	e->referencias = xml_child_text(node, "referencias");
	for (xmlNodePtr n = node->children; n; n = n->next) {
		if (n->type != XML_ELEMENT_NODE ||
		    xmlStrcmp(n->name, (const xmlChar *)"estudio"))
			continue;
		DiccEstudio *es = g_new0(DiccEstudio, 1);
		xmlChar *autor = xmlGetProp(n, (const xmlChar *)"autor");
		xmlChar *titulo = xmlGetProp(n, (const xmlChar *)"titulo");
		xmlChar *texto = xmlNodeGetContent(n);
		es->autor = autor ? g_strdup((const char *)autor) : g_strdup(_("Estudio"));
		es->titulo = titulo ? g_strdup((const char *)titulo) : g_strdup("");
		es->texto = texto ? g_strstrip(g_strdup((const char *)texto)) : g_strdup("");
		if (autor)
			xmlFree(autor);
		if (titulo)
			xmlFree(titulo);
		if (texto)
			xmlFree(texto);
		e->estudios = g_list_append(e->estudios, es);
	}
	return e;
}

static void
load_from_doc(xmlDocPtr doc)
{
	xmlNodePtr root = xmlDocGetRootElement(doc);
	if (!root)
		return;
	for (xmlNodePtr n = root->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp(n->name, (const xmlChar *)"entrada"))
			entradas = g_list_append(entradas, parse_entrada(n));
	}
}

void
main_diccionario_init(void)
{
	if (loaded)
		return;
	loaded = TRUE;

	GBytes *bytes = g_resources_lookup_data("/org/xiphos/ui/diccionario-elim.xml",
						G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (bytes) {
		gsize n = 0;
		const char *data = (const char *)g_bytes_get_data(bytes, &n);
		xmlDocPtr doc = xmlReadMemory(data, (int)n, "diccionario-elim.xml",
					      "UTF-8", XML_PARSE_NOBLANKS);
		if (doc) {
			load_from_doc(doc);
			xmlFreeDoc(doc);
		}
		g_bytes_unref(bytes);
		return;
	}

	gchar *path = gui_general_user_file("diccionario-elim.xml", FALSE);
	if (!path)
		return;
	xmlDocPtr doc = xmlReadFile(path, "UTF-8", XML_PARSE_NOBLANKS);
	g_free(path);
	if (!doc)
		return;
	load_from_doc(doc);
	xmlFreeDoc(doc);
}

void
main_diccionario_shutdown(void)
{
	g_list_free_full(entradas, (GDestroyNotify)entrada_free);
	entradas = NULL;
	loaded = FALSE;
}

static gboolean
entrada_coincide(const DiccEntrada *e, const char *norm)
{
	if (!e || !norm || !*norm)
		return FALSE;
	gchar *t = main_nube_normalizar(e->titulo);
	gboolean ok = (t && strstr(t, norm) != NULL);
	g_free(t);
	if (ok)
		return TRUE;
	if (!e->claves)
		return FALSE;
	for (gchar **p = e->claves; *p; p++) {
		gchar *c = main_nube_normalizar(*p);
		ok = (c && (strcmp(c, norm) == 0 || strstr(c, norm) || strstr(norm, c)));
		g_free(c);
		if (ok)
			return TRUE;
	}
	return FALSE;
}

GList *
main_diccionario_sugerencias(const char *texto)
{
	main_diccionario_init();
	GList *out = NULL;
	gchar *norm = main_nube_normalizar(texto ? texto : "");
	for (GList *l = entradas; l; l = l->next) {
		DiccEntrada *e = (DiccEntrada *)l->data;
		if (!norm[0] || entrada_coincide(e, norm))
			out = g_list_append(out, g_strdup(e->titulo));
	}
	g_free(norm);
	return out;
}

const DiccEntrada *
main_diccionario_buscar(const char *palabra)
{
	main_diccionario_init();
	if (!palabra || !*palabra)
		return NULL;
	gchar *norm = main_nube_normalizar(palabra);
	const DiccEntrada *exact = NULL;
	const DiccEntrada *parcial = NULL;
	for (GList *l = entradas; l; l = l->next) {
		DiccEntrada *e = (DiccEntrada *)l->data;
		gchar *t = main_nube_normalizar(e->titulo);
		if (t && strcmp(t, norm) == 0) {
			g_free(t);
			exact = e;
			break;
		}
		g_free(t);
		if (e->claves) {
			for (gchar **p = e->claves; *p; p++) {
				gchar *c = main_nube_normalizar(*p);
				if (c && strcmp(c, norm) == 0) {
					g_free(c);
					exact = e;
					goto done;
				}
				g_free(c);
			}
		}
		if (!parcial && entrada_coincide(e, norm))
			parcial = e;
	}
done:
	g_free(norm);
	return exact ? exact : parcial;
}

static gchar *
primeras_lineas(const char *texto, int max)
{
	if (!texto)
		return g_strdup("");
	gchar *s = g_strstrip(g_strdup(texto));
	if ((int)strlen(s) <= max)
		return s;
	gchar *cut = g_strndup(s, max);
	g_free(s);
	gchar *out = g_strconcat(cut, "…", NULL);
	g_free(cut);
	return out;
}

GList *
main_diccionario_comentarios(const char *clave_versiculo)
{
	GList *out = NULL;
	GList *mods = get_list(COMM_LIST);
	const char *key = (clave_versiculo && *clave_versiculo)
			      ? clave_versiculo
			      : settings.currentverse;
	if (!key)
		return NULL;

	for (GList *l = mods; l; l = l->next) {
		const char *mod = (const char *)l->data;
		if (!mod || !main_is_module((char *)mod))
			continue;
		char *raw = main_get_striptext((char *)mod, (char *)key);
		if (!raw || !*raw || strspn(raw, " \t\n\r") == strlen(raw)) {
			g_free(raw);
			continue;
		}
		DiccComentario *c = g_new0(DiccComentario, 1);
		char *author = main_get_mod_config_entry(mod, "Author");
		const char *desc = main_get_module_description(mod);
		c->modulo = g_strdup(mod);
		c->autor = (author && *author) ? g_strdup(author)
					       : g_strdup(desc ? desc : mod);
		c->descripcion = g_strdup(desc ? desc : mod);
		c->extracto = primeras_lineas(raw, 280);
		g_free(author);
		g_free(raw);
		out = g_list_append(out, c);
	}
	return out;
}

void
main_diccionario_comentarios_free(GList *lista)
{
	for (GList *l = lista; l; l = l->next) {
		DiccComentario *c = (DiccComentario *)l->data;
		g_free(c->autor);
		g_free(c->modulo);
		g_free(c->descripcion);
		g_free(c->extracto);
		g_free(c);
	}
	g_list_free(lista);
}
