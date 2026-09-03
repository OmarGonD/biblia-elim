/*
 * Biblia Elim
 * racha.c - los días que el lector de verdad leyó
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

#include "main/racha.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

#define RA_SECCION "racha"

/* Las fechas se leen una vez y se quedan en memoria. El calendario
 * pregunta por trescientos y pico días cada vez que se repinta, y
 * recorrer el XML entero en cada casilla sería absurdo. */
static GHashTable *dias = NULL;
static gchar *primera = NULL;

static void
cargar(void)
{
	if (dias)
		return;

	dias = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	if (!xml_set_section_ptr(RA_SECCION))
		return;
	do {
		char *label = xml_get_label();

		if (!label)
			continue;
		if (*label) {
			if (!primera || strcmp(label, primera) < 0) {
				g_free(primera);
				primera = g_strdup(label);
			}
			g_hash_table_add(dias, label); /* se queda con label */
		} else
			g_free(label);
	} while (xml_next_item());
}

/* --------------------------------------------------------------------
 * Apuntar
 * ------------------------------------------------------------------ */

void
main_racha_apuntar(const char *fecha)
{
	if (!fecha || !*fecha)
		return;
	cargar();

	if (g_hash_table_contains(dias, fecha))
		return;

	xml_set_list_item(RA_SECCION, "dia", fecha, "1");
	g_hash_table_add(dias, g_strdup(fecha));
	if (!primera || strcmp(fecha, primera) < 0) {
		g_free(primera);
		primera = g_strdup(fecha);
	}
}

void
main_racha_apuntar_hoy(void)
{
	GDateTime *ahora = g_date_time_new_now_local();
	gchar *hoy = g_date_time_format(ahora, "%Y-%m-%d");

	g_date_time_unref(ahora);
	main_racha_apuntar(hoy);
	g_free(hoy);
}

gboolean
main_racha_dia(const char *fecha)
{
	if (!fecha || !*fecha)
		return FALSE;
	cargar();
	return g_hash_table_contains(dias, fecha);
}

/* --------------------------------------------------------------------
 * Contar
 * ------------------------------------------------------------------ */

/* Las fechas se escriben y se leen a mano, sin g_date_strftime() ni
 * g_date_set_parse(): las dos miran el locale, y esta cadena es un
 * formato de archivo, igual en Lima que en Tokio. */
static void
fecha_texto(const GDate *d, gchar *buf, gsize n)
{
	g_snprintf(buf, n, "%04d-%02d-%02d", g_date_get_year(d),
		   g_date_get_month(d), g_date_get_day(d));
}

static gboolean
fecha_parse(const char *s, GDate *d)
{
	int anio, mes, dia;

	if (!s || sscanf(s, "%4d-%2d-%2d", &anio, &mes, &dia) != 3)
		return FALSE;
	if (!g_date_valid_dmy(dia, mes, anio))
		return FALSE;
	g_date_clear(d, 1);
	g_date_set_dmy(d, dia, mes, anio);
	return TRUE;
}

static gboolean
hubo(const GDate *d)
{
	gchar buf[16];

	if (!g_date_valid(d))
		return FALSE;
	fecha_texto(d, buf, sizeof(buf));
	return main_racha_dia(buf);
}

static void
hoy_gdate(GDate *d)
{
	GDateTime *ahora = g_date_time_new_now_local();

	g_date_clear(d, 1);
	g_date_set_dmy(d, g_date_time_get_day_of_month(ahora),
		       g_date_time_get_month(ahora),
		       g_date_time_get_year(ahora));
	g_date_time_unref(ahora);
}

/* El ancla desde la que se cuenta hacia atrás: hoy si ya se leyó, ayer
 * si no. FALSE cuando no hay racha viva. */
static gboolean
ancla(GDate *fuera, gboolean *desde_ayer)
{
	GDate d;

	hoy_gdate(&d);
	if (desde_ayer)
		*desde_ayer = FALSE;
	if (hubo(&d)) {
		*fuera = d;
		return TRUE;
	}

	g_date_subtract_days(&d, 1);
	if (hubo(&d)) {
		if (desde_ayer)
			*desde_ayer = TRUE;
		*fuera = d;
		return TRUE;
	}
	return FALSE;
}

int
main_racha_actual(void)
{
	GDate d;
	int n = 0;

	cargar();
	if (!ancla(&d, NULL))
		return 0;

	while (hubo(&d)) {
		++n;
		if (g_date_get_julian(&d) <= 1)
			break;	/* no hay ayer que valga */
		g_date_subtract_days(&d, 1);
	}
	return n;
}

gboolean
main_racha_hoy_pendiente(void)
{
	GDate d;
	gboolean ayer = FALSE;

	cargar();
	if (!ancla(&d, &ayer))
		return FALSE;
	return ayer;
}

int
main_racha_total(void)
{
	cargar();
	return (int)g_hash_table_size(dias);
}

const char *
main_racha_desde(void)
{
	cargar();
	return primera;
}

static gint
por_fecha(gconstpointer a, gconstpointer b)
{
	return strcmp((const char *)a, (const char *)b);
}

int
main_racha_mejor(void)
{
	GList *lista, *l;
	GDate anterior;
	int mejor = 0, seguidos = 0;

	cargar();
	lista = g_hash_table_get_keys(dias);
	lista = g_list_sort(lista, por_fecha);

	g_date_clear(&anterior, 1);
	for (l = lista; l; l = l->next) {
		GDate d;

		if (!fecha_parse((const char *)l->data, &d))
			continue;

		if (seguidos && g_date_valid(&anterior)) {
			GDate siguiente = anterior;
			g_date_add_days(&siguiente, 1);
			seguidos = (g_date_compare(&siguiente, &d) == 0)
				       ? seguidos + 1
				       : 1;
		} else
			seguidos = 1;

		if (seguidos > mejor)
			mejor = seguidos;
		anterior = d;
	}

	g_list_free(lista);
	return mejor;
}
