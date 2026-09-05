/*
 * Biblia Elim
 * pulpito.c - la ventana de púlpito
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "gui/pulpito.h"
#include "gui/dialog.h"
#include "gui/main_window.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#ifdef USE_WEBKIT_EDITOR
#include "editor/webkit_editor.h"
#else
#include "editor/slib-editor.h"
#endif

#include "main/pulpito.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/url.hh"
#include "main/xml.h"

#include "gui/debug_glib_null.h"

/* La columna de lectura: 42rem del enunciado, a 16px de raíz. Más ancho
 * que esto y el ojo pierde el renglón desde el atril. */
#define PU_ANCHO 672

/* Y su medida en caracteres, que es lo que ata a la etiqueta: sin ella
 * pide de natural el renglón entero de una pieza y se sale de la
 * pantalla. El ancho de verdad lo pone la columna; esta cuenta solo
 * tiene que quedarse por debajo, y por eso se mide con la letra del
 * tema y no con la del púlpito. */
#define PU_MEDIDA 52

/* Tamaños de partida, en píxeles. El zoom los multiplica. */
#define PU_PX_TITULO 32
#define PU_PX_VERSO 28
#define PU_PX_VINETA 20
#define PU_PX_CHROME 13

/* Interlineado: 1.55 veces el tamaño de la letra. */
#define PU_INTERLINEA 1.55

/* La segunda pantalla se ve desde el fondo del salón y no desde el
 * atril, así que su letra es bastante mayor que la de aquí. Y como
 * puede ser un proyector de cualquier tamaño, no se fija en píxeles:
 * se saca de lo que mida el monitor. El versículo ocupa un dieciseisavo
 * del alto, y la columna, cuatro quintos del ancho. */
#define PU_ALTO2_VERSO 16.0
#define PU_ANCHO2 0.8

enum {
	CAPA_NINGUNA = 0,
	CAPA_BOSQUEJO,
	CAPA_CAPITULO,
	CAPA_VERSO,
	CAPA_ATAJOS
};

typedef struct {
	GtkWidget *win;
	GtkWidget *lbl_cabecera;
	GtkWidget *lbl_ref;
	GtkWidget *lbl_reloj;
	GtkWidget *caja_escena;
	GtkWidget *lbl_siguiente;
	GtkWidget *capa;
	GtkWidget *capa_scroll;
	GtkWidget *lbl_capa_titulo;
	GtkWidget *lbl_capa;

	GtkWidget *lbl_tiempo;	/* lo que se lleva predicado */
	GtkWidget *lbl_negro;	/* el aviso de que la otra está apagada */

	/* La segunda pantalla, si la hay: lo que ve la congregación. */
	GtkWidget *win2;
	GtkWidget *tapa2;	/* el negro, encima de todo lo demás */
	GtkWidget *lbl2_punto;
	GtkWidget *lbl2_verso;
	GtkWidget *lbl2_cita;

	PU_SERMON *sermon;
	int paso;
	int zoom;
	int capa_tipo;
	int apoyo_ciclo;	/* el siguiente apoyo que enseña R */
	int apoyo_de;		/* de qué punto es esa cuenta */
	int segunda;		/* PU_SEGUNDA: qué se manda a la otra */
	int monitor;		/* en cuál está el atril, -1 si no se sabe */
	int px2_verso;		/* la letra de la otra, según su tamaño */
	int px2_punto;
	int px2_cita;
	gboolean negro;		/* la segunda, apagada de golpe */

	/* El tiempo de predicación. Arranca al primer paso y no al abrir
	 * la ventana: entre que se abre y se empieza puede pasar de todo
	 * -- colocar el atril, esperar al que canta -- y un reloj que ya
	 * va por catorce minutos cuando dices «buenos días» no sirve. */
	gint64 empezado;	/* g_get_monotonic_time(), 0 si no arrancó */
	int objetivo;		/* minutos previstos, 0 si no se quiere aviso */
	int aviso_tiempo;	/* 0 normal, 1 cerca, 2 pasado: para avisar una vez */
	guint reloj_id;
	guint aviso_id;		/* el pie, cuando dice algo y se retira */
	gulong mon_mas, mon_menos;	/* enchufar y desenchufar pantallas */
	guint rehacer_id;	/* la segunda, cuando cambian las pantallas */
} PULPITO;

static PULPITO *pu = NULL;

static void pintar(void);
static void pintar_pie(void);
static void pintar_estado_negro(void);
static void pintar_tiempo(void);
static void completa_en(GtkWidget *win, int monitor);
static gboolean preguntar(GtkWindow *padre, const char *texto,
			  const char *si, const char *no);

/* --------------------------------------------------------------------
 * Tipografía
 *
 * Los tamaños se ponen en píxeles absolutos con atributos de Pango y no
 * por CSS: el zoom los mueve en caliente, y así no hay que recargar una
 * hoja de estilos en cada pulsación.
 * ------------------------------------------------------------------ */

static void
fuente_color(GtkWidget *lbl, int px, gboolean negrita, double alpha,
	     const GdkRGBA *color)
{
	PangoAttrList *at = pango_attr_list_new();
	int escalado = (px * pu->zoom) / 100;

	if (escalado < 8)
		escalado = 8;
	pango_attr_list_insert(at,
			       pango_attr_size_new_absolute(escalado *
							    PANGO_SCALE));
	if (negrita)
		pango_attr_list_insert(at,
				       pango_attr_weight_new(PANGO_WEIGHT_BOLD));
	if (alpha < 1.0)
		pango_attr_list_insert(
		    at, pango_attr_foreground_alpha_new(
			    (guint16)(alpha * 65535)));
	/* El interlineado ancho es lo que hace legible un párrafo a tres
	 * metros. Va como atributo porque la etiqueta rehace su layout en
	 * cada medida y lo que se le ponga al layout se pierde. */
#if PANGO_VERSION_CHECK(1, 50, 0)
	pango_attr_list_insert(at,
			       pango_attr_line_height_new(PU_INTERLINEA));
#endif
	if (color)
		pango_attr_list_insert(
		    at, pango_attr_foreground_new(
			    (guint16)(color->red * 65535),
			    (guint16)(color->green * 65535),
			    (guint16)(color->blue * 65535)));
	gtk_label_set_attributes(GTK_LABEL(lbl), at);
	pango_attr_list_unref(at);
}

static void
fuente(GtkWidget *lbl, int px, gboolean negrita, double alpha)
{
	fuente_color(lbl, px, negrita, alpha, NULL);
}

/* Los avisos del atril -- la nota del predicador y el reloj cuando se
 * acaba el tiempo -- son lo único que lleva color propio en toda la
 * vista, y tienen que leerse igual sobre el púlpito oscuro que sobre el
 * claro, porque el púlpito se pinta con el tema que tenga puesto la
 * aplicación. Por eso hay dos juegos y no uno. */
typedef enum {
	AV_NOTA = 0,
	AV_CERCA,
	AV_PASADO
} PU_AVISO;

static const GdkRGBA *
color_aviso(PU_AVISO cual)
{
	static const GdkRGBA sobre_oscuro[] = {
	    {0.855, 0.690, 0.424, 1.0},	 /* nota   */
	    {0.878, 0.631, 0.227, 1.0},	 /* cerca  */
	    {0.878, 0.424, 0.353, 1.0}	 /* pasado */
	};
	static const GdkRGBA sobre_claro[] = {
	    {0.482, 0.361, 0.086, 1.0},
	    {0.627, 0.392, 0.000, 1.0},
	    {0.690, 0.165, 0.102, 1.0}
	};

	return settings.darktheme ? &sobre_oscuro[cual] : &sobre_claro[cual];
}

/* --------------------------------------------------------------------
 * Los pasos
 * ------------------------------------------------------------------ */

static PU_PASO *
paso_actual(void)
{
	return pu_paso(pu->sermon, pu->paso);
}

/* El versículo que se está enseñando, o el último que salió: es lo que
 * abre V cuando el paso de ahora no es un verso. */
static PU_PASO *
verso_visible(void)
{
	return pu_paso(pu->sermon, pu_verso_visible(pu->sermon, pu->paso));
}

/* --------------------------------------------------------------------
 * Las capas
 *
 * Ninguna cambia el paso: se abren encima, se cierran con Esc y el
 * sermón sigue donde estaba.
 * ------------------------------------------------------------------ */

static void
capa_cerrar(void)
{
	pu->capa_tipo = CAPA_NINGUNA;
	gtk_widget_hide(pu->capa);
}

static void
capa_abrir(int tipo, const char *titulo, const char *cuerpo)
{
	GtkAdjustment *ajuste;

	pu->capa_tipo = tipo;
	gtk_label_set_text(GTK_LABEL(pu->lbl_capa_titulo),
			   titulo ? titulo : "");
	gtk_label_set_text(GTK_LABEL(pu->lbl_capa), cuerpo ? cuerpo : "");
	fuente(pu->lbl_capa_titulo, PU_PX_CHROME + 3, TRUE, 0.75);
	fuente(pu->lbl_capa,
	       (tipo == CAPA_VERSO) ? PU_PX_VERSO : PU_PX_VINETA, FALSE, 1.0);
	gtk_widget_show_all(pu->capa);

	ajuste = gtk_scrolled_window_get_vadjustment(
	    GTK_SCROLLED_WINDOW(pu->capa_scroll));
	gtk_adjustment_set_value(ajuste, 0);
}

static void
capa_desplazar(int direccion)
{
	GtkAdjustment *a = gtk_scrolled_window_get_vadjustment(
	    GTK_SCROLLED_WINDOW(pu->capa_scroll));
	double paso = gtk_adjustment_get_page_increment(a) / 2;
	double v = gtk_adjustment_get_value(a) + direccion * paso;
	double tope = gtk_adjustment_get_upper(a) -
		      gtk_adjustment_get_page_size(a);

	gtk_adjustment_set_value(a, CLAMP(v, gtk_adjustment_get_lower(a),
					  MAX(tope, 0)));
}

/* El mapa del bosquejo, con el punto en el que se está marcado: es lo
 * que se viene a mirar cuando uno se pierde. */
static void
capa_bosquejo(void)
{
	GString *g = g_string_new(NULL);
	PU_PASO *actual = paso_actual();
	int mio = actual ? actual->titulo_de : -1;
	int i;

	for (i = 0; i < pu_total(pu->sermon); ++i) {
		PU_PASO *s = pu_paso(pu->sermon, i);
		int n;

		if (s->tipo != PU_TITULO)
			continue;
		g_string_append(g, (i == mio) ? "  ▸  " : "      ");
		for (n = 1; n < s->nivel; ++n)
			g_string_append(g, "    ");
		g_string_append(g, s->titulo ? s->titulo : "");
		g_string_append_c(g, '\n');
	}
	capa_abrir(CAPA_BOSQUEJO, _("Bosquejo"), g->str);
	g_string_free(g, TRUE);
}

/* El capítulo del texto base, solo lectura. */
static void
capa_capitulo(void)
{
	PU_PASO *v = verso_visible();
	const char *ref = (v && v->ref) ? v->ref : pu->sermon->ref_base;
	gchar *capitulo, *texto;

	if (!ref || !*ref) {
		capa_abrir(CAPA_CAPITULO, _("Capítulo"),
			   _("Este sermón no tiene un texto base."));
		return;
	}

	capitulo = main_pulpito_capitulo(pu->sermon->version, ref);
	if (!capitulo) {
		capa_abrir(CAPA_CAPITULO, ref, _("Texto no disponible."));
		return;
	}
	texto = main_pulpito_texto(pu->sermon->version, capitulo);
	capa_abrir(CAPA_CAPITULO, capitulo,
		   texto ? texto : _("Texto no disponible."));
	g_free(texto);
	g_free(capitulo);
}

static void
capa_verso(void)
{
	PU_PASO *v = verso_visible();

	if (!v) {
		capa_abrir(CAPA_VERSO, _("Versículo"),
			   _("Todavía no ha salido ningún versículo."));
		return;
	}
	capa_abrir(CAPA_VERSO, v->ref ? v->ref : "",
		   v->texto ? v->texto : _("Texto no disponible."));
}

static void
capa_atajos(void)
{
	capa_abrir(CAPA_ATAJOS, _("Atajos"),
		   _("Espacio o ↓     el paso siguiente\n"
		     "↑               el paso anterior\n"
		     "N / P           punto siguiente / anterior\n"
		     "Inicio / Fin    primer / último paso\n"
		     "\n"
		     "V               el versículo, a pantalla\n"
		     "B               el bosquejo entero\n"
		     "C               el capítulo del texto base\n"
		     "R               siguiente versículo de apoyo\n"
		     "I               enseñar u ocultar la ilustración\n"
		     "2               qué va a la segunda pantalla\n"
		     ".               apagar y encender esa pantalla\n"
		     "T               el tiempo, desde cero\n"
		     "Esc             cerrar lo que esté abierto\n"
		     "\n"
		     "+ / −           tamaño de la letra\n"
		     "Q               salir al sermón\n"
		     "E               salir al editor del sermón\n"
		     "L               salir al modo lectura en el texto\n"
		     "?               esta lista"));
}

/* --------------------------------------------------------------------
 * Pintar el paso
 * ------------------------------------------------------------------ */

static void
limpiar_escena(void)
{
	GList *hijos = gtk_container_get_children(GTK_CONTAINER(pu->caja_escena));
	GList *l;

	for (l = hijos; l; l = l->next)
		gtk_widget_destroy(GTK_WIDGET(l->data));
	g_list_free(hijos);
}

static GtkWidget *
renglon(const char *texto, int px, gboolean negrita, double alpha)
{
	GtkWidget *lbl = gtk_label_new(texto ? texto : "");

	gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(lbl), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_max_width_chars(GTK_LABEL(lbl), PU_MEDIDA);
	gtk_label_set_xalign(GTK_LABEL(lbl), 0.0);
	gtk_label_set_selectable(GTK_LABEL(lbl), FALSE);
	gtk_box_pack_start(GTK_BOX(pu->caja_escena), lbl, FALSE, FALSE, 0);
	gtk_widget_show(lbl);
	fuente(lbl, px, negrita, alpha);
	return lbl;
}

/* Lo que el bosquejo apuntó para uno mismo.
 *
 * Va en ámbar apagado y pequeño, debajo y separado, porque no es parte
 * del sermón: es el recado que uno se dejó escrito. Y no aparece en
 * segunda_pintar(), que es lo que hace que esto sea una vista de
 * presentador y no un espejo -- la congregación no lee los recados del
 * que predica. */
static void
pintar_nota(PU_PASO *s)
{
	gchar **lineas;
	gchar **l;
	gboolean primera = TRUE;

	if (!s || !s->nota || !*s->nota)
		return;

	lineas = g_strsplit(s->nota, "\n", -1);
	for (l = lineas; *l; ++l) {
		gchar *texto;
		GtkWidget *lbl;

		if (!**l)
			continue;
		/* Asterisco de texto (U+2217) y no el de emoji: los de la
		 * zona emoji los pinta la fuente de color con su propio
		 * tono y la nota saldría verde. */
		texto = g_strdup_printf("∗  %s", *l);
		lbl = renglon(texto, PU_PX_VINETA - 3, FALSE, 1.0);
		fuente_color(lbl, PU_PX_VINETA - 3, FALSE, 0.8,
			     color_aviso(AV_NOTA));
		gtk_widget_set_margin_top(lbl, primera ? 30 : 6);
		primera = FALSE;
		g_free(texto);
	}
	g_strfreev(lineas);
}

static void
pintar_escena(void)
{
	PU_PASO *s = paso_actual();
	guint i;

	limpiar_escena();
	if (!s)
		return;

	switch (s->tipo) {
	case PU_TITULO:
		renglon(s->titulo, PU_PX_TITULO, TRUE, 1.0);
		break;

	case PU_VERSO:
	case PU_APOYO:
		renglon(s->texto ? s->texto : _("Texto no disponible"),
			PU_PX_VERSO, FALSE, s->texto ? 1.0 : 0.55);
		/* La cita, pequeña y debajo: el que predica ya sabe dónde
		 * está; el que escucha quiere poder anotarla. */
		{
			gchar *cita = g_strdup_printf(
			    "%s  ·  %s", s->ref ? s->ref : "",
			    pu->sermon->version ? pu->sermon->version : "");
			GtkWidget *l = renglon(cita, PU_PX_CHROME + 2, FALSE,
					       0.55);

			gtk_label_set_xalign(GTK_LABEL(l), 1.0);
			gtk_widget_set_margin_top(l, 14);
			g_free(cita);
		}
		break;

	case PU_VINETAS:
		if (s->vinetas)
			for (i = 0; i < s->vinetas->len; ++i) {
				gchar *v = g_strdup_printf(
				    "·  %s",
				    (const char *)g_ptr_array_index(s->vinetas,
								    i));
				GtkWidget *l = renglon(v, PU_PX_VINETA, FALSE,
						       1.0);

				gtk_widget_set_margin_bottom(l, 10);
				g_free(v);
			}
		break;

	case PU_ILUSTRACION:
		/* Secundaria a propósito: es un apoyo, no el punto. */
		renglon(s->ilustracion, PU_PX_VINETA + 2, FALSE, 0.7);
		break;
	}

	pintar_nota(s);
}

static void
pintar_pie(void)
{
	PU_PASO *sig = pu_paso(pu->sermon, pu->paso + 1);
	gchar *resumen, *texto;

	if (!sig) {
		gtk_label_set_text(GTK_LABEL(pu->lbl_siguiente),
				   _("último paso"));
		fuente(pu->lbl_siguiente, PU_PX_CHROME, FALSE, 0.45);
		return;
	}

	/* Solo el enunciado, no el contenido: adelantarlo entero quita la
	 * razón de que haya pasos. */
	resumen = pu_resumen(sig);
	if (resumen && g_utf8_strlen(resumen, -1) > 60) {
		gchar *corte = g_utf8_substring(resumen, 0, 57);
		gchar *puntos = g_strconcat(corte, "…", NULL);

		g_free(resumen);
		g_free(corte);
		resumen = puntos;
	}
	texto = g_strdup_printf(_("siguiente: %s"), resumen ? resumen : "");
	gtk_label_set_text(GTK_LABEL(pu->lbl_siguiente), texto);
	fuente(pu->lbl_siguiente, PU_PX_CHROME, FALSE, 0.45);
	g_free(texto);
	g_free(resumen);
}


/* --------------------------------------------------------------------
 * La segunda pantalla
 *
 * Es la que ve la congregación, y por eso no es un espejo del atril:
 * aquí no salen ni el pie de camino, ni el reloj, ni los apoyos, ni las
 * capas. Solo el versículo y el enunciado del punto, según lo que se
 * haya elegido. Si no hay un segundo monitor, nada de esto ocurre.
 * ------------------------------------------------------------------ */

/* En qué monitor está una ventana, por número; -1 si todavía no se sabe
 * (una ventana que no se ha mapeado no está en ninguno). */
static int
monitor_de(GtkWidget *w)
{
	GdkDisplay *dpy = gtk_widget_get_display(w);
	GdkWindow *gw = gtk_widget_get_window(w);
	GdkMonitor *m = NULL;
	int n, i;

	if (gw)
		m = gdk_display_get_monitor_at_window(dpy, gw);
	if (!m)
		return -1;
	n = gdk_display_get_n_monitors(dpy);
	for (i = 0; i < n; ++i)
		if (gdk_display_get_monitor(dpy, i) == m)
			return i;
	return -1;
}

/* El monitor donde va el atril: el mismo en el que está la aplicación,
 * que es la pantalla que el que predica tiene delante. */
static int
monitor_del_atril(void)
{
	GdkDisplay *dpy = gdk_display_get_default();
	GdkMonitor *primero;
	int atril = -1, n, i;

	if (widgets.app)
		atril = monitor_de(widgets.app);
	if (atril >= 0)
		return atril;
	/* Sin ventana principal a la que mirar, el monitor principal. */
	primero = dpy ? gdk_display_get_primary_monitor(dpy) : NULL;
	n = dpy ? gdk_display_get_n_monitors(dpy) : 0;
	for (i = 0; i < n; ++i)
		if (gdk_display_get_monitor(dpy, i) == primero)
			return i;
	return (n > 0) ? 0 : -1;
}

/* El monitor que no es el del atril, o -1 si solo hay uno. */
static int
segunda_monitor(GdkDisplay **dpy_out)
{
	GdkDisplay *dpy = pu ? gtk_widget_get_display(pu->win)
			     : gdk_display_get_default();
	int atril = pu ? pu->monitor : monitor_del_atril();
	int n, i;

	if (dpy_out)
		*dpy_out = dpy;
	n = gdk_display_get_n_monitors(dpy);
	if (n < 2)
		return -1;
	for (i = 0; i < n; ++i)
		if (i != atril)
			return i;
	return -1;
}

static void
segunda_cerrar(void)
{
	if (!pu->win2)
		return;
	gtk_widget_destroy(pu->win2);
	pu->win2 = NULL;
	pu->tapa2 = NULL;
	pu->lbl2_punto = pu->lbl2_verso = pu->lbl2_cita = NULL;
}

static GtkWidget *
renglon2(GtkWidget *caja, gdouble xalign)
{
	GtkWidget *lbl = gtk_label_new("");

	gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
	gtk_label_set_line_wrap_mode(GTK_LABEL(lbl), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_xalign(GTK_LABEL(lbl), xalign);
	gtk_label_set_justify(GTK_LABEL(lbl), GTK_JUSTIFY_LEFT);
	/* La visibilidad la lleva segunda_pintar(), no el show_all. */
	gtk_widget_set_no_show_all(lbl, TRUE);
	gtk_box_pack_start(GTK_BOX(caja), lbl, FALSE, FALSE, 0);
	return lbl;
}

static void
segunda_abrir(void)
{
	GdkDisplay *dpy = NULL;
	int idx = segunda_monitor(&dpy);
	GtkWidget *fila, *col, *envoltura;
	GdkRectangle geo;
	int ancho2;

	/* Con la pantalla en negro se abre igual aunque no haya nada que
	 * enseñar: taparla es justamente para lo que sirve. Sin ventana,
	 * la congregación se quedaría mirando el escritorio. */
	if (pu->win2 || (pu->segunda == PU2_NADA && !pu->negro) || idx < 0)
		return;

	/* Las medidas salen de lo que mida esa pantalla, que lo mismo es un
	 * proyector de 1024 que un televisor. Y el ancho de la columna hay
	 * que decirlo a mano: GTK reparte el de una etiqueta que envuelve
	 * con la letra del tema, no con la que se le pone después. */
	gdk_monitor_get_geometry(gdk_display_get_monitor(dpy, idx), &geo);
	pu->px2_verso = CLAMP((int)(geo.height / PU_ALTO2_VERSO), 20, 96);
	pu->px2_punto = (pu->px2_verso * 3) / 4;
	pu->px2_cita = MAX(pu->px2_verso / 2, 14);
	ancho2 = (int)(geo.width * PU_ANCHO2);

	pu->win2 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(pu->win2), _("Púlpito · pantalla"));
	gtk_window_set_decorated(GTK_WINDOW(pu->win2), FALSE);
	/* Ni roba el teclado ni se pone delante de nada: el que predica
	 * sigue escribiendo en la suya. */
	gtk_window_set_accept_focus(GTK_WINDOW(pu->win2), FALSE);
	gtk_window_set_focus_on_map(GTK_WINDOW(pu->win2), FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(pu->win2),
				    "pulpito");

	/* Todo va dentro de un overlay para poder echarle la tapa negra
	 * encima. El fondo de una ventana se fija al realizarla, así que
	 * ponerle una clase después no la repinta: por eso el negro es un
	 * widget que se enseña y se esconde, y no un color que se cambia. */
	envoltura = gtk_overlay_new();
	gtk_container_add(GTK_CONTAINER(pu->win2), envoltura);

	fila = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add(GTK_CONTAINER(envoltura), fila);
	col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	/* La columna se estira a lo ancho de la pantalla y se estrecha con
	 * los márgenes. Centrarla en su ancho natural no vale: una etiqueta
	 * que envuelve pide de natural el renglón entero de una pieza, y se
	 * saldría por los dos lados. */
	gtk_widget_set_halign(col, GTK_ALIGN_FILL);
	gtk_widget_set_valign(col, GTK_ALIGN_CENTER);
	gtk_widget_set_margin_start(col, (geo.width - ancho2) / 2);
	gtk_widget_set_margin_end(col, (geo.width - ancho2) / 2);
	gtk_box_pack_start(GTK_BOX(fila), col, TRUE, TRUE, 0);

	pu->lbl2_punto = renglon2(col, 0.0);
	pu->lbl2_verso = renglon2(col, 0.0);
	pu->lbl2_cita = renglon2(col, 1.0);
	gtk_widget_set_margin_bottom(pu->lbl2_punto, 28);
	gtk_widget_set_margin_top(pu->lbl2_cita, 24);

	pu->tapa2 = gtk_event_box_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(pu->tapa2),
				    "pulpito-negro");
	gtk_widget_set_halign(pu->tapa2, GTK_ALIGN_FILL);
	gtk_widget_set_valign(pu->tapa2, GTK_ALIGN_FILL);
	/* La enseña segunda_pintar(), no el show_all. */
	gtk_widget_set_no_show_all(pu->tapa2, TRUE);
	gtk_overlay_add_overlay(GTK_OVERLAY(envoltura), pu->tapa2);

	/* El monitor se pide antes de enseñarla: después, el compositor ya
	 * la ha colocado y la petición llega tarde. */
	completa_en(pu->win2, idx);
	gtk_widget_show_all(pu->win2);
	/* Y el teclado se queda donde estaba. */
	gtk_window_present(GTK_WINDOW(pu->win));
}

static void
segunda_pintar(void)
{
	gboolean quiere_verso, quiere_punto;
	PU_PASO *actual, *punto, *v;

	if (!pu->win2)
		return;

	/* En negro se echa la tapa y además se esconde lo de debajo: si
	 * por lo que sea la tapa no llegara a pintarse, lo que no puede
	 * pasar es que el versículo siga ahí. */
	if (pu->negro) {
		if (pu->tapa2)
			gtk_widget_show(pu->tapa2);
		gtk_widget_hide(pu->lbl2_punto);
		gtk_widget_hide(pu->lbl2_verso);
		gtk_widget_hide(pu->lbl2_cita);
		return;
	}
	if (pu->tapa2)
		gtk_widget_hide(pu->tapa2);

	quiere_verso = (pu->segunda == PU2_VERSO || pu->segunda == PU2_AMBOS);
	quiere_punto = (pu->segunda == PU2_PUNTO || pu->segunda == PU2_AMBOS);

	actual = paso_actual();
	punto = actual ? pu_paso(pu->sermon, actual->titulo_de) : NULL;
	v = quiere_verso ? verso_visible() : NULL;

	if (quiere_punto && punto && punto->titulo && *punto->titulo) {
		gtk_label_set_text(GTK_LABEL(pu->lbl2_punto), punto->titulo);
		/* Con el versículo delante, el punto es el encabezado; sin
		 * él, es lo único que hay y se pone grande. */
		fuente(pu->lbl2_punto,
		       v ? pu->px2_punto : pu->px2_verso, TRUE,
		       v ? 0.7 : 1.0);
		gtk_widget_show(pu->lbl2_punto);
	} else
		gtk_widget_hide(pu->lbl2_punto);

	if (v) {
		gchar *cita;

		gtk_label_set_text(GTK_LABEL(pu->lbl2_verso),
				   v->texto ? v->texto
					    : _("Texto no disponible"));
		fuente(pu->lbl2_verso, pu->px2_verso, FALSE, 1.0);
		gtk_widget_show(pu->lbl2_verso);

		cita = g_strdup_printf("%s  ·  %s", v->ref ? v->ref : "",
				       pu->sermon->version ? pu->sermon->version
							  : "");
		gtk_label_set_text(GTK_LABEL(pu->lbl2_cita), cita);
		fuente(pu->lbl2_cita, pu->px2_cita, FALSE, 0.55);
		gtk_widget_show(pu->lbl2_cita);
		g_free(cita);
	} else {
		gtk_widget_hide(pu->lbl2_verso);
		gtk_widget_hide(pu->lbl2_cita);
	}
}

/* Que la pantalla está apagada tiene que verse todo el rato y no dos
 * segundos: lo que no puede pasar es seguir predicando media hora
 * creyendo que la congregación ve los versículos. */
static void
pintar_estado_negro(void)
{
	if (!pu || !pu->lbl_negro)
		return;
	if (!pu->negro) {
		gtk_widget_hide(pu->lbl_negro);
		return;
	}
	gtk_label_set_text(GTK_LABEL(pu->lbl_negro), _("● pantalla en negro"));
	fuente_color(pu->lbl_negro, PU_PX_CHROME, FALSE, 0.9,
		     color_aviso(AV_CERCA));
	gtk_widget_show(pu->lbl_negro);
}

/* El pie dice algo un momento y vuelve a lo suyo. */
static gboolean
quitar_aviso(gpointer datos)
{
	(void)datos;
	if (pu) {
		pu->aviso_id = 0;
		pintar_pie();
	}
	return G_SOURCE_REMOVE;
}

static void
avisar(const char *texto)
{
	if (!pu)
		return;
	if (pu->aviso_id)
		g_source_remove(pu->aviso_id);
	gtk_label_set_text(GTK_LABEL(pu->lbl_siguiente), texto);
	fuente(pu->lbl_siguiente, PU_PX_CHROME, FALSE, 0.7);
	pu->aviso_id = g_timeout_add(2500, quitar_aviso, NULL);
}

/* Enchufar o desenchufar una pantalla a mitad de sermón. Lo que no
 * puede pasar es que la ventana de la congregación se quede huérfana
 * encima del atril cuando se va el proyector, así que se rehace: se
 * cierra, se mira dónde ha quedado el atril y se vuelve a abrir si
 * todavía hay dónde. */
static gboolean
rehacer_segunda(gpointer datos)
{
	(void)datos;
	if (!pu)
		return G_SOURCE_REMOVE;
	pu->rehacer_id = 0;
	pu->monitor = monitor_de(pu->win);
	if (pu->monitor < 0)
		pu->monitor = monitor_del_atril();
	segunda_abrir();
	segunda_pintar();
	return G_SOURCE_REMOVE;
}

static void
on_pantallas(GdkDisplay *dpy, GdkMonitor *monitor, gpointer datos)
{
	(void)dpy;
	(void)monitor;
	(void)datos;
	if (!pu)
		return;
	/* La de antes se cierra ya, no vaya a quedarse encima del atril.
	 * La nueva espera un momento: recién enchufada, la pantalla aún no
	 * sabe de qué tamaño es, y una ventana medida con eso sale con la
	 * letra y la columna de cualquier manera. */
	segunda_cerrar();
	if (pu->rehacer_id)
		g_source_remove(pu->rehacer_id);
	pu->rehacer_id = g_timeout_add(700, rehacer_segunda, NULL);
}

/* El punto apaga la pantalla de la congregación y la vuelve a encender
 * donde estaba. Es la tecla que se usa cuando alguien pasa por delante
 * del proyector, cuando hay que leer algo de un papel o cuando lo que
 * toca es que miren al púlpito y no a la pared. No mueve el paso ni
 * cambia lo elegido: solo tapa. */
static void
alternar_negro(void)
{
	if (segunda_monitor(NULL) < 0) {
		avisar(_("no hay segunda pantalla"));
		return;
	}

	pu->negro = !pu->negro;

	if (pu->negro)
		segunda_abrir();	/* aunque no hubiera nada que enseñar */
	segunda_pintar();
	/* Al encender, si no había nada elegido la ventana sobra: se
	 * cierra y la pantalla vuelve a ser de quien fuera. */
	if (!pu->negro && pu->segunda == PU2_NADA)
		segunda_cerrar();

	pintar_estado_negro();
	avisar(pu->negro ? _("pantalla en negro")
			 : _("pantalla encendida"));
}

/* La tecla 2 va pasando por lo que se puede mandar a la otra pantalla. */
static void
ciclar_segunda(void)
{
	static const char *nombres[] = {
	    N_("nada"), N_("el versículo"), N_("el punto"),
	    N_("el versículo y el punto")
	};
	gchar *aviso;

	if (segunda_monitor(NULL) < 0) {
		avisar(_("no hay segunda pantalla"));
		return;
	}
	pu->segunda = (pu->segunda + 1) % 4;
	main_pulpito_segunda_poner((PU_SEGUNDA)pu->segunda);
	if (pu->segunda == PU2_NADA)
		segunda_cerrar();
	else
		segunda_abrir();
	segunda_pintar();

	/* Cambiar lo que se manda con la pantalla tapada no se ve, y
	 * callárselo sería dejar a alguien esperando a que aparezca. */
	aviso = g_strdup_printf(
	    pu->negro ? _("segunda pantalla: %s · sigue en negro")
		      : _("segunda pantalla: %s"),
	    _(nombres[pu->segunda]));
	avisar(aviso);
	g_free(aviso);
}

static void
pintar(void)
{
	gchar *ref;

	if (!pu)
		return;

	pintar_escena();
	pintar_pie();

	ref = g_strdup_printf(
	    "%s%s%s", pu->sermon->ref_base ? pu->sermon->ref_base : "",
	    (pu->sermon->ref_base && pu->sermon->version) ? "  ·  " : "",
	    pu->sermon->version ? pu->sermon->version : "");
	gtk_label_set_text(GTK_LABEL(pu->lbl_ref), ref);
	fuente(pu->lbl_ref, PU_PX_CHROME, FALSE, 0.5);
	g_free(ref);

	gtk_label_set_text(GTK_LABEL(pu->lbl_cabecera), pu->sermon->titulo);
	fuente(pu->lbl_cabecera, PU_PX_CHROME + 1, TRUE, 0.65);

	/* El zoom también mueve la letra de la cabecera. */
	pintar_tiempo();
	pintar_estado_negro();

	segunda_pintar();
}

/* --------------------------------------------------------------------
 * El tiempo
 *
 * Arriba a la derecha van dos cosas distintas y conviene no confundirlas:
 * la hora que es, pequeña y apagada, y lo que se lleva predicado, que es
 * el número que de verdad se mira. La cuenta empieza en el primer paso
 * -- no al abrir la ventana -- y avisa por color si hay una duración
 * prevista: ámbar en el último quinto, rojo al pasarse. Sin duración
 * prevista no cambia de color nunca; el reloj no está para meter prisa
 * a quien no la ha pedido.
 * ------------------------------------------------------------------ */

static gchar *
tiempo_texto(gint64 segundos)
{
	if (segundos >= 3600)
		return g_strdup_printf("%d:%02d:%02d",
				       (int)(segundos / 3600),
				       (int)((segundos / 60) % 60),
				       (int)(segundos % 60));
	return g_strdup_printf("%d:%02d", (int)(segundos / 60),
			       (int)(segundos % 60));
}

static void
pintar_tiempo(void)
{
	gint64 segundos = 0;
	gchar *texto;
	int estado = 0;

	if (!pu || !pu->lbl_tiempo)
		return;

	if (pu->empezado)
		segundos = (g_get_monotonic_time() - pu->empezado) / 1000000;

	if (pu->objetivo > 0 && pu->empezado) {
		gint64 previsto = (gint64)pu->objetivo * 60;

		if (segundos >= previsto)
			estado = 2;
		else if (segundos >= (previsto * 4) / 5)
			estado = 1;
	}

	texto = tiempo_texto(segundos);
	gtk_label_set_text(GTK_LABEL(pu->lbl_tiempo), texto);
	g_free(texto);

	/* Antes de empezar se enseña 0:00 casi transparente: está, para
	 * que se sepa que hay cuenta, y no llama la atención. */
	fuente_color(pu->lbl_tiempo, PU_PX_CHROME + 3, estado != 0,
		     pu->empezado ? 0.85 : 0.25,
		     (estado == 2)   ? color_aviso(AV_PASADO)
		     : (estado == 1) ? color_aviso(AV_CERCA)
				     : NULL);

	/* Al cruzar cada raya se dice una vez, en el pie, y no se vuelve a
	 * insistir: el color ya se queda ahí. Un aviso que se repite a
	 * media predicación es peor que no tenerlo. */
	if (estado > pu->aviso_tiempo) {
		if (estado == 1)
			avisar(_("queda el último quinto del tiempo"));
		else
			avisar(_("se cumplió el tiempo previsto"));
	}
	pu->aviso_tiempo = estado;
}

static gboolean
tic_reloj(gpointer datos)
{
	GDateTime *ahora;
	gchar *hhmm;

	(void)datos;
	if (!pu)
		return G_SOURCE_REMOVE;

	ahora = g_date_time_new_now_local();
	hhmm = g_date_time_format(ahora, "%H:%M");
	g_date_time_unref(ahora);
	if (g_strcmp0(hhmm, gtk_label_get_text(GTK_LABEL(pu->lbl_reloj)))) {
		gtk_label_set_text(GTK_LABEL(pu->lbl_reloj), hhmm);
		fuente(pu->lbl_reloj, PU_PX_CHROME, FALSE, 0.45);
	}
	g_free(hhmm);

	pintar_tiempo();
	return G_SOURCE_CONTINUE;
}

/* T vuelve a poner la cuenta a cero, que es lo que hace falta cuando la
 * ventana llevaba abierta desde antes o cuando se predica dos veces
 * seguidas sin cerrarla. */
static void
reiniciar_tiempo(void)
{
	pu->empezado = g_get_monotonic_time();
	pu->aviso_tiempo = 0;
	pintar_tiempo();
	avisar(_("tiempo: desde cero"));
}

/* --------------------------------------------------------------------
 * Moverse
 * ------------------------------------------------------------------ */

static void
ir_a(int i)
{
	PU_PASO *destino;

	if (!pu_total(pu->sermon))
		return;

	/* El primer movimiento es el que pone en marcha el reloj. */
	if (!pu->empezado && CLAMP(i, 0, pu_ultimo(pu->sermon)) != pu->paso)
		pu->empezado = g_get_monotonic_time();

	pu->paso = CLAMP(i, 0, pu_ultimo(pu->sermon));

	/* La cuenta de los apoyos es de un punto: al cambiar de punto
	 * vuelve a empezar. */
	destino = paso_actual();
	if (destino && destino->titulo_de != pu->apoyo_de) {
		pu->apoyo_de = destino->titulo_de;
		pu->apoyo_ciclo = 0;
	}
	pintar();
}

/* R enseña los versículos de apoyo del punto, uno tras otro. */
static void
ciclar_apoyo(void)
{
	GArray *apoyos = pu_apoyos_de(pu->sermon, pu->paso);
	int cual, destino;

	if (!apoyos->len) {
		/* Sin apoyos, no pasa nada: la tecla no hace ruido. */
		g_array_free(apoyos, TRUE);
		return;
	}
	cual = pu->apoyo_ciclo % (int)apoyos->len;
	destino = g_array_index(apoyos, int, cual);
	ir_a(destino);
	/* ir_a() no ha tocado la cuenta: el apoyo es del mismo punto. */
	pu->apoyo_ciclo = (cual + 1) % (int)apoyos->len;
	g_array_free(apoyos, TRUE);
}

/* I enseña la ilustración del punto, y devuelve al punto si ya está. */
static void
alternar_ilustracion(void)
{
	PU_PASO *actual = paso_actual();
	int donde = pu_ilustracion_de(pu->sermon, pu->paso);

	if (donde < 0)
		return;
	if (actual && actual->tipo == PU_ILUSTRACION)
		ir_a(actual->titulo_de);
	else
		ir_a(donde);
}

/* --------------------------------------------------------------------
 * Salir
 * ------------------------------------------------------------------ */

static void
guardar_estado(void)
{
	if (!pu || !pu->sermon)
		return;
	main_pulpito_guardar_paso(pu->sermon->modulo, pu->paso);
	main_pulpito_zoom_poner(pu->zoom);
	if (settings.fnconfigure)
		xml_save_settings_doc(settings.fnconfigure);
}

/* Al cerrar se ofrece anotarlo, una vez, sin insistir. */
static void
preguntar_predicado(void)
{
	gchar *pregunta;
	gboolean si;

	if (main_pulpito_predicado(pu->sermon->modulo))
		return;

	/* Con el tiempo delante, porque es lo primero que uno quiere saber
	 * al bajar del atril y lo que le sirve para el domingo que viene. */
	if (pu->empezado) {
		gint64 segundos =
		    (g_get_monotonic_time() - pu->empezado) / 1000000;
		gchar *cuanto = tiempo_texto(segundos);

		pregunta = g_strdup_printf(
		    _("Has predicado %s.\n\n¿Marcar «%s» como predicado?"),
		    cuanto, pu->sermon->titulo);
		g_free(cuanto);
	} else
		pregunta = g_strdup_printf(_("¿Marcar «%s» como predicado?"),
					   pu->sermon->titulo);
	si = preguntar(GTK_WINDOW(pu->win), pregunta, _("Sí"), _("Después"));
	g_free(pregunta);

	if (si) {
		main_pulpito_marcar_predicado(pu->sermon->modulo, TRUE);
		if (settings.fnconfigure)
			xml_save_settings_doc(settings.fnconfigure);
	}
}

static void
salir(int a_donde)
{
	gchar *modulo, *ref = NULL;
	PU_PASO *v;

	if (!pu)
		return;

	v = verso_visible();
	if (v && v->ref)
		ref = g_strdup(v->ref);
	else if (pu->sermon->ref_base)
		ref = g_strdup(pu->sermon->ref_base);
	modulo = g_strdup(pu->sermon->modulo);

	guardar_estado();
	preguntar_predicado();

	gtk_widget_destroy(pu->win);	/* on_destroy limpia pu */

	switch (a_donde) {
	case 'E':
		editor_create_new(modulo, NULL, BOOK_EDITOR);
		break;
	case 'L':
		if (ref) {
			gchar *url = g_strdup_printf("sword:///%s", ref);

			main_url_handler(url, TRUE);
			g_free(url);
		}
		/* El modo lectura es otra vista y sigue siendo suya: aquí
		 * solo se enciende, sin tocar nada de cómo funciona. */
		gui_toggle_reading_mode(TRUE);
		break;
	default:
		break;
	}

	if (widgets.app)
		gtk_window_present(GTK_WINDOW(widgets.app));
	g_free(modulo);
	g_free(ref);
}

/* --------------------------------------------------------------------
 * Teclado
 *
 * El manejador está en esta ventana y no en la aplicación: mientras el
 * púlpito no esté montado, ninguna de estas teclas existe, así que no
 * pueden chocar con las del modo lectura.
 * ------------------------------------------------------------------ */

static gboolean
on_tecla(GtkWidget *widget, GdkEventKey *ev, gpointer datos)
{
	guint k = ev->keyval;

	(void)widget;
	(void)datos;

	/* Con una capa abierta, las flechas la recorren y casi todo lo
	 * demás la cierra. */
	if (pu->capa_tipo != CAPA_NINGUNA) {
		switch (k) {
		case GDK_KEY_Down:
		case GDK_KEY_Page_Down:
			capa_desplazar(1);
			return TRUE;
		case GDK_KEY_Up:
		case GDK_KEY_Page_Up:
			capa_desplazar(-1);
			return TRUE;
		default:
			capa_cerrar();
			return TRUE;
		}
	}

	switch (k) {
	case GDK_KEY_space:
	case GDK_KEY_Down:
	case GDK_KEY_Page_Down:
		ir_a(pu_siguiente(pu->sermon, pu->paso));
		return TRUE;
	case GDK_KEY_Up:
	case GDK_KEY_Page_Up:
		ir_a(pu_anterior(pu->sermon, pu->paso));
		return TRUE;
	case GDK_KEY_Home:
		ir_a(pu_primero(pu->sermon));
		return TRUE;
	case GDK_KEY_End:
		ir_a(pu_ultimo(pu->sermon));
		return TRUE;
	case GDK_KEY_n:
	case GDK_KEY_N:
		ir_a(pu_titulo_cercano(pu->sermon, pu->paso, 1));
		return TRUE;
	case GDK_KEY_p:
	case GDK_KEY_P:
		ir_a(pu_titulo_cercano(pu->sermon, pu->paso, -1));
		return TRUE;
	case GDK_KEY_b:
	case GDK_KEY_B:
		capa_bosquejo();
		return TRUE;
	case GDK_KEY_c:
	case GDK_KEY_C:
		capa_capitulo();
		return TRUE;
	case GDK_KEY_v:
	case GDK_KEY_V:
		capa_verso();
		return TRUE;
	case GDK_KEY_question:
		capa_atajos();
		return TRUE;
	case GDK_KEY_r:
	case GDK_KEY_R:
		ciclar_apoyo();
		return TRUE;
	case GDK_KEY_i:
	case GDK_KEY_I:
		alternar_ilustracion();
		return TRUE;
	case GDK_KEY_2:
	case GDK_KEY_KP_2:
		ciclar_segunda();
		return TRUE;
	/* El punto apaga y enciende la pantalla de la congregación. Es la
	 * tecla de todos los presentadores; aquí no puede ser la B, que ya
	 * abre el bosquejo. */
	case GDK_KEY_period:
	case GDK_KEY_KP_Decimal:
	case GDK_KEY_0:
	case GDK_KEY_KP_0:
		alternar_negro();
		return TRUE;
	case GDK_KEY_t:
	case GDK_KEY_T:
		reiniciar_tiempo();
		return TRUE;
	case GDK_KEY_plus:
	case GDK_KEY_equal:
	case GDK_KEY_KP_Add:
		pu->zoom = CLAMP(pu->zoom + 10, 50, 300);
		pintar();
		return TRUE;
	case GDK_KEY_minus:
	case GDK_KEY_KP_Subtract:
		pu->zoom = CLAMP(pu->zoom - 10, 50, 300);
		pintar();
		return TRUE;
	case GDK_KEY_Escape:
		/* Sin capa abierta no hace nada: salirse del púlpito por un
		 * Escape a media predicación sería un desastre. */
		return TRUE;
	case GDK_KEY_q:
	case GDK_KEY_Q:
		salir('Q');
		return TRUE;
	case GDK_KEY_e:
	case GDK_KEY_E:
		salir('E');
		return TRUE;
	case GDK_KEY_l:
	case GDK_KEY_L:
		salir('L');
		return TRUE;
	default:
		break;
	}
	return TRUE;	/* nada más se cuela: es una vista de entrega */
}

/* Ratón, como apoyo: abajo avanza, arriba retrocede. */
static gboolean
on_click(GtkWidget *widget, GdkEventButton *ev, gpointer datos)
{
	int alto = gtk_widget_get_allocated_height(widget);

	(void)datos;
	if (ev->type != GDK_BUTTON_PRESS || ev->button != 1)
		return FALSE;
	if (pu->capa_tipo != CAPA_NINGUNA) {
		capa_cerrar();
		return TRUE;
	}
	ir_a((ev->y > alto / 2) ? pu_siguiente(pu->sermon, pu->paso)
				: pu_anterior(pu->sermon, pu->paso));
	return TRUE;
}

/* --------------------------------------------------------------------
 * Construcción
 * ------------------------------------------------------------------ */

static void
on_destroy(GtkWidget *widget, gpointer datos)
{
	(void)widget;
	(void)datos;
	if (!pu)
		return;
	if (pu->reloj_id)
		g_source_remove(pu->reloj_id);
	if (pu->aviso_id)
		g_source_remove(pu->aviso_id);
	if (pu->rehacer_id)
		g_source_remove(pu->rehacer_id);
	{
		GdkDisplay *dpy = gdk_display_get_default();

		if (dpy && pu->mon_mas)
			g_signal_handler_disconnect(dpy, pu->mon_mas);
		if (dpy && pu->mon_menos)
			g_signal_handler_disconnect(dpy, pu->mon_menos);
	}
	segunda_cerrar();
	main_pulpito_cerrar(pu->sermon);
	g_free(pu);
	pu = NULL;
}

static void
poner_estilo(GtkWidget *win)
{
	/* Fondo oscuro y contraste alto: es lo que se ve desde el fondo del
	 * salón y lo que no deslumbra al que predica. Las clases solo las
	 * lleva esta ventana, así que el resto de la aplicación no se
	 * entera; se registra una sola vez. */
	static const char *css =
	    ".pulpito { background-color: #0f1114; color: #f2f3f5; }"
	    ".pulpito-capa { background-color: rgba(15,17,20,0.97); }"
	    /* Negro de verdad y no el gris del púlpito: en un proyector,
	     * un fondo casi negro sigue siendo una mancha de luz. */
	    /* La tapa de la segunda pantalla. Negro de verdad y no el gris
	     * del púlpito: en un proyector, un fondo casi negro sigue
	     * siendo una mancha de luz en la pared. */
	    ".pulpito-negro { background-color: #000000; }";
	static gboolean puesto = FALSE;
	GtkCssProvider *prov;

	if (puesto)
		return;
	prov = gtk_css_provider_new();
	gtk_css_provider_load_from_data(prov, css, -1, NULL);
	gtk_style_context_add_provider_for_screen(
	    gtk_widget_get_screen(win), GTK_STYLE_PROVIDER(prov),
	    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(prov);
	puesto = TRUE;
}

/* Pedirle a GTK un monitor concreto es lo suyo, pero la petición se
 * pierde si la lista de monitores ha cambiado entre medias (justo lo que
 * pasa al enchufar un proyector). Y una ventana de púlpito que se queda
 * flotando a su tamaño natural, en mitad de un sermón, es lo peor que
 * puede pasar: si al poco no está en pantalla completa, se le insiste
 * sin decirle en cuál. */
static gboolean
asegurar_completa(gpointer datos)
{
	GtkWidget *win = GTK_WIDGET(datos);
	GdkWindow *gw;

	if (!GTK_IS_WINDOW(win))
		return G_SOURCE_REMOVE;
	gw = gtk_widget_get_window(win);
	if (gw && !(gdk_window_get_state(gw) & GDK_WINDOW_STATE_FULLSCREEN))
		gtk_window_fullscreen(GTK_WINDOW(win));
	return G_SOURCE_REMOVE;
}

static void
completa_en(GtkWidget *win, int monitor)
{
	if (monitor >= 0)
		gtk_window_fullscreen_on_monitor(
		    GTK_WINDOW(win),
		    gdk_display_get_default_screen(
			gtk_widget_get_display(win)),
		    monitor);
	else
		gtk_window_fullscreen(GTK_WINDOW(win));
	g_timeout_add(600, asegurar_completa, win);
}

static GtkWidget *
columna(GtkWidget *dentro, GtkSizeGroup *anchos)
{
	GtkWidget *col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

	gtk_widget_set_size_request(col, PU_ANCHO, -1);
	gtk_widget_set_halign(col, GTK_ALIGN_CENTER);
	gtk_widget_set_hexpand(col, FALSE);
	if (anchos)
		gtk_size_group_add_widget(anchos, col);
	gtk_box_pack_start(GTK_BOX(dentro), col, TRUE, TRUE, 0);
	return col;
}

/* Dos botones dichos por su nombre, y no un sí/no: en el púlpito las dos
 * respuestas son distintas de verdad, y se leen de una ojeada. TRUE si se
 * eligió la primera. */
static gboolean
preguntar(GtkWindow *padre, const char *texto, const char *si, const char *no)
{
	GtkWidget *dlg;
	gint resp;

	dlg = gtk_message_dialog_new(padre, GTK_DIALOG_MODAL,
				     GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
				     "%s", texto);
	gtk_dialog_add_buttons(GTK_DIALOG(dlg), no, GTK_RESPONSE_NO, si,
			       GTK_RESPONSE_YES, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_YES);
	resp = gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);
	return (resp == GTK_RESPONSE_YES);
}

void
gui_pulpito_abrir(const char *modulo)
{
	PU_SERMON *sermon;
	GtkWidget *raiz, *overlay, *cabecera, *cuerpo, *col, *pie;
	GtkSizeGroup *anchos;
	int guardado;

	if (pu) {
		gtk_window_present(GTK_WINDOW(pu->win));
		return;
	}
	if (!modulo || !*modulo)
		return;

	/* El texto de todos los pasos se resuelve aquí, de una vez: en el
	 * púlpito no se va a la base de datos en cada pulsación. */
	sermon = main_pulpito_abrir(modulo);
	if (!sermon) {
		gui_generic_warning(_("No se pudo abrir ese bosquejo."));
		return;
	}
	if (pu_total(sermon) == 0) {
		gchar *aviso = g_strdup_printf(
		    _("«%s» no tiene pasos de púlpito.\n\n"
		      "Escribe los puntos del bosquejo en el editor."),
		    sermon->titulo);
		gboolean abrir = preguntar(
		    widgets.app ? GTK_WINDOW(widgets.app) : NULL, aviso,
		    _("Abrir editor"), _("Ahora no"));

		g_free(aviso);
		main_pulpito_cerrar(sermon);
		if (abrir)
			editor_create_new(modulo, NULL, BOOK_EDITOR);
		return;
	}

	pu = g_new0(PULPITO, 1);
	pu->sermon = sermon;
	pu->zoom = main_pulpito_zoom();
	pu->objetivo = main_pulpito_objetivo();
	pu->capa_tipo = CAPA_NINGUNA;
	pu->apoyo_de = -1;
	pu->segunda = (int)main_pulpito_segunda();

	/* Continuar donde se quedó, si tiene sentido preguntarlo. */
	guardado = main_pulpito_ultimo_paso(modulo);
	if (pu_preguntar_continuar(sermon, guardado)) {
		gchar *texto = g_strdup_printf(
		    _("«%s» se quedó a medias."), sermon->titulo);

		if (preguntar(widgets.app ? GTK_WINDOW(widgets.app) : NULL,
			      texto, _("Continuar"),
			      _("Empezar desde el inicio")))
			pu->paso = pu_paso_guardado(sermon, guardado);
		g_free(texto);
	}

	pu->win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(pu->win), sermon->titulo);
	gtk_style_context_add_class(gtk_widget_get_style_context(pu->win),
				    "pulpito");
	poner_estilo(pu->win);
	if (widgets.app)
		gtk_window_set_transient_for(GTK_WINDOW(pu->win),
					     GTK_WINDOW(widgets.app));
	gtk_widget_add_events(pu->win, GDK_BUTTON_PRESS_MASK);

	anchos = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	overlay = gtk_overlay_new();
	gtk_container_add(GTK_CONTAINER(pu->win), overlay);

	raiz = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(overlay), raiz);

	/* --- cabecera mínima --- */
	{
		GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		GtkWidget *izq = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		GtkWidget *fila = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

		gtk_widget_set_margin_top(caja, 18);
		gtk_widget_set_margin_bottom(caja, 6);
		cabecera = columna(caja, anchos);

		pu->lbl_cabecera = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(pu->lbl_cabecera), 0.0);
		gtk_label_set_ellipsize(GTK_LABEL(pu->lbl_cabecera),
					PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(pu->lbl_cabecera),
					      PU_MEDIDA);
		pu->lbl_ref = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(pu->lbl_ref), 0.0);
		gtk_box_pack_start(GTK_BOX(izq), pu->lbl_cabecera, FALSE,
				   FALSE, 0);
		gtk_box_pack_start(GTK_BOX(izq), pu->lbl_ref, FALSE, FALSE, 0);

		/* A la derecha, en columna: lo que se lleva predicado
		 * arriba y la hora debajo, en pequeño. El número grande es
		 * el que se mira de un vistazo desde el atril. */
		{
			GtkWidget *der = gtk_box_new(GTK_ORIENTATION_VERTICAL,
						     0);

			pu->lbl_tiempo = gtk_label_new("");
			gtk_label_set_xalign(GTK_LABEL(pu->lbl_tiempo), 1.0);
			pu->lbl_reloj = gtk_label_new("");
			gtk_label_set_xalign(GTK_LABEL(pu->lbl_reloj), 1.0);
			gtk_box_pack_start(GTK_BOX(der), pu->lbl_tiempo, FALSE,
					   FALSE, 0);
			gtk_box_pack_start(GTK_BOX(der), pu->lbl_reloj, FALSE,
					   FALSE, 0);
			gtk_widget_set_valign(der, GTK_ALIGN_START);

			pu->lbl_negro = gtk_label_new("");
			gtk_widget_set_valign(pu->lbl_negro, GTK_ALIGN_START);
			gtk_widget_set_margin_end(pu->lbl_negro, 18);
			/* La lleva pintar_estado_negro(), no el show_all. */
			gtk_widget_set_no_show_all(pu->lbl_negro, TRUE);

			gtk_box_pack_start(GTK_BOX(fila), izq, TRUE, TRUE, 0);
			gtk_box_pack_end(GTK_BOX(fila), der, FALSE, FALSE, 0);
			gtk_box_pack_end(GTK_BOX(fila), pu->lbl_negro, FALSE,
					 FALSE, 0);
		}
		gtk_box_pack_start(GTK_BOX(cabecera), fila, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(raiz), caja, FALSE, FALSE, 0);
	}

	/* --- el escenario --- */
	cuerpo = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(raiz), cuerpo, TRUE, TRUE, 0);
	col = columna(cuerpo, anchos);
	pu->caja_escena = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_valign(pu->caja_escena, GTK_ALIGN_CENTER);
	gtk_widget_set_vexpand(pu->caja_escena, TRUE);
	gtk_box_pack_start(GTK_BOX(col), pu->caja_escena, TRUE, TRUE, 0);

	/* --- el pie de camino --- */
	{
		GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

		gtk_widget_set_margin_bottom(caja, 20);
		pie = columna(caja, anchos);
		pu->lbl_siguiente = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(pu->lbl_siguiente), 0.0);
		gtk_label_set_ellipsize(GTK_LABEL(pu->lbl_siguiente),
					PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(pu->lbl_siguiente),
					      PU_MEDIDA);
		gtk_box_pack_start(GTK_BOX(pie), pu->lbl_siguiente, FALSE,
				   FALSE, 0);
		gtk_box_pack_end(GTK_BOX(raiz), caja, FALSE, FALSE, 0);
	}

	/* --- la capa de los overlays --- */
	{
		GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
		GtkWidget *fila = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		GtkWidget *ccol;

		pu->capa = caja;
		pu->capa_scroll = gtk_scrolled_window_new(NULL, NULL);
		gtk_style_context_add_class(gtk_widget_get_style_context(caja),
					    "pulpito-capa");
		gtk_box_pack_start(GTK_BOX(caja), fila, TRUE, TRUE, 0);
		ccol = columna(fila, anchos);
		gtk_widget_set_margin_top(ccol, 30);
		gtk_widget_set_margin_bottom(ccol, 30);

		pu->lbl_capa_titulo = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(pu->lbl_capa_titulo), 0.0);
		gtk_box_pack_start(GTK_BOX(ccol), pu->lbl_capa_titulo, FALSE,
				   FALSE, 0);

		pu->lbl_capa = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(pu->lbl_capa), 0.0);
		gtk_label_set_yalign(GTK_LABEL(pu->lbl_capa), 0.0);
		gtk_label_set_line_wrap(GTK_LABEL(pu->lbl_capa), TRUE);
		gtk_label_set_line_wrap_mode(GTK_LABEL(pu->lbl_capa),
					     PANGO_WRAP_WORD_CHAR);
		gtk_label_set_max_width_chars(GTK_LABEL(pu->lbl_capa),
					      PU_MEDIDA);
		gtk_container_add(GTK_CONTAINER(pu->capa_scroll), pu->lbl_capa);
		gtk_scrolled_window_set_policy(
		    GTK_SCROLLED_WINDOW(pu->capa_scroll), GTK_POLICY_NEVER,
		    GTK_POLICY_AUTOMATIC);
		gtk_box_pack_start(GTK_BOX(ccol), pu->capa_scroll, TRUE, TRUE,
				   0);

		gtk_overlay_add_overlay(GTK_OVERLAY(overlay), caja);
	}

	g_signal_connect(pu->win, "key-press-event", G_CALLBACK(on_tecla),
			 NULL);
	g_signal_connect(pu->win, "button-press-event", G_CALLBACK(on_click),
			 NULL);
	g_signal_connect(pu->win, "destroy", G_CALLBACK(on_destroy), NULL);

	pu->monitor = monitor_del_atril();
	completa_en(pu->win, pu->monitor);
	gtk_widget_show_all(pu->win);
	gtk_widget_hide(pu->capa);	/* la capa empieza cerrada */

	g_object_unref(anchos);

	/* La otra pantalla, ya con la ventana del atril puesta: hasta que
	 * no está en un monitor no se sabe cuál es el otro. */
	segunda_abrir();
	{
		GdkDisplay *dpy = gtk_widget_get_display(pu->win);

		pu->mon_mas = g_signal_connect(dpy, "monitor-added",
					       G_CALLBACK(on_pantallas), NULL);
		pu->mon_menos = g_signal_connect(dpy, "monitor-removed",
						 G_CALLBACK(on_pantallas),
						 NULL);
	}

	/* Cada segundo, que es lo que pide una cuenta en minutos y
	 * segundos; la hora solo se reescribe cuando cambia. */
	pu->reloj_id = g_timeout_add_seconds(1, tic_reloj, NULL);
	tic_reloj(NULL);
	pintar();
	gtk_widget_grab_focus(pu->win);
}

/* --------------------------------------------------------------------
 * Elegir el bosquejo
 * ------------------------------------------------------------------ */

void
gui_pulpito_elegir(GtkWindow *padre)
{
	GList *sermones = main_pulpito_sermones();
	GtkWidget *dlg, *combo, *caja, *reloj;
	GtkWidget *combo2 = NULL;
	GdkDisplay *dpy = gdk_display_get_default();
	GList *l;
	gint resp;

	if (!sermones) {
		gui_generic_warning(
		    _("No hay ningún bosquejo todavía.\n\n"
		      "Créalo en la barra lateral, en las listas y esquemas, "
		      "escribe los puntos y vuelve aquí."));
		return;
	}

	dlg = gtk_dialog_new_with_buttons(_("Abrir en púlpito"), padre,
					  GTK_DIALOG_MODAL |
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					  _("Cancelar"), GTK_RESPONSE_CANCEL,
					  _("Abrir en púlpito"),
					  GTK_RESPONSE_OK, NULL);
	caja = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
	combo = gtk_combo_box_text_new();
	for (l = sermones; l; l = l->next) {
		const char *m = (const char *)l->data;
		gchar *etiqueta;

		etiqueta = main_pulpito_predicado(m)
			       ? g_strdup_printf(_("%s  ·  predicado"), m)
			       : g_strdup(m);
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combo), m,
					  etiqueta);
		g_free(etiqueta);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
	gtk_widget_set_margin_start(combo, 12);
	gtk_widget_set_margin_end(combo, 12);
	gtk_widget_set_margin_top(combo, 12);
	gtk_widget_set_margin_bottom(combo, 12);
	gtk_box_pack_start(GTK_BOX(caja), combo, FALSE, FALSE, 0);

	/* La duración prevista, para el aviso del reloj del atril. En cero
	 * no avisa de nada, que es como viene: el que no quiere que le
	 * metan prisa no tiene que apagar nada. */
	{
		GtkWidget *filat = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
		GtkWidget *etq = gtk_label_new(_("Duración prevista:"));

		reloj = gtk_spin_button_new_with_range(0, 240, 5);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(reloj),
					  main_pulpito_objetivo());
		gtk_label_set_xalign(GTK_LABEL(etq), 0.0);
		gtk_box_pack_start(GTK_BOX(filat), etq, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(filat), reloj, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(filat),
				   gtk_label_new(_("minutos · 0 = sin aviso")),
				   FALSE, FALSE, 0);
		gtk_widget_set_margin_start(filat, 12);
		gtk_widget_set_margin_end(filat, 12);
		gtk_widget_set_margin_bottom(filat, 6);
		gtk_box_pack_start(GTK_BOX(caja), filat, FALSE, FALSE, 0);
	}

	/* Lo de la segunda pantalla solo se pregunta si la hay: en un
	 * portátil solo, la pregunta sobra. */
	if (dpy && gdk_display_get_n_monitors(dpy) > 1) {
		static const char *que[] = {N_("nada"), N_("el versículo"),
					    N_("el punto"),
					    N_("el versículo y el punto")};
		GtkWidget *etq = gtk_label_new(_("En la segunda pantalla:"));
		int i;

		combo2 = gtk_combo_box_text_new();
		for (i = 0; i < 4; ++i)
			gtk_combo_box_text_append_text(
			    GTK_COMBO_BOX_TEXT(combo2), _(que[i]));
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo2),
					 (int)main_pulpito_segunda());
		gtk_label_set_xalign(GTK_LABEL(etq), 0.0);
		gtk_widget_set_margin_start(etq, 12);
		gtk_widget_set_margin_top(etq, 6);
		gtk_widget_set_margin_start(combo2, 12);
		gtk_widget_set_margin_end(combo2, 12);
		gtk_widget_set_margin_bottom(combo2, 12);
		gtk_box_pack_start(GTK_BOX(caja), etq, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(caja), combo2, FALSE, FALSE, 0);
	}

	gtk_widget_show_all(dlg);

	resp = gtk_dialog_run(GTK_DIALOG(dlg));
	if (resp == GTK_RESPONSE_OK) {
		const gchar *id =
		    gtk_combo_box_get_active_id(GTK_COMBO_BOX(combo));
		gchar *elegido = g_strdup(id ? id : "");

		main_pulpito_objetivo_poner(gtk_spin_button_get_value_as_int(
		    GTK_SPIN_BUTTON(reloj)));
		if (combo2)
			main_pulpito_segunda_poner((PU_SEGUNDA)
			    gtk_combo_box_get_active(GTK_COMBO_BOX(combo2)));
		gtk_widget_destroy(dlg);
		if (*elegido)
			gui_pulpito_abrir(elegido);
		g_free(elegido);
	} else
		gtk_widget_destroy(dlg);

	g_list_free_full(sermones, g_free);
}
