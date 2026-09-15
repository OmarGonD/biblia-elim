/* Next/previous verse on the installed SWORD modules, each in its own
 * versification: KJV (KJV, SpaRV), Vulg (TorresAmat), NRSVA (NacarColunga).
 * A module that is not installed is reported as skipped, not passed. */
#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include <swmodule.h>
#include <versekey.h>

#include "backend/content_availability.h"
#include "backend/content_resolver.h"
#include "backend/sword/sword_backend.h"
#include "main/settings.h"
#include "main/verse_navigation.h"

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

namespace {

int failures;
int skipped;

#define CHECK(condition) do { \
	if (!(condition)) { \
		std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		++failures; \
	} \
} while (0)

SwordBackend *nav_backend;

bool installed(const char *module)
{
	if (nav_backend->hasModule(module))
		return true;
	std::printf("skipped module=%s reason=not-installed\n", module);
	++skipped;
	return false;
}

VerseSlotNavigable navigableIn(const std::string &module)
{
	return [module](const BibleReference &reference) {
		return verseSlotNavigable(*nav_backend, module, reference);
	};
}

std::string canonical(const std::string &module, const std::string &key)
{
	BibleKeyInfo info;
	return nav_backend->resolveKey(module, key, info) ? info.key : std::string();
}

/* One arrow press in the app: resolver-aware step. */
VerseStep press(const std::string &module, const std::string &key, int direction)
{
	return stepVerse(*nav_backend, module, key, direction, navigableIn(module));
}

void expectStep(const char *module, const char *from, int direction,
	const char *to)
{
	const VerseStep step = press(module, from, direction);
	const std::string expected = canonical(module, to);
	if (step.status != VerseStepStatus::Valid || step.key != expected) {
		std::fprintf(stderr, "FAIL %s %s %s -> got '%s' (status %d), want '%s'\n",
			module, from, direction > 0 ? "DOWN" : "UP", step.key.c_str(),
			static_cast<int>(step.status), expected.c_str());
		++failures;
	}
}

void expectEdge(const char *module, const char *from, int direction,
	VerseStepStatus edge)
{
	const VerseStep step = press(module, from, direction);
	CHECK(step.status == edge);
	CHECK(step.key.empty());
}

void testKjvCrossings()
{
	for (const char *module : { "KJV", "SpaRV" }) {
		if (!installed(module))
			continue;
		expectStep(module, "Genesis 1:31", 1, "Genesis 2:1");
		expectStep(module, "Genesis 2:1", -1, "Genesis 1:31");
		expectStep(module, "Matthew 28:20", 1, "Mark 1:1");
		expectStep(module, "Mark 1:1", -1, "Matthew 28:20");
		expectStep(module, "Malachi 4:6", 1, "Matthew 1:1");
		expectStep(module, "Matthew 1:1", -1, "Malachi 4:6");
		expectStep(module, "Psalms 150:6", 1, "Proverbs 1:1");
		expectStep(module, "John 3:16", 1, "John 3:17");
		expectEdge(module, "Genesis 1:1", -1, VerseStepStatus::BeginningOfBible);
		expectEdge(module, "Revelation of John 22:21", 1,
			VerseStepStatus::EndOfBible);
	}
}

void testVulgTitleSlotsAndFallback()
{
	if (!installed("TorresAmat"))
		return;
	/* Native psalm-title slots are real verses. */
	expectStep("TorresAmat", "Psalms 3:2", -1, "Psalms 3:1");
	expectStep("TorresAmat", "Psalms 3:1", -1, "Psalms 2:13");
	expectStep("TorresAmat", "Psalms 2:13", 1, "Psalms 3:1");
	expectStep("TorresAmat", "Psalms 3:1", 1, "Psalms 3:2");
	expectStep("TorresAmat", "Psalms 50:21", 1, "Psalms 51:1");
	expectStep("TorresAmat", "Psalms 51:1", -1, "Psalms 50:21");
	expectStep("TorresAmat", "Genesis 1:31", 1, "Genesis 2:1");
	expectStep("TorresAmat", "Genesis 2:1", -1, "Genesis 1:31");

	/* Gen 2:24-25 have no raw text of their own and are filled by the
	 * fallback module: they are the last verses of the chapter and the
	 * arrows used to skip straight past them. */
	BibleReference gen_2_24;
	BibleKeyInfo info;
	CHECK(nav_backend->resolveKey("TorresAmat", "Genesis 2:24", info));
	gen_2_24 = info.reference;
	const BibleVerseContent resolved =
		resolveVerseContent(*nav_backend, "TorresAmat", gen_2_24);
	CHECK(resolved.isFallback);
	expectStep("TorresAmat", "Genesis 2:23", 1, "Genesis 2:24");
	expectStep("TorresAmat", "Genesis 2:24", 1, "Genesis 2:25");
	expectStep("TorresAmat", "Genesis 2:25", 1, "Genesis 3:1");
	expectStep("TorresAmat", "Genesis 3:1", -1, "Genesis 2:25");

	expectEdge("TorresAmat", "Genesis 1:1", -1, VerseStepStatus::BeginningOfBible);
}

void testNrsvaCrossings()
{
	if (!installed("NacarColunga"))
		return;
	expectStep("NacarColunga", "Genesis 1:31", 1, "Genesis 2:1");
	expectStep("NacarColunga", "Genesis 2:1", -1, "Genesis 1:31");
	expectStep("NacarColunga", "Matthew 28:20", 1, "Mark 1:1");
	expectStep("NacarColunga", "Mark 1:1", -1, "Matthew 28:20");
	expectStep("NacarColunga", "Psalms 3:8", 1, "Psalms 4:1");
	expectStep("NacarColunga", "Psalms 4:1", -1, "Psalms 3:8");
	expectStep("NacarColunga", "Exodus 40:38", 1, "Leviticus 1:1");
	expectStep("NacarColunga", "Leviticus 1:1", -1, "Exodus 40:38");
	expectEdge("NacarColunga", "Genesis 1:1", -1, VerseStepStatus::BeginningOfBible);
}

/* Verse 0 never becomes a stop, even when the walk starts on it. */
void testIntroSlotsSkipped()
{
	if (!installed("KJV"))
		return;
	const VerseStep step = stepVerse(*nav_backend, "KJV", "Genesis 2:0", 1);
	CHECK(step.status == VerseStepStatus::Valid);
	CHECK(step.reference.chapter == 2 && step.reference.verse == 1);
}

bool sameSlot(const BibleReference &a, const BibleReference &b)
{
	return a.testament == b.testament && a.book == b.book &&
	       a.chapter == b.chapter && a.verse == b.verse;
}

BibleReference nativeSlot(const sword::VerseKey &key)
{
	BibleReference reference;
	reference.testament = key.getTestament();
	reference.book = key.getBook();
	reference.chapter = key.getChapter();
	reference.verse = key.getVerse();
	return reference;
}

/* A native key positioned from an OSIS reference, which SWORD parses
 * unambiguously -- independent of the backend code under test. */
sword::VerseKey *nativeKey(const char *module, const char *osis)
{
	sword::SWModule *mod = nav_backend->get_SWModule(module);
	sword::VerseKey *vk = dynamic_cast<sword::VerseKey *>(mod->createKey());
	vk->setAutoNormalize(1);
	vk->setIntros(false);
	vk->setText(osis);
	return vk;
}

/* What an arrow press must reach, found by walking SWORD's own VerseKey:
 * the first slot in that direction the predicate accepts. */
bool expectedSlot(const char *module, const char *osis, int direction,
	const VerseSlotNavigable &navigable, BibleReference &found)
{
	sword::VerseKey *vk = nativeKey(module, osis);
	bool ok = false;
	for (int walked = 0; walked < 40000; ++walked) {
		if (direction > 0)
			++(*vk);
		else
			--(*vk);
		if (vk->popError())
			break;
		const BibleReference slot = nativeSlot(*vk);
		if (!navigable || navigable(slot)) {
			found = slot;
			ok = true;
			break;
		}
	}
	delete vk;
	return ok;
}

void expectBoundary(const char *module, const char *from_osis, int direction,
	const VerseSlotNavigable &navigable, const char *label)
{
	sword::VerseKey *from = nativeKey(module, from_osis);
	const std::string from_key = from->getOSISRef();
	const BibleReference from_slot = nativeSlot(*from);
	delete from;

	BibleReference expected;
	CHECK(expectedSlot(module, from_osis, direction, navigable, expected));
	const VerseStep step =
		stepVerse(*nav_backend, module, from_key, direction, navigable);
	BibleKeyInfo landed;
	const bool resolved = step.status == VerseStepStatus::Valid &&
		nav_backend->resolveKey(module, step.key, landed);
	std::printf("boundary %s %s %s %s -> '%s' osis=%s\n", module, label,
		from_osis, direction > 0 ? "DOWN" : "UP", step.key.c_str(),
		resolved ? landed.osisBook.c_str() : "-");
	CHECK(step.status == VerseStepStatus::Valid);
	CHECK(resolved);
	CHECK(sameSlot(step.reference, expected));
	CHECK(sameSlot(landed.reference, expected));
	CHECK(landed.osisBook != "Rev");

	/* and back: to the first navigable slot on the other side, which is
	 * where we started whenever the start is itself navigable */
	const VerseStep back =
		stepVerse(*nav_backend, module, step.key, -direction, navigable);
	BibleReference expected_back;
	CHECK(expectedSlot(module, landed.osisBook.empty() ? from_osis
		: (landed.osisBook + "." + std::to_string(landed.reference.chapter) +
		   "." + std::to_string(landed.reference.verse)).c_str(),
		-direction, navigable, expected_back));
	CHECK(back.status == VerseStepStatus::Valid);
	CHECK(sameSlot(back.reference, expected_back));
	if (!navigable || navigable(from_slot))
		CHECK(sameSlot(back.reference, from_slot));
	std::printf("boundary %s %s back -> '%s' start_navigable=%d\n", module,
		label, back.key.c_str(), (!navigable || navigable(from_slot)) ? 1 : 0);
}

/* The two boundaries of NRSVA's Greek Esther, which sits between Judith
 * and Wisdom. Its display name does not parse back ("Esther (Greek) 1:1"
 * reads as Revelation 1:1). */
void testNrsvaGreekEstherBoundaries()
{
	if (!installed("NacarColunga"))
		return;
	CHECK(nav_backend->versification("NacarColunga") == "NRSVA");

	/* every native slot: the book is entered and left */
	expectBoundary("NacarColunga", "Jdt.16.25", 1, VerseSlotNavigable(), "native");
	expectBoundary("NacarColunga", "Wis.1.1", -1, VerseSlotNavigable(), "native");
	{
		const VerseStep in = stepVerse(*nav_backend, "NacarColunga",
			"Jdt.16.25", 1);
		BibleKeyInfo info;
		CHECK(nav_backend->resolveKey("NacarColunga", in.key, info));
		CHECK(info.osisBook == "EsthGr");
		CHECK(info.reference.chapter == 1 && info.reference.verse == 1);
		const VerseStep out = stepVerse(*nav_backend, "NacarColunga",
			"Wis.1.1", -1);
		CHECK(nav_backend->resolveKey("NacarColunga", out.key, info));
		CHECK(info.osisBook == "EsthGr");
		CHECK(info.reference.chapter == info.chapterCount);
		CHECK(info.reference.verse == info.verseCount);
	}

	/* an arrow press: first navigable slot, whatever the module holds */
	expectBoundary("NacarColunga", "Jdt.16.25", 1, navigableIn("NacarColunga"),
		"resolved");
	expectBoundary("NacarColunga", "Wis.1.1", -1, navigableIn("NacarColunga"),
		"resolved");
}

/* A: own body, B: fallback body, C: a real slot with neither (not
 * rendered), D: a reference the versification does not have. */
struct SlotCategories {
	long own = 0;
	long fallback = 0;
	long empty = 0;
	std::string firstEmpty;
};

SlotCategories classifySlots(const char *module)
{
	SlotCategories c;
	sword::VerseKey *vk = nativeKey(module, "Gen.1.1");
	vk->setPosition(sword::TOP);
	for (; !vk->popError(); ++(*vk)) {
		const BibleReference slot = nativeSlot(*vk);
		const BibleVerseContent content =
			resolveVerseContent(*nav_backend, module, slot);
		if (classifyVerseContent(content, slot) != ContentAvailability::Available) {
			if (!c.empty)
				c.firstEmpty = vk->getOSISRef();
			++c.empty;
		} else if (content.isFallback) {
			++c.fallback;
		} else {
			++c.own;
		}
	}
	delete vk;
	return c;
}

void testSlotCategories()
{
	for (const char *module : { "KJV", "TorresAmat", "NacarColunga" }) {
		if (!installed(module))
			continue;
		const SlotCategories c = classifySlots(module);
		std::printf("categories module=%s own=%ld fallback=%ld empty=%ld "
			"first_empty=%s\n", module, c.own, c.fallback, c.empty,
			c.firstEmpty.empty() ? "-" : c.firstEmpty.c_str());
		CHECK(c.own > 0);
		if (c.empty == 0)
			continue;

		/* C is skipped, in both directions, and landing on either side
		 * matches SWORD's own walk. */
		const VerseSlotNavigable navigable = navigableIn(module);
		sword::VerseKey *vk = nativeKey(module, c.firstEmpty.c_str());
		BibleReference empty_slot = nativeSlot(*vk);
		CHECK(!verseSlotNavigable(*nav_backend, module, empty_slot));
		delete vk;
		BibleReference before, after;
		const bool have_before = expectedSlot(module, c.firstEmpty.c_str(), -1,
			navigable, before);
		const bool have_after = expectedSlot(module, c.firstEmpty.c_str(), 1,
			navigable, after);
		if (have_before && have_after) {
			sword::VerseKey *b = nativeKey(module, c.firstEmpty.c_str());
			while (!sameSlot(nativeSlot(*b), before))
				--(*b);
			const VerseStep step = stepVerse(*nav_backend, module,
				b->getOSISRef(), 1, navigable);
			CHECK(step.status == VerseStepStatus::Valid);
			CHECK(sameSlot(step.reference, after));
			const VerseStep back = stepVerse(*nav_backend, module,
				step.key, -1, navigable);
			CHECK(sameSlot(back.reference, before));
			std::printf("empty-slot module=%s skipped=%s from=%s to=%s\n",
				module, c.firstEmpty.c_str(), b->getOSISRef(), step.key.c_str());
			delete b;
		}
	}

	/* D: not in the versification. SWORD normalizes such text onto a real
	 * slot instead of failing, so it is not a "slot without content";
	 * navigation itself only ever walks real slots. */
	if (installed("KJV")) {
		BibleKeyInfo info;
		const bool ok = nav_backend->resolveKey("KJV", "Genesis 51:1", info);
		std::printf("not-in-versification KJV 'Genesis 51:1' -> resolved=%d key='%s'\n",
			ok ? 1 : 0, info.key.c_str());
	}
}

/* next(previous(x)) == x and previous(next(x)) == x over every slot of a
 * versification (every slot navigable), and over a sample with the
 * resolver deciding. Slots are addressed by OSIS reference and every key
 * produced must name its own slot. */
struct PropertyCount {
	long slots = 0;
	long transitions = 0;
	long invalid = 0;
	long roundTripFailures = 0;
	long edges = 0;
};

PropertyCount walkProperty(const char *module, bool resolver, int stride)
{
	PropertyCount count;
	sword::VerseKey *vk = nativeKey(module, "Gen.1.1");
	vk->setPosition(sword::TOP);
	const VerseSlotNavigable navigable =
		resolver ? navigableIn(module) : VerseSlotNavigable();
	long index = 0;
	for (; !vk->popError(); ++(*vk), ++index) {
		if (index % stride)
			continue;
		const BibleReference slot = nativeSlot(*vk);
		if (resolver && !navigable(slot))
			continue;
		++count.slots;
		BibleKeyInfo info;
		if (!nav_backend->resolveKey(module, vk->getOSISRef(), info) ||
		    !sameSlot(info.reference, slot)) {
			++count.roundTripFailures;
			continue;
		}
		BibleKeyInfo again;
		if (!nav_backend->resolveKey(module, info.key, again) ||
		    !sameSlot(again.reference, slot)) {
			std::fprintf(stderr, "FAIL %s key '%s' does not name %s\n",
				module, info.key.c_str(), vk->getOSISRef());
			++count.roundTripFailures;
		}

		for (int direction : { 1, -1 }) {
			const VerseStep step =
				stepVerse(*nav_backend, module, info.key, direction, navigable);
			if (step.status == VerseStepStatus::Valid) {
				++count.transitions;
				const VerseStep back = stepVerse(*nav_backend, module,
					step.key, -direction, navigable);
				if (back.status != VerseStepStatus::Valid ||
				    !sameSlot(back.reference, slot)) {
					std::fprintf(stderr, "FAIL %s %s(%s) = '%s' (status %d)\n",
						module, direction > 0 ? "previous(next" : "next(previous",
						vk->getOSISRef(), back.key.c_str(),
						static_cast<int>(back.status));
					++count.roundTripFailures;
				}
			} else if (step.status == VerseStepStatus::Invalid) {
				std::fprintf(stderr, "FAIL %s %s from %s: Invalid\n", module,
					direction > 0 ? "DOWN" : "UP", vk->getOSISRef());
				++count.invalid;
			} else {
				CHECK(step.status == (direction > 0
					? VerseStepStatus::EndOfBible
					: VerseStepStatus::BeginningOfBible));
				++count.edges;
			}
		}
	}
	delete vk;
	return count;
}

void testProperty()
{
	const struct {
		const char *module;
		const char *v11n;
	} cases[] = {
		{ "KJV", "KJV" },
		{ "TorresAmat", "Vulg" },
		{ "NacarColunga", "NRSVA" },
	};
	for (const auto &c : cases) {
		if (!installed(c.module))
			continue;
		CHECK(nav_backend->versification(c.module) == c.v11n);
		for (bool resolver : { false, true }) {
			const PropertyCount n = walkProperty(c.module, resolver,
				resolver ? 7 : 1);
			std::printf("property v11n=%s module=%s mode=%s slots=%ld "
				"transitions=%ld invalid=%ld roundtrip_failures=%ld edges=%ld\n",
				c.v11n, c.module, resolver ? "resolved/7" : "native",
				n.slots, n.transitions, n.invalid, n.roundTripFailures,
				n.edges);
			CHECK(n.invalid == 0);
			CHECK(n.roundTripFailures == 0);
			if (!resolver) {
				CHECK(n.slots > 30000);
				CHECK(n.edges == 2);
				CHECK(n.transitions == 2 * n.slots - 2);
			} else {
				CHECK(n.slots > 1000);
			}
		}
	}
}

}

int main()
{
	setvbuf(stdout, nullptr, _IOLBF, 0);
	SwordBackend sword_backend;
	nav_backend = &sword_backend;

	testKjvCrossings();
	testVulgTitleSlotsAndFallback();
	testNrsvaCrossings();
	testIntroSlotsSkipped();
	testNrsvaGreekEstherBoundaries();
	testSlotCategories();
	testProperty();

	std::printf("verse_navigation_sword_failures=%d skipped=%d\n", failures,
		skipped);
	return failures ? 1 : 0;
}
