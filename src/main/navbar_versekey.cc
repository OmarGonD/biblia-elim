/*
 * Xiphos Bible Study Tool
 * navbar_versekey.cc - glue between all navbar_versekey and sword
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
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <swmodule.h>
#include <versekey.h>

#include "main/module_dialogs.h"
#include "main/navbar_versekey.h"
#include "main/interlineal.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/xml.h"

#include "gui/navbar_versekey.h"
#include "gui/tabbed_browser.h"

#ifdef USE_WEBKIT_EDITOR
#include "editor/webkit_editor.h"
#else
#include "editor/slib-editor.h"
#endif

#include "backend/sword_main.hh"

#include "gui/debug_glib_null.h"

extern gboolean do_display;
extern gboolean do_display_dict;

static DIALOG_DATA *c_dialog;
static EDITOR *c_editor;
static gint c_type;

/******************************************************************************
 * Name
 *   main_get_valid_key
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   const char *main_get_valid_key(const char *module, const char * key)
 *
 * Description
 *   get a valid versekey from the backend
 *
 * Return value
 *   void
 */

const char *main_get_valid_key(const char *module_name, const char *key)
{
	return backend->get_valid_key(module_name, key);
}

/******************************************************************************
 * Name
 *  main_navbar_versekey_spin_book
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_navbar_versekey_spin_book(NAVBAR_VERSEKEY navbar, int direction)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static gboolean
navbar_main_locked(NAVBAR_VERSEKEY navbar)
{
	return navbar.lookup_entry == navbar_versekey.lookup_entry &&
	       main_interlineal_bloquea_navegacion();
}

void main_navbar_versekey_spin_book(NAVBAR_VERSEKEY navbar, int direction)
{
	char *tmpkey = NULL;
	int book;

	if (navbar_main_locked(navbar))
		return;

	if (!navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	VerseKey *vkey = (VerseKey *)mod->createKey();
	tmpkey = backend->get_valid_key(navbar.module_name->str, navbar.key->str);

	vkey->setAutoNormalize(1);
	vkey->setText(tmpkey);

	book = vkey->getBook() + (direction ? 1 : -1);
	vkey->setBook(book);

	tmpkey = g_strdup_printf("%s 1:1", vkey->getBookName());
	gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry), tmpkey);
	gtk_widget_activate(navbar.lookup_entry);
	g_free(tmpkey);
	delete vkey;
}

/******************************************************************************
 * Name
 *  main_navbar_versekey_spin_chapter
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_navbar_versekey_spin_chapter(NAVBAR_VERSEKEY navbar, int direction)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void main_navbar_versekey_spin_chapter(NAVBAR_VERSEKEY navbar, int direction)
{
	char *tmpkey = NULL;
	int chapter;

	if (navbar_main_locked(navbar))
		return;

	if (!navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	VerseKey *vkey = (VerseKey *)mod->createKey();
	tmpkey = backend->get_valid_key(navbar.module_name->str, navbar.key->str);

	vkey->setAutoNormalize(1);
	vkey->setText(tmpkey);

	chapter = vkey->getChapter() + (direction ? 1 : -1);
	vkey->setChapter(chapter);

	tmpkey = g_strdup_printf("%s %d:1", vkey->getBookName(), vkey->getChapter());
	gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry), tmpkey);
	gtk_widget_activate(navbar.lookup_entry);
	g_free(tmpkey);
	delete vkey;
}

/******************************************************************************
 * Name
 *  main_navbar_versekey_spin_verse
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   char * main_navbar_versekey_spin_verse(NAVBAR_VERSEKEY navbar, int direction)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void main_navbar_versekey_spin_verse(NAVBAR_VERSEKEY navbar, int direction)
{

	char *tmpkey = NULL;
	int verse;

	if (navbar_main_locked(navbar))
		return;

	if (!navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	VerseKey *vkey = (VerseKey *)mod->createKey();
	tmpkey = backend->get_valid_key(navbar.module_name->str, navbar.key->str);

	vkey->setAutoNormalize(1);
	vkey->setText(tmpkey);

	verse = vkey->getVerse() + (direction ? 1 : -1);
	vkey->setVerse(verse);

	tmpkey = g_strdup_printf("%s %d:%d", vkey->getBookName(),
				 vkey->getChapter(), vkey->getVerse());
	gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry), tmpkey);
	gtk_widget_activate(navbar.lookup_entry);
	g_free(tmpkey);
	delete vkey;
}

/******************************************************************************
 * Name
 *  on_nt_book_menu_select
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void on_nt_book_menu_select(GtkMenuItem * menuitem, gpointer user_data)
 *
 * Description
 *   user selected new new testament book from dropdown menu - change verse key
 *   to new book set lookup_entry text to new verse key and activate entry
 *
 * Return value
 *   void
 */

static void on_nt_book_menu_select(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *entry = NULL;
	int book = GPOINTER_TO_INT(user_data);
	gchar *name, *key;

	switch (c_type) {
	case NB_MAIN:
		name = navbar_versekey.module_name->str;
		key = navbar_versekey.key->str;
		entry = navbar_versekey.lookup_entry;
		break;

	case NB_PARALLEL:
		name = navbar_parallel.module_name->str;
		key = navbar_parallel.key->str;
		entry = navbar_parallel.lookup_entry;
		break;

	case NB_DIALOG:
		if (!c_dialog)
			return;
		name = c_dialog->navbar.module_name->str;
		key = c_dialog->navbar.key->str;
		entry = c_dialog->navbar.lookup_entry;
		break;

	case NB_EDITOR:
		if (!c_editor)
			return;
		name = c_editor->navbar.module_name->str;
		key = c_editor->navbar.key->str;
		entry = c_editor->navbar.lookup_entry;
		break;
	}
	if (c_type == NB_MAIN && main_interlineal_bloquea_navegacion())
		return;

	if (entry) {
		SWModule *mod = backend->get_SWModule(name);
		if (mod) {
			VerseKey *vkey = (VerseKey *)mod->createKey();
			vkey->setAutoNormalize(1);
			vkey->setText(key);
			vkey->setTestament(2);
			vkey->setBook(book + 1);

			gtk_entry_set_text(GTK_ENTRY(entry), vkey->getText());
			gtk_widget_activate(entry);

			delete vkey;
		}
	}
}

/******************************************************************************
 * Name
 *  on_ot_book_menu_select
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void on_ot_book_menu_select(GtkMenuItem * menuitem, gpointer user_data)
 *
 * Description
 *   user selected new old testament book from dropdown menu - change verse key
 *   to new book set lookup_entry text to new verse key and activate entry
 *
 * Return value
 *   void
 */

static void on_ot_book_menu_select(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *entry = NULL;
	int book = GPOINTER_TO_INT(user_data);
	gchar *name, *key;

	switch (c_type) {
	case NB_MAIN:
		name = navbar_versekey.module_name->str;
		key = navbar_versekey.key->str;
		entry = navbar_versekey.lookup_entry;
		break;

	case NB_PARALLEL:
		name = navbar_parallel.module_name->str;
		key = navbar_parallel.key->str;
		entry = navbar_parallel.lookup_entry;
		break;

	case NB_DIALOG:
		if (!c_dialog)
			return;
		name = c_dialog->navbar.module_name->str;
		key = c_dialog->navbar.key->str;
		entry = c_dialog->navbar.lookup_entry;
		break;

	case NB_EDITOR:
		if (!c_editor)
			return;
		name = c_editor->navbar.module_name->str;
		key = c_editor->navbar.key->str;
		entry = c_editor->navbar.lookup_entry;
		break;
	}
	if (c_type == NB_MAIN && main_interlineal_bloquea_navegacion())
		return;

	if (entry) {
		SWModule *mod = backend->get_SWModule(name);
		if (mod) {
			VerseKey *vkey = (VerseKey *)mod->createKey();
			vkey->setAutoNormalize(1);
			vkey->setText(key);
			vkey->setTestament(1);
			vkey->setBook(book + 1);

			gtk_entry_set_text(GTK_ENTRY(entry), vkey->getText());
			gtk_widget_activate(entry);

			delete vkey;
		}
	}
}

/******************************************************************************
 * Name
 *  on_chapter_menu_select
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void on_chapter_menu_select(GtkMenuItem * menuitem, gpointer user_data)
 *
 * Description
 *   user selected new chapter from dropdown menu - change verse key to new chapter
 *   set lookup_entry text to new verse key and activate entry
 *
 * Return value
 *   void
 */

static void on_chapter_menu_select(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *entry = NULL;
	int chapter = GPOINTER_TO_INT(user_data);
	gchar *name, *key;

	switch (c_type) {
	case NB_MAIN:
		name = navbar_versekey.module_name->str;
		key = navbar_versekey.key->str;
		entry = navbar_versekey.lookup_entry;
		break;

	case NB_PARALLEL:
		name = navbar_parallel.module_name->str;
		key = navbar_parallel.key->str;
		entry = navbar_parallel.lookup_entry;
		break;

	case NB_DIALOG:
		if (!c_dialog)
			return;
		name = c_dialog->navbar.module_name->str;
		key = c_dialog->navbar.key->str;
		entry = c_dialog->navbar.lookup_entry;
		break;

	case NB_EDITOR:
		if (!c_editor)
			return;
		name = c_editor->navbar.module_name->str;
		key = c_editor->navbar.key->str;
		entry = c_editor->navbar.lookup_entry;
		break;
	}
	if (c_type == NB_MAIN && main_interlineal_bloquea_navegacion())
		return;
	if (entry) {
		SWModule *mod = backend->get_SWModule(name);
		if (mod) {
			VerseKey *vkey = (VerseKey *)mod->createKey();
			vkey->setAutoNormalize(1);
			vkey->setText(key);
			vkey->setChapter(chapter);

			gtk_entry_set_text(GTK_ENTRY(entry), vkey->getText());
			gtk_widget_activate(entry);

			delete vkey;
		}
	}
}

/******************************************************************************
 * Name
 *  on_verse_menu_select
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void on_verse_menu_select(GtkMenuItem * menuitem, gpointer user_data)
 *
 * Description
 *   user selected new verse from dropdown menu - change verse key to new verse
 *   set lookup_entry text to new verse key and activate entry
 *
 * Return value
 *   void
 */

static void on_verse_menu_select(GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *entry = NULL;
	int verse = GPOINTER_TO_INT(user_data);
	gchar *name, *key;

	switch (c_type) {
	case NB_MAIN:
		name = navbar_versekey.module_name->str;
		key = navbar_versekey.key->str;
		entry = navbar_versekey.lookup_entry;
		break;

	case NB_PARALLEL:
		name = navbar_parallel.module_name->str;
		key = navbar_parallel.key->str;
		entry = navbar_parallel.lookup_entry;
		break;

	case NB_DIALOG:
		if (!c_dialog)
			return;
		name = c_dialog->navbar.module_name->str;
		key = c_dialog->navbar.key->str;
		entry = c_dialog->navbar.lookup_entry;
		break;

	case NB_EDITOR:
		if (!c_editor)
			return;
		name = c_editor->navbar.module_name->str;
		key = c_editor->navbar.key->str;
		entry = c_editor->navbar.lookup_entry;
		break;
	}
	if (c_type == NB_MAIN && main_interlineal_bloquea_navegacion())
		return;
	if (entry) {
		SWModule *mod = backend->get_SWModule(name);
		if (mod) {
			VerseKey *vkey = (VerseKey *)mod->createKey();
			vkey->setAutoNormalize(1);
			vkey->setText(key);
			vkey->setVerse(verse);

			gtk_entry_set_text(GTK_ENTRY(entry), vkey->getText());
			gtk_widget_activate(entry);

			delete vkey;
		}
	}
}

/******************************************************************************
 * Name
 *  main_navbar_versekey_set
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_navbar_versekey_set(NAVBAR_VERSEKEY navbar, const char * key)
 *
 * Description
 *   separate key info John 3:16 to book=john chapter=3 verse=16 and write
 *   to navbar labels - also set lookup entry text
 *
 * Return value
 *   void
 */

void main_navbar_versekey_set(NAVBAR_VERSEKEY navbar, const char *key)
{
	char *tmpbuf = NULL;

	if (!navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	// previously, we set and normalized the key, but we also
	// kept a record of whether that key made sense.
	if (navbar.valid_key) {
		VerseKey *vkey = (VerseKey *)mod->createKey();
		vkey->setAutoNormalize(1);
		vkey->setText(key);

		tmpbuf = g_strdup_printf("<b>%s</b>", vkey->getBookName());
		gtk_label_set_label(GTK_LABEL(navbar.label_book_menu), tmpbuf);
		g_free(tmpbuf);

		gchar *num = main_format_number(vkey->getChapter());
		tmpbuf = g_strdup_printf("<b>%s</b>", num);
		g_free(num);
		gtk_label_set_label(GTK_LABEL(navbar.label_chapter_menu), tmpbuf);
		g_free(tmpbuf);

		num = main_format_number(vkey->getVerse());
		tmpbuf = g_strdup_printf("<b>%s</b>", num);
		g_free(num);
		gtk_label_set_label(GTK_LABEL(navbar.label_verse_menu), tmpbuf);
		g_free(tmpbuf);

		navbar.key = g_string_assign(navbar.key, (char *)vkey->getText());
		gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry), navbar.key->str);

		delete vkey;
	} else {
		tmpbuf = g_strdup(" ");
		gtk_label_set_label(GTK_LABEL(navbar.label_book_menu), tmpbuf);
		gtk_label_set_label(GTK_LABEL(navbar.label_chapter_menu), tmpbuf);
		gtk_label_set_label(GTK_LABEL(navbar.label_verse_menu), tmpbuf);
		gtk_entry_set_text(GTK_ENTRY(navbar.lookup_entry), tmpbuf);
		g_free(tmpbuf);
	}
}

/******************************************************************************
 * Name
 *  selector numérico de capítulo y versículo
 *
 * Description
 *   Esto era una GtkMenu con la rejilla de números y nada más: para ir a
 *   Salmos 119 había que recorrer 150 casillas con la vista. Ahora el
 *   desplegable abre con el foco en una entrada -- se escribe el número y
 *   Enter navega -- y conserva la rejilla debajo para quien prefiera
 *   señalar. Lo tecleado se resalta en la rejilla y se desplaza hasta
 *   quedar a la vista, así que las dos formas de elegir van juntas.
 */

#define NUMPICKER_COLS 10

typedef struct {
	GtkWidget *popover;
	GtkWidget *entry;
	GtkWidget *scroll;
	GtkWidget *grid;
	GtkWidget **button; /* button[n - 1] es el número n */
	gint max;
	gint current;
	gint marked;	  /* número resaltado por lo tecleado; 0 = ninguno */
	gboolean placed;  /* ya centramos la rejilla en el número actual */
	gboolean verse;
} NUMPICKER;

/* los botones de un GtkGrid heredan el relleno generoso del tema y una
 * rejilla de diez columnas se iba a un tercio de la pantalla: aquí basta
 * con que el número quepa holgado. */
static void picker_add_style(GtkWidget *popover)
{
	static GtkCssProvider *css = NULL;

	if (!css) {
		css = gtk_css_provider_new();
		gtk_css_provider_load_from_data(
		    css,
		    ".elim-picker button {\n"
		    "  padding: 1px 4px;\n"
		    "  min-width: 26px;\n"
		    "  min-height: 22px;\n"
		    "}\n",
		    -1, NULL);
		gtk_style_context_add_provider_for_screen(
		    gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
		    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	gtk_style_context_add_class(gtk_widget_get_style_context(popover),
				    "elim-picker");
}

static void numpicker_free(gpointer data)
{
	NUMPICKER *p = (NUMPICKER *)data;

	g_free(p->button);
	g_free(p);
}

/* centrar en su marco un hijo del contenido desplazable: ni la rejilla de
 * Salmos (quince filas) ni la lista de libros caben enteras, y tanto lo
 * que hay seleccionado como lo que se teclea suelen quedar fuera. */
static void picker_scroll_to(GtkWidget *scroll, GtkWidget *content,
			     GtkWidget *child)
{
	GtkAdjustment *adj;
	gint x, y;
	gdouble page, top, last;

	if (!child ||
	    !gtk_widget_translate_coordinates(child, content, 0, 0, &x, &y))
		return;
	adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
	page = gtk_adjustment_get_page_size(adj);
	last = MAX(gtk_adjustment_get_lower(adj),
		   gtk_adjustment_get_upper(adj) - page);
	top = y + (gtk_widget_get_allocated_height(child) / 2.0) - (page / 2.0);
	gtk_adjustment_set_value(adj, CLAMP(top, gtk_adjustment_get_lower(adj),
					    last));
}

static void numpicker_scroll_to(NUMPICKER *p, gint n)
{
	if (n < 1 || n > p->max)
		return;
	picker_scroll_to(p->scroll, p->grid, p->button[n - 1]);
}

static void numpicker_grid_allocated(GtkWidget *widget,
				     GdkRectangle *allocation,
				     gpointer data)
{
	NUMPICKER *p = (NUMPICKER *)data;

	/* la primera asignación de tamaño es la primera vez que se puede
	 * calcular el desplazamiento; antes de eso todo mide cero. */
	if (p->placed || allocation->height <= 1)
		return;
	p->placed = TRUE;
	numpicker_scroll_to(p, p->current);
}

/* marca (como si el ratón estuviera encima) el número que se va tecleando,
 * para que se vea en la rejilla adónde lleva lo escrito. */
static void numpicker_mark_typed(NUMPICKER *p, gint n)
{
	if (n == p->marked)
		return;
	if (p->marked)
		gtk_widget_unset_state_flags(p->button[p->marked - 1],
					     GTK_STATE_FLAG_PRELIGHT);
	p->marked = ((n >= 1) && (n <= p->max)) ? n : 0;
	if (p->marked)
		gtk_widget_set_state_flags(p->button[p->marked - 1],
					   GTK_STATE_FLAG_PRELIGHT, FALSE);
}

static gboolean picker_destroy_idle(gpointer popover)
{
	gtk_widget_destroy(GTK_WIDGET(popover));
	return G_SOURCE_REMOVE;
}

static void picker_closed(GtkPopover *popover, gpointer anchor)
{
	if (g_object_get_data(G_OBJECT(popover), "elim-cerrado"))
		return;
	g_object_set_data(G_OBJECT(popover), "elim-cerrado",
			  GINT_TO_POINTER(1));

	if (anchor && GTK_IS_TOGGLE_BUTTON(anchor))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(anchor), FALSE);

	/* destruir dentro del propio "closed" es destruir el widget que está
	 * emitiendo la señal: lo dejamos para el idle siguiente, al que le
	 * cedemos la referencia que tomamos al crearlo (así sigue en pie
	 * aunque el ancla se lo lleve antes de que llegue el idle). */
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, picker_destroy_idle,
			popover, g_object_unref);
}

static void picker_popdown(GtkWidget *popover)
{
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_popover_popdown(GTK_POPOVER(popover));
#else
	gtk_widget_hide(popover);
#endif
}

static void numpicker_go(NUMPICKER *p, gint n)
{
	gboolean verse = p->verse;

	if (n < 1 || n > p->max)
		return;
	picker_popdown(p->popover);
	if (verse)
		on_verse_menu_select(NULL, GINT_TO_POINTER(n));
	else
		on_chapter_menu_select(NULL, GINT_TO_POINTER(n));
}

/* 0 = entrada vacía, -1 = escrito pero fuera de rango, >0 = número válido */
static gint numpicker_typed(NUMPICKER *p)
{
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(p->entry));
	gchar *end;
	gint64 n;

	if (!text || !*text)
		return 0;
	n = g_ascii_strtoll(text, &end, 10);
	if (*end || (n < 1) || (n > p->max))
		return -1;
	return (gint)n;
}

static void numpicker_entry_changed(GtkEditable *editable, gpointer data)
{
	NUMPICKER *p = (NUMPICKER *)data;
	GtkStyleContext *ctx = gtk_widget_get_style_context(p->entry);
	gint n = numpicker_typed(p);

	if (n < 0)
		gtk_style_context_add_class(ctx, GTK_STYLE_CLASS_ERROR);
	else
		gtk_style_context_remove_class(ctx, GTK_STYLE_CLASS_ERROR);
	numpicker_mark_typed(p, (n > 0) ? n : 0);
	if (n > 0)
		numpicker_scroll_to(p, n);
}

static void numpicker_entry_activate(GtkEntry *entry, gpointer data)
{
	NUMPICKER *p = (NUMPICKER *)data;
	gint n = numpicker_typed(p);

	if (n > 0)
		numpicker_go(p, n);
	else
		gtk_widget_error_bell(GTK_WIDGET(entry));
}

static void numpicker_entry_insert_text(GtkEditable *editable,
					const gchar *text,
					gint length,
					gint *position,
					gpointer data)
{
	gint i;

	for (i = 0; text[i] && ((length < 0) || (i < length)); i++) {
		if (g_ascii_isdigit(text[i]))
			continue;
		g_signal_stop_emission_by_name(editable, "insert-text");
		gtk_widget_error_bell(GTK_WIDGET(editable));
		return;
	}
}

static void numpicker_button_clicked(GtkButton *button, gpointer data)
{
	NUMPICKER *p = (NUMPICKER *)data;

	numpicker_go(p, GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button),
							  "elim-numero")));
}

static NUMPICKER *numpicker_new(GtkWidget *anchor, gint max, gint current,
				gboolean verse)
{
	NUMPICKER *p = g_new0(NUMPICKER, 1);
	GtkWidget *box, *btn;
	gchar *text;
	gint i;

	p->max = max;
	p->current = CLAMP(current, 1, max);
	p->verse = verse;
	p->button = g_new0(GtkWidget *, max);

	p->popover = gtk_popover_new(anchor);
	g_object_ref_sink(p->popover);
	gtk_popover_set_position(GTK_POPOVER(p->popover), GTK_POS_BOTTOM);
	picker_add_style(p->popover);
	g_object_set_data_full(G_OBJECT(p->popover), "elim-numpicker", p,
			       numpicker_free);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);
	gtk_container_add(GTK_CONTAINER(p->popover), box);

	p->entry = gtk_entry_new();
	gtk_entry_set_input_purpose(GTK_ENTRY(p->entry),
				    GTK_INPUT_PURPOSE_DIGITS);
	gtk_entry_set_max_length(GTK_ENTRY(p->entry), 3);
	gtk_entry_set_width_chars(GTK_ENTRY(p->entry), 6);
	gtk_entry_set_alignment(GTK_ENTRY(p->entry), 0.5);
	text = g_strdup_printf(verse ? _("Versículo 1–%d") : _("Capítulo 1–%d"),
			       max);
	gtk_entry_set_placeholder_text(GTK_ENTRY(p->entry), text);
	g_free(text);
	gtk_widget_set_tooltip_text(p->entry,
				    _("Escribe el número y pulsa Enter"));
	gtk_box_pack_start(GTK_BOX(box), p->entry, FALSE, FALSE, 0);

	p->grid = gtk_grid_new();
	gtk_grid_set_row_homogeneous(GTK_GRID(p->grid), TRUE);
	gtk_grid_set_column_homogeneous(GTK_GRID(p->grid), TRUE);
	gtk_grid_set_row_spacing(GTK_GRID(p->grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(p->grid), 2);

	for (i = 1; i <= max; i++) {
		gchar *num = main_format_number(i);

		btn = gtk_button_new_with_label(num);
		g_free(num);
		gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
		g_object_set_data(G_OBJECT(btn), "elim-numero",
				  GINT_TO_POINTER(i));
		g_signal_connect(btn, "clicked",
				 G_CALLBACK(numpicker_button_clicked), p);
		gtk_grid_attach(GTK_GRID(p->grid), btn,
				(i - 1) % NUMPICKER_COLS,
				(i - 1) / NUMPICKER_COLS, 1, 1);
		p->button[i - 1] = btn;
	}
	/* dónde estamos ahora, con el color de acento del tema */
	gtk_style_context_add_class(
	    gtk_widget_get_style_context(p->button[p->current - 1]),
	    GTK_STYLE_CLASS_SUGGESTED_ACTION);

	p->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p->scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_scrolled_window_set_propagate_natural_height(
	    GTK_SCROLLED_WINDOW(p->scroll), TRUE);
	gtk_scrolled_window_set_max_content_height(
	    GTK_SCROLLED_WINDOW(p->scroll), 260);
#else
	gtk_widget_set_size_request(p->scroll, -1, 260);
#endif
	gtk_container_add(GTK_CONTAINER(p->scroll), p->grid);
	gtk_box_pack_start(GTK_BOX(box), p->scroll, TRUE, TRUE, 0);

	g_signal_connect(p->entry, "changed",
			 G_CALLBACK(numpicker_entry_changed), p);
	g_signal_connect(p->entry, "activate",
			 G_CALLBACK(numpicker_entry_activate), p);
	g_signal_connect(p->entry, "insert-text",
			 G_CALLBACK(numpicker_entry_insert_text), p);
	g_signal_connect_after(p->grid, "size-allocate",
			       G_CALLBACK(numpicker_grid_allocated), p);

	gtk_widget_show_all(box);
	return p;
}

static void numpicker_popup(NAVBAR_VERSEKEY navbar, gint nb_type,
			    gpointer dialog, gpointer editor,
			    GtkWidget *anchor, gboolean verse)
{
	NUMPICKER *p;
	gint max, current;

	c_dialog = (DIALOG_DATA *)dialog;
	c_editor = (EDITOR *)editor;
	c_type = nb_type;

	if (!anchor || !navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	VerseKey *vkey = (VerseKey *)mod->createKey();
	vkey->setAutoNormalize(1);
	vkey->setText(navbar.key->str);
	max = (verse ? vkey->getVerseMax() : vkey->getChapterMax());
	current = (verse ? vkey->getVerse() : vkey->getChapter());
	delete vkey;

	if (max < 1)
		return;

	p = numpicker_new(anchor, max, current, verse);
	g_signal_connect(p->popover, "closed", G_CALLBACK(picker_closed),
			 anchor);
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_popover_popup(GTK_POPOVER(p->popover));
#else
	gtk_widget_show(p->popover);
#endif
	gtk_widget_grab_focus(p->entry);
}

/******************************************************************************
 * Name
 *  main_versekey_popup_verse
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_versekey_popup_verse(NAVBAR_VERSEKEY navbar, gint nb_type,
 *				    gpointer dialog, gpointer editor,
 *				    GtkWidget *anchor)
 *
 * Description
 *   despliega bajo anchor el selector de versículo del capítulo actual
 *
 * Return value
 *   void
 */

void main_versekey_popup_verse(NAVBAR_VERSEKEY navbar, gint nb_type,
			       gpointer dialog, gpointer editor,
			       GtkWidget *anchor)
{
	numpicker_popup(navbar, nb_type, dialog, editor, anchor, TRUE);
}

/******************************************************************************
 * Name
 *  main_versekey_popup_chapter
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_versekey_popup_chapter(NAVBAR_VERSEKEY navbar, gint nb_type,
 *				      gpointer dialog, gpointer editor,
 *				      GtkWidget *anchor)
 *
 * Description
 *   despliega bajo anchor el selector de capítulo del libro actual
 *
 * Return value
 *   void
 */

void main_versekey_popup_chapter(NAVBAR_VERSEKEY navbar, gint nb_type,
				 gpointer dialog, gpointer editor,
				 GtkWidget *anchor)
{
	numpicker_popup(navbar, nb_type, dialog, editor, anchor, FALSE);
}

/******************************************************************************
 * Name
 *  selector de libro
 *
 * Description
 *   El menú de libros eran dos columnas de treinta y nueve y veintisiete
 *   nombres: para llegar a Habacuc había que recorrerlas con la vista.
 *   Ahora el desplegable abre con el foco en una entrada que filtra la
 *   lista según se escribe -- sin importar tildes ni mayúsculas, "hab"
 *   basta, y "1 co" encuentra 1 Corintios --, las flechas mueven la
 *   selección y Enter va al libro señalado.
 */

typedef struct {
	GtkWidget *popover;
	GtkWidget *entry;
	GtkWidget *list;
	GtkWidget *scroll;
	GtkWidget *current; /* fila del libro en que estamos */
	gchar *needle;
	gboolean placed; /* ya centramos la lista en el libro actual */
} BOOKPICKER;

static void bookpicker_free(gpointer data)
{
	BOOKPICKER *p = (BOOKPICKER *)data;

	g_free(p->needle);
	g_free(p);
}

/* g_str_match_string() compara por palabras y sin tildes ni mayúsculas,
 * que es justo lo que hace falta con "Éxodo" o "1 Corintios". */
static gboolean bookpicker_matches(GtkListBoxRow *row, BOOKPICKER *p)
{
	const gchar *name;

	if (!p->needle || !*p->needle)
		return TRUE;
	name = (const gchar *)g_object_get_data(G_OBJECT(row), "elim-libro");
	return name && g_str_match_string(p->needle, name, TRUE);
}

static gboolean bookpicker_filter(GtkListBoxRow *row, gpointer data)
{
	return bookpicker_matches(row, (BOOKPICKER *)data);
}

static void bookpicker_header(GtkListBoxRow *row, GtkListBoxRow *before,
			      gpointer data)
{
	gint testament = GPOINTER_TO_INT(
	    g_object_get_data(G_OBJECT(row), "elim-testamento"));
	GtkWidget *label;

	if (before &&
	    (testament == GPOINTER_TO_INT(g_object_get_data(G_OBJECT(before),
							    "elim-testamento")))) {
		gtk_list_box_row_set_header(row, NULL);
		return;
	}
	label = gtk_label_new((testament == 1) ? _("Antiguo Testamento")
					       : _("Nuevo Testamento"));
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(label, 8);
	gtk_widget_set_margin_top(label, 6);
	gtk_widget_set_margin_bottom(label, 2);
	gtk_style_context_add_class(gtk_widget_get_style_context(label),
				    GTK_STYLE_CLASS_DIM_LABEL);
	gtk_widget_show(label);
	gtk_list_box_row_set_header(row, label);
}

/* GTK no expone qué filas dejó fuera el filtro, así que lo volvemos a
 * aplicar nosotros para saber cuál encabeza la lista. */
static GtkListBoxRow *bookpicker_first_match(BOOKPICKER *p)
{
	GtkListBoxRow *hit = NULL;
	GList *rows, *l;

	rows = gtk_container_get_children(GTK_CONTAINER(p->list));
	for (l = rows; l && !hit; l = l->next)
		if (bookpicker_matches(GTK_LIST_BOX_ROW(l->data), p))
			hit = GTK_LIST_BOX_ROW(l->data);
	g_list_free(rows);
	return hit;
}

static void bookpicker_select(BOOKPICKER *p, GtkListBoxRow *row)
{
	if (!row)
		return;
	gtk_list_box_select_row(GTK_LIST_BOX(p->list), row);
	picker_scroll_to(p->scroll, p->list, GTK_WIDGET(row));
}

static void bookpicker_go(BOOKPICKER *p, GtkListBoxRow *row)
{
	gint testament, book;

	if (!row) {
		gtk_widget_error_bell(p->entry);
		return;
	}
	testament = GPOINTER_TO_INT(
	    g_object_get_data(G_OBJECT(row), "elim-testamento"));
	book = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "elim-indice"));

	picker_popdown(p->popover);
	if (testament == 1)
		on_ot_book_menu_select(NULL, GINT_TO_POINTER(book));
	else
		on_nt_book_menu_select(NULL, GINT_TO_POINTER(book));
}

static void bookpicker_row_activated(GtkListBox *list, GtkListBoxRow *row,
				     gpointer data)
{
	bookpicker_go((BOOKPICKER *)data, row);
}

static void bookpicker_entry_changed(GtkEditable *editable, gpointer data)
{
	BOOKPICKER *p = (BOOKPICKER *)data;

	g_free(p->needle);
	p->needle = g_strdup(gtk_entry_get_text(GTK_ENTRY(p->entry)));
	gtk_list_box_invalidate_filter(GTK_LIST_BOX(p->list));
	/* dejar señalado a dónde lleva Enter; si no queda ninguno, se ve
	 * la lista vacía y basta con borrar una letra. */
	bookpicker_select(p, bookpicker_first_match(p));
}

static void bookpicker_entry_activate(GtkEntry *entry, gpointer data)
{
	BOOKPICKER *p = (BOOKPICKER *)data;
	GtkListBoxRow *row =
	    gtk_list_box_get_selected_row(GTK_LIST_BOX(p->list));

	if (!row || !bookpicker_matches(row, p))
		row = bookpicker_first_match(p);
	bookpicker_go(p, row);
}

/* el foco vive en la entrada, así que las flechas no llegan a la lista:
 * se las pasamos nosotros, saltándonos las filas que el filtro esconde. */
static gboolean bookpicker_entry_key(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	BOOKPICKER *p = (BOOKPICKER *)data;
	GtkListBoxRow *selected;
	GList *rows, *l, *matches = NULL;
	gint step, at = -1, i = 0, len;

	switch (event->keyval) {
	case GDK_KEY_Down:
	case GDK_KEY_KP_Down:
		step = 1;
		break;
	case GDK_KEY_Up:
	case GDK_KEY_KP_Up:
		step = -1;
		break;
	default:
		return FALSE;
	}

	selected = gtk_list_box_get_selected_row(GTK_LIST_BOX(p->list));
	rows = gtk_container_get_children(GTK_CONTAINER(p->list));
	for (l = rows; l; l = l->next) {
		if (!bookpicker_matches(GTK_LIST_BOX_ROW(l->data), p))
			continue;
		if (GTK_LIST_BOX_ROW(l->data) == selected)
			at = i;
		matches = g_list_append(matches, l->data);
		i++;
	}
	g_list_free(rows);

	len = g_list_length(matches);
	if (len) {
		at = CLAMP(at + step, 0, len - 1);
		bookpicker_select(p, GTK_LIST_BOX_ROW(
					  g_list_nth_data(matches, at)));
	}
	g_list_free(matches);
	return TRUE;
}

static void bookpicker_list_allocated(GtkWidget *widget,
				      GdkRectangle *allocation,
				      gpointer data)
{
	BOOKPICKER *p = (BOOKPICKER *)data;

	if (p->placed || allocation->height <= 1)
		return;
	p->placed = TRUE;
	picker_scroll_to(p->scroll, p->list, p->current);
}

static void bookpicker_add(BOOKPICKER *p, const gchar *name, gint testament,
			   gint book, gboolean current)
{
	GtkWidget *row = gtk_list_box_row_new();
	GtkWidget *label = gtk_label_new(name);

	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_start(label, 8);
	gtk_widget_set_margin_end(label, 8);
	gtk_widget_set_margin_top(label, 2);
	gtk_widget_set_margin_bottom(label, 2);
	gtk_container_add(GTK_CONTAINER(row), label);

	g_object_set_data_full(G_OBJECT(row), "elim-libro", g_strdup(name),
			       g_free);
	g_object_set_data(G_OBJECT(row), "elim-testamento",
			  GINT_TO_POINTER(testament));
	g_object_set_data(G_OBJECT(row), "elim-indice", GINT_TO_POINTER(book));
	gtk_container_add(GTK_CONTAINER(p->list), row);
	if (current)
		p->current = row;
}

static BOOKPICKER *bookpicker_new(GtkWidget *anchor)
{
	BOOKPICKER *p = g_new0(BOOKPICKER, 1);
	GtkWidget *box;

	p->popover = gtk_popover_new(anchor);
	g_object_ref_sink(p->popover);
	gtk_popover_set_position(GTK_POPOVER(p->popover), GTK_POS_BOTTOM);
	picker_add_style(p->popover);
	g_object_set_data_full(G_OBJECT(p->popover), "elim-bookpicker", p,
			       bookpicker_free);

	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(box), 8);
	gtk_container_add(GTK_CONTAINER(p->popover), box);

	p->entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(p->entry), 20);
	gtk_entry_set_placeholder_text(GTK_ENTRY(p->entry), _("Buscar libro"));
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(p->entry),
					  GTK_ENTRY_ICON_PRIMARY,
					  "edit-find-symbolic");
	gtk_widget_set_tooltip_text(
	    p->entry, _("Escribe parte del nombre y pulsa Enter"));
	gtk_box_pack_start(GTK_BOX(box), p->entry, FALSE, FALSE, 0);

	p->list = gtk_list_box_new();
	gtk_list_box_set_selection_mode(GTK_LIST_BOX(p->list),
					GTK_SELECTION_BROWSE);
	gtk_list_box_set_filter_func(GTK_LIST_BOX(p->list), bookpicker_filter,
				     p, NULL);
	gtk_list_box_set_header_func(GTK_LIST_BOX(p->list), bookpicker_header,
				     p, NULL);

	p->scroll = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(p->scroll),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_scrolled_window_set_propagate_natural_height(
	    GTK_SCROLLED_WINDOW(p->scroll), TRUE);
	gtk_scrolled_window_set_max_content_height(
	    GTK_SCROLLED_WINDOW(p->scroll), 320);
#else
	gtk_widget_set_size_request(p->scroll, -1, 320);
#endif
	gtk_container_add(GTK_CONTAINER(p->scroll), p->list);
	gtk_box_pack_start(GTK_BOX(box), p->scroll, TRUE, TRUE, 0);

	g_signal_connect(p->entry, "changed",
			 G_CALLBACK(bookpicker_entry_changed), p);
	g_signal_connect(p->entry, "activate",
			 G_CALLBACK(bookpicker_entry_activate), p);
	g_signal_connect(p->entry, "key-press-event",
			 G_CALLBACK(bookpicker_entry_key), p);
	g_signal_connect(p->list, "row-activated",
			 G_CALLBACK(bookpicker_row_activated), p);
	g_signal_connect_after(p->list, "size-allocate",
			       G_CALLBACK(bookpicker_list_allocated), p);
	return p;
}

/******************************************************************************
 * Name
 *  main_versekey_popup_book
 *
 * Synopsis
 *   #include "main/navbar_versekey.h"
 *
 *   void main_versekey_popup_book(NAVBAR_VERSEKEY navbar, gint nb_type,
 *				   gpointer dialog, gpointer editor,
 *				   GtkWidget *anchor)
 *
 * Description
 *   despliega bajo anchor la lista filtrable de libros del módulo actual
 *
 * Return value
 *   void
 */

void main_versekey_popup_book(NAVBAR_VERSEKEY navbar, gint nb_type,
			      gpointer dialog, gpointer editor,
			      GtkWidget *anchor)
{
	BOOKPICKER *p;
	gchar *current_book;
	gint i;

	c_dialog = (DIALOG_DATA *)dialog;
	c_editor = (EDITOR *)editor;
	c_type = nb_type;

	if (!anchor || !navbar.module_name->len)
		return;

	SWModule *mod = backend->get_SWModule(navbar.module_name->str);
	if (!mod)
		return;

	VerseKey *key = (VerseKey *)mod->createKey();
	VerseKey *key_current = (VerseKey *)mod->createKey();
	key->setAutoNormalize(1);
	key_current->setAutoNormalize(1);
	key_current->setText(navbar.key->str);
	current_book = g_strdup((const char *)key_current->getBookName());

	p = bookpicker_new(anchor);

	if (backend->module_has_testament(navbar.module_name->str, 1)) {
		for (i = 0; i < key->BMAX[0]; i++) {
			key->setTestament(1);
			key->setBook(i + 1);
			const char *book = (const char *)key->getBookName();
			bookpicker_add(p, book, 1, i,
				       !strcmp(book, current_book));
		}
	}
	if (backend->module_has_testament(navbar.module_name->str, 2)) {
		for (i = 0; i < key->BMAX[1]; i++) {
			key->setTestament(2);
			key->setBook(i + 1);
			const char *book = (const char *)key->getBookName();
			bookpicker_add(p, book, 2, i,
				       !strcmp(book, current_book));
		}
	}

	delete key;
	delete key_current;
	g_free(current_book);

	gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(p->popover)));
	if (p->current)
		gtk_list_box_select_row(GTK_LIST_BOX(p->list),
					GTK_LIST_BOX_ROW(p->current));

	g_signal_connect(p->popover, "closed", G_CALLBACK(picker_closed),
			 anchor);
#if GTK_CHECK_VERSION(3, 22, 0)
	gtk_popover_popup(GTK_POPOVER(p->popover));
#else
	gtk_widget_show(p->popover);
#endif
	gtk_widget_grab_focus(p->entry);
}
