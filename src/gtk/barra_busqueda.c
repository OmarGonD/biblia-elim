/*
 * Biblia Elim
 * barra_busqueda.c - buscar dentro del texto, sin ventana que estorbe
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

/*
 * La búsqueda dentro del capítulo se hacía en una ventana aparte, y esa
 * ventana se plantaba encima del texto: el versículo hallado quedaba
 * detrás de ella y no había forma de apartarla en un escritorio que
 * coloca las ventanas por su cuenta. Aquí la búsqueda vive dentro de la
 * ventana principal, en una franja que se despliega bajo la barra de
 * navegación, así que no tapa nada ni hay que moverla.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "gui/barra_busqueda.h"
#include "gui/widgets.h"
#include "main/search_dialog.h"
#include "main/settings.h"
#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

static GtkWidget *barra;
static GtkWidget *entrada;
static GtkWidget *contador;
static GtkWidget *b_anterior;
static GtkWidget *b_siguiente;

/* El panel en el que se está buscando y el aviso que nos tiene al día de
 * lo que va hallando; los dos se sueltan al cambiar de panel. */
static GtkWidget *objetivo;
static gulong aviso;

static void
poner_al_dia(void)
{
	const gchar *texto;
	gint total, cual;
	gchar *rotulo;
	gboolean hay;

	if (!contador)
		return;

	texto = gtk_entry_get_text(GTK_ENTRY(entrada));
	total = objetivo ? XIPHOS_HTML_FIND_COUNT(objetivo) : 0;
	cual = objetivo ? XIPHOS_HTML_FIND_POSITION(objetivo) : 0;
	hay = (total > 0);

	gtk_widget_set_sensitive(b_anterior, hay);
	gtk_widget_set_sensitive(b_siguiente, hay);

	if (!texto || !*texto) {
		gtk_label_set_text(GTK_LABEL(contador), "");
		gtk_style_context_remove_class(gtk_widget_get_style_context(entrada),
					       GTK_STYLE_CLASS_ERROR);
		return;
	}

	if (!hay) {
		gtk_label_set_text(GTK_LABEL(contador), _("sin coincidencias"));
		gtk_style_context_add_class(gtk_widget_get_style_context(entrada),
					    GTK_STYLE_CLASS_ERROR);
		return;
	}

	gtk_style_context_remove_class(gtk_widget_get_style_context(entrada),
				       GTK_STYLE_CLASS_ERROR);
	if (cual > 0)
		rotulo = g_strdup_printf(_("%d de %d"), cual, total);
	else
		rotulo = g_strdup_printf(ngettext("%d coincidencia",
						  "%d coincidencias", total),
					 total);
	gtk_label_set_text(GTK_LABEL(contador), rotulo);
	g_free(rotulo);
}

static void
on_find_updated(GtkWidget *html, gpointer datos)
{
	poner_al_dia();
}

static void
soltar_objetivo(gboolean limpiar)
{
	if (!objetivo)
		return;
	if (aviso) {
		g_signal_handler_disconnect(objetivo, aviso);
		aviso = 0;
	}
	if (limpiar)
		XIPHOS_HTML_FIND_CLEAR(objetivo);
	objetivo = NULL;
}

static void
buscar(void)
{
	const gchar *texto = gtk_entry_get_text(GTK_ENTRY(entrada));

	if (!objetivo)
		return;
	XIPHOS_HTML_FIND_ALL(objetivo, texto);
	if (texto && *texto)
		XIPHOS_HTML_FIND_STEP(objetivo, TRUE);
	poner_al_dia();
}

static void
saltar(gboolean adelante)
{
	if (!objetivo)
		return;
	XIPHOS_HTML_FIND_STEP(objetivo, adelante);
	poner_al_dia();
}

static void
on_cambio(GtkSearchEntry *entry, gpointer datos)
{
	buscar();
}

static void
on_siguiente(GtkWidget *w, gpointer datos)
{
	saltar(TRUE);
}

static void
on_anterior(GtkWidget *w, gpointer datos)
{
	saltar(FALSE);
}

static void
on_parar(GtkSearchEntry *entry, gpointer datos)
{
	gui_barra_busqueda_ocultar();
}

/* Enter avanza; con Mayúsculas, retrocede -- como en cualquier navegador. */
static gboolean
on_tecla(GtkWidget *w, GdkEventKey *ev, gpointer datos)
{
	if ((ev->keyval != GDK_KEY_Return) && (ev->keyval != GDK_KEY_KP_Enter) &&
	    (ev->keyval != GDK_KEY_ISO_Enter))
		return FALSE;
	saltar(!(ev->state & GDK_SHIFT_MASK));
	return TRUE;
}

static void
on_avanzada(GtkWidget *w, gpointer datos)
{
	gui_barra_busqueda_ocultar();
	main_open_search_dialog();
}

/* La franja se pliega sola al pulsar su aspa: al plegarse hay que quitar
 * lo realzado, o el texto se queda pintado de amarillo. */
static void
on_modo(GObject *obj, GParamSpec *spec, gpointer datos)
{
	if (!gtk_search_bar_get_search_mode(GTK_SEARCH_BAR(barra)))
		soltar_objetivo(TRUE);
}

static GtkWidget *
boton_flecha(const gchar *icono, const gchar *ayuda, GCallback cb)
{
	GtkWidget *b = gtk_button_new_from_icon_name(icono, GTK_ICON_SIZE_BUTTON);

	gtk_widget_set_tooltip_text(b, ayuda);
	gtk_widget_set_sensitive(b, FALSE);
	gtk_widget_set_focus_on_click(b, FALSE);
	g_signal_connect(b, "clicked", cb, NULL);
	return b;
}

GtkWidget *
gui_barra_busqueda_crear(void)
{
	GtkWidget *caja, *flechas, *avanzada;

	if (barra)
		return barra;

	barra = gtk_search_bar_new();
	gtk_style_context_add_class(gtk_widget_get_style_context(barra),
				    "elim-barra-busqueda");
	gtk_search_bar_set_show_close_button(GTK_SEARCH_BAR(barra), TRUE);

	caja = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

	entrada = gtk_search_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(entrada), 28);
	gtk_entry_set_placeholder_text(GTK_ENTRY(entrada),
				       _("Buscar en el texto"));
	gtk_widget_set_tooltip_text(entrada,
				    _("No hacen falta las tildes: «espiritu» "
				      "encuentra «Espíritu»."));
	gtk_box_pack_start(GTK_BOX(caja), entrada, FALSE, FALSE, 0);

	contador = gtk_label_new("");
	gtk_style_context_add_class(gtk_widget_get_style_context(contador),
				    GTK_STYLE_CLASS_DIM_LABEL);
	gtk_label_set_width_chars(GTK_LABEL(contador), 14);
	gtk_label_set_xalign(GTK_LABEL(contador), 0.0);
	gtk_box_pack_start(GTK_BOX(caja), contador, FALSE, FALSE, 0);

	flechas = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(flechas),
				    GTK_STYLE_CLASS_LINKED);
	b_anterior = boton_flecha("go-up-symbolic", _("Anterior (Mayús+Intro)"),
				  G_CALLBACK(on_anterior));
	b_siguiente = boton_flecha("go-down-symbolic", _("Siguiente (Intro)"),
				   G_CALLBACK(on_siguiente));
	gtk_box_pack_start(GTK_BOX(flechas), b_anterior, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(flechas), b_siguiente, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(caja), flechas, FALSE, FALSE, 0);

	/* Quien no encuentra lo que busca en este capítulo casi siempre lo
	 * que quiere es buscarlo en toda la Biblia. */
	avanzada = gtk_button_new_with_label(_("Buscar en toda la Biblia"));
	gtk_style_context_add_class(gtk_widget_get_style_context(avanzada), "flat");
	gtk_widget_set_focus_on_click(avanzada, FALSE);
	g_signal_connect(avanzada, "clicked", G_CALLBACK(on_avanzada), NULL);
	gtk_box_pack_start(GTK_BOX(caja), avanzada, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(barra), caja);
	gtk_search_bar_connect_entry(GTK_SEARCH_BAR(barra), GTK_ENTRY(entrada));

	g_signal_connect(entrada, "search-changed", G_CALLBACK(on_cambio), NULL);
	g_signal_connect(entrada, "next-match", G_CALLBACK(on_siguiente), NULL);
	g_signal_connect(entrada, "previous-match", G_CALLBACK(on_anterior), NULL);
	g_signal_connect(entrada, "stop-search", G_CALLBACK(on_parar), NULL);
	g_signal_connect(entrada, "key-press-event", G_CALLBACK(on_tecla), NULL);
	g_signal_connect(barra, "notify::search-mode-enabled",
			 G_CALLBACK(on_modo), NULL);

	gtk_widget_show_all(caja);
	gtk_widget_show(barra);
	return barra;
}

void
gui_barra_busqueda_mostrar(GtkWidget *html)
{
	if (!barra || !html)
		return;

	if (objetivo != html) {
		soltar_objetivo(TRUE);
		objetivo = html;
		aviso = g_signal_connect(objetivo, "find-updated",
					 G_CALLBACK(on_find_updated), NULL);
	}

	gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(barra), TRUE);
	gtk_widget_grab_focus(entrada);
	gtk_editable_select_region(GTK_EDITABLE(entrada), 0, -1);

	/* Reabrir la barra con lo de antes escrito ha de volver a marcarlo:
	 * el capítulo puede ser otro desde entonces. */
	if (*gtk_entry_get_text(GTK_ENTRY(entrada)))
		buscar();
	else
		poner_al_dia();
}

void
gui_barra_busqueda_ocultar(void)
{
	if (!barra)
		return;
	/* Basta con plegarla: on_modo() se encarga de apagar lo realzado. */
	gtk_search_bar_set_search_mode(GTK_SEARCH_BAR(barra), FALSE);
}
