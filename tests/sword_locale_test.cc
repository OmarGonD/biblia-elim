/*
 * SWORD-LOCALE-101: the reader-locale manager serves exactly what SWORD's
 * full LocaleMgr serves for that locale -- same locale chosen, same book
 * names, same abbreviations -- without reading the other 57.
 *
 *   ./tests/sword_locale_test
 */
#include "backend/sword/sword_locale.h"

#include <glib.h>

#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include <localemgr.h>
#include <swlocale.h>
#include <versificationmgr.h>

using namespace sword;

namespace {

int failures = 0;

void check(bool condition, const std::string &message)
{
	if (condition) return;
	++failures;
	std::fprintf(stderr, "FAIL: %s\n", message.c_str());
}

/* The previous rule (set_sword_locale): the whole name, then 5, then 2
 * characters, over the available names in order. */
std::string chooseAsBefore(LocaleMgr &full, const char *lang)
{
	StringList names = full.getAvailableLocales();
	const int lengths[3] = {100, 5, 2};
	for (int length : lengths)
		for (const SWBuf &name : names)
			if (!strncmp(lang, name.c_str(), length))
				return name.c_str();
	return std::string();
}

std::set<std::string> abbreviations(SWLocale *locale)
{
	std::set<std::string> out;
	int count = 0;
	const struct abbrev *list = locale->getBookAbbrevs(&count);
	for (int i = 0; i < count; ++i)
		out.insert(std::string(list[i].ab) + "=" + list[i].osis);
	return out;
}

/* Every book name of the systems a Spanish reader's Bibles use. */
std::set<std::string> bookNames()
{
	std::set<std::string> out;
	for (const char *system : {"KJV", "Vulg", "NRSVA", "LXX"}) {
		const VersificationMgr::System *v11n =
			VersificationMgr::getSystemVersificationMgr()
				->getVersificationSystem(system);
		for (int i = 0; v11n && i < v11n->getBookCount(); ++i)
			out.insert(v11n->getBook(i)->getLongName());
	}
	return out;
}

} // namespace

int main()
{
	LocaleMgr full; /* SWORD's: every locale it finds */
	const std::set<std::string> books = bookNames();
	int compared = 0;
	for (const char *lang : {"es_PE.UTF-8", "es_ES.UTF-8", "es", "de_DE.UTF-8",
				 "pt_BR.UTF-8", "fr_FR.UTF-8", "en_US.UTF-8",
				 "zh_CN.UTF-8", "ru_RU.UTF-8", "C", "xx_YY"}) {
		const std::string expected = chooseAsBefore(full, lang);
		gchar *chosen = swordInstallReaderLocale(lang);
		check((chosen ? std::string(chosen) : std::string()) == expected,
		      std::string(lang) + ": chose " + (chosen ? chosen : "(none)") +
			      ", SWORD chose " + (expected.empty() ? "(none)" : expected));
		if (chosen && !expected.empty()) {
			SWLocale *mine = LocaleMgr::getSystemLocaleMgr()->getLocale(chosen);
			SWLocale *theirs = full.getLocale(expected.c_str());
			check(mine && theirs, std::string(lang) + ": locale object");
			if (mine && theirs) {
				check(!strcmp(mine->getEncoding() ? mine->getEncoding() : "",
					      theirs->getEncoding() ? theirs->getEncoding() : ""),
				      std::string(lang) + ": encoding");
				for (const std::string &book : books) {
					const std::string a = mine->translate(book.c_str());
					const std::string b = theirs->translate(book.c_str());
					check(a == b, std::string(lang) + ": " + book + " -> " +
						      a + " / " + b);
				}
				check(abbreviations(mine) == abbreviations(theirs),
				      std::string(lang) + ": abbreviations");
				check(!strcmp(LocaleMgr::getSystemLocaleMgr()->getDefaultLocaleName(),
					      chosen),
				      std::string(lang) + ": default locale");
				++compared;
			}
		}
		g_free(chosen);
	}
	std::printf("sword_locale_compared=%d books=%zu failures=%d\n", compared,
		    books.size(), failures);
	return failures ? 1 : 0;
}
