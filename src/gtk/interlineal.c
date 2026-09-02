/*
 * Biblia Elim — ficha de término interlineal
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/interlineal.h"
#include "gui/diccionario.h"
#include "gui/lectura_sync.h"
#include "gui/main_window.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/interlineal.h"
#include "main/diccionario.h"
#include "main/lectura_sync.h"
#include "main/settings.h"
#include "main/sword.h"
#include "main/xml.h"
#include "main/url.hh"

#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

static GtkWidget *ficha = NULL;
static GtkWidget *ficha_html = NULL;
static GtkWidget *btn_interlineal = NULL;
static gboolean syncing = FALSE;
static gchar *tools_key = NULL;
/* Módulo capturado junto con tools_key, en el mismo instante en que se
 * abrió el menú de herramientas. on_tools_nota() no debe releer
 * settings.MainWindowModule al momento del clic en "Agregar nota": si
 * un refresco por scroll (gui_bibletext_lectura_sync_focus_refresh)
 * dispara mientras el menú sigue abierto, ese global puede haber
 * cambiado de módulo -y hasta de versículo, si el otro módulo usa una
 * versificación distinta- para cuando el usuario efectivamente hace
 * clic. */
static gchar *tools_mod = NULL;

static void
verse_tools_goto(const char *key)
{
	gchar *url;
	if (!key || !*key)
		return;
	url = g_strdup_printf("sword:///%s", key);
	main_url_handler(url, TRUE);
	g_free(url);
}

void
gui_interlineal_rellenar(void)
{
	gboolean on = settings.show_interlineal != 0;

	if (btn_interlineal &&
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_interlineal)) != on) {
		syncing = TRUE;
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_interlineal), on);
		syncing = FALSE;
	}
	if (widgets.interlineal_item &&
	    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widgets.interlineal_item)) != on) {
		syncing = TRUE;
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.interlineal_item),
					       on);
		syncing = FALSE;
	}
}

void
gui_interlineal_set_active(gboolean active)
{
	settings.show_interlineal = active ? 1 : 0;
	xml_set_or_create_value("misc", "show_interlineal",
				settings.show_interlineal ? "1" : "0");
	if (syncing)
		return;
	syncing = TRUE;
	if (btn_interlineal &&
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_interlineal)) != active)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_interlineal), active);
	if (widgets.interlineal_item &&
	    gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widgets.interlineal_item)) != active)
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widgets.interlineal_item),
					       active);
	syncing = FALSE;
	if (active) {
		if (!main_interlineal_verso_abierto() && settings.currentverse)
			main_interlineal_abrir_verso(settings.currentverse);
		main_interlineal_empezar_indice();
		main_bible_note_interlinear_html();
		if (settings.currentverse)
			main_display_bible(NULL, settings.currentverse);
	} else {
		main_interlineal_cerrar_verso();
		gui_lectura_sync_ficha_clear();
		if (settings.show_lectura_sync)
			main_lectura_sync_actualizar();
		main_bible_note_interlinear_html();
		if (settings.currentverse)
			main_display_bible(NULL, settings.currentverse);
	}
}

static void
on_toggle_interlineal(GtkToggleButton *button, gpointer user_data)
{
	(void)user_data;
	if (syncing)
		return;
	gui_interlineal_set_active(gtk_toggle_button_get_active(button));
}

static void
on_toggle_comparar(GtkToggleButton *button, gpointer user_data)
{
	(void)user_data;
	gui_lectura_sync_set_visible(gtk_toggle_button_get_active(button));
}

GtkWidget *
gui_interlineal_wrap(GtkWidget *html_master)
{
	GtkWidget *vbox, *bar;

	g_return_val_if_fail(html_master != NULL, html_master);

	UI_VBOX(vbox, FALSE, 0);
	gtk_widget_show(vbox);
	/* La cinta lleva margen, y por ese margen asomaba el blanco del
	 * contenedor: un marco claro alrededor del interlineal en cuanto el
	 * tema dejaba de ser oscuro. El envoltorio toma el color del papel. */
	gtk_style_context_add_class(gtk_widget_get_style_context(vbox),
				    "elim-lienzo");

	UI_HBOX(bar, FALSE, 8);
	widgets.bar_interlineal = bar;
	gtk_widget_show(bar);
	gtk_widget_set_margin_start(bar, 8);
	gtk_widget_set_margin_end(bar, 8);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 2);
	gtk_box_pack_start(GTK_BOX(vbox), bar, FALSE, FALSE, 0);

	btn_interlineal = gtk_toggle_button_new();
	{
		GtkWidget *alpha = gtk_label_new("α");
		gtk_widget_show(alpha);
		gtk_container_add(GTK_CONTAINER(btn_interlineal), alpha);
	}
	gtk_style_context_add_class(gtk_widget_get_style_context(btn_interlineal),
				    "elim-pill");
	gtk_style_context_add_class(gtk_widget_get_style_context(btn_interlineal),
				    "elim-greek");
	gtk_style_context_add_class(gtk_widget_get_style_context(bar),
				    "elim-toolbar-strip");
	gtk_widget_show(btn_interlineal);
	gtk_widget_set_tooltip_text(btn_interlineal,
				    _("Interlineal: griego o hebreo de este versículo, palabra por palabra (Forward / Reverse)"));
	gtk_box_pack_start(GTK_BOX(bar), btn_interlineal, FALSE, FALSE, 0);
	g_signal_connect(btn_interlineal, "toggled",
			 G_CALLBACK(on_toggle_interlineal), NULL);

	/* "Comparar" panel (lectura sincronizada con otra versión) --
	 * hidden by default, only appears on demand from this button (or
	 * the matching View-menu item / its own close button). */
	widgets.lectura_sync_button = gtk_toggle_button_new_with_label(_("Comparar"));
	gtk_style_context_add_class(gtk_widget_get_style_context(widgets.lectura_sync_button),
				    "elim-pill");
	gtk_widget_show(widgets.lectura_sync_button);
	gtk_widget_set_tooltip_text(widgets.lectura_sync_button,
				    _("Muestra un panel para comparar esta versión con otra, "
				      "sincronizado al mismo versículo."));
	gtk_box_pack_start(GTK_BOX(bar), widgets.lectura_sync_button, FALSE, FALSE, 0);
	g_signal_connect(widgets.lectura_sync_button, "toggled",
			 G_CALLBACK(on_toggle_comparar), NULL);

	gtk_box_pack_start(GTK_BOX(vbox), html_master, TRUE, TRUE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_interlineal),
				     settings.show_interlineal != 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.lectura_sync_button),
				     settings.show_lectura_sync != 0);
	return vbox;
}

static gchar *
esc(const char *s)
{
	return g_markup_escape_text(s ? s : "", -1);
}

static void
on_ficha_destroy(GtkWidget *w, gpointer data)
{
	(void)w;
	(void)data;
	ficha = NULL;
	ficha_html = NULL;
}

static void
escribir(const gchar *html)
{
	if (!ficha_html)
		return;
	XIPHOS_HTML_OPEN_STREAM(ficha_html, "text/html");
	XIPHOS_HTML_WRITE(ficha_html, html, strlen(html));
	XIPHOS_HTML_CLOSE(ficha_html);
}

void
gui_interlineal_ficha(const char *strong)
{
	const InterlStrong *info;
	const DiccEntrada *dicc;
	GList *ocurr, *l;
	GString *body;
	gchar *html, *num, *lema, *tr;
	const char *bg, *fg;
	gint nocc;

	if (!strong || !*strong)
		return;
	main_interlineal_init();
	info = main_interlineal_strong(strong);
	dicc = main_diccionario_buscar(strong);
	if (!dicc && info && info->glosa && *info->glosa)
		dicc = main_diccionario_buscar(info->glosa);
	if (!dicc && info && info->lema && *info->lema)
		dicc = main_diccionario_buscar(info->lema);

	bg = settings.bible_bg_color ? settings.bible_bg_color : "#ffffff";
	fg = settings.bible_text_color ? settings.bible_text_color : "#222";
	body = g_string_new(NULL);
	num = esc(info && info->num && *info->num ? info->num : strong);
	lema = esc(info && info->lema ? info->lema : "");
	tr = esc(info && info->translit ? info->translit : "");

	g_string_append_printf(body, "<div class=\"num\">%s</div>", num);
	if (lema && *lema)
		g_string_append_printf(body, "<div class=\"orig\">%s</div>", lema);
	if (tr && *tr)
		g_string_append_printf(body, "<div class=\"tr\">%s</div>", tr);
	if (info && info->glosa && *info->glosa) {
		gchar *gl = esc(info->glosa);
		g_string_append_printf(body, "<div class=\"gl\">%s</div>", gl);
		g_free(gl);
	}
	if (info && info->raiz && *info->raiz) {
		gchar *rz = esc(info->raiz);
		g_string_append_printf(body,
				       "<p><b>%s</b> <a href=\"passagestudy.jsp?action=showInterlineal&amp;value=%s\">%s</a></p>",
				       _("Raíz:"), rz, rz);
		g_free(rz);
	}
	if (info && info->definicion && *info->definicion) {
		gchar *df = esc(info->definicion);
		g_string_append_printf(body, "<p>%s</p>", df);
		g_free(df);
	}
	if (dicc && dicc->definicion && *dicc->definicion) {
		gchar *d = esc(dicc->definicion);
		g_string_append_printf(body, "<p>%s</p>", d);
		g_free(d);
	}

	ocurr = main_interlineal_ocurrencias(strong, 80);
	if (!ocurr && !main_interlineal_indice_listo()) {
		g_string_append_printf(body, "<p class=\"occ\">%s</p>",
				       _("Ocurrencias: se están preparando…"));
	} else {
		nocc = ocurr ? (gint)g_list_length(ocurr) : 0;
		g_string_append_printf(body, "<p class=\"occ\"><b>%s:</b> %d</p>",
				       _("Ocurrencias"), nocc);
		if (ocurr) {
			g_string_append(body, "<p>");
			for (l = ocurr; l; l = l->next) {
				const char *k = (const char *)l->data;
				gchar *cita = main_interlineal_cita_es(k);
				gchar *ke = esc(k);
				gchar *ce = esc(cita);
				g_string_append_printf(body,
						       "<a href=\"sword:///%s\">%s</a>%s",
						       ke, ce,
						       l->next ? "; " : "");
				g_free(ke);
				g_free(ce);
				g_free(cita);
			}
			g_string_append(body, "</p>");
		}
	}
	g_list_free_full(ocurr, g_free);

	html = g_strdup_printf(
	    "<html><head><meta charset=\"utf-8\"/><style>"
	    "body{margin:0;padding:14px 16px;background:%s;color:%s;"
	    "font-family:'Noto Sans','DejaVu Sans',sans-serif;line-height:1.45;}"
	    ".num{font-size:1.35em;font-weight:700;margin:0 0 .15em;}"
	    ".orig{font-size:1.8em;font-family:'Noto Serif','SBL Hebrew','Ezra SIL',serif;margin:.1em 0;}"
	    ".tr{opacity:.75;font-style:italic;margin-bottom:.5em;}"
	    ".gl{color:#8B008B;font-weight:700;font-size:1.25em;margin:.15em 0 .45em;}"
	    ".occ{margin-top:1em;}"
	    "a{color:#1a4f8b;}"
	    "</style></head><body>%s</body></html>",
	    bg, fg, body->str);

	if (widgets.html_lectura_sync) {
		gui_lectura_sync_escribir(html);
	} else if (widgets.html_dict && gtk_widget_get_realized(widgets.html_dict)) {
		gui_show_hide_dicts(TRUE);
		gui_notebook_dict_goto_dict();
		HtmlOutput(html, widgets.html_dict, NULL, NULL);
	} else {
		if (!ficha) {
			GtkWidget *box, *btn;
			ficha = gtk_dialog_new_with_buttons(_("Término original"),
							    widgets.app ? GTK_WINDOW(widgets.app) : NULL,
							    GTK_DIALOG_DESTROY_WITH_PARENT,
							    NULL, NULL);
			gtk_window_set_default_size(GTK_WINDOW(ficha), 520, 560);
			box = gtk_dialog_get_content_area(GTK_DIALOG(ficha));
			ficha_html = GTK_WIDGET(XIPHOS_HTML_NEW(NULL, FALSE, VIEWER_TYPE));
			gtk_widget_set_vexpand(ficha_html, TRUE);
			gtk_box_pack_start(GTK_BOX(box), ficha_html, TRUE, TRUE, 0);
			btn = gtk_dialog_add_button(GTK_DIALOG(ficha), _("Cerrar"), GTK_RESPONSE_CLOSE);
			g_signal_connect(ficha, "destroy", G_CALLBACK(on_ficha_destroy), NULL);
			g_signal_connect(ficha, "response", G_CALLBACK(gtk_widget_destroy), NULL);
			gtk_widget_show_all(ficha);
			(void)btn;
		}
		escribir(html);
		gtk_window_present(GTK_WINDOW(ficha));
	}
	g_free(html);
	g_string_free(body, TRUE);
	g_free(num);
	g_free(lema);
	g_free(tr);
}

static void
on_tools_interlineal(GtkMenuItem *item, gpointer data)
{
	(void)item;
	(void)data;
	main_interlineal_abrir_verso(tools_key);
	verse_tools_goto(tools_key);
	gui_interlineal_set_active(TRUE);
}

static void
on_tools_comparar(GtkMenuItem *item, gpointer data)
{
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	gui_lectura_sync_set_visible(TRUE);
}

static void
on_tools_comentario(GtkMenuItem *item, gpointer data)
{
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	gui_show_hide_comms(TRUE);
	main_display_commentary(NULL, tools_key);
}

static void
on_tools_diccionario(GtkMenuItem *item, gpointer data)
{
	(void)item;
	(void)data;
	gui_diccionario_dialog();
}

static void
on_tools_xrefs(GtkMenuItem *item, gpointer data)
{
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	main_verse_tools_xrefs(tools_key);
}

static void
on_tools_nota(GtkMenuItem *item, gpointer data)
{
	const char *mod;
	gchar *osis, *cita;

	(void)item;
	(void)data;
	mod = tools_mod ? tools_mod : settings.MainWindowModule;
	if (!mod || !tools_key)
		return;
	osis = g_strdup(main_get_osisref_from_key(mod, tools_key));
	if (!osis || !*osis) {
		g_free(osis);
		return;
	}
	cita = main_interlineal_cita_es(tools_key);
	gui_lectura_sync_ficha_nota(mod, osis, cita);
	g_free(osis);
	g_free(cita);
}

void
gui_verse_tools_popup(const char *key)
{
	GtkWidget *menu, *item;

	if (!key || !*key)
		return;
	g_free(tools_key);
	tools_key = g_strdup(key);
	g_free(tools_mod);
	tools_mod = g_strdup(settings.MainWindowModule);

	menu = gtk_menu_new();

	item = gtk_menu_item_new_with_label(_("α   Interlineal"));
	gtk_widget_set_tooltip_text(item,
				    _("Muestra el griego o hebreo de este versículo"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_interlineal), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Comparar"));
	gtk_widget_set_tooltip_text(item, _("Compara este versículo con otra versión"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_comparar), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Nota"));
	gtk_widget_set_tooltip_text(item,
				    _("Escribe una nota de este versículo"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_nota), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Comentarios"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_comentario), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Diccionario"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_diccionario), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	item = gtk_menu_item_new_with_label(_("Referencias cruzadas"));
	g_signal_connect(item, "activate", G_CALLBACK(on_tools_xrefs), NULL);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	gtk_widget_show_all(menu);
	g_signal_connect_swapped(menu, "selection-done",
				 G_CALLBACK(gtk_widget_destroy), menu);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);
}

static void
on_il_strong(GtkButton *button, gpointer data)
{
	const char *num = g_object_get_data(G_OBJECT(button), "strong");

	(void)data;
	if (num && *num)
		gui_interlineal_ficha(num);
}

static GtkWidget *
il_label(const char *text, const char *klass, gboolean wrap)
{
	GtkWidget *l = gtk_label_new(text ? text : "");
	gtk_label_set_xalign(GTK_LABEL(l), 0.0);
	gtk_label_set_line_wrap(GTK_LABEL(l), wrap);
	gtk_label_set_line_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
	if (klass)
		gtk_style_context_add_class(gtk_widget_get_style_context(l), klass);
	gtk_widget_show(l);
	return l;
}

static GtkWidget *
il_row_widget(InterlFila *f)
{
	GtkWidget *row, *esbox, *orig, *morphbox, *btn, *badge;
	gchar *tip;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_style_context_add_class(gtk_widget_get_style_context(row), "il-row");
	gtk_widget_set_hexpand(row, TRUE);

	esbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_hexpand(esbox, TRUE);
	gtk_widget_set_valign(esbox, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(esbox), il_label(f->es, "il-es", TRUE),
			   FALSE, FALSE, 0);
	if (f->phrase) {
		badge = gtk_label_new(_("FRASE"));
		gtk_style_context_add_class(gtk_widget_get_style_context(badge),
					    "il-phrase");
		gtk_widget_set_halign(badge, GTK_ALIGN_START);
		gtk_widget_show(badge);
		gtk_box_pack_start(GTK_BOX(esbox), badge, FALSE, FALSE, 0);
	}
	gtk_widget_show(esbox);
	gtk_box_pack_start(GTK_BOX(row), esbox, TRUE, TRUE, 0);

	btn = gtk_button_new_with_label(f->strongs && *f->strongs ? f->strongs
								  : (f->strong ? f->strong : ""));
	gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus(btn, FALSE);
	gtk_widget_set_valign(btn, GTK_ALIGN_START);
	gtk_style_context_add_class(gtk_widget_get_style_context(btn), "il-strong");
	{
		GtkWidget *lab = gtk_bin_get_child(GTK_BIN(btn));
		if (GTK_IS_LABEL(lab)) {
			gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_NONE);
			gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
			gtk_style_context_add_class(gtk_widget_get_style_context(lab),
						    "il-strong");
		}
	}
	if (f->strong && *f->strong)
		g_object_set_data_full(G_OBJECT(btn), "strong",
				       g_strdup(f->strong), g_free);
	g_signal_connect(btn, "clicked", G_CALLBACK(on_il_strong), NULL);
	gtk_widget_set_size_request(btn, 108, -1);
	gtk_widget_show(btn);
	gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 0);

	orig = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(orig, TRUE);
	gtk_widget_set_valign(orig, GTK_ALIGN_START);
	if (f->forma && *f->forma) {
		GtkWidget *fl = il_label(f->forma,
					 f->hebrew ? "il-forma-he" : "il-forma",
					 TRUE);
		if (f->hebrew)
			gtk_widget_set_direction(fl, GTK_TEXT_DIR_RTL);
		if (f->strong && *f->strong) {
			GtkWidget *fb = gtk_button_new();
			gtk_button_set_relief(GTK_BUTTON(fb), GTK_RELIEF_NONE);
			gtk_widget_set_can_focus(fb, FALSE);
			gtk_container_add(GTK_CONTAINER(fb), fl);
			gtk_style_context_add_class(gtk_widget_get_style_context(fb),
						    "il-origbtn");
			g_object_set_data_full(G_OBJECT(fb), "strong",
					       g_strdup(f->strong), g_free);
			g_signal_connect(fb, "clicked", G_CALLBACK(on_il_strong), NULL);
			gtk_widget_show(fb);
			gtk_box_pack_start(GTK_BOX(orig), fb, FALSE, FALSE, 0);
		} else {
			gtk_box_pack_start(GTK_BOX(orig), fl, FALSE, FALSE, 0);
		}
	}
	if (f->raiz && *f->raiz &&
	    (!f->forma || strcmp(f->raiz, f->forma)))
		gtk_box_pack_start(GTK_BOX(orig),
				   il_label(f->raiz, "il-raiz", TRUE),
				   FALSE, FALSE, 0);
	if (f->translit && *f->translit)
		gtk_box_pack_start(GTK_BOX(orig),
				   il_label(f->translit, "il-trans", TRUE),
				   FALSE, FALSE, 0);
	gtk_widget_show(orig);
	gtk_box_pack_start(GTK_BOX(row), orig, TRUE, TRUE, 0);

	morphbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_valign(morphbox, GTK_ALIGN_START);
	gtk_widget_set_halign(morphbox, GTK_ALIGN_END);
	gtk_widget_set_size_request(morphbox, 96, -1);
	if (f->morph && *f->morph) {
		GtkWidget *pill = gtk_label_new(f->morph);
		gtk_style_context_add_class(gtk_widget_get_style_context(pill),
					    "il-morph");
		gtk_widget_set_halign(pill, GTK_ALIGN_END);
		if (f->morph_es && *f->morph_es)
			gtk_widget_set_tooltip_text(pill, f->morph_es);
		gtk_widget_show(pill);
		gtk_box_pack_start(GTK_BOX(morphbox), pill, FALSE, FALSE, 0);
	}
	gtk_widget_show(morphbox);
	gtk_box_pack_start(GTK_BOX(row), morphbox, FALSE, FALSE, 0);

	tip = g_strdup_printf("%s%s%s",
			      f->es ? f->es : "",
			      f->strong ? " · " : "",
			      f->strong ? f->strong : "");
	gtk_widget_set_tooltip_text(row, tip);
	g_free(tip);
	gtk_widget_show(row);
	return row;
}

static GtkWidget *
il_header_row(void)
{
	GtkWidget *row, *a, *b, *c, *d;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_style_context_add_class(gtk_widget_get_style_context(row), "il-hdr");
	a = il_label(_("Español"), "il-hdr-cell", FALSE);
	gtk_widget_set_hexpand(a, TRUE);
	gtk_box_pack_start(GTK_BOX(row), a, TRUE, TRUE, 0);
	b = il_label(_("Strong's"), "il-hdr-cell", FALSE);
	gtk_widget_set_size_request(b, 108, -1);
	gtk_box_pack_start(GTK_BOX(row), b, FALSE, FALSE, 0);
	c = il_label(_("Forma, raíz y transliteración"), "il-hdr-cell", FALSE);
	gtk_widget_set_hexpand(c, TRUE);
	gtk_box_pack_start(GTK_BOX(row), c, TRUE, TRUE, 0);
	d = il_label(_("Análisis"), "il-hdr-cell", FALSE);
	gtk_widget_set_halign(d, GTK_ALIGN_END);
	gtk_widget_set_size_request(d, 96, -1);
	gtk_box_pack_start(GTK_BOX(row), d, FALSE, FALSE, 0);
	gtk_widget_show(row);
	return row;
}

static void
il_fill_rows(GtkWidget *box, const char *key, gboolean reverse)
{
	GtkWidget *rows;
	GList *filas, *l;
	int i;

	rows = g_object_get_data(G_OBJECT(box), "il-rows");
	if (rows)
		gtk_widget_destroy(rows);
	rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(rows), "il-rows");
	gtk_box_pack_start(GTK_BOX(rows), il_header_row(), FALSE, FALSE, 0);

	filas = main_interlineal_filas(key, reverse);
	for (l = filas, i = 0; l; l = l->next, i++) {
		GtkWidget *r = il_row_widget((InterlFila *)l->data);
		if (i % 2)
			gtk_style_context_add_class(gtk_widget_get_style_context(r),
						    "il-row-alt");
		gtk_box_pack_start(GTK_BOX(rows), r, FALSE, FALSE, 0);
	}
	main_interlineal_filas_free(filas);
	gtk_widget_show(rows);
	gtk_box_pack_start(GTK_BOX(box), rows, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(box), "il-rows", rows);
	{
		GtkWidget *pie = g_object_get_data(G_OBJECT(box), "il-pie");
		const char *t = main_interlineal_pie_es();
		if (pie) {
			if (t && *t) {
				gtk_label_set_text(GTK_LABEL(pie), t);
				gtk_widget_show(pie);
			} else {
				gtk_label_set_text(GTK_LABEL(pie), "");
				gtk_widget_hide(pie);
			}
		}
	}
}

static void
il_mark_tab(GtkWidget *active, GtkWidget *idle)
{
	GtkStyleContext *a = gtk_widget_get_style_context(active);
	GtkStyleContext *b = gtk_widget_get_style_context(idle);
	gtk_style_context_add_class(a, "il-tab-active");
	gtk_style_context_remove_class(b, "il-tab-active");
}

static void
on_il_tab(GtkButton *btn, gpointer data)
{
	GtkWidget *box = GTK_WIDGET(data);
	GtkWidget *fwd, *revb;
	const char *key;
	gboolean reverse;

	reverse = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "il-rev"));
	fwd = g_object_get_data(G_OBJECT(box), "il-fwd");
	revb = g_object_get_data(G_OBJECT(box), "il-revb");
	if (fwd && revb)
		il_mark_tab(reverse ? revb : fwd, reverse ? fwd : revb);
	main_interlineal_set_modo_reverse(reverse);
	key = g_object_get_data(G_OBJECT(box), "il-key");
	if (key)
		il_fill_rows(box, key, reverse);
}

GtkWidget *
gui_interlineal_tabla_widget(const char *key)
{
	GtkWidget *box, *tabs, *fwd, *rev;
	gboolean reverse;

	if (!key || !*key)
		return NULL;
	reverse = main_interlineal_modo_reverse();
	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(box), "il-table");
	gtk_style_context_add_class(gtk_widget_get_style_context(box),
				    settings.darktheme ? "il-dark" : "il-light");
	g_object_set_data_full(G_OBJECT(box), "il-key", g_strdup(key), g_free);
	gtk_widget_set_hexpand(box, TRUE);

	tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_style_context_add_class(gtk_widget_get_style_context(tabs), "il-tabs");
	fwd = gtk_button_new_with_label(_("Original → Español"));
	rev = gtk_button_new_with_label(_("Español → Original"));
	gtk_button_set_relief(GTK_BUTTON(fwd), GTK_RELIEF_NONE);
	gtk_button_set_relief(GTK_BUTTON(rev), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus(fwd, FALSE);
	gtk_widget_set_can_focus(rev, FALSE);
	gtk_style_context_add_class(gtk_widget_get_style_context(fwd), "il-tab");
	gtk_style_context_add_class(gtk_widget_get_style_context(rev), "il-tab");
	gtk_widget_set_tooltip_text(fwd,
				    _("Orden del original: cada palabra griega o hebrea y su equivalente en español"));
	gtk_widget_set_tooltip_text(rev,
				    _("Orden del español: cada palabra de la traducción y su original"));
	g_object_set_data(G_OBJECT(fwd), "il-rev", GINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(rev), "il-rev", GINT_TO_POINTER(1));
	g_object_set_data(G_OBJECT(box), "il-fwd", fwd);
	g_object_set_data(G_OBJECT(box), "il-revb", rev);
	il_mark_tab(reverse ? rev : fwd, reverse ? fwd : rev);
	g_signal_connect(fwd, "clicked", G_CALLBACK(on_il_tab), box);
	g_signal_connect(rev, "clicked", G_CALLBACK(on_il_tab), box);
	gtk_box_pack_start(GTK_BOX(tabs), fwd, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(tabs), rev, FALSE, FALSE, 0);
	gtk_widget_show_all(tabs);
	gtk_box_pack_start(GTK_BOX(box), tabs, FALSE, FALSE, 0);

	{
		GtkWidget *pie = gtk_label_new("");
		gtk_style_context_add_class(gtk_widget_get_style_context(pie),
					    "il-pie");
		gtk_label_set_xalign(GTK_LABEL(pie), 0.0);
		gtk_label_set_line_wrap(GTK_LABEL(pie), TRUE);
		gtk_widget_set_margin_start(pie, 12);
		gtk_widget_set_margin_end(pie, 12);
		gtk_widget_set_margin_top(pie, 4);
		gtk_widget_set_margin_bottom(pie, 2);
		gtk_box_pack_start(GTK_BOX(box), pie, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(box), "il-pie", pie);
	}

	il_fill_rows(box, key, reverse);
	gtk_widget_show(box);
	return box;
}

G_MODULE_EXPORT void
on_interlineal_activate(GtkCheckMenuItem *menuitem, gpointer user_data)
{
	(void)user_data;
	if (syncing)
		return;
	gui_interlineal_set_active(gtk_check_menu_item_get_active(menuitem));
}
