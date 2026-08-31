/*
 * Biblia Elim — lectura sincronizada (versión inferior)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

/* Renders just the one verse `key_text` points at -- not the whole
 * chapter with it marked -- since this pane's whole job is "show me
 * this specific verse's equivalent in the other version", following
 * whatever verse is focused (centered) in the main pane above. */
static void
lectura_sync_render_for(const char *key_text)
{
	gchar *bg, *fg, *vnum, *hl_bg, *hl_fg;
	int pct;
	const char *plain;

	if (!settings.show_lectura_sync)
		return;
	if (gui_lectura_sync_ficha_activa())
		return;
	if (!backend)
		return;
	if (!widgets.html_lectura_sync ||
	    !gtk_widget_get_realized(widgets.html_lectura_sync))
		return;
	if (!settings.LecturaSyncModule || !key_text)
		return;
	if (!backend->is_module(settings.LecturaSyncModule))
		return;

	SWModule *mod = backend->get_SWModule(settings.LecturaSyncModule);
	if (!mod)
		return;

	gui_lectura_sync_rellenar_combo();
	gui_lectura_sync_set_ref(key_text);

	SWBuf saved = mod->getKey()->getText();
	VerseKey *vk = (VerseKey *)mod->createKey();
	vk->setAutoNormalize(1);
	vk->setText(key_text);
	int v = vk->getVerse();
	mod->setKey(*vk);
	delete vk;

	bg = css_color(settings.bible_bg_color, "#ffffff");
	fg = css_color(settings.bible_text_color, "#222222");
	vnum = css_color(settings.bible_verse_num_color, "#888888");
	hl_bg = css_color(settings.highlight_bg, "#fff3bf");
	hl_fg = css_color(settings.highlight_fg, "#111111");

	pct = 100 + settings.base_font_size * 12;
	if (pct < 85)
		pct = 85;
	if (pct > 160)
		pct = 160;

	GString *html = g_string_new(NULL);
	g_string_append_printf(html,
			       "<html><head><meta charset=\"utf-8\"/>"
			       "<style>"
			       "body{margin:0;padding:16px;background:%s;color:%s;"
			       "font-family:'Noto Serif','DejaVu Serif','Liberation Serif',serif;"
			       "font-size:%d%%;line-height:1.6;}"
			       ".v{color:%s;font-size:.72em;font-weight:700;"
			       "vertical-align:super;padding-right:.4em;}"
			       /* deliberately bolder than plain body text --
			        * matches the reading-focus highlight above. */
			       "p.cur{background:%s;color:%s;font-weight:600;"
			       "font-size:1.12em;border-radius:6px;"
			       "box-shadow:inset 3px 0 0 %s;padding:10px 12px;margin:0;}"
			       "</style></head><body>",
			       bg, fg, pct, vnum,
			       hl_bg, hl_fg, hl_fg);

	plain = mod->stripText();
	if (plain && *plain) {
		gchar *esc = esc_con_saltos(plain);
		g_string_append_printf(html,
				       "<p class=\"cur\"><span class=\"v\">%d</span> %s</p>",
				       v, esc);
		g_free(esc);
	} else {
		g_string_append(html,
				"<p style=\"opacity:.65\">"
				"Este versículo no está en esta versión.</p>");
	}

	g_string_append(html, "</body></html>");
	mod->setKeyText(saved.c_str());

	HtmlOutput(html->str, widgets.html_lectura_sync, NULL, NULL);
	g_string_free(html, TRUE);
	g_free(bg);
	g_free(fg);
	g_free(vnum);
	g_free(hl_bg);
	g_free(hl_fg);
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
