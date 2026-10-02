/*
 * Biblia Elim
 * interl_enriq_test.c - ficha de estudio enriquecida del interlineal
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <glib.h>
#include <glib/gstdio.h>

#include "main/interl_enriq.h"

#define COMPLETA                                                              \
	"{\"ref\":\"John.1.2\",\"strong\":\"G4314\",\"glosa_interlineal\":\"con\"," \
	"\"rango_semantico\":[\"hacia\",\"con, junto a\"],"                      \
	"\"construccion\":\"πρός + acusativo\","                                 \
	"\"sentido_en_contexto\":\"Cercanía <y> relación.\","                    \
	"\"matiz\":null,"                                                        \
	"\"traducciones_comparadas\":["                                          \
	"{\"version\":\"RVR1960\",\"texto\":\"con Dios\"},"                      \
	"{\"version\":\"Torres Amat\",\"texto\":\"en Dios\",\"nota\":\"apud\"},"  \
	"{\"version\":\"sin texto\"}],"                                          \
	"\"notas_traduccion\":null,"                                             \
	"\"otros_usos\":[\"Jn 14:6\"],\"nivel_certeza\":\"alto\"}"

static gchar *
escribir(const char *json)
{
	gchar *ruta = g_build_filename(g_get_tmp_dir(), "interl_enriq_test.json", NULL);

	g_assert_true(g_file_set_contents(ruta, json, -1, NULL));
	return ruta;
}

static void
prueba_ficha_completa(void)
{
	gchar *ruta = escribir("[" COMPLETA "]");
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	h = main_interl_enriq_html("John.1.2", "G4314");
	g_assert_nonnull(h);
	g_assert_nonnull(g_strstr_len(h, -1, "πρός + acusativo"));
	g_assert_nonnull(g_strstr_len(h, -1, "Cercanía &lt;y&gt; relación."));
	g_assert_nonnull(g_strstr_len(h, -1, "<b>Torres Amat:</b> en Dios"));
	g_assert_nonnull(g_strstr_len(h, -1, "(apud)"));
	g_assert_nonnull(g_strstr_len(h, -1, "Jn 14:6"));
	/* null se omite y una versión sin texto no se inventa. */
	g_assert_null(g_strstr_len(h, -1, "Matiz"));
	g_assert_null(g_strstr_len(h, -1, "sin texto"));
	g_assert_null(g_strstr_len(h, -1, "Sobre las diferencias"));
	g_free(h);
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

/* Palabra frecuente: solo construcción y rango; el resto, nada. */
static void
prueba_ficha_minima(void)
{
	gchar *ruta = escribir("[{\"ref\":\"John.1.1\",\"strong\":\"G3588\","
			       "\"rango_semantico\":[\"el\"],\"construccion\":null,"
			       "\"sentido_en_contexto\":null}]");
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	h = main_interl_enriq_html("John.1.1", "G3588");
	g_assert_nonnull(h);
	g_assert_null(g_strstr_len(h, -1, "En este versículo"));
	g_free(h);
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

static void
prueba_sin_ficha_y_entradas_invalidas(void)
{
	gchar *ruta = escribir("[{\"ref\":\"John.1.1\"},3,"
			       "{\"ref\":\"John.1.3\",\"strong\":\"G1\"}]");

	g_assert_null(main_interl_enriq_html("John.1.1", "G1"));
	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	g_assert_null(main_interl_enriq_html("John.1.1", "G1"));
	/* Sin campos útiles tampoco hay ficha. */
	g_assert_null(main_interl_enriq_html("John.1.3", "G1"));
	g_assert_null(main_interl_enriq_html(NULL, "G1"));
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

static void
prueba_archivo_malo_no_borra_lo_cargado(void)
{
	gchar *ok = escribir("[" COMPLETA "]");
	gchar *mal;
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ok), ==, 1);
	mal = g_build_filename(g_get_tmp_dir(), "interl_enriq_mal.json", NULL);
	g_assert_true(g_file_set_contents(mal, "{no es json", -1, NULL));
	g_assert_cmpint(main_interl_enriq_cargar(mal), ==, -1);
	g_assert_cmpint(main_interl_enriq_cargar("/no/existe.json"), ==, -1);
	g_assert_cmpint(main_interl_enriq_cargar_de("/no/existe"), ==, 0);
	h = main_interl_enriq_html("John.1.2", "G4314");
	g_assert_nonnull(h);
	g_free(h);
	main_interl_enriq_liberar();
	g_remove(ok);
	g_remove(mal);
	g_free(ok);
	g_free(mal);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/interl-enriq/completa", prueba_ficha_completa);
	g_test_add_func("/interl-enriq/minima", prueba_ficha_minima);
	g_test_add_func("/interl-enriq/invalidas", prueba_sin_ficha_y_entradas_invalidas);
	g_test_add_func("/interl-enriq/archivo-malo", prueba_archivo_malo_no_borra_lo_cargado);
	return g_test_run();
}
