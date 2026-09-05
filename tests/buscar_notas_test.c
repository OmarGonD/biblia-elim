/*
 * Biblia Elim
 * buscar_notas_test.c - buscar en las notas propias
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Lo que se prueba aquí es lo que decide qué sale y qué se enseña de
 * cada nota; leer las notas del XML y pintarlas es de otros.
 *
 *   cmake --build build --target buscar_notas_test
 *   ./build/tests/buscar_notas_test
 */

#include <glib.h>
#include <string.h>

#include "main/buscar_notas.h"

static BN_NOTA notas[] = {
    {"SpaRVG", "Eph.2.1", "MV:Eph.2.1", NULL,
     "Estábamos muertos, no enfermos. El Espíritu es quien da vida."},
    {"SpaRVG", "Eph.2.8", "HL:1700", "por gracia sois salvos",
     "Apóstol Pablo: la fe tampoco es mérito nuestro."},
    {"SpaRV", "Rom.6.23", "MV:Rom.6.23", NULL,
     "La paga del pecado.\nContrastar con la dádiva de Dios."},
    {"SpaRVG", "John.3.16", "MV:John.3.16", NULL,
     "De tal manera amó Dios al mundo: el amor no es sentimiento."},
};

static GList *
lista(void)
{
	GList *l = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS(notas); ++i)
		l = g_list_append(l, &notas[i]);
	return l;
}

static BN_RESULTADO *
nesimo(GList *r, guint i)
{
	return (BN_RESULTADO *)g_list_nth_data(r, i);
}

/* Lo escrito con tilde se encuentra sin ella, y al revés: es lo que hace
 * la búsqueda del lector y lo que espera cualquiera que teclee deprisa. */
static void
prueba_texto_sin_tildes(void)
{
	GList *notas_l = lista();
	GList *r = main_buscar_notas(notas_l, "espiritu", BN_TEXTO, FALSE, NULL);

	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_cmpstr(nesimo(r, 0)->osisref, ==, "Eph.2.1");
	main_buscar_notas_libre(r);

	r = main_buscar_notas(notas_l, "APÓSTOL", BN_TEXTO, FALSE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_cmpstr(nesimo(r, 0)->osisref, ==, "Eph.2.8");
	main_buscar_notas_libre(r);

	g_list_free(notas_l);
}

/* Distinguiendo mayúsculas, «dios» ya no encuentra «Dios». */
static void
prueba_distinguir_mayusculas(void)
{
	GList *notas_l = lista();
	GList *r = main_buscar_notas(notas_l, "dios", BN_TEXTO, TRUE, NULL);

	g_assert_cmpint(g_list_length(r), ==, 0);
	main_buscar_notas_libre(r);

	r = main_buscar_notas(notas_l, "Dios", BN_TEXTO, TRUE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 2);
	main_buscar_notas_libre(r);

	g_list_free(notas_l);
}

/* La expresión regular hace lo que dice: anclas, alternativas y límites
 * de palabra. */
static void
prueba_regex(void)
{
	GList *notas_l = lista();
	GList *r;

	/* Solo al principio de la nota. */
	r = main_buscar_notas(notas_l, "^Apóstol", BN_REGEX, FALSE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_cmpstr(nesimo(r, 0)->osisref, ==, "Eph.2.8");
	main_buscar_notas_libre(r);

	/* Dos palabras de una vez. */
	r = main_buscar_notas(notas_l, "muertos|pecado", BN_REGEX, FALSE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 2);
	main_buscar_notas_libre(r);

	/* «fe» como palabra suelta: no vale la de «enfermos». */
	r = main_buscar_notas(notas_l, "\\bfe\\b", BN_REGEX, FALSE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_cmpstr(nesimo(r, 0)->osisref, ==, "Eph.2.8");
	main_buscar_notas_libre(r);

	g_list_free(notas_l);
}

/* Una expresión mal escrita es una errata de quien la escribe, no un
 * fallo: se devuelve el motivo para poder enseñarlo. */
static void
prueba_regex_rota(void)
{
	GList *notas_l = lista();
	GError *error = NULL;
	GList *r = main_buscar_notas(notas_l, "gracia(", BN_REGEX, FALSE,
				     &error);

	g_assert_null(r);
	g_assert_nonnull(error);
	g_assert_cmpuint(strlen(error->message), >, 0);
	g_error_free(error);
	g_list_free(notas_l);
}

/* Lo subrayado también es del lector: si la palabra está ahí y no en la
 * nota, la fila sale, y dice que salió por eso. */
static void
prueba_encuentra_en_la_frase(void)
{
	GList *notas_l = lista();
	GList *r = main_buscar_notas(notas_l, "sois salvos", BN_TEXTO, FALSE,
				     NULL);

	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_true(nesimo(r, 0)->en_frase);
	g_assert_nonnull(strstr(nesimo(r, 0)->extracto, "sois salvos"));
	main_buscar_notas_libre(r);
	g_list_free(notas_l);
}

/* El extracto enseña lo hallado, marca dónde cae y no parte el UTF-8. */
static void
prueba_extracto(void)
{
	GList *notas_l = lista();
	GList *r = main_buscar_notas(notas_l, "dádiva", BN_TEXTO, FALSE, NULL);
	BN_RESULTADO *uno;
	gchar *marcado;

	g_assert_cmpint(g_list_length(r), ==, 1);
	uno = nesimo(r, 0);

	g_assert_true(g_utf8_validate(uno->extracto, -1, NULL));
	/* El salto de línea de la nota no parte el renglón. */
	g_assert_null(strchr(uno->extracto, '\n'));

	g_assert_cmpint(uno->ini, >=, 0);
	g_assert_cmpint(uno->fin, >, uno->ini);
	g_assert_cmpint(uno->fin, <=, (gint)strlen(uno->extracto));

	marcado = g_strndup(uno->extracto + uno->ini, uno->fin - uno->ini);
	g_assert_cmpstr(marcado, ==, "dádiva");
	g_free(marcado);

	main_buscar_notas_libre(r);
	g_list_free(notas_l);
}

/* Una nota larguísima se recorta alrededor de lo hallado, con puntos
 * suspensivos, y lo hallado sigue estando dentro. */
static void
prueba_extracto_recorta(void)
{
	GString *larga = g_string_new(NULL);
	BN_NOTA una;
	GList *notas_l = NULL;
	GList *r;
	BN_RESULTADO *uno;
	gchar *marcado;
	int i;

	for (i = 0; i < 40; ++i)
		g_string_append(larga, "palabras de relleno ");
	g_string_append(larga, "misericordia");
	for (i = 0; i < 40; ++i)
		g_string_append(larga, " y más relleno todavía");

	una.modulo = "SpaRVG";
	una.osisref = "Ps.51.1";
	una.note_key = "MV:Ps.51.1";
	una.frase = NULL;
	una.nota = larga->str;
	notas_l = g_list_append(NULL, &una);

	r = main_buscar_notas(notas_l, "misericordia", BN_TEXTO, FALSE, NULL);
	g_assert_cmpint(g_list_length(r), ==, 1);
	uno = nesimo(r, 0);

	g_assert_cmpint((gint)g_utf8_strlen(uno->extracto, -1), <, 200);
	g_assert_true(g_str_has_prefix(uno->extracto, "…"));
	g_assert_true(g_str_has_suffix(uno->extracto, "…"));

	marcado = g_strndup(uno->extracto + uno->ini, uno->fin - uno->ini);
	g_assert_cmpstr(marcado, ==, "misericordia");
	g_free(marcado);

	main_buscar_notas_libre(r);
	g_list_free(notas_l);
	g_string_free(larga, TRUE);
}

/* Cuántas veces aparece en una misma nota. */
static void
prueba_cuenta_repeticiones(void)
{
	BN_NOTA una = {"SpaRVG", "Ps.23.1", "MV:Ps.23.1", NULL,
		       "pastor, pastor y pastor"};
	GList *notas_l = g_list_append(NULL, &una);
	GList *r = main_buscar_notas(notas_l, "pastor", BN_TEXTO, FALSE, NULL);

	g_assert_cmpint(g_list_length(r), ==, 1);
	g_assert_cmpint(nesimo(r, 0)->cuantas, ==, 3);
	main_buscar_notas_libre(r);
	g_list_free(notas_l);
}

/* Sin nada escrito no se busca: enseñar las notas enteras al abrir el
 * cuadro es cosa de quien lo pinta, no de aquí. */
static void
prueba_consulta_vacia(void)
{
	GList *notas_l = lista();

	g_assert_null(main_buscar_notas(notas_l, "", BN_TEXTO, FALSE, NULL));
	g_assert_null(main_buscar_notas(notas_l, NULL, BN_TEXTO, FALSE, NULL));
	g_assert_null(main_buscar_notas(NULL, "gracia", BN_TEXTO, FALSE, NULL));
	g_list_free(notas_l);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/notas/texto-sin-tildes", prueba_texto_sin_tildes);
	g_test_add_func("/notas/distinguir-mayusculas",
			prueba_distinguir_mayusculas);
	g_test_add_func("/notas/regex", prueba_regex);
	g_test_add_func("/notas/regex-rota", prueba_regex_rota);
	g_test_add_func("/notas/encuentra-en-la-frase",
			prueba_encuentra_en_la_frase);
	g_test_add_func("/notas/extracto", prueba_extracto);
	g_test_add_func("/notas/extracto-recorta", prueba_extracto_recorta);
	g_test_add_func("/notas/cuenta-repeticiones",
			prueba_cuenta_repeticiones);
	g_test_add_func("/notas/consulta-vacia", prueba_consulta_vacia);
	return g_test_run();
}
