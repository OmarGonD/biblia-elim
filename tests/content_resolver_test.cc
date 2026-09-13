#include <glib.h>

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "backend/content_resolver.h"
#include "backend/source_quirks.h"

namespace {

struct StoredVerse {
	std::string module;
	BibleReference reference;
	std::string bookName;
	std::string osisBook;
	BibleVerseContent content;
};

class HarnessBackend final : public BibleBackend
{
public:
	std::map<std::string, int> verseCalls;
	std::map<std::string, int> chapterCalls;
	std::map<std::string, int> resolveCalls;

	void addModule(const std::string &id, BibleModuleType type,
		       const std::string &v11n = "KJV")
	{
		types_[id] = type;
		v11n_[id] = v11n.empty() ? "KJV" : v11n;
	}

	void addBook(const std::string &id, int testament,
		     const std::string &name, const std::string &osis)
	{
		books_[id][testament].push_back(name);
		osis_[id][name] = osis;
	}

	void addVerse(const std::string &id, const BibleReference &reference,
		      const std::string &bookName, const std::string &osisBook,
		      const std::string &text,
		      const std::vector<BibleHeading> &headings = {})
	{
		StoredVerse verse;
		verse.module = id;
		verse.reference = reference;
		verse.bookName = bookName;
		verse.osisBook = osisBook;
		verse.content.reference = reference;
		verse.content.plainText = text;
		verse.content.renderedText = text;
		verse.content.headings = headings;
		verse.content.valid = true;
		applySourceQuirks(id, verse.content);
		verses_.push_back(verse);
	}

	std::vector<BibleModuleInfo> listModules() const override
	{
		std::vector<BibleModuleInfo> modules;
		for (const auto &entry : types_)
			modules.push_back({ entry.first, entry.first, "es",
					    entry.second });
		return modules;
	}

	bool hasModule(const std::string &id) const override
	{
		return types_.count(id) != 0;
	}

	BibleModuleType moduleType(const std::string &id) const override
	{
		auto it = types_.find(id);
		return it == types_.end() ? BibleModuleType::Unknown : it->second;
	}

	BibleModuleCapabilities moduleCapabilities(
		const std::string &) const override
	{
		return {};
	}

	std::string moduleDescription(const std::string &id) const override
	{
		return id;
	}

	std::string moduleLanguage(const std::string &) const override
	{
		return "es";
	}

	std::string versification(const std::string &id) const override
	{
		auto it = v11n_.find(id);
		return it == v11n_.end() ? std::string("KJV") : it->second;
	}

	std::string osisRefFromKey(const std::string &id,
				   const std::string &key) override
	{
		BibleKeyInfo info;
		if (!resolveKey(id, key, info))
			return std::string();
		if (info.osisBook.empty())
			return std::string();
		return info.osisBook + "." +
		       std::to_string(info.reference.chapter) + "." +
		       std::to_string(info.reference.verse);
	}

	bool resolveKey(const std::string &id, const std::string &key,
			BibleKeyInfo &result) override
	{
		resolveCalls[id]++;
		std::string book;
		int chapter = 0;
		int verse = 0;
		if (!parseKey(key, book, chapter, verse))
			return false;
		int testament = 0;
		int book_number = 0;
		if (!findBook(id, book, testament, book_number))
			return false;
		result = BibleKeyInfo();
		result.reference.testament = testament;
		result.reference.book = book_number;
		result.reference.chapter = chapter;
		result.reference.verse = verse;
		result.key = key;
		result.bookName = books_.at(id).at(testament)
					  [(std::size_t)book_number - 1];
		result.osisBook = osisFor(id, result.bookName);
		result.chapterCount = 150;
		result.verseCount = 50;
		return true;
	}

	std::vector<BibleVerse> getChapter(const std::string &id,
					   const BibleReference &reference,
					   bool rendered) override
	{
		chapterCalls[id]++;
		std::vector<BibleVerse> chapter;
		for (const StoredVerse &verse : verses_) {
			if (verse.module != id ||
			    verse.reference.testament != reference.testament ||
			    verse.reference.book != reference.book ||
			    verse.reference.chapter != reference.chapter)
				continue;
			BibleVerse item;
			item.reference = verse.reference;
			item.key = verse.bookName + " " +
				   std::to_string(verse.reference.chapter) +
				   ":" +
				   std::to_string(verse.reference.verse);
			item.text = rendered ? verse.content.renderedText
					     : verse.content.plainText;
			chapter.push_back(item);
		}
		return chapter;
	}

	BibleVerseContent getVerseContent(const std::string &id,
					  const BibleReference &reference,
					  bool) override
	{
		verseCalls[id]++;
		for (const StoredVerse &verse : verses_) {
			if (verse.module == id &&
			    sameRef(verse.reference, reference))
				return verse.content;
		}
		return BibleVerseContent();
	}

	DictionaryEntry lookupDictionary(const std::string &,
					 const std::string &) override
	{
		return {};
	}

	std::string navigate(const std::string &, const std::string &,
			     int) override
	{
		return {};
	}

	std::string setChapter(const std::string &, const std::string &,
			       int) override
	{
		return {};
	}

	std::string setVerse(const std::string &, const std::string &,
			     int) override
	{
		return {};
	}

	std::string setBook(const std::string &, const std::string &, int,
			    int) override
	{
		return {};
	}

	std::vector<std::string> bookNames(const std::string &id,
					   int testament) const override
	{
		auto module = books_.find(id);
		if (module == books_.end())
			return {};
		auto names = module->second.find(testament);
		if (names == module->second.end())
			return {};
		return names->second;
	}

	std::vector<BibleSearchResult> search(const std::string &,
					      const BibleSearchQuery &) override
	{
		return {};
	}

private:
	std::map<std::string, BibleModuleType> types_;
	std::map<std::string, std::string> v11n_;
	std::map<std::string, std::map<int, std::vector<std::string>>> books_;
	std::map<std::string, std::map<std::string, std::string>> osis_;
	std::vector<StoredVerse> verses_;

	static bool sameRef(const BibleReference &a, const BibleReference &b)
	{
		return a.testament == b.testament && a.book == b.book &&
		       a.chapter == b.chapter && a.verse == b.verse;
	}

	static bool parseKey(const std::string &key, std::string &book,
			     int &chapter, int &verse)
	{
		const std::size_t space = key.find_last_of(' ');
		if (space == std::string::npos)
			return false;
		const std::size_t colon = key.find(':', space + 1);
		if (colon == std::string::npos)
			return false;
		book = key.substr(0, space);
		chapter = std::atoi(key.c_str() + space + 1);
		verse = std::atoi(key.c_str() + colon + 1);
		return !book.empty() && chapter > 0 && verse > 0;
	}

	bool findBook(const std::string &id, const std::string &name,
		      int &testament, int &book_number) const
	{
		auto module = books_.find(id);
		if (module == books_.end())
			return false;
		for (const auto &entry : module->second) {
			for (std::size_t i = 0; i < entry.second.size(); i++) {
				if (entry.second[i] != name &&
				    osisFor(id, entry.second[i]) != name)
					continue;
				testament = entry.first;
				book_number = (int)i + 1;
				return true;
			}
		}
		return false;
	}

	std::string osisFor(const std::string &id, const std::string &name) const
	{
		auto module = osis_.find(id);
		if (module == osis_.end())
			return {};
		auto book = module->second.find(name);
		if (book == module->second.end())
			return {};
		return book->second;
	}
};

BibleReference ref(int testament, int book, int chapter, int verse)
{
	BibleReference reference;
	reference.testament = testament;
	reference.book = book;
	reference.chapter = chapter;
	reference.verse = verse;
	return reference;
}

HarnessBackend
makeCorpus()
{
	HarnessBackend backend;
	backend.addModule("NacarColunga", BibleModuleType::Bible, "NRSVA");
	backend.addModule("SpaRV1909", BibleModuleType::Bible, "KJV");
	backend.addModule("TorresAmat", BibleModuleType::Bible, "Vulg");
	backend.addModule("SpaRVG", BibleModuleType::Bible);
	backend.addModule("Tisch", BibleModuleType::Bible);
	backend.addModule("SomeCommentary", BibleModuleType::Commentary);

	backend.addBook("NacarColunga", 1, "Genesis", "Gen");
	backend.addBook("NacarColunga", 1, "Psalms", "Ps");
	backend.addBook("NacarColunga", 1, "Tobit", "Tob");
	backend.addBook("SpaRV1909", 1, "Genesis", "Gen");
	backend.addBook("SpaRV1909", 1, "Psalms", "Ps");
	backend.addBook("TorresAmat", 1, "Genesis", "Gen");
	backend.addBook("TorresAmat", 1, "Psalms", "Ps");
	backend.addBook("SpaRVG", 1, "Genesis", "Gen");
	backend.addBook("SpaRVG", 1, "Psalms", "Ps");
	backend.addBook("Tisch", 1, "Genesis", "Gen");
	backend.addBook("Tisch", 2, "John", "John");
	backend.addBook("SomeCommentary", 1, "Genesis", "Gen");

	backend.addVerse("NacarColunga", ref(1, 1, 1, 1), "Genesis", "Gen",
			 "En el principio creó Dios los cielos y la tierra.");
	backend.addVerse("NacarColunga", ref(1, 2, 2, 3), "Psalms", "Ps",
			 "Rompamos sus coyundas.");
	backend.addVerse("SpaRV1909", ref(1, 1, 1, 1), "Genesis", "Gen",
			 "EN el principio crió Dios los cielos y la tierra.");
	backend.addVerse("SpaRV1909", ref(1, 2, 1, 1), "Psalms", "Ps",
			 "Bienaventurado el varón que no anduvo en consejo "
			 "de malos.");
	backend.addVerse("SpaRV1909", ref(1, 2, 1, 2), "Psalms", "Ps",
			 "Sino que en la ley de Jehová está su delicia.");
	backend.addVerse("TorresAmat", ref(1, 2, 2, 2), "Psalms", "Ps",
			 "Hanse coligado los reyes de la tierra; y se han "
			 "confederado los príncipes contra el Señor, y "
			 "contra su Christo, o // WMestas.");
	backend.addVerse("SpaRV1909", ref(1, 2, 2, 1), "Psalms", "Ps",
			 "¿Por qué se amotinan las gentes, Y los pueblos "
			 "piensan cosas vanas?");
	backend.addVerse("SpaRV1909", ref(1, 2, 2, 2), "Psalms", "Ps",
			 "Estarán los reyes de la tierra, Y príncipes "
			 "consultarán unidos Contra Jehová y contra su "
			 "ungido.");
	backend.addVerse("SpaRV1909", ref(1, 2, 4, 1), "Psalms", "Ps",
			 "Respóndeme cuando clamo, oh Dios de mi justicia.");
	backend.addVerse("SpaRV1909", ref(1, 2, 10, 1), "Psalms", "Ps",
			 "TEXTO-KJV-PS-10");
	backend.addVerse("SpaRV1909", ref(1, 2, 11, 1), "Psalms", "Ps",
			 "TEXTO-KJV-PS-11");
	backend.addVerse("SpaRVG", ref(1, 2, 1, 1), "Psalms", "Ps",
			 "«El piadoso será prosperado, el impío perecerá» "
			 "Bienaventurado el varón que no anduvo en consejo "
			 "de malos.");
	backend.addVerse("NacarColunga", ref(1, 3, 1, 2), "Tobit", "Tob",
			 "Tobías de la tribu de Neftalí.");
	backend.addVerse("Tisch", ref(2, 1, 1, 1), "John", "John",
			 "εν αρχη ην ο λογος");
	backend.addVerse("SomeCommentary", ref(1, 1, 1, 1), "Genesis", "Gen",
			 "");

	BibleHeading heading;
	heading.text = "<h3>Salmo de David</h3>";
	backend.addVerse("NacarColunga", ref(1, 2, 3, 1), "Psalms", "Ps", "",
			 { heading });
	backend.addVerse("SpaRV1909", ref(1, 2, 3, 1), "Psalms", "Ps",
			 "Señor, cuán multiplicados son mis enemigos.");
	return backend;
}

int callCount(const std::map<std::string, int> &calls, const std::string &id)
{
	auto it = calls.find(id);
	return it == calls.end() ? 0 : it->second;
}

void
test_availability_semantics()
{
	BibleReference intro = ref(1, 2, 0, 0);
	BibleVerseContent empty;
	g_assert_true(classifyVerseContent(empty, intro) ==
		      ContentAvailability::NotApplicable);

	BibleReference verse = ref(1, 2, 1, 1);
	g_assert_true(classifyVerseContent(empty, verse) ==
		      ContentAvailability::Missing);

	BibleVerseContent ocr;
	ocr.renderedText = "o // WMestas.";
	g_assert_true(hasUsableVerseBody(ocr.renderedText));
	g_assert_true(classifyVerseContent(ocr, verse) ==
		      ContentAvailability::Available);

	BibleVerseContent heading_only;
	BibleHeading heading;
	heading.text = "<h3>Título</h3>";
	heading_only.headings.push_back(heading);
	g_assert_true(classifyVerseContent(heading_only, verse) ==
		      ContentAvailability::Missing);

	BibleVerseContent markup;
	markup.renderedText = "<span class=\"line\"></span>&nbsp;";
	g_assert_false(hasUsableVerseBody(markup.renderedText));

	BibleVerseContent html_heading;
	html_heading.renderedText = "<h3>Salmo de David</h3>";
	g_assert_true(classifyVerseContent(html_heading, verse) ==
		      ContentAvailability::Missing);

	BibleVerseContent heading_and_body;
	heading_and_body.headings.push_back(heading);
	heading_and_body.renderedText = "Bienaventurado el varón.";
	heading_and_body.plainText = "Bienaventurado el varón.";
	g_assert_true(classifyVerseContent(heading_and_body, verse) ==
		      ContentAvailability::Available);
}

void
test_nacar_psalm_1_falls_back()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 1, 1));
	g_assert_true(content.isFallback);
	g_assert_cmpstr(content.requestedModuleId.c_str(), ==, "NacarColunga");
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "SpaRV1909");
	g_assert_true(content.renderedText.find("Bienaventurado el varón") !=
		      std::string::npos);
	g_assert_true(missingContentFallbackNotice(content).find(
			      "Reina-Valera 1909") != std::string::npos);
}

void
test_nacar_genesis_does_not_consult_fallback()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "NacarColunga");
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
	g_assert_cmpint(callCount(backend.chapterCalls, "SpaRV1909"), ==, 0);
	g_assert_cmpint(callCount(backend.resolveCalls, "SpaRV1909"), ==, 0);
	g_assert_true(content.renderedText.find("En el principio") !=
		      std::string::npos);
}

void
test_torres_ocr_is_not_replaced()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "TorresAmat", ref(1, 2, 2, 2));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "TorresAmat");
	g_assert_true(content.renderedText.find("WMestas") != std::string::npos);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_sparvg_quirks_do_not_fallback()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "SpaRVG", ref(1, 2, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "SpaRVG");
	g_assert_cmpuint(content.headings.size(), ==, 1);
	g_assert_true(content.headings[0].text.find(
			      "El piadoso será prosperado") != std::string::npos);
	g_assert_true(content.renderedText.find("Bienaventurado el varón") == 0);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_rvr1909_missing_does_not_loop()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "SpaRV1909", ref(1, 2, 1, 99));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.requestedModuleId.c_str(), ==, "SpaRV1909");
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "SpaRV1909");
	g_assert_false(hasUsableVerseBody(content.renderedText));
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 1);
}

void
test_deuterocanonical_is_unavailable()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 3, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "NacarColunga");
	g_assert_false(hasUsableVerseBody(content.renderedText));
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_heading_only_keeps_original_heading()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 3, 1));
	g_assert_true(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "SpaRV1909");
	g_assert_cmpstr(content.headingSourceModuleId.c_str(), ==,
			"NacarColunga");
	g_assert_cmpuint(content.headings.size(), ==, 1);
	g_assert_true(content.headings[0].text.find("Salmo de David") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("multiplicados") !=
		      std::string::npos);
}

void
test_out_of_corpus_book_is_not_filled()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content =
		resolveVerseContent(backend, "Tisch", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_commentary_is_not_filled()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "SomeCommentary", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_chapter_fallback_preserves_per_verse_provenance()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	std::vector<BibleVerseContent> chapter = resolveChapterContent(
		backend, "NacarColunga", ref(1, 2, 1, 1), 2);
	g_assert_cmpuint(chapter.size(), ==, 2);
	g_assert_true(chapter[0].isFallback);
	g_assert_cmpstr(chapter[0].sourceModuleId.c_str(), ==, "SpaRV1909");
	g_assert_true(chapter[1].isFallback);
	g_assert_cmpstr(chapter[1].requestedModuleId.c_str(), ==,
			"NacarColunga");
}

void
test_default_policy_id_is_sparv1909()
{
	g_assert_cmpstr(defaultFallbackPolicy().fallbackModuleId.c_str(), ==,
			"SpaRV1909");
}

void
test_nrsva_keeps_kjv_psalm_numbers()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 10, 1));
	g_assert_true(content.isFallback);
	g_assert_true(content.renderedText.find("TEXTO-KJV-PS-10") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("TEXTO-KJV-PS-11") ==
		      std::string::npos);
}

void
test_vulg_psalm_uses_sword_mapping()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	g_assert_true(classifyFallbackVersification("Vulg", "KJV") ==
		      FallbackMappingStatus::Mapped);
	BibleVerseContent content = resolveVerseContent(
		backend, "TorresAmat", ref(1, 2, 10, 1));
	g_assert_true(content.isFallback);
	g_assert_true(content.renderedText.find("TEXTO-KJV-PS-11") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("TEXTO-KJV-PS-10") ==
		      std::string::npos);
}

void
test_kjv_to_kjv_is_verified_identity()
{
	g_assert_true(classifyFallbackVersification("KJV", "KJV") ==
		      FallbackMappingStatus::VerifiedIdentity);
}

void
test_nrsva_psalm_1_is_verified_identity()
{
	resetContentResolverCache();
	g_assert_true(classifyFallbackVersification("NRSVA", "KJV") ==
		      FallbackMappingStatus::VerifiedIdentity);
	HarnessBackend backend = makeCorpus();
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 1, 1));
	g_assert_true(content.isFallback);
	g_assert_true(content.renderedText.find("Bienaventurado el varón") !=
		      std::string::npos);
}

void
add_hebrew_module(HarnessBackend &backend, const char *id, const char *v11n)
{
	backend.addModule(id, BibleModuleType::Bible, v11n);
	backend.addBook(id, 1, "Genesis", "Gen");
	backend.addVerse(id, ref(1, 1, 1, 2), "Genesis", "Gen",
			 "Y la tierra estaba desordenada.");
}

void
test_mt_to_kjv_is_unsupported()
{
	resetContentResolverCache();
	g_assert_true(classifyFallbackVersification("MT", "KJV") ==
		      FallbackMappingStatus::Unsupported);
	HarnessBackend backend = makeCorpus();
	add_hebrew_module(backend, "OSHB", "MT");
	BibleVerseContent content =
		resolveVerseContent(backend, "OSHB", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
	g_assert_false(hasUsableVerseBody(content.renderedText));
}

static std::string
introDumpHtml()
{
	return "<span class=\"ocr-facsímil\">"
	       "Las denominaciones de los Salmos de David. El principal autor. "
	       "Las inscripciones atribuyen setenta y cuatro. Asaf, levita, "
	       "doce. Los coreítas, cincuenta y nueve. El período cubre desde "
	       "la monarquía hasta después del destierro. La versión de los "
	       "LXX conoce ya estas rúbricas. Son indicaciones del autor, de "
	       "la melodía y de los instrumentos. Mizmor, sir, masquil. "
	       "Neginot, alamot, Jedutún. El valor histórico que tienen "
	       "varias inscripciones. Así, por ejemplo, las circunstancias "
	       "históricas de algunos salmos se recogen en esas rúbricas. "
	       "No es el cuerpo del salmo segundo. Es aparato introductorio "
	       "del libro, no el texto del versículo primero. Queda fuera "
	       "del canto. Se conserva como encabezado del salmo."
	       "</span>";
}

void
test_intro_dump_is_heading_and_falls_back()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	backend.addVerse("NacarColunga", ref(1, 2, 2, 1), "Psalms", "Ps",
			 introDumpHtml().c_str());
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 2, 1));
	g_assert_true(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "SpaRV1909");
	g_assert_true(content.renderedText.find("amotinan las gentes") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("denominaciones") ==
		      std::string::npos);
	g_assert_false(content.headings.empty());
	g_assert_true(content.headings[0].text.find("denominaciones") !=
		      std::string::npos);
}

void
test_heading_plus_body_stays_original()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	BibleHeading heading;
	heading.text = "<h3>Salmo de David</h3>";
	backend.addVerse("NacarColunga", ref(1, 2, 4, 1), "Psalms", "Ps",
			 "Respóndeme cuando clamo.", { heading });
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 4, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpstr(content.sourceModuleId.c_str(), ==, "NacarColunga");
	g_assert_true(content.renderedText.find("Respóndeme") !=
		      std::string::npos);
}

void
test_html_heading_only_falls_back()
{
	resetContentResolverCache();
	HarnessBackend backend = makeCorpus();
	backend.addVerse("NacarColunga", ref(1, 2, 4, 2), "Psalms", "Ps",
			 "<h3>Salmo de David</h3>");
	backend.addVerse("SpaRV1909", ref(1, 2, 4, 2), "Psalms", "Ps",
			 "Tú ensanchaste mi corazón.");
	BibleVerseContent content = resolveVerseContent(
		backend, "NacarColunga", ref(1, 2, 4, 2));
	g_assert_true(content.isFallback);
	g_assert_true(content.renderedText.find("ensanchaste") !=
		      std::string::npos);
	g_assert_false(content.headings.empty());
}

void
test_leningrad_heading_only_does_not_fallback()
{
	resetContentResolverCache();
	g_assert_true(classifyFallbackVersification("Leningrad", "KJV") ==
		      FallbackMappingStatus::Unsupported);
	HarnessBackend backend = makeCorpus();
	add_hebrew_module(backend, "WLC2", "Leningrad");
	BibleHeading heading;
	heading.text = "<h3>Introducción</h3>";
	backend.addVerse("WLC2", ref(1, 1, 1, 1), "Genesis", "Gen", "",
			 { heading });
	BibleVerseContent content =
		resolveVerseContent(backend, "WLC2", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
}

void
test_leningrad_to_kjv_is_unsupported()
{
	resetContentResolverCache();
	g_assert_true(classifyFallbackVersification("Leningrad", "KJV") ==
		      FallbackMappingStatus::Unsupported);
	HarnessBackend backend = makeCorpus();
	add_hebrew_module(backend, "WLC", "Leningrad");
	BibleVerseContent content =
		resolveVerseContent(backend, "WLC", ref(1, 1, 1, 1));
	g_assert_false(content.isFallback);
	g_assert_cmpint(callCount(backend.verseCalls, "SpaRV1909"), ==, 0);
	g_assert_false(hasUsableVerseBody(content.renderedText));
}

} // namespace

int
main(void)
{
	test_availability_semantics();
	test_nacar_psalm_1_falls_back();
	test_nacar_genesis_does_not_consult_fallback();
	test_torres_ocr_is_not_replaced();
	test_sparvg_quirks_do_not_fallback();
	test_rvr1909_missing_does_not_loop();
	test_deuterocanonical_is_unavailable();
	test_heading_only_keeps_original_heading();
	test_out_of_corpus_book_is_not_filled();
	test_commentary_is_not_filled();
	test_chapter_fallback_preserves_per_verse_provenance();
	test_default_policy_id_is_sparv1909();
	test_nrsva_keeps_kjv_psalm_numbers();
	test_vulg_psalm_uses_sword_mapping();
	test_kjv_to_kjv_is_verified_identity();
	test_nrsva_psalm_1_is_verified_identity();
	test_mt_to_kjv_is_unsupported();
	test_leningrad_to_kjv_is_unsupported();
	test_intro_dump_is_heading_and_falls_back();
	test_heading_plus_body_stays_original();
	test_html_heading_only_falls_back();
	test_leningrad_heading_only_does_not_fallback();
	return 0;
}
