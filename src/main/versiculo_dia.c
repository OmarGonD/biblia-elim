/*
 * Biblia Elim
 * versiculo_dia.c - el versículo del día y la reflexión del lector
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

#include "main/versiculo_dia.h"
#include "main/planes_lectura.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* --------------------------------------------------------------------
 * Las citas
 *
 * Citas conocidas de los 66 libros, comprobadas una a una contra el
 * módulo: el motor, cuando le pides un versículo que no existe, no se
 * queja, te devuelve otro; así que la tabla se validó pidiéndolas todas
 * y mirando que volviera la misma referencia que se pidió.
 *
 * El orden está barajado a posta y luego congelado. Si fueran en orden
 * canónico, enero entero sería Génesis y diciembre entero Apocalipsis.
 * Barajarlas una vez y dejarlas quietas da variedad sin perder que la
 * cita de una fecha sea siempre la misma para todo el mundo.
 * ------------------------------------------------------------------ */

typedef struct {
	const char *libro;	/* nombre OSIS: "John" */
	short cap;
	short verso;
} VD_CITA;

static const VD_CITA citas[] = {
    {"Isa", 12, 2}, {"Luke", 12, 7}, {"Prov", 2, 6},
    {"Judg", 6, 12}, {"Jas", 5, 16}, {"John", 3, 16},
    {"Mal", 3, 10}, {"Mark", 10, 27}, {"2Cor", 5, 7},
    {"Job", 1, 21}, {"John", 14, 1}, {"Rom", 8, 18},
    {"John", 6, 35}, {"Luke", 9, 23}, {"1Kgs", 8, 23},
    {"Phil", 4, 8}, {"Col", 3, 12}, {"Rom", 15, 13},
    {"Rom", 8, 28}, {"Acts", 2, 38}, {"Phil", 4, 19},
    {"Gal", 6, 2}, {"Ps", 86, 15}, {"Acts", 16, 31},
    {"Luke", 4, 18}, {"Jer", 29, 11}, {"Col", 3, 15},
    {"Ps", 34, 8}, {"Gen", 22, 14}, {"Mark", 12, 30},
    {"Num", 23, 19}, {"Zeph", 3, 17}, {"Jas", 4, 7},
    {"Ps", 34, 18}, {"Matt", 7, 12}, {"Gen", 1, 27},
    {"Rev", 21, 5}, {"John", 13, 34}, {"John", 15, 7},
    {"Ps", 46, 10}, {"Ps", 19, 1}, {"Joel", 2, 13},
    {"Isa", 66, 13}, {"Deut", 32, 4}, {"Ps", 51, 17},
    {"Deut", 10, 12}, {"Heb", 11, 1}, {"Col", 1, 16},
    {"Isa", 9, 6}, {"Ps", 23, 1}, {"1John", 4, 18},
    {"Prov", 11, 25}, {"Jer", 31, 3}, {"Rev", 19, 6},
    {"1Pet", 5, 6}, {"Ps", 18, 2}, {"Matt", 28, 20},
    {"Ps", 32, 8}, {"John", 15, 12}, {"John", 20, 29},
    {"Eph", 6, 10}, {"Ps", 116, 1}, {"Jer", 1, 5},
    {"Matt", 11, 29}, {"Song", 2, 4}, {"Eph", 2, 10},
    {"Ps", 16, 11}, {"Exod", 20, 12}, {"Gen", 12, 2},
    {"1Pet", 1, 3}, {"Isa", 53, 5}, {"Titus", 3, 5},
    {"2Pet", 3, 9}, {"Josh", 1, 8}, {"Phlm", 1, 6},
    {"Job", 23, 10}, {"Prov", 18, 10}, {"Isa", 26, 3},
    {"Prov", 16, 24}, {"Rev", 3, 20}, {"Ps", 121, 2},
    {"Josh", 24, 15}, {"Rom", 1, 16}, {"2Cor", 1, 3},
    {"Ps", 91, 2}, {"Isa", 64, 8}, {"Rom", 12, 12},
    {"Deut", 31, 6}, {"Phil", 4, 7}, {"Prov", 3, 9},
    {"John", 10, 28}, {"Ps", 119, 9}, {"Mark", 10, 45},
    {"Matt", 6, 26}, {"Mic", 7, 7}, {"Song", 8, 7},
    {"Ps", 27, 14}, {"2Pet", 1, 3}, {"Isa", 30, 15},
    {"Zech", 4, 6}, {"Gen", 9, 13}, {"Ps", 19, 14},
    {"Matt", 6, 33}, {"Matt", 6, 21}, {"Eph", 2, 8},
    {"Jer", 29, 13}, {"Heb", 4, 16}, {"Isa", 54, 10},
    {"Hos", 6, 3}, {"1Pet", 4, 10}, {"2John", 1, 6},
    {"Heb", 10, 24}, {"Ps", 121, 7}, {"1Chr", 29, 11},
    {"John", 1, 14}, {"John", 1, 12}, {"Isa", 49, 15},
    {"Matt", 19, 26}, {"Luke", 6, 38}, {"Ps", 103, 2},
    {"Isa", 41, 13}, {"1Tim", 2, 5}, {"1Chr", 16, 34},
    {"Lam", 3, 23}, {"Rom", 8, 26}, {"Eph", 4, 32},
    {"1Thess", 5, 17}, {"1Cor", 13, 13}, {"Luke", 2, 11},
    {"2Cor", 12, 9}, {"Eccl", 4, 9}, {"Amos", 5, 24},
    {"Ps", 9, 9}, {"Phil", 4, 4}, {"2Cor", 4, 16},
    {"1Tim", 4, 12}, {"Prov", 1, 7}, {"Ps", 71, 5},
    {"Prov", 19, 21}, {"Acts", 20, 24}, {"Ps", 90, 12},
    {"Luke", 18, 27}, {"Ps", 63, 1}, {"2Chr", 20, 15},
    {"Esth", 4, 14}, {"Ps", 91, 1}, {"Matt", 6, 9},
    {"Luke", 1, 37}, {"Jer", 33, 3}, {"Phil", 4, 6},
    {"Lam", 3, 25}, {"Ps", 8, 1}, {"1John", 4, 10},
    {"Dan", 2, 20}, {"Isa", 58, 11}, {"1Thess", 5, 16},
    {"Eph", 1, 7}, {"Ruth", 1, 16}, {"2Tim", 2, 15},
    {"Ps", 51, 10}, {"Ps", 20, 7}, {"Gen", 15, 6},
    {"1Cor", 13, 4}, {"Prov", 3, 6}, {"Col", 3, 2},
    {"Jas", 1, 5}, {"Rev", 5, 12}, {"Luke", 11, 9},
    {"Isa", 53, 6}, {"Mark", 16, 15}, {"John", 14, 6},
    {"Prov", 31, 30}, {"Jer", 32, 17}, {"Luke", 10, 27},
    {"Jer", 17, 7}, {"Exod", 14, 14}, {"Gen", 2, 24},
    {"Hab", 3, 19}, {"Rom", 10, 9}, {"Prov", 16, 3},
    {"1Tim", 6, 6}, {"Isa", 41, 10}, {"Phil", 2, 5},
    {"Num", 6, 24}, {"2Thess", 3, 3}, {"Rom", 5, 8},
    {"Zech", 9, 9}, {"2Tim", 4, 7}, {"2Cor", 9, 7},
    {"Isa", 30, 21}, {"Prov", 12, 25}, {"Gal", 2, 20},
    {"Ps", 23, 6}, {"John", 4, 14}, {"2Tim", 3, 16},
    {"Gal", 6, 9}, {"Dan", 12, 3}, {"Prov", 28, 13},
    {"Isa", 53, 4}, {"Job", 42, 2}, {"Ps", 55, 22},
    {"Prov", 3, 5}, {"Isa", 40, 29}, {"Gal", 5, 1},
    {"1Cor", 10, 31}, {"Ps", 31, 24}, {"Isa", 43, 19},
    {"Ps", 37, 23}, {"Jonah", 2, 2}, {"1Thess", 4, 16},
    {"Ps", 119, 105}, {"Rev", 21, 4}, {"Acts", 4, 12},
    {"John", 16, 33}, {"Isa", 55, 11}, {"Prov", 16, 9},
    {"Dan", 3, 17}, {"Ezra", 8, 22}, {"1Thess", 5, 24},
    {"Heb", 12, 1}, {"Jude", 1, 24}, {"Isa", 61, 1},
    {"Ps", 5, 3}, {"Rom", 12, 21}, {"1Cor", 1, 9},
    {"1Cor", 15, 10}, {"Isa", 40, 8}, {"Matt", 9, 37},
    {"1Thess", 5, 11}, {"Hag", 2, 9}, {"1Cor", 2, 9},
    {"1John", 5, 14}, {"Prov", 29, 25}, {"Joel", 2, 25},
    {"Lev", 26, 12}, {"Ps", 84, 11}, {"Ps", 107, 1},
    {"Mark", 9, 23}, {"1Tim", 6, 12}, {"Ps", 73, 26},
    {"Ps", 1, 1}, {"Isa", 43, 2}, {"Ps", 118, 24},
    {"2Cor", 9, 8}, {"2Chr", 7, 14}, {"2Tim", 1, 7},
    {"Rom", 8, 38}, {"Ps", 100, 5}, {"Matt", 18, 20},
    {"Obad", 1, 15}, {"Ps", 127, 1}, {"Deut", 4, 29},
    {"Ps", 33, 4}, {"Prov", 9, 10}, {"Matt", 5, 14},
    {"Phil", 4, 13}, {"Isa", 40, 31}, {"Mark", 1, 17},
    {"Col", 3, 23}, {"Gen", 1, 1}, {"1Cor", 10, 13},
    {"Ps", 146, 5}, {"Ps", 145, 8}, {"Mic", 6, 8},
    {"Matt", 5, 16}, {"Heb", 6, 19}, {"Jas", 1, 17},
    {"Heb", 13, 8}, {"Matt", 28, 19}, {"Mark", 11, 24},
    {"Acts", 17, 28}, {"3John", 1, 4}, {"Heb", 13, 5},
    {"Ps", 139, 14}, {"2Sam", 22, 2}, {"Gen", 8, 22},
    {"Rom", 8, 14}, {"Gal", 5, 22}, {"Lev", 19, 18},
    {"Ps", 91, 11}, {"Rom", 8, 37}, {"Eccl", 12, 13},
    {"Rom", 12, 2}, {"Prov", 4, 23}, {"Rom", 3, 23},
    {"Ps", 3, 3}, {"Ps", 56, 3}, {"John", 11, 25},
    {"Phil", 2, 3}, {"Matt", 5, 6}, {"1Cor", 15, 57},
    {"Ps", 37, 7}, {"Ps", 89, 1}, {"Ps", 37, 4},
    {"Ps", 27, 1}, {"John", 1, 1}, {"Gen", 18, 14},
    {"Ps", 36, 7}, {"Rom", 6, 23}, {"Deut", 7, 9},
    {"Eph", 5, 2}, {"Prov", 31, 25}, {"Luke", 15, 7},
    {"Isa", 46, 4}, {"Ps", 8, 4}, {"Ps", 94, 19},
    {"Matt", 7, 7}, {"Eccl", 3, 1}, {"Heb", 4, 12},
    {"1John", 1, 9}, {"2Thess", 3, 16}, {"Ps", 40, 1},
    {"Exod", 3, 14}, {"Josh", 1, 9}, {"Hab", 2, 4},
    {"Ps", 103, 8}, {"Phil", 1, 6}, {"Ps", 119, 11},
    {"Titus", 2, 11}, {"1Chr", 16, 11}, {"Ps", 126, 3},
    {"Ps", 95, 6}, {"Heb", 12, 2}, {"Prov", 17, 22},
    {"Rom", 12, 1}, {"Ps", 28, 7}, {"Ps", 30, 5},
    {"Hos", 14, 4}, {"Ps", 147, 3}, {"Ps", 143, 8},
    {"Prov", 18, 24}, {"Eccl", 3, 11}, {"Ps", 133, 1},
    {"Rom", 8, 1}, {"1John", 3, 16}, {"Matt", 11, 28},
    {"Ps", 37, 5}, {"1Pet", 2, 9}, {"Gen", 50, 20},
    {"1Sam", 16, 7}, {"Rom", 5, 1}, {"John", 14, 27},
    {"Heb", 10, 23}, {"Rom", 8, 31}, {"Col", 2, 6},
    {"Ps", 66, 5}, {"Isa", 43, 1}, {"Lam", 3, 22},
    {"1Cor", 6, 19}, {"1John", 3, 1}, {"Ps", 62, 8},
    {"Ps", 46, 1}, {"Isa", 40, 28}, {"1Pet", 3, 15},
    {"Prov", 15, 1}, {"Matt", 5, 9}, {"Ps", 27, 4},
    {"1Cor", 16, 14}, {"2Kgs", 6, 16}, {"Exod", 15, 2},
    {"Col", 3, 16}, {"Prov", 22, 6}, {"Ps", 23, 4},
    {"Ps", 24, 1}, {"2Cor", 5, 17}, {"Ps", 136, 1},
    {"Matt", 4, 4}, {"Exod", 33, 14}, {"2Cor", 4, 17},
    {"Deut", 8, 3}, {"1Sam", 2, 2}, {"Isa", 55, 6},
    {"Luke", 21, 33}, {"John", 8, 12}, {"Matt", 22, 37},
    {"Jas", 1, 12}, {"Prov", 30, 5}, {"Ps", 25, 4},
    {"Exod", 34, 6}, {"Heb", 11, 6}, {"Rev", 22, 13},
    {"Prov", 27, 17}, {"Ps", 16, 8}, {"Rev", 4, 11},
    {"Luke", 19, 10}, {"1Thess", 5, 18}, {"Ps", 119, 130},
    {"Isa", 25, 1}, {"Ps", 100, 4}, {"Luke", 6, 31},
    {"Ezek", 36, 26}, {"1John", 4, 7}, {"Isa", 55, 8},
    {"Jas", 1, 22}, {"Deut", 31, 8}, {"Job", 19, 25},
    {"Isa", 7, 14}, {"Ps", 29, 11}, {"Prov", 17, 17},
    {"Acts", 20, 35}, {"Isa", 1, 18}, {"Deut", 33, 27},
    {"Matt", 16, 24}, {"Eph", 3, 20}, {"Ps", 42, 1},
    {"Ezek", 34, 16}, {"Isa", 6, 8}, {"Ps", 42, 11},
    {"Gen", 28, 15}, {"John", 3, 17}, {"John", 10, 10},
    {"Nah", 1, 7}, {"1Pet", 5, 7}, {"1John", 4, 16},
    {"2Cor", 3, 17}, {"Neh", 9, 6}, {"2Sam", 22, 31},
    {"Ps", 9, 10}, {"Ps", 138, 8}, {"Dan", 6, 10},
    {"Ps", 103, 12}, {"John", 15, 5}, {"Mal", 3, 6},
    {"Neh", 8, 10}, {"Ps", 1, 2}, {"Ps", 62, 1},
    {"Ps", 121, 1}, {"Deut", 6, 5}, {"1Sam", 12, 24},
    {"2Chr", 16, 9}, {"Ps", 130, 5}, {"Phil", 3, 13},
    {"Eph", 4, 2}, {"John", 17, 3}, {"Jas", 4, 8},
    {"Ps", 4, 8}, {"Jas", 1, 2}, {"Jas", 1, 19},
    {"Rev", 1, 8}, {"1John", 4, 19}, {"Ps", 139, 23},
    {"Rom", 10, 17}, {"Gal", 5, 13}, {"1Cor", 15, 58},
    {"Acts", 1, 8}, {"Eph", 6, 11}, {"Ps", 145, 18},
    {"John", 10, 11}, {"John", 8, 32}, {"Ps", 150, 6}
};

#define VD_N ((int)(sizeof(citas) / sizeof(citas[0])))

/* Sección de settings.xml donde vive la reflexión de cada día. */
#define VD_SECCION "versiculo"

int
main_versiculo_cuantos(void)
{
	return VD_N;
}

/* Qué cita toca. Se cuenta por días transcurridos, no por día del año:
 * la tabla no mide un año justo, así que la rueda va corriendo y una
 * misma fecha no repite versículo de un año para otro. El día juliano
 * es justo ese número corrido, y sale de la fecha del calendario, con
 * lo que no se tuerce con los bisiestos ni depende de la hora. */
static const VD_CITA *
cita_de(GDateTime *fecha)
{
	GDate dia;
	guint32 juliano;

	if (!fecha)
		return &citas[0];

	g_date_clear(&dia, 1);
	g_date_set_dmy(&dia, g_date_time_get_day_of_month(fecha),
		       g_date_time_get_month(fecha),
		       g_date_time_get_year(fecha));
	if (!g_date_valid(&dia))
		return &citas[0];

	juliano = g_date_get_julian(&dia);
	return &citas[juliano % VD_N];
}

gchar *
main_versiculo_cita(GDateTime *fecha)
{
	const VD_CITA *c = cita_de(fecha);
	int libro = main_planes_libro_por_osis(c->libro);

	if (libro < 0)
		return g_strdup_printf("%s %d:%d", c->libro, c->cap, c->verso);
	return g_strdup_printf("%s %d:%d", _(main_planes_libro_nombre(libro)),
			       c->cap, c->verso);
}

gchar *
main_versiculo_clave(GDateTime *fecha)
{
	const VD_CITA *c = cita_de(fecha);

	return g_strdup_printf("%s %d:%d", c->libro, c->cap, c->verso);
}

/* --------------------------------------------------------------------
 * La reflexión
 * ------------------------------------------------------------------ */

gchar *
main_versiculo_fecha(GDateTime *fecha)
{
	if (!fecha)
		return NULL;
	return g_date_time_format(fecha, "%Y-%m-%d");
}

gchar *
main_versiculo_reflexion(const char *fecha)
{
	char *val;
	gchar *texto;

	if (!fecha || !*fecha)
		return NULL;
	val = xml_get_list_from_label(VD_SECCION, "reflexion", fecha);
	if (!val || !*val) {
		g_free(val);
		return NULL;
	}
	texto = g_uri_unescape_string(val, NULL);
	g_free(val);
	if (texto && !*texto) {
		g_free(texto);
		return NULL;
	}
	return texto;
}

void
main_versiculo_reflexion_poner(const char *fecha, const char *texto)
{
	gchar *limpio, *escapado;

	if (!fecha || !*fecha)
		return;

	/* Un cuadro de texto que solo tiene espacios y saltos de línea
	 * está vacío para el lector; que lo esté también en el archivo. */
	limpio = texto ? g_strdup(texto) : NULL;
	if (limpio)
		g_strstrip(limpio);

	if (!limpio || !*limpio) {
		xml_remove_node(VD_SECCION, "reflexion", fecha);
		g_free(limpio);
		return;
	}

	escapado = g_uri_escape_string(limpio, NULL, TRUE);
	xml_set_list_item(VD_SECCION, "reflexion", fecha, escapado);
	g_free(escapado);
	g_free(limpio);
}

int
main_versiculo_reflexiones_cuantas(void)
{
	int n = 0;

	if (!xml_set_section_ptr(VD_SECCION))
		return 0;
	do {
		char *label = xml_get_label();
		char *valor;

		if (!label)
			continue;
		valor = xml_get_list();
		if (valor && *valor)
			++n;
		g_free(valor);
		g_free(label);
	} while (xml_next_item());

	return n;
}
