/*
 * Biblia Elim
 * pulpito_test.c - pruebas del reparto en pasos y del recorrido
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Se prueba pulpito_pasos.c, que es la parte que decide qué se ve y en
 * qué orden. Lo que toca Sword (reconocer una cita, sacar su texto) se
 * sustituye aquí por un resolutor de mentira, que es justo para lo que
 * el resolutor es un puntero a función.
 *
 *   cmake --build build --target pulpito_test
 *   ./build/tests/pulpito_test
 */

#include <glib.h>
#include <string.h>

#include "main/pulpito_pasos.h"

/* --------------------------------------------------------------------
 * El resolutor de mentira
 * ------------------------------------------------------------------ */

static const char *citas[] = {
    "Efesios 2:1", "Efesios 2:4-5", "Efesios 2:8-9",
    "Romanos 6:23", "Tito 3:5", NULL
};

static gchar *
falso_ref(const char *linea, gpointer datos)
{
	int i;

	(void)datos;
	for (i = 0; citas[i]; ++i)
		if (!g_strcmp0(linea, citas[i]))
			return g_strdup(citas[i]);
	return NULL;
}

static gchar *
falso_texto(const char *ref, gpointer datos)
{
	(void)datos;
	return g_strdup_printf("texto de %s", ref);
}

/* Un sermón de tres puntos: enunciado, texto base y dos viñetas cada
 * uno, que es el bosquejo de andar por casa. */
static PU_SERMON *
sermon_de_prueba(void)
{
	static const char *puntos[][2] = {
	    {"1. Estábamos muertos",
	     "Efesios 2:1<br/>El pecado no deja a medias<br/>"
	     "Sin Cristo no hay vida propia<br/>"
	     "Apoyo: Romanos 6:23<br/>"
	     "Nota: mirar el griego de «delitos» antes del domingo<br/>"
	     "Ilustración: un reloj parado acierta dos veces al día."},
	    {"2. Pero Dios, rico en misericordia",
	     "<p>Efesios 2:4-5</p><p>La iniciativa es suya</p>"
	     "<p>La misericordia llega antes que el mérito</p>"
	     "<p>Apoyo: Tito 3:5</p>"},
	    {"3. Por gracia sois salvos",
	     "Efesios 2:8-9<br/>No de vosotros, es don de Dios<br/>"
	     "Nadie se gloría de lo que no pagó"},
	};
	PU_RESOLUTOR res = {falso_ref, falso_texto, NULL};
	PU_SERMON *s = pu_sermon_nuevo("SermonPrueba", "Efesios 2", "SpaRV");
	guint i;

	for (i = 0; i < G_N_ELEMENTS(puntos); ++i) {
		int titulo = pu_sermon_titulo(s, puntos[i][0], 1);

		pu_sermon_contenido(s, puntos[i][1], 1, titulo, &res);
	}
	return s;
}

/* --------------------------------------------------------------------
 * Las pruebas
 * ------------------------------------------------------------------ */

static void
prueba_sin_etiquetas(void)
{
	gchar *t = pu_sin_etiquetas("<p class=\"x\">uno</p><br />dos"
				    "<span>&amp;</span>tres");

	g_assert_cmpstr(t, ==, "\nuno\n\ndos&tres");
	g_free(t);

	t = pu_sin_etiquetas(NULL);
	g_assert_cmpstr(t, ==, "");
	g_free(t);
}

/* --- las notas del predicador ---
 *
 * Se ven en el atril y no salen a la pantalla de la congregación, así
 * que aquí lo que se comprueba es a qué paso acaba pegada cada una.
 */

/* La nota va con el paso que viene detrás: en el sermón de prueba está
 * escrita delante de la ilustración, y es con la ilustración con lo que
 * tiene que salir. */
static void
prueba_nota_va_con_lo_siguiente(void)
{
	PU_SERMON *s = sermon_de_prueba();
	PU_PASO *ilustracion = pu_paso(s, 4);

	g_assert_cmpint(ilustracion->tipo, ==, PU_ILUSTRACION);
	g_assert_cmpstr(ilustracion->nota, ==,
			"mirar el griego de «delitos» antes del domingo");

	/* Y no se le pega a nadie más. */
	g_assert_null(pu_paso(s, 3)->nota);
	g_assert_null(pu_paso(s, 5)->nota);
	pu_sermon_libre(s);
}

/* Una nota no es un paso: no aparece en el recorrido. */
static void
prueba_nota_no_es_un_paso(void)
{
	PU_RESOLUTOR res = {falso_ref, falso_texto, NULL};
	PU_SERMON *s = pu_sermon_nuevo("N", "N", "SpaRV");
	int t = pu_sermon_titulo(s, "1. Punto", 1);

	pu_sermon_contenido(s, "Nota: respirar<br/>Nota: y mirar al fondo<br/>"
			       "una viñeta", 1, t, &res);

	g_assert_cmpint(pu_total(s), ==, 2);	/* el enunciado y la viñeta */
	g_assert_cmpint(pu_paso(s, 1)->tipo, ==, PU_VINETAS);
	/* Dos notas seguidas se juntan en renglones. */
	g_assert_cmpstr(pu_paso(s, 1)->nota, ==, "respirar\ny mirar al fondo");
	pu_sermon_libre(s);
}

/* Escrita al final, cuando ya no viene nada detrás, se queda con el
 * último paso del punto. */
static void
prueba_nota_al_final(void)
{
	PU_RESOLUTOR res = {falso_ref, falso_texto, NULL};
	PU_SERMON *s = pu_sermon_nuevo("N", "N", "SpaRV");
	int t = pu_sermon_titulo(s, "1. Punto", 1);

	pu_sermon_contenido(s, "Efesios 2:1<br/>Nota: no pasar de cinco minutos",
			    1, t, &res);

	g_assert_cmpint(pu_total(s), ==, 2);
	g_assert_cmpint(pu_paso(s, 1)->tipo, ==, PU_VERSO);
	g_assert_cmpstr(pu_paso(s, 1)->nota, ==, "no pasar de cinco minutos");
	pu_sermon_libre(s);
}

/* Y un punto que no tiene más que una nota se la queda en el enunciado,
 * que si no se perdería. */
static void
prueba_nota_sin_pasos(void)
{
	PU_RESOLUTOR res = {falso_ref, falso_texto, NULL};
	PU_SERMON *s = pu_sermon_nuevo("N", "N", "SpaRV");
	int t = pu_sermon_titulo(s, "1. El perdón", 1);

	pu_sermon_contenido(s, "Nota: contar lo de Ana", 1, t, &res);

	g_assert_cmpint(pu_total(s), ==, 1);
	g_assert_cmpint(pu_paso(s, 0)->tipo, ==, PU_TITULO);
	g_assert_cmpstr(pu_paso(s, 0)->nota, ==, "contar lo de Ana");
	pu_sermon_libre(s);
}

/* Una nota vacía no deja un renglón en blanco esperando. */
static void
prueba_nota_vacia(void)
{
	PU_RESOLUTOR res = {falso_ref, falso_texto, NULL};
	PU_SERMON *s = pu_sermon_nuevo("N", "N", "SpaRV");
	int t = pu_sermon_titulo(s, "1. Punto", 1);

	pu_sermon_contenido(s, "Nota:   <br/>una viñeta", 1, t, &res);

	g_assert_cmpint(pu_total(s), ==, 2);
	g_assert_null(pu_paso(s, 1)->nota);
	pu_sermon_libre(s);
}

/* De un bosquejo salen los pasos que se esperan, en su orden. */
static void
prueba_reparto(void)
{
	PU_SERMON *s = sermon_de_prueba();
	static const PU_TIPO esperado[] = {
	    PU_TITULO, PU_VERSO, PU_VINETAS, PU_APOYO, PU_ILUSTRACION,
	    PU_TITULO, PU_VERSO, PU_VINETAS, PU_APOYO,
	    PU_TITULO, PU_VERSO, PU_VINETAS
	};
	PU_PASO *p;
	guint i;

	g_assert_cmpint(pu_total(s), ==, (int)G_N_ELEMENTS(esperado));
	for (i = 0; i < G_N_ELEMENTS(esperado); ++i)
		g_assert_cmpint(pu_paso(s, i)->tipo, ==, esperado[i]);

	/* El texto base del sermón es la primera cita que aparece. */
	g_assert_cmpstr(s->ref_base, ==, "Efesios 2:1");

	/* El versículo llega con su texto ya resuelto: en el púlpito no se
	 * va a buscar nada. */
	p = pu_paso(s, 1);
	g_assert_cmpstr(p->ref, ==, "Efesios 2:1");
	g_assert_cmpstr(p->texto, ==, "texto de Efesios 2:1");

	/* Las viñetas seguidas van juntas en un paso, y la nota no sale. */
	p = pu_paso(s, 2);
	g_assert_cmpint(p->vinetas->len, ==, 2);
	g_assert_cmpstr((char *)g_ptr_array_index(p->vinetas, 0), ==,
			"El pecado no deja a medias");

	/* El apoyo y la ilustración son pasos aparte, del mismo punto. */
	g_assert_cmpstr(pu_paso(s, 3)->ref, ==, "Romanos 6:23");
	g_assert_cmpint(pu_paso(s, 3)->titulo_de, ==, 0);
	g_assert_cmpstr(pu_paso(s, 4)->ilustracion, ==,
			"un reloj parado acierta dos veces al día.");

	pu_sermon_libre(s);
}

/* Un bosquejo sin puntos no da pasos: la vista tiene que poder decirlo
 * sin romperse. */
static void
prueba_vacio(void)
{
	PU_SERMON *s = pu_sermon_nuevo("Vacio", "Vacío", "SpaRV");

	g_assert_cmpint(pu_total(s), ==, 0);
	g_assert_null(pu_paso(s, 0));
	g_assert_cmpint(pu_siguiente(s, 0), ==, 0);
	g_assert_cmpint(pu_verso_visible(s, 0), ==, -1);
	pu_sermon_libre(s);
}

/* Recorrer entero con Espacio y volver con la flecha. */
static void
prueba_adelante_atras(void)
{
	PU_SERMON *s = sermon_de_prueba();
	int i = 0;
	int vueltas = 0;

	while (i < pu_ultimo(s) && vueltas < 100) {
		int siguiente = pu_siguiente(s, i);

		g_assert_cmpint(siguiente, ==, i + 1);
		i = siguiente;
		++vueltas;
	}
	g_assert_cmpint(i, ==, pu_ultimo(s));
	/* En el último, Espacio ya no adelanta. */
	g_assert_cmpint(pu_siguiente(s, i), ==, pu_ultimo(s));

	while (i > 0)
		i = pu_anterior(s, i);
	g_assert_cmpint(i, ==, 0);
	/* Y en el primero, la flecha arriba no se sale. */
	g_assert_cmpint(pu_anterior(s, 0), ==, 0);

	pu_sermon_libre(s);
}

/* N y P saltan de enunciado en enunciado. */
static void
prueba_saltar_punto(void)
{
	PU_SERMON *s = sermon_de_prueba();

	g_assert_cmpint(pu_paso(s, 0)->tipo, ==, PU_TITULO);
	g_assert_cmpint(pu_titulo_cercano(s, 0, 1), ==, 5);
	g_assert_cmpint(pu_titulo_cercano(s, 5, 1), ==, 9);
	/* Desde media página también salta al punto siguiente. */
	g_assert_cmpint(pu_titulo_cercano(s, 2, 1), ==, 5);
	g_assert_cmpint(pu_titulo_cercano(s, 6, -1), ==, 5);
	/* En el último punto no hay a dónde ir: se queda. */
	g_assert_cmpint(pu_titulo_cercano(s, 9, 1), ==, 9);
	g_assert_cmpint(pu_titulo_cercano(s, 0, -1), ==, 0);

	pu_sermon_libre(s);
}

/* V enseña el versículo del paso, o el último que salió. */
static void
prueba_verso_visible(void)
{
	PU_SERMON *s = sermon_de_prueba();

	g_assert_cmpint(pu_verso_visible(s, 0), ==, -1);	/* aún ninguno */
	g_assert_cmpint(pu_verso_visible(s, 1), ==, 1);
	g_assert_cmpint(pu_verso_visible(s, 2), ==, 1);	/* el de antes */
	g_assert_cmpint(pu_verso_visible(s, 4), ==, 3);	/* el de apoyo */
	pu_sermon_libre(s);
}

/* R cicla los apoyos del punto; I encuentra su ilustración. */
static void
prueba_apoyos(void)
{
	PU_SERMON *s = sermon_de_prueba();
	GArray *a = pu_apoyos_de(s, 0);

	g_assert_cmpint(a->len, ==, 1);
	g_assert_cmpint(g_array_index(a, int, 0), ==, 3);
	g_array_free(a, TRUE);

	a = pu_apoyos_de(s, 9);	/* el tercer punto no tiene apoyos */
	g_assert_cmpint(a->len, ==, 0);
	g_array_free(a, TRUE);

	g_assert_cmpint(pu_ilustracion_de(s, 0), ==, 4);
	g_assert_cmpint(pu_ilustracion_de(s, 9), ==, -1);
	pu_sermon_libre(s);
}

/* Por dónde se iba: lo que se guarda al salir y lo que se hace con ello
 * al volver a entrar. */
static void
prueba_paso_guardado(void)
{
	PU_SERMON *s = sermon_de_prueba();

	/* Se pregunta solo si se quedó de verdad a medias. */
	g_assert_false(pu_preguntar_continuar(s, 0));
	g_assert_true(pu_preguntar_continuar(s, 4));
	g_assert_false(pu_preguntar_continuar(s, pu_ultimo(s)));

	/* Y lo guardado nunca deja el púlpito fuera del bosquejo, aunque
	 * el sermón haya encogido desde la última vez. */
	g_assert_cmpint(pu_paso_guardado(s, 4), ==, 4);
	g_assert_cmpint(pu_paso_guardado(s, 999), ==, pu_ultimo(s));
	g_assert_cmpint(pu_paso_guardado(s, -3), ==, 0);

	pu_sermon_libre(s);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/pulpito/sin-etiquetas", prueba_sin_etiquetas);
	g_test_add_func("/pulpito/reparto", prueba_reparto);
	g_test_add_func("/pulpito/vacio", prueba_vacio);
	g_test_add_func("/pulpito/adelante-atras", prueba_adelante_atras);
	g_test_add_func("/pulpito/saltar-punto", prueba_saltar_punto);
	g_test_add_func("/pulpito/verso-visible", prueba_verso_visible);
	g_test_add_func("/pulpito/apoyos", prueba_apoyos);
	g_test_add_func("/pulpito/paso-guardado", prueba_paso_guardado);
	g_test_add_func("/pulpito/nota-va-con-lo-siguiente",
			prueba_nota_va_con_lo_siguiente);
	g_test_add_func("/pulpito/nota-no-es-un-paso",
			prueba_nota_no_es_un_paso);
	g_test_add_func("/pulpito/nota-al-final", prueba_nota_al_final);
	g_test_add_func("/pulpito/nota-sin-pasos", prueba_nota_sin_pasos);
	g_test_add_func("/pulpito/nota-vacia", prueba_nota_vacia);
	return g_test_run();
}
