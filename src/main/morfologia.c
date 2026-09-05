/*
 * Biblia Elim
 * morfologia.c - los códigos de morfología, dichos en español
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "main/morfologia.h"

/* Dos maneras de decir lo mismo: entera para la ficha y el emergente,
 * apretada para la etiqueta de la fila, que tiene el ancho que tiene. */
typedef struct {
	GString *largo;
	GString *corto;
	gboolean recien_unido;	/* venimos de un " + ": no toca separador */
} SALIDA;

static void
pon(SALIDA *s, const char *largo, const char *corto, gboolean junto)
{
	if (!largo || !*largo)
		return;
	if (s->largo->len && !s->recien_unido)
		g_string_append(s->largo, " · ");
	g_string_append(s->largo, largo);

	if (!corto || !*corto) {
		s->recien_unido = FALSE;
		return;
	}
	/* "junto" es lo que va pegado a lo anterior sin espacio, como el
	 * tiempo, la voz y el modo de un verbo: "pres.act.ind." */
	if (s->corto->len && !junto && !s->recien_unido)
		g_string_append_c(s->corto, ' ');
	g_string_append(s->corto, corto);
	s->recien_unido = FALSE;
}

/* --------------------------------------------------------------------
 * Griego: los códigos de Robinson
 * ------------------------------------------------------------------ */

/* Caso, número y género, que en Robinson van pegados: "NSM", "GPF". El
 * género puede faltar: el pronombre personal se escribe "1AS", primera
 * persona acusativo singular, y ahí no hay género que dar. */
static void
caso_numero_genero(SALIDA *s, const char *cng)
{
	if (!cng || strlen(cng) < 2)
		return;
	switch (cng[0]) {
	case 'N': pon(s, _("nominativo"), _("nom."), FALSE); break;
	case 'G': pon(s, _("genitivo"), _("gen."), FALSE); break;
	case 'D': pon(s, _("dativo"), _("dat."), FALSE); break;
	case 'A': pon(s, _("acusativo"), _("ac."), FALSE); break;
	case 'V': pon(s, _("vocativo"), _("voc."), FALSE); break;
	default: break;
	}
	switch (cng[1]) {
	case 'S': pon(s, _("singular"), _("sg."), TRUE); break;
	case 'P': pon(s, _("plural"), _("pl."), TRUE); break;
	case 'D': pon(s, _("dual"), _("du."), TRUE); break;
	default: break;
	}
	if (!cng[2])
		return;
	switch (cng[2]) {
	case 'M': pon(s, _("masculino"), _("m."), TRUE); break;
	case 'F': pon(s, _("femenino"), _("f."), TRUE); break;
	case 'N': pon(s, _("neutro"), _("n."), TRUE); break;
	default: break;
	}
}

static void
persona(SALIDA *s, char d)
{
	switch (d) {
	case '1': pon(s, _("1ª persona"), _("1ª"), FALSE); break;
	case '2': pon(s, _("2ª persona"), _("2ª"), FALSE); break;
	case '3': pon(s, _("3ª persona"), _("3ª"), FALSE); break;
	default: break;
	}
}

static void
numero(SALIDA *s, char c, gboolean junto)
{
	if (c == 'S')
		pon(s, _("singular"), _("sg."), junto);
	else if (c == 'P')
		pon(s, _("plural"), _("pl."), junto);
}

/* El cuerpo del verbo: tiempo, voz y modo, "PAI" o "2AAP". */
static const char *
verbo_tvm(SALIDA *s, const char *p)
{
	/* "2AAI" es el segundo aoristo; el 2 va delante del tiempo. */
	gboolean segundo = FALSE;

	if (p[0] == '2' && p[1]) {
		segundo = TRUE;
		p++;
	}
	switch (*p) {
	case 'P':
		pon(s, segundo ? _("presente 2º") : _("presente"),
		    segundo ? _("pres.2") : _("pres."), FALSE);
		break;
	case 'I': pon(s, _("imperfecto"), _("impf."), FALSE); break;
	case 'F':
		pon(s, segundo ? _("futuro 2º") : _("futuro"),
		    segundo ? _("fut.2") : _("fut."), FALSE);
		break;
	case 'A':
		pon(s, segundo ? _("aoristo 2º") : _("aoristo"),
		    segundo ? _("aor.2") : _("aor."), FALSE);
		break;
	case 'R':
		pon(s, segundo ? _("perfecto 2º") : _("perfecto"),
		    segundo ? _("perf.2") : _("perf."), FALSE);
		break;
	case 'L':
		pon(s, segundo ? _("pluscuamperfecto 2º") : _("pluscuamperfecto"),
		    segundo ? _("plusc.2") : _("plusc."), FALSE);
		break;
	case 'X': pon(s, _("sin tiempo"), _("s/t"), FALSE); break;
	default: return p;
	}
	if (!*++p)
		return p;

	switch (*p) {
	case 'A': pon(s, _("activo"), _("act."), TRUE); break;
	case 'M': pon(s, _("medio"), _("med."), TRUE); break;
	case 'P': pon(s, _("pasivo"), _("pas."), TRUE); break;
	case 'E': pon(s, _("medio o pasivo"), _("med/pas."), TRUE); break;
	case 'D': pon(s, _("deponente medio"), _("dep.med."), TRUE); break;
	case 'O': pon(s, _("deponente pasivo"), _("dep.pas."), TRUE); break;
	case 'N': pon(s, _("deponente medio o pasivo"), _("dep."), TRUE); break;
	case 'Q': pon(s, _("impersonal activo"), _("imp.act."), TRUE); break;
	case 'X': break;	/* voz sin especificar: sigue el modo */
	default: return p;
	}
	if (!*++p)
		return p;

	switch (*p) {
	case 'I': pon(s, _("indicativo"), _("ind."), TRUE); break;
	case 'S': pon(s, _("subjuntivo"), _("subj."), TRUE); break;
	case 'O': pon(s, _("optativo"), _("opt."), TRUE); break;
	case 'M': pon(s, _("imperativo"), _("imper."), TRUE); break;
	case 'N': pon(s, _("infinitivo"), _("inf."), TRUE); break;
	case 'P': pon(s, _("participio"), _("part."), TRUE); break;
	default: break;
	}
	return ++p;
}

/* Los añadidos que Robinson pega detrás con guion. */
static gboolean
matiz(SALIDA *s, const char *m)
{
	if (!g_strcmp0(m, "N")) {
		pon(s, _("negativo"), _("neg."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "C")) {
		pon(s, _("comparativo"), _("comp."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "S")) {
		pon(s, _("superlativo"), _("sup."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "I")) {
		pon(s, _("interrogativo"), _("interr."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "K")) {
		/* "merged by crasis with a second word", dice el módulo
		 * Robinson: καί + ἐγώ = κἀγώ. */
		pon(s, _("por crasis"), _("crasis"), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "ATT")) {
		pon(s, _("forma ática"), _("át."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "ABB")) {
		pon(s, _("abreviado"), _("abr."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "P")) {
		pon(s, _("nombre propio"), _("propio"), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "L")) {
		pon(s, _("topónimo"), _("lugar"), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "G")) {
		pon(s, _("gentilicio"), _("gent."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "T")) {
		pon(s, _("título"), _("tít."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "PRI")) {
		pon(s, _("nombre propio indeclinable"), _("propio ind."),
		    FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "NUI")) {
		pon(s, _("numeral indeclinable"), _("num. ind."), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "LI")) {
		pon(s, _("letra indeclinable"), _("letra"), FALSE);
		return TRUE;
	}
	if (!g_strcmp0(m, "OI")) {
		pon(s, _("indeclinable"), _("ind."), FALSE);
		return TRUE;
	}
	return FALSE;
}

/* TRUE si esa clase lleva delante el número de persona, como los
 * pronombres personales, reflexivos y posesivos. */
static gboolean
lleva_persona(const char *pos)
{
	return (!g_strcmp0(pos, "P") || !g_strcmp0(pos, "F") ||
		!g_strcmp0(pos, "S"));
}

static gboolean
clase(SALIDA *s, const char *pos)
{
	static const struct {
		const char *cod;
		const char *largo;
		const char *corto;
	} tabla[] = {
	    {"N", N_("Sustantivo"), N_("sust.")},
	    {"A", N_("Adjetivo"), N_("adj.")},
	    {"T", N_("Artículo"), N_("art.")},
	    {"V", N_("Verbo"), N_("v.")},
	    {"P", N_("Pronombre personal"), N_("pron.")},
	    {"R", N_("Pronombre relativo"), N_("pron.rel.")},
	    {"D", N_("Pronombre demostrativo"), N_("pron.dem.")},
	    {"C", N_("Pronombre recíproco"), N_("pron.recípr.")},
	    {"F", N_("Pronombre reflexivo"), N_("pron.refl.")},
	    {"S", N_("Pronombre posesivo"), N_("pos.")},
	    {"X", N_("Pronombre indefinido"), N_("pron.indef.")},
	    {"I", N_("Pronombre interrogativo"), N_("pron.interr.")},
	    {"K", N_("Pronombre correlativo"), N_("pron.corr.")},
	    {"Q", N_("Pronombre correlativo o interrogativo"),
	     N_("pron.corr./int.")},
	    {"ADV", N_("Adverbio"), N_("adv.")},
	    {"CONJ", N_("Conjunción"), N_("conj.")},
	    {"PREP", N_("Preposición"), N_("prep.")},
	    {"PRT", N_("Partícula"), N_("part.")},
	    {"COND", N_("Partícula condicional"), N_("cond.")},
	    {"INJ", N_("Interjección"), N_("interj.")},
	    {"HEB", N_("Palabra hebrea transliterada"), N_("heb.")},
	    {"ARAM", N_("Palabra aramea transliterada"), N_("aram.")},
	    {NULL, NULL, NULL}
	};
	int i;

	for (i = 0; tabla[i].cod; ++i)
		if (!g_strcmp0(pos, tabla[i].cod)) {
			pon(s, _(tabla[i].largo), _(tabla[i].corto), FALSE);
			return TRUE;
		}
	return FALSE;
}

static void
robinson(SALIDA *s, const char *codigo)
{
	gchar **campos = g_strsplit(codigo, "-", -1);
	guint n = g_strv_length(campos);
	const char *pos = (n > 0) ? campos[0] : "";
	const char *dos = (n > 1) ? campos[1] : "";
	guint i;

	if (!clase(s, pos)) {
		/* Una clase que no conocemos se deja tal cual: mejor el
		 * código a la vista que una etiqueta inventada. */
		pon(s, pos, pos, FALSE);
		g_strfreev(campos);
		return;
	}

	if (!g_strcmp0(pos, "V")) {
		const char *resto = verbo_tvm(s, dos);
		const char *tres = (n > 2) ? campos[2] : "";

		(void)resto;
		/* Detrás del verbo va la persona y el número, salvo en el
		 * participio, que lleva caso, número y género. */
		if (strlen(tres) == 3 && strchr("NGDAV", tres[0]))
			caso_numero_genero(s, tres);
		else if (tres[0] >= '1' && tres[0] <= '3') {
			persona(s, tres[0]);
			numero(s, tres[1], TRUE);
		} else if (*tres)
			matiz(s, tres);
		for (i = 3; i < n; ++i)
			if (!matiz(s, campos[i]))
				pon(s, campos[i], campos[i], FALSE);
		g_strfreev(campos);
		return;
	}

	if (*dos) {
		const char *p = dos;

		if (lleva_persona(pos) && p[0] >= '1' && p[0] <= '3') {
			persona(s, p[0]);
			p++;
			/* El posesivo dice además de cuántos es lo poseído:
			 * "1SNSM" es de uno, "1PNSM" es de varios. */
			if (strlen(p) == 4 && (p[0] == 'S' || p[0] == 'P')) {
				if (p[0] == 'S')
					pon(s, _("de un poseedor"), _("de 1"),
					    FALSE);
				else
					pon(s, _("de varios poseedores"),
					    _("de vs"), FALSE);
				p++;
			}
		}
		/* Los indeclinables van antes que el caso: "NUI" empieza por
		 * N pero no es un nominativo, es un numeral. */
		if (!matiz(s, p)) {
			if (strlen(p) >= 2 && strchr("NGDAV", p[0]))
				caso_numero_genero(s, p);
			else
				pon(s, p, p, FALSE);
		}
	}

	for (i = 2; i < n; ++i)
		if (!matiz(s, campos[i]))
			pon(s, campos[i], campos[i], FALSE);

	g_strfreev(campos);
}


/* --------------------------------------------------------------------
 * Hebreo y arameo: los códigos OSHM (Open Scriptures Hebrew Morphology)
 *
 * Vienen así:  "oshm:HTd/Ncmpa"  =  artículo + sustantivo común m. pl.
 *
 * La letra de delante es el idioma (H hebreo, A arameo) y la barra
 * separa los trozos que en hebreo van pegados a la palabra: la
 * conjunción, la preposición, el artículo, el sufijo pronominal.
 *
 * Las tablas de abajo no están sacadas de memoria: salen de las 3.481
 * entradas del módulo OSHM de CrossWire, y tests/morfologia_test.c las
 * comprueba una por una contra ese mismo módulo. Los temas verbales
 * cambian de nombre según el idioma (la 'q' es qal en hebreo y peal en
 * arameo), y por eso hay dos tablas.
 * ------------------------------------------------------------------ */

typedef struct {
	char letra;
	const char *largo;
	const char *corto;
} LETRA;

static const LETRA temas_he[] = {
    {'q', N_("qal"), N_("qal")},
    {'Q', N_("qal pasivo"), N_("qal pas.")},
    {'N', N_("nifal"), N_("nif.")},
    {'p', N_("piel"), N_("piel")},
    {'P', N_("pual"), N_("pual")},
    {'h', N_("hifil"), N_("hif.")},
    {'H', N_("hofal"), N_("hof.")},
    {'t', N_("hitpael"), N_("hitp.")},
    {'D', N_("nitpael"), N_("nitp.")},
    {'o', N_("polel"), N_("polel")},
    {'O', N_("polal"), N_("polal")},
    {'r', N_("hitpolel"), N_("hitpolel")},
    {'m', N_("poel"), N_("poel")},
    {'M', N_("poal"), N_("poal")},
    {'l', N_("pilpel"), N_("pilpel")},
    {'L', N_("polpal"), N_("polpal")},
    {'f', N_("hitpalpel"), N_("hitpalpel")},
    {'k', N_("palel"), N_("palel")},
    {'K', N_("pulal"), N_("pulal")},
    {'i', N_("pilel"), N_("pilel")},
    {'j', N_("pealal"), N_("pealal")},
    {'u', N_("hotpaal"), N_("hotpaal")},
    {'v', N_("hishtafel"), N_("hishtafel")},
    {'z', N_("hitpoel"), N_("hitpoel")},
    {'c', N_("tifil"), N_("tifil")},
    {'e', N_("shafel"), N_("shafel")},
    {'s', N_("safel"), N_("safel")},
    {0, NULL, NULL}
};

static const LETRA temas_ar[] = {
    {'q', N_("peal"), N_("peal")},
    {'Q', N_("peil"), N_("peil")},
    {'p', N_("pael"), N_("pael")},
    {'P', N_("itpaal"), N_("itpaal")},
    {'a', N_("afel"), N_("afel")},
    {'h', N_("hafel"), N_("hafel")},
    {'H', N_("hofal"), N_("hof.")},
    {'M', N_("hitpaal"), N_("hitpaal")},
    {'t', N_("hishtafel"), N_("hishtafel")},
    {'u', N_("hitpeel"), N_("hitpeel")},
    {'i', N_("itpeel"), N_("itpeel")},
    {'z', N_("itpoel"), N_("itpoel")},
    {'v', N_("ishtafel"), N_("ishtafel")},
    {'e', N_("shafel"), N_("shafel")},
    {'s', N_("safel"), N_("safel")},
    {'o', N_("polel"), N_("polel")},
    {'r', N_("hitpolel"), N_("hitpolel")},
    {0, NULL, NULL}
};

static const LETRA generos[] = {
    {'m', N_("masculino"), N_("m.")},
    {'f', N_("femenino"), N_("f.")},
    {'b', N_("ambos géneros"), N_("amb.")},
    {'c', N_("género común"), N_("com.")},
    {'x', NULL, NULL},		/* sin especificar */
    {0, NULL, NULL}
};

static const LETRA numeros[] = {
    {'s', N_("singular"), N_("sg.")},
    {'p', N_("plural"), N_("pl.")},
    {'d', N_("dual"), N_("du.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA estados[] = {
    {'a', N_("absoluto"), N_("abs.")},
    {'c', N_("constructo"), N_("cons.")},
    {'d', N_("determinado"), N_("det.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA personas[] = {
    {'1', N_("1ª persona"), N_("1ª")},
    {'2', N_("2ª persona"), N_("2ª")},
    {'3', N_("3ª persona"), N_("3ª")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

/* El tercer carácter del verbo. */
static const LETRA conjugaciones[] = {
    {'p', N_("perfecto"), N_("perf.")},
    {'i', N_("imperfecto"), N_("impf.")},
    {'v', N_("imperativo"), N_("imper.")},
    {'h', N_("cohortativo"), N_("cohort.")},
    {'j', N_("yusivo"), N_("yus.")},
    {'q', N_("secuencial perfecto"), N_("sec.perf.")},
    {'w', N_("secuencial imperfecto"), N_("sec.impf.")},
    {'a', N_("infinitivo absoluto"), N_("inf.abs.")},
    {'c', N_("infinitivo constructo"), N_("inf.cons.")},
    {'r', N_("participio activo"), N_("part.act.")},
    {'s', N_("participio pasivo"), N_("part.pas.")},
    {0, NULL, NULL}
};

static const LETRA tipos_sustantivo[] = {
    {'c', N_("común"), N_("com.")},
    {'g', N_("gentilicio"), N_("gent.")},
    {'p', N_("nombre propio"), N_("propio")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA tipos_adjetivo[] = {
    {'a', NULL, NULL},		/* "adjetivo adjetivo" no aporta nada */
    {'c', N_("número cardinal"), N_("card.")},
    {'o', N_("número ordinal"), N_("ord.")},
    {'g', N_("gentilicio"), N_("gent.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA tipos_pronombre[] = {
    {'d', N_("demostrativo"), N_("dem.")},
    {'f', N_("indefinido"), N_("indef.")},
    {'i', N_("interrogativo"), N_("interr.")},
    {'p', N_("personal"), N_("pers.")},
    {'r', N_("relativo"), N_("rel.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA tipos_particula[] = {
    {'a', N_("de afirmación"), N_("afirm.")},
    {'d', N_("artículo definido"), N_("art.")},
    {'e', N_("de exhortación"), N_("exhort.")},
    {'i', N_("interrogativa"), N_("interr.")},
    {'j', N_("interjección"), N_("interj.")},
    {'m', N_("demostrativa"), N_("dem.")},
    {'n', N_("negativa"), N_("neg.")},
    {'o', N_("marcador de objeto directo"), N_("obj.dir.")},
    {'r', N_("relativa"), N_("rel.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

static const LETRA tipos_sufijo[] = {
    {'p', N_("pronominal"), N_("pron.")},
    {'d', N_("he direccional"), N_("he dir.")},
    {'h', N_("he paragógico"), N_("he par.")},
    {'n', N_("nun paragógico"), N_("nun par.")},
    {'x', NULL, NULL},
    {0, NULL, NULL}
};

/* Busca una letra en una tabla y la escribe. TRUE si la conocía. */
static gboolean
letra(SALIDA *s, const LETRA *tabla, char c, gboolean junto)
{
	int i;

	if (!c)
		return TRUE;	/* código corto: no falta nada */
	for (i = 0; tabla[i].letra; ++i)
		if (tabla[i].letra == c) {
			if (tabla[i].largo)
				pon(s, _(tabla[i].largo), _(tabla[i].corto),
				    junto);
			return TRUE;
		}
	return FALSE;
}

/* El carácter i del segmento, o 0 si el código se acaba antes. */
static char
en(const char *seg, guint i)
{
	return (strlen(seg) > i) ? seg[i] : 0;
}

/* Género, número y estado, que van siempre en ese orden al final. */
static void
gne(SALIDA *s, const char *seg, guint desde)
{
	letra(s, generos, en(seg, desde), FALSE);
	letra(s, numeros, en(seg, desde + 1), TRUE);
	letra(s, estados, en(seg, desde + 2), FALSE);
}

static void
oshm_segmento(SALIDA *s, const char *seg, gboolean arameo)
{
	const LETRA *temas = arameo ? temas_ar : temas_he;
	char conj;

	switch (seg[0]) {
	case 'V':
		pon(s, _("Verbo"), _("v."), FALSE);
		if (!letra(s, temas, en(seg, 1), FALSE))
			pon(s, seg + 1, seg + 1, FALSE);
		conj = en(seg, 2);
		letra(s, conjugaciones, conj, FALSE);
		if (conj == 'r' || conj == 's')
			gne(s, seg, 3);		/* participio */
		else if (conj != 'a' && conj != 'c') {
			letra(s, personas, en(seg, 3), FALSE);
			letra(s, generos, en(seg, 4), TRUE);
			letra(s, numeros, en(seg, 5), TRUE);
		}
		break;
	case 'N':
		pon(s, _("Sustantivo"), _("sust."), FALSE);
		letra(s, tipos_sustantivo, en(seg, 1), FALSE);
		if (en(seg, 1) != 'p')
			gne(s, seg, 2);
		break;
	case 'A':
		pon(s, _("Adjetivo"), _("adj."), FALSE);
		letra(s, tipos_adjetivo, en(seg, 1), FALSE);
		gne(s, seg, 2);
		break;
	case 'P':
		pon(s, _("Pronombre"), _("pron."), FALSE);
		letra(s, tipos_pronombre, en(seg, 1), FALSE);
		letra(s, personas, en(seg, 2), FALSE);
		letra(s, generos, en(seg, 3), FALSE);
		letra(s, numeros, en(seg, 4), TRUE);
		break;
	case 'S':
		pon(s, _("Sufijo"), _("suf."), FALSE);
		letra(s, tipos_sufijo, en(seg, 1), FALSE);
		if (en(seg, 1) == 'p') {
			letra(s, personas, en(seg, 2), FALSE);
			letra(s, generos, en(seg, 3), FALSE);
			letra(s, numeros, en(seg, 4), TRUE);
		}
		break;
	case 'T':
		pon(s, _("Partícula"), _("part."), FALSE);
		letra(s, tipos_particula, en(seg, 1), FALSE);
		break;
	case 'R':
		pon(s, _("Preposición"), _("prep."), FALSE);
		if (en(seg, 1) == 'd')
			pon(s, _("con artículo definido"), _("+art."), FALSE);
		break;
	case 'C':
		pon(s, _("Conjunción"), _("conj."), FALSE);
		break;
	case 'D':
		pon(s, _("Adverbio"), _("adv."), FALSE);
		break;
	default:
		pon(s, seg, seg, FALSE);
		break;
	}
}

/* Quién manda aquí es el esquema del atributo ("oshm:"), no la forma del
 * código: "ADV" es un adverbio griego de Robinson y a la vez encajaría
 * como "arameo, adverbio". Sin esquema, solo se da por hebreo lo que
 * lleva barra, que en Robinson no existe. */
static gboolean
es_oshm(const char *codigo, const char *sin_prefijo)
{
	if (!codigo || !sin_prefijo)
		return FALSE;
	if (!g_ascii_strncasecmp(codigo, "oshm:", 5))
		return TRUE;
	if (codigo != sin_prefijo)
		return FALSE;	/* traía otro esquema: no es OSHM */
	return (strchr(sin_prefijo, '/') != NULL &&
		(sin_prefijo[0] == 'H' || sin_prefijo[0] == 'A'));
}

static void
oshm(SALIDA *s, const char *codigo)
{
	gboolean arameo = (codigo[0] == 'A');
	gchar **partes = g_strsplit(codigo + 1, "/", -1);
	guint i;

	/* El hebreo es lo normal en el Antiguo Testamento; el arameo no, y
	 * por eso solo se dice cuando toca. */
	if (arameo)
		pon(s, _("Arameo"), _("aram."), FALSE);
	for (i = 0; partes[i]; ++i) {
		if (!*partes[i])
			continue;
		if (i && s->largo->len) {
			g_string_append(s->largo, " + ");
			g_string_append(s->corto, " + ");
			s->recien_unido = TRUE;
		}
		oshm_segmento(s, partes[i], arameo);
	}
	g_strfreev(partes);
}

/* --------------------------------------------------------------------
 * Un código suelto
 * ------------------------------------------------------------------ */

/* Quita el nombre del esquema: "robinson:V-PAI-3S" -> "V-PAI-3S". */
static const char *
sin_esquema(const char *codigo)
{
	const char *dos;

	if (!codigo)
		return NULL;
	dos = strchr(codigo, ':');
	return dos ? dos + 1 : codigo;
}

static void
uno(SALIDA *s, const char *codigo)
{
	const char *c = sin_esquema(codigo);

	if (!c || !*c)
		return;

	/* Los TH del Antiguo Testamento (KJV) son la tabla de análisis
	 * verbal hebreo de Strong: van solo sobre verbos, y eso es lo
	 * único que aquí se puede afirmar sin inventarse el tema y la
	 * conjugación. El código se sigue enseñando al lado, y quien
	 * quiera el detalle tiene el módulo de morfología de CrossWire. */
	if ((c[0] == 'T' || c[0] == 't') && (c[1] == 'H' || c[1] == 'h') &&
	    g_ascii_isdigit(c[2])) {
		pon(s, _("Verbo hebreo"), _("v.heb."), FALSE);
		return;
	}
	if (g_ascii_isdigit(c[0])) {
		pon(s, c, c, FALSE);
		return;
	}
	if (es_oshm(codigo, c)) {
		oshm(s, c);
		return;
	}
	robinson(s, c);
}

static void
recorrer(SALIDA *s, const char *attr)
{
	gchar **trozos;
	int i;

	if (!attr || !*attr)
		return;
	/* Dos palabras fundidas en una traen los dos códigos en el mismo
	 * atributo: "robinson:T-NSM robinson:N-NSM". El '+' vale como
	 * separador porque es como viajan por una URL, donde un espacio
	 * daría guerra y un '+' no aparece nunca dentro de un código. */
	trozos = g_strsplit_set(attr, " \t+", -1);
	for (i = 0; trozos[i]; ++i) {
		if (!*g_strstrip(trozos[i]))
			continue;
		if (s->largo->len) {
			g_string_append(s->largo, " + ");
			g_string_append(s->corto, " + ");
			s->recien_unido = TRUE;
		}
		uno(s, trozos[i]);
	}
	g_strfreev(trozos);
}

static void
salida_init(SALIDA *s)
{
	s->largo = g_string_new(NULL);
	s->corto = g_string_new(NULL);
	s->recien_unido = FALSE;
}

gchar *
main_morf_es(const char *attr)
{
	SALIDA s;

	salida_init(&s);
	recorrer(&s, attr);
	g_string_free(s.corto, TRUE);
	return g_string_free(s.largo, FALSE);
}

gchar *
main_morf_corto(const char *attr)
{
	SALIDA s;

	salida_init(&s);
	recorrer(&s, attr);
	g_string_free(s.largo, TRUE);
	return g_string_free(s.corto, FALSE);
}

gchar *
main_morf_codigo(const char *attr)
{
	GString *out;
	gchar **trozos;
	int i;

	if (!attr || !*attr)
		return g_strdup("");
	out = g_string_new(NULL);
	trozos = g_strsplit_set(attr, " \t+", -1);
	for (i = 0; trozos[i]; ++i) {
		const char *c;

		if (!*g_strstrip(trozos[i]))
			continue;
		c = sin_esquema(trozos[i]);
		if (!c || !*c)
			continue;
		if (out->len)
			g_string_append_c(out, ' ');
		g_string_append(out, c);
	}
	g_strfreev(trozos);
	return g_string_free(out, FALSE);
}
