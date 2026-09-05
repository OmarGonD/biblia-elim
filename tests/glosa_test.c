/*
 * Biblia Elim
 * glosa_test.c - la ficha del término, sin repetirse
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <glib.h>

#include "main/glosa.h"

#define ENTRADA                                                            \
	"Vencido. Hacer peor, es decir, vencer (literal o figurativamente) " \
	"En Reina-Valera 1909: habéis sido menos; vencido; son vencidos."

static void
prueba_quita_la_glosa_repetida(void)
{
	gchar *d = main_glosa_definicion(ENTRADA, "vencido");

	g_assert_cmpstr(d, ==,
			"Hacer peor, es decir, vencer (literal o figurativamente)");
	g_free(d);
}

static void
prueba_saca_la_reina_valera(void)
{
	gchar *rv = main_glosa_rv1909(ENTRADA);

	g_assert_cmpstr(rv, ==, "habéis sido menos; vencido; son vencidos");
	g_free(rv);
	g_assert_null(main_glosa_rv1909("Sin coletilla."));
	g_assert_null(main_glosa_rv1909(NULL));
}

/* Cuando la definición es solo la glosa, no se queda vacía. */
static void
prueba_definicion_minima(void)
{
	gchar *d = main_glosa_definicion("Dueño.", "dueño");

	g_assert_cmpstr(d, ==, "Dueño.");
	g_free(d);
}

/* Sin glosa, la definición se devuelve entera (menos la coletilla). */
static void
prueba_sin_glosa(void)
{
	gchar *d = main_glosa_definicion("Blando. En Reina-Valera 1909: tierna.",
					 "");

	g_assert_cmpstr(d, ==, "Blando.");
	g_free(d);
	d = main_glosa_definicion(NULL, NULL);
	g_assert_cmpstr(d, ==, "");
	g_free(d);
}

/* Mayúsculas y acentos: la glosa va en minúscula y la definición no. */
static void
prueba_mayusculas(void)
{
	gchar *d = main_glosa_definicion("Ánimo. Coraje.", "ánimo");

	g_assert_cmpstr(d, ==, "Coraje.");
	g_free(d);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/glosa/quita-repetida", prueba_quita_la_glosa_repetida);
	g_test_add_func("/glosa/reina-valera", prueba_saca_la_reina_valera);
	g_test_add_func("/glosa/minima", prueba_definicion_minima);
	g_test_add_func("/glosa/sin-glosa", prueba_sin_glosa);
	g_test_add_func("/glosa/mayusculas", prueba_mayusculas);
	return g_test_run();
}
