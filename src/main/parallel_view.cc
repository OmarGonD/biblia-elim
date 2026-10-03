/*
 * Xiphos Bible Study Tool
 * parallel_view.cc - support for displaying multiple modules
 *
 * Copyright (C) 2004-2026 Xiphos Developer Team
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

#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include <gtk/gtk.h>

#include <swmodule.h>
#include <swkey.h>
#include <versekey.h>

#include "backend/sword_main.hh"
#include "main/backend_access.h"
#include "main/strong_interaction.h"
#include "main/gtk_compat.h"

#include "gui/parallel_view.h"
#include "gui/parallel_dialog.h"
#include "gui/parallel_tab.h"
#include "gui/dialog.h"
#include "gui/utilities.h"
#include "gui/widgets.h"

#include "main/parallel_view.h"
#include "main/global_ops.hh"
#include "main/lists.h"
#include "main/sword.h"
#include "main/settings.h"
#include "main/url.hh"
#include "main/xml.h"
#include "main/display.hh"

#include "backend/sword/sword_backend.h"

#include "xiphos_html/xiphos_html.h"

#include "gui/debug_glib_null.h"

#define HTML_START \
	"<html><head><meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\" />\
<style type=\"text/css\">\
A { text-decoration:none } \
*[dir=rtl] { text-align: right; }\
h3 { font-style: %s }\
%s %s %s \
</style></head>"
// last 3 "%s" are for CSS init from getRenderHeader() sticky modnames, and margin justify.

// how to build a table whose top row of modname labels doesn't scroll. based on:
// http://geeksforgeeks.org/how-to-create-a-table-with-fixed-header-and-scrollable-body/
#define	STICKY_MODNAMES \
"<style> \
.table-container { overflow-y: auto; width: 100%; } \
table { width: 100%; border-collapse: collapse; } \
thead th { position: sticky; top: 0; z-index: 1; } \
th { text-align: center; } \
</style>"

extern GtkWidget *entrycbIntBook;
extern GtkWidget *sbIntChapter;
extern GtkWidget *sbIntVerse;
extern GtkWidget *entryIntLookup;

SWBuf unknown_parallel = _("Unknown parallel module: ");

// for alternation color ease.
gchar *white = "#FFFFFF", *grey  = "#606060", *bg_choice;

/* Fondo alternado de fila, atenuado y coherente con el tema activo.
 * La versión anterior usaba directamente bible_text_color como fondo
 * de la fila impar (con el texto invertido a bible_bg_color encima) --
 * un truco de "negativo fotográfico" que en el tema oscuro de Biblia
 * Elim (texto claro, ~#e8e4dc) pintaba franjas color crema sobre
 * fondo oscuro, y como el enlace "ver contexto" no recibía ese color
 * invertido, terminaba quedando casi invisible (texto claro sobre
 * fondo también claro). Mezclar apenas el fondo hacia el color del
 * texto -- en vez de reemplazarlo entero -- da una franja sutil que
 * funciona igual en claro y en oscuro, sin necesitar invertir nada. */
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

/******************************************************************************
 * static
 */

BackEnd *backend_p;
extern const gchar *no_content;

static const gchar *tf2of(int true_false)
{
	if (true_false)
		return "On";
	else
		return "Off";
}

/******************************************************************************
 * Name
 *   set_global_option
 *
 * Synopsis
 *   #include "main/parallel_view.h"
 *
 *   void set_global_option(char * option, gboolean choice)
 *
 * Description
 *   sets a sword global option and saves it to settings.xml
 *
 *
 *
 * Return value
 *   void
 */

static void set_global_option(char *option, gboolean choice)
{
	const char *on_off = tf2of(choice);
	SWMgr *mgr = backend_p->get_mgr();
	char *buf = g_strdup(option);


	mgr->setGlobalOption(buf, on_off);

	xml_set_value("Xiphos",
		      "parallel",
		      (char *)g_strdelimit(buf, "' ", '_'),
		      (char *)(choice ? "1" : "0"));
	g_free(buf);
}

static void set_global_textual_reading(const char *option, int choice)
{
	char *buf = g_strdup(option);
	SWMgr *mgr = backend_p->get_mgr();

	xml_set_value("Xiphos",
		      "parallel",
		      (char *)g_strdelimit(buf, "' ", '_'),
		      (char *)(choice ? "1" : "0"));

	XI_message(("set_global_textual_reading\noption://%s", option));

	mgr->setGlobalOption("Textual Variants", option);

	g_free(buf);
}

/******************************************************************************
 * Name
 *   gui_set_parallel_module_global_options
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void gui_set_parallel_module_global_options(gchar *option,
 *						gboolean choice)
 *
 * Description
 *   user checked or unchecked parallel option menu item
 *   set settings.parallel_<item> and call main_update_parallel_page()
 *   to display change.
 *
 * Return value
 *   void
 */

static void apply_parallel_option(const gchar *name, gboolean choice)
{
	gchar *option = (gchar *)name; /* the setters predate const */

	if (!strcmp(option, "Strong's Numbers")) {
		settings.parallel_strongs = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Footnotes")) {
		settings.parallel_footnotes = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Morphological Tags")) {
		settings.parallel_morphs = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Hebrew Vowel Points")) {
		settings.parallel_hebrewpoints = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Hebrew Cantillation")) {
		settings.parallel_cantillationmarks = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Greek Accents")) {
		settings.parallel_greekaccents = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Cross-references")) {
		settings.parallel_crossref = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Transliteration")) {
		settings.parallel_transliteration = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Words of Christ in Red")) {
		settings.parallel_red_words = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Morpheme Segmentation")) {
		settings.parallel_segmentation = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Headings")) {
		settings.parallel_headings = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Italic Headings")) {
		settings.parallel_italic_headings = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Lemmas")) {
		settings.parallel_lemmas = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Primary Reading")) {
		settings.parallel_variants_primary = choice;
		set_global_textual_reading(option, choice);
	}
	else if (!strcmp(option, "Secondary Reading")) {
		settings.parallel_variants_secondary = choice;
		set_global_textual_reading(option, choice);
	}
	else if (!strcmp(option, "All Readings")) {
		settings.parallel_variants_all = choice;
		set_global_textual_reading(option, choice);
	}

	else if (!strcmp(option, "Transliterated Forms")) {
		settings.parallel_xlit = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Enumerations")) {
		settings.parallel_enumerated = choice;
		set_global_option(option, choice);
	}

	else if (!strcmp(option, "Glosses")) {
		settings.parallel_glosses = choice;
		set_global_option(option, choice);
	}

	/* display change */
	if (settings.dockedInt) {
		main_update_parallel_page();
	} else {
		main_update_parallel_page_detached();
	}
}

/******************************************************************************
 * Name
 *   gui_set_parallel_options_at_start
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void gui_set_parallel_options_at_start(void)
 *
 * Description
 *   set sword global options on program start
 *
 * Return value
 *   void
 */

void main_set_parallel_options_at_start(void)
{
	char *value;
	SWMgr *mgr = backend_p->get_mgr();
	/* Las notas editoriales pertenecen al panel "Comentarios del autor".
	 * Nunca se intercalan entre las versiones comparadas. */
	settings.parallel_footnotes = 0;
	mgr->setGlobalOption("Footnotes", "Off");

	GList *tmp = backend_p->get_module_options();
	while (tmp) {
		char *option = g_strdup((char *)tmp->data);
		g_strdelimit(option, "' ", '_');
		value = xml_get_value("parallel", option);
		int choice = (value ? atoi(value) : 0);
		if (!strcmp((char *)tmp->data, "Textual Variants")) {
			if (atoi(xml_get_value("parallel", "Primary_Reading")))
				mgr->setGlobalOption("Textual Variants",
						     "Primary Reading");
			else if (atoi(xml_get_value("parallel", "Secondary_Reading")))
				mgr->setGlobalOption("Textual Variants",
						     "Secondary Reading");
			else
				mgr->setGlobalOption("Textual Variants",
						     "All Readings");
		} else
			mgr->setGlobalOption((char *)tmp->data, choice ? "On" : "Off");
		g_free(option);
		tmp = g_list_next(tmp);
	}
}

/* GTK4-PORT-101 step 2: the module options are a GMenu over stateful
 * «paralelo» actions, built when the menu opens so their states follow
 * the settings. Each check item is a boolean action on one option. */
struct ParallelOption {
	const char *option;	/* SWORD option name, also the label */
	int *setting;
};

static const ParallelOption *parallel_options(guint *n)
{
	static const ParallelOption options[] = {
		{ "Strong's Numbers", &settings.parallel_strongs },
		{ "Morphological Tags", &settings.parallel_morphs },
		{ "Hebrew Vowel Points", &settings.parallel_hebrewpoints },
		{ "Hebrew Cantillation", &settings.parallel_cantillationmarks },
		{ "Greek Accents", &settings.parallel_greekaccents },
		{ "Cross-references", &settings.parallel_crossref },
		{ "Lemmas", &settings.parallel_lemmas },
		{ "Headings", &settings.parallel_headings },
		{ "Italic Headings", &settings.parallel_italic_headings },
		{ "Morpheme Segmentation", &settings.parallel_segmentation },
		{ "Words of Christ in Red", &settings.parallel_red_words },
		{ "Transliteration", &settings.parallel_transliteration },
		{ "Transliterated Forms", &settings.parallel_xlit },
		{ "Enumerations", &settings.parallel_enumerated },
		{ "Glosses", &settings.parallel_glosses },
	};
	*n = G_N_ELEMENTS(options);
	return options;
}

static void on_option_state(GSimpleAction *action, GVariant *state, gpointer data)
{
	g_simple_action_set_state(action, state);
	apply_parallel_option((const char *)data, g_variant_get_boolean(state));
}

static const char *const variant_readings[] = {
	"Primary Reading", "Secondary Reading", "All Readings"
};

static void on_variants_state(GSimpleAction *action, GVariant *state, gpointer data)
{
	(void)data;
	const char *chosen = g_variant_get_string(state, NULL);
	g_simple_action_set_state(action, state);
	settings.parallel_variants_primary = !strcmp(chosen, variant_readings[0]);
	settings.parallel_variants_secondary = !strcmp(chosen, variant_readings[1]);
	settings.parallel_variants_all = !strcmp(chosen, variant_readings[2]);
	/* The readings not chosen are saved off first; the chosen one last,
	 * so it is the global option left set. */
	for (guint i = 0; i < G_N_ELEMENTS(variant_readings); ++i)
		if (strcmp(chosen, variant_readings[i]))
			apply_parallel_option(variant_readings[i], FALSE);
	apply_parallel_option(chosen, TRUE);
}

void main_parallel_options_menu(GMenu *menu, GActionMap *actions)
{
	guint n;
	const ParallelOption *options = parallel_options(&n);
	for (guint i = 0; i < n; ++i) {
		/* Italic headings only matter with headings shown. */
		if (options[i].setting == &settings.parallel_italic_headings &&
		    !settings.parallel_headings)
			continue;
		gchar *name = g_strdup_printf("op%u", i);
		GSimpleAction *action = g_simple_action_new_stateful(
		    name, NULL, g_variant_new_boolean(*options[i].setting != 0));
		g_signal_connect(action, "change-state", G_CALLBACK(on_option_state),
				 (gpointer)options[i].option);
		g_action_map_add_action(actions, G_ACTION(action));
		g_object_unref(action);
		gchar *detailed = g_strconcat("paralelo.", name, NULL);
		g_menu_append(menu, _(options[i].option), detailed);
		g_free(detailed);
		g_free(name);
		/* Textual variants sit after transliteration, as before. */
		if (options[i].setting == &settings.parallel_transliteration) {
			const char *current = settings.parallel_variants_primary ? variant_readings[0] :
				settings.parallel_variants_secondary ? variant_readings[1] :
				variant_readings[2];
			GSimpleAction *variants = g_simple_action_new_stateful(
			    "variantes", G_VARIANT_TYPE_STRING, g_variant_new_string(current));
			g_signal_connect(variants, "change-state",
					 G_CALLBACK(on_variants_state), NULL);
			g_action_map_add_action(actions, G_ACTION(variants));
			g_object_unref(variants);
			GMenu *readings = g_menu_new();
			for (guint r = 0; r < G_N_ELEMENTS(variant_readings); ++r) {
				GMenuItem *item = g_menu_item_new(_(variant_readings[r]), NULL);
				g_menu_item_set_action_and_target(item, "paralelo.variantes", "s",
								  variant_readings[r]);
				g_menu_append_item(readings, item);
				g_object_unref(item);
			}
			g_menu_append_submenu(menu, _("Textual Variants"), G_MENU_MODEL(readings));
			g_object_unref(readings);
		}
	}
}

/******************************************************************************
 * Name
 *   gui_check_parallel_modules
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void gui_check_parallel_modules(void)
 *
 * Description
 *   check for parallel modules on program start
 *   we don't want to try to display modules we don't have
 *   it makes bad things happen
 *
 * Return value
 *   void
 */

void main_check_parallel_modules(void)
{
	/* i don't know what it's good for, but we'll keep it for now. */
	return;
}

static BibleBackend &parallel_backend(const char *module)
{
    // Retain the separate SWORD rendering options in compatibility mode.
    if (main_backend_is_sword()) return *backend_p;
    return main_backend_for(module);
}

static std::string parallel_content(const char *module, const char *key)
{
    BibleBackend &reader = parallel_backend(module);
    BibleKeyInfo info;
    if (!reader.resolveKey(module, key, info)) return no_content;
    auto content = reader.getVerseContent(module, info.reference, true);
    if (!content.valid) return no_content;
    std::string html;
    for (const auto &heading : content.headings) {
        gchar *safe = g_markup_escape_text(heading.text.c_str(), -1);
        html += "<h3>" + std::string(safe) + "</h3>";
        g_free(safe);
    }
    if (content.paragraphBreak) html += "<br/>";
    // Parallel cells have always kept editorial notes in the commentary pane.
    content.footnotes.clear();
    content.crossReferences.clear();
    html += !main_backend_is_sword() && &reader == bible_backend
        ? renderAnnotatedVerseText(content, module, key, settings.parallel_strongs,
                                   VerseTextStyle{settings.parallel_red_words != 0})
        : content.renderedText;
    return html;
}

/* The docked «Vista paralela» page: two versions side by side, each a
 * whole window of chapters laid out like the main pane, kept on the same
 * verse by gui/parallel_view.c as either one scrolls. */

/* cvparallel is native to the main pane's Bible: the same passage in
 * `module`'s own numbering, or NULL when it has none. */
static gchar *parallel_key_for(const char *module)
{
	const char *passage = settings.cvparallel ? settings.cvparallel
						  : settings.currentverse;
	if (!passage || !settings.MainWindowModule)
		return NULL;
	return main_reference_for_module(settings.MainWindowModule, passage,
					 module);
}

gboolean main_parallel_render_pane(GtkWidget *html, const char *module)
{
	if (!html || !module || !*module || !bible_backend ||
	    !gtk_widget_get_realized(html))
		return FALSE;
	const char *real_mod = main_abbrev_to_name(module);
	if (real_mod)
		module = real_mod;

	BibleBackend &reader = parallel_backend(module);
	gchar *key = reader.hasModule(module) ? parallel_key_for(module) : NULL;
	gboolean shown = key && main_display_bible_side_pane(html, &reader,
							     module, key);
	g_free(key);
	if (!shown) {
		gchar *page = g_strdup_printf(
		    "<html><body bgcolor=\"%s\" text=\"%s\">%s</body></html>",
		    settings.bible_bg_color, settings.bible_text_color,
		    no_content);
		HtmlOutput(page, html, NULL, NULL);
		g_free(page);
	}
	return shown;
}

gint main_parallel_current_position(const char *module, gchar **book)
{
	if (book)
		*book = NULL;
	if (!module || !bible_backend)
		return 0;
	const char *real_mod = main_abbrev_to_name(module);
	if (real_mod)
		module = real_mod;
	BibleBackend &reader = parallel_backend(module);
	gchar *key = reader.hasModule(module) ? parallel_key_for(module) : NULL;
	BibleKeyInfo info;
	gint anchor = 0;
	if (key && reader.resolveKey(module, key, info)) {
		anchor = info.reference.chapter * 1000 + info.reference.verse;
		if (book)
			*book = g_strdup(info.osisBook.c_str());
	}
	g_free(key);
	return anchor;
}

gint main_parallel_current_anchor(const char *module)
{
	return main_parallel_current_position(module, NULL);
}

gint main_parallel_map_anchor(const char *from_mod, const char *to_mod,
			      gint anchor)
{
	if (anchor < 1000 || !from_mod || !to_mod || !bible_backend)
		return 0;
	BibleBackend &from = parallel_backend(from_mod);
	BibleBackend &to = parallel_backend(to_mod);
	if (!from.hasModule(from_mod) || !to.hasModule(to_mod))
		return 0;
	/* Both panes hold the same passage: under one versification an
	 * anchor names the same verse in either. */
	if (!strcmp(from_mod, to_mod) ||
	    from.versification(from_mod) == to.versification(to_mod))
		return anchor;

	/* Otherwise verse by verse, never chapter + offset (KJV Psalm 147:12
	 * is Vulgate 147:1), and only into the book the other pane holds.
	 * The books the two panes hold are worked out once per passage, and
	 * a verse once per pair of Bibles and book: lining up the rows of a
	 * window of chapters asks for every verse in it again and again. */
	static std::string context_key, from_book, to_book;
	static std::map<std::string, gint> verses;
	const char *passage = settings.cvparallel ? settings.cvparallel
						  : settings.currentverse;
	const std::string key_now = std::string(from_mod) + "\t" + to_mod + "\t" +
				    (passage ? passage : "");
	if (key_now != context_key) {
		gchar *from_key = parallel_key_for(from_mod);
		gchar *to_key = parallel_key_for(to_mod);
		BibleKeyInfo from_info, to_info;
		const bool known = from_key && to_key &&
			from.resolveKey(from_mod, from_key, from_info) &&
			to.resolveKey(to_mod, to_key, to_info);
		g_free(from_key);
		g_free(to_key);
		from_book = known ? from_info.osisBook : std::string();
		to_book = known ? to_info.osisBook : std::string();
		context_key = key_now;
	}
	if (from_book.empty() || to_book.empty())
		return 0;

	const std::string verse_key = std::string(from_mod) + "\t" + to_mod + "\t" +
				      from_book + "\t" + to_book + "\t" +
				      std::to_string(anchor);
	auto known = verses.find(verse_key);
	if (known != verses.end())
		return known->second;

	const int chapter = anchor / 1000, verse = anchor % 1000;
	BibleKeyInfo mapped_info;
	gint mapped = 0;
	/* a chapter's heading goes with its first verse */
	gchar *key = g_strdup_printf("%s %d:%d", from_book.c_str(),
				     chapter, verse ? verse : 1);
	gchar *other = main_reference_for_module(from_mod, key, to_mod);
	if (other && to.resolveKey(to_mod, other, mapped_info) &&
	    mapped_info.osisBook == to_book)
		mapped = mapped_info.reference.chapter * 1000 +
			 (verse ? mapped_info.reference.verse : 0);
	g_free(other);
	g_free(key);
	if (verses.size() > 50000)	/* a few books' worth: start over */
		verses.clear();
	verses[verse_key] = mapped;
	return mapped;
}

void main_update_parallel_page(void)
{
	/* A new passage may already be laid out in both panes (the next
	 * verse, say): they only move to it. The same passage again is a
	 * change of how it is shown (Preferences, options): laid out anew. */
	const gboolean moved = g_strcmp0(settings.cvparallel, settings.currentverse) != 0;

	gui_reassign_strdup(&settings.cvparallel, settings.currentverse);
	if (moved)
		gui_parallel_panes_follow(TRUE);
	else
		gui_parallel_panes_update(FALSE);
}

/******************************************************************************
 * Name
 *   interpolate_parallel_display
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void interpolate_parallel_display(SWBuf& text, gchar *key)
 *
 * Description
 *   carry out the hard work of getting verses for parallel display.
 *
 * Return value
 *   void
 */

/* how often the column names are repeated down a long chapter */
#define PARALLEL_LABEL_EVERY 12

static void interpolate_parallel_display(char *control_name,
					 SWBuf    &text,
					 gchar    *key,
					 gint     parallel_count,
					 gint     fraction)
{
	// we are guaranteed that control, the module whose v11n
	// controls our verse limit and display choices, exists.

	gchar *utf8str, *textColor, *tmpkey, tmpbuf[256];
	const gchar *bgColor;
	gchar str[500];
	gint cur_verse, cur_chapter, verse, modidx;
	char *cur_book;
	MOD_FONT **mf;
	gboolean *is_rtol, *is_module, *is_bible_text;

	if (!gtk_widget_get_realized(GTK_WIDGET(widgets.notebook_bible_parallel)))
		return;

	// need #verses to process in this chapter.

    BibleKeyInfo control_info;
    if (!parallel_backend(control_name).resolveKey(control_name, key, control_info)) return;
    const int xverses = control_info.verseCount;

	is_module = g_new(gboolean, parallel_count);
	is_rtol = g_new(gboolean, parallel_count);
	is_bible_text = g_new(gboolean, parallel_count);
	mf = g_new(MOD_FONT *, parallel_count);

	// quick cache of fonts/rtol/type info.
	for (modidx = 0; modidx < parallel_count; ++modidx) {
		gchar *mod = settings.parallel_list[modidx];
		// might have an abbrev. get the real.
		const char *real_mod = main_abbrev_to_name(mod);
		if (real_mod)
			mod = (gchar *)real_mod;

		// determine module presence once each.
		is_module[modidx] = parallel_backend(mod).hasModule(mod);

		if (is_module[modidx]) {
			is_rtol[modidx] = main_is_mod_rtol(mod);
			mf[modidx] = get_font(mod);
			apply_bible_body_font(mf[modidx]);
			is_bible_text[modidx] =
			    (main_get_mod_type(mod) == TEXT_TYPE);
		} else {
			gui_generic_warning((unknown_parallel + (SWBuf)mod).c_str());
		}
	}

	// frankly, we're faking it here.
	// we have potentially variable v11n among the parallel modules.

	// but we must validate a key in some vaguely consistent manner.
	// arbitrarily, we have picked the 1st.
	// it's consistent, but very possibly wrong for all but the 1st.
    tmpkey = g_strdup(control_info.key.c_str());
    cur_book = g_strdup(control_info.osisBook.c_str());
    cur_chapter = control_info.reference.chapter;
    cur_verse = control_info.reference.verse;
	settings.intCurVerse = cur_verse;

	/* Alternating verse rows. 0.10 was almost invisible on a dark
	 * background -- the point of the banding is to let the eye track a
	 * row across columns, so it has to actually be seen. */
	gchar *row_tint = blend_hex_color(settings.bible_bg_color,
					  settings.bible_text_color, 0.17);

	for (verse = 1; verse <= xverses; ++verse) {
		snprintf(tmpbuf, 255, "%s %d:%d", cur_book, cur_chapter, verse);
		free(tmpkey);
		tmpkey = g_strdup(tmpbuf);

		/* The <thead> naming the columns scrolls away with
		 * everything else -- this is a GtkGrid inside a text
		 * buffer, not a real table widget, so nothing sticks. A
		 * chapter in, there is no way to tell which version is on
		 * the left. Repeating the names every so often costs one
		 * thin row and answers the question wherever you happen to
		 * be reading. */
		if (verse > 1 && ((verse - 1) % PARALLEL_LABEL_EVERY) == 0) {
			const gchar *lab_fg = settings.darktheme ? "#E6C989" : "#8A6D1E";
			gchar *lab_bg = blend_hex_color(settings.bible_bg_color,
							lab_fg, 0.22);
			text += "<tr>";
			for (modidx = 0; modidx < parallel_count; modidx++) {
				const char *ab =
				    main_name_to_abbrev(settings.parallel_list[modidx]);
				snprintf(str, 499,
					 "<td width=\"%d%%\" bgcolor=\"%s\">"
					 "<font color=\"%s\" size=\"-1\"><b>%s</b></font></td>",
					 fraction, lab_bg, lab_fg,
					 (ab ? ab : settings.parallel_list[modidx]));
				text += str;
			}
			text += "</tr>";
			g_free(lab_bg);
		}

		BibleKeyInfo row_info;
		const bool have_row = bible_backend &&
			parallel_backend(control_name).resolveKey(control_name, tmpkey, row_info);

		text += "<tr valign=\"top\">";

		// alternate background colors.
		bgColor = (settings.alternation && (verse % 2 == 0))
			? row_tint
			: settings.bible_bg_color;

		for (modidx = 0; modidx < parallel_count; modidx++) {
			gchar *mod = settings.parallel_list[modidx];
			// might have an abbrev. get the real.
			const char *real_mod = main_abbrev_to_name(mod);
			if (real_mod)
				mod = (gchar *)real_mod;

			if (is_module[modidx]) {

				// mark current verse properly; texto siempre
				// legible, sin invertir contra el fondo.
				textColor = ((verse == cur_verse) && is_bible_text[modidx])
						? settings.currentverse_color
						: settings.bible_text_color;

				/* tmpkey is expressed in the *control* module's
				 * versification. Each cell maps it to its own:
				 * comparing SpaRV1909 (KJV) against SpaPlatense
				 * (Vulg) by reusing the string put Psalm 121
				 * beside Psalm 120. Modules without verse keys
				 * have no versification to map through. */
                const auto type = parallel_backend(mod).moduleType(mod);
                const bool verse_keyed = type == BibleModuleType::Bible || type == BibleModuleType::Commentary;
				gchar *modkey = verse_keyed
					? main_reference_for_module(control_name, tmpkey, mod)
					: g_strdup(tmpkey);

				/* The cell numbers its own verse, with the
				 * chapter when it is not the row's. */
				BibleKeyInfo cell_info;
				gchar *num;
				if (modkey && verse_keyed && have_row &&
				    parallel_backend(mod).resolveKey(mod, modkey, cell_info) &&
				    (cell_info.reference.chapter != row_info.reference.chapter ||
				     cell_info.osisBook != row_info.osisBook))
					num = g_strdup_printf("%d:%d",
							      cell_info.reference.chapter,
							      cell_info.reference.verse);
				else if (modkey && verse_keyed && have_row &&
					 parallel_backend(mod).resolveKey(mod, modkey, cell_info))
					num = main_format_number(cell_info.reference.verse);
				else
					num = main_format_number(verse);

				const gchar *newurl = main_url_encode(tmpkey);
				snprintf(str, 499,
					 "<td width=\"%d%%\" bgcolor=\"%s\">"
					 "<a name=\"%d\">%s</a>"
					 "<a href=\"passagestudy.jsp?action=showParallel&"
					 "type=verse&value=%s\">"
					 "<font color=\"%s\" size=\"%+d\">%s. </font></a>"
					 "<font face=\"%s\" size=\"%+d\" color=\"%s\">",
					 fraction, bgColor,
					 verse,
					 ((verse == cur_verse) ? "<hr><hr>" : ""),
					 newurl,
					 settings.bible_verse_num_color,
					 settings.verse_num_font_size + settings.base_font_size,
					 num,
					 mf[modidx]->old_font,
					 mf[modidx]->old_font_size_value,
					 textColor);
				g_free((gchar *)newurl);
				g_free(num);
				text += str;
				if (modkey && verse_keyed) {
					gchar *marca = highlight_note_marker_for(mod, modkey);
					if (marca)
						text += marca;
					g_free(marca);
				}

				if (is_rtol[modidx])
					text += "<br/><div align=right>";

				/* Mapped per verse, never chapter + offset: KJV
				 * Psalm 147:11 is Vulgate 146:11 and 147:12 is
				 * Vulgate 147:1. A verse with no counterpart
				 * stays empty rather than reading the same
				 * numbers in the other versification. modkey
				 * is our own copy, never the live key's buffer
				 * (aliasing that produced Revelation 1:1 in
				 * every row). */
				if (modkey) {
                    utf8str = g_strdup(parallel_content(mod, modkey).c_str());
					if (utf8str) {
						text += utf8str;
						g_free(utf8str);
					}
				} else {
					text += no_content;
				}
				g_free(modkey);

				if (is_rtol[modidx])
					text += "</div>";
			}

			text += "</font></td>";
		}

		text += "</tr>";
	}
	g_free(tmpkey);
	g_free(cur_book);

	/* clear bookkeeping data */
	for (modidx = 0; modidx < parallel_count; ++modidx)
		if (is_module[modidx])
			free_font(mf[modidx]);
	g_free(mf);
	g_free(is_rtol);
	g_free(is_module);
	g_free(is_bible_text);
	g_free(row_tint);
}

/******************************************************************************
 * Name
 *   main_update_parallel_page_detached
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void main_update_parallel_page_detached(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

/* The whole-chapter, one-column-per-module HTML: a <thead> of module
 * names over one <tr> per verse, each verse's text in its own <td>.
 * wk-html.c turns every cell into its own nested WkHtml inside a
 * GtkGrid, which is what makes the columns line up verse by verse.
 *
 * Split out of main_update_parallel_page_detached() so the reading
 * pane can render the same thing -- see main_parallel_html_for_pane().
 * Returns FALSE when there is nothing to build (no parallel list, or
 * the controlling module is missing). */
static gboolean
parallel_build_html(SWBuf &text, gint *parallel_count_out)
{
    if (!bible_backend || !backend_p) return FALSE;
    backend_p->get_mgr()->setGlobalOption("Footnotes", "Off");
	gchar buf[5000];
	gint modidx, parallel_count, fraction;

	/* how big a pile of parallels have we got? */
	if (settings.parallel_list == NULL)
		return FALSE;
	for (parallel_count = 0; settings.parallel_list[parallel_count]; ++parallel_count)
		/* just count non-null string ptrs */;

	/* get the per-column percentage width. */
	/* 2 => 50, 4 => 25, 5 => 20, 10 => 10, ... */
	fraction = (parallel_count ? (100 / parallel_count) : 100);

	// identify the module whose v11n and CSS display defaults will drive us.
	// should be 1st parallel module.
	// it would be really weird if that's missing, but be ready anyhow.
	char *control_name = (settings.parallel_list
			     ? settings.parallel_list[0]
			     : settings.MainWindowModule);
	// might have an abbrev. get the real.
	const char *real_mod = main_abbrev_to_name(control_name);
	if (real_mod)
		control_name = (gchar *)real_mod;

	BibleBackend &control = parallel_backend(control_name);
	if (!control.hasModule(control_name))
	{
		gui_generic_warning(_("Failed to find 1st parallel module for display control."));
		return FALSE;
	}

	/* cvparallel is native to the main pane's Bible, but the rows are
	 * laid out in the control module's chapter: carry the reference
	 * over first. No counterpart there, no table. */
	gchar *control_key = main_reference_for_module(
	    settings.MainWindowModule, settings.cvparallel, control_name);
	if (!control_key)
		return FALSE;

	snprintf(buf, 4999, HTML_START
		 "<body bgcolor=\"%s\" text=\"%s\" link=\"%s\">"
		 "  <div class=\"table-container\">"
		 "    <table>"
		 "      <thead>"
		 "        <tr>",
		 (settings.parallel_italic_headings ? "italic" : "bold"),
		 control.moduleRenderHeader(control_name).c_str(),
		 STICKY_MODNAMES,
		 (settings.justify_margins ? "<style> td { text-align: justify; padding: 5px; } </style>" : ""),
		 settings.bible_bg_color, settings.bible_text_color,
		 settings.link_color);
	text += buf;

	/* The header used to be a hardcoded light grey (#c0c0c0) with the
	 * verse-number colour on top: fine on the old light default, a
	 * glaring band of near-white with poor contrast on a dark theme.
	 * It now uses the same gold-on-tinted-background treatment the
	 * rest of this fork gives module labels, derived from the theme's
	 * own colours so it works on either. */
	{
		const gchar *hdr_fg = settings.darktheme ? "#E6C989" : "#8A6D1E";
		gchar *hdr_bg = blend_hex_color(settings.bible_bg_color, hdr_fg, 0.22);

		for (modidx = 0; settings.parallel_list[modidx]; ++modidx) {
			const char *abbreviation = main_name_to_abbrev(settings.parallel_list[modidx]);
			snprintf(buf, 499,
				 "<th width=\"%d%%\" bgcolor=\"%s\"><font color=\"%s\" size=\"%+d\"><b>%s</b></font></th>",
				 fraction,
				 hdr_bg,
				 hdr_fg,
				 settings.verse_num_font_size + settings.base_font_size,
				 (abbreviation ? abbreviation : settings.parallel_list[modidx]));
			text += buf;
		}
		g_free(hdr_bg);
	}

	text += "</tr> </thead> <tbody>";
	interpolate_parallel_display(control_name, text, control_key, parallel_count, fraction);
	g_free(control_key);
	text += "</tbody> </table> </div> </body> </html>";

	if (parallel_count_out)
		*parallel_count_out = parallel_count;
	return TRUE;
}

gchar *main_parallel_html(void)
{
    SWBuf html;
    return parallel_build_html(html, nullptr) ? g_strdup(html.c_str()) : nullptr;
}

void main_update_parallel_page_detached(void)
{
	SWBuf text("");
	gchar buf[500];
	gint parallel_count = 0;

	if (!widgets.html_parallel_dialog ||
	    !gtk_widget_get_realized(GTK_WIDGET(widgets.html_parallel_dialog)))
		return;

	gui_reassign_strdup(&settings.cvparallel, settings.currentverse);
	if (!parallel_build_html(text, &parallel_count))
		return;

	snprintf(buf, 499, "%d", settings.intCurVerse);

	HtmlOutput((char *)(settings.imageresize
				? AnalyzeForImageSize((char *)text.c_str(), parallel_count*2/3,
						      widgets.html_parallel_dialog)
				: (char *)text.c_str()),
		   widgets.html_parallel_dialog, NULL, buf);
}

/* Renders the verse-aligned comparison into the main reading pane,
 * in place of the ordinary single-module chapter. Only reachable from
 * reading mode (see main_display_bible()), where the sidebar, tabs and
 * previewer are already out of the way and the columns get the full
 * width of the window.
 *
 * Costs what the table costs: about 1.3 ms per cell, so ~80 ms for a
 * 31-verse chapter against two versions and ~470 ms for Psalm 119.
 * Measured, and accepted as the price of verse-by-verse alignment. */
gboolean main_reading_compare_render(const char *key)
{
	SWBuf text("");
	gchar buf[500];

	if (!widgets.html_text ||
	    !gtk_widget_get_realized(GTK_WIDGET(widgets.html_text)))
		return FALSE;

	gui_reassign_strdup(&settings.cvparallel,
			    key ? key : settings.currentverse);
	if (!parallel_build_html(text, NULL))
		return FALSE;

	snprintf(buf, 499, "%d", settings.intCurVerse);
	HtmlOutput((char *)text.c_str(), widgets.html_text, NULL, buf);
	return TRUE;
}

/******************************************************************************
 * Name
 *   gui_swap_parallel_with_main
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void gui_swap_parallel_with_main(char * intmod)
 *
 * Description
 *   swaps parallel mod with mod in main text window
 *
 * Return value
 *   void
 */

void main_swap_parallel_with_main(char *intmod)
{
	if (!main_display_bible_from_module(settings.MainWindowModule,
					    settings.currentverse, intmod))
		return;
	main_update_parallel_page();
	gtk_notebook_set_current_page(GTK_NOTEBOOK(widgets.notebook_bible_parallel), 0);
}

/******************************************************************************
 * Name
 *   main_init_parallel_view
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void main_init_parallel_view(void)
 *
 * Description
 *   create a new sword backend for parallel use
 *   call gui_create_parallel_page to create a docked parallel pane
 *   call main_set_parallel_options_at_start to sword global opts
 *
 * Return value
 *   void
 */

void main_init_parallel_view(void)
{
	backend_p = new SwordBackend();
}

/******************************************************************************
 * Name
 *   main_delete_parallel_view
 *
 * Synopsis
 *   #include "main/parallel_view.h
 *
 *   void main_delete_parallel_view(void)
 *
 * Description
 *   delete the sword backend for the parallel view
 *
 * Return value
 *   void
 */

void main_delete_parallel_view(void)
{
	delete backend_p;
}
