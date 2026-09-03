/*
 * Biblia Elim
 * recordatorio.c - el aviso diario de la lectura
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

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include "gui/recordatorio.h"
#include "gui/planes_lectura.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/planes_lectura.h"
#include "main/settings.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* Cada minuto se mira el reloj, en vez de calcular un temporizador
 * largo hasta la hora: así el aviso sobrevive a suspender el portátil,
 * a cambiar la hora del sistema y a que el lector mueva la hora del
 * recordatorio mientras la aplicación está abierta. Un latido por
 * minuto no le cuesta nada a nadie. */
#define LATIDO_SEGUNDOS 60
/* El primero, poco después de arrancar: si la hora ya pasó con la
 * aplicación cerrada, el aviso llega al abrirla, no un minuto más
 * tarde. */
#define PRIMER_LATIDO_SEGUNDOS 5

static guint reloj = 0;
static guint32 aviso_id = 0;
static guint suscripcion = 0;

/* --------------------------------------------------------------------
 * El aviso del escritorio
 *
 * Se habla directamente con org.freedesktop.Notifications por D-Bus, que
 * es lo que ya trae GLib: nada de libnotify ni de llamar a notify-send.
 * Si no hay nadie escuchando (un escritorio sin demonio de avisos), se
 * cae con elegancia a la barra de estado.
 * ------------------------------------------------------------------ */

static void
en_accion(GDBusConnection *bus, const gchar *emisor, const gchar *ruta,
	  const gchar *interfaz, const gchar *senal, GVariant *parametros,
	  gpointer datos)
{
	guint32 id = 0;
	const gchar *accion = NULL;

	(void)bus;
	(void)emisor;
	(void)ruta;
	(void)interfaz;
	(void)senal;
	(void)datos;

	g_variant_get(parametros, "(u&s)", &id, &accion);
	if (id != aviso_id)
		return;

	/* Tanto el botón como el clic en el cuerpo del aviso (que los
	 * demonios mandan como "default") llevan a lo mismo: la lectura
	 * abierta y la ventana delante. */
	if (widgets.app)
		gtk_window_present(GTK_WINDOW(widgets.app));
	gui_planes_lectura_hoy();
}

static void
en_respuesta(GObject *fuente, GAsyncResult *res, gpointer datos)
{
	GDBusConnection *bus = G_DBUS_CONNECTION(fuente);
	GError *error = NULL;
	GVariant *ret;

	ret = g_dbus_connection_call_finish(bus, res, &error);
	if (!ret) {
		/* Sin demonio de avisos el recordatorio no se pierde: se
		 * dice donde se pueda. */
		gui_set_statusbar((const gchar *)datos);
		g_message("recordatorio: no se pudo avisar (%s)",
			  error ? error->message : "?");
		g_clear_error(&error);
		g_free(datos);
		return;
	}

	g_variant_get(ret, "(u)", &aviso_id);
	g_variant_unref(ret);
	g_free(datos);

	if (!suscripcion)
		suscripcion = g_dbus_connection_signal_subscribe(
		    bus, NULL, "org.freedesktop.Notifications", "ActionInvoked",
		    "/org/freedesktop/Notifications", NULL,
		    G_DBUS_SIGNAL_FLAGS_NONE, en_accion, NULL, NULL);
}

static void
avisar(const gchar *cuerpo)
{
	GDBusConnection *bus;
	GVariantBuilder acciones, pistas;
	GError *error = NULL;

	bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (!bus) {
		gui_set_statusbar(cuerpo);
		g_message("recordatorio: sin bus de sesión (%s)",
			  error ? error->message : "?");
		g_clear_error(&error);
		return;
	}

	g_variant_builder_init(&acciones, G_VARIANT_TYPE("as"));
	g_variant_builder_add(&acciones, "s", "default");
	g_variant_builder_add(&acciones, "s", _("Abrir la lectura"));
	g_variant_builder_add(&acciones, "s", "abrir");
	g_variant_builder_add(&acciones, "s", _("Abrir la lectura"));

	g_variant_builder_init(&pistas, G_VARIANT_TYPE("a{sv}"));
	g_variant_builder_add(&pistas, "{sv}", "desktop-entry",
			      g_variant_new_string("biblia-elim"));
	g_variant_builder_add(&pistas, "{sv}", "urgency",
			      g_variant_new_byte(1));

	g_dbus_connection_call(bus, "org.freedesktop.Notifications",
			       "/org/freedesktop/Notifications",
			       "org.freedesktop.Notifications", "Notify",
			       g_variant_new("(susssasa{sv}i)",
					     "Biblia Elim",
					     aviso_id,	/* reemplaza el de ayer */
					     "biblia-elim",
					     _("Tu lectura de hoy"),
					     cuerpo,
					     &acciones, &pistas,
					     -1),
			       G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE,
			       -1, NULL, en_respuesta, g_strdup(cuerpo));
}

/* --------------------------------------------------------------------
 * El reloj
 * ------------------------------------------------------------------ */

/* Guardar en el momento: el aviso del día se anota para no repetirlo, y
 * eso tiene que aguantar aunque la aplicación se cierre mal. */
static void
guardar_ya(void)
{
	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);
}

static gboolean
latido(gpointer datos)
{
	GDateTime *ahora;
	const char *ultimo;
	gchar *hoy, *detalle = NULL;
	int hora = 0, minuto = 0;
	gboolean ya_avisado;

	(void)datos;

	if (!main_planes_recordatorio(&hora, &minuto))
		return G_SOURCE_CONTINUE;

	ahora = g_date_time_new_now_local();
	if (g_date_time_get_hour(ahora) * 60 + g_date_time_get_minute(ahora) <
	    hora * 60 + minuto) {
		g_date_time_unref(ahora);
		return G_SOURCE_CONTINUE;
	}

	hoy = g_date_time_format(ahora, "%Y-%m-%d");
	g_date_time_unref(ahora);
	ultimo = main_planes_recordatorio_ultimo();
	ya_avisado = (ultimo && hoy && !strcmp(ultimo, hoy));
	g_free(hoy);
	if (ya_avisado)
		return G_SOURCE_CONTINUE;

	/* Que hoy tocaba avisar queda anotado aunque no haya nada que
	 * decir: si el lector ya marcó su lectura antes de la hora, el
	 * recordatorio se calla, y se calla hasta mañana. */
	main_planes_recordatorio_avisado();
	guardar_ya();

	if (gui_planes_lectura_estado_hoy(&detalle) == PL_HOY_PENDIENTE)
		avisar(detalle);
	g_free(detalle);

	return G_SOURCE_CONTINUE;
}

static gboolean
primer_latido(gpointer datos)
{
	latido(datos);
	reloj = g_timeout_add_seconds(LATIDO_SEGUNDOS, latido, NULL);
	return G_SOURCE_REMOVE;
}

void
gui_recordatorio_arrancar(void)
{
	if (reloj)
		return;
	g_timeout_add_seconds(PRIMER_LATIDO_SEGUNDOS, primer_latido, NULL);
}

void
gui_recordatorio_probar(void)
{
	gchar *detalle = NULL;
	gchar *cuerpo;

	if (gui_planes_lectura_estado_hoy(&detalle) == PL_HOY_PENDIENTE)
		cuerpo = g_strdup(detalle);
	else
		cuerpo = g_strdup(_("Así se verá el recordatorio de cada día."));
	avisar(cuerpo);
	g_free(detalle);
	g_free(cuerpo);
}
