/*
 * Biblia Elim
 * testimonios_test.c - que el contenido de «Jesús en la historia» esté entero
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Lo que se rompe en una ficha no es el código: es el XML. Una etiqueta
 * mal cerrada, una fuente sin cautelas, dos ids iguales. Nada de eso da
 * error al compilar y todo se ve fatal en pantalla, así que se comprueba
 * aquí, leyendo el archivo de verdad.
 */

#include <glib.h>

#include "main/testimonios.h"

#define ARCHIVO SRCDIR "/ui/testimonios-elim.xml"

static void
cargar(void)
{
	static gboolean hecho = FALSE;

	if (!hecho) {
		g_assert_true(main_testimonios_cargar_archivo(ARCHIVO));
		hecho = TRUE;
	}
}

static void
prueba_se_lee_el_archivo(void)
{
	cargar();
	g_assert_cmpuint(g_list_length((GList *)main_testimonios_grupos()), >=, 5);
	g_assert_cmpuint(main_testimonios_cuantos(), >=, 15);
}

/* Ninguna ficha puede salir a pantalla sin lo que la sostiene: de dónde
 * sale, qué dice, qué muestra, qué no muestra y dónde leerlo en la
 * Biblia. Si falta una de esas cinco cosas, la ficha no vale. */
static void
prueba_ninguna_ficha_esta_coja(void)
{
	const GList *g;
	GHashTable *ids;

	cargar();
	ids = g_hash_table_new(g_str_hash, g_str_equal);

	for (g = main_testimonios_grupos(); g; g = g->next) {
		TestimonioGrupo *grupo = (TestimonioGrupo *)g->data;
		GList *l;

		g_assert_nonnull(grupo->titulo);
		g_assert_cmpuint(strlen(grupo->titulo), >, 0);
		g_assert_nonnull(grupo->testimonios);

		for (l = grupo->testimonios; l; l = l->next) {
			Testimonio *t = (Testimonio *)l->data;

			g_assert_cmpuint(strlen(t->id), >, 0);
			g_assert_cmpuint(strlen(t->titulo), >, 0);
			g_assert_cmpuint(strlen(t->obra), >, 0);
			g_assert_cmpuint(strlen(t->autor), >, 0);
			g_assert_cmpuint(strlen(t->postura), >, 0);
			g_assert_cmpuint(strlen(t->cita), >, 0);
			g_assert_cmpuint(strlen(t->muestra), >, 0);
			g_assert_cmpuint(strlen(t->cautela), >, 0);
			g_assert_cmpuint(strlen(t->referencias), >, 0);

			/* Dos ids iguales y el menú abriría siempre la
			 * misma ficha, sin decir nada. */
			g_assert_false(g_hash_table_contains(ids, t->id));
			g_hash_table_add(ids, t->id);
		}
	}
	g_hash_table_destroy(ids);
}

static void
prueba_se_busca_por_id(void)
{
	const Testimonio *t;

	cargar();
	t = main_testimonios_por_id("tacito");
	g_assert_nonnull(t);
	g_assert_nonnull(g_strstr_len(t->cita, -1, "Poncio Pilato"));
	g_assert_null(main_testimonios_por_id("no-existe"));
	g_assert_null(main_testimonios_por_id(""));
	g_assert_null(main_testimonios_por_id(NULL));
}

/* Las referencias se convierten en enlaces uno a uno; si una llega vacía
 * o con espacios de más, el enlace sale roto. */
static void
prueba_las_referencias_se_trocean(void)
{
	const Testimonio *t;
	gchar **refs;
	gchar **p;
	int n = 0;

	cargar();
	t = main_testimonios_por_id("josefo-santiago");
	g_assert_nonnull(t);

	refs = main_testimonios_referencias(t);
	g_assert_nonnull(refs);
	for (p = refs; *p; p++, n++) {
		g_assert_cmpuint(strlen(*p), >, 0);
		g_assert_cmpint((*p)[0], !=, ' ');
		g_assert_cmpint((*p)[strlen(*p) - 1], !=, ' ');
	}
	g_assert_cmpint(n, >=, 2);
	g_strfreev(refs);
}

/* Toda ficha tiene que llevar al menos un pasaje que abrir. */
static void
prueba_todas_llevan_a_la_biblia(void)
{
	const GList *g;

	cargar();
	for (g = main_testimonios_grupos(); g; g = g->next) {
		GList *l;

		for (l = ((TestimonioGrupo *)g->data)->testimonios; l;
		     l = l->next) {
			Testimonio *t = (Testimonio *)l->data;
			gchar **refs = main_testimonios_referencias(t);

			g_assert_nonnull(refs);
			g_assert_nonnull(refs[0]);
			g_assert_cmpuint(strlen(refs[0]), >, 0);
			g_strfreev(refs);
		}
	}
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/testimonios/se-lee-el-archivo",
			prueba_se_lee_el_archivo);
	g_test_add_func("/testimonios/ninguna-ficha-esta-coja",
			prueba_ninguna_ficha_esta_coja);
	g_test_add_func("/testimonios/se-busca-por-id", prueba_se_busca_por_id);
	g_test_add_func("/testimonios/referencias-troceadas",
			prueba_las_referencias_se_trocean);
	g_test_add_func("/testimonios/todas-llevan-a-la-biblia",
			prueba_todas_llevan_a_la_biblia);
	return g_test_run();
}
