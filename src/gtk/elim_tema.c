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
	const char *bg;	    /* fondo del texto biblico */
	const char *fg;
	const char *navy;   /* acento */
	const char *navy_soft;
	const char *ink;
	const char *paper;
	const char *chrome; /* ventana, barra de titulo, navegacion, pestanas */
	const char *panel;  /* barra lateral */
	const char *menu;   /* desplegables y menus contextuales */
	const char *klass;
} TemaFijo;

/* El cromo, el panel y el menu no se deducian de nada: cada tema tocaba
 * solo el texto biblico y dejaba el resto -- ventana, barras y menus --
 * al tema de GTK del escritorio, asi que "pergamino" salia con la
 * cabecera gris de Adwaita y el menu del sistema.  Ahora cada tema trae
 * su paleta entera. */
static const TemaFijo temas_fijos[] = {
	{"claro", N_("Claro"), FALSE,
	 "#F7F4EE", "#1C1917", "#2C4A6E", "#5B7A9D", "#1C1917", "#F7F4EE",
	 "#EFEAE2", "#E6E0D5", "#FBF9F5",
	 "elim-claro"},
	{"oscuro", N_("Oscuro"), TRUE,
	 "#1A1C22", "#E8E4DC", "#8AA4C8", "#5B7A9D", "#E8E4DC", "#1A1C22",
	 "#22252D", "#262A33", "#2B2F3A",
	 "elim-dark"},
	{"claroluna", N_("Claro luna"), FALSE,
	 "#E4EAF2", "#243044", "#4A6A8A", "#7A9BB8", "#243044", "#E4EAF2",
	 "#DCE4EE", "#D2DCEA", "#F1F5FB",
	 "elim-luna"},
	{"pergamino", N_("Pergamino"), FALSE,
	 "#F3E6C9", "#3D2B1F", "#6B4F2A", "#A08050", "#3D2B1F", "#F3E6C9",
	 "#EAD9B4", "#E2CFA6", "#F8F0DC",
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
	gtk_style_context_remove_class(ctx, "elim-claro");
	gtk_style_context_remove_class(ctx, "elim-dark");
	gtk_style_context_remove_class(ctx, "elim-luna");
	gtk_style_context_remove_class(ctx, "elim-pergamino");
	gtk_style_context_remove_class(ctx, "elim-omarchy");
	/* elim-app la lleva siempre: es la que engancha el color del cromo,
	 * y va en la ventana y no en "window" a secas porque los menus son
	 * ventanas aparte y se pintarian con el mismo fondo opaco, sin
	 * esquinas ni sombra. */
	gtk_style_context_add_class(ctx, "elim-app");
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

/* Vestir los diálogos con el tema.
 *
 * La ventana principal lleva la clase .elim-app desde que se aplica el
 * tema, pero los diálogos —preferencias, búsqueda, gestor de módulos,
 * los "acerca de"— son ventanas propias y nacen sin ella: salían con el
 * gris de Adwaita en mitad de un pergamino.
 *
 * Se cogen por donde pasan todos, la señal "map" de GtkWidget. Un gancho
 * de emisión ahorra tener que acordarse de la clase en cada uno de los
 * veintitantos sitios que abren un diálogo, y alcanza también a los que
 * se crean después de haber cambiado de tema.
 *
 * Las emergentes quedan fuera a propósito: los menús y los mensajes
 * flotantes son GTK_WINDOW_POPUP y llevan sus propios colores, que se
 * definen sin selector de tema porque es lo único que les llega.
 */
static void
vestir_ventana(GtkWidget *w)
{
	if (!GTK_IS_WINDOW(w) ||
	    gtk_window_get_window_type(GTK_WINDOW(w)) != GTK_WINDOW_TOPLEVEL)
		return;
	gtk_style_context_add_class(gtk_widget_get_style_context(w),
				    "elim-app");
}

static gboolean
vestir_al_mapear(GSignalInvocationHint *hint, guint n_valores,
		 const GValue *valores, gpointer data)
{
	(void)hint;
	(void)n_valores;
	(void)data;
	vestir_ventana(GTK_WIDGET(g_value_get_object(valores)));
	return TRUE; /* el gancho se queda puesto */
}

static void
vestir_ventanas(void)
{
	static gboolean puesto = FALSE;
	GList *lista, *n;

	if (puesto)
		return;
	puesto = TRUE;
	g_signal_add_emission_hook(g_signal_lookup("map", GTK_TYPE_WIDGET), 0,
				   vestir_al_mapear, NULL, NULL);
	/* Las que ya estuvieran abiertas cuando esto arranca. */
	lista = gtk_window_list_toplevels();
	for (n = lista; n; n = n->next)
		vestir_ventana(GTK_WIDGET(n->data));
	g_list_free(lista);
}

/* Alinear el tema de GTK con el modo elegido.
 *
 * "gtk-application-prefer-dark-theme" no bastaba: es una preferencia, y
 * un tema que ya es oscuro -- aquí el del escritorio es Adwaita-dark --
 * lo sigue siendo con la preferencia puesta o quitada. El resultado era
 * que "claro" y "pergamino" dejaban la ventana clara pero los botones,
 * las cajas de texto y los selectores negros.
 *
 * Del tema sólo se conoce el nombre, así que la dirección se deduce del
 * sufijo, que es la convención que siguen Adwaita, Arc, Materia y los
 * demás. Si el nombre construido no existe, GTK cae en el suyo por
 * defecto, que junto a la preferencia de arriba también sirve.
 */
static void
alinear_tema_gtk(gboolean dark)
{
	GtkSettings *gs = gtk_settings_get_default();
	gchar *actual = NULL;
	gboolean es_oscuro;

	if (!gs)
		return;
	g_object_set(gs, "gtk-application-prefer-dark-theme", dark, NULL);
	g_object_get(gs, "gtk-theme-name", &actual, NULL);
	if (!actual)
		return;

	es_oscuro = g_str_has_suffix(actual, "-dark") ||
		    g_str_has_suffix(actual, "-Dark");
	if (es_oscuro != dark) {
		gchar *quiero;

		if (dark)
			quiero = g_strconcat(actual, "-dark", NULL);
		else
			quiero = g_strndup(actual, strlen(actual) - 5);
		g_object_set(gs, "gtk-theme-name", quiero, NULL);
		g_free(quiero);
	}
	g_free(actual);
}

/* Una sola paleta para todos los modos.
 *
 * Los menús desplegables y los contextuales son ventanas aparte: no
 * descienden de la ventana principal, así que ninguna regla colgada de
 * la clase del tema los alcanza. Por eso sus colores se definen aquí
 * como colores con nombre, sin selector de tema, y las reglas que los
 * usan viven en ui/xiphos-style.css.
 */
static void
push_palette(const char *bg, const char *fg, const char *acc,
	     const char *acc_soft, const char *chrome, const char *panel,
	     const char *menu, gboolean dark)
{
	gchar *css;
	gchar *c_bg = css_escape_hash(bg);
	gchar *c_fg = css_escape_hash(fg);
	gchar *c_acc = css_escape_hash(acc);
	gchar *c_soft = css_escape_hash(acc_soft);
	gchar *linea = dark ? g_strdup("alpha(#ffffff, 0.11)")
			    : g_strdup_printf("alpha(%s, 0.13)", c_fg);
	/* El acento de un tema oscuro es claro, y al revés: el texto que se
	 * pone encima tiene que ir al contrario. */
	gchar *sel_fg = dark ? g_strdup(chrome) : g_strdup("#ffffff");

	css = g_strdup_printf(
	    "@define-color elim_navy %s;\n"
	    "@define-color elim_navy_soft %s;\n"
	    "@define-color elim_ink %s;\n"
	    "@define-color elim_paper %s;\n"
	    "@define-color elim_chrome %s;\n"
	    "@define-color elim_panel %s;\n"
	    "@define-color elim_menu_bg %s;\n"
	    "@define-color elim_menu_sel %s;\n"
	    "@define-color elim_menu_sel_fg %s;\n"
	    "@define-color elim_line %s;\n"
	    ".elim-app {\n"
	    "  background-color: @elim_chrome;\n"
	    "  color: @elim_ink;\n"
	    "}\n"
	    "textview.elim-html, textview.elim-html text {\n"
	    "  background-color: %s;\n"
	    "  color: %s;\n"
	    "}\n",
	    c_acc, c_soft, c_fg, c_bg, chrome, panel, menu, c_acc, sel_fg,
	    linea, c_bg, c_fg);
	push_css(css);
	g_free(css);
	g_free(c_bg);
	g_free(c_fg);
	g_free(c_acc);
	g_free(c_soft);
	g_free(linea);
	g_free(sel_fg);
}

static void
guardar_colores_biblia(void)
{
	if (live_bg) {
		g_free(settings.bible_bg_color);
		settings.bible_bg_color = g_strdup(live_bg);
	}
	if (live_fg) {
		g_free(settings.bible_text_color);
		settings.bible_text_color = g_strdup(live_fg);
	}
}

static void
apply_fijo(const TemaFijo *t)
{
	push_palette(t->bg, t->fg, t->navy, t->navy_soft, t->chrome, t->panel,
		     t->menu, t->dark);
	set_live_colors(t->bg, t->fg);
	guardar_colores_biblia();
	settings.darktheme = t->dark ? 1 : 0;
	clear_app_classes();
	if (widgets.app && t->klass)
		gtk_style_context_add_class(gtk_widget_get_style_context(widgets.app),
					    t->klass);
}

static gboolean
apply_omarchy(void)
{
	gchar *path, *txt = NULL, *mode, *bg, *fg, *acc, *paper;
	gchar *panel, *menu;
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

	/* El colors.toml sólo da fondo, texto y acento; el panel y el menú
	 * se separan del cromo lo justo para que se distingan de él. */
	panel = g_strdup_printf("shade(%s, %s)", paper, dark ? "1.14" : "0.96");
	menu = g_strdup_printf("shade(%s, %s)", paper, dark ? "1.26" : "1.04");
	push_palette(bg, fg, acc, acc, paper, panel, menu, dark);
	g_free(panel);
	g_free(menu);

	set_live_colors(bg, fg);
	guardar_colores_biblia();
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

	alinear_tema_gtk(settings.darktheme ? TRUE : FALSE);
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

/* La fuente de la interfaz: menús, pestañas, paneles y diálogos.
 *
 * Vacía significa "la que diga el escritorio", que es como se comportaba
 * la aplicación antes de que esto se pudiera elegir. El texto bíblico no
 * pasa por aquí: va por fonts.conf, para poder leer con una serif sin
 * arrastrar a los menús a ella. */
void
gui_elim_fuente_app_aplicar(const gchar *font)
{
	GtkSettings *gs = gtk_settings_get_default();

	if (!gs)
		return;
	if (font && *font)
		g_object_set(gs, "gtk-font-name", font, NULL);
	else
		gtk_settings_reset_property(gs, "gtk-font-name");
}

void
gui_elim_tema_init(void)
{
	vestir_ventanas();
	watch_omarchy();
	gui_elim_tema_aplicar();
	gui_elim_fuente_app_aplicar(settings.app_font);
}

void
gui_elim_tema_marcar_listo(void)
{
	sword_listo = TRUE;
}
