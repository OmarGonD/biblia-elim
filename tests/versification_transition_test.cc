/*
 * Carrying a reference between modules with different versifications.
 *
 * The mapping part runs against libsword's versification tables alone.
 * The module part uses the installed Bibles (SpaRV = KJV, TorresAmat and
 * SpaPlatense = Vulg, NacarColunga = NRSVA) and skips each case whose
 * modules are missing.
 *
 *   cmake --build build --target versification_transition_test && \
 *     ./build/tests/versification_transition_test
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

struct Mapped {
	BibleReferenceMapping status;
	std::string book;
	int chapter;
	int verse;
};

static Mapped map_ref(const char *from, const char *text, const char *to)
{
	sword::VerseKey source;
	source.setVersificationSystem(from);
	source.setIntros(true);
	source.setAutoNormalize(0);
	source.setText(text);
	g_assert_false(source.popError());
	sword::VerseKey target;
	target.setVersificationSystem(to);
	Mapped result;
	result.status = swordMapVerseKey(source, target);
	result.book = target.getOSISBookName();
	result.chapter = target.getChapter();
	result.verse = target.getVerse();
	return result;
}

static void expect_map(const char *from, const char *text, const char *to,
		       const char *book, int chapter, int verse)
{
	const Mapped m = map_ref(from, text, to);
	if (m.status != BibleReferenceMapping::Mapped || m.book != book ||
	    m.chapter != chapter || m.verse != verse) {
		g_error("%s %s -> %s: got %s %d:%d (status %d), want %s %d:%d",
			from, text, to, m.book.c_str(), m.chapter, m.verse,
			(int)m.status, book, chapter, verse);
	}
}

static void expect_unmapped(const char *from, const char *text, const char *to)
{
	const Mapped m = map_ref(from, text, to);
	if (m.status != BibleReferenceMapping::Unmapped)
		g_error("%s %s -> %s: expected Unmapped, got %s %d:%d",
			from, text, to, m.book.c_str(), m.chapter, m.verse);
}

static void test_psalm_mapping(void)
{
	expect_map("KJV", "Psalms 118:10", "Vulg", "Ps", 117, 10);
	expect_map("KJV", "Psalms 119:1", "Vulg", "Ps", 118, 1);
	expect_map("KJV", "Psalms 120:1", "Vulg", "Ps", 119, 1);
	expect_map("KJV", "Psalms 147:12", "Vulg", "Ps", 147, 1);

	expect_map("Vulg", "Psalms 118:1", "KJV", "Ps", 119, 1);
	expect_map("Vulg", "Psalms 119:1", "KJV", "Ps", 120, 1);

	expect_map("KJV", "Psalms 119:1", "KJV", "Ps", 119, 1);
	expect_map("Vulg", "Psalms 118:1", "Vulg", "Ps", 118, 1);

	/* NRSVA and KJVA number the Psalms like KJV. */
	expect_map("NRSVA", "Psalms 119:1", "Vulg", "Ps", 118, 1);
	expect_map("KJVA", "Psalms 119:1", "Vulg", "Ps", 118, 1);
}

/* Verse by verse: mapping the chapter once and counting verses from
 * there would put 147:12..20 in Vulgate 146. */
static void test_psalm_147_verse_by_verse(void)
{
	for (int verse = 1; verse <= 20; ++verse) {
		gchar *ref = g_strdup_printf("Psalms 147:%d", verse);
		if (verse <= 11)
			expect_map("KJV", ref, "Vulg", "Ps", 146, verse);
		else
			expect_map("KJV", ref, "Vulg", "Ps", 147, verse - 11);
		g_free(ref);
	}
}

static void test_deuterocanon(void)
{
	expect_map("NRSVA", "Tobit 1:1", "Vulg", "Tob", 1, 1);
	expect_map("KJVA", "Sirach 1:1", "Vulg", "Sir", 1, 1);
	expect_map("NRSVA", "Baruch 6:1", "Vulg", "Bar", 6, 1);
	expect_map("Vulg", "Tobit 1:1", "NRSVA", "Tob", 1, 1);
	/* KJV has no Tobit: explicit failure, not some other book. */
	expect_unmapped("NRSVA", "Tobit 1:1", "KJV");
}

static void test_unmapped_is_explicit(void)
{
	/* No Vulgate counterpart in SWORD's tables for this KJV verse. */
	expect_unmapped("KJV", "Psalms 13:6", "Vulg");
}

/* The intro slot follows the chapter its first verse lands in, not the
 * chapter number: KJV Psalm 51 is Vulgate Psalm 50. */
static void test_intro_follows_first_verse(void)
{
	expect_map("KJV", "Psalms 119:0", "Vulg", "Ps", 118, 0);
	expect_map("KJV", "Psalms 51:0", "Vulg", "Ps", 50, 0);
	expect_map("KJV", "Psalms 3:0", "Vulg", "Ps", 3, 0);
	expect_map("KJV", "Genesis 1:0", "Vulg", "Gen", 1, 0);
}

static int skipped = 0;

static bool have(SwordBackend &backend, const char *module)
{
	if (backend.hasModule(module))
		return true;
	std::printf("versification_transition_skipped_case=missing-%s\n", module);
	++skipped;
	return false;
}

static void expect_transition(SwordBackend &backend, const char *source,
			      const char *key, const char *target,
			      int chapter, int verse, int verse_count,
			      const char *text_fragment)
{
	if (!have(backend, source) || !have(backend, target))
		return;
	sword::SWModule *target_module = backend.get_SWModule(target);
	sword::SWKey *const target_key_ptr = target_module->getKey();
	const std::string target_key_text = target_module->getKeyText();

	const BibleModuleTransitionPlan plan =
		planBibleModuleTransition(backend, source, key, target);
	if (plan.status != BibleModuleTransition::Converted)
		g_error("%s %s -> %s: status %d", source, key, target,
			(int)plan.status);
	/* Conversion works on keys of its own, never the modules' live
	 * keys the chapter renderer holds. */
	g_assert_true(target_module->getKey() == target_key_ptr);
	g_assert_cmpstr(target_module->getKeyText(), ==, target_key_text.c_str());

	/* What the navbar reads after the switch: the target's own
	 * reference and counts. */
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(target, plan.key, info));
	if (info.reference.chapter != chapter || info.reference.verse != verse)
		g_error("%s %s -> %s: got %s (%d:%d), want %d:%d", source, key,
			target, plan.key.c_str(), info.reference.chapter,
			info.reference.verse, chapter, verse);
	g_assert_cmpint(info.verseCount, ==, verse_count);

	if (text_fragment) {
		const std::string text = backend.getText(target, plan.key, false);
		if (text.find(text_fragment) == std::string::npos)
			g_error("%s %s -> %s %s: text lacks \"%s\": %s", source,
				key, target, plan.key.c_str(), text_fragment,
				text.c_str());
	}

	std::printf("transition %s %s -> %s %s ok\n", source, key, target,
		    plan.key.c_str());
}

static void test_module_transitions(SwordBackend &backend)
{
	expect_transition(backend, "SpaRV", "Psalms 119:1", "TorresAmat",
			  118, 1, 176, "Bienaventurados");
	expect_transition(backend, "TorresAmat", "Psalms 118:1", "SpaRV",
			  119, 1, 176, "BIENAVENTURADOS");
	expect_transition(backend, "TorresAmat", "Psalms 119:1", "SpaRV",
			  120, 1, 7, nullptr);
	expect_transition(backend, "SpaRV", "Psalms 147:12", "TorresAmat",
			  147, 1, 9, "Jerusalem");
	expect_transition(backend, "SpaRV", "Psalms 119:1", "SpaPlatense",
			  118, 1, 176, "Dichosos");
	expect_transition(backend, "NacarColunga", "Tobit 1:1", "TorresAmat",
			  1, 1, 25, nullptr);

	if (have(backend, "SpaRV")) {
		const BibleModuleTransitionPlan same = planBibleModuleTransition(
			backend, "SpaRV", "Psalms 119:1", "SpaRV");
		g_assert_true(same.status == BibleModuleTransition::SameModule);
		g_assert_cmpstr(same.key.c_str(), ==, "Psalms 119:1");
	}

	if (have(backend, "SpaRV") && have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan none = planBibleModuleTransition(
			backend, "SpaRV", "Psalms 13:6", "TorresAmat");
		g_assert_true(none.status == BibleModuleTransition::Unmapped);
		g_assert_true(none.key.empty());

		const BibleReferenceConversion intro = backend.convertReference(
			"SpaRV", "Psalms 119:0", "TorresAmat");
		g_assert_true(intro.status == BibleReferenceMapping::Mapped);
		g_assert_cmpint(intro.target.reference.chapter, ==, 118);
		g_assert_cmpint(intro.target.reference.verse, ==, 0);
	}

	if (have(backend, "SpaRV")) {
		const BibleModuleTransitionPlan missing = planBibleModuleTransition(
			backend, "SpaRV", "Psalms 119:1", "NoSuchModule");
		g_assert_true(missing.status == BibleModuleTransition::Invalid);
	}
}

/* The main Bible was uninstalled: only its versification name is left. */
static void test_removed_module_transition(SwordBackend &backend)
{
	if (have(backend, "SpaRV")) {
		const BibleModuleTransitionPlan plan =
			planBibleVersificationTransition(backend, "Vulg",
							 "Psalms 118:1", "SpaRV");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		BibleKeyInfo info;
		g_assert_true(backend.resolveKey("SpaRV", plan.key, info));
		g_assert_cmpint(info.reference.chapter, ==, 119);
		g_assert_cmpint(info.reference.verse, ==, 1);
	}
	if (have(backend, "TorresAmat")) {
		sword::SWModule *m = backend.get_SWModule("TorresAmat");
		sword::SWKey *const ptr = m->getKey();
		const std::string text = m->getKeyText();

		const BibleModuleTransitionPlan plan =
			planBibleVersificationTransition(backend, "KJV",
							 "Psalms 119:1", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		BibleKeyInfo info;
		g_assert_true(backend.resolveKey("TorresAmat", plan.key, info));
		g_assert_cmpint(info.reference.chapter, ==, 118);
		g_assert_cmpint(info.reference.verse, ==, 1);

		const BibleModuleTransitionPlan none =
			planBibleVersificationTransition(backend, "KJV",
							 "Psalms 13:6", "TorresAmat");
		g_assert_true(none.status == BibleModuleTransition::Unmapped);

		/* An unknown name must not quietly become KJV. */
		const BibleModuleTransitionPlan unknown =
			planBibleVersificationTransition(backend, "NoSuchV11n",
							 "Psalms 119:1", "TorresAmat");
		g_assert_true(unknown.status == BibleModuleTransition::Invalid);
		const BibleModuleTransitionPlan empty =
			planBibleVersificationTransition(backend, "",
							 "Psalms 119:1", "TorresAmat");
		g_assert_true(empty.status == BibleModuleTransition::Invalid);

		g_assert_true(m->getKey() == ptr);
		g_assert_cmpstr(m->getKeyText(), ==, text.c_str());
	}
	std::puts("removed_module_transition=ok");
}

/* BOOKMARK-V11N-101: a bookmark saved without a module is KJV numbering. */
static void expect_legacy(SwordBackend &backend, const char *key,
			  const char *target, const char *want)
{
	if (!have(backend, target))
		return;
	const BibleModuleTransitionPlan plan =
		planLegacyBookmarkKeyList(backend, key, target);
	if (!want) {
		if (plan.status != BibleModuleTransition::Unmapped)
			g_error("legacy %s -> %s: expected Unmapped, got %s", key,
				target, plan.key.c_str());
		return;
	}
	if ((plan.status != BibleModuleTransition::Converted &&
	     plan.status != BibleModuleTransition::SameModule) ||
	    plan.key != want)
		g_error("legacy %s -> %s: got \"%s\" (status %d), want \"%s\"",
			key, target, plan.key.c_str(), (int)plan.status, want);
	std::printf("legacy bookmark %s -> %s %s ok\n", key, target, want);
}

static void test_legacy_bookmarks(SwordBackend &backend)
{
	g_assert_cmpstr(kLegacyBookmarkVersification, ==, "KJV");
	/* Same verse in either Bible: KJV identity, Vulgate converted. */
	expect_legacy(backend, "Psalms 119:1", "SpaRV", "Psalms 119:1");
	expect_legacy(backend, "Psalms 119:1", "TorresAmat", "Psalms 118:1");
	expect_legacy(backend, "Psalms 119:1", "SpaPlatense", "Psalms 118:1");
	if (have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan one =
			planLegacyBookmarkKey(backend, "Psalms 119:1", "TorresAmat");
		g_assert_true(one.status == BibleModuleTransition::Converted);
		BibleKeyInfo info;
		g_assert_true(backend.resolveKey("TorresAmat", one.key, info));
		g_assert_cmpint(info.reference.chapter, ==, 118);
		g_assert_cmpint(info.reference.verse, ==, 1);
	}
	/* Lists, ranges and comma lists, item by item. */
	expect_legacy(backend, "Psalms 119:1-5; Psalms 121:1", "TorresAmat",
		      "Psalms 118:1-5; Psalms 120:1");
	expect_legacy(backend, "Ephesians 2:8,9", "SpaRV",
		      "Ephesians 2:8; Ephesians 2:9");
	expect_legacy(backend, "Psalms 147:10-12", "TorresAmat",
		      "Psalms 146:10-147:1");
	/* No counterpart: explicit, never the same numbers reread. */
	expect_legacy(backend, "Psalms 13:6", "TorresAmat", nullptr);
	expect_legacy(backend, "Psalms 119:1; Psalms 13:6", "TorresAmat",
		      nullptr);
	std::puts("legacy_bookmarks=ok");
}

/* Each edition's own notes: same versification as the edition, so the
 * conversion is identity; from another Bible it maps. */
static void test_author_commentaries(SwordBackend &backend)
{
	g_assert_null(authorCommentaryForBible("SpaRV"));
	for (const char *bible : {"SpaPlatense", "NacarColunga", "TorresAmat"}) {
		const char *notes = authorCommentaryForBible(bible);
		g_assert_nonnull(notes);
		if (!have(backend, bible) || !have(backend, notes))
			continue;
		g_assert_cmpstr(backend.versification(bible).c_str(), ==,
				backend.versification(notes).c_str());
		for (const char *key : {"Psalms 118:1", "Psalms 147:1",
					"Matthew 5:1", "Psalms 3:1"}) {
			const BibleModuleTransitionPlan plan =
				planBibleModuleTransition(backend, bible, key, notes);
			g_assert_true(plan.status == BibleModuleTransition::Converted);
			BibleKeyInfo from, to;
			g_assert_true(backend.resolveKey(bible, key, from));
			g_assert_true(backend.resolveKey(notes, plan.key, to));
			if (from.osisBook != to.osisBook ||
			    from.reference.chapter != to.reference.chapter ||
			    from.reference.verse != to.reference.verse)
				g_error("%s %s -> %s %s: not identity", bible, key,
					notes, plan.key.c_str());
		}
		std::printf("author_commentary %s -> %s identity ok\n", bible, notes);
	}
	if (have(backend, "SpaRV") && have(backend, "TorresAmatNotas")) {
		const BibleModuleTransitionPlan plan = planBibleModuleTransition(
			backend, "SpaRV", "Psalms 119:1", "TorresAmatNotas");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		BibleKeyInfo info;
		g_assert_true(backend.resolveKey("TorresAmatNotas", plan.key, info));
		g_assert_cmpint(info.reference.chapter, ==, 118);
		g_assert_cmpint(info.reference.verse, ==, 1);
	}
	std::puts("author_commentaries=ok");
}

int main()
{
	test_psalm_mapping();
	test_psalm_147_verse_by_verse();
	test_deuterocanon();
	test_unmapped_is_explicit();
	test_intro_follows_first_verse();
	std::puts("versification_mapping=ok");

	SwordBackend backend;
	test_module_transitions(backend);
	test_removed_module_transition(backend);
	test_author_commentaries(backend);
	test_legacy_bookmarks(backend);
	std::printf("versification_transition=ok skipped_cases=%d\n", skipped);
	return 0;
}
