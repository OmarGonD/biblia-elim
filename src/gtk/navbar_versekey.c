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
#include "gui/widget_helpers.h"
#include <glib/gi18n.h>

#include "editor/editor.h"

#include "gui/navbar_versekey.h"
#include "gui/bibletext.h"
#include "gui/bibletext_dialog.h"
#include "gui/tabbed_browser.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/lists.h"
#include "main/module_dialogs.h"
#include "main/navbar_versekey.h"
#include "main/interlineal.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/tab_history.h"
#include "main/url.hh"

#include "gui/debug_glib_null.h"
#include "navbar_entry_reference.h"

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

	gui_toggle_set_active(GTK_WIDGET(menu_button),
				     FALSE);
}

/******************************************************************************
 * Name
 *   select_button_press_callback
 *
 * Synopsis
 *   #include "gui/navbar_versekey.h"
 *
 *   gboolean select_button_press_callback (GtkWidget *widget,
 *			      GuiButtonEvent *event,
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
					     GuiButtonEvent *event,
					     gpointer user_data)
{
	if (event->type != GDK_BUTTON_PRESS || event->button != 1)
		return FALSE;
	/* GTK4-PORT-101 step 2: a popover from the history GMenu; the
	 * «historial» actions live on the button (see navbar creation). */
	GMenuModel *model = main_tab_history_menu_model(cur_passage_tab);
	GtkWidget *popover = gtk_popover_menu_new_from_model(model);
	gtk_widget_set_parent(popover, widget);
	g_object_unref(model);
	gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
	g_signal_connect(popover, "closed",
			 G_CALLBACK(menu_deactivate_callback), widget);
	gui_popover_destroy_on_close(popover);
	gtk_widget_grab_focus(widget);
	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
	gtk_popover_popup(GTK_POPOVER(popover));
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
 *			      GuiButtonEvent *event,
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
						  GuiButtonEvent *event,
						  gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
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
 *			      GuiButtonEvent *event,
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
						     GuiButtonEvent *
							 event,
						     gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
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
 *			      GuiButtonEvent *event,
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
						   GuiButtonEvent *event,
						   gpointer user_data)
{
	if ((event->type != GDK_BUTTON_PRESS) || (event->button != 1))
		return FALSE;

	gui_toggle_set_active(GTK_WIDGET(widget), TRUE);
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

/* Everything after "this reference exists": normalize the key, update the
 * navbar and display it through the sword:// handler. Shared by the typed
 * entry and the verse arrows, which reach here with a slot the backend
 * already produced. FALSE if the key does not normalize. */
static gboolean navbar_versekey_go_to(const gchar *key, const gchar *anchor)
{
	gchar *gkey =
	    main_get_valid_key(settings.MainWindowModule, key);

	// we got a valid key. but was it really a valid key within v11n?
	// for future use in determining whether to show normal navbar content.
	navbar_versekey.valid_key =
	    main_is_Bible_key(settings.MainWindowModule, gkey);

	if (gkey == NULL)
		return FALSE;

	gchar *url = g_strdup_printf("sword:///%s%s", gkey,
				     anchor ? anchor : "");

	navbar_versekey.module_name =
	    g_string_assign(navbar_versekey.module_name,
			    settings.MainWindowModule);
	main_navbar_versekey_set(navbar_versekey, gkey);
	main_url_handler(url, TRUE);
	g_free(url);
	g_free(gkey);
	return TRUE;
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *rawtext;
	gchar *gkey;
	const gchar *buf = gtk_editable_get_text(GTK_EDITABLE(entry));
	NavbarEntryReference reference;

	if (buf == NULL)
		return;
	reference = navbar_entry_reference_parse(buf);
	settings.special_anchor = reference.anchor;

	rawtext =
	    main_get_raw_text(navbar_versekey.module_name->str,
			      reference.key);

	if (!rawtext || (rawtext && (strlen(rawtext) < 2))) {
		gtk_editable_set_text(GTK_EDITABLE(entry), navbar_versekey.key->str);
		g_free(rawtext);
		settings.special_anchor = NULL;
		navbar_entry_reference_clear(&reference);
		return;
	}
	g_free(rawtext);
	if (!navbar_versekey_go_to(reference.key, reference.anchor))
		gtk_editable_set_text(GTK_EDITABLE(entry), navbar_versekey.key->str);
	settings.special_anchor = NULL;
	navbar_entry_reference_clear(&reference);
}

void gui_navbar_versekey_go_to(const gchar *key)
{
	settings.special_anchor = NULL;
	navbar_versekey_go_to(key, NULL);
}

/* A wheel step on the book, chapter or verse selector moves it by one. */
static gboolean on_button_verse_menu_verse_scroll_event(GtkWidget *widget,
							GuiScrollEvent *event,
							gpointer user_data)
{
	main_navbar_versekey_spin_verse(navbar_versekey, event->direction);
	return FALSE;
}

static gboolean on_button_verse_menu_chapter_scroll_event(GtkWidget *widget,
							  GuiScrollEvent *event,
							  gpointer user_data)
{
	main_navbar_versekey_spin_chapter(navbar_versekey, event->direction);
	return FALSE;
}

static gboolean on_button_verse_menu_book_scroll_event(GtkWidget *widget,
						       GuiScrollEvent *event,
						       gpointer user_data)
{
	main_navbar_versekey_spin_book(navbar_versekey, event->direction);
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
 *                                       	GuiButtonEvent * event,
 *                                       	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_up_eventbox_button_release_event(GtkWidget *widget,
						    GuiButtonEvent *event,
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
		gui_bibletext_reading_focus_flush();
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
 *                                      	GuiButtonEvent * event,
 *                                      	gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean on_down_eventbox_button_release_event(GtkWidget *widget,
						      GuiButtonEvent *event,
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
		gui_bibletext_reading_focus_flush();
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

/* Up/Down on the reference entry. The entry shows the verse the reader
 * is on, and that is where the keyboard often is when the app opens.
 * A single-line entry does not move its caret on those keys, so GTK
 * hands the key on and focus drops to whatever sits under the bar
 * (Comparar). The verse does not change until the next press. While
 * the entry still shows the current reference, the arrows move that
 * verse instead. A reference the reader has started to type is left
 * alone. */
static gboolean on_lookup_entry_key_press(GtkWidget *widget, GuiKeyEvent *event,
					  gpointer user_data)
{
	const gchar *text, *key;

	(void)user_data;
	if (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_ALT_MASK))
		return FALSE;
	if (event->keyval != GDK_KEY_Up && event->keyval != GDK_KEY_KP_Up &&
	    event->keyval != GDK_KEY_Down && event->keyval != GDK_KEY_KP_Down)
		return FALSE;
	if (widget != navbar_versekey.lookup_entry)
		return FALSE;
	text = gtk_editable_get_text(GTK_EDITABLE(widget));
	key = navbar_versekey.key ? navbar_versekey.key->str : NULL;
	if (!text || !key || !*key || strcmp(text, key) != 0)
		return FALSE;
	if (main_interlineal_bloquea_navegacion())
		return TRUE;
	if (event->keyval == GDK_KEY_Up || event->keyval == GDK_KEY_KP_Up)
		access_on_up_eventbox_button_release_event(VERSE_BUTTON);
	else
		access_on_down_eventbox_button_release_event(VERSE_BUTTON);
	return TRUE;
}

static void _connect_signals(NAVBAR_VERSEKEY navbar)
{

	gui_widget_on_key(navbar.lookup_entry, (GuiKeyFunc)on_lookup_entry_key_press, NULL, NULL);
	g_signal_connect((gpointer)navbar.lookup_entry,
			 "activate", G_CALLBACK(on_entry_activate), NULL);
	gui_widget_on_button(navbar.button_book_up, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_up_eventbox_button_release_event, GINT_TO_POINTER(BOOK_BUTTON));
	gui_widget_on_button(navbar.button_book_down, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_down_eventbox_button_release_event, GINT_TO_POINTER(BOOK_BUTTON));
	gui_widget_on_button(navbar.button_chapter_up, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_up_eventbox_button_release_event, GINT_TO_POINTER(CHAPTER_BUTTON));
	gui_widget_on_button(navbar.button_chapter_down, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_down_eventbox_button_release_event, GINT_TO_POINTER(CHAPTER_BUTTON));
	gui_widget_on_button(navbar.button_verse_up, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_up_eventbox_button_release_event, GINT_TO_POINTER(VERSE_BUTTON));
	gui_widget_on_button(navbar.button_verse_down, GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_down_eventbox_button_release_event, GINT_TO_POINTER(VERSE_BUTTON));

	g_signal_connect((gpointer)navbar.button_history_back,
			 "clicked",
			 G_CALLBACK(on_button_history_back_clicked), NULL);
	g_signal_connect((gpointer)navbar.button_history_next, "clicked",
			 G_CALLBACK(on_button_history_next_clicked), NULL);
	main_tab_history_install_actions(navbar.button_history_menu);
	/* before the toggle button's own click */
	gui_widget_on_button(navbar.button_history_menu, GTK_PHASE_CAPTURE, (GuiButtonFunc)select_button_press_callback, NULL, NULL);
	gui_widget_on_button(navbar.button_book_menu, GTK_PHASE_CAPTURE, (GuiButtonFunc)select_book_button_press_callback, NULL, NULL);
	gui_widget_on_button(navbar.button_chapter_menu, GTK_PHASE_CAPTURE, (GuiButtonFunc)select_chapter_button_press_callback, NULL, NULL);
	gui_widget_on_button(navbar.button_verse_menu, GTK_PHASE_CAPTURE, (GuiButtonFunc)select_verse_button_press_callback, NULL, NULL);
}

/* Picker de versión bíblica en la barra de navegación: cambia el
 * módulo de la ventana principal preservando el pasaje enfocado. La
 * referencia se convierte a la versificación del módulo nuevo (SpaRV
 * Salmos 119:1 es TorresAmat Salmos 118:1), así que los números de la
 * barra pueden cambiar.
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
static GMenu *version_menu = NULL;
static GSimpleAction *version_action = NULL;
static GHashTable *version_labels = NULL;

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
on_version_elegida(GSimpleAction *action, GVariant *state, gpointer datos)
{
	const char *mod = g_variant_get_string(state, NULL);

	(void)datos;
	g_simple_action_set_state(action, state);
	if (!mod || (settings.MainWindowModule &&
		     !strcmp(mod, settings.MainWindowModule)))
		return;
	main_display_bible_from_module(settings.MainWindowModule,
				       settings.currentverse, mod);
}

/* El botón enseña la versión puesta; el nombre completo, en el tooltip,
 * porque a 150 px no cabe entero. */
static void
version_actualizar_boton(void)
{
	const gchar *texto;

	if (!version_etiqueta || !version_labels || !settings.MainWindowModule)
		return;
	texto = g_hash_table_lookup(version_labels, settings.MainWindowModule);
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
	GList *l, *d, *entradas = NULL, *n;
	gchar *idioma_actual = NULL;
	const gchar *primera = NULL;
	GMenu *grupo = NULL;

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

	if (version_menu)
		g_object_unref(version_menu);
	version_menu = g_menu_new();
	if (version_labels)
		g_hash_table_destroy(version_labels);
	version_labels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	for (n = entradas; n; n = n->next) {
		EntradaVersion *e = n->data;

		if (g_strcmp0(idioma_actual, e->idioma)) {
			if (grupo) {
				g_menu_append_section(version_menu, idioma_actual,
						      G_MENU_MODEL(grupo));
				g_object_unref(grupo);
			}
			g_free(idioma_actual);
			idioma_actual = g_strdup(e->idioma);
			grupo = g_menu_new();
		}
		GMenuItem *item = g_menu_item_new(e->desc, NULL);
		g_menu_item_set_action_and_target(item, "version.elegir", "s", e->id);
		g_menu_append_item(grupo, item);
		g_object_unref(item);
		g_hash_table_insert(version_labels, g_strdup(e->id), g_strdup(e->desc));
		if (!primera)
			primera = e->id;
	}
	if (grupo) {
		g_menu_append_section(version_menu, idioma_actual, G_MENU_MODEL(grupo));
		g_object_unref(grupo);
	}
	/* Si el módulo guardado ya no está instalado, se marca el primero
	 * para no dejar el menú sin señalar. No se cambia de módulo desde
	 * aquí: esto corre al arrancar, antes de que la ventana esté lista,
	 * y de un módulo que falta ya se ocupa settings.c. */
	const gchar *elegida = settings.MainWindowModule &&
		g_hash_table_contains(version_labels, settings.MainWindowModule)
				 ? settings.MainWindowModule : primera;
	GSimpleActionGroup *actions = g_simple_action_group_new();
	version_action = g_simple_action_new_stateful(
	    "elegir", G_VARIANT_TYPE_STRING,
	    g_variant_new_string(elegida ? elegida : ""));
	g_signal_connect(version_action, "change-state",
			 G_CALLBACK(on_version_elegida), NULL);
	g_action_map_add_action(G_ACTION_MAP(actions), G_ACTION(version_action));
	gui_widget_insert_action_group(widgets.combo_bible_version, "version",
				       G_ACTION_GROUP(actions));
	g_object_unref(actions);

	g_free(idioma_actual);
	g_list_free_full(entradas, version_liberar);

	gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(widgets.combo_bible_version),
				       G_MENU_MODEL(version_menu));
	version_actualizar_boton();
}

void
gui_navbar_version_combo_sync(void)
{
	if (!version_action || !settings.MainWindowModule)
		return;
	g_simple_action_set_state(version_action,
				  g_variant_new_string(settings.MainWindowModule));
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
	GtkWidget *eventbox;

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
	fprintf(stderr, "CKPT navbar=%p hist_back=%p hist_next=%p hist_menu=%p\n",
		(void*)navbar_versekey.navbar, (void*)navbar_versekey.button_history_back,
		(void*)navbar_versekey.button_history_next, (void*)navbar_versekey.button_history_menu);
	fflush(stderr);
	fprintf(stderr, "CKPT book_up=%p book_down=%p chap_up=%p chap_down=%p verse_up=%p verse_down=%p\n",
		(void*)navbar_versekey.button_book_up, (void*)navbar_versekey.button_book_down,
		(void*)navbar_versekey.button_chapter_up, (void*)navbar_versekey.button_chapter_down,
		(void*)navbar_versekey.button_verse_up, (void*)navbar_versekey.button_verse_down);
	fflush(stderr);
	fprintf(stderr, "CKPT book_menu=%p chap_menu=%p verse_menu=%p lookup=%p\n",
		(void*)navbar_versekey.button_book_menu, (void*)navbar_versekey.button_chapter_menu,
		(void*)navbar_versekey.button_verse_menu, (void*)navbar_versekey.lookup_entry);
	fflush(stderr);
	eventbox = UI_GET_ITEM(gxml, "eventbox_book");
	fprintf(stderr, "CKPT eventbox_book=%p\n", (void*)eventbox); fflush(stderr);
	gui_widget_on_scroll(eventbox, (GuiScrollFunc)on_button_verse_menu_book_scroll_event, NULL);
	eventbox = UI_GET_ITEM(gxml, "eventbox_chapter");
	fprintf(stderr, "CKPT eventbox_chapter=%p\n", (void*)eventbox); fflush(stderr);
	gui_widget_on_scroll(eventbox, (GuiScrollFunc)on_button_verse_menu_chapter_scroll_event, NULL);
	eventbox = UI_GET_ITEM(gxml, "eventbox_verse");
	fprintf(stderr, "CKPT eventbox_verse=%p\n", (void*)eventbox); fflush(stderr);
	gui_widget_on_scroll(eventbox, (GuiScrollFunc)on_button_verse_menu_verse_scroll_event, NULL);
	{
		GtkWidget *caja;
		GtkWidget *flecha;

		widgets.combo_bible_version = gtk_menu_button_new();

		UI_HBOX(caja, FALSE, 6);
		version_etiqueta = gtk_label_new("");
		gtk_label_set_xalign(GTK_LABEL(version_etiqueta), 0.0);
		/* Las descripciones son largas —"Biblia Platense
		 * (Straubinger)"— y esta barra ya va justa: el botón se topa
		 * y el nombre entero queda en el menú y en el tooltip. */
		gtk_label_set_ellipsize(GTK_LABEL(version_etiqueta),
					PANGO_ELLIPSIZE_END);
		gtk_label_set_max_width_chars(GTK_LABEL(version_etiqueta), 16);
		flecha = gtk_image_new_from_icon_name("pan-down-symbolic");
		gui_box_pack(GTK_BOX(caja), version_etiqueta, TRUE, TRUE, 0);
		gtk_box_append(GTK_BOX(caja), flecha);
		gtk_menu_button_set_child(GTK_MENU_BUTTON(widgets.combo_bible_version),
					  caja);
	}
	gtk_widget_set_tooltip_text(widgets.combo_bible_version,
				    _("Cambiar de versión (mantiene el versículo enfocado)"));
	gtk_widget_set_valign(widgets.combo_bible_version, GTK_ALIGN_CENTER);
	gtk_widget_set_size_request(widgets.combo_bible_version, 150, -1);
	if (sword_disponible)
		gui_navbar_fill_version_combo();
	gtk_widget_show(widgets.combo_bible_version);
	gui_box_pack(GTK_BOX(navbar_versekey.navbar), widgets.combo_bible_version, FALSE, FALSE, 4);
	/* al frente del todo, antes que los íconos de historial, para que
	 * sea lo primero visible de la barra en vez de quedar al final
	 * (donde se lo pidieron mover porque ahí pasaba desapercibido). */
	gtk_box_reorder_child_after(GTK_BOX(navbar_versekey.navbar),
				    widgets.combo_bible_version, NULL);

	_connect_signals(navbar_versekey);

	return navbar_versekey.navbar;
}
