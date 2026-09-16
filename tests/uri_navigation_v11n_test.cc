/*
 * V11N-URI-NAV-101 -- what a module-less "sword:///KEY" URI may carry.
 *
 * Such a URI names no module, so sword_uri() navigates whatever Bible is
 * currently selected and KEY is read in *that* module's versification.
 * Emitters whose key comes from elsewhere used to interpolate it raw:
 *
 *     sidebar verse list   (native to the module that published the xref)
 *     Strong's occurrences (native to the concordance module, KJV/Tisch)
 *     Bible dialog "sync"  (native to the dialog's own module)
 *
 * Between Vulgate and KJV numbering that lands on a different psalm: the
 * reference says Psalm 119 in KJV numbering and the Vulgate Bible reads
 * its own Psalm 119, which is KJV 120.
 *
 * The pairs below are the ones tests/versification_transition_test.cc
 * already verifies against libsword's tables, reused rather than
 * reinvented. Modules: SpaRV = KJV, TorresAmat and SpaPlatense = Vulg,
 * NacarColunga = NRSVA, EsWiktionary = a dictionary (no versification).
 */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

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

static int skipped;

static bool have(SwordBackend &backend, const char *module)
{
	if (backend.hasModule(module))
		return true;
	std::printf("uri_navigation_v11n_skipped_case=missing-%s\n", module);
	++skipped;
	return false;
}

/* What the emitter builds the URI from. */
static BibleModuleTransitionPlan uri_key(SwordBackend &backend,
					 const char *source, const char *key,
					 const char *main_module)
{
	return planUriKeyForMainBible(backend, source ? source : "", key,
				      main_module ? main_module : "");
}

/* The reference the URI carries, resolved in the Bible it will navigate:
 * where the reader actually lands. */
static void landed(SwordBackend &backend, const char *main_module,
		   const std::string &key, int *chapter, int *verse)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(main_module, key, info));
	*chapter = info.reference.chapter;
	*verse = info.reference.verse;
}

static void expect_lands(SwordBackend &backend, const char *source,
			 const char *key, const char *main_module,
			 int chapter, int verse)
{
	if (!have(backend, source) || !have(backend, main_module))
		return;
	const BibleModuleTransitionPlan plan =
		uri_key(backend, source, key, main_module);
	if (plan.status != BibleModuleTransition::SameModule &&
	    plan.status != BibleModuleTransition::Converted)
		g_error("%s %s -> %s: status %d", source, key, main_module,
			(int)plan.status);

	int got_chapter = 0, got_verse = 0;
	landed(backend, main_module, plan.key, &got_chapter, &got_verse);
	if (got_chapter != chapter || got_verse != verse)
		g_error("%s %s via sword:///%s -> %s: landed %d:%d, want %d:%d",
			source, key, plan.key.c_str(), main_module, got_chapter,
			got_verse, chapter, verse);
	std::printf("uri %s %s -> %s %s ok\n", source, key, main_module,
		    plan.key.c_str());
}

int main()
{
	SwordBackend backend;

	/* A. SAME-V11N: source is the Bible on screen. The key is used
	 * exactly as given -- no conversion is introduced where there was
	 * none, so verse 0, subverses and chapter-only refs are untouched. */
	if (have(backend, "SpaRV")) {
		static const char *const refs[] = {
			"Psalms 119:1", "Genesis 1:1", "Matthew 11:7",
			"Psalms 119:0", "Genesis 1", nullptr
		};
		for (int i = 0; refs[i]; ++i) {
			const BibleModuleTransitionPlan plan =
				uri_key(backend, "SpaRV", refs[i], "SpaRV");
			g_assert_true(plan.status ==
				      BibleModuleTransition::SameModule);
			g_assert_cmpstr(plan.key.c_str(), ==, refs[i]);
		}
		std::puts("uri_same_module=identity");
	}

	/* B. CROSS-V11N KJV -> Vulg. The whole bug in one line: the raw
	 * key must NOT be what the URI carries, because the Vulgate Bible
	 * would read its own Psalm 119 (which is KJV 120). */
	if (have(backend, "SpaRV") && have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "SpaRV", "Psalms 119:1", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		g_assert_cmpstr(plan.key.c_str(), !=, "Psalms 119:1");

		int chapter = 0, verse = 0;
		landed(backend, "TorresAmat", plan.key, &chapter, &verse);
		g_assert_cmpint(chapter, ==, 118);
		g_assert_cmpint(verse, ==, 1);

		/* and what the old code did, spelled out: handing the raw
		 * text to the target lands somewhere else entirely. */
		int raw_chapter = 0, raw_verse = 0;
		landed(backend, "TorresAmat", "Psalms 119:1", &raw_chapter,
		       &raw_verse);
		g_assert_cmpint(raw_chapter, ==, 119);
		g_assert_cmpint(raw_chapter, !=, chapter);
		std::puts("uri_cross_kjv_to_vulg=ok");
	}
	expect_lands(backend, "SpaRV", "Psalms 119:1", "SpaPlatense", 118, 1);
	expect_lands(backend, "SpaRV", "Psalms 147:12", "TorresAmat", 147, 1);

	/* C. CROSS-V11N Vulg -> KJV, the same thing in reverse. */
	expect_lands(backend, "TorresAmat", "Psalms 118:1", "SpaRV", 119, 1);
	expect_lands(backend, "TorresAmat", "Psalms 119:1", "SpaRV", 120, 1);

	/* D. CROSS REFERENCE: a verse list built for the module that
	 * published it, double-clicked while another Bible is selected.
	 * NacarColunga is NRSVA, which numbers the Psalms like KJV. */
	expect_lands(backend, "NacarColunga", "Psalms 119:1", "TorresAmat",
		     118, 1);
	expect_lands(backend, "TorresAmat", "Psalms 118:1", "NacarColunga",
		     119, 1);

	/* E. Verse 0 -- and the boundary of what this task owns.
	 *
	 * The conversion carries the intro slot across as verse 0: KJV
	 * Psalms 119:0 becomes Vulgate Psalms 118:0, following the chapter
	 * its first verse lands in. That key is what the URI carries, and
	 * it is what this task is responsible for.
	 *
	 * Downstream, the key validator every navigation goes through
	 * (BackEnd::get_valid_key(), which is what main_update_nav_controls()
	 * calls in the running application, and resolveKey() on the other
	 * branch) runs a VerseKey with autonormalise on and intros off, so
	 * verse 0 is not a slot it will keep: "Psalms 118:0" comes back as
	 * "Psalms 117:29".
	 *
	 * That is NOT a versification problem and not something this task
	 * introduced -- the assertion below proves it by doing the same
	 * thing with no conversion at all, in the source module: SpaRV
	 * "Psalms 119:0" collapses to 118:29 just the same. Title-slot
	 * navigation belongs to the psalm-title work, not to URI routing.
	 *
	 * So what is pinned here is the real contract: the URI conversion
	 * is correct, and it adds no verse-0 loss of its own. This stays
	 * true whether or not title-slot navigation is fixed later.
	 */
	if (have(backend, "SpaRV") && have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "SpaRV", "Psalms 119:0", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		g_assert_cmpstr(plan.key.c_str(), !=, "Psalms 119:0");

		/* what this task delivers: the intro slot of the right psalm */
		const BibleReferenceConversion conv = backend.convertReference(
			"SpaRV", "Psalms 119:0", "TorresAmat");
		g_assert_true(conv.status == BibleReferenceMapping::Mapped);
		g_assert_cmpstr(conv.target.key.c_str(), ==, plan.key.c_str());
		g_assert_cmpint(conv.target.reference.chapter, ==, 118);
		g_assert_cmpint(conv.target.reference.verse, ==, 0);
		std::printf("uri_verse_zero_conversion=%s (chapter %d verse %d)\n",
			    plan.key.c_str(), conv.target.reference.chapter,
			    conv.target.reference.verse);

		/* the boundary: the shared key validator, measured on the
		 * converted key and on a key that was never converted. */
		gchar *converted_valid =
			backend.get_valid_key("TorresAmat", plan.key.c_str());
		gchar *same_module_valid =
			backend.get_valid_key("SpaRV", "Psalms 119:0");
		BibleKeyInfo converted_info, same_module_info;
		g_assert_true(backend.resolveKey("TorresAmat", plan.key,
						 converted_info));
		g_assert_true(backend.resolveKey("SpaRV", "Psalms 119:0",
						 same_module_info));
		std::printf("uri_verse_zero_downstream converted=[%s] %d:%d "
			    "same_module=[%s] %d:%d\n",
			    converted_valid, converted_info.reference.chapter,
			    converted_info.reference.verse, same_module_valid,
			    same_module_info.reference.chapter,
			    same_module_info.reference.verse);

		/* Conversion adds no verse-0 loss: whatever the validator
		 * does to the intro slot, it does with or without a
		 * versification crossing. */
		g_assert_cmpint(converted_info.reference.verse == 0, ==,
				same_module_info.reference.verse == 0);
		g_free(converted_valid);
		g_free(same_module_valid);
		std::puts("uri_verse_zero=converted-correctly;"
			  "downstream-title-slot-navigation-out-of-scope");
	}

	/* F. FAILED CONVERSION: no counterpart means no URI at all. The
	 * raw string is never offered to the target as a plan B, and no
	 * nearby verse is substituted.
	 *
	 * Why this pair has no mapping, measured rather than assumed:
	 * SpaRV (KJV) gives Psalm 13 six verses, while its Vulgate
	 * counterpart is Psalm 12, which TorresAmat gives six verses of
	 * which the first is the title. KJV 13:5 already maps to Vulg
	 * 12:6, the last one, so KJV 13:6 has no slot left and SWORD's
	 * table reports Unmapped. versification_transition_test.cc pins
	 * the same pair at the mapping layer; here it is pinned at the
	 * URI layer, where it has to mean "do not navigate".
	 *
	 * The verse is ordinary text a reader can really land a verse
	 * list on ("Cantare a Jehova, porque me ha hecho bien"), so this
	 * is a reachable situation, not a synthetic one. */
	if (have(backend, "SpaRV") && have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "SpaRV", "Psalms 13:6", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::Unmapped);
		g_assert_true(plan.key.empty());
		std::puts("uri_unmapped=explicit");
	}
	if (have(backend, "SpaRV")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "SpaRV", "Psalms 119:1", "NoSuchModule");
		g_assert_true(plan.status != BibleModuleTransition::SameModule &&
			      plan.status != BibleModuleTransition::Converted);
		std::puts("uri_missing_target=explicit");
	}

	/* G. SAME-V11N across different modules: SpaRV and SpaRVG are both
	 * KJV-numbered, so the reference comes through unchanged. */
	if (have(backend, "SpaRV") && have(backend, "SpaRVG")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "SpaRV", "Psalms 119:1", "SpaRVG");
		g_assert_true(plan.status == BibleModuleTransition::Converted);
		int chapter = 0, verse = 0;
		landed(backend, "SpaRVG", plan.key, &chapter, &verse);
		g_assert_cmpint(chapter, ==, 119);
		g_assert_cmpint(verse, ==, 1);
		expect_lands(backend, "SpaRV", "Genesis 1:1", "SpaRVG", 1, 1);
		expect_lands(backend, "SpaRV", "Matthew 11:7", "SpaRVG", 11, 7);
		std::puts("uri_same_v11n_other_module=unchanged");
	}

	/* H. NON-BIBLE sources keep their key untouched: a dictionary
	 * declares no versification, so there is nothing to convert from
	 * and the routing of those links is left exactly as it was. */
	if (have(backend, "EsWiktionary") && have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan plan = uri_key(
			backend, "EsWiktionary", "GISOFILA", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::SameModule);
		g_assert_cmpstr(plan.key.c_str(), ==, "GISOFILA");
		std::puts("uri_non_bible=untouched");
	}

	/* An emitter with no module to name (history, app-authored
	 * references) keeps the behaviour it had: the key is used as is. */
	if (have(backend, "TorresAmat")) {
		const BibleModuleTransitionPlan plan =
			uri_key(backend, "", "Psalms 118:1", "TorresAmat");
		g_assert_true(plan.status == BibleModuleTransition::SameModule);
		g_assert_cmpstr(plan.key.c_str(), ==, "Psalms 118:1");
		std::puts("uri_no_source_module=unchanged");
	}

	std::printf("uri_navigation_v11n_skipped=%d\n", skipped);
	std::puts("uri_navigation_v11n_test=ok");
	return 0;
}
