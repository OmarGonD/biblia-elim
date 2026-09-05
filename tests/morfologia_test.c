/*
 * Biblia Elim
 * morfologia_test.c - los códigos de morfología, dichos en español
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *   cmake --build build --target morfologia_test
 *   ./build/tests/morfologia_test
 *
 * Y contra el corpus de verdad, que son los códigos que traen los
 * módulos instalados (uno por línea):
 *
 *   ./build/tests/morfologia_test --corpus codigos.txt
 */

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "main/morfologia.h"

static void
igual(const char *codigo, const char *esperado)
{
	gchar *r = main_morf_es(codigo);

	g_assert_cmpstr(r, ==, esperado);
	g_free(r);
}

/* Lo que más sale en el Nuevo Testamento. */
static void
prueba_frecuentes(void)
{
	igual("robinson:CONJ", "Conjunción");
	igual("robinson:PREP", "Preposición");
	igual("robinson:ADV", "Adverbio");
	igual("robinson:N-NSM", "Sustantivo · nominativo · singular · masculino");
	igual("robinson:T-GSM", "Artículo · genitivo · singular · masculino");
	igual("robinson:V-PAI-3S",
	      "Verbo · presente · activo · indicativo · 3ª persona · singular");
}

/* Lo que antes salía mal. */
static void
prueba_lo_que_estaba_roto(void)
{
	/* Era "Partícula · nom.": la negación más común del NT, 2.644 veces. */
	igual("robinson:PRT-N", "Partícula · negativo");
	igual("robinson:ADV-N", "Adverbio · negativo");
	igual("robinson:ADV-C", "Adverbio · comparativo");
	/* "I" es el pronombre interrogativo, no una interjección. */
	igual("robinson:I-NSM",
	      "Pronombre interrogativo · nominativo · singular · masculino");
	igual("robinson:INJ", "Interjección");
	/* "C" es el recíproco (ἀλλήλων), no una conjunción. */
	igual("robinson:C-APM",
	      "Pronombre recíproco · acusativo · plural · masculino");
	/* El participio perdía el caso. */
	igual("robinson:V-PAP-NSM",
	      "Verbo · presente · activo · participio · nominativo · "
	      "singular · masculino");
	/* Dos palabras fundidas venían en un solo atributo y se mezclaban. */
	igual("robinson:T-NSM robinson:N-NSM",
	      "Artículo · nominativo · singular · masculino + "
	      "Sustantivo · nominativo · singular · masculino");
}

/* Las clases que antes no se traducían en absoluto. */
static void
prueba_clases_nuevas(void)
{
	igual("robinson:R-GSM",
	      "Pronombre relativo · genitivo · singular · masculino");
	igual("robinson:F-3ASM",
	      "Pronombre reflexivo · 3ª persona · acusativo · singular · "
	      "masculino");
	igual("robinson:X-NSF",
	      "Pronombre indefinido · nominativo · singular · femenino");
	igual("robinson:S-1SNSM",
	      "Pronombre posesivo · 1ª persona · de un poseedor · "
	      "nominativo · singular · masculino");
	igual("robinson:N-PRI", "Sustantivo · nombre propio indeclinable");
	/* K es el correlativo y Q el correlativo o interrogativo, no al
	 * revés: lo dice el módulo Robinson de CrossWire. */
	igual("robinson:K-NSM",
	      "Pronombre correlativo · nominativo · singular · masculino");
	igual("robinson:Q-GPN",
	      "Pronombre correlativo o interrogativo · genitivo · plural · neutro");
	/* El pronombre personal no lleva género: "1AS" son tres datos. */
	igual("robinson:P-1AS",
	      "Pronombre personal · 1ª persona · acusativo · singular");
	igual("robinson:A-NUI", "Adjetivo · numeral indeclinable");
	igual("robinson:V-2AAI-3S",
	      "Verbo · aoristo 2º · activo · indicativo · 3ª persona · "
	      "singular");
	igual("robinson:V-PAN", "Verbo · presente · activo · infinitivo");
	igual("robinson:N-DSF-ATT",
	      "Sustantivo · dativo · singular · femenino · forma ática");
}

/* El hebreo de verdad: los códigos OSHM del módulo OSHB. Las tablas de
 * morfologia.c salen de las 3.481 entradas del módulo OSHM de CrossWire
 * y se comprobaron una a una contra él; esto es la muestra que queda
 * como red de seguridad. */
static void
prueba_hebreo(void)
{
	igual("oshm:HVqp3ms",
	      "Verbo · qal · perfecto · 3ª persona · masculino · singular");
	igual("oshm:HNcmpa",
	      "Sustantivo · común · masculino · plural · absoluto");
	/* Lo que en hebreo va pegado a la palabra se separa con barra. */
	igual("oshm:HTd/Ncmpa",
	      "Partícula · artículo definido + Sustantivo · común · "
	      "masculino · plural · absoluto");
	igual("oshm:HR/Ncfsa",
	      "Preposición + Sustantivo · común · femenino · singular · "
	      "absoluto");
	igual("oshm:HNcmsc/Sp3ms",
	      "Sustantivo · común · masculino · singular · constructo + "
	      "Sufijo · pronominal · 3ª persona · masculino · singular");
	igual("oshm:HVqrmsa",
	      "Verbo · qal · participio activo · masculino · singular · "
	      "absoluto");
	igual("oshm:HNp", "Sustantivo · nombre propio");
	/* El mismo tema cambia de nombre en arameo: 'q' es qal en hebreo
	 * y peal en arameo. */
	igual("oshm:AVqp3ms",
	      "Arameo · Verbo · peal · perfecto · 3ª persona · masculino · "
	      "singular");

	/* Y el Antiguo Testamento del KJV, que solo trae la tabla verbal
	 * de Strong: sin OSHB instalado, eso es cuanto se puede afirmar. */
	igual("strongMorph:TH8804", "Verbo hebreo");
	g_assert_cmpstr(main_morf_codigo("strongMorph:TH8804"), ==, "TH8804");
}

static void
prueba_corto(void)
{
	gchar *r = main_morf_corto("robinson:V-PAI-3S");

	g_assert_cmpstr(r, ==, "v. pres.act.ind. 3ªsg.");
	g_free(r);
	r = main_morf_corto("robinson:N-NSM");
	g_assert_cmpstr(r, ==, "sust. nom.sg.m.");
	g_free(r);
}

static void
prueba_vacios(void)
{
	igual(NULL, "");
	igual("", "");
	igual("robinson:", "");
	g_assert_cmpstr(main_morf_codigo(NULL), ==, "");
}

/* Contra los códigos de verdad: ninguno puede quedarse sin traducir.
 * Se considera sin traducir el que devuelve algo que sigue siendo el
 * código en crudo. */
static int
corpus(const char *ruta)
{
	gchar *texto = NULL;
	gchar **lineas;
	int total = 0, crudos = 0, i;

	if (!g_file_get_contents(ruta, &texto, NULL, NULL)) {
		g_printerr("no puedo leer %s\n", ruta);
		return 2;
	}
	lineas = g_strsplit(texto, "\n", -1);
	for (i = 0; lineas[i]; ++i) {
		gchar *es, *codigo;

		g_strstrip(lineas[i]);
		if (!*lineas[i])
			continue;
		total++;
		es = main_morf_es(lineas[i]);
		codigo = main_morf_codigo(lineas[i]);
		if (!*es || !g_strcmp0(es, codigo) || strstr(es, codigo)) {
			crudos++;
			if (crudos <= 20)
				g_print("  sin traducir: %-24s -> %s\n",
					lineas[i], es);
		}
		g_free(es);
		g_free(codigo);
	}
	g_strfreev(lineas);
	g_free(texto);
	g_print("corpus: %d códigos, %d sin traducir\n", total, crudos);
	return crudos ? 1 : 0;
}

int
main(int argc, char *argv[])
{
	if (argc == 3 && !g_strcmp0(argv[1], "--corpus"))
		return corpus(argv[2]);

	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/morf/frecuentes", prueba_frecuentes);
	g_test_add_func("/morf/lo-que-estaba-roto", prueba_lo_que_estaba_roto);
	g_test_add_func("/morf/clases-nuevas", prueba_clases_nuevas);
	g_test_add_func("/morf/hebreo", prueba_hebreo);
	g_test_add_func("/morf/corto", prueba_corto);
	g_test_add_func("/morf/vacios", prueba_vacios);
	return g_test_run();
}
