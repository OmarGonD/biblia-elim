/*
 * What the "Comentarios del autor" pane resolves and renders.
 *
 * Covers the data half of the panel contract: the Bible maps to its own
 * commentary module through the existing infrastructure, the reference
 * is carried across with the neutral converter, and navigating really
 * does change the text -- 11:30, 12:4 and 12:5 must each give their own
 * answer, the last one being an explicit absence.
 */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
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

/* What the pane resolves for one Bible verse: the commentary module's
 * own reference, then its body -- the same two steps
 * author_commentary_render() takes in main/sword.cc. */
static std::string
commentary_body(SwordBackend &backend, const char *bible, const char *key)
{
	const char *commentary = authorCommentaryForBible(bible);
	g_assert_nonnull(commentary);

	const BibleModuleTransitionPlan plan =
		planBibleModuleTransition(backend, bible, key, commentary);
	g_assert_true(plan.status == BibleModuleTransition::SameModule ||
		      plan.status == BibleModuleTransition::Converted);

	BibleKeyInfo info;
	if (!backend.resolveKey(commentary, plan.key, info))
		return std::string();
	return backend.getVerseContent(commentary, info.reference, true)
		.plainText;
}

static bool
blank(const std::string &text)
{
	return text.find_first_not_of(" \t\r\n") == std::string::npos;
}

int
main()
{
	SwordBackend backend;

	if (!backend.hasModule("SpaPlatense") ||
	    !backend.hasModule("SpaPlatenseComentarios")) {
		std::puts("author_commentary_content_skipped=no-SpaPlatense");
		return 0;
	}

	/* The mapping is the existing one, not a name repeated in a
	 * handler. Editions without an author commentary stay NULL, which
	 * is what makes the clicked marker fall back to the previewer. */
	g_assert_cmpstr(authorCommentaryForBible("SpaPlatense"), ==,
			"SpaPlatenseComentarios");
	g_assert_null(authorCommentaryForBible("SpaRV1909"));
	g_assert_null(authorCommentaryForBible("SpaRVG"));
	g_assert_null(authorCommentaryForBible(NULL));
	std::puts("author_commentary_mapping=ok");

	/* C. Navigating changes the text. Each verse gets its own comment,
	 * and none of them is another verse's. */
	const std::string at_1130 =
		commentary_body(backend, "SpaPlatense", "Matthew 11:30");
	const std::string at_1204 =
		commentary_body(backend, "SpaPlatense", "Matthew 12:4");
	const std::string at_1205 =
		commentary_body(backend, "SpaPlatense", "Matthew 12:5");

	g_assert_true(contains(at_1130, "El adjetivo griego"));
	g_assert_false(contains(at_1130, "panes de la proposición"));

	g_assert_true(contains(at_1204, "panes de la proposición"));
	g_assert_false(contains(at_1204, "El adjetivo griego"));

	g_assert_cmpstr(at_1130.c_str(), !=, at_1204.c_str());

	/* Matthew 12:5 carries no comment at all: the pane must be able to
	 * tell "nothing here" from "nothing rendered". */
	g_assert_true(blank(at_1205));
	std::puts("author_commentary_navigation=ok");

	/* The commentary is verse-keyed in the Bible's own versification
	 * (both declare Vulg), so the conversion is identity here and the
	 * verse numbers line up rather than sliding. */
	{
		BibleKeyInfo bible_info, comm_info;
		g_assert_true(backend.resolveKey("SpaPlatense",
						 "Matthew 12:4", bible_info));
		g_assert_true(backend.resolveKey("SpaPlatenseComentarios",
						 "Matthew 12:4", comm_info));
		g_assert_cmpint(bible_info.reference.chapter, ==,
				comm_info.reference.chapter);
		g_assert_cmpint(bible_info.reference.verse, ==,
				comm_info.reference.verse);
		g_assert_cmpstr(backend.versification("SpaPlatense").c_str(), ==,
				backend.versification("SpaPlatenseComentarios")
					.c_str());
		std::puts("author_commentary_versification=ok");
	}

	/* Multiple notes in one verse: the source keeps no per-note anchor.
	 * SpaPlatenseComentarios stores one aggregated entry per verse,
	 * with each note introduced by its own "<b>N. </b>" heading, so
	 * opening the verse's comment is the whole of what there is to
	 * open. Genesis 50:25 is one of the 14 verses carrying two notes. */
	{
		const std::string joined =
			commentary_body(backend, "SpaPlatense", "Genesis 50:25");
		g_assert_true(contains(joined, "25."));
		g_assert_true(contains(joined, "26."));
		g_assert_true(contains(joined, "sepultaron en Siquem"));

		sword::SWModule *comm =
			backend.get_SWModule("SpaPlatenseComentarios");
		g_assert_nonnull(comm);
		comm->setKeyText("Genesis 50:25");
		const std::string raw = comm->getRawEntry();
		/* one entry, no anchors or ids to address a single note by */
		g_assert_true(raw.find("<a ") == std::string::npos);
		g_assert_true(raw.find("id=") == std::string::npos);
		std::puts("author_commentary_multiple_notes=ok");
	}

	std::puts("author_commentary_content_test=ok");
	return 0;
}
