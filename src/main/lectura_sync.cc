/*
 * Biblia Elim — lectura sincronizada (versión inferior)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <swmodule.h>
#include <versekey.h>
#include <swbuf.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "backend/sword_main.hh"
#include "gui/bibletext.h"
#include "gui/lectura_sync.h"
#include "gui/utilities.h"
#include "gui/widgets.h"
#include "main/lectura_sync.h"
#include "main/lists.h"
#include "main/settings.h"
#include "main/sword.h"

using namespace sword;

static gchar *
css_color(const char *c, const char *fallback)
{
	if (!c || !*c)
		return g_strdup(fallback);
	if (c[0] == '#')
		return g_strdup(c);
	if (c[0] == '0' && (c[1] == 'x' || c[1] == 'X') && strlen(c) >= 8)
		return g_strdup_printf("#%s", c + 2);
	return g_strdup(c);
}

/* Mezcla dos colores "#RRGGBB" -- para un separador entre versiones
 * que sea visible pero sutil, sin importar el tema activo. */
static gchar *
blend_hex_color(const gchar *base, const gchar *toward, double ratio)
{
	guint br = 0, bg = 0, bb = 0, tr = 0, tg = 0, tb = 0;

	if (!base || base[0] != '#' || strlen(base) < 7 ||
	    !toward || toward[0] != '#' || strlen(toward) < 7)
		return g_strdup(base ? base : "#808080");
	if (sscanf(base + 1, "%02x%02x%02x", &br, &bg, &bb) != 3)
		return g_strdup(base);
	if (sscanf(toward + 1, "%02x%02x%02x", &tr, &tg, &tb) != 3)
		return g_strdup(base);
	if (ratio < 0.0)
		ratio = 0.0;
	if (ratio > 1.0)
		ratio = 1.0;
	return g_strdup_printf("#%02x%02x%02x",
			      (guint)(br + (tr - (double)br) * ratio),
			      (guint)(bg + (tg - (double)bg) * ratio),
			      (guint)(bb + (tb - (double)bb) * ratio));
}

static gchar *
esc_con_saltos(const char *plain)
{
	gchar *esc = g_markup_escape_text(plain ? plain : "", -1);
	gchar **parts = g_strsplit(esc, "\n", -1);
	gchar *out;

	g_free(esc);
	out = g_strjoinv("<br/>", parts);
	g_strfreev(parts);
	return out;
}

gchar *
main_lectura_sync_default_module(void)
{
	/* TEXT_LIST is already Bible texts. Do not touch `backend` here:
	 * settings_init() calls this after main_init_lists(), which
	 * deletes the temporary BackEnd and leaves backend == NULL. */
	GList *bibles = get_list(TEXT_LIST);
	for (GList *l = bibles; l; l = l->next) {
		const char *name = (const char *)l->data;
		if (!name)
			continue;
		if (settings.MainWindowModule &&
		    !strcmp(name, settings.MainWindowModule))
			continue;
		return g_strdup(name);
	}
	if (settings.MainWindowModule)
		return g_strdup(settings.MainWindowModule);
	if (bibles && bibles->data)
		return g_strdup((const char *)bibles->data);
	return NULL;
}

static const char *
desc_de_modulo(const char *name)
{
	GList *b = get_list(TEXT_LIST);
	GList *d = get_list(TEXT_DESC_LIST);

	for (; b; b = b->next, d = d ? d->next : NULL) {
		if (b->data && name && !strcmp((const char *)b->data, name))
			return (d && d->data) ? (const char *)d->data : name;
	}
	return name ? name : "";
}

typedef struct {
	const char *head_bg;
	const char *head_fg;
	const char *row_bg;
	const char *row_fg;
} BandaCmp;

static const BandaCmp bandas_oscuro[] = {
	{"#3D3428", "#F6EBD4", "#2C261C", "#F6EBD4"},
	{"#243044", "#E8EEF4", "#1C2430", "#E8EEF4"},
	{"#24382C", "#E8F4EC", "#1C2822", "#E8F4EC"},
	{"#382434", "#F4E8F0", "#281C26", "#F4E8F0"},
};

static const BandaCmp bandas_claro[] = {
	{"#E4D4A8", "#1A140C", "#F6EEDC", "#1A140C"},
	{"#C8D6E6", "#1A2430", "#E8EEF4", "#1A2430"},
	{"#C8E0D0", "#1A281C", "#E8F2EA", "#1A281C"},
	{"#E4CCD8", "#28141E", "#F4E8EE", "#28141E"},
};

static void
append_un_versiculo(GString *html, const char *mod_name, const char *key_text,
		    int slot)
{
	SWModule *mod;
	VerseKey *vk;
	SWBuf saved;
	const char *plain;
	gchar *esc, *de;
	int v;
	const BandaCmp *b;

	if (slot < 0)
		slot = 0;
	if (slot > 3)
		slot = 3;
	b = settings.darktheme ? &bandas_oscuro[slot] : &bandas_claro[slot];

	if (!mod_name || !*mod_name || !backend->is_module(mod_name)) {
		g_string_append_printf(html,
				       "<p class=\"miss\">%s</p>",
				       _("Módulo no disponible."));
		return;
	}
	mod = backend->get_SWModule(mod_name);
	if (!mod)
		return;
	saved = mod->getKey()->getText();
	vk = (VerseKey *)mod->createKey();
	vk->setAutoNormalize(1);
	vk->setText(key_text);
	v = vk->getVerse();
	mod->setKey(*vk);
	delete vk;

	de = g_markup_escape_text(desc_de_modulo(mod_name), -1);
	g_string_append_printf(html,
			       "<p class=\"modh\" style=\"background-color:%s;color:%s\">%s</p>",
			       b->head_bg, b->head_fg, de);
	g_free(de);

	plain = mod->stripText();
	if (plain && *plain) {
		esc = esc_con_saltos(plain);
		g_string_append_printf(html,
				       "<p class=\"cur\" style=\"background-color:%s;color:%s\">"
				       "<span class=\"v\">%d</span> %s</p>",
				       b->row_bg, b->row_fg, v, esc);
		g_free(esc);
	} else {
		g_string_append_printf(html,
				       "<p class=\"miss\" style=\"background-color:%s;color:%s\">%s</p>",
				       b->row_bg, b->row_fg,
				       _("Este versículo no está en esta versión."));
	}
	mod->setKeyText(saved.c_str());
}

/* Renders the focused verse in every Comparar version (up to 4). */
static void
lectura_sync_render_for(const char *key_text)
{
	gchar *bg, *fg;
	gchar **mods;
	int pct, i;
	gboolean any = FALSE;

	if (!settings.show_lectura_sync)
		return;
	if (gui_lectura_sync_ficha_activa())
		return;
	if (!backend)
		return;
	if (!widgets.html_lectura_sync ||
	    !gtk_widget_get_realized(widgets.html_lectura_sync))
		return;
	if (!key_text)
		return;

	gui_lectura_sync_rellenar_combo();
	gui_lectura_sync_set_ref(key_text);

	bg = css_color(settings.bible_bg_color, "#ffffff");
	fg = css_color(settings.bible_text_color, "#222222");

	pct = 100 + settings.base_font_size * 12;
	if (pct < 85)
		pct = 85;
	if (pct > 160)
		pct = 160;

	/* raya fina entre una versión y la siguiente -- mezcla sutil hacia
	 * el color de texto, igual en cualquier tema. Nota: el <style>
	 * de abajo es puramente decorativo para quien lea el HTML -- el
	 * parser propio de la app (wk-html.c) ignora por completo los
	 * bloques <style> (ver el `return` para "style"/"head" al
	 * principio de walk_element()), así que el separador real se
	 * inserta como <hr> explícito en el bucle de abajo, no por CSS. */
	gchar *divider = blend_hex_color(bg, fg, 0.30);

	GString *html = g_string_new(NULL);
	g_string_append_printf(html,
			       "<html><head><meta charset=\"utf-8\"/>"
			       "<style>"
			       "body{margin:0;padding:10px 12px;background:%s;color:%s;"
			       "font-family:'Noto Serif','DejaVu Serif','Liberation Serif',serif;"
			       "font-size:%d%%;line-height:1.5;}"
			       "p.modh{font-family:'Noto Sans','DejaVu Sans',sans-serif;"
			       "font-size:.78em;font-weight:700;letter-spacing:.05em;"
			       "padding:7px 12px;margin:8px 0 0;}"
			       "p.modh:first-child{margin-top:0;}"
			       ".v{font-size:.72em;font-weight:700;"
			       "vertical-align:super;padding-right:.4em;opacity:.8;}"
			       "p.cur{font-weight:500;padding:10px 12px;margin:0 0 6px;}"
			       "p.miss{opacity:.7;padding:10px 12px;margin:0 0 6px;}"
			       "</style></head><body>",
			       bg, fg, pct);

	mods = settings.LecturaSyncModule
		   ? g_strsplit(settings.LecturaSyncModule, ",", 4)
		   : NULL;
	for (i = 0; mods && mods[i]; i++) {
		int j;
		gboolean dup = FALSE;

		g_strstrip(mods[i]);
		if (!mods[i][0])
			continue;
		for (j = 0; j < i; j++) {
			if (mods[j] && !strcmp(mods[j], mods[i])) {
				dup = TRUE;
				break;
			}
		}
		if (dup)
			continue;
		if (any)
			g_string_append_printf(html, "<hr color=\"%s\">", divider);
		append_un_versiculo(html, mods[i], key_text, i);
		any = TRUE;
	}
	g_strfreev(mods);
	g_free(divider);
	if (!any)
		g_string_append_printf(html, "<p class=\"miss\">%s</p>",
				       _("Elige una o más Biblias para comparar este versículo."));

	g_string_append(html, "</body></html>");
	HtmlOutput(html->str, widgets.html_lectura_sync, NULL, NULL);
	g_string_free(html, TRUE);
	g_free(bg);
	g_free(fg);
}

void
main_lectura_sync_actualizar(void)
{
	if (!settings.currentverse)
		return;
	lectura_sync_render_for(settings.currentverse);
	/* keep the main pane's reading-focus highlight matched to whatever
	 * verse the user actually navigated to -- this runs on every
	 * navigation while the panel is open, not just when it first
	 * opens, so typing a new reference moves the highlight there too
	 * instead of leaving it wherever a scroll last left it. */
	gui_bibletext_lectura_sync_focus_current();
}

void
main_lectura_sync_focus_verse(const gchar *key_text)
{
	lectura_sync_render_for(key_text);
}
