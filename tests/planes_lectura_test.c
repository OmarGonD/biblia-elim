/*
 * Biblia Elim
 * planes_lectura_test.c - selección diaria de los planes de lectura
 */

#include <glib.h>
#include <stdarg.h>

#include "main/planes_lectura.h"

static gchar *progreso = NULL;

/* planes_lectura.c is compiled with the DEBUG printf guard in this build. */
gchar *
XI_g_strdup_printf(const char *filename, int linenumber,
			 const gchar *format, ...)
{
	va_list args;
	gchar *resultado;

	(void)filename;
	(void)linenumber;
	va_start(args, format);
	resultado = g_strdup_vprintf(format, args);
	va_end(args);
	return resultado;
}

/* Este contrato mínimo de settings mantiene la prueba aislada: el código
 * bajo prueba solo lee el progreso del plan para calcular el día. */
char *
xml_get_list_from_label(const char *section, const char *item,
			const char *label)
{
	if (!g_strcmp0(section, "planes") && !g_strcmp0(item, "plan") &&
	    !g_strcmp0(label, "jesus-7"))
		return g_strdup(progreso);
	return NULL;
}

void
xml_set_list_item(const char *section, const char *item, const char *label,
		  const char *value)
{
	if (!g_strcmp0(section, "planes") && !g_strcmp0(item, "plan") &&
	    !g_strcmp0(label, "jesus-7")) {
		g_free(progreso);
		progreso = g_strdup(value);
	}
}

int
xml_set_section_ptr(const char *section)
{
	(void)section;
	return 0;
}

char *
xml_get_label(void)
{
	return NULL;
}

char *
xml_get_list(void)
{
	return NULL;
}

int
xml_next_item(void)
{
	return 0;
}

void
xml_remove_node(const char *section, const char *item, const char *label)
{
	(void)section;
	(void)item;
	(void)label;
}

void
main_racha_apuntar_hoy(void)
{
}

static gchar *
fecha_hace(gint dias)
{
	GDateTime *ahora = g_date_time_new_now_local();
	GDateTime *fecha = g_date_time_add_days(ahora, -dias);
	gchar *texto = g_date_time_format(fecha, "%Y-%m-%d");

	g_date_time_unref(fecha);
	g_date_time_unref(ahora);
	return texto;
}

static void
prueba_hoy_avanza_aunque_ayer_no_se_marco(void)
{
	const PL_PLAN *plan = main_planes_por_id("jesus-7");
	GDate fecha;

	g_date_clear(&fecha, 1);
	g_date_set_dmy(&fecha, 22, 9, 2026);
	progreso = g_strdup("2026-09-20|0000000");
	g_assert_cmpint(main_planes_dia_para_fecha(plan, &fecha), ==, 3);
	/* El acceso de producción toma la misma selección, con la fecha local. */
	g_assert_cmpint(main_planes_dia_de_hoy(plan), ==,
			main_planes_dia_segun_calendario(plan));
	g_clear_pointer(&progreso, g_free);
}

static void
prueba_atraso_sigue_el_primer_pendiente(void)
{
	const PL_PLAN *plan = main_planes_por_id("jesus-7");
	gchar *inicio = fecha_hace(2);

	progreso = g_strdup_printf("%s|0000000", inicio);
	g_assert_cmpint(main_planes_dias_atrasados(plan), ==, 2);
	g_free(inicio);
	g_clear_pointer(&progreso, g_free);
}

static void
prueba_reprogramar_conserva_el_primer_pendiente(void)
{
	const PL_PLAN *plan = main_planes_por_id("jesus-7");
	gchar *inicio = fecha_hace(3);
	gchar *hoy = fecha_hace(0);
	gchar *esperado;

	progreso = g_strdup_printf("%s|0100000", inicio);
	main_planes_reprogramar(plan);
	esperado = g_strdup_printf("%s|0100000", hoy);
	g_assert_cmpstr(progreso, ==, esperado);
	g_free(esperado);
	g_free(hoy);
	g_free(inicio);
	g_clear_pointer(&progreso, g_free);
}

static void
prueba_sin_inicio_conserva_el_primer_pendiente(void)
{
	const PL_PLAN *plan = main_planes_por_id("jesus-7");

	progreso = g_strdup("|0100000");
	g_assert_cmpint(main_planes_dia_segun_calendario(plan), ==, 0);
	g_assert_cmpint(main_planes_dia_de_hoy(plan), ==, 1);
	g_clear_pointer(&progreso, g_free);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/planes-lectura/hoy-avanza-por-calendario",
			prueba_hoy_avanza_aunque_ayer_no_se_marco);
	g_test_add_func("/planes-lectura/atraso-primer-pendiente",
			prueba_atraso_sigue_el_primer_pendiente);
	g_test_add_func("/planes-lectura/sin-inicio-primer-pendiente",
			prueba_sin_inicio_conserva_el_primer_pendiente);
	g_test_add_func("/planes-lectura/reprogramar-primer-pendiente",
			prueba_reprogramar_conserva_el_primer_pendiente);
	return g_test_run();
}
