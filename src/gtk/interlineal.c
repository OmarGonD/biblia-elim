/*
 * Biblia Elim — ficha de término interlineal
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include <glib/gi18n.h>

#include "gui/interlineal.h"
#include "gui/diccionario.h"
#include "gui/lectura_sync.h"
#include "gui/main_window.h"
#include "gui/main_menu.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/interlineal.h"
#include "main/morfologia.h"
#include "main/glosa.h"
#include "main/interl_enriq.h"
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
 * el seguimiento del foco por scroll (reading_focus_update() en bibletext.c)
 * dispara mientras el menú sigue abierto, ese global puede haber
 * cambiado de módulo -y hasta de versículo, si el otro módulo usa una
 * versificación distinta- para cuando el usuario efectivamente hace
 * clic. */
static gchar *tools_mod = NULL;

/* `key` is native to tools_mod, captured with it when the menu opened.
 * A module-less URI navigates whatever Bible is selected *now*, which
 * the comment above explains can already be a different one -- and with
 * a different versification. Carry the reference over rather than let
 * the new module reparse the text. */
static void
verse_tools_goto(const char *key)
{
	gchar *url;
	gchar *main_key;

	if (!key || !*key)
		return;
	main_key = main_bible_key_for_uri(tools_mod, key);
	if (!main_key) {
		main_warn_reference_unmapped(key, settings.MainWindowModule);
		return;
	}
	url = g_strdup_printf("sword:///%s", main_key);
	main_url_handler(url, TRUE);
	g_free(main_key);
	g_free(url);
}

void
gui_interlineal_rellenar(void)
{
	gboolean on = settings.show_interlineal != 0;

	if (btn_interlineal &&
	    gui_toggle_get_active(GTK_WIDGET(btn_interlineal)) != on) {
		syncing = TRUE;
		gui_toggle_set_active(GTK_WIDGET(btn_interlineal), on);
		syncing = FALSE;
	}
	gui_main_menu_set_state("interlinear", on);
	gui_reading_interlinear_sync();
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
	    gui_toggle_get_active(GTK_WIDGET(btn_interlineal)) != active)
		gui_toggle_set_active(GTK_WIDGET(btn_interlineal), active);
	gui_main_menu_set_state("interlinear", active);
	syncing = FALSE;
	gui_reading_interlinear_sync();
	if (active) {
		if (!main_interlineal_verso_abierto() && settings.currentverse)
			main_interlineal_abrir_verso(settings.currentverse);
		main_interlineal_empezar_indice();
		main_bible_note_interlinear_html();
		{
			/* El versículo que se pidió abrir, no el guardado: desde el menú
			 * del versículo se navega a él justo antes de llegar aquí, y
			 * settings.currentverse puede ir un paso atrás (el lector
			 * volvía al versículo anterior). */
			const char *k = main_interlineal_verso_abierto();

			if (!k || !*k)
				k = settings.currentverse;
			if (k)
				main_display_bible(NULL, k);
		}
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
	gui_interlineal_set_active(gui_toggle_get_active(button));
}

static void
on_toggle_comparar(GtkToggleButton *button, gpointer user_data)
{
	(void)user_data;
	gui_lectura_sync_set_visible(gui_toggle_get_active(button));
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
	gtk_widget_add_css_class(vbox, "elim-lienzo");

	UI_HBOX(bar, FALSE, 8);
	widgets.bar_interlineal = bar;
	gtk_widget_show(bar);
	gtk_widget_set_margin_start(bar, 8);
	gtk_widget_set_margin_end(bar, 8);
	gtk_widget_set_margin_top(bar, 4);
	gtk_widget_set_margin_bottom(bar, 2);
	gtk_box_append(GTK_BOX(vbox), bar);

	btn_interlineal = gtk_toggle_button_new();
	{
		/* Una α suelta no dice qué hace el botón: lleva su nombre. */
		GtkWidget *content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
		GtkWidget *alpha = gtk_label_new("α");
		GtkWidget *name = gtk_label_new(_("Interlineal"));

		gtk_widget_add_css_class(alpha, "elim-greek-glyph");
		gtk_box_append(GTK_BOX(content), alpha);
		gtk_box_append(GTK_BOX(content), name);
		gtk_widget_show(alpha);
		gtk_widget_show(name);
		gtk_widget_show(content);
		gtk_button_set_child(GTK_BUTTON(btn_interlineal), content);
	}
	gtk_widget_add_css_class(btn_interlineal, "elim-pill");
	gtk_widget_add_css_class(bar, "elim-toolbar-strip");
	gtk_widget_show(btn_interlineal);
	gtk_widget_set_tooltip_text(btn_interlineal,
				    _("Interlineal: griego o hebreo de este versículo, palabra por palabra (Forward / Reverse)"));
	gtk_box_append(GTK_BOX(bar), btn_interlineal);
	g_signal_connect(btn_interlineal, "toggled",
			 G_CALLBACK(on_toggle_interlineal), NULL);

	/* "Comparar" panel (lectura sincronizada con otra versión) --
	 * hidden by default, only appears on demand from this button (or
	 * the matching View-menu item / its own close button). */
	widgets.lectura_sync_button = gtk_toggle_button_new_with_label(_("Comparar"));
	gtk_widget_add_css_class(widgets.lectura_sync_button, "elim-pill");
	gtk_widget_show(widgets.lectura_sync_button);
	gtk_widget_set_tooltip_text(widgets.lectura_sync_button,
				    _("Muestra un panel para comparar esta versión con otra, "
				      "sincronizado al mismo versículo."));
	gtk_box_append(GTK_BOX(bar), widgets.lectura_sync_button);
	g_signal_connect(widgets.lectura_sync_button, "toggled",
			 G_CALLBACK(on_toggle_comparar), NULL);

	gui_box_pack(GTK_BOX(vbox), html_master, TRUE, TRUE, 0);

	gui_toggle_set_active(GTK_WIDGET(btn_interlineal),
				     settings.show_interlineal != 0);
	gui_toggle_set_active(GTK_WIDGET(widgets.lectura_sync_button),
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
	gui_interlineal_ficha_morf(strong, NULL);
}

void
gui_interlineal_ficha_morf(const char *strong, const char *morph)
{
	gui_interlineal_ficha_ctx(strong, morph, NULL, 0);
}

/* Referencia OSIS ("John.1.39") del versículo `key` en el módulo Tisch (su numeración, que puede diferir de la
 * estándar), o NULL. Es la ref_tisch de las fichas. */
static gchar *
ref_tisch_de(const char *key)
{
	gchar *mapped;
	const char *osis;
	gchar *res;

	mapped = main_reference_for_module(settings.MainWindowModule, key, "Tisch");
	if (!mapped)
		return NULL;
	osis = main_get_osisref_from_key("Tisch", mapped);
	res = osis && *osis ? g_strdup(osis) : NULL;
	g_free(mapped);
	return res;
}

/* La ficha enriquecida es de la palabra en su pasaje (versículo de Tisch + posición), no del número. */
static gchar *
ficha_enriquecida(const char *strong, const char *key, int pos, gboolean texto_plano)
{
	gchar *ref, *html;

	if (!key || !*key)
		key = main_interlineal_verso_actual();
	if (!key || !*key)
		return NULL;
	ref = ref_tisch_de(key);
	if (!ref)
		return NULL;
	html = texto_plano ? main_interl_enriq_texto(ref, pos, strong)
			   : main_interl_enriq_html(ref, pos, strong);
	g_free(ref);
	return html;
}

void
gui_interlineal_ficha_ctx(const char *strong, const char *morph,
			  const char *key, int pos)
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
	/* Cómo está la palabra en este versículo. Va antes que la raíz y la
	 * definición porque es lo que se acaba de pulsar. */
	if (morph && *morph) {
		gchar *m_es = main_morf_es(morph);
		gchar *m_cod = main_morf_codigo(morph);

		if (m_es && *m_es) {
			gchar *e1 = esc(m_es), *e2 = esc(m_cod);

			g_string_append_printf(body,
					       "<p class=\"morf\"><b>%s</b> %s <span class=\"cod\">%s</span></p>",
					       _("Aquí:"), e1, e2);
			g_free(e1);
			g_free(e2);
		}
		g_free(m_es);
		g_free(m_cod);
	}
	{
		gchar *enr = ficha_enriquecida(strong, key, pos, FALSE);

		if (enr)
			g_string_append(body, enr);
		g_free(enr);
	}
	if (info && info->raiz && *info->raiz) {
		gchar *rz = esc(info->raiz);
		g_string_append_printf(body,
				       "<p><b>%s</b> <a href=\"passagestudy.jsp?action=showInterlineal&amp;value=%s\">%s</a></p>",
				       _("Raíz:"), rz, rz);
		g_free(rz);
	}
	if (info && info->definicion && *info->definicion) {
		/* La definición, sin repetir la glosa que ya está arriba. */
		gchar *limpia = main_glosa_definicion(info->definicion,
						      info->glosa);
		gchar *rv = main_glosa_rv1909(info->definicion);

		if (limpia && *limpia) {
			gchar *df = esc(limpia);

			g_string_append_printf(body, "<p>%s</p>", df);
			g_free(df);
		}
		/* Cómo lo tradujo la Reina-Valera no es lo que la palabra
		 * significa: es otro dato y va en su renglón. */
		if (rv) {
			gchar *r = esc(rv);

			g_string_append_printf(body,
					       "<p class=\"rv\"><b>%s</b> %s</p>",
					       _("En la Reina-Valera 1909:"), r);
			g_free(r);
		}
		g_free(limpia);
		g_free(rv);
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
			const char *occ_mod =
			    main_interlineal_ocurrencias_modulo(strong);
			for (l = ocurr; l; l = l->next) {
				const char *k = (const char *)l->data;
				/* The concordance is numbered by its own
				 * module; the link navigates the reader's
				 * Bible, so carry the reference across and
				 * label it with where it actually lands. A
				 * verse that has no counterpart there is
				 * not offered as a link. */
				gchar *nk = main_bible_key_for_uri(occ_mod, k);
				gchar *cita;
				gchar *ke, *ce;

				if (!nk)
					continue;
				cita = main_interlineal_cita_es(nk);
				ke = esc(nk);
				ce = esc(cita);
				g_string_append_printf(body,
						       "<a href=\"sword:///%s\">%s</a>%s",
						       ke, ce,
						       l->next ? "; " : "");
				g_free(nk);
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
	    ".morf{margin:.2em 0 .6em;opacity:.9;}"
	    ".morf .cod{opacity:.5;font-size:.85em;margin-left:.4em;}"
	    ".rv{opacity:.75;font-size:.95em;}"
	    ".enr{margin:.5em 0;padding:.5em .7em;border-left:3px solid #8B008B;}"
	    ".enr p{margin:.35em 0;}.enr ul{margin:.1em 0 .4em 1.2em;padding:0;}"
	    ".enr .nota,.enr-cz{opacity:.65;font-size:.9em;}"
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
			gui_box_pack(GTK_BOX(box), ficha_html, TRUE, TRUE, 0);
			btn = gtk_dialog_add_button(GTK_DIALOG(ficha), _("Cerrar"), GTK_RESPONSE_CLOSE);
			g_signal_connect(ficha, "destroy", G_CALLBACK(on_ficha_destroy), NULL);
			g_signal_connect(ficha, "response", G_CALLBACK(gui_widget_destroy), NULL);
			gtk_widget_show(ficha);
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
on_tools_interlineal(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
	(void)item;
	(void)data;
	main_interlineal_abrir_verso(tools_key);
	verse_tools_goto(tools_key);
	gui_interlineal_set_active(TRUE);
}

static void
on_tools_comparar(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	gui_lectura_sync_set_visible(TRUE);
}

static void
on_tools_comentario(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	gui_show_hide_comms(TRUE);
	main_display_commentary(NULL, tools_key);
}

static void
on_tools_diccionario(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
	(void)item;
	(void)data;
	gui_diccionario_dialog();
}

static void
on_tools_xrefs(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
	(void)item;
	(void)data;
	verse_tools_goto(tools_key);
	main_verse_tools_xrefs(tools_key);
}

static void
on_tools_nota(GSimpleAction *item, GVariant *parameter, gpointer data)
{
	(void)parameter;
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

/* GTK4-PORT-101 step 2: a GMenu over «versiculo» actions, shown as a
 * popover at the pointer over the main window. */
GtkWidget *
gui_verse_tools_popup(const char *key)
{
	static const GActionEntry acciones[] = {
		{ "interlineal", on_tools_interlineal, NULL, NULL, NULL, { 0 } },
		{ "comparar", on_tools_comparar, NULL, NULL, NULL, { 0 } },
		{ "nota", on_tools_nota, NULL, NULL, NULL, { 0 } },
		{ "comentario", on_tools_comentario, NULL, NULL, NULL, { 0 } },
		{ "diccionario", on_tools_diccionario, NULL, NULL, NULL, { 0 } },
		{ "xrefs", on_tools_xrefs, NULL, NULL, NULL, { 0 } },
	};
	GtkWidget *relative;
	GSimpleActionGroup *grupo;
	GMenu *menu, *estudio, *consulta;

	if (!key || !*key || !widgets.app)
		return NULL;
	g_free(tools_key);
	tools_key = g_strdup(key);
	g_free(tools_mod);
	tools_mod = g_strdup(settings.MainWindowModule);

	relative = gtk_window_get_child(GTK_WINDOW(widgets.app));
	grupo = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(grupo), acciones,
					G_N_ELEMENTS(acciones), NULL);
	gui_widget_insert_action_group(relative, "versiculo", G_ACTION_GROUP(grupo));
	g_object_unref(grupo);

	menu = g_menu_new();
	estudio = g_menu_new();
	g_menu_append(estudio, _("α   Interlineal"), "versiculo.interlineal");
	g_menu_append(estudio, _("Comparar"), "versiculo.comparar");
	g_menu_append(estudio, _("Nota"), "versiculo.nota");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(estudio));
	g_object_unref(estudio);
	consulta = g_menu_new();
	g_menu_append(consulta, _("Comentarios"), "versiculo.comentario");
	g_menu_append(consulta, _("Diccionario"), "versiculo.diccionario");
	g_menu_append(consulta, _("Referencias cruzadas"), "versiculo.xrefs");
	g_menu_append_section(menu, NULL, G_MENU_MODEL(consulta));
	g_object_unref(consulta);
	GtkWidget *popover = gui_popup_menu_model_at_pointer(G_MENU_MODEL(menu), relative);
	g_object_unref(menu);
	return popover;
}

static void
on_il_strong(GtkButton *button, gpointer data)
{
	const char *num = g_object_get_data(G_OBJECT(button), "strong");
	const char *morf = g_object_get_data(G_OBJECT(button), "morph");

	(void)data;
	if (num && *num)
		gui_interlineal_ficha_ctx(num, morf,
					  g_object_get_data(G_OBJECT(button), "key"),
					  GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "pos")));
}

static GtkWidget *
il_label(const char *text, const char *klass, gboolean wrap)
{
	GtkWidget *l = gtk_label_new(text ? text : "");
	gtk_label_set_xalign(GTK_LABEL(l), 0.0);
	gtk_label_set_wrap(GTK_LABEL(l), wrap);
	gtk_label_set_wrap_mode(GTK_LABEL(l), PANGO_WRAP_WORD_CHAR);
	if (klass)
		gtk_widget_add_css_class(l, klass);
	gtk_widget_show(l);
	return l;
}

static GtkWidget *
il_original_label(const char *text, const char *klass)
{
	GtkWidget *label = il_label(text, klass, TRUE);
	if (main_interlineal_es_hebreo(text)) {
		gtk_widget_set_direction(label, GTK_TEXT_DIR_RTL);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
	}
	return label;
}

static void
on_il_copy_study(GtkButton *button, gpointer data)
{
	const char *text = g_object_get_data(G_OBJECT(button), "study-text");
	(void)data;
	if (!text)
		return;
	gdk_clipboard_set_text(gtk_widget_get_clipboard(GTK_WIDGET(button)), text);
	gtk_button_set_label(button, _("Copiado"));
}

static GtkWidget *
il_copy_button(const char *title, const char *text)
{
	GtkWidget *button = gtk_button_new_with_label(title);
	gtk_widget_set_halign(button, GTK_ALIGN_START);
	g_object_set_data_full(G_OBJECT(button), "study-text", g_strdup(text), g_free);
	g_signal_connect(button, "clicked", G_CALLBACK(on_il_copy_study), NULL);
	return button;
}

/* Ancho de las dos columnas fijas. Cabecera y filas las comparten: si
 * cada una repartiera el espacio por su cuenta, los títulos no caerían
 * encima de sus datos. */
#define IL_COL_STRONG 108
#define IL_COL_MORPH 96

/* Las dos columnas de texto (original y español), a partes iguales. Es la
 * misma estructura en la cabecera y en cada fila. */
static GtkWidget *
il_text_cols(GtkWidget *first, GtkWidget *second)
{
	GtkWidget *cols = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);

	gtk_box_set_homogeneous(GTK_BOX(cols), TRUE);
	gtk_widget_set_hexpand(cols, TRUE);
	gtk_widget_set_hexpand(first, TRUE);
	gtk_widget_set_hexpand(second, TRUE);
	gtk_box_append(GTK_BOX(cols), first);
	gtk_box_append(GTK_BOX(cols), second);
	gtk_widget_show(cols);
	return cols;
}

static GtkWidget *
il_row_widget(InterlFila *f, gboolean reverse, const char *key)
{
	GtkWidget *row, *esbox, *orig, *morphbox, *btn, *badge;
	gchar *tip;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_set_hexpand(row, TRUE);

	esbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_hexpand(esbox, TRUE);
	gtk_widget_set_valign(esbox, GTK_ALIGN_START);
	gtk_box_append(GTK_BOX(esbox), il_label(f->es, "il-es", TRUE));
	if (f->phrase) {
		badge = gtk_label_new(_("FRASE"));
		gtk_widget_add_css_class(badge, "il-phrase");
		gtk_widget_set_halign(badge, GTK_ALIGN_START);
		gtk_widget_show(badge);
		gtk_box_append(GTK_BOX(esbox), badge);
	}
	btn = gtk_button_new_with_label(f->strongs && *f->strongs ? f->strongs
								  : (f->strong ? f->strong : ""));
	gtk_button_set_has_frame(GTK_BUTTON(btn), FALSE);
	gtk_widget_set_can_focus(btn, FALSE);
	gtk_widget_set_valign(btn, GTK_ALIGN_START);
	gtk_widget_add_css_class(btn, "il-strong");
	{
		GtkWidget *lab = gtk_button_get_child(GTK_BUTTON(btn));
		if (GTK_IS_LABEL(lab)) {
			gtk_label_set_ellipsize(GTK_LABEL(lab), PANGO_ELLIPSIZE_NONE);
			gtk_label_set_xalign(GTK_LABEL(lab), 0.0);
			gtk_widget_add_css_class(lab, "il-strong");
		}
	}
	if (f->strong && *f->strong) {
		g_object_set_data_full(G_OBJECT(btn), "strong",
				       g_strdup(f->strong), g_free);
		/* La ficha enseña además cómo está esa palabra aquí, y eso
		 * es de la fila, no del número: el mismo Strong sale en un
		 * versículo en aoristo y en otro en imperativo. */
		if (key)
			g_object_set_data_full(G_OBJECT(btn), "key",
					       g_strdup(key), g_free);
		g_object_set_data(G_OBJECT(btn), "pos", GINT_TO_POINTER(f->pos));
		if (f->morph && *f->morph)
			g_object_set_data_full(G_OBJECT(btn), "morph",
					       g_strdup(f->morph), g_free);
	}
	g_signal_connect(btn, "clicked", G_CALLBACK(on_il_strong), NULL);
	gtk_widget_set_size_request(btn, IL_COL_STRONG, -1);
	orig = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_hexpand(orig, TRUE);
	gtk_widget_set_valign(orig, GTK_ALIGN_START);
	if (f->forma && *f->forma) {
		GtkWidget *fl = il_original_label(f->forma,
			main_interlineal_es_hebreo(f->forma) ? "il-forma-he" : "il-forma");
		if (f->strong && *f->strong) {
			GtkWidget *fb = gtk_button_new();
			gtk_button_set_has_frame(GTK_BUTTON(fb), FALSE);
			gtk_widget_set_can_focus(fb, FALSE);
			gtk_button_set_child(GTK_BUTTON(fb), fl);
			gtk_widget_add_css_class(fb, "il-origbtn");
			g_object_set_data_full(G_OBJECT(fb), "strong",
					       g_strdup(f->strong), g_free);
			if (key)
				g_object_set_data_full(G_OBJECT(fb), "key",
						       g_strdup(key), g_free);
			g_object_set_data(G_OBJECT(fb), "pos", GINT_TO_POINTER(f->pos));
			if (f->morph && *f->morph)
				g_object_set_data_full(G_OBJECT(fb), "morph",
						       g_strdup(f->morph),
						       g_free);
			g_signal_connect(fb, "clicked", G_CALLBACK(on_il_strong), NULL);
			gtk_widget_show(fb);
			gtk_box_append(GTK_BOX(orig), fb);
		} else {
			gtk_box_append(GTK_BOX(orig), fl);
		}
	}
	if (f->raiz && *f->raiz &&
	    (!f->forma || strcmp(f->raiz, f->forma)))
		gtk_box_append(GTK_BOX(orig), il_original_label(f->raiz, "il-raiz"));
	if (f->translit && *f->translit)
		gtk_box_append(GTK_BOX(orig), il_label(f->translit, "il-trans", TRUE));
	/* La dirección seleccionada debe verse también en las columnas: en
	 * «Griego/hebreo → Español» la forma original va primero; en la
	 * inversa, el español. Antes ambas pestañas empezaban por español. */
	gtk_widget_show(esbox);
	gtk_widget_show(orig);
	gtk_widget_show(btn);
	gtk_box_append(GTK_BOX(row), reverse ? il_text_cols(esbox, orig)
					     : il_text_cols(orig, esbox));
	gtk_box_append(GTK_BOX(row), btn);

	morphbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_set_valign(morphbox, GTK_ALIGN_START);
	gtk_widget_set_halign(morphbox, GTK_ALIGN_END);
	gtk_widget_set_size_request(morphbox, IL_COL_MORPH, -1);
	/* En la etiqueta va el español, no el código: "V-PAI-3S" no le dice
	 * nada a quien no estudió griego, y es justo para ese lector para
	 * quien se hizo el interlineal. El código sigue estando, en el
	 * emergente, para quien sí sepa leerlo. */
	if (f->morph && *f->morph) {
		const char *visible = (f->morph_corto && *f->morph_corto)
					  ? f->morph_corto
					  : f->morph;
		GtkWidget *pill = gtk_label_new(visible);
		gchar *tip_m;

		gtk_widget_add_css_class(pill, "il-morph");
		gtk_widget_set_halign(pill, GTK_ALIGN_END);
		/* Una etiqueta corta cabe en una línea; con el ajuste activo
		 * GTK la medía a una anchura y la asignaba a otra, y la
		 * píldora mostraba «conj» con el punto cortado debajo. */
		if (g_utf8_strlen(visible, -1) > 14) {
			gtk_label_set_wrap(GTK_LABEL(pill), TRUE);
			gtk_label_set_wrap_mode(GTK_LABEL(pill),
						PANGO_WRAP_WORD_CHAR);
			gtk_label_set_justify(GTK_LABEL(pill),
					      GTK_JUSTIFY_RIGHT);
			gtk_label_set_max_width_chars(GTK_LABEL(pill), 14);
		}
		tip_m = g_strdup_printf("%s\n%s",
					(f->morph_es && *f->morph_es)
					    ? f->morph_es
					    : f->morph,
					f->morph);
		gtk_widget_set_tooltip_text(pill, tip_m);
		g_free(tip_m);
		gtk_widget_show(pill);
		gtk_box_append(GTK_BOX(morphbox), pill);
	}
	gtk_widget_show(morphbox);
	gtk_box_append(GTK_BOX(row), morphbox);

	tip = g_strdup_printf("%s%s%s",
			      f->es ? f->es : "",
			      f->strong ? " · " : "",
			      f->strong ? f->strong : "");
	gtk_widget_set_tooltip_text(row, tip);
	g_free(tip);
	gtk_widget_show(row);
	/* El detalle se puede leer con teclado y copiar sin depender del
	 * tooltip. Conservamos el texto: las filas se liberan al llenar la tabla. */
	GtkWidget *item = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_add_css_class(item, "il-row");
	GtkWidget *detail = gtk_expander_new(_("Ficha de estudio"));
	GtkWidget *content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gchar *cita = main_interlineal_cita_es(key);
	/* La tarjeta ya muestra forma, lema, glosa, Strong y análisis: el desplegable enseña solo lo enriquecido.
	 * Sin ficha enriquecida (palabra sin redacción) conserva los datos básicos para no quedar vacío. Lo que se
	 * copia es siempre la ficha completa, con el pasaje. */
	gchar *enr = ficha_enriquecida(f->strong, key, f->pos, TRUE);
	gchar *basic = main_interlineal_ficha_texto(f, NULL);
	gchar *text = main_interlineal_ficha_texto(f, cita);
	gchar *shown = enr ? g_strdup(enr) : g_strdup(basic);
	if (enr) {
		gchar *c = g_strdup_printf("%s\n\n%s", text, enr);

		g_free(text);
		text = c;
	}
	GtkWidget *label = gtk_label_new(shown);
	GtkWidget *copy = il_copy_button(_("Copiar ficha"), text);
	g_free(enr);
	g_free(basic);
	g_free(cita);
	g_free(shown);
	g_free(text);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_append(GTK_BOX(content), label);
	gtk_box_append(GTK_BOX(content), copy);
	if (main_interlineal_es_hebreo(f->forma)) {
		gchar *sin_signos = main_interlineal_sin_signos_hebreos(f->forma);
		gtk_box_append(GTK_BOX(content),
			il_copy_button(_("Copiar hebreo original"), f->forma));
		if (strcmp(sin_signos, f->forma))
			gtk_box_append(GTK_BOX(content),
				il_copy_button(_("Copiar hebreo sin signos"), sin_signos));
		g_free(sin_signos);
	}
	gtk_expander_set_child(GTK_EXPANDER(detail), content);
	gtk_box_append(GTK_BOX(item), row);
	gtk_box_append(GTK_BOX(item), detail);
	return item;
}

static GtkWidget *
il_header_row(gboolean reverse)
{
	GtkWidget *row, *a, *b, *c, *d;

	row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_widget_add_css_class(row, "il-hdr");
	a = il_label(reverse ? _("Español") : _("Griego / hebreo"),
		     "il-hdr-cell", FALSE);
	c = il_label(reverse ? _("Griego / hebreo") : _("Español"),
		     "il-hdr-cell", FALSE);
	gtk_box_append(GTK_BOX(row), il_text_cols(a, c));
	b = il_label(_("Strong's"), "il-hdr-cell", FALSE);
	gtk_widget_set_size_request(b, IL_COL_STRONG, -1);
	gtk_box_append(GTK_BOX(row), b);
	d = il_label(_("Análisis"), "il-hdr-cell", FALSE);
	gtk_widget_set_halign(d, GTK_ALIGN_END);
	gtk_label_set_xalign(GTK_LABEL(d), 1.0);
	gtk_widget_set_size_request(d, IL_COL_MORPH, -1);
	gtk_box_append(GTK_BOX(row), d);
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
		gui_widget_destroy(rows);
	rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_add_css_class(rows, "il-rows");
	gtk_box_append(GTK_BOX(rows), il_header_row(reverse));

	filas = main_interlineal_filas(key, reverse);
	for (l = filas, i = 0; l; l = l->next, i++) {
		GtkWidget *r = il_row_widget((InterlFila *)l->data, reverse, key);
		if (i % 2)
			gtk_widget_add_css_class(r, "il-row-alt");
		gtk_box_append(GTK_BOX(rows), r);
	}
	main_interlineal_filas_free(filas);
	gtk_widget_show(rows);
	gtk_box_append(GTK_BOX(box), rows);
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
	gtk_widget_add_css_class(box, "il-table");
	gtk_widget_add_css_class(box, settings.darktheme ? "il-dark" : "il-light");
	/* En modo lectura el interlineal vive dentro de la columna de
	 * lectura, no en su propia franja: se estiliza como parte de la
	 * página (borde suave, sin caja dura) y se enmarca en el ancho de
	 * lectura que il_table_fit() le da. */
	if (settings.reading_mode)
		gtk_widget_add_css_class(box, "il-reading");
	g_object_set_data_full(G_OBJECT(box), "il-key", g_strdup(key), g_free);
	gtk_widget_set_hexpand(box, TRUE);

	tabs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_add_css_class(tabs, "il-tabs");
	fwd = gtk_button_new_with_label(_("Griego/hebreo → Español"));
	rev = gtk_button_new_with_label(_("Español → griego/hebreo"));
	gtk_button_set_has_frame(GTK_BUTTON(fwd), FALSE);
	gtk_button_set_has_frame(GTK_BUTTON(rev), FALSE);
	gtk_widget_set_can_focus(fwd, FALSE);
	gtk_widget_set_can_focus(rev, FALSE);
	gtk_widget_add_css_class(fwd, "il-tab");
	gtk_widget_add_css_class(rev, "il-tab");
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
	gtk_box_append(GTK_BOX(tabs), fwd);
	gtk_box_append(GTK_BOX(tabs), rev);
	gtk_widget_show(tabs);
	gtk_box_append(GTK_BOX(box), tabs);

	{
		GtkWidget *pie = gtk_label_new("");
		gtk_widget_add_css_class(pie, "il-pie");
		gtk_label_set_xalign(GTK_LABEL(pie), 0.0);
		gtk_label_set_wrap(GTK_LABEL(pie), TRUE);
		gtk_widget_set_margin_start(pie, 12);
		gtk_widget_set_margin_end(pie, 12);
		gtk_widget_set_margin_top(pie, 4);
		gtk_widget_set_margin_bottom(pie, 2);
		gtk_box_append(GTK_BOX(box), pie);
		g_object_set_data(G_OBJECT(box), "il-pie", pie);
	}

	il_fill_rows(box, key, reverse);
	gtk_widget_show(box);
	return box;
}

G_MODULE_EXPORT void
on_interlineal_activate(gpointer menuitem, gpointer user_data)
{
	(void)user_data;
	if (syncing)
		return;
	gui_interlineal_set_active(GPOINTER_TO_INT(user_data) != 0);
}
