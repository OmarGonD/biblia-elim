/*
 * Biblia Elim
 * recordatorio.c - el aviso diario con la aplicación cerrada
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
#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

#include "main/recordatorio.h"
#include "main/planes_lectura.h"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* Lo que el aviso espera con la notificación en pantalla por si el
 * lector la pulsa. Pasado ese rato el proceso se va: nadie va a hacer
 * clic en el aviso de esta mañana a media tarde, y un proceso colgado
 * todo el día para nada no tiene ninguna gracia. */
#define ESPERA_MAXIMA_SEGUNDOS 900

#define UNIDAD_BASE "biblia-elim-recordatorio"

/* --------------------------------------------------------------------
 * Dónde están las cosas
 *
 * Se repite aquí el cálculo de settings_init() a propósito: llamarla
 * traería Sword, las listas de módulos y hasta diálogos de instalación.
 * Son dos g_build_filename(); el precio de la copia es más bajo que el
 * de arrancar media aplicación para leer una hora.
 * ------------------------------------------------------------------ */

static gchar *
dir_config(void)
{
	const char *home = g_getenv("HOME");
	gchar *nuevo, *viejo;

	if (!home || !*home)
		home = g_get_home_dir();

#ifdef WIN32
	return g_build_filename(home, "xiphos", NULL);
#else
	nuevo = g_build_filename(home, ".config", "xiphos", NULL);
	if (g_file_test(nuevo, G_FILE_TEST_IS_DIR))
		return nuevo;

	/* Instalaciones de antes de la mudanza a ~/.config. */
	viejo = g_build_filename(home, ".xiphos", NULL);
	if (g_file_test(viejo, G_FILE_TEST_IS_DIR)) {
		g_free(nuevo);
		return viejo;
	}
	g_free(viejo);
	return nuevo;
#endif
}

gchar *
main_recordatorio_ruta_settings(void)
{
	gchar *dir = dir_config();
	gchar *ruta = g_build_filename(dir, "settings.xml", NULL);

	g_free(dir);
	return ruta;
}

/* El binario que está corriendo ahora mismo, que es el que tiene que
 * poner el temporizador en ExecStart: así vale igual instalado en
 * ~/.local/bin que recién compilado en el directorio de trabajo. */
static gchar *
ruta_binario(void)
{
#ifndef WIN32
	gchar *yo = g_file_read_link("/proc/self/exe", NULL);

	if (yo && g_file_test(yo, G_FILE_TEST_IS_EXECUTABLE))
		return yo;
	g_free(yo);
#endif
	return g_find_program_in_path("biblia-elim");
}

/* --------------------------------------------------------------------
 * El cerrojo
 *
 * Un flock que la aplicación coge al arrancar y suelta el núcleo al
 * cerrarse, pase lo que pase: no hay pid que se quede rancio ni fichero
 * que limpiar tras un cuelgue.
 * ------------------------------------------------------------------ */

gboolean
main_recordatorio_cerrojo(void)
{
#ifdef WIN32
	return TRUE;
#else
	static int fd = -1;
	gchar *dir, *ruta;

	if (fd >= 0)
		return TRUE;

	dir = dir_config();
	g_mkdir_with_parents(dir, 0700);
	ruta = g_build_filename(dir, "recordatorio.lock", NULL);
	g_free(dir);

	fd = g_open(ruta, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	g_free(ruta);
	if (fd < 0)
		return FALSE;

	if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
		close(fd);
		fd = -1;
		return FALSE;
	}
	return TRUE;
#endif
}

/* --------------------------------------------------------------------
 * El aviso del escritorio
 *
 * Como en la aplicación, se habla directamente con
 * org.freedesktop.Notifications por D-Bus. Aquí las llamadas van
 * síncronas porque el proceso no tiene nada mejor que hacer, y se queda
 * escuchando por si el lector pulsa el aviso: entonces abre la
 * aplicación, que es lo que estaba pidiendo.
 * ------------------------------------------------------------------ */

typedef struct {
	GMainLoop *bucle;
	guint32 id;
	guint tiempo;	/* 0 cuando el plazo ya venció y se retiró solo */
	gboolean abrir;
} ESPERA;

static void
en_senal(GDBusConnection *bus, const gchar *emisor, const gchar *ruta,
	 const gchar *interfaz, const gchar *senal, GVariant *parametros,
	 gpointer datos)
{
	ESPERA *e = datos;
	guint32 id = 0;

	(void)bus;
	(void)emisor;
	(void)ruta;
	(void)interfaz;

	g_variant_get_child(parametros, 0, "u", &id);
	if (id != e->id)
		return;

	/* Tanto el botón como el clic en el cuerpo (que los demonios
	 * mandan como "default") quieren lo mismo: abrir la aplicación.
	 * Cerrar el aviso sin más solo termina la espera. */
	if (!g_strcmp0(senal, "ActionInvoked"))
		e->abrir = TRUE;
	g_main_loop_quit(e->bucle);
}

static gboolean
se_acabo_la_espera(gpointer datos)
{
	ESPERA *e = datos;

	e->tiempo = 0;
	g_main_loop_quit(e->bucle);
	return G_SOURCE_REMOVE;
}

/* Manda el aviso y espera. TRUE si el aviso llegó a salir. */
static gboolean
avisar(const gchar *cuerpo)
{
	GDBusConnection *bus;
	GVariantBuilder acciones, pistas;
	GError *error = NULL;
	GVariant *ret;
	ESPERA e = {NULL, 0, 0, FALSE};
	guint sub_accion, sub_cierre;

	bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
	if (!bus) {
		g_printerr("recordatorio: sin bus de sesión (%s)\n",
			   error ? error->message : "?");
		g_clear_error(&error);
		return FALSE;
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

	ret = g_dbus_connection_call_sync(
	    bus, "org.freedesktop.Notifications",
	    "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
	    "Notify",
	    g_variant_new("(susssasa{sv}i)", "Biblia Elim", 0u, "biblia-elim",
			  _("Tu lectura de hoy"), cuerpo, &acciones, &pistas,
			  -1),
	    G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
	if (!ret) {
		g_printerr("recordatorio: no se pudo avisar (%s)\n",
			   error ? error->message : "?");
		g_clear_error(&error);
		g_object_unref(bus);
		return FALSE;
	}
	g_variant_get(ret, "(u)", &e.id);
	g_variant_unref(ret);

	e.bucle = g_main_loop_new(NULL, FALSE);
	sub_accion = g_dbus_connection_signal_subscribe(
	    bus, NULL, "org.freedesktop.Notifications", "ActionInvoked",
	    "/org/freedesktop/Notifications", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
	    en_senal, &e, NULL);
	sub_cierre = g_dbus_connection_signal_subscribe(
	    bus, NULL, "org.freedesktop.Notifications", "NotificationClosed",
	    "/org/freedesktop/Notifications", NULL, G_DBUS_SIGNAL_FLAGS_NONE,
	    en_senal, &e, NULL);
	e.tiempo = g_timeout_add_seconds(ESPERA_MAXIMA_SEGUNDOS,
					 se_acabo_la_espera, &e);

	g_main_loop_run(e.bucle);

	/* Si el bucle salió por el plazo, la fuente ya se retiró sola y
	 * quitarla otra vez solo saca un aviso por la consola. */
	if (e.tiempo)
		g_source_remove(e.tiempo);
	g_dbus_connection_signal_unsubscribe(bus, sub_accion);
	g_dbus_connection_signal_unsubscribe(bus, sub_cierre);
	g_main_loop_unref(e.bucle);
	g_object_unref(bus);

	if (e.abrir) {
		gchar *yo = ruta_binario();
		gchar *argv[2];

		if (yo) {
			argv[0] = yo;
			argv[1] = NULL;
			g_spawn_async(NULL, argv, NULL,
				      G_SPAWN_SEARCH_PATH |
					  G_SPAWN_STDOUT_TO_DEV_NULL |
					  G_SPAWN_STDERR_TO_DEV_NULL,
				      NULL, NULL, NULL, NULL);
			g_free(yo);
		}
	}
	return TRUE;
}

/* --------------------------------------------------------------------
 * El aviso de una vez
 * ------------------------------------------------------------------ */

int
main_recordatorio_una_vez(void)
{
	gchar *ruta, *detalle = NULL, *hoy;
	const char *ultimo;
	GDateTime *ahora;
	gboolean repetido, pendiente;

	/* Si la aplicación está abierta, ella lleva el recordatorio: dos
	 * avisos del mismo día son peor que ninguno, y además escribir
	 * settings.xml por detrás se lo pisaría al guardar. */
	if (!main_recordatorio_cerrojo())
		return 0;

	ruta = main_recordatorio_ruta_settings();
	if (!xml_parse_settings_file(ruta)) {
		/* Todavía no se ha abierto nunca: no hay nada que recordar. */
		g_free(ruta);
		return 0;
	}

	/* La hora la manda el temporizador; aquí solo se mira si el
	 * recordatorio sigue puesto, por si se apagó y quedó una unidad
	 * vieja rondando. */
	if (!main_planes_recordatorio(NULL, NULL)) {
		g_free(ruta);
		return 0;
	}

	ahora = g_date_time_new_now_local();
	hoy = g_date_time_format(ahora, "%Y-%m-%d");
	g_date_time_unref(ahora);
	ultimo = main_planes_recordatorio_ultimo();
	repetido = (ultimo && hoy && !strcmp(ultimo, hoy));
	g_free(hoy);
	if (repetido) {
		/* Ya avisó hoy la aplicación, o este mismo temporizador
		 * en un arranque anterior (Persistent=true lo dispara al
		 * encender si el equipo estaba apagado a la hora). */
		g_free(ruta);
		return 0;
	}

	pendiente = (main_planes_estado_hoy(&detalle) == PL_HOY_PENDIENTE);

	/* Si no hay nada que decir se anota igual y se calla hasta
	 * mañana; si lo hay, solo se anota cuando el aviso salió de
	 * verdad, para que un escritorio sin demonio de avisos no se
	 * coma el recordatorio del día sin haberlo enseñado. */
	if (pendiente && !avisar(detalle)) {
		g_free(detalle);
		g_free(ruta);
		return 1;
	}
	g_free(detalle);

	main_planes_recordatorio_avisado();
	xml_save_settings_doc(ruta);
	g_free(ruta);
	return 0;
}

/* --------------------------------------------------------------------
 * Las unidades de systemd
 *
 * El temporizador es de usuario y vive en ~/.config/systemd/user: no
 * hace falta root ni tocar nada del sistema, y corre mientras el lector
 * tenga sesión abierta. La hora la escribe la aplicación cada vez que
 * cambia, así que la unidad y la casilla del diálogo no se separan.
 * ------------------------------------------------------------------ */

#ifndef WIN32

static gchar *
dir_unidades(void)
{
	return g_build_filename(g_get_user_config_dir(), "systemd", "user",
				NULL);
}

static void
systemctl(const char *orden, const char *unidad)
{
	gchar *programa = g_find_program_in_path("systemctl");
	gchar *argv[6];
	int i = 0;

	/* Sin systemd no hay nada que sincronizar y tampoco nada de qué
	 * quejarse: la aplicación abierta sigue avisando igual. */
	if (!programa)
		return;

	argv[i++] = programa;
	argv[i++] = (gchar *)"--user";
	argv[i++] = (gchar *)orden;
	if (unidad)
		argv[i++] = (gchar *)unidad;
	argv[i] = NULL;

	g_spawn_sync(NULL, argv, NULL,
		     G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
		     NULL, NULL, NULL, NULL, NULL, NULL);
	g_free(programa);
}

/* Escribe solo si cambió: así encender el temporizador no arrastra un
 * daemon-reload cada vez que se toca la hora con el ratón. TRUE si el
 * fichero quedó distinto de como estaba. */
static gboolean
escribir_si_cambia(const char *ruta, const char *contenido)
{
	gchar *viejo = NULL;
	gboolean igual;

	if (g_file_get_contents(ruta, &viejo, NULL, NULL))
		igual = !g_strcmp0(viejo, contenido);
	else
		igual = FALSE;
	g_free(viejo);
	if (igual)
		return FALSE;

	return g_file_set_contents(ruta, contenido, -1, NULL);
}

void
main_recordatorio_systemd_sincronizar(void)
{
	gchar *dir = dir_unidades();
	gchar *f_serv = g_build_filename(dir, UNIDAD_BASE ".service", NULL);
	gchar *f_temp = g_build_filename(dir, UNIDAD_BASE ".timer", NULL);
	gchar *yo, *serv, *temp;
	gboolean activo;
	int hora = 7, minuto = 0;

	activo = main_planes_recordatorio(&hora, &minuto);

	if (!activo) {
		/* Se apaga antes de borrar: systemd necesita ver la
		 * unidad para poder deshacer el enlace. */
		if (g_file_test(f_temp, G_FILE_TEST_EXISTS)) {
			systemctl("disable", UNIDAD_BASE ".timer");
			systemctl("stop", UNIDAD_BASE ".timer");
			g_unlink(f_temp);
			g_unlink(f_serv);
			systemctl("daemon-reload", NULL);
		}
		goto fin;
	}

	yo = ruta_binario();
	if (!yo)
		goto fin;

	g_mkdir_with_parents(dir, 0700);

	serv = g_strdup_printf(
	    "# Lo escribe Biblia Elim: los cambios a mano se pierden al\n"
	    "# tocar la hora del recordatorio en la aplicación.\n"
	    "[Unit]\n"
	    "Description=Aviso de la lectura diaria de Biblia Elim\n"
	    "\n"
	    "[Service]\n"
	    "Type=simple\n"
	    "ExecStart=\"%s\" --recordar\n"
	    /* Si el aviso se queda esperando a que lo pulsen, que no se
	     * quede para siempre aunque cambie el código de arriba. */
	    "RuntimeMaxSec=%d\n",
	    yo, ESPERA_MAXIMA_SEGUNDOS + 60);

	temp = g_strdup_printf(
	    "# Lo escribe Biblia Elim: los cambios a mano se pierden al\n"
	    "# tocar la hora del recordatorio en la aplicación.\n"
	    "[Unit]\n"
	    "Description=Recordatorio diario de la lectura de Biblia Elim\n"
	    "\n"
	    "[Timer]\n"
	    "OnCalendar=*-*-* %02d:%02d:00\n"
	    /* Si el equipo estaba apagado a la hora, el aviso llega al
	     * encenderlo en vez de perderse el día. */
	    "Persistent=true\n"
	    "AccuracySec=1min\n"
	    "Unit=" UNIDAD_BASE ".service\n"
	    "\n"
	    "[Install]\n"
	    "WantedBy=timers.target\n",
	    hora, minuto);

	{
		/* Las dos escrituras, sin cortocircuito: la del
		 * temporizador tiene que ocurrir aunque el servicio ya
		 * estuviera al día. */
		gboolean cambio_serv = escribir_si_cambia(f_serv, serv);
		gboolean cambio_temp = escribir_si_cambia(f_temp, temp);

		if (cambio_serv || cambio_temp)
			systemctl("daemon-reload", NULL);
	}

	/* «enable --now» no tiene forma corta con esta ayuda de dos
	 * argumentos, así que van las dos órdenes; repetirlas cuando ya
	 * está encendido no cuesta nada y arregla el caso de que alguien
	 * lo hubiera parado por su cuenta. */
	systemctl("enable", UNIDAD_BASE ".timer");
	systemctl("start", UNIDAD_BASE ".timer");

	g_free(serv);
	g_free(temp);
	g_free(yo);

fin:
	g_free(f_serv);
	g_free(f_temp);
	g_free(dir);
}

#else /* WIN32 */

void
main_recordatorio_systemd_sincronizar(void)
{
}

#endif /* WIN32 */
