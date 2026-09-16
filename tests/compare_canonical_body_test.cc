/*
 * The "Comparar" panel compares scripture, never the editorial apparatus
 * around it.  It reads the verse body through the neutral content
 * interface, whose contract (bible_types.h) keeps plainText apart from
 * footnotes, crossReferences and headings.
 *
 * SWORD's plain-text filters do not honour that split on their own: with
 * the global Footnotes option on -- which is what the main reading pane
 * asks for, and the default for every module -- OSISPlain inlines each
 * <note> body into the stripped text, bracketed.  SpaPlatense Matthew
 * 11:30 then strips as the verse plus the whole of Straubinger's
 * commentary, and Psalms 23:1 strips as the commentary followed by the
 * verse.  These tests pin the canonical-body guarantee and, just as
 * importantly, that it costs nothing to modules carrying no notes.
 */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <swmgr.h>
#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
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

/* What the main reading pane leaves on the manager: notes and cross
 * references on, because it draws markers for them (global_ops.cc
 * defaults both to On for every module). */
static void
main_pane_options(SwordBackend &backend)
{
	sword::SWMgr *mgr = backend.get_mgr();
	mgr->setGlobalOption("Footnotes", "On");
	mgr->setGlobalOption("Cross-references", "On");
	mgr->setGlobalOption("Headings", "On");
}

static bool
contains(const std::string &haystack, const char *needle)
{
	return haystack.find(needle) != std::string::npos;
}

/* The verse body, exactly as the Comparar panel reads it (see
 * append_un_versiculo() in main/lectura_sync.cc). */
static std::string
compare_body(SwordBackend &backend, const char *module, const char *key)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(module, key, info));
	return backend.getVerseBodyText(module, info.reference);
}

/* The main reading view's model of the same verse. */
static std::string
display_plain_text(SwordBackend &backend, const char *module, const char *key)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(module, key, info));
	return backend.getVerseContent(module, info.reference, true).plainText;
}

static std::string
main_pane_render(SwordBackend &backend, const char *module, const char *key)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(module, key, info));
	return backend.getVerseContent(module, info.reference, true).renderedText;
}

/* Reading the module's stripped text directly -- what the panel used to
 * do.  Kept here so the leak these tests guard against stays visible:
 * if this ever stops carrying the note, the fixture changed, not the
 * behaviour under test. */
static std::string
raw_strip(SwordBackend &backend, const char *module, const char *key)
{
	sword::SWModule *mod = backend.get_SWModule(module);
	g_assert_nonnull(mod);
	const std::string saved = mod->getKeyText();
	mod->setKeyText(key);
	const char *plain = mod->stripText();
	const std::string text = plain ? plain : "";
	mod->setKeyText(saved.c_str());
	return text;
}

static const char *const NOTE_FRAGMENT = "El adjetivo griego";
static const char *const NOTE_TAIL = "Ubi amatur, non laboratur";

static void
test_platense_verse_with_note(SwordBackend &backend)
{
	const std::string body =
		compare_body(backend, "SpaPlatense", "Matthew 11:30");

	/* The verse itself is there, whole. */
	g_assert_true(contains(body, "Porque mi yugo es excelente"));
	g_assert_true(contains(body, "mi carga es liviana"));

	/* Straubinger's comment is not -- not its opening, not its body,
	 * not its closing Latin tag, and not the brackets the plain-text
	 * filter wraps an inlined note in. */
	g_assert_false(contains(body, NOTE_FRAGMENT));
	g_assert_false(contains(body, NOTE_TAIL));
	g_assert_false(contains(body, "jrestós"));
	g_assert_false(contains(body, "["));

	/* The note is still in the module and still reaches the main
	 * reading view: nothing was removed from the source. */
	g_assert_true(contains(raw_strip(backend, "SpaPlatense", "Matthew 11:30"),
			       NOTE_FRAGMENT));
	g_assert_true(contains(main_pane_render(backend, "SpaPlatense",
						"Matthew 11:30"),
			       "showNote"));

	/* BibleVerseContent documents plainText as the body and keeps
	 * notes in their own field, so the display model must not inline
	 * them either, whatever the reading pane's options say. */
	g_assert_false(contains(display_plain_text(backend, "SpaPlatense",
						   "Matthew 11:30"),
			        NOTE_FRAGMENT));
	std::puts("compare_canonical_body_note_suppressed=ok");
}

/* Psalms 23:1 and Genesis 1:1 open with the note, so an inlined comment
 * pushes the verse out of sight entirely rather than trailing it. */
static void
test_platense_note_before_verse(SwordBackend &backend)
{
	const std::string psalm =
		compare_body(backend, "SpaPlatense", "Psalms 23:1");
	g_assert_false(psalm.empty());
	g_assert_false(contains(psalm, "uso litúrgico"));
	g_assert_false(contains(psalm, "["));

	const std::string genesis =
		compare_body(backend, "SpaPlatense", "Genesis 1:1");
	g_assert_true(contains(genesis, "Al principio creó Dios"));
	g_assert_false(contains(genesis, "Desde antiguo se ha observado"));
	g_assert_false(contains(genesis, "["));
	std::puts("compare_canonical_body_leading_note_suppressed=ok");
}

/* A Platense verse carrying no note at all must come through untouched,
 * emphasis included: OSISPlain marks <hi type="italic"> with asterisks
 * and that is body text, not apparatus. */
static void
test_platense_verse_without_note(SwordBackend &backend)
{
	const std::string body =
		compare_body(backend, "SpaPlatense", "Matthew 11:27");
	g_assert_true(contains(body, "A Mí me ha sido transmitido todo por mi Padre"));
	g_assert_true(contains(body, "quisiere revelar"));
	g_assert_true(contains(body, "*"));
	/* g_assert_cmpstr() keeps the two char pointers past the end of the
	 * expressions that produced them, so std::string temporaries must
	 * be named first. */
	const std::string legacy =
		raw_strip(backend, "SpaPlatense", "Matthew 11:27");
	g_assert_cmpstr(body.c_str(), ==, legacy.c_str());
	std::puts("compare_canonical_body_note_free_verse_unchanged=ok");
}

/* Modules with no notes must render exactly as before.  Byte-for-byte
 * against the old read is the strongest statement available here. */
static void
test_modules_without_notes(SwordBackend &backend)
{
	static const char *const modules[] = {"SpaRVG", "SpaRV1909",
					      "NacarColunga", "TorresAmat"};
	static const char *const keys[] = {"Genesis 1:1", "Psalms 23:1",
					   "Matthew 11:30", "John 3:16"};
	int checked = 0;

	for (const char *module : modules) {
		if (!backend.hasModule(module))
			continue;
		for (const char *key : keys) {
			BibleKeyInfo info;
			if (!backend.resolveKey(module, key, info))
				continue;
			const std::string body =
				compare_body(backend, module, key);
			const std::string legacy =
				raw_strip(backend, module, info.key.c_str());
			g_assert_cmpstr(body.c_str(), ==, legacy.c_str());
			checked++;
		}
	}
	g_assert_cmpint(checked, >, 0);

	/* SpaRVG's quoted Psalm superscriptions are lifted out of the
	 * body into headings for the main view, which draws them.  The
	 * comparison panel draws no headings, so its text must still
	 * carry them -- 124 Psalm openings would otherwise go silently
	 * missing from a change that was only ever about notes. */
	if (backend.hasModule("SpaRVG")) {
		const std::string body =
			compare_body(backend, "SpaRVG", "Psalms 23:1");
		g_assert_true(contains(body, "Salmo de David"));
		g_assert_true(contains(body, "Jehová es mi pastor"));
		const std::string display =
			display_plain_text(backend, "SpaRVG", "Psalms 23:1");
		g_assert_false(contains(display, "Salmo de David"));
	}
	std::printf("compare_canonical_body_note_free_modules=ok checked=%d\n",
		    checked);
}

/* Comparar renders up to four versions in one pass, and the module in a
 * slot can be swapped while the panel is open.  Suppressing notes for a
 * body read must therefore be strictly scoped: it may not leak into the
 * next module's read, and it may not leave the main pane's own options
 * turned off behind it.
 *
 * Matthew 11:29 rather than 11:30 because NacarColunga's facsimile
 * merged 29 and 30 into one verse, leaving 30 empty in that module --
 * a property of the source, not of this behaviour. */
static void
test_several_modules_in_one_pass(SwordBackend &backend)
{
	static const char *const slots[] = {"SpaPlatense", "SpaRV1909",
					    "NacarColunga"};
	sword::SWMgr *mgr = backend.get_mgr();
	int rendered = 0;

	for (const char *module : slots) {
		if (!backend.hasModule(module))
			continue;
		const std::string body =
			compare_body(backend, module, "Matthew 11:29");
		g_assert_false(body.empty());
		g_assert_false(contains(body, "["));
		g_assert_cmpstr(mgr->getGlobalOption("Footnotes"), ==, "On");
		g_assert_cmpstr(mgr->getGlobalOption("Cross-references"), ==, "On");
		rendered++;
	}
	g_assert_cmpint(rendered, >, 1);

	/* The one of the three that carries a note carries only its verse. */
	const std::string platense =
		compare_body(backend, "SpaPlatense", "Matthew 11:29");
	g_assert_true(contains(platense, "Tomad sobre vosotros el yugo mío"));
	g_assert_false(contains(platense, "Nótese que no dice"));

	/* Swapping a slot's module mid-panel: read another module, then
	 * come back to the one that has notes. */
	if (backend.hasModule("SpaRVG")) {
		g_assert_false(compare_body(backend, "SpaRVG",
					    "Matthew 11:30").empty());
		const std::string again =
			compare_body(backend, "SpaPlatense", "Matthew 11:30");
		g_assert_true(contains(again, "Porque mi yugo es excelente"));
		g_assert_false(contains(again, NOTE_FRAGMENT));
	}

	/* And the main pane still gets its note markers afterwards. */
	g_assert_true(contains(main_pane_render(backend, "SpaPlatense",
						"Matthew 11:30"),
			       "showNote"));
	std::puts("compare_canonical_body_multi_module=ok");
}

int
main()
{
	SwordBackend backend;

	if (!backend.hasModule("SpaPlatense")) {
		std::puts("compare_canonical_body_skipped=no-SpaPlatense");
		return 0;
	}
	main_pane_options(backend);

	test_platense_verse_with_note(backend);
	test_platense_note_before_verse(backend);
	test_platense_verse_without_note(backend);
	test_modules_without_notes(backend);
	test_several_modules_in_one_pass(backend);

	std::puts("compare_canonical_body_test=ok");
	return 0;
}
