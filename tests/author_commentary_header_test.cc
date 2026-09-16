/*
 * The heading of "Comentarios del autor" names the verse it is showing.
 *
 * Regression for the clicked-note bug: with the Bible pane focused on
 * Matthew 11:5, clicking the editorial marker of Matthew 11:6 rendered
 * 11:6's comment under an "Mt 11:5" heading.  GTKEntryDisp::display()
 * built the heading from settings.currentverse -- the Bible pane's
 * focus -- while the body came from the module key that
 * main_display_commentary() had just set.
 *
 * Clicking a note is an aside about one reference; it must not drag the
 * Bible pane along, so the fix could not be to move settings.currentverse.
 * Heading and body now both follow the module key.
 *
 * This exercises the heading derivation for real: the key the pane sets,
 * read back off the module exactly as author_commentary_heading_key()
 * does (getOSISRef()), then main_interlineal_cita_es() -- the real
 * formatter, linked in here -- on top of it, against the body resolved
 * by the same key.  settings.currentverse is deliberately held on
 * another verse throughout and asserted never to move.
 */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
#include "main/interlineal.h"
#include "main/reference_transition.h"
#include "main/settings.h"

SETTINGS settings = {};
char *sword_locale = nullptr;

extern "C" void main_dialog_search_percent_update(char, void *) {}
extern "C" void main_sidebar_search_percent_update(char, void *) {}
extern "C" void main_index_percent_update(char, void *) {}
extern "C" void main_setup_displays(void) {}
extern "C" void main_clear_abbreviations(void) {}
extern "C" void main_add_abbreviation(const char *, const char *) {}
extern "C" int main_is_module(char *) { return 0; }
extern "C" void gui_generic_warning(const char *) {}
extern "C" const char *main_get_language_map(const char *language)
{
	return language;
}
extern "C" char *main_get_mod_config_file(const char *, const char *)
{
	return nullptr;
}
extern "C" char *main_format_number(int value)
{
	return g_strdup_printf("%d", value);
}
/* interlineal.cc is linked in whole for main_interlineal_cita_es(); the
 * rest of that file reaches into the GTK layer, which has no place in a
 * headless test. None of these is on the citation's path. */
extern "C" char *xml_get_value(const char *, const char *) { return nullptr; }
extern "C" void xml_set_or_create_value(const char *, const char *,
					const char *) {}
extern "C" void gui_interlineal_rellenar(void) {}
extern "C" void gui_lectura_sync_ficha_clear(void) {}
extern "C" gchar *main_morf_codigo(const char *) { return nullptr; }
extern "C" gchar *main_morf_es(const char *) { return nullptr; }
extern "C" gchar *main_morf_corto(const char *) { return nullptr; }
extern "C" void main_display_verse_list_in_sidebar(gchar *, gchar *, gchar *) {}

extern "C" gchar *XI_g_strdup_printf(const char *, int, const gchar *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	gchar *result = g_strdup_vprintf(format, arguments);
	va_end(arguments);
	return result;
}

static bool
contains(const std::string &haystack, const char *needle)
{
	return haystack.find(needle) != std::string::npos;
}

/*
 * What the pane does with one reference, in the order it does it:
 * resolve the Bible verse to the commentary module's own key, set that
 * key on the module (main_display_commentary's set_module_key), and
 * then render.  `heading` and `body` come back from that one key, which
 * is the whole point: they cannot disagree.
 */
struct Rendered
{
	std::string heading;
	std::string body;
};

static Rendered
render(SwordBackend &backend, const char *bible, const char *key)
{
	Rendered out;
	const char *commentary = authorCommentaryForBible(bible);
	g_assert_nonnull(commentary);

	const BibleModuleTransitionPlan plan =
		planBibleModuleTransition(backend, bible, key, commentary);
	g_assert_true(plan.status == BibleModuleTransition::SameModule ||
		      plan.status == BibleModuleTransition::Converted);

	sword::SWModule *module = backend.get_SWModule(commentary);
	g_assert_nonnull(module);
	module->setKeyText(plan.key.c_str());
	module->getRawEntry(); // snap to entry, as display() does

	/* the heading: author_commentary_heading_key() + the formatter */
	sword::VerseKey *vk =
		dynamic_cast<sword::VerseKey *>(module->getKey());
	g_assert_nonnull(vk);
	gchar *heading_key = g_strdup((const char *)vk->getOSISRef());
	gchar *citation = main_interlineal_cita_es(heading_key);
	out.heading = citation ? citation : "";
	g_free(citation);
	g_free(heading_key);

	/* the body: the same key, nothing else */
	BibleKeyInfo info;
	if (backend.resolveKey(commentary, plan.key, info))
		out.body = backend.getVerseContent(commentary, info.reference,
						   true)
				   .plainText;
	return out;
}

int
main()
{
	SwordBackend backend;

	if (!backend.hasModule("SpaPlatense") ||
	    !backend.hasModule("SpaPlatenseComentarios")) {
		std::puts("author_commentary_header_skipped=no-SpaPlatense");
		return 0;
	}

	/* The Bible pane's focus. It is set once here and must survive
	 * every clicked note below untouched -- a clicked marker is an
	 * aside, not navigation. */
	settings.currentverse = g_strdup("Matthew 11:5");
	const gchar *focus_at_start = settings.currentverse;

	/* Sanity: the heading must not be able to pass by accident. The
	 * focused verse and the clicked one have to render differently. */
	const Rendered focused =
		render(backend, "SpaPlatense", "Matthew 11:5");
	g_assert_cmpstr(focused.heading.c_str(), ==, "Mt 11:5");

	/* Case A: focus on Matthew 11:5, click the note of Matthew 11:6.
	 * Heading and body are both the clicked verse. */
	{
		const Rendered clicked =
			render(backend, "SpaPlatense", "Matthew 11:6");

		g_assert_cmpstr(clicked.heading.c_str(), ==, "Mt 11:6");
		g_assert_cmpstr(clicked.heading.c_str(), !=,
				focused.heading.c_str());
		g_assert_true(contains(clicked.body, "escandalizare"));
		g_assert_cmpstr(clicked.body.c_str(), !=,
				focused.body.c_str());

		/* and the Bible pane did not move */
		g_assert_cmpstr(settings.currentverse, ==, "Matthew 11:5");
		g_assert_true(settings.currentverse == focus_at_start);
		std::puts("author_commentary_header_case_a=ok");
	}

	/* Case B: a clicked note further away in the same chapter. */
	{
		const Rendered clicked =
			render(backend, "SpaPlatense", "Matthew 11:30");

		g_assert_cmpstr(clicked.heading.c_str(), ==, "Mt 11:30");
		g_assert_true(contains(clicked.body, "El adjetivo griego"));

		g_assert_cmpstr(settings.currentverse, ==, "Matthew 11:5");
		std::puts("author_commentary_header_case_b=ok");
	}

	/* Case C: after the clicked note, ordinary navigation to Matthew
	 * 12:4. Here the focus really does move, and the pane follows it
	 * through the same one-key path -- so normal navigation still
	 * titles itself with the focused verse. */
	{
		g_free(settings.currentverse);
		settings.currentverse = g_strdup("Matthew 12:4");

		const Rendered navigated =
			render(backend, "SpaPlatense", settings.currentverse);

		g_assert_cmpstr(navigated.heading.c_str(), ==, "Mt 12:4");
		g_assert_true(contains(navigated.body,
				       "panes de la proposición"));
		g_assert_false(contains(navigated.body, "escandalizare"));
		std::puts("author_commentary_header_case_c=ok");
	}

	/* The formatter is locale-independent because the key handed to it
	 * is OSIS, which is what makes the heading survive whatever locale
	 * the running application has set. */
	{
		gchar *from_osis = main_interlineal_cita_es("Matt.11.6");
		gchar *from_plain = main_interlineal_cita_es("Matthew 11:6");
		g_assert_cmpstr(from_osis, ==, "Mt 11:6");
		g_assert_cmpstr(from_osis, ==, from_plain);
		g_free(from_osis);
		g_free(from_plain);
		std::puts("author_commentary_header_osis=ok");
	}

	g_free(settings.currentverse);
	settings.currentverse = nullptr;

	std::puts("author_commentary_header_test=ok");
	return 0;
}
