/*
 * Xiphos Bible Study Tool
 * navbar_verse.c - navigation bar for versekey modules
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "editor/slib-editor.h"

#include "gui/navbar_versekey.h"
#include "gui/bibletext_dialog.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/lists.h"
#include "main/module_dialogs.h"
#include "main/navbar_versekey.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/tab_history.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"

NAVBAR_VERSEKEY navbar_versekey;

extern PASSAGE_TAB_INFO *cur_passage_tab;

/******************************************************************************
 * Name
 *   menu_deactivate_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void menu_deactivate_callback (GtkWidget *widget, gpointer user_data)
 *
 * Description
 *   return toggle button to normal
 *
 * Return value
 *   void
 */

static void menu_deactivate_callback(GtkWidget *widget,
				     gpointer user_data)
{
	GtkWidget *menu_button;

	menu_button = GTK_WIDGET(user_data);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(menu_button),
				     FALSE);
}

/******************************************************************************
 * Name
 *   menu_position_under
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void menu_position_under(GtkMenu * menu, int * x, int * y,
 *				gboolean * push_in, gpointer user_data)
 *
 * Description
 *   position drop down menu under toogle button
 *
 *
 * Return value
 *   void
 */

static void menu_position_under(GtkMenu *menu, int *x, int *y,
				gboolean *push_in, gpointer user_data)
{
	GtkWidget *widget;
	GtkAllocation allocation;

	g_return_if_fail(GTK_IS_BUTTON(user_data));
#if GTK_CHECK_VERSION(2, 20, 0)
	g_return_if_fail(gtk_widget_get_window(user_data));
#else
	g_return_if_fail(GTK_WIDGET_NO_WINDOW(user_data));
#endif
	widget = GTK_WIDGET(user_data);

	gdk_window_get_origin(gtk_widget_get_window(widget), x, y);
	gtk_widget_get_allocation(widget, &allocation);
	*x += allocation.x;
	*y += allocation.y + allocation.height;

	*push_in = FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_button_press_callback(GtkWidget *widget,
					     GdkEventButton *event,
					     gpointer user_data)
{
	GtkWidget *menu;

	menu = main_versekey_drop_down_new(cur_passage_tab);
	if (!menu)
		return 0;
	g_signal_connect(menu, "deactivate",
			 G_CALLBACK(menu_deactivate_callback), widget);
	if ((event->type == GDK_BUTTON_PRESS) && event->button == 1) {
		gtk_widget_grab_focus(widget);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
					     TRUE);
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_widget(GTK_MENU(menu), widget, GDK_GRAVITY_SOUTH_WEST, GDK_GRAVITY_NORTH_WEST, NULL);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
			       menu_position_under, widget, event->button,
			       event->time);
#endif
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_book_button_press_callback(GtkWidget *widget,
						  GdkEventButton *event,
						  gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_widget_grab_focus(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_book(navbar_versekey, NB_MAIN,
				 NULL, NULL, widget);
	return TRUE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_chapter_button_press_callback(GtkWidget *widget,
						     GdkEventButton *
							 event,
						     gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_widget_grab_focus(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_chapter(navbar_versekey, NB_MAIN,
				    NULL, NULL, widget);
	return TRUE;
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GdkEventButton *event,
 *			      gpointer user_data)
 *
 * Description
 *    make the tooglebutton act like a gtk optionmenu by dropping a popup
 *    under the button
 *
 * Return value
 *   gboolean
 */

static gboolean select_verse_button_press_callback(GtkWidget *widget,
						   GdkEventButton *event,
						   gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gtk_widget_grab_focus(widget);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), TRUE);
	main_versekey_popup_verse(navbar_versekey, NB_MAIN,
				    NULL, NULL, widget);
	return TRUE;
}

/******************************************************************************
 * Name
 *  on_button_history_next_clicked
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  void on_button_history_next_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_button_history_next_clicked(GtkButton *button, gpointer user_data)
{
	main_navigate_tab_history(1);
}

/******************************************************************************
 * Name
 *  on_button_history_back_clicked
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  void on_button_history_back_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void on_button_history_back_clicked(GtkButton *button, gpointer user_data)
{
	main_navigate_tab_history(0);
}

/******************************************************************************
 * Name
 *   on_entry_activate
 *
 * Synopsis
 *   #include "bibletext_dialog.h"
 *
 *   void on_entry_activate(GtkEntry * entry, DIALOG_DATA * c)
 *
 * Description
 *   go to verse in free form entry if user hit <enter>
 *
 * Return value
 *   void
 */

static void on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *rawtext;
	const gchar *gkey, *buf = gtk_entry_get_text(entry);

	if (buf == NULL)
		return;
	/* handle potential subsection anchor */
	if ((settings.special_anchor = strchr(buf, '#')) || /* thml */
	    (settings.special_anchor = strchr(buf, '!')))   /* osisref */
		*settings.special_anchor = '\0';

	rawtext =
	    main_get_raw_text(navbar_versekey.module_name->str,
			      (gchar *)buf);

	if (!rawtext || (rawtext && (strlen(rawtext) < 2))) {
		gtk_entry_set_text(entry, navbar_versekey.key->str);
		g_free(rawtext);
		return;
	}
	gkey =
	    main_get_valid_key(settings.MainWindowModule, (gchar *)buf);

	// we got a valid key. but was it really a valid key within v11n?
	// for future use in determining whether to show normal navbar content.
	navbar_versekey.valid_key =
	    main_is_Bible_key(settings.MainWindowModule, gkey);

	if (settings.special_anchor)
		*settings.special_anchor = '#'; /* put it back. */
	if (gkey == NULL) {
		gtk_entry_set_text(entry, navbar_versekey.key->str);
		return;
	}

	gchar *url = g_strdup_printf("sword:///%s%s", gkey,
				     (settings.special_anchor ? settings.special_anchor : ""));

	navbar_versekey.module_name =
	    g_string_assign(navbar_versekey.module_name,
			    settings.MainWindowModule);
	main_navbar_versekey_set(navbar_versekey, gkey);
	main_url_handler(url, TRUE);
	if (url)
		g_free(url);
	if (gkey)
		g_free((gchar *)gkey);
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_verse_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_verse_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_verse_scroll_event(GtkWidget *widget,
							GdkEvent *event,
							gpointer user_data)
{
	main_navbar_versekey_spin_verse(navbar_versekey,
					event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_chapter_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_chapter_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_chapter_scroll_event(GtkWidget *widget,
							  GdkEvent *event,
							  gpointer user_data)
{
	main_navbar_versekey_spin_chapter(navbar_versekey,
					  event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *  on_button_verse_menu_book_scroll_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  gboolean on_button_verse_menu_book_scroll_event(GtkWidget * widget,
 *                                           GdkEvent * event,
 *                                           gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_button_verse_menu_book_scroll_event(GtkWidget *widget,
						       GdkEvent *event,
						       gpointer user_data)
{
	main_navbar_versekey_spin_book(navbar_versekey,
				       event->scroll.direction);
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_up_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_eventbox_button_release_event (GtkWidget * widget,
 *                                       	GdkEventButton * event,
 *                                       	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_up_eventbox_button_release_event(GtkWidget *widget,
						    GdkEventButton *event,
						    gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		main_navbar_versekey_spin_book(navbar_versekey, 0);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_versekey, 0);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_versekey, 0);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   on_down_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_eventbox_button_release_event(GtkWidget * widget,
 *                                      	GdkEventButton * event,
 *                                      	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_down_eventbox_button_release_event(GtkWidget *widget,
						      GdkEventButton *event,
						      gpointer user_data)
{
	switch (GPOINTER_TO_INT(user_data)) {
	case BOOK_BUTTON:
		main_navbar_versekey_spin_book(navbar_versekey, 1);
		break;
	case CHAPTER_BUTTON:
		main_navbar_versekey_spin_chapter(navbar_versekey, 1);
		break;
	case VERSE_BUTTON:
		main_navbar_versekey_spin_verse(navbar_versekey, 1);
		break;
	}
	return FALSE;
}

/******************************************************************************
 * Name
 *   access_on_up_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_up_eventbox_button_release_event(gpointer element)
 *
 * Description
 *   access to internal static method from main_window.c
 *
 * Return value
 *   gboolean
 */

gboolean access_on_up_eventbox_button_release_event(gint element)
{
	return on_up_eventbox_button_release_event(NULL, NULL, GINT_TO_POINTER(element));
}

/******************************************************************************
 * Name
 *   access_on_down_eventbox_button_release_event
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean on_down_eventbox_button_release_event(gpointer element)
 *
 * Description
 *   access to internal static method from main_window.c
 *
 * Return value
 *   gboolean
 */

gboolean access_on_down_eventbox_button_release_event(gint element)
{
	return on_down_eventbox_button_release_event(NULL, NULL, GINT_TO_POINTER(element));
}

/******************************************************************************
 * Name
 *   _connect_signals
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   void _connect_signals(NAVBAR_VERSEKEY navbar)
 *
 * Description
 *
 *
 * Return value
 *  void
 */

static void _connect_signals(NAVBAR_VERSEKEY navbar)
{

	g_signal_connect((gpointer)navbar.lookup_entry,
			 "activate", G_CALLBACK(on_entry_activate), NULL);
	g_signal_connect((gpointer)navbar.button_book_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_book_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(BOOK_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_chapter_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(CHAPTER_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_up,
			 "button_release_event",
			 G_CALLBACK(on_up_eventbox_button_release_event),
			 GINT_TO_POINTER(VERSE_BUTTON));
	g_signal_connect((gpointer)navbar.button_verse_down,
			 "button_release_event",
			 G_CALLBACK(on_down_eventbox_button_release_event),
			 GINT_TO_POINTER(VERSE_BUTTON));

	g_signal_connect((gpointer)navbar.button_history_back,
			 "clicked",
			 G_CALLBACK(on_button_history_back_clicked), NULL);
	g_signal_connect((gpointer)navbar.button_history_next, "clicked",
			 G_CALLBACK(on_button_history_next_clicked), NULL);
	g_signal_connect((gpointer)navbar.button_history_menu,
			 "button_press_event",
			 G_CALLBACK(select_button_press_callback), NULL);
	g_signal_connect((gpointer)navbar.button_book_menu,
			 "button_press_event",
			 G_CALLBACK(select_book_button_press_callback),
			 NULL);
	g_signal_connect((gpointer)navbar.button_chapter_menu,
			 "button_press_event",
			 G_CALLBACK(select_chapter_button_press_callback),
			 NULL);
	g_signal_connect((gpointer)navbar.button_verse_menu,
			 "button_press_event",
			 G_CALLBACK(select_verse_button_press_callback),
			 NULL);
#if !GTK_CHECK_VERSION(3, 4, 0)
	g_signal_connect((gpointer)navbar.button_verse_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_verse_scroll_event), NULL);
	g_signal_connect((gpointer)navbar.button_chapter_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_chapter_scroll_event),
			 NULL);
	g_signal_connect((gpointer)navbar.button_book_menu,
			 "scroll_event",
			 G_CALLBACK(on_button_verse_menu_book_scroll_event), NULL);
#endif
}

/* Picker de versión bíblica en la barra de navegación: cambia el
 * módulo de la ventana principal preservando libro/capítulo/versículo
 * enfocado (main_display_bible() re-usa settings.currentverse tal
 * cual se lo pasamos).
 *
 * Va agrupado por idioma, y el idioma de la interfaz manda: la lista era
 * plana y mezclaba castellano, inglés, griego, hebreo y siríaco en el
 * orden en que SWORD devuelve los módulos, así que para cambiar de
 * versión había que leerla entera.
 *
 * Es un botón de menú y no un GtkComboBox porque un combo abre alineando
 * el elemento activo con el botón: con la KJV puesta —la última de la
 * lista— el desplegable se abría desplazado hasta el final y el grupo de
 * arriba, el del idioma de la interfaz, no llegaba a verse. Un menú abre
 * siempre por el principio, y además se ensancha con su contenido en vez
 * de quedarse en los 150 px del botón.
 */

/* Los nombres que da SWORD son endónimos —"English", "Ελληνικά",
 * "עברית מקראית"—, que como cabecera de una lista en castellano no
 * ayudan. Los idiomas con los que se trabaja aquí llevan nombre propio;
 * el resto se queda con el de SWORD y se va al final de la lista.
 *
 * El "orden" de esta tabla es sólo el desempate: quien manda es el
 * idioma de la interfaz, que se pone el primero sea cual sea. */
static const struct {
	const char *codigo;
	const char *nombre;
	int orden;
} idiomas_conocidos[] = {
	{"es", N_("Español"), 0},
	{"grc", N_("Griego koiné"), 1},
	{"hbo", N_("Hebreo bíblico"), 2},
	{"el", N_("Griego"), 3},
	{"he", N_("Hebreo"), 4},
	{"arc", N_("Arameo"), 5},
	{"syr", N_("Siríaco"), 6},
	{"la", N_("Latín"), 7},
	{"en", N_("Inglés"), 8},
	{"pt", N_("Portugués"), 9},
	{NULL, NULL, 0}
};
#define ORDEN_INTERFAZ 0
#define ORDEN_TABLA 10		/* los conocidos, tras el de la interfaz */
#define ORDEN_RESTO 50

/* El idioma de cada módulo se lo pedimos a SWORD, y dentro de
 * create_mainwindow() —que es donde nace esta barra— el backend todavía
 * no existe: main() lo levanta unas líneas más abajo, y create_mainwindow()
 * hace girar el bucle principal por el camino, así que ni siquiera vale
 * aplazarlo a un idle. La lista se llena cuando main() avisa de que Sword
 * está en pie; a partir de ahí, cualquier barra nueva se llena al
 * construirse. */
static gboolean sword_disponible = FALSE;
static GtkWidget *version_etiqueta = NULL;
static GtkWidget *version_menu = NULL;
/* Marcar el elemento del módulo actual dispara "toggled" igual que
 * pulsarlo; esto distingue una cosa de la otra. */
static gboolean version_sincronizando = FALSE;

static void gui_navbar_fill_version_combo(void);


/* ¿Es <codigo> el idioma en que está la interfaz? g_get_language_names()
 * devuelve lo que gettext está usando de verdad —"es_PE.UTF-8", "es_PE",
 * "es", "C"—, que es justo lo que hay que comparar: el menú en castellano
 * quiere las biblias en castellano arriba, y el mismo binario en inglés
 * las inglesas. */
static gboolean
es_idioma_de_la_interfaz(const char *codigo)
{
	const gchar *const *idiomas = g_get_language_names();
	gsize n = strlen(codigo);
	int i;

	for (i = 0; idiomas && idiomas[i]; i++) {
		char siguiente;

		if (g_ascii_strncasecmp(idiomas[i], codigo, n))
			continue;
		/* "es" no debe casar con "estonio"; sólo con "es", "es_PE"
		 * o "es_PE.UTF-8" */
		siguiente = idiomas[i][n];
		if (siguiente == '\0' || siguiente == '_' ||
		    siguiente == '-' || siguiente == '.')
			return TRUE;
	}
	return FALSE;
}

typedef struct {
	gchar *id;
	gchar *desc;
	gchar *idioma;
	int orden;
} EntradaVersion;

static void
version_idioma(const gchar *modulo, gchar **nombre, int *orden)
{
	gchar *codigo = main_get_mod_config_entry(modulo, "Lang");
	gchar *base = NULL;
	int i;

	*nombre = NULL;
	*orden = ORDEN_RESTO;
	if (codigo && *codigo) {
		gchar *sufijo = strpbrk(codigo, "-_");

		/* "en-GB" o "he-Hebr-IL": basta la raíz */
		base = sufijo ? g_strndup(codigo, sufijo - codigo) : NULL;
		for (i = 0; idiomas_conocidos[i].codigo; i++) {
			if (!g_ascii_strcasecmp(codigo,
						idiomas_conocidos[i].codigo) ||
			    (base && !g_ascii_strcasecmp(base,
							 idiomas_conocidos[i].codigo))) {
				*nombre = g_strdup(_(idiomas_conocidos[i].nombre));
				*orden = ORDEN_TABLA + idiomas_conocidos[i].orden;
				break;
			}
		}
		if (es_idioma_de_la_interfaz(base ? base : codigo))
			*orden = ORDEN_INTERFAZ;
	}
	if (!*nombre) {
		const char *swordiano = main_get_module_language(modulo);

		*nombre = g_strdup((swordiano && *swordiano) ? swordiano
							    : _("Otros"));
	}
	g_free(base);
	g_free(codigo);
}

static gint
version_comparar(gconstpointer a, gconstpointer b)
{
	const EntradaVersion *x = a;
	const EntradaVersion *y = b;
	gint r;

	if (x->orden != y->orden)
		return x->orden - y->orden;
	r = g_utf8_collate(x->idioma, y->idioma);
	if (r)
		return r;
	return g_utf8_collate(x->desc, y->desc);
}

static void
version_liberar(gpointer datos)
{
	EntradaVersion *e = datos;

	g_free(e->id);
	g_free(e->desc);
	g_free(e->idioma);
	g_free(e);
}

static void
on_version_elegida(GtkCheckMenuItem *item, gpointer datos)
{
	const char *mod = g_object_get_data(G_OBJECT(item), "modulo");

	(void)datos;
	if (version_sincronizando || !gtk_check_menu_item_get_active(item))
		return;
	if (!mod || (settings.MainWindowModule &&
		     !strcmp(mod, settings.MainWindowModule)))
		return;
	main_display_bible((char *)mod, settings.currentverse);
}

/* El botón enseña la versión puesta; el nombre completo, en el tooltip,
 * porque a 150 px no cabe entero. */
static void
version_actualizar_boton(void)
{
	GList *hijos, *n;
	const gchar *texto = NULL;

	if (!version_etiqueta || !version_menu || !settings.MainWindowModule)
		return;
	hijos = gtk_container_get_children(GTK_CONTAINER(version_menu));
	for (n = hijos; n; n = n->next) {
		const char *mod = g_object_get_data(G_OBJECT(n->data), "modulo");

		if (mod && !strcmp(mod, settings.MainWindowModule)) {
			texto = gtk_menu_item_get_label(GTK_MENU_ITEM(n->data));
			break;
		}
	}
	g_list_free(hijos);
	if (!texto)
		texto = settings.MainWindowModule;
	gtk_label_set_text(GTK_LABEL(version_etiqueta), texto);
	gtk_widget_set_tooltip_text(widgets.combo_bible_version, texto);
}

void
gui_navbar_version_combo_refill(void)
{
	sword_disponible = TRUE;
	gui_navbar_fill_version_combo();
}

static void
gui_navbar_fill_version_combo(void)
{
	GtkWidget *menu;
	GSList *grupo = NULL;
	GList *l, *d, *entradas = NULL, *n;
	gchar *idioma_actual = NULL;
	GtkWidget *primera = NULL, *elegida = NULL;

	if (!widgets.combo_bible_version || !sword_disponible)
		return;

	for (l = get_list(TEXT_LIST), d = get_list(TEXT_DESC_LIST); l;
	     l = l->next, d = d ? d->next : NULL) {
		const char *name = (const char *)l->data;
		const char *desc = d ? (const char *)d->data : NULL;
		EntradaVersion *e;

		if (!name)
			continue;
		e = g_new0(EntradaVersion, 1);
		e->id = g_strdup(name);
		e->desc = g_strdup((desc && *desc) ? desc : name);
		version_idioma(name, &e->idioma, &e->orden);
		entradas = g_list_prepend(entradas, e);
	}
	entradas = g_list_sort(entradas, version_comparar);

	menu = gtk_menu_new();
	version_sincronizando = TRUE;
	for (n = entradas; n; n = n->next) {
		EntradaVersion *e = n->data;
		GtkWidget *item;

		if (g_strcmp0(idioma_actual, e->idioma)) {
			item = gtk_menu_item_new_with_label(e->idioma);
			gtk_widget_set_sensitive(item, FALSE);
			gtk_style_context_add_class(
			    gtk_widget_get_style_context(item),
			    "elim-menu-titulo");
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			g_free(idioma_actual);
			idioma_actual = g_strdup(e->idioma);
		}
		item = gtk_radio_menu_item_new_with_label(grupo, e->desc);
		grupo = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
		g_object_set_data_full(G_OBJECT(item), "modulo",
				       g_strdup(e->id), g_free);
		if (!primera)
			primera = item;
		if (settings.MainWindowModule &&
		    !strcmp(e->id, settings.MainWindowModule))
			elegida = item;
		g_signal_connect(item, "toggled",
				 G_CALLBACK(on_version_elegida), NULL);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	}
	/* Si el módulo guardado ya no está instalado, se marca el primero
	 * para no dejar el menú sin señalar. No se cambia de módulo desde
	 * aquí: esto corre al arrancar, antes de que la ventana esté lista,
	 * y de un módulo que falta ya se ocupa settings.c. */
	if (!elegida)
		elegida = primera;
	if (elegida)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(elegida),
					       TRUE);
	version_sincronizando = FALSE;

	g_free(idioma_actual);
	g_list_free_full(entradas, version_liberar);

	gtk_widget_show_all(menu);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(widgets.combo_bible_version),
				  menu);
	version_menu = menu;
	version_actualizar_boton();
}

void
gui_navbar_version_combo_sync(void)
{
	GList *hijos, *n;

	if (!version_menu || !settings.MainWindowModule)
		return;
	version_sincronizando = TRUE;
	hijos = gtk_container_get_children(GTK_CONTAINER(version_menu));
	for (n = hijos; n; n = n->next) {
		const char *mod = g_object_get_data(G_OBJECT(n->data), "modulo");

		if (mod && !strcmp(mod, settings.MainWindowModule)) {
			gtk_check_menu_item_set_active(
			    GTK_CHECK_MENU_ITEM(n->data), TRUE);
			break;
		}
	}
	g_list_free(hijos);
	version_sincronizando = FALSE;
	version_actualizar_boton();
}


/******************************************************************************
 * Name
 *  gui_navbar_versekey_new
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *  GtkWidget *gui_navbar_book_new(void)
 *
 * Description
 *   create a new Bible navigation toolbar and return it
 *
 * Return value
 *   GtkWidget *
 */

GtkWidget *gui_navbar_versekey_new(void)
{

	GtkBuilder *gxml;
#if GTK_CHECK_VERSION(3, 4, 0)
	GtkWidget *eventbox;
#endif

/* build the widget */
	gxml = elim_gtk_builder_new();
	gtk_builder_add_from_resource(gxml, "/org/xiphos/ui/navbar_versekey.gtkbuilder", NULL);
	navbar_versekey.dialog = FALSE;
	navbar_versekey.module_name =
	    g_string_new(settings.MainWindowModule);
	navbar_versekey.key = g_string_new(settings.currentverse);

	navbar_versekey.navbar = UI_GET_ITEM(gxml, "navbar");

	navbar_versekey.button_history_back =
	    UI_GET_ITEM(gxml, "button_history_back");
	navbar_versekey.button_history_next =
	    UI_GET_ITEM(gxml, "button_history_foward");
	navbar_versekey.button_history_menu =
	    UI_GET_ITEM(gxml, "togglebutton_history_list");

	navbar_versekey.button_book_up = UI_GET_ITEM(gxml, "eventbox9");
	navbar_versekey.button_book_down = UI_GET_ITEM(gxml, "eventbox6");
	navbar_versekey.button_chapter_up = UI_GET_ITEM(gxml, "eventbox8");
	navbar_versekey.button_chapter_down =
	    UI_GET_ITEM(gxml, "eventbox4");
	navbar_versekey.button_verse_up = UI_GET_ITEM(gxml, "eventbox7");
	navbar_versekey.button_verse_down = UI_GET_ITEM(gxml, "eventbox1");

	navbar_versekey.arrow_book_up = UI_GET_ITEM(gxml, "image12");
	navbar_versekey.arrow_book_down = UI_GET_ITEM(gxml, "image14");
	navbar_versekey.arrow_chapter_up = UI_GET_ITEM(gxml, "image8");
	navbar_versekey.arrow_chapter_down = UI_GET_ITEM(gxml, "image10");
	navbar_versekey.arrow_verse_up = UI_GET_ITEM(gxml, "image6");
	navbar_versekey.arrow_verse_down = UI_GET_ITEM(gxml, "image5");

	navbar_versekey.button_book_menu =
	    UI_GET_ITEM(gxml, "togglebutton_book");
	navbar_versekey.button_chapter_menu =
	    UI_GET_ITEM(gxml, "togglebutton_chapter");
	navbar_versekey.button_verse_menu =
	    UI_GET_ITEM(gxml, "togglebutton_verse");
	navbar_versekey.lookup_entry = UI_GET_ITEM(gxml, "entry_lookup");
	navbar_versekey.label_book_menu = UI_GET_ITEM(gxml, "label_book");
	navbar_versekey.label_chapter_menu =
	    UI_GET_ITEM(gxml, "label_chapter");
	navbar_versekey.label_verse_menu =
	    UI_GET_ITEM(gxml, "label_verse");
#if GTK_CHECK_VERSION(3, 4, 0)
	eventbox = UI_GET_ITEM(gxml, "eventbox_book");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_book_scroll_event), NULL);

	eventbox = UI_GET_ITEM(gxml, "eventbox_chapter");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_chapter_scroll_event),
			 NULL);

	eventbox = UI_GET_ITEM(gxml, "eventbox_verse");
	g_signal_connect((gpointer)eventbox, "scroll_event",
			 G_CALLBACK(on_button_verse_menu_verse_scroll_event), NULL);
#endif
	{
		GtkWidget *caja;
		GtkWidget *flecha;

		widgets.combo_bible_version = gtk_menu_button_new();
		gtk_container_remove(
		    GTK_CONTAINER(widgets.combo_bible_version),
		    gtk_bin_get_child(GTK_BIN(widgets.combo_bible_version)));

		UI_HBOX(caja, FALSE, 6);
		version_etiqueta = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(version_etiqueta), 0.0);
		/* Las descripciones son largas —"Biblia Platense
		 * (Straubinger)"— y esta barra ya va justa: el botón se topa
		 * y el nombre entero queda en el menú y en el tooltip. */
		gtk_label_set_ellipsize(GTK_LABEL(version_etiqueta),
					PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(version_etiqueta), 16);
		flecha = gtk_image_new_from_icon_name("pan-down-symbolic",
						      GTK_ICON_SIZE_BUTTON);
		gtk_box_pack_start(GTK_BOX(caja), version_etiqueta, TRUE, TRUE,
				   0);
		gtk_box_pack_start(GTK_BOX(caja), flecha, FALSE, FALSE, 0);
		gtk_container_add(GTK_CONTAINER(widgets.combo_bible_version),
				  caja);
		gtk_widget_show_all(caja);
	}
	gtk_widget_set_tooltip_text(widgets.combo_bible_version,
				    _("Cambiar de versión (mantiene el versículo enfocado)"));
	gtk_widget_set_valign(widgets.combo_bible_version, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(widgets.combo_bible_version, 150, -1);
	if (sword_disponible)
		gui_navbar_fill_version_combo();
	gtk_widget_show(widgets.combo_bible_version);
	gtk_box_pack_start(GTK_BOX(navbar_versekey.navbar),
			   widgets.combo_bible_version, FALSE, FALSE, 4);
	/* al frente del todo, antes que los íconos de historial, para que
	 * sea lo primero visible de la barra en vez de quedar al final
	 * (donde se lo pidieron mover porque ahí pasaba desapercibido). */
	gtk_box_reorder_child(GTK_BOX(navbar_versekey.navbar),
			      widgets.combo_bible_version, 0);

	_connect_signals(navbar_versekey);

	return navbar_versekey.navbar;
}
