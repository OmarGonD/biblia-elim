/*
 * Biblia Elim
 * interl_enriq_test.c - ficha de estudio enriquecida del interlineal (lee fichas.sqlite)
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
#include <sqlite3.h>

#include "main/interl_enriq.h"

/* Mismo esquema (reducido) que tools/construir_fichas_sqlite.py. */
#define ESQUEMA                                                                          \
	"PRAGMA user_version=1;"                                                            \
	"CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"                  \
	"INSERT INTO metadata VALUES('formato','fichas-v1'),('fichas','%d');"               \
	"CREATE TABLE fichas(ref_tisch TEXT,posicion INT,strong TEXT,ref_estandar TEXT,"    \
	"glosa_interlineal TEXT,rango_semantico TEXT,construccion TEXT,"                    \
	"sentido_en_contexto TEXT,matiz TEXT,variantes_textuales TEXT,notas_traduccion TEXT," \
	"otros_usos TEXT,nivel_certeza TEXT);"                                              \
	"CREATE TABLE citas(ref_estandar TEXT,version TEXT,texto TEXT,"                     \
	"ocr_sospechoso INT DEFAULT 0);"

static gchar *
base(const char *nombre, const char *sql_filas, int n)
{
	gchar *ruta = g_build_filename(g_get_tmp_dir(), nombre, NULL);
	gchar *esquema = g_strdup_printf(ESQUEMA, n);
	sqlite3 *db = NULL;

	g_remove(ruta);
	g_assert_cmpint(sqlite3_open(ruta, &db), ==, SQLITE_OK);
	g_assert_cmpint(sqlite3_exec(db, esquema, NULL, NULL, NULL), ==, SQLITE_OK);
	g_assert_cmpint(sqlite3_exec(db, sql_filas, NULL, NULL, NULL), ==, SQLITE_OK);
	sqlite3_close(db);
	g_free(esquema);
	return ruta;
}

#define COMPLETA                                                                         \
	"INSERT INTO fichas VALUES('John.1.2',5,'G4314','John.1.2','con',"                  \
	"'[\"hacia\",\"con, junto a\"]','πρός + acusativo','Cercanía <y> relación.',NULL,"  \
	"NULL,NULL,'[\"Jn 14:6\"]','alto');"                                                \
	"INSERT INTO citas VALUES('John.1.2','Torres Amat','en Dios',0),"                   \
	"('John.1.2','Reina-Valera 1909','con Dios',0),('John.1.2','Texto OCR','$ roto',1);"

static void
prueba_ficha_completa(void)
{
	gchar *ruta = base("interl_enriq_test.sqlite", COMPLETA, 1);
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	h = main_interl_enriq_html("John.1.2", 5, "G4314");
	g_assert_nonnull(h);
	g_assert_nonnull(g_strstr_len(h, -1, "πρός + acusativo"));
	g_assert_nonnull(g_strstr_len(h, -1, "Cercanía &lt;y&gt; relación."));
	/* las citas salen de la tabla `citas` por ref_estandar, con el nombre exacto de la versión... */
	g_assert_nonnull(g_strstr_len(h, -1, "<b>Torres Amat:</b> en Dios"));
	g_assert_nonnull(g_strstr_len(h, -1, "<b>Reina-Valera 1909:</b> con Dios"));
	g_assert_nonnull(g_strstr_len(h, -1, "Jn 14:6"));
	/* ...y las marcadas como OCR sospechoso no se muestran; null se omite. */
	g_assert_null(g_strstr_len(h, -1, "$ roto"));
	g_assert_null(g_strstr_len(h, -1, "Matiz"));
	g_assert_null(g_strstr_len(h, -1, "Sobre las diferencias"));
	g_free(h);
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

/* Misma ficha en texto plano: una línea por campo, listas con «; », sin escapar y sin citas. */
static void
prueba_ficha_en_texto_plano(void)
{
	gchar *ruta = base("interl_enriq_txt.sqlite", COMPLETA, 1);
	gchar *t;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	t = main_interl_enriq_texto("John.1.2", 5, "G4314");
	g_assert_nonnull(t);
	g_assert_nonnull(g_strstr_len(t, -1, "Construcción: πρός + acusativo\n"));
	g_assert_nonnull(g_strstr_len(t, -1, "Cercanía <y> relación."));
	g_assert_nonnull(g_strstr_len(t, -1, "Significados en el Nuevo Testamento: hacia; con, junto a"));
	g_assert_nonnull(g_strstr_len(t, -1, "Otros usos: Jn 14:6"));
	g_assert_null(g_strstr_len(t, -1, "Torres Amat"));
	g_assert_null(g_strstr_len(t, -1, "Matiz"));
	g_free(t);
	g_assert_null(main_interl_enriq_texto("John.1.2", 5, "G1"));
	g_assert_null(main_interl_enriq_texto(NULL, 5, "G4314"));
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

/* Palabra frecuente: solo construcción y rango; el resto, nada. */
static void
prueba_ficha_minima(void)
{
	gchar *ruta = base("interl_enriq_min.sqlite",
			   "INSERT INTO fichas(ref_tisch,posicion,strong,glosa_interlineal,rango_semantico)"
			   " VALUES('John.1.1',4,'G3588','el','[\"el\"]');", 1);
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	h = main_interl_enriq_html("John.1.1", 4, "G3588");
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
	gchar *ruta = base("interl_enriq_inv.sqlite",
			   "INSERT INTO fichas(ref_tisch,posicion,strong,glosa_interlineal)"
			   " VALUES('John.1.3',1,'G1','x');", 1);

	g_assert_null(main_interl_enriq_html("John.1.1", 1, "G1"));	/* sin base abierta */
	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 1);
	g_assert_null(main_interl_enriq_html("John.1.1", 1, "G1"));	/* otro versículo */
	/* Sin campos útiles tampoco hay ficha. */
	g_assert_null(main_interl_enriq_html("John.1.3", 1, "G1"));
	g_assert_null(main_interl_enriq_html(NULL, 1, "G1"));
	g_assert_null(main_interl_enriq_html("John.1.3", 1, NULL));
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

static void
prueba_archivo_malo_no_cierra_lo_abierto(void)
{
	gchar *ok = base("interl_enriq_ok.sqlite", COMPLETA, 1);
	gchar *mal = g_build_filename(g_get_tmp_dir(), "interl_enriq_mal.sqlite", NULL);
	gchar *otra = g_build_filename(g_get_tmp_dir(), "interl_enriq_otra.sqlite", NULL);
	sqlite3 *db = NULL;
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ok), ==, 1);
	g_assert_true(g_file_set_contents(mal, "{no es sqlite", -1, NULL));
	g_assert_cmpint(main_interl_enriq_cargar(mal), ==, -1);
	g_assert_cmpint(main_interl_enriq_cargar("/no/existe.sqlite"), ==, -1);
	/* una base SQLite que no es de fichas (sin metadata) tampoco vale */
	g_remove(otra);
	g_assert_cmpint(sqlite3_open(otra, &db), ==, SQLITE_OK);
	g_assert_cmpint(sqlite3_exec(db, "CREATE TABLE t(a)", NULL, NULL, NULL), ==, SQLITE_OK);
	sqlite3_close(db);
	g_assert_cmpint(main_interl_enriq_cargar(otra), ==, -1);
	h = main_interl_enriq_html("John.1.2", 5, "G4314");
	g_assert_nonnull(h);
	g_free(h);
	main_interl_enriq_liberar();
	g_remove(ok);
	g_remove(mal);
	g_remove(otra);
	g_free(ok);
	g_free(mal);
	g_free(otra);
}

/* La posición identifica la palabra; sin posición solo vale un Strong único en el versículo. */
static void
prueba_posicion_y_fallback(void)
{
	gchar *ruta = base("interl_enriq_pos.sqlite",
			   "INSERT INTO fichas(ref_tisch,posicion,strong,glosa_interlineal,construccion) VALUES"
			   "('John.1.1',5,'G3056','Verbo','primera'),"
			   "('John.1.1',8,'G3056','Verbo','segunda'),"
			   "('John.1.1',10,'G4314','con','única');", 3);
	gchar *h;

	g_assert_cmpint(main_interl_enriq_cargar(ruta), ==, 3);
	h = main_interl_enriq_html("John.1.1", 8, "G3056");
	g_assert_nonnull(g_strstr_len(h, -1, "segunda"));
	g_free(h);
	/* la posición correcta con otro Strong no es esa ficha */
	g_assert_null(main_interl_enriq_html("John.1.1", 8, "G4314"));
	/* posición 0: Strong repetido -> no se adivina */
	g_assert_null(main_interl_enriq_html("John.1.1", 0, "G3056"));
	/* posición 0: Strong único -> sí */
	h = main_interl_enriq_html("John.1.1", 0, "G4314");
	g_assert_nonnull(g_strstr_len(h, -1, "única"));
	g_free(h);
	g_assert_null(main_interl_enriq_html("John.1.1", 99, "G3056"));
	main_interl_enriq_liberar();
	g_remove(ruta);
	g_free(ruta);
}

/* Lee una ficha de Jn 1:1 desde la base REAL generada por el build (tools/construir_fichas_sqlite.py). */
static void
prueba_jn_1_1_desde_la_base_real(void)
{
	gchar *h;

	if (!g_file_test(FICHAS_DB, G_FILE_TEST_IS_REGULAR)) {
		g_test_skip("fichas.sqlite no generada");
		return;
	}
	g_assert_cmpint(main_interl_enriq_cargar(FICHAS_DB), >, 4000);
	/* θεὸς sin artículo (pos. 14): el predicado de «el Verbo era Dios». */
	h = main_interl_enriq_html("John.1.1", 14, "G2316");
	g_assert_nonnull(h);
	g_assert_nonnull(g_strstr_len(h, -1, "Colwell"));
	g_assert_nonnull(g_strstr_len(h, -1, "<b>La Santa Biblia Reina-Valera (1909):</b>"));
	g_assert_nonnull(g_strstr_len(h, -1, "el Verbo era Dios"));
	g_free(h);
	/* y θεόν con artículo (pos. 12) es otra ficha, no la misma */
	h = main_interl_enriq_html("John.1.1", 12, "G2316");
	g_assert_nonnull(h);
	g_assert_null(g_strstr_len(h, -1, "Colwell"));
	g_free(h);
	/* λόγος aparece 3 veces en Jn 1:1: sin posición no se adivina; con posición, sí */
	g_assert_null(main_interl_enriq_html("John.1.1", 0, "G3056"));
	h = main_interl_enriq_html("John.1.1", 17, "G3056");
	g_assert_nonnull(h);
	g_free(h);
	/* πρός es único en el versículo: el fallback por Strong lo resuelve */
	h = main_interl_enriq_html("John.1.1", 0, "G4314");
	g_assert_nonnull(h);
	g_free(h);
	/* Jn 1:2: la RV 1909 sí; Torres Amat se omite en ese versículo (OCR sospechoso) */
	h = main_interl_enriq_html("John.1.2", 5, "G4314");
	g_assert_nonnull(h);
	g_assert_nonnull(g_strstr_len(h, -1, "Reina-Valera (1909):</b>"));
	g_assert_null(g_strstr_len(h, -1, "Torres Amat"));
	g_free(h);
	/* Jn 8:30 no tiene fichas (rango excluido) */
	g_assert_null(main_interl_enriq_html("John.8.30", 1, "G1161"));
	main_interl_enriq_liberar();
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/interl-enriq/completa", prueba_ficha_completa);
	g_test_add_func("/interl-enriq/texto-plano", prueba_ficha_en_texto_plano);
	g_test_add_func("/interl-enriq/minima", prueba_ficha_minima);
	g_test_add_func("/interl-enriq/invalidas", prueba_sin_ficha_y_entradas_invalidas);
	g_test_add_func("/interl-enriq/archivo-malo", prueba_archivo_malo_no_cierra_lo_abierto);
	g_test_add_func("/interl-enriq/posicion", prueba_posicion_y_fallback);
	g_test_add_func("/interl-enriq/jn-1-1-base-real", prueba_jn_1_1_desde_la_base_real);
	return g_test_run();
}
