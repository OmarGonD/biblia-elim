/*
 * Biblia Elim — modos de apariencia (claro, oscuro, claroluna, pergamino, omarchy).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gui/elim_tema.h"
#include "gui/widgets.h"
#include "gui/interlineal.h"
#include "main/settings.h"
#include "main/xml.h"
#include "main/sword.h"
#include "main/interlineal.h"

typedef struct {
	const char *id;
	const char *label;
	gboolean dark;
	const char *bg;
	const char *fg;
	const char *navy;
	const char *navy_soft;
	const char *ink;
	const char *paper;
	const char *klass;
} TemaFijo;

static const TemaFijo temas_fijos[] = {
	{"claro", N_("Claro"), FALSE,
	 "#F7F4EE", "#1C1917", "#2C4A6E", "#5B7A9D", "#1C1917", "#F7F4EE",
	 NULL},
	{"oscuro", N_("Oscuro"), TRUE,
	 "#1A1C22", "#E8E4DC", "#8AA4C8", "#5B7A9D", "#E8E4DC", "#1A1C22",
	 "elim-dark"},
	{"claroluna", N_("Claro luna"), FALSE,
	 "#E4EAF2", "#243044", "#4A6A8A", "#7A9BB8", "#243044", "#E4EAF2",
	 "elim-luna"},
	{"pergamino", N_("Pergamino"), FALSE,
	 "#F3E6C9", "#3D2B1F", "#6B4F2A", "#A08050", "#3D2B1F", "#F3E6C9",
	 "elim-pergamino"},
	{NULL}
};

static GtkCssProvider *tema_css = NULL;
static GFileMonitor *omarchy_mon = NULL;
static gchar *omarchy_name = NULL;
static gchar *live_bg = NULL;
static gchar *live_fg = NULL;
static GtkWidget *radio_items[8];
static int radio_n = 0;
static gboolean applying = FALSE;
static gboolean sword_listo = FALSE;

static gchar *
toml_str(const char *txt, const char *key)
{
	gchar *pat;
	const char *p, *q, *r;

	if (!txt || !key)
		return NULL;
	pat = g_strdup_printf("\n%s", key);
	p = strstr(txt, pat);
	if (!p && g_str_has_prefix(txt, key))
		p = txt;
	else if (p)
		p++;
	g_free(pat);
	if (!p)
		return NULL;
	p += strlen(key);
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p != '=')
		return NULL;
	p++;
	while (*p == ' ' || *p == '\t')
		p++;
	if (*p == '"') {
		p++;
		q = strchr(p, '"');
		if (!q)
			return NULL;
		return g_strndup(p, q - p);
	}
	r = p;
	while (*r && *r != '\n' && *r != ' ' && *r != '\t' && *r != '#')
		r++;
	if (r == p)
		return NULL;
	return g_strndup(p, r - p);
}

static gchar *
omarchy_colors_path(void)
{
	const char *home = g_get_home_dir();
	gchar *p = g_build_filename(home, ".local", "state", "omarchy",
				    "current", "theme", "colors.toml", NULL);
	if (g_file_test(p, G_FILE_TEST_EXISTS))
		return p;
	g_free(p);
	p = g_build_filename(home, ".config", "omarchy", "themes",
			     omarchy_name ? omarchy_name : "maranatha",
			     "colors.toml", NULL);
	if (g_file_test(p, G_FILE_TEST_EXISTS))
		return p;
	g_free(p);
	return NULL;
}

static void
omarchy_refresh_name(void)
{
	gchar *path, *txt = NULL;

	g_free(omarchy_name);
	omarchy_name = NULL;
	path = g_build_filename(g_get_home_dir(), ".local", "state",
				"omarchy", "current", "theme.name", NULL);
	if (g_file_get_contents(path, &txt, NULL, NULL) && txt) {
		g_strstrip(txt);
		if (*txt)
			omarchy_name = g_strdup(txt);
		g_free(txt);
	}
	g_free(path);
}

static gchar *
css_escape_hash(const char *c)
{
	if (!c || *c != '#')
		return g_strdup("#808080");
	return g_strdup(c);
}

static void
clear_app_classes(void)
{
	GtkStyleContext *ctx;
	if (!widgets.app)
		return;
	ctx = gtk_widget_get_style_context(widgets.app);
	gtk_style_context_remove_class(ctx, "elim-dark");
	gtk_style_context_remove_class(ctx, "elim-luna");
	gtk_style_context_remove_class(ctx, "elim-pergamino");
	gtk_style_context_remove_class(ctx, "elim-omarchy");
}

static void
set_live_colors(const char *bg, const char *fg)
{
	g_free(live_bg);
	g_free(live_fg);
	live_bg = g_strdup(bg);
	live_fg = g_strdup(fg);
}

static void
push_css(const char *css)
{
	GdkScreen *scr;

	if (!tema_css)
		tema_css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(tema_css, css, -1, NULL);
	if (!g_object_get_data(G_OBJECT(tema_css), "attached")) {
		scr = gdk_screen_get_default();
		if (scr)
			gtk_style_context_add_provider_for_screen(
			    scr, GTK_STYLE_PROVIDER(tema_css),
			    GTK_STYLE_PROVIDER_PRIORITY_USER);
		g_object_set_data(G_OBJECT(tema_css), "attached",
				  GINT_TO_POINTER(1));
	}
}

static void
apply_fijo(const TemaFijo *t)
{
	gchar *css, *bg, *fg, *navy, *soft, *ink, *paper;

	bg = css_escape_hash(t->bg);
	fg = css_escape_hash(t->fg);
	navy = css_escape_hash(t->navy);
	soft = css_escape_hash(t->navy_soft);
	ink = css_escape_hash(t->ink);
	paper = css_escape_hash(t->paper);
	css = g_strdup_printf(
	    "@define-color elim_navy %s;\n"
	    "@define-color elim_navy_soft %s;\n"
	    "@define-color elim_ink %s;\n"
	    "@define-color elim_paper %s;\n"
	    "textview.elim-html, textview.elim-html text {\n"
	    "  background-color: %s;\n"
	    "  color: %s;\n"
	    "}\n",
	    navy, soft, ink, paper, bg, fg);
	push_css(css);
	g_free(css);
	g_free(bg);
	g_free(fg);
	g_free(navy);
	g_free(soft);
	g_free(ink);
	g_free(paper);
	set_live_colors(t->bg, t->fg);
	if (live_bg) {
		g_free(settings.bible_bg_color);
		settings.bible_bg_color = g_strdup(live_bg);
	}
	if (live_fg) {
		g_free(settings.bible_text_color);
		settings.bible_text_color = g_strdup(live_fg);
	}
	settings.darktheme = t->dark ? 1 : 0;
	clear_app_classes();
	if (widgets.app && t->klass)
		gtk_style_context_add_class(gtk_widget_get_style_context(widgets.app),
					    t->klass);
}

static gboolean
apply_omarchy(void)
{
	gchar *path, *txt = NULL, *mode, *bg, *fg, *acc, *paper, *css;
	gboolean dark = TRUE;

	omarchy_refresh_name();
	path = omarchy_colors_path();
	if (!path || !g_file_get_contents(path, &txt, NULL, NULL) || !txt) {
		g_free(path);
		g_free(txt);
		return FALSE;
	}
	mode = toml_str(txt, "mode");
	bg = toml_str(txt, "background");
	fg = toml_str(txt, "foreground");
	acc = toml_str(txt, "accent");
	paper = toml_str(txt, "lighter_background");
	if (mode && (!g_ascii_strcasecmp(mode, "light") ||
		     !g_ascii_strcasecmp(mode, "claro")))
		dark = FALSE;
	if (!bg)
		bg = g_strdup(dark ? "#1a1c22" : "#F7F4EE");
	if (!fg)
		fg = g_strdup(dark ? "#e8e4dc" : "#1C1917");
	if (!acc)
		acc = g_strdup(dark ? "#8AA4C8" : "#2C4A6E");
	if (!paper)
		paper = g_strdup(bg);

	css = g_strdup_printf(
	    "@define-color elim_navy %s;\n"
	    "@define-color elim_navy_soft %s;\n"
	    "@define-color elim_ink %s;\n"
	    "@define-color elim_paper %s;\n"
	    "window, headerbar, .elim-navbar, .elim-sidebar, .elim-tabstrip {\n"
	    "  background-color: %s;\n"
	    "  color: %s;\n"
	    "}\n"
	    "textview.elim-html, textview.elim-html text {\n"
	    "  background-color: %s;\n"
	    "  color: %s;\n"
	    "}\n",
	    acc, acc, fg, paper, paper, fg, bg, fg);
	push_css(css);
	g_free(css);
	set_live_colors(bg, fg);
	if (live_bg) {
		g_free(settings.bible_bg_color);
		settings.bible_bg_color = g_strdup(live_bg);
	}
	if (live_fg) {
		g_free(settings.bible_text_color);
		settings.bible_text_color = g_strdup(live_fg);
	}
	settings.darktheme = dark ? 1 : 0;
	clear_app_classes();
	if (widgets.app) {
		GtkStyleContext *ctx = gtk_widget_get_style_context(widgets.app);
		gtk_style_context_add_class(ctx, "elim-omarchy");
		if (dark)
			gtk_style_context_add_class(ctx, "elim-dark");
	}
	g_free(path);
	g_free(txt);
	g_free(mode);
	g_free(bg);
	g_free(fg);
	g_free(acc);
	g_free(paper);
	return TRUE;
}

static const TemaFijo *
fijo_by_id(const char *id)
{
	int i;
	for (i = 0; temas_fijos[i].id; i++)
		if (!g_strcmp0(temas_fijos[i].id, id))
			return &temas_fijos[i];
	return NULL;
}

static void
redisplay_text(void)
{
	if (!sword_listo || !widgets.html_text || !settings.currentverse)
		return;
	main_display_bible(NULL, settings.currentverse);
}

static void
sync_radios(void)
{
	int i;
	const char *mode = settings.ui_mode ? settings.ui_mode : "omarchy";

	applying = TRUE;
	for (i = 0; i < radio_n; i++) {
		const char *id = g_object_get_data(G_OBJECT(radio_items[i]), "tema-id");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(radio_items[i]),
					       !g_strcmp0(id, mode));
	}
	applying = FALSE;
}

static void
on_omarchy_changed(GFileMonitor *m, GFile *f, GFile *o, GFileMonitorEvent e,
		   gpointer data)
{
	(void)m;
	(void)f;
	(void)o;
	(void)data;
	if (e != G_FILE_MONITOR_EVENT_CHANGED &&
	    e != G_FILE_MONITOR_EVENT_CREATED &&
	    e != G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
		return;
	if (g_strcmp0(settings.ui_mode, "omarchy") == 0)
		gui_elim_tema_aplicar();
}

static void
watch_omarchy(void)
{
	gchar *path;
	GFile *gf;

	if (omarchy_mon)
		return;
	path = g_build_filename(g_get_home_dir(), ".local", "state",
				"omarchy", "current", "theme.name", NULL);
	gf = g_file_new_for_path(path);
	omarchy_mon = g_file_monitor_file(gf, G_FILE_MONITOR_NONE, NULL, NULL);
	if (omarchy_mon)
		g_signal_connect(omarchy_mon, "changed",
				 G_CALLBACK(on_omarchy_changed), NULL);
	g_object_unref(gf);
	g_free(path);
}

void
gui_elim_tema_aplicar(void)
{
	const char *mode = settings.ui_mode ? settings.ui_mode : "omarchy";
	const TemaFijo *t;

	if (!g_strcmp0(mode, "omarchy")) {
		if (!apply_omarchy()) {
			t = fijo_by_id("oscuro");
			if (t)
				apply_fijo(t);
		}
	} else {
		t = fijo_by_id(mode);
		if (!t)
			t = &temas_fijos[0];
		apply_fijo(t);
	}

	g_object_set(gtk_settings_get_default(),
		     "gtk-application-prefer-dark-theme",
		     settings.darktheme, NULL);
	xml_set_or_create_value("misc", "darktheme",
				settings.darktheme ? "1" : "0");
	if (widgets.app)
		gtk_widget_queue_draw(widgets.app);
	redisplay_text();
}

void
gui_elim_tema_set(const char *mode)
{
	if (!mode || !*mode)
		mode = "omarchy";
	g_free(settings.ui_mode);
	settings.ui_mode = g_strdup(mode);
	xml_set_or_create_value("misc", "ui_mode", settings.ui_mode);
	gui_elim_tema_aplicar();
	sync_radios();
}

const char *
gui_elim_tema_bg(void)
{
	if (live_bg)
		return live_bg;
	return settings.bible_bg_color ? settings.bible_bg_color : "#F7F4EE";
}

const char *
gui_elim_tema_fg(void)
{
	if (live_fg)
		return live_fg;
	return settings.bible_text_color ? settings.bible_text_color : "#1C1917";
}

static void
on_tema_activate(GtkCheckMenuItem *item, gpointer data)
{
	const char *id = data;
	if (applying)
		return;
	if (!gtk_check_menu_item_get_active(item))
		return;
	gui_elim_tema_set(id);
}

void
gui_elim_tema_bind_menu(GtkBuilder *gxml)
{
	static const char *ids[] = {
		"tema_omarchy", "tema_claro", "tema_oscuro",
		"tema_claroluna", "tema_pergamino", NULL
	};
	static const char *modes[] = {
		"omarchy", "claro", "oscuro", "claroluna", "pergamino"
	};
	int i;

	radio_n = 0;
	for (i = 0; ids[i]; i++) {
		GtkWidget *w = GTK_WIDGET(gtk_builder_get_object(gxml, ids[i]));
		if (!w)
			continue;
		g_object_set_data(G_OBJECT(w), "tema-id", (gpointer)modes[i]);
		g_signal_connect(w, "toggled", G_CALLBACK(on_tema_activate),
				 (gpointer)modes[i]);
		radio_items[radio_n++] = w;
	}
	sync_radios();
}

void
gui_elim_tema_init(void)
{
	watch_omarchy();
	gui_elim_tema_aplicar();
}

void
gui_elim_tema_marcar_listo(void)
{
	sword_listo = TRUE;
}
