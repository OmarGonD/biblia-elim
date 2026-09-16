/*
 * TORRES-PSALM-TITLES-101 -- the psalm title structure Torres Amat really has.
 *
 * Measured, not assumed. Torres Amat numbers as the Vulgate, where a
 * psalm's superscription is a verse of its own and the printed page gives
 * it a number. The module therefore carries the title *inside verse 1*,
 * marked <seg type="x-psalm-title"> (SWORD renders it as
 * <span class="x-psalm-title">). Vulg Ps 52:1 holds the title and the
 * first line of the psalm in the same slot.
 *
 * Verse 0 is NOT a title slot here. Every chapter of every book has a
 * verse-0 entry, but in this Psalter it holds only the <chapter …/>
 * milestone that osis2mod writes -- no text at all. That is why this task
 * does not make navigation preserve ":0": there is nothing there to
 * preserve, and inventing a slot for it would send the reader to an empty
 * entry. The assertions below pin that as a measured fact, so the day a
 * module really does put a title in verse 0 this test is what notices.
 *
 * Whole-Psalter invariants are derived from the module, not from 150
 * hand-written expectations.
 *
 *   cmake --build build --target torresamat_psalter_structure_test && \
 *     ./build/tests/torresamat_psalter_structure_test
 */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include <swmodule.h>
#include <versekey.h>

#include "backend/sword/sword_backend.h"
#include "main/psalm_title.h"
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

static const char BIBLE[] = "TorresAmat";

/* The raw entry of one slot, addressed structurally: intros on and
 * autonormalise off, so verse 0 is asked for as verse 0 and nothing is
 * silently moved somewhere else. */
static std::string raw_slot(SwordBackend &backend, const char *book,
			    int chapter, int verse, bool *addressable)
{
	sword::SWModule *module = backend.get_SWModule(BIBLE);
	g_assert_nonnull(module);
	sword::VerseKey key;
	key.setVersificationSystem(backend.versification(BIBLE).c_str());
	key.setIntros(true);
	key.setAutoNormalize(0);
	gchar *text = g_strdup_printf("%s %d:%d", book, chapter, verse);
	key.setText(text);
	g_free(text);
	*addressable = !key.popError() && key.getChapter() == chapter &&
		       key.getVerse() == verse;
	if (!*addressable)
		return std::string();
	module->setKey(key);
	const char *raw = module->getRawEntry();
	return raw ? raw : "";
}

/* Whether an entry is nothing but the chapter milestone osis2mod writes
 * at every chapter start -- structural, no words involved. */
static bool only_chapter_milestone(const std::string &raw)
{
	if (raw.find("<chapter") == std::string::npos)
		return false;
	std::string rest;
	bool in_tag = false;
	for (char c : raw) {
		if (c == '<') in_tag = true;
		else if (c == '>') in_tag = false;
		else if (!in_tag && !g_ascii_isspace(c)) rest += c;
	}
	return rest.empty();
}

static int psalm_chapters(SwordBackend &backend)
{
	sword::VerseKey key;
	key.setVersificationSystem(backend.versification(BIBLE).c_str());
	key.setIntros(true);
	key.setText("Psalms 1:1");
	return key.getChapterMax();
}

int main()
{
	SwordBackend backend;

	if (!backend.hasModule(BIBLE)) {
		std::puts("torresamat_psalter_structure_skipped=no-TorresAmat");
		return 0;
	}
	g_assert_cmpstr(backend.versification(BIBLE).c_str(), ==, "Vulg");

	const int chapters = psalm_chapters(backend);
	g_assert_cmpint(chapters, ==, 150);

	/* ---- I. FULL PSALTER STRUCTURAL AUDIT ---- */
	int verse0_addressable = 0, verse0_with_entry = 0, verse0_with_text = 0;
	int verse1_with_entry = 0, marked_titles = 0;
	std::vector<int> title_psalms, verse0_text_psalms;

	for (int chapter = 1; chapter <= chapters; ++chapter) {
		bool addressable = false;
		const std::string v0 =
			raw_slot(backend, "Psalms", chapter, 0, &addressable);
		if (addressable) {
			++verse0_addressable;
			if (!v0.empty()) {
				++verse0_with_entry;
				if (!only_chapter_milestone(v0)) {
					++verse0_with_text;
					verse0_text_psalms.push_back(chapter);
				}
			}
		}

		bool v1_addressable = false;
		const std::string v1 =
			raw_slot(backend, "Psalms", chapter, 1, &v1_addressable);
		g_assert_true(v1_addressable);
		if (!v1.empty())
			++verse1_with_entry;
		if (v1.find("x-psalm-title") != std::string::npos) {
			++marked_titles;
			title_psalms.push_back(chapter);
		}
	}

	std::printf("psalter_chapters=%d verse0_addressable=%d "
		    "verse0_with_entry=%d verse0_with_text=%d "
		    "verse1_with_entry=%d marked_titles=%d\n",
		    chapters, verse0_addressable, verse0_with_entry,
		    verse0_with_text, verse1_with_entry, marked_titles);
	std::printf("marked_title_psalms=");
	for (int c : title_psalms) std::printf(" %d", c);
	std::printf("\n");

	/* The measured shape of this Psalter: verse 0 is addressable and
	 * present everywhere, and everywhere it is only the milestone. If
	 * that ever stops being true this assertion is what says so. */
	g_assert_cmpint(verse0_addressable, ==, chapters);
	g_assert_cmpint(verse0_with_entry, ==, chapters);
	g_assert_cmpint(verse0_with_text, ==, 0);
	g_assert_true(verse0_text_psalms.empty());

	/* Titles exist, and they live in verse 1. */
	g_assert_cmpint(marked_titles, >, 0);
	std::puts("psalter_audit=ok");

	/* ---- The title is really a title, and only the title ---- */
	for (int chapter : title_psalms) {
		bool ok = false;
		const std::string v1 =
			raw_slot(backend, "Psalms", chapter, 1, &ok);
		g_assert_true(ok);
		sword::SWModule *module = backend.get_SWModule(BIBLE);
		const std::string rendered = module->renderText().c_str();

		/* SWORD turns the seg into the class the renderer keys on. */
		g_assert_true(rendered.find("class=\"x-psalm-title\"") !=
			      std::string::npos);

		const PsalmTitleParts parts = splitPsalmTitle(rendered);
		g_assert_true(parts.hasTitle);

		/* Presented once, never duplicated, and the body (when the
		 * slot carries one) is separated rather than run together. */
		const std::string shown = psalmTitleVerseHtml(parts);
		g_assert_cmpint((int)shown.find("x-psalm-title"), >=, 0);
		g_assert_true(shown.find("x-psalm-title") ==
			      shown.rfind("x-psalm-title"));
		if (parts.hasBody)
			g_assert_true(shown.find("<br/>") != std::string::npos);
		else
			g_assert_true(shown.find("<br/>") == std::string::npos);
	}
	std::puts("psalter_titles_render=ok");

	/* ---- B/C/G. Key validation semantics, unchanged by this task ----
	 *
	 * BackEnd::get_valid_key() is the one validator every navigation
	 * goes through. It is generic: Bibles, commentaries, bad input.
	 * This task does not change it, because nothing in this Psalter
	 * justifies it -- and these assertions are what keep an ordinary
	 * verse ordinary if someone later tries. */
	/* C's reference is derived from the module, never guessed: the
	 * previous psalm's last verse is whatever Vulg says it is. */
	sword::VerseKey psalm117;
	psalm117.setVersificationSystem(backend.versification(BIBLE).c_str());
	psalm117.setIntros(true);
	psalm117.setText("Psalms 117:1");
	gchar *previous_last =
		g_strdup_printf("Psalms 117:%d", psalm117.getVerseMax());
	std::printf("previous_psalm_last_verse=%s\n", previous_last);

	struct { const char *key; const char *expect; } unchanged[] = {
		{ "Psalms 118:1", "Psalms 118:1" },   /* B: verse 1 */
		{ previous_last, previous_last },     /* C: previous psalm's last verse */
		{ "Psalms 150:1", "Psalms 150:1" },   /* G: normal navigation */
		{ "Psalms 3:1", "Psalms 3:1" },       /* a real title slot's verse */
		{ "Psalms 52:1", "Psalms 52:1" },     /* title + body slot */
		{ "Genesis 1:1", "Genesis 1:1" },
		{ "Matthew 11:7", "Matthew 11:7" },
	};
	for (const auto &c : unchanged) {
		char *valid = backend.get_valid_key(BIBLE, c.key);
		g_assert_nonnull(valid);
		g_assert_cmpstr(valid, ==, c.expect);
		free(valid);
	}
	g_free(previous_last);
	std::puts("key_validation_unchanged=ok");

	/* C, stated as the invariant it really is: the slot verse 0
	 * normalises onto is the previous psalm's last verse, and that
	 * verse is still itself -- real text, addressable under its own
	 * number. Nothing about titles may consume it. */
	{
		bool ok = false;
		const std::string last =
			raw_slot(backend, "Psalms", 117,
				 psalm117.getVerseMax(), &ok);
		g_assert_true(ok);
		g_assert_false(last.empty());
		g_assert_true(last.find("x-psalm-title") == std::string::npos);
		std::puts("previous_last_verse_intact=ok");
	}

	/* ---- D. No phantom title slot ----
	 *
	 * Verse 0 carries no title anywhere in this Psalter, so asking for
	 * it must not start resolving to a title slot. The current
	 * behaviour (it normalises away) is recorded rather than blessed:
	 * what matters is that it does NOT come back as a verse-0 key the
	 * reader could be sent to, since there is no text there. */
	for (const char *key : { "Psalms 118:0", "Psalms 3:0", "Genesis 1:0" }) {
		char *valid = backend.get_valid_key(BIBLE, key);
		g_assert_nonnull(valid);
		g_assert_cmpstr(valid, !=, key);
		std::printf("phantom_check %s -> %s\n", key, valid);
		free(valid);
	}
	std::puts("no_phantom_title_slot=ok");

	/* ---- F. Cross-v11n boundary, only the boundary ----
	 *
	 * V11N-URI-NAV-101 converts KJV Psalms 119:0 to Vulg Psalms 118:0.
	 * That contract is unchanged and is not retested here; what is
	 * checked is the condition this task attaches to it: the title-slot
	 * preservation would apply "if and only if TorresAmat really has
	 * that title slot". It does not -- Ps 118:0 is the milestone -- so
	 * no title-aware preservation is owed, and none was added. */
	if (backend.hasModule("SpaRV")) {
		const BibleModuleTransitionPlan plan = planBibleModuleTransition(
			backend, "SpaRV", "Psalms 119:0", BIBLE);
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		g_assert_cmpstr(plan.key.c_str(), ==, "Psalms 118:0");

		bool addressable = false;
		const std::string target =
			raw_slot(backend, "Psalms", 118, 0, &addressable);
		g_assert_true(addressable);
		g_assert_true(only_chapter_milestone(target));
		std::puts("cross_v11n_boundary=converted-but-no-title-slot-there");
	}

	/* ---- H. A really invalid reference is not made acceptable ----
	 *
	 * What the generic validator does with nonsense is its own,
	 * pre-existing business, and it is odd: SWORD's VerseKey does not
	 * report failure, it clamps to the end of the versification
	 * ("Laodiceans 1:1" in Vulg) or wanders ("Psalms 118:9999" comes
	 * back as "I Maccabees 6:6"). That is recorded here, not blessed,
	 * and it is untouched by this task.
	 *
	 * The invariant this task owes is narrower and is what would break
	 * if someone "fixed" verse 0 by loosening normalisation: nonsense
	 * must not start coming back as itself, and must never come back
	 * as a verse-0 slot. */
	for (const char *bad : { "Nowhere 9:9", "", "Psalms 999:1",
				 "Psalms 118:9999", "zzz" }) {
		char *valid = backend.get_valid_key(BIBLE, bad);
		g_assert_nonnull(valid);
		const std::string got(valid);
		std::printf("invalid_key [%s] -> [%s]\n", bad, valid);
		g_assert_cmpstr(valid, !=, bad);
		g_assert_true(got.find(":0") == std::string::npos);
		free(valid);
	}
	std::puts("invalid_key_not_made_acceptable=ok");

	std::puts("torresamat_psalter_structure_test=ok");
	return 0;
}
