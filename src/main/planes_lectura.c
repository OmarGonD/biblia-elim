/*
 * Biblia Elim
 * planes_lectura.c - planes de lectura listos y su progreso
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
#include <stdlib.h>
#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "main/planes_lectura.h"
#include "main/racha.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* --------------------------------------------------------------------
 * Los libros
 *
 * La cuenta de capítulos es la de la versificación KJV, que es la que
 * usan los módulos SWORD del canon protestante. El nombre OSIS es el
 * que se le pasa al motor para navegar (lo entiende con cualquier
 * idioma cargado); el castellano es solo para mostrar.
 * ------------------------------------------------------------------ */

enum {
	LB_GEN = 0, LB_EXOD, LB_LEV, LB_NUM, LB_DEUT,
	LB_JOSH, LB_JUDG, LB_RUTH,
	LB_1SAM, LB_2SAM, LB_1KGS, LB_2KGS, LB_1CHR, LB_2CHR,
	LB_EZRA, LB_NEH, LB_ESTH,
	LB_JOB, LB_PS, LB_PROV, LB_ECCL, LB_SONG,
	LB_ISA, LB_JER, LB_LAM, LB_EZEK, LB_DAN,
	LB_HOS, LB_JOEL, LB_AMOS, LB_OBAD, LB_JONAH, LB_MIC,
	LB_NAH, LB_HAB, LB_ZEPH, LB_HAG, LB_ZECH, LB_MAL,
	LB_MATT, LB_MARK, LB_LUKE, LB_JOHN, LB_ACTS,
	LB_ROM, LB_1COR, LB_2COR, LB_GAL, LB_EPH, LB_PHIL, LB_COL,
	LB_1THESS, LB_2THESS, LB_1TIM, LB_2TIM, LB_TITUS, LB_PHLM,
	LB_HEB, LB_JAS, LB_1PET, LB_2PET, LB_1JOHN, LB_2JOHN, LB_3JOHN,
	LB_JUDE, LB_REV,
	LB_N
};

typedef struct {
	const char *osis;
	const char *nombre;
	short capitulos;
} PL_LIBRO;

static const PL_LIBRO libros[LB_N] = {
    {"Gen", N_("Génesis"), 50},
    {"Exod", N_("Éxodo"), 40},
    {"Lev", N_("Levítico"), 27},
    {"Num", N_("Números"), 36},
    {"Deut", N_("Deuteronomio"), 34},
    {"Josh", N_("Josué"), 24},
    {"Judg", N_("Jueces"), 21},
    {"Ruth", N_("Rut"), 4},
    {"1Sam", N_("1 Samuel"), 31},
    {"2Sam", N_("2 Samuel"), 24},
    {"1Kgs", N_("1 Reyes"), 22},
    {"2Kgs", N_("2 Reyes"), 25},
    {"1Chr", N_("1 Crónicas"), 29},
    {"2Chr", N_("2 Crónicas"), 36},
    {"Ezra", N_("Esdras"), 10},
    {"Neh", N_("Nehemías"), 13},
    {"Esth", N_("Ester"), 10},
    {"Job", N_("Job"), 42},
    {"Ps", N_("Salmos"), 150},
    {"Prov", N_("Proverbios"), 31},
    {"Eccl", N_("Eclesiastés"), 12},
    {"Song", N_("Cantares"), 8},
    {"Isa", N_("Isaías"), 66},
    {"Jer", N_("Jeremías"), 52},
    {"Lam", N_("Lamentaciones"), 5},
    {"Ezek", N_("Ezequiel"), 48},
    {"Dan", N_("Daniel"), 12},
    {"Hos", N_("Oseas"), 14},
    {"Joel", N_("Joel"), 3},
    {"Amos", N_("Amós"), 9},
    {"Obad", N_("Abdías"), 1},
    {"Jonah", N_("Jonás"), 4},
    {"Mic", N_("Miqueas"), 7},
    {"Nah", N_("Nahúm"), 3},
    {"Hab", N_("Habacuc"), 3},
    {"Zeph", N_("Sofonías"), 3},
    {"Hag", N_("Hageo"), 2},
    {"Zech", N_("Zacarías"), 14},
    {"Mal", N_("Malaquías"), 4},
    {"Matt", N_("Mateo"), 28},
    {"Mark", N_("Marcos"), 16},
    {"Luke", N_("Lucas"), 24},
    {"John", N_("Juan"), 21},
    {"Acts", N_("Hechos"), 28},
    {"Rom", N_("Romanos"), 16},
    {"1Cor", N_("1 Corintios"), 16},
    {"2Cor", N_("2 Corintios"), 13},
    {"Gal", N_("Gálatas"), 6},
    {"Eph", N_("Efesios"), 6},
    {"Phil", N_("Filipenses"), 4},
    {"Col", N_("Colosenses"), 4},
    {"1Thess", N_("1 Tesalonicenses"), 5},
    {"2Thess", N_("2 Tesalonicenses"), 3},
    {"1Tim", N_("1 Timoteo"), 6},
    {"2Tim", N_("2 Timoteo"), 4},
    {"Titus", N_("Tito"), 3},
    {"Phlm", N_("Filemón"), 1},
    {"Heb", N_("Hebreos"), 13},
    {"Jas", N_("Santiago"), 5},
    {"1Pet", N_("1 Pedro"), 5},
    {"2Pet", N_("2 Pedro"), 3},
    {"1John", N_("1 Juan"), 5},
    {"2John", N_("2 Juan"), 1},
    {"3John", N_("3 Juan"), 1},
    {"Jude", N_("Judas"), 1},
    {"Rev", N_("Apocalipsis"), 22}};

/* Libro entero, sin repetir la cuenta de capítulos. */
#define ENTERO(x) {x, 1, 0}

/* Sección de settings.xml donde vive el progreso de todos los planes. */
#define PL_SECCION "planes"

/* --------------------------------------------------------------------
 * Vías y planes
 * ------------------------------------------------------------------ */

static const PL_TRAMO via_at[] = {
    ENTERO(LB_GEN), ENTERO(LB_EXOD), ENTERO(LB_LEV), ENTERO(LB_NUM),
    ENTERO(LB_DEUT), ENTERO(LB_JOSH), ENTERO(LB_JUDG), ENTERO(LB_RUTH),
    ENTERO(LB_1SAM), ENTERO(LB_2SAM), ENTERO(LB_1KGS), ENTERO(LB_2KGS),
    ENTERO(LB_1CHR), ENTERO(LB_2CHR), ENTERO(LB_EZRA), ENTERO(LB_NEH),
    ENTERO(LB_ESTH), ENTERO(LB_JOB), ENTERO(LB_PS), ENTERO(LB_PROV),
    ENTERO(LB_ECCL), ENTERO(LB_SONG), ENTERO(LB_ISA), ENTERO(LB_JER),
    ENTERO(LB_LAM), ENTERO(LB_EZEK), ENTERO(LB_DAN), ENTERO(LB_HOS),
    ENTERO(LB_JOEL), ENTERO(LB_AMOS), ENTERO(LB_OBAD), ENTERO(LB_JONAH),
    ENTERO(LB_MIC), ENTERO(LB_NAH), ENTERO(LB_HAB), ENTERO(LB_ZEPH),
    ENTERO(LB_HAG), ENTERO(LB_ZECH), ENTERO(LB_MAL)};

static const PL_TRAMO via_nt[] = {
    ENTERO(LB_MATT), ENTERO(LB_MARK), ENTERO(LB_LUKE), ENTERO(LB_JOHN),
    ENTERO(LB_ACTS), ENTERO(LB_ROM), ENTERO(LB_1COR), ENTERO(LB_2COR),
    ENTERO(LB_GAL), ENTERO(LB_EPH), ENTERO(LB_PHIL), ENTERO(LB_COL),
    ENTERO(LB_1THESS), ENTERO(LB_2THESS), ENTERO(LB_1TIM), ENTERO(LB_2TIM),
    ENTERO(LB_TITUS), ENTERO(LB_PHLM), ENTERO(LB_HEB), ENTERO(LB_JAS),
    ENTERO(LB_1PET), ENTERO(LB_2PET), ENTERO(LB_1JOHN), ENTERO(LB_2JOHN),
    ENTERO(LB_3JOHN), ENTERO(LB_JUDE), ENTERO(LB_REV)};

/* Orden cronológico aproximado, por bloques de capítulos: Job entre los
 * patriarcas, los Salmos repartidos por sus cinco libros a lo largo de
 * la monarquía y el destierro, los profetas metidos en la historia de
 * Reyes y Crónicas, los Evangelios armonizados y las cartas en el orden
 * en que se escribieron. Cada capítulo aparece una sola vez: las 1189
 * piezas del rompecabezas, reordenadas. */
static const PL_TRAMO via_crono[] = {
    /* patriarcas */
    {LB_GEN, 1, 11},
    ENTERO(LB_JOB),
    {LB_GEN, 12, 50},
    /* éxodo y ley */
    ENTERO(LB_EXOD), ENTERO(LB_LEV), ENTERO(LB_NUM), ENTERO(LB_DEUT),
    /* conquista y jueces */
    ENTERO(LB_JOSH), ENTERO(LB_JUDG), ENTERO(LB_RUTH),
    /* reino unido */
    ENTERO(LB_1SAM),
    {LB_PS, 1, 41},
    ENTERO(LB_2SAM),
    ENTERO(LB_1CHR),
    {LB_PS, 42, 72},
    {LB_1KGS, 1, 11},
    {LB_2CHR, 1, 9},
    ENTERO(LB_PROV), ENTERO(LB_ECCL), ENTERO(LB_SONG),
    {LB_PS, 73, 89},
    /* reino dividido */
    {LB_1KGS, 12, 22},
    {LB_2CHR, 10, 20},
    ENTERO(LB_OBAD), ENTERO(LB_JOEL), ENTERO(LB_JONAH),
    ENTERO(LB_AMOS), ENTERO(LB_HOS),
    {LB_2KGS, 1, 17},
    {LB_ISA, 1, 39},
    ENTERO(LB_MIC),
    {LB_2CHR, 21, 28},
    ENTERO(LB_NAH), ENTERO(LB_ZEPH),
    {LB_2KGS, 18, 25},
    {LB_2CHR, 29, 36},
    {LB_ISA, 40, 66},
    ENTERO(LB_JER), ENTERO(LB_HAB), ENTERO(LB_LAM),
    ENTERO(LB_EZEK), ENTERO(LB_DAN),
    {LB_PS, 90, 106},
    /* vuelta del destierro */
    ENTERO(LB_EZRA), ENTERO(LB_HAG), ENTERO(LB_ZECH),
    ENTERO(LB_ESTH), ENTERO(LB_NEH), ENTERO(LB_MAL),
    {LB_PS, 107, 150},
    /* evangelios armonizados */
    {LB_LUKE, 1, 1},
    {LB_JOHN, 1, 1},
    {LB_MATT, 1, 2},
    {LB_LUKE, 2, 2},
    {LB_MATT, 3, 4},
    {LB_MARK, 1, 1},
    {LB_LUKE, 3, 4},
    {LB_JOHN, 2, 4},
    {LB_MATT, 5, 7},
    {LB_MARK, 2, 3},
    {LB_LUKE, 5, 6},
    {LB_MATT, 8, 13},
    {LB_MARK, 4, 6},
    {LB_LUKE, 7, 9},
    {LB_JOHN, 5, 6},
    {LB_MATT, 14, 18},
    {LB_MARK, 7, 9},
    {LB_LUKE, 10, 13},
    {LB_JOHN, 7, 10},
    {LB_MATT, 19, 23},
    {LB_MARK, 10, 12},
    {LB_LUKE, 14, 20},
    {LB_JOHN, 11, 12},
    {LB_MATT, 24, 25},
    {LB_MARK, 13, 13},
    {LB_LUKE, 21, 21},
    {LB_MATT, 26, 28},
    {LB_MARK, 14, 16},
    {LB_LUKE, 22, 24},
    {LB_JOHN, 13, 21},
    /* la iglesia y las cartas */
    {LB_ACTS, 1, 12},
    ENTERO(LB_JAS),
    {LB_ACTS, 13, 14},
    ENTERO(LB_GAL),
    {LB_ACTS, 15, 17},
    ENTERO(LB_1THESS), ENTERO(LB_2THESS),
    {LB_ACTS, 18, 19},
    ENTERO(LB_1COR), ENTERO(LB_2COR), ENTERO(LB_ROM),
    {LB_ACTS, 20, 28},
    ENTERO(LB_EPH), ENTERO(LB_PHIL), ENTERO(LB_COL), ENTERO(LB_PHLM),
    ENTERO(LB_1TIM), ENTERO(LB_TITUS),
    ENTERO(LB_1PET), ENTERO(LB_2PET), ENTERO(LB_2TIM),
    ENTERO(LB_HEB), ENTERO(LB_JUDE),
    ENTERO(LB_1JOHN), ENTERO(LB_2JOHN), ENTERO(LB_3JOHN),
    ENTERO(LB_REV)};

static const PL_TRAMO via_salmos[] = {ENTERO(LB_PS)};
static const PL_TRAMO via_proverbios[] = {ENTERO(LB_PROV)};
static const PL_TRAMO via_marcos[] = {ENTERO(LB_MARK)};
static const PL_TRAMO via_juan_hechos[] = {ENTERO(LB_JOHN), ENTERO(LB_ACTS)};

/* Plan curado: un capítulo por día, elegido a mano. */
static const PL_TRAMO via_quien_es_jesus[] = {
    {LB_JOHN, 1, 1},
    {LB_LUKE, 2, 2},
    {LB_MARK, 1, 1},
    {LB_MATT, 5, 5},
    {LB_LUKE, 15, 15},
    {LB_JOHN, 11, 11},
    {LB_LUKE, 24, 24}};

static const char *const titulos_quien_es_jesus[] = {
    N_("El Verbo se hizo carne"),
    N_("Nace en Belén"),
    N_("Empieza a predicar"),
    N_("El Sermón del Monte"),
    N_("El corazón del Padre"),
    N_("La resurrección y la vida"),
    N_("Está vivo")};

#define VIA(v, nom) {nom, v, (int)(sizeof(v) / sizeof((v)[0]))}

static const PL_VIA vias_un_anio[] = {
    VIA(via_at, N_("Antiguo Testamento")),
    VIA(via_nt, N_("Nuevo Testamento"))};
static const PL_VIA vias_crono[] = {VIA(via_crono, NULL)};
static const PL_VIA vias_nt90[] = {VIA(via_nt, NULL)};
static const PL_VIA vias_salmos_prov[] = {
    VIA(via_salmos, N_("Salmos")),
    VIA(via_proverbios, N_("Proverbios"))};
static const PL_VIA vias_jesus7[] = {VIA(via_quien_es_jesus, NULL)};
static const PL_VIA vias_marcos[] = {VIA(via_marcos, NULL)};
static const PL_VIA vias_juan_hechos[] = {VIA(via_juan_hechos, NULL)};

#define PLAN_VIAS(v) v, (int)(sizeof(v) / sizeof((v)[0]))

static const PL_PLAN planes[] = {
    {"un-anio",
     N_("La Biblia en un año"),
     N_("Toda la Biblia en 365 días, con una porción del Antiguo "
	"Testamento y otra del Nuevo casi todos los días: unos tres o "
	"cuatro capítulos por jornada."),
     365, PLAN_VIAS(vias_un_anio), FALSE, NULL},

    {"cronologico",
     N_("Cronológico en un año"),
     N_("La Biblia entera en 365 días, pero en el orden aproximado en "
	"que ocurrieron las cosas: Job entre los patriarcas, los Salmos "
	"junto a la vida de David, los profetas dentro de la historia de "
	"Reyes y Crónicas, los Evangelios armonizados y las cartas según "
	"cuándo se escribieron."),
     365, PLAN_VIAS(vias_crono), FALSE, NULL},

    {"nt-90",
     N_("Nuevo Testamento en 90 días"),
     N_("Los 260 capítulos del Nuevo Testamento en tres meses: unos "
	"tres capítulos por día, de Mateo a Apocalipsis."),
     90, PLAN_VIAS(vias_nt90), FALSE, NULL},

    {"salmos-proverbios",
     N_("Salmos y Proverbios en un mes"),
     N_("Cinco salmos y un capítulo de Proverbios cada día durante 31 "
	"días. El clásico para acompañar la mañana o cerrar el día."),
     31, PLAN_VIAS(vias_salmos_prov), FALSE, NULL},

    {"jesus-7",
     N_("Quién es Jesús (7 días)"),
     N_("Una semana, un capítulo por día, para conocer a Jesús: quién "
	"es, qué hizo y por qué importa. El plan más corto para empezar."),
     7, PLAN_VIAS(vias_jesus7), TRUE, titulos_quien_es_jesus},

    {"marcos-14",
     N_("El Evangelio de Marcos (14 días)"),
     N_("Los 16 capítulos del evangelio más breve en dos semanas: uno o "
	"dos por día. La vida de Jesús contada a paso rápido."),
     14, PLAN_VIAS(vias_marcos), FALSE, NULL},

    {"juan-hechos-30",
     N_("Nuevo comienzo: Juan y Hechos (30 días)"),
     N_("Un mes con el evangelio de Juan y el libro de los Hechos: "
	"quién es Jesús y qué hizo la primera iglesia. Uno o dos "
	"capítulos por día."),
     30, PLAN_VIAS(vias_juan_hechos), FALSE, NULL}};

#define N_PLANES ((int)(sizeof(planes) / sizeof(planes[0])))

/* --------------------------------------------------------------------
 * Reparto de capítulos
 * ------------------------------------------------------------------ */

static int
tramo_capitulos(const PL_TRAMO *t)
{
	int fin = t->cap_fin ? t->cap_fin : libros[t->libro].capitulos;
	return fin - t->cap_ini + 1;
}

static int
via_capitulos(const PL_VIA *via)
{
	int i, n = 0;
	for (i = 0; i < via->n_tramos; ++i)
		n += tramo_capitulos(&via->tramos[i]);
	return n;
}

/* Capítulo n-ésimo (0-based) de la vía, desplegando los tramos. */
static gboolean
via_capitulo(const PL_VIA *via, int n, int *libro, int *cap)
{
	int i;
	for (i = 0; i < via->n_tramos; ++i) {
		int largo = tramo_capitulos(&via->tramos[i]);
		if (n < largo) {
			*libro = via->tramos[i].libro;
			*cap = via->tramos[i].cap_ini + n;
			return TRUE;
		}
		n -= largo;
	}
	return FALSE;
}

/* Qué trozo de la vía toca el día `dia` (1-based): [desde, hasta).
 * Reparte los capítulos lo más parejo posible; si la vía tiene menos
 * capítulos que días el trozo puede salir vacío, que es justo lo que
 * pasa con el Nuevo Testamento en el plan de un año. */
static void
via_trozo_del_dia(const PL_VIA *via, int dias, int dia, int *desde, int *hasta)
{
	int total = via_capitulos(via);
	*desde = (int)(((gint64)(dia - 1) * total) / dias);
	*hasta = (int)(((gint64)dia * total) / dias);
}

/* --------------------------------------------------------------------
 * Planes que arma el lector
 *
 * Elige libros enteros y cuántos días quiere; el reparto es el mismo de
 * arriba, una sola vía con los libros en orden canónico. Se guardan en
 * settings.xml, sección <planespersonales>:
 *
 *   <plan label="personal-1" list="90|John,Acts|Juan y Hechos"/>
 *
 * El nombre va al final a propósito: así puede llevar barras, comas o
 * lo que sea, porque lo que hay tras la segunda barra es el nombre
 * entero. El progreso se guarda como el de cualquier otro plan, por id,
 * de modo que estos planes marcan días, se reinician y salen en la
 * lista igual que los de la casa.
 * ------------------------------------------------------------------ */

#define PL_SECCION_PERSONAL "planespersonales"

typedef struct {
	PL_PLAN plan;		/* lo que ve el resto del programa */
	PL_VIA via;
	PL_TRAMO *tramos;
	gchar *id;
	gchar *nombre;
	gchar *descripcion;
} PL_PERSONAL;

static GPtrArray *personales = NULL;

static int
libro_por_osis(const char *osis)
{
	int i;
	for (i = 0; i < LB_N; ++i)
		if (!strcmp(libros[i].osis, osis))
			return i;
	return -1;
}

int
main_planes_libro_por_osis(const char *osis)
{
	return osis ? libro_por_osis(osis) : -1;
}

int
main_planes_libros_cuantos(void)
{
	return LB_N;
}

const char *
main_planes_libro_nombre(int libro)
{
	if (libro < 0 || libro >= LB_N)
		return "";
	return _(libros[libro].nombre);
}

int
main_planes_libro_capitulos(int libro)
{
	if (libro < 0 || libro >= LB_N)
		return 0;
	return libros[libro].capitulos;
}

gboolean
main_planes_libro_es_nt(int libro)
{
	return (libro >= LB_MATT && libro < LB_N);
}

int
main_planes_capitulos_de(const gboolean *marcados)
{
	int i, n = 0;
	if (!marcados)
		return 0;
	for (i = 0; i < LB_N; ++i)
		if (marcados[i])
			n += libros[i].capitulos;
	return n;
}

char *
main_planes_ritmo_texto(int capitulos, int dias)
{
	int base, resto;

	if (capitulos < 1 || dias < 1)
		return g_strdup("");
	base = capitulos / dias;
	resto = capitulos % dias;
	if (base == 0)
		return g_strdup(_("menos de un capítulo por día"));
	if (resto == 0)
		return g_strdup_printf(ngettext("%d capítulo por día",
						"%d capítulos por día", base),
				       base);
	return g_strdup_printf(_("entre %d y %d capítulos por día"),
			       base, base + 1);
}

static gchar *
personal_descripcion(const PL_PERSONAL *p)
{
	int n_libros = p->via.n_tramos;
	int caps = via_capitulos(&p->via);
	gchar *libros_txt, *dias_txt, *ritmo, *texto;

	libros_txt = g_strdup_printf(ngettext("%d libro", "%d libros",
					      n_libros),
				     n_libros);
	dias_txt = g_strdup_printf(ngettext("%d día", "%d días",
					    p->plan.dias),
				   p->plan.dias);
	ritmo = main_planes_ritmo_texto(caps, p->plan.dias);
	texto = g_strdup_printf(_("Un plan tuyo: %s, %d capítulos en %s · %s."),
				libros_txt, caps, dias_txt, ritmo);
	g_free(libros_txt);
	g_free(dias_txt);
	g_free(ritmo);
	return texto;
}

/* Rehace la vía, el nombre y la descripción a partir de lo elegido. La
 * estructura no se mueve de sitio: quien tenga el PL_PLAN en la mano
 * (el diálogo, el plan en curso) lo sigue teniendo bueno. */
static void
personal_montar(PL_PERSONAL *p, const char *nombre,
		const gboolean *marcados, int dias)
{
	int i, n = 0, caps;

	for (i = 0; i < LB_N; ++i)
		if (marcados[i])
			++n;

	g_free(p->tramos);
	p->tramos = g_new0(PL_TRAMO, MAX(n, 1));
	n = 0;
	for (i = 0; i < LB_N; ++i) {
		if (!marcados[i])
			continue;
		p->tramos[n].libro = i;
		p->tramos[n].cap_ini = 1;
		p->tramos[n].cap_fin = 0;	/* libro entero */
		++n;
	}

	p->via.nombre = NULL;
	p->via.tramos = p->tramos;
	p->via.n_tramos = n;

	if (nombre != p->nombre) {
		g_free(p->nombre);
		p->nombre = g_strdup((nombre && *nombre) ? nombre
						        : _("Mi plan"));
	}

	/* Más días que capítulos dejaría jornadas en blanco: el plan dura
	 * como mucho lo que dan de sí los libros elegidos. */
	caps = via_capitulos(&p->via);
	if (dias < 1)
		dias = 1;
	if (caps && dias > caps)
		dias = caps;

	p->plan.id = p->id;
	p->plan.nombre = p->nombre;
	p->plan.dias = dias;
	p->plan.vias = &p->via;
	p->plan.n_vias = 1;
	p->plan.tramo_por_dia = FALSE;
	p->plan.titulos = NULL;

	g_free(p->descripcion);
	p->descripcion = personal_descripcion(p);
	p->plan.descripcion = p->descripcion;
}

static void
personal_guardar(const PL_PERSONAL *p)
{
	GString *lista = g_string_new(NULL);
	gchar *valor;
	int i;

	for (i = 0; i < p->via.n_tramos; ++i) {
		if (lista->len)
			g_string_append_c(lista, ',');
		g_string_append(lista, libros[p->via.tramos[i].libro].osis);
	}
	valor = g_strdup_printf("%d|%s|%s", p->plan.dias, lista->str,
				p->nombre);
	xml_set_list_item(PL_SECCION_PERSONAL, "plan", p->id, valor);
	g_free(valor);
	g_string_free(lista, TRUE);
}

static void
personal_liberar(PL_PERSONAL *p)
{
	g_free(p->tramos);
	g_free(p->id);
	g_free(p->nombre);
	g_free(p->descripcion);
	g_free(p);
}

/* "90|John,Acts|Juan y Hechos" -> plan. Devuelve NULL si la línea no
 * trae ningún libro que conozcamos: mejor saltarse una entrada rota que
 * ofrecer un plan vacío. */
static PL_PERSONAL *
personal_desde_texto(const char *id, const char *valor)
{
	gchar **campos = g_strsplit(valor, "|", 3);
	gboolean marcados[LB_N];
	PL_PERSONAL *p = NULL;
	int dias, i, n = 0;

	if (g_strv_length(campos) < 2)
		goto fin;

	memset(marcados, 0, sizeof(marcados));
	dias = atoi(campos[0]);

	{
		gchar **nombres = g_strsplit(campos[1], ",", -1);
		for (i = 0; nombres[i]; ++i) {
			int lb = libro_por_osis(g_strstrip(nombres[i]));
			if (lb >= 0 && !marcados[lb]) {
				marcados[lb] = TRUE;
				++n;
			}
		}
		g_strfreev(nombres);
	}
	if (!n)
		goto fin;

	p = g_new0(PL_PERSONAL, 1);
	p->id = g_strdup(id);
	personal_montar(p, campos[2], marcados, dias);

fin:
	g_strfreev(campos);
	return p;
}

static void
personales_cargar(void)
{
	if (personales)
		return;
	personales = g_ptr_array_new();

	if (!xml_set_section_ptr(PL_SECCION_PERSONAL))
		return;
	do {
		char *label = xml_get_label();
		char *valor;

		if (!label)
			continue;
		valor = xml_get_list();
		if (valor) {
			PL_PERSONAL *p = personal_desde_texto(label, valor);
			if (p)
				g_ptr_array_add(personales, p);
			g_free(valor);
		}
		g_free(label);
	} while (xml_next_item());
}

static PL_PERSONAL *
personal_de(const PL_PLAN *plan)
{
	guint i;

	personales_cargar();
	for (i = 0; i < personales->len; ++i) {
		PL_PERSONAL *p = g_ptr_array_index(personales, i);
		if (&p->plan == plan)
			return p;
	}
	return NULL;
}

gboolean
main_planes_es_personal(const PL_PLAN *plan)
{
	return plan && personal_de(plan) != NULL;
}

const PL_PLAN *
main_planes_personal_nuevo(const char *nombre, const gboolean *marcados,
			   int dias)
{
	PL_PERSONAL *p;
	gchar *id = NULL;
	int i;

	if (!marcados || main_planes_capitulos_de(marcados) < 1)
		return NULL;
	personales_cargar();

	for (i = 1; i < 10000; ++i) {
		g_free(id);
		id = g_strdup_printf("personal-%d", i);
		if (!main_planes_por_id(id))
			break;
	}

	p = g_new0(PL_PERSONAL, 1);
	p->id = id;
	personal_montar(p, nombre, marcados, dias);
	g_ptr_array_add(personales, p);
	personal_guardar(p);
	return &p->plan;
}

gboolean
main_planes_personal_editar(const PL_PLAN *plan, const char *nombre,
			    const gboolean *marcados, int dias)
{
	PL_PERSONAL *p = personal_de(plan);

	if (!p || !marcados || main_planes_capitulos_de(marcados) < 1)
		return FALSE;
	personal_montar(p, nombre, marcados, dias);
	personal_guardar(p);
	return TRUE;
}

void
main_planes_personal_borrar(const PL_PLAN *plan)
{
	PL_PERSONAL *p = personal_de(plan);
	const char *activo;

	if (!p)
		return;
	activo = main_planes_activo();
	if (activo && !strcmp(activo, p->id))
		main_planes_soltar();
	xml_remove_node(PL_SECCION_PERSONAL, "plan", p->id);
	xml_remove_node(PL_SECCION, "plan", p->id);
	g_ptr_array_remove(personales, p);
	personal_liberar(p);
}

void
main_planes_personal_libros(const PL_PLAN *plan, gboolean *marcados)
{
	int i;

	if (!marcados)
		return;
	memset(marcados, 0, sizeof(gboolean) * LB_N);
	if (!plan)
		return;
	for (i = 0; i < plan->n_vias; ++i) {
		const PL_VIA *via = &plan->vias[i];
		int t;
		for (t = 0; t < via->n_tramos; ++t)
			marcados[via->tramos[t].libro] = TRUE;
	}
}

int
main_planes_cuantos(void)
{
	personales_cargar();
	return N_PLANES + (int)personales->len;
}

const PL_PLAN *
main_planes_get(int i)
{
	personales_cargar();
	if (i < 0)
		return NULL;
	if (i < N_PLANES)
		return &planes[i];
	i -= N_PLANES;
	if (i < (int)personales->len) {
		PL_PERSONAL *p = g_ptr_array_index(personales, i);
		return &p->plan;
	}
	return NULL;
}

const PL_PLAN *
main_planes_por_id(const char *id)
{
	int i;
	if (!id || !*id)
		return NULL;
	for (i = 0; i < N_PLANES; ++i)
		if (!strcmp(planes[i].id, id))
			return &planes[i];

	personales_cargar();
	for (i = 0; i < (int)personales->len; ++i) {
		PL_PERSONAL *p = g_ptr_array_index(personales, i);
		if (!strcmp(p->id, id))
			return &p->plan;
	}
	return NULL;
}

int
main_planes_total_capitulos(const PL_PLAN *plan)
{
	int i, n = 0;
	if (!plan)
		return 0;
	for (i = 0; i < plan->n_vias; ++i)
		n += via_capitulos(&plan->vias[i]);
	return n;
}

const char *
main_planes_titulo(const PL_PLAN *plan, int dia)
{
	if (!plan || !plan->titulos || dia < 1 || dia > plan->dias)
		return NULL;
	return _(plan->titulos[dia - 1]);
}

/* Junta capítulos seguidos del mismo libro en "Génesis 1-3", y cambia
 * de trozo cuando cambia el libro: "Malaquías 4; Mateo 1-2". */
static void
agrega_capitulo(GString *out, int libro, int cap,
		int *libro_ini, int *cap_ini, int *cap_prev)
{
	if (*libro_ini == libro && cap == *cap_prev + 1) {
		*cap_prev = cap;
		return;
	}
	if (*libro_ini >= 0) {
		if (out->len)
			g_string_append(out, "; ");
		if (*cap_ini == *cap_prev)
			g_string_append_printf(out, "%s %d",
					       _(libros[*libro_ini].nombre),
					       *cap_ini);
		else
			g_string_append_printf(out, "%s %d-%d",
					       _(libros[*libro_ini].nombre),
					       *cap_ini, *cap_prev);
	}
	*libro_ini = libro;
	*cap_ini = cap;
	*cap_prev = cap;
}

static void
via_texto(const PL_VIA *via, int desde, int hasta, GString *out)
{
	int n, libro, cap;
	int libro_ini = -1, cap_ini = 0, cap_prev = 0;

	for (n = desde; n < hasta; ++n)
		if (via_capitulo(via, n, &libro, &cap))
			agrega_capitulo(out, libro, cap,
					&libro_ini, &cap_ini, &cap_prev);
	/* cierra el último trozo */
	agrega_capitulo(out, -1, 0, &libro_ini, &cap_ini, &cap_prev);
}

char *
main_planes_lectura(const PL_PLAN *plan, int dia)
{
	GString *out;
	int i;

	if (!plan || dia < 1 || dia > plan->dias)
		return NULL;

	out = g_string_new(NULL);

	if (plan->tramo_por_dia) {
		const PL_VIA *via = &plan->vias[0];
		if (dia <= via->n_tramos)
			via_texto(via, dia - 1, dia, out);
		return g_string_free(out, FALSE);
	}

	for (i = 0; i < plan->n_vias; ++i) {
		int desde, hasta;
		GString *trozo;

		via_trozo_del_dia(&plan->vias[i], plan->dias, dia,
				  &desde, &hasta);
		if (desde >= hasta)
			continue;
		trozo = g_string_new(NULL);
		via_texto(&plan->vias[i], desde, hasta, trozo);
		if (out->len)
			g_string_append(out, "  ·  ");
		g_string_append(out, trozo->str);
		g_string_free(trozo, TRUE);
	}
	return g_string_free(out, FALSE);
}

static void
via_referencias(const PL_VIA *via, int desde, int hasta, GList **lista)
{
	int n, libro, cap;
	for (n = desde; n < hasta; ++n)
		if (via_capitulo(via, n, &libro, &cap))
			*lista = g_list_prepend(*lista,
						g_strdup_printf("%s %d",
								libros[libro].osis,
								cap));
}

GList *
main_planes_referencias(const PL_PLAN *plan, int dia)
{
	GList *lista = NULL;
	int i;

	if (!plan || dia < 1 || dia > plan->dias)
		return NULL;

	if (plan->tramo_por_dia) {
		const PL_VIA *via = &plan->vias[0];
		if (dia <= via->n_tramos)
			via_referencias(via, dia - 1, dia, &lista);
		return g_list_reverse(lista);
	}

	for (i = 0; i < plan->n_vias; ++i) {
		int desde, hasta;
		via_trozo_del_dia(&plan->vias[i], plan->dias, dia,
				  &desde, &hasta);
		via_referencias(&plan->vias[i], desde, hasta, &lista);
	}
	return g_list_reverse(lista);
}

/* --------------------------------------------------------------------
 * Progreso
 *
 * En settings.xml, sección <planes>:
 *   <estado label="activo" list="un-anio"/>
 *   <plan label="un-anio" list="2026-09-02|0110100..."/>
 * La lista de días es una tira de '0'/'1', uno por día del plan: es
 * corta hasta para el año (365 caracteres) y sobrevive a que el lector
 * marque los días fuera de orden, que es lo que pasa de verdad.
 * ------------------------------------------------------------------ */

static gchar *activo_cache = NULL;

const char *
main_planes_activo(void)
{
	char *val = xml_get_list_from_label(PL_SECCION, "estado", "activo");
	g_free(activo_cache);
	activo_cache = (val && *val) ? g_strdup(val) : NULL;
	g_free(val);
	return activo_cache;
}

/* "inicio|marcas" -> las dos mitades. Las dos salen siempre no nulas. */
static void
progreso_leer(const PL_PLAN *plan, gchar **inicio, gchar **marcas)
{
	char *val = xml_get_list_from_label(PL_SECCION, "plan", plan->id);
	const char *barra;

	*inicio = NULL;
	*marcas = NULL;

	if (val && *val) {
		barra = strchr(val, '|');
		if (barra) {
			*inicio = g_strndup(val, barra - val);
			*marcas = g_strdup(barra + 1);
		} else {
			*inicio = g_strdup(val);
		}
	}
	g_free(val);

	if (!*inicio)
		*inicio = g_strdup("");
	/* Normaliza el largo: si el plan cambió de días, o el archivo
	 * viene de otra versión, no queremos leer fuera de la tira. */
	if (!*marcas || (int)strlen(*marcas) != plan->dias) {
		gchar *nuevo = g_strnfill(plan->dias, '0');
		if (*marcas) {
			int n = MIN((int)strlen(*marcas), plan->dias);
			memcpy(nuevo, *marcas, n);
			g_free(*marcas);
		}
		*marcas = nuevo;
	}
}

static void
progreso_guardar(const PL_PLAN *plan, const char *inicio, const char *marcas)
{
	gchar *val = g_strdup_printf("%s|%s", inicio ? inicio : "", marcas);
	xml_set_list_item(PL_SECCION, "plan", plan->id, val);
	g_free(val);
}

static gchar *
hoy_iso(void)
{
	GDateTime *ahora = g_date_time_new_now_local();
	gchar *s = g_date_time_format(ahora, "%Y-%m-%d");
	g_date_time_unref(ahora);
	return s;
}

void
main_planes_activar(const PL_PLAN *plan)
{
	gchar *inicio, *marcas;

	if (!plan)
		return;
	xml_set_list_item(PL_SECCION, "estado", "activo", plan->id);

	progreso_leer(plan, &inicio, &marcas);
	if (!*inicio) {
		g_free(inicio);
		inicio = hoy_iso();
	}
	progreso_guardar(plan, inicio, marcas);
	g_free(inicio);
	g_free(marcas);
}

void
main_planes_soltar(void)
{
	xml_set_list_item(PL_SECCION, "estado", "activo", "");
}

const char *
main_planes_inicio(const PL_PLAN *plan)
{
	static gchar *cache = NULL;
	gchar *inicio, *marcas;

	g_clear_pointer(&cache, g_free);
	if (!plan)
		return NULL;
	progreso_leer(plan, &inicio, &marcas);
	g_free(marcas);
	if (*inicio)
		cache = inicio;
	else
		g_free(inicio);
	return cache;
}

gboolean
main_planes_dia_hecho(const PL_PLAN *plan, int dia)
{
	gchar *inicio, *marcas;
	gboolean hecho;

	if (!plan || dia < 1 || dia > plan->dias)
		return FALSE;
	progreso_leer(plan, &inicio, &marcas);
	hecho = (marcas[dia - 1] == '1');
	g_free(inicio);
	g_free(marcas);
	return hecho;
}

void
main_planes_marcar(const PL_PLAN *plan, int dia, gboolean hecho)
{
	gchar *inicio, *marcas;

	if (!plan || dia < 1 || dia > plan->dias)
		return;
	progreso_leer(plan, &inicio, &marcas);
	marcas[dia - 1] = hecho ? '1' : '0';
	/* Marcar un día es empezar el plan: si no tenía fecha, hoy. */
	if (hecho && !*inicio) {
		g_free(inicio);
		inicio = hoy_iso();
	}
	/* Y hoy queda apuntado como día de lectura. Aquí y no en
	 * main_planes_marcar_hasta(), que escribe las marcas por su
	 * cuenta: ponerse al día es declarar que esos días no se leyeron,
	 * y una racha que se rellena hacia atrás no mide nada. */
	if (hecho)
		main_racha_apuntar_hoy();
	progreso_guardar(plan, inicio, marcas);
	g_free(inicio);
	g_free(marcas);
}

int
main_planes_dias_hechos(const PL_PLAN *plan)
{
	gchar *inicio, *marcas;
	int i, n = 0;

	if (!plan)
		return 0;
	progreso_leer(plan, &inicio, &marcas);
	for (i = 0; i < plan->dias; ++i)
		if (marcas[i] == '1')
			++n;
	g_free(inicio);
	g_free(marcas);
	return n;
}

int
main_planes_dia_de_hoy(const PL_PLAN *plan)
{
	gchar *inicio, *marcas;
	int i, dia = 0;

	if (!plan)
		return 0;
	progreso_leer(plan, &inicio, &marcas);
	for (i = 0; i < plan->dias; ++i) {
		if (marcas[i] != '1') {
			dia = i + 1;
			break;
		}
	}
	g_free(inicio);
	g_free(marcas);
	/* Todo marcado: el plan está terminado, se queda en el último. */
	return dia ? dia : plan->dias;
}

int
main_planes_dia_segun_calendario(const PL_PLAN *plan)
{
	const char *inicio = main_planes_inicio(plan);
	int a = 0, m = 0, d = 0, dias;
	GDate empezo, hoy;

	if (!inicio || sscanf(inicio, "%d-%d-%d", &a, &m, &d) != 3)
		return 0;
	if (!g_date_valid_dmy(d, m, a))
		return 0;

	g_date_clear(&empezo, 1);
	g_date_clear(&hoy, 1);
	g_date_set_dmy(&empezo, d, m, a);
	g_date_set_time_t(&hoy, time(NULL));

	dias = (int)g_date_days_between(&empezo, &hoy) + 1;
	if (dias < 1)
		dias = 1;
	if (dias > plan->dias)
		dias = plan->dias;
	return dias;
}

void
main_planes_reiniciar(const PL_PLAN *plan)
{
	gchar *marcas;

	if (!plan)
		return;
	marcas = g_strnfill(plan->dias, '0');
	progreso_guardar(plan, "", marcas);
	g_free(marcas);
}

/* --------------------------------------------------------------------
 * Qué toca hoy
 * ------------------------------------------------------------------ */

PL_HOY
main_planes_estado_hoy(gchar **detalle)
{
	const char *activo = main_planes_activo();
	const PL_PLAN *plan = activo ? main_planes_por_id(activo) : NULL;
	gchar *lectura;
	int dia;

	if (detalle)
		*detalle = NULL;
	if (!plan)
		return PL_HOY_SIN_PLAN;

	dia = main_planes_dia_de_hoy(plan);
	/* El día que toca es el primero sin marcar; que ese salga ya
	 * marcado solo pasa cuando no queda ninguno, o sea, al final. */
	if (main_planes_dia_hecho(plan, dia)) {
		if (detalle)
			*detalle = g_strdup_printf(_("%s · terminado"),
						   _(plan->nombre));
		return PL_HOY_TERMINADO;
	}

	lectura = main_planes_lectura(plan, dia);
	if (detalle)
		*detalle = g_strdup_printf(_("Día %d de %d · %s"), dia,
					   plan->dias, lectura ? lectura : "");
	g_free(lectura);
	return PL_HOY_PENDIENTE;
}

/* --------------------------------------------------------------------
 * Ponerse al día
 * ------------------------------------------------------------------ */

int
main_planes_dias_atrasados(const PL_PLAN *plan)
{
	int calendario = main_planes_dia_segun_calendario(plan);
	int hoy = main_planes_dia_de_hoy(plan);

	if (!plan || !calendario || calendario <= hoy)
		return 0;
	return calendario - hoy;
}

void
main_planes_marcar_hasta(const PL_PLAN *plan, int dia)
{
	gchar *inicio, *marcas;
	int i;

	if (!plan || dia < 1)
		return;
	if (dia > plan->dias)
		dia = plan->dias;

	progreso_leer(plan, &inicio, &marcas);
	for (i = 0; i < dia; ++i)
		marcas[i] = '1';
	if (!*inicio) {
		g_free(inicio);
		inicio = hoy_iso();
	}
	progreso_guardar(plan, inicio, marcas);
	g_free(inicio);
	g_free(marcas);
}

void
main_planes_reprogramar(const PL_PLAN *plan)
{
	gchar *inicio, *marcas, *nuevo;
	GDate fecha;
	int dia;

	if (!plan)
		return;

	/* El día pendiente pasa a caer hoy: si va por el 7, el plan
	 * empezó hace seis días, no cuando fuera. Nada se marca. */
	dia = main_planes_dia_de_hoy(plan);
	g_date_clear(&fecha, 1);
	g_date_set_time_t(&fecha, time(NULL));
	if (dia > 1)
		g_date_subtract_days(&fecha, dia - 1);
	nuevo = g_strdup_printf("%04d-%02d-%02d",
				g_date_get_year(&fecha),
				g_date_get_month(&fecha),
				g_date_get_day(&fecha));

	progreso_leer(plan, &inicio, &marcas);
	progreso_guardar(plan, nuevo, marcas);
	g_free(inicio);
	g_free(marcas);
	g_free(nuevo);
}

/* --------------------------------------------------------------------
 * Recordatorio diario
 *
 * En settings.xml, junto al resto del estado de los planes:
 *   <estado label="recordatorio" list="1|07:30"/>
 *   <estado label="recordatorio-ultimo" list="2026-09-02"/>
 * Lo segundo es la fecha del último aviso: sin eso, abrir la aplicación
 * tres veces en una tarde daría tres avisos del mismo día.
 * ------------------------------------------------------------------ */

gboolean
main_planes_recordatorio(int *hora, int *minuto)
{
	char *val = xml_get_list_from_label(PL_SECCION, "estado",
					    "recordatorio");
	int activo = 0, h = 7, m = 0;

	if (val && *val) {
		if (sscanf(val, "%d|%d:%d", &activo, &h, &m) != 3)
			activo = 0;
	}
	g_free(val);

	if (h < 0 || h > 23)
		h = 7;
	if (m < 0 || m > 59)
		m = 0;
	if (hora)
		*hora = h;
	if (minuto)
		*minuto = m;
	return (activo == 1);
}

void
main_planes_recordatorio_poner(gboolean activo, int hora, int minuto)
{
	gchar *val = g_strdup_printf("%d|%02d:%02d", activo ? 1 : 0,
				     CLAMP(hora, 0, 23), CLAMP(minuto, 0, 59));

	xml_set_list_item(PL_SECCION, "estado", "recordatorio", val);
	g_free(val);
}

const char *
main_planes_recordatorio_ultimo(void)
{
	static gchar *cache = NULL;
	char *val = xml_get_list_from_label(PL_SECCION, "estado",
					    "recordatorio-ultimo");

	g_clear_pointer(&cache, g_free);
	if (val && *val)
		cache = g_strdup(val);
	g_free(val);
	return cache;
}

void
main_planes_recordatorio_avisado(void)
{
	gchar *hoy = hoy_iso();

	xml_set_list_item(PL_SECCION, "estado", "recordatorio-ultimo", hoy);
	g_free(hoy);
}

/* --------------------------------------------------------------------
 * Capítulos leídos
 *
 * El progreso se guarda por días de plan, que es como se lee y como se
 * marca; para decir cuánto lleva de Josué o de la Biblia entera hay que
 * deshacer ese reparto y quedarse con los capítulos. Se recorren todos
 * los planes, los de la casa y los suyos, y se juntan en un mapa: un
 * capítulo leído dos veces con dos planes distintos cuenta una.
 * ------------------------------------------------------------------ */

#define PL_MAX_CAPS 150		/* Salmos, el libro más largo */

static void
mapa_marca_rango(const PL_VIA *via, int desde, int hasta, gboolean *mapa)
{
	int n, libro, cap;

	for (n = desde; n < hasta; ++n)
		if (via_capitulo(via, n, &libro, &cap) &&
		    cap >= 1 && cap <= PL_MAX_CAPS)
			mapa[libro * PL_MAX_CAPS + (cap - 1)] = TRUE;
}

static void
mapa_marca_dia(const PL_PLAN *plan, int dia, gboolean *mapa)
{
	int i;

	if (plan->tramo_por_dia) {
		const PL_VIA *via = &plan->vias[0];
		if (dia <= via->n_tramos)
			mapa_marca_rango(via, dia - 1, dia, mapa);
		return;
	}
	for (i = 0; i < plan->n_vias; ++i) {
		int desde, hasta;
		via_trozo_del_dia(&plan->vias[i], plan->dias, dia,
				  &desde, &hasta);
		mapa_marca_rango(&plan->vias[i], desde, hasta, mapa);
	}
}

int
main_planes_capitulos_leidos(int *por_libro)
{
	gboolean *mapa = g_new0(gboolean, LB_N * PL_MAX_CAPS);
	int i, libro, cap, total = 0;

	for (i = 0; i < main_planes_cuantos(); ++i) {
		const PL_PLAN *plan = main_planes_get(i);
		gchar *inicio, *marcas;
		int dia;

		if (!plan)
			continue;
		progreso_leer(plan, &inicio, &marcas);
		for (dia = 1; dia <= plan->dias; ++dia)
			if (marcas[dia - 1] == '1')
				mapa_marca_dia(plan, dia, mapa);
		g_free(inicio);
		g_free(marcas);
	}

	for (libro = 0; libro < LB_N; ++libro) {
		int n = 0;
		for (cap = 0; cap < libros[libro].capitulos; ++cap)
			if (mapa[libro * PL_MAX_CAPS + cap])
				++n;
		if (por_libro)
			por_libro[libro] = n;
		total += n;
	}

	g_free(mapa);
	return total;
}
