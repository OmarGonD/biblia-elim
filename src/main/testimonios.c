/*
 * Biblia Elim
 * testimonios.c - Jesús en la historia: las fuentes de fuera de la Biblia
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * El contenido no está aquí sino en ui/testimonios-elim.xml, que va
 * dentro del binario como recurso, igual que el diccionario: así se
 * corrige una fecha o se añade una fuente sin tocar una línea de C, y
 * la aplicación sigue funcionando sin red. La prueba de tests/ lee ese
 * mismo archivo del árbol de fuentes con main_testimonios_cargar_archivo().
 *
 * Los originales son de la antigüedad y están en el dominio público; las
 * traducciones al castellano se hicieron para esta aplicación, así que
 * salen con ella bajo la GPL y no arrastran derechos de nadie.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "main/testimonios.h"

#include "gui/debug_glib_null.h"

static GList *grupos = NULL;  /* TestimonioGrupo* */
static gboolean cargado = FALSE;
static guint cuantos = 0;

/* --------------------------------------------------------------------
 * Leer el archivo
 * ------------------------------------------------------------------ */

static gchar *
texto_hijo(xmlNodePtr padre, const char *nombre)
{
	xmlNodePtr n;

	for (n = padre->children; n; n = n->next) {
		if (n->type == XML_ELEMENT_NODE &&
		    !xmlStrcmp(n->name, (const xmlChar *)nombre)) {
			xmlChar *t = xmlNodeGetContent(n);
			gchar *s = t ? g_strstrip(g_strdup((const char *)t))
				     : g_strdup("");
			if (t)
				xmlFree(t);
			return s;
		}
	}
	return g_strdup("");
}

static gchar *
propiedad(xmlNodePtr nodo, const char *nombre)
{
	xmlChar *v = xmlGetProp(nodo, (const xmlChar *)nombre);
	gchar *s = v ? g_strdup((const char *)v) : g_strdup("");

	if (v)
		xmlFree(v);
	return s;
}

static Testimonio *
leer_testimonio(xmlNodePtr nodo)
{
	Testimonio *t = g_new0(Testimonio, 1);

	t->id = propiedad(nodo, "id");
	t->titulo = texto_hijo(nodo, "titulo");
	t->obra = texto_hijo(nodo, "obra");
	t->autor = texto_hijo(nodo, "autor");
	t->fecha = texto_hijo(nodo, "fecha");
	t->postura = texto_hijo(nodo, "postura");
	t->cita = texto_hijo(nodo, "cita");
	t->muestra = texto_hijo(nodo, "muestra");
	t->cautela = texto_hijo(nodo, "cautela");
	t->referencias = texto_hijo(nodo, "referencias");
	return t;
}

static void
testimonio_free(Testimonio *t)
{
	if (!t)
		return;
	g_free(t->id);
	g_free(t->titulo);
	g_free(t->obra);
	g_free(t->autor);
	g_free(t->fecha);
	g_free(t->postura);
	g_free(t->cita);
	g_free(t->muestra);
	g_free(t->cautela);
	g_free(t->referencias);
	g_free(t);
}

static void
grupo_free(TestimonioGrupo *g)
{
	if (!g)
		return;
	g_list_free_full(g->testimonios, (GDestroyNotify)testimonio_free);
	g_free(g->id);
	g_free(g->titulo);
	g_free(g);
}

static void
leer_documento(xmlDocPtr doc)
{
	xmlNodePtr raiz = xmlDocGetRootElement(doc);
	xmlNodePtr n, h;

	if (!raiz)
		return;

	for (n = raiz->children; n; n = n->next) {
		TestimonioGrupo *g;

		if (n->type != XML_ELEMENT_NODE ||
		    xmlStrcmp(n->name, (const xmlChar *)"grupo"))
			continue;

		g = g_new0(TestimonioGrupo, 1);
		g->id = propiedad(n, "id");
		g->titulo = propiedad(n, "titulo");

		for (h = n->children; h; h = h->next) {
			if (h->type != XML_ELEMENT_NODE ||
			    xmlStrcmp(h->name, (const xmlChar *)"testimonio"))
				continue;
			g->testimonios =
			    g_list_append(g->testimonios, leer_testimonio(h));
			cuantos++;
		}
		grupos = g_list_append(grupos, g);
	}
}

void
main_testimonios_init(void)
{
	GBytes *bytes;
	gsize n = 0;
	const char *datos;
	xmlDocPtr doc;

	if (cargado)
		return;
	cargado = TRUE;

	bytes = g_resources_lookup_data("/org/xiphos/ui/testimonios-elim.xml",
					G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (!bytes)
		return;

	datos = (const char *)g_bytes_get_data(bytes, &n);
	doc = xmlReadMemory(datos, (int)n, "testimonios-elim.xml", "UTF-8",
			    XML_PARSE_NOBLANKS);
	if (doc) {
		leer_documento(doc);
		xmlFreeDoc(doc);
	}
	g_bytes_unref(bytes);
}

gboolean
main_testimonios_cargar_archivo(const char *ruta)
{
	xmlDocPtr doc;

	if (cargado || !ruta || !*ruta)
		return FALSE;

	doc = xmlReadFile(ruta, "UTF-8", XML_PARSE_NOBLANKS);
	if (!doc)
		return FALSE;

	cargado = TRUE;
	leer_documento(doc);
	xmlFreeDoc(doc);
	return TRUE;
}

void
main_testimonios_shutdown(void)
{
	g_list_free_full(grupos, (GDestroyNotify)grupo_free);
	grupos = NULL;
	cargado = FALSE;
	cuantos = 0;
}

/* --------------------------------------------------------------------
 * Consultarlo
 * ------------------------------------------------------------------ */

const GList *
main_testimonios_grupos(void)
{
	main_testimonios_init();
	return grupos;
}

guint
main_testimonios_cuantos(void)
{
	main_testimonios_init();
	return cuantos;
}

const Testimonio *
main_testimonios_por_id(const char *id)
{
	const GList *g;

	if (!id || !*id)
		return NULL;
	main_testimonios_init();

	for (g = grupos; g; g = g->next) {
		GList *l;

		for (l = ((TestimonioGrupo *)g->data)->testimonios; l;
		     l = l->next) {
			Testimonio *t = (Testimonio *)l->data;

			if (!g_strcmp0(t->id, id))
				return t;
		}
	}
	return NULL;
}

gchar **
main_testimonios_referencias(const Testimonio *t)
{
	gchar **partes;
	gchar **p;

	if (!t || !t->referencias || !*t->referencias)
		return NULL;

	partes = g_strsplit(t->referencias, ";", -1);
	for (p = partes; p && *p; p++)
		g_strstrip(*p);
	return partes;
}
