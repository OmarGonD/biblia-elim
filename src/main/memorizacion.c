/*
 * Biblia Elim
 * memorizacion.c - un versículo por semana, con repaso espaciado
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "main/memorizacion.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

#define MEM_SECCION "memoria"

/* Días que espera un versículo en cada caja. El índice es la caja, así
 * que la casilla 0 no se usa. */
static const int espera[MEM_CAJAS + 1] = {0, 1, 2, 4, 8, 15, 30, 90};

/* --------------------------------------------------------------------
 * Fechas
 *
 * A mano, sin g_date_strftime() ni g_date_set_parse(): las dos miran el
 * locale, y esto es un formato de archivo.
 * ------------------------------------------------------------------ */

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

static gchar *
hoy_texto(void)
{
	GDate d;
	gchar buf[16];

	hoy_gdate(&d);
	fecha_texto(&d, buf, sizeof(buf));
	return g_strdup(buf);
}

static gchar *
dentro_de(int dias)
{
	GDate d;
	gchar buf[16];

	hoy_gdate(&d);
	if (dias > 0)
		g_date_add_days(&d, dias);
	fecha_texto(&d, buf, sizeof(buf));
	return g_strdup(buf);
}

/* --------------------------------------------------------------------
 * Leer y escribir
 * ------------------------------------------------------------------ */

static void
verso_libre(gpointer p)
{
	MEM_VERSO *v = p;

	if (!v)
		return;
	g_free(v->clave);
	g_free(v->proximo);
	g_free(v->alta);
	g_free(v);
}

void
main_memoria_libre(GList *lista)
{
	g_list_free_full(lista, verso_libre);
}

static MEM_VERSO *
desde_texto(const char *clave, const char *valor)
{
	gchar **campos = g_strsplit(valor ? valor : "", "|", 5);
	MEM_VERSO *v;

	v = g_new0(MEM_VERSO, 1);
	v->clave = g_strdup(clave);
	v->caja = campos[0] ? atoi(campos[0]) : 1;
	v->proximo = g_strdup((campos[1] && *campos[1]) ? campos[1] : "");
	v->alta = g_strdup((campos[2] && *campos[2]) ? campos[2] : "");
	v->aciertos = campos[3] ? atoi(campos[3]) : 0;
	v->fallos = campos[4] ? atoi(campos[4]) : 0;
	g_strfreev(campos);

	v->caja = CLAMP(v->caja, 1, MEM_CAJAS);
	return v;
}

static void
guardar(const MEM_VERSO *v)
{
	gchar *valor = g_strdup_printf("%d|%s|%s|%d|%d", v->caja,
				       v->proximo ? v->proximo : "",
				       v->alta ? v->alta : "", v->aciertos,
				       v->fallos);

	xml_set_list_item(MEM_SECCION, "verso", v->clave, valor);
	g_free(valor);
}

/* Lo urgente primero: lo más atrasado arriba, y a igualdad, la caja más
 * baja, que es lo que peor se lleva. */
static gint
por_urgencia(gconstpointer a, gconstpointer b)
{
	const MEM_VERSO *x = a, *y = b;
	int c = g_strcmp0(x->proximo, y->proximo);

	if (c)
		return c;
	return x->caja - y->caja;
}

GList *
main_memoria_todos(void)
{
	GList *lista = NULL;

	if (!xml_set_section_ptr(MEM_SECCION))
		return NULL;
	do {
		char *label = xml_get_label();
		char *valor;

		if (!label)
			continue;
		if (!*label) {
			g_free(label);
			continue;
		}
		valor = xml_get_list();
		lista = g_list_prepend(lista, desde_texto(label, valor));
		g_free(valor);
		g_free(label);
	} while (xml_next_item());

	return g_list_sort(lista, por_urgencia);
}

static gboolean
toca_hoy(const MEM_VERSO *v)
{
	GDate hoy, cuando;

	/* Sin fecha, toca: es un alta a medio escribir de otra versión. */
	if (!v->proximo || !*v->proximo)
		return TRUE;
	if (!fecha_parse(v->proximo, &cuando))
		return TRUE;
	hoy_gdate(&hoy);
	return g_date_compare(&cuando, &hoy) <= 0;
}

GList *
main_memoria_de_hoy(void)
{
	GList *todos = main_memoria_todos();
	GList *hoy = NULL, *l;

	for (l = todos; l; l = l->next) {
		MEM_VERSO *v = l->data;

		if (toca_hoy(v))
			hoy = g_list_append(hoy, v);
		else
			verso_libre(v);
	}
	g_list_free(todos);
	return hoy;
}

int
main_memoria_cuantos(void)
{
	GList *l = main_memoria_todos();
	int n = g_list_length(l);

	main_memoria_libre(l);
	return n;
}

int
main_memoria_pendientes(void)
{
	GList *l = main_memoria_de_hoy();
	int n = g_list_length(l);

	main_memoria_libre(l);
	return n;
}

int
main_memoria_asentados(void)
{
	GList *todos = main_memoria_todos(), *l;
	int n = 0;

	for (l = todos; l; l = l->next)
		if (((MEM_VERSO *)l->data)->caja >= MEM_CAJAS)
			++n;
	main_memoria_libre(todos);
	return n;
}

gboolean
main_memoria_tiene(const char *clave)
{
	char *valor;
	gboolean hay;

	if (!clave || !*clave)
		return FALSE;
	valor = xml_get_list_from_label(MEM_SECCION, "verso", clave);
	hay = (valor && *valor);
	g_free(valor);
	return hay;
}

gboolean
main_memoria_anadir(const char *clave)
{
	MEM_VERSO v;

	if (!clave || !*clave || main_memoria_tiene(clave))
		return FALSE;

	/* Caja 1 y para hoy: el primer repaso es el mismo día en que se
	 * elige, que es cuando se tiene fresco. */
	v.clave = (gchar *)clave;
	v.caja = 1;
	v.proximo = hoy_texto();
	v.alta = hoy_texto();
	v.aciertos = 0;
	v.fallos = 0;
	guardar(&v);
	g_free(v.proximo);
	g_free(v.alta);
	return TRUE;
}

void
main_memoria_quitar(const char *clave)
{
	if (!clave || !*clave)
		return;
	xml_remove_node(MEM_SECCION, "verso", clave);
}

void
main_memoria_repasar(const char *clave, gboolean acertado)
{
	char *valor;
	MEM_VERSO *v;

	if (!clave || !*clave)
		return;
	valor = xml_get_list_from_label(MEM_SECCION, "verso", clave);
	if (!valor || !*valor) {
		g_free(valor);
		return;
	}
	v = desde_texto(clave, valor);
	g_free(valor);

	if (acertado) {
		if (v->caja < MEM_CAJAS)
			++v->caja;
		++v->aciertos;
	} else {
		/* A la primera caja entera, no un escalón: si no salió, no
		 * salió, y lo que hace falta es volver a verlo pronto. */
		v->caja = 1;
		++v->fallos;
	}
	g_free(v->proximo);
	v->proximo = dentro_de(espera[v->caja]);
	guardar(v);
	verso_libre(v);
}

/* --------------------------------------------------------------------
 * El ritmo de uno por semana
 * ------------------------------------------------------------------ */

int
main_memoria_altas_de_esta_semana(void)
{
	GList *todos = main_memoria_todos(), *l;
	GDate lunes;
	int n = 0;

	/* La semana empieza el lunes, como en el calendario de constancia. */
	hoy_gdate(&lunes);
	while (g_date_get_weekday(&lunes) != G_DATE_MONDAY)
		g_date_subtract_days(&lunes, 1);

	for (l = todos; l; l = l->next) {
		MEM_VERSO *v = l->data;
		GDate alta;

		if (!fecha_parse(v->alta, &alta))
			continue;
		if (g_date_compare(&alta, &lunes) >= 0)
			++n;
	}
	main_memoria_libre(todos);
	return n;
}
