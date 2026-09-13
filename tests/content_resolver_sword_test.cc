#include <glib.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include "backend/content_resolver.h"
#include "backend/sword/sword_backend.h"
#include "main/intro_lookup.h"
#include "main/settings.h"

#include <swmodule.h>
#include <versekey.h>

#include <versificationmgr.h>

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
key_at_chapter_verse(sword::SWModule *mod, int chapter, int verse)
{
	sword::VerseKey *vk =
		dynamic_cast<sword::VerseKey *>(mod->getKey());
	return vk && vk->getChapter() == chapter && vk->getVerse() == verse;
}

static std::string
verse_identity_html(int chapter, int k, const char *key_text)
{
	char *num = main_format_number(k);
	gchar *html = g_strdup_printf(
		"<p class=\"verse\"><a name=\"%d\"></a>"
		"<span class=\"vtools\" data-key=\"%s\"> </span>"
		"<a href=\"sword:///%s\"><font>%s</font></a>",
		chapter * 1000 + k,
		key_text ? key_text : "",
		key_text ? key_text : "",
		num);
	std::string out = html ? html : "";
	g_free(html);
	g_free(num);
	return out;
}

static BibleReference
resolved(SwordBackend &backend, const char *module, const char *key)
{
	BibleKeyInfo info;
	g_assert_true(backend.resolveKey(module, key, info));
	return info.reference;
}

int
main()
{
	resetContentResolverCache();
	SwordBackend backend;
	if (!backend.hasModule("SpaRV1909")) {
		std::puts("content_resolver_sword_skipped=no-SpaRV1909");
		return 0;
	}

	{
		sword::VersificationMgr *vmgr =
			sword::VersificationMgr::getSystemVersificationMgr();
		const sword::VersificationMgr::System *vulg =
			vmgr->getVersificationSystem("Vulg");
		const sword::VersificationMgr::System *nrsva =
			vmgr->getVersificationSystem("NRSVA");
		const sword::VersificationMgr::System *kjv =
			vmgr->getVersificationSystem("KJV");
		g_assert_nonnull(vulg);
		g_assert_nonnull(nrsva);
		g_assert_nonnull(kjv);
		const char *book = "Ps";
		int ch = 10, vs = 1, ve = 1;
		vulg->translateVerse(kjv, &book, &ch, &vs, &ve);
		g_assert_cmpstr(book, ==, "Ps");
		g_assert_cmpint(ch, ==, 11);
		g_assert_cmpint(vs, ==, 1);
		book = "Ps";
		ch = 1;
		vs = 1;
		ve = 1;
		nrsva->translateVerse(kjv, &book, &ch, &vs, &ve);
		g_assert_cmpint(ch, ==, 1);
		g_assert_cmpint(vs, ==, 1);
		std::puts("content_resolver_sword_v11n_tables=ok");
	}

	if (backend.hasModule("NacarColunga")) {
		BibleVerseContent ps1 = resolveVerseContent(
			backend, "NacarColunga",
			resolved(backend, "NacarColunga", "Psalms 1:1"));
		g_assert_false(ps1.isFallback);
		g_assert_cmpstr(ps1.requestedModuleId.c_str(), ==,
				"NacarColunga");
		g_assert_cmpstr(ps1.sourceModuleId.c_str(), ==,
				"NacarColunga");
		g_assert_true(ps1.renderedText.find("Bienaventurado") !=
			      std::string::npos);

		BibleVerseContent gen1 = resolveVerseContent(
			backend, "NacarColunga",
			resolved(backend, "NacarColunga", "Genesis 1:1"));
		g_assert_false(gen1.isFallback);
		g_assert_cmpstr(gen1.sourceModuleId.c_str(), ==,
				"NacarColunga");
		g_assert_true(gen1.renderedText.find("principio") !=
			      std::string::npos);

		BibleVerseContent ps2 = resolveVerseContent(
			backend, "NacarColunga",
			resolved(backend, "NacarColunga", "Psalms 2:1"));
		g_assert_false(ps2.isFallback);
		g_assert_cmpstr(ps2.sourceModuleId.c_str(), ==,
				"NacarColunga");
		g_assert_true(ps2.renderedText.find("amotinan") !=
			      std::string::npos);
		g_assert_true(ps2.renderedText.find("naciones") !=
			      std::string::npos);
		g_assert_true(ps2.renderedText.find("vanos") !=
			      std::string::npos);
		g_assert_true(ps2.renderedText.find("denominaciones") ==
			      std::string::npos);
		g_assert_true(ps2.renderedText.find("vanidad") ==
			      std::string::npos);
		g_assert_true(ps2.renderedText.find("ALMOS") ==
			      std::string::npos);
		g_assert_true(ps2.plainText.find("ALMOS") ==
			      std::string::npos);

		{
			g_assert_true(intro_lookup_stole_verse_body(0, 1));
			g_assert_false(intro_lookup_stole_verse_body(0, 0));
			g_assert_false(intro_lookup_stole_verse_body(1, 1));

			sword::SWModule *mod =
				backend.get_SWModule("NacarColunga");
			g_assert_nonnull(mod);
			mod->setSkipConsecutiveLinks(true);
			sword::SWKey *const orig_key = mod->getKey();
			const std::string orig_text = orig_key
				? std::string(mod->getKeyText())
				: std::string();

			BibleReference v0 = ps2.reference;
			v0.verse = 0;
			BibleVerseContent intro0 =
				backend.getVerseContent("NacarColunga", v0,
							true);
			g_assert_true(mod->getKey() == orig_key);
			g_assert_true(intro0.renderedText.find("amotinan") ==
				      std::string::npos);
			g_assert_true(intro0.plainText.find("amotinan") ==
				      std::string::npos);
			for (size_t h = 0; h < intro0.headings.size(); h++) {
				g_assert_true(
					intro0.headings[h].text.find(
						"amotinan") ==
					std::string::npos);
			}

			BibleReference book0 = v0;
			book0.chapter = 0;
			BibleVerseContent book_intro =
				backend.getVerseContent("NacarColunga",
							book0, true);
			g_assert_true(
				book_intro.renderedText.find("amotinan") ==
				std::string::npos);
			g_assert_true(
				book_intro.renderedText.find("himmos") !=
					std::string::npos ||
				book_intro.renderedText.find("Salmos") !=
					std::string::npos ||
				book_intro.renderedText.find("salmos") !=
					std::string::npos ||
				book_intro.renderedText.find("introduction") !=
					std::string::npos ||
				!book_intro.renderedText.empty());

			if (!orig_text.empty())
				mod->setKeyText(orig_text.c_str());
			mod->setSkipConsecutiveLinks(false);
		}

		{
			sword::SWModule *mod =
				backend.get_SWModule("NacarColunga");
			g_assert_nonnull(mod);
			BibleKeyInfo ps21;
			g_assert_true(backend.resolveKey("NacarColunga",
							  "Psalms 2:1", ps21));
			mod->setKeyText(ps21.key.c_str());
			sword::SWKey *const orig_key = mod->getKey();
			const std::string stay = mod->getKeyText();
			g_assert_true(key_at_chapter_verse(mod, 2, 1));

			BibleKeyInfo ps212;
			g_assert_true(backend.resolveKey("NacarColunga",
							  "Psalms 2:12",
							  ps212));
			BibleVerseContent v12 = backend.getVerseContent(
				"NacarColunga", ps212.reference, true);
			g_assert_true(mod->getKey() == orig_key);
			g_assert_cmpstr(mod->getKeyText(), ==, stay.c_str());
			g_assert_true(key_at_chapter_verse(mod, 2, 1));
			g_assert_true(v12.renderedText.find("amotinan") ==
				      std::string::npos);

			resetContentResolverCache();
			BibleReference chapter_ref = ps21.reference;
			chapter_ref.verse = 1;
			std::vector<BibleVerseContent> cold =
				resolveChapterContent(backend, "NacarColunga",
						      chapter_ref, 12);
			g_assert_true(mod->getKey() == orig_key);
			g_assert_true(key_at_chapter_verse(mod, 2, 1));
			g_assert_cmpuint(cold.size(), ==, 12);
			g_assert_true(cold[0].renderedText.find("amotinan") !=
				      std::string::npos);
			g_assert_true(cold[0].renderedText.find("naciones") !=
				      std::string::npos);
			g_assert_false(cold[0].isFallback);

			std::string cold_html = verse_identity_html(
				2, 1, mod->getKeyText());
			g_assert_true(cold_html.find("name=\"2001\"") !=
				      std::string::npos);
			g_assert_true(cold_html.find("2:1") !=
				      std::string::npos);
			g_assert_true(cold_html.find("href=\"sword:///") !=
				      std::string::npos);
			g_assert_true(cold_html.find("<font>1</font>") !=
				      std::string::npos);
			g_assert_true(cold_html.find("name=\"2012\"") ==
				      std::string::npos);

			int n2012 = 0;
			std::string chapter_html;
			for (int k = 1; k <= 12; k++) {
				gchar *tag = g_strdup_printf("name=\"%d\"",
							     2000 + k);
				if (k == 12)
					n2012++;
				chapter_html += tag;
				g_free(tag);
			}
			g_assert_cmpint(n2012, ==, 1);
			g_assert_true(chapter_html.find("name=\"2001\"") !=
				      std::string::npos);
			g_assert_true(chapter_html.find("name=\"2012\"") !=
				      std::string::npos);

			std::vector<BibleVerseContent> warm =
				resolveChapterContent(backend, "NacarColunga",
						      chapter_ref, 12);
			g_assert_true(key_at_chapter_verse(mod, 2, 1));
			std::string warm_html = verse_identity_html(
				2, 1, mod->getKeyText());
			g_assert_cmpstr(warm_html.c_str(), ==,
					cold_html.c_str());
			g_assert_true(warm[0].renderedText.find("amotinan") !=
				      std::string::npos);
			std::puts("content_resolver_sword_ps2_key_restore=ok");
		}

		BibleVerseContent ps22n = resolveVerseContent(
			backend, "NacarColunga",
			resolved(backend, "NacarColunga", "Psalms 2:2"));
		g_assert_false(ps22n.isFallback);
		g_assert_cmpstr(ps22n.sourceModuleId.c_str(), ==,
				"NacarColunga");
		g_assert_true(ps22n.renderedText.find("reyes") !=
			      std::string::npos);

		BibleKeyInfo tobit;
		if (backend.resolveKey("NacarColunga", "Tobit 1:1", tobit)) {
			BibleVerseContent deutero = resolveVerseContent(
				backend, "NacarColunga", tobit.reference);
			g_assert_false(
				deutero.isFallback &&
				deutero.sourceModuleId == "SpaRV1909");
		}
		std::puts("content_resolver_sword_nacar=ok");
	}

	if (backend.hasModule("TorresAmat")) {
		BibleVerseContent ps22 = resolveVerseContent(
			backend, "TorresAmat",
			resolved(backend, "TorresAmat", "Psalms 2:2"));
		g_assert_false(ps22.isFallback);
		g_assert_cmpstr(ps22.sourceModuleId.c_str(), ==, "TorresAmat");
		g_assert_true(ps22.renderedText.find("WMestas") !=
			      std::string::npos);

		BibleReference vulg_ps10 =
			resolved(backend, "TorresAmat", "Psalms 10:1");
		BibleVerseContent original = backend.getVerseContent(
			"TorresAmat", vulg_ps10);
		if (classifyVerseContent(original, vulg_ps10) ==
		    ContentAvailability::Missing) {
			BibleVerseContent fb = resolveVerseContent(
				backend, "TorresAmat", vulg_ps10);
			g_assert_true(fb.isFallback);
			BibleVerseContent rv11 = backend.getVerseContent(
				"SpaRV1909",
				resolved(backend, "SpaRV1909", "Psalms 11:1"));
			BibleVerseContent rv10 = backend.getVerseContent(
				"SpaRV1909",
				resolved(backend, "SpaRV1909", "Psalms 10:1"));
			g_assert_cmpstr(fb.renderedText.c_str(), ==,
					rv11.renderedText.c_str());
			g_assert_true(fb.renderedText != rv10.renderedText);
			std::puts("content_resolver_sword_vulg_ps10_mapped=ok");
		} else
			std::puts("content_resolver_sword_vulg_ps10_present");

		BibleReference vulg_ps33 =
			resolved(backend, "TorresAmat", "Psalms 3:3");
		BibleVerseContent orig33 = backend.getVerseContent(
			"TorresAmat", vulg_ps33);
		if (classifyVerseContent(orig33, vulg_ps33) ==
		    ContentAvailability::Missing) {
			BibleVerseContent fb = resolveVerseContent(
				backend, "TorresAmat", vulg_ps33);
			g_assert_true(fb.isFallback);
			BibleVerseContent rv32 = backend.getVerseContent(
				"SpaRV1909",
				resolved(backend, "SpaRV1909", "Psalms 3:2"));
			BibleVerseContent rv33 = backend.getVerseContent(
				"SpaRV1909",
				resolved(backend, "SpaRV1909", "Psalms 3:3"));
			g_assert_cmpstr(fb.renderedText.c_str(), ==,
					rv32.renderedText.c_str());
			g_assert_true(fb.renderedText != rv33.renderedText);
			std::puts("content_resolver_sword_vulg_ps33_mapped=ok");
		} else
			std::puts("content_resolver_sword_vulg_ps33_present");
		std::puts("content_resolver_sword_torres=ok");
	}

	if (backend.hasModule("SpaRVG")) {
		BibleVerseContent rvg = resolveVerseContent(
			backend, "SpaRVG",
			resolved(backend, "SpaRVG", "Psalms 1:1"));
		g_assert_false(rvg.isFallback);
		g_assert_cmpstr(rvg.sourceModuleId.c_str(), ==, "SpaRVG");
		g_assert_cmpuint(rvg.headings.size(), ==, 1);
		g_assert_true(rvg.renderedText.find("Bienaventurado") !=
			      std::string::npos);
		std::puts("content_resolver_sword_sparvg=ok");
	}

	{
		BibleReference ps2 = resolved(backend, "SpaRV1909",
					      "Psalms 2:1");
		BibleVerseContent v1 =
			resolveVerseContent(backend, "SpaRV1909", ps2);
		BibleVerseContent v2 = resolveVerseContent(
			backend, "SpaRV1909",
			resolved(backend, "SpaRV1909", "Psalms 2:2"));
		g_assert_false(v1.isFallback);
		g_assert_true(v1.renderedText.find("amotinan") !=
			      std::string::npos);
		g_assert_false(v2.isFallback);
		g_assert_true(v2.renderedText.find("reyes") !=
			      std::string::npos);
		g_assert_true(v1.renderedText.find("reyes") ==
			      std::string::npos);
		std::puts("content_resolver_sword_rvr_ps2=ok");
	}

	BibleKeyInfo missing;
	const char *candidates[] = { "Psalms 151:1", "3 John 15",
				     "Malachi 4:7", nullptr };
	for (int i = 0; candidates[i]; i++) {
		if (!backend.resolveKey("SpaRV1909", candidates[i], missing))
			continue;
		if (missing.reference.chapter <= 0 ||
		    missing.reference.verse <= 0)
			continue;
		BibleVerseContent original = backend.getVerseContent(
			"SpaRV1909", missing.reference);
		if (classifyVerseContent(original, missing.reference) !=
		    ContentAvailability::Missing)
			continue;
		resetContentResolverCache();
		BibleVerseContent self = resolveVerseContent(
			backend, "SpaRV1909", missing.reference);
		g_assert_false(self.isFallback);
		g_assert_cmpstr(self.sourceModuleId.c_str(), ==, "SpaRV1909");
		break;
	}
	std::puts("content_resolver_sword_no_loop=ok");
	return 0;
}
