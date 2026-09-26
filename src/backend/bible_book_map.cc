#include "backend/bible_book_map.h"

#include <algorithm>
#include <cctype>
#include <iterator>

namespace {
const BibleBookDefinition books[] = {
 {1,"GEN","Gen","Genesis","Gen",1,1},{2,"EXO","Exod","Exodus","Exod",1,2},
 {3,"LEV","Lev","Leviticus","Lev",1,3},{4,"NUM","Num","Numbers","Num",1,4},
 {5,"DEU","Deut","Deuteronomy","Deut",1,5},{6,"JOS","Josh","Joshua","Josh",1,6},
 {7,"JDG","Judg","Judges","Judg",1,7},{8,"RUT","Ruth","Ruth","Ruth",1,8},
 {9,"1SA","1Sam","1 Samuel","1Sam",1,9},{10,"2SA","2Sam","2 Samuel","2Sam",1,10},
 {11,"1KI","1Kgs","1 Kings","1Kgs",1,11},{12,"2KI","2Kgs","2 Kings","2Kgs",1,12},
 {13,"1CH","1Chr","1 Chronicles","1Chr",1,13},{14,"2CH","2Chr","2 Chronicles","2Chr",1,14},
 {15,"EZR","Ezra","Ezra","Ezra",1,15},{16,"NEH","Neh","Nehemiah","Neh",1,16},
 {17,"EST","Esth","Esther","Esth",1,17},{18,"JOB","Job","Job","Job",1,18},
 {19,"PSA","Ps","Psalms","Ps",1,19},{20,"PRO","Prov","Proverbs","Prov",1,20},
 {21,"ECC","Eccl","Ecclesiastes","Eccl",1,21},{22,"SNG","Song","Song of Solomon","Song",1,22},
 {23,"ISA","Isa","Isaiah","Isa",1,23},{24,"JER","Jer","Jeremiah","Jer",1,24},
 {25,"LAM","Lam","Lamentations","Lam",1,25},{26,"EZK","Ezek","Ezekiel","Ezek",1,26},
 {27,"DAN","Dan","Daniel","Dan",1,27},{28,"HOS","Hos","Hosea","Hos",1,28},
 {29,"JOL","Joel","Joel","Joel",1,29},{30,"AMO","Amos","Amos","Amos",1,30},
 {31,"OBA","Obad","Obadiah","Obad",1,31},{32,"JON","Jonah","Jonah","Jonah",1,32},
 {33,"MIC","Mic","Micah","Mic",1,33},{34,"NAM","Nah","Nahum","Nah",1,34},
 {35,"HAB","Hab","Habakkuk","Hab",1,35},{36,"ZEP","Zeph","Zephaniah","Zeph",1,36},
 {37,"HAG","Hag","Haggai","Hag",1,37},{38,"ZEC","Zech","Zechariah","Zech",1,38},
 {39,"MAL","Mal","Malachi","Mal",1,39},{40,"MAT","Matt","Matthew","Matt",2,40},
 {41,"MRK","Mark","Mark","Mark",2,41},{42,"LUK","Luke","Luke","Luke",2,42},
 {43,"JHN","John","John","John",2,43},{44,"ACT","Acts","Acts","Acts",2,44},
 {45,"ROM","Rom","Romans","Rom",2,45},{46,"1CO","1Cor","1 Corinthians","1Cor",2,46},
 {47,"2CO","2Cor","2 Corinthians","2Cor",2,47},{48,"GAL","Gal","Galatians","Gal",2,48},
 {49,"EPH","Eph","Ephesians","Eph",2,49},{50,"PHP","Phil","Philippians","Phil",2,50},
 {51,"COL","Col","Colossians","Col",2,51},{52,"1TH","1Thess","1 Thessalonians","1Thess",2,52},
 {53,"2TH","2Thess","2 Thessalonians","2Thess",2,53},{54,"1TI","1Tim","1 Timothy","1Tim",2,54},
 {55,"2TI","2Tim","2 Timothy","2Tim",2,55},{56,"TIT","Titus","Titus","Titus",2,56},
 {57,"PHM","Phlm","Philemon","Phlm",2,57},{58,"HEB","Heb","Hebrews","Heb",2,58},
 {59,"JAS","Jas","James","Jas",2,59},{60,"1PE","1Pet","1 Peter","1Pet",2,60},
 {61,"2PE","2Pet","2 Peter","2Pet",2,61},{62,"1JN","1John","1 John","1John",2,62},
 {63,"2JN","2John","2 John","2John",2,63},{64,"3JN","3John","3 John","3John",2,64},
 {65,"JUD","Jude","Jude","Jude",2,65},{66,"REV","Rev","Revelation","Rev",2,66},
 /* Deuterocanonical books (Catholic Bibles: Vulgate, NRSVA). Their ids
  * follow the 66 so an id still indexes this table; where they sit in a
  * module is its own business (the importer keeps the source order). The
  * Greek parts of Esther and Daniel are chapters of those books. */
 {67,"TOB","Tob","Tobit","Tob",1,67},{68,"JDT","Jdt","Judith","Jdt",1,68},
 {69,"WIS","Wis","Wisdom","Wis",1,69},{70,"SIR","Sir","Sirach","Sir",1,70},
 {71,"BAR","Bar","Baruch","Bar",1,71},{72,"1MA","1Macc","1 Maccabees","1Macc",1,72},
 {73,"2MA","2Macc","2 Maccabees","2Macc",1,73}
};

/* Spanish names, by book id: the same spelling SWORD's Spanish locale
 * gives, so a reference saved while reading a SWORD module («Lucas
 * 23:33») still resolves in the same Bible imported to SQLite. SWORD has
 * no Spanish names for the deuterocanonical books; the usual ones are
 * used. */
const char *const spanishNames[] = {
 "Génesis","Éxodo","Levítico","Números","Deuteronomio","Josué","Jueces","Rut",
 "1 Samuel","2 Samuel","1 Reyes","2 Reyes","1 Crónicas","2 Crónicas","Esdras",
 "Nehemías","Ester","Job","Salmos","Proverbios","Eclesiastés","Cantares",
 "Isaías","Jeremías","Lamentaciones","Ezequiel","Daniel","Oseas","Joel","Amós",
 "Abdías","Jonás","Miqueas","Nahum","Habacuc","Sofonías","Hageo","Zacarías",
 "Malaquías","Mateo","Marcos","Lucas","Juan","Hechos","Romanos","1 Corintios",
 "2 Corintios","Gálatas","Efesios","Filipenses","Colosenses","1 Tesalonicenses",
 "2 Tesalonicenses","1 Timoteo","2 Timoteo","Tito","Filemón","Hebreos",
 "Santiago","1 Pedro","2 Pedro","1 Juan","2 Juan","3 Juan","Judas","Apocalipsis",
 "Tobías","Judit","Sabiduría","Eclesiástico","Baruc","1 Macabeos","2 Macabeos"
};
static_assert(sizeof(spanishNames) / sizeof(spanishNames[0]) ==
	      sizeof(books) / sizeof(books[0]), "one Spanish name per book");
}

const BibleBookDefinition *findBibleBookByUsfm(const std::string &code)
{
	std::string upper = code;
	for (char &c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	for (const auto &book : books) if (upper == book.usfm) return &book;
	return nullptr;
}

const std::vector<BibleBookDefinition> &canonicalBibleBooks()
{
	static const std::vector<BibleBookDefinition> result(std::begin(books), std::end(books));
	return result;
}

const char *bibleBookName(const BibleBookDefinition &book,
			  const std::string &language)
{
	const std::string primary = language.substr(0, language.find_first_of("-_"));
	if ((primary == "es" || primary == "spa") && book.bookId >= 1 &&
	    book.bookId <= static_cast<int>(sizeof(spanishNames) / sizeof(spanishNames[0])))
		return spanishNames[book.bookId - 1];
	return book.name;
}

/* Lowercase for comparing book names: ASCII, and the Latin-1 capitals
 * Spanish names use (Á É Í Ó Ú Ñ Ü, U+00C0-U+00DE), which are two UTF-8
 * bytes whose second byte moves by 0x20. Enough for these names, with no
 * dependency for the importers that build this file. */
static std::string foldBookName(const std::string &name)
{
	std::string out = name;
	for (std::size_t i = 0; i < out.size(); ++i) {
		const unsigned char c = static_cast<unsigned char>(out[i]);
		if (c < 0x80) {
			out[i] = static_cast<char>(std::tolower(c));
		} else if (c == 0xC3 && i + 1 < out.size()) {
			const unsigned char d = static_cast<unsigned char>(out[i + 1]);
			if (d >= 0x80 && d <= 0x9E && d != 0x97)
				out[i + 1] = static_cast<char>(d + 0x20);
			++i;
		}
	}
	return out;
}

const BibleBookDefinition *findBibleBookByAnyName(const std::string &name)
{
	const std::string wanted = foldBookName(name);
	for (const auto &book : books)
		if (wanted == foldBookName(book.osis) ||
		    wanted == foldBookName(book.name) ||
		    wanted == foldBookName(book.shortName) ||
		    wanted == foldBookName(spanishNames[book.bookId - 1]))
			return &book;
	return nullptr;
}

const char *versificationSystemName(const std::string &stored)
{
	static const struct { const char *stored, *system; } systems[] = {
		{"kjv", "KJV"}, {"custom", "KJV"}, {"kjva", "KJVA"},
		{"nrsv", "NRSV"}, {"nrsva", "NRSVA"}, {"vulg", "Vulg"},
		{"lxx", "LXX"}, {"mt", "MT"}, {"leningrad", "Leningrad"},
		{"synodal", "Synodal"}, {"synodalprot", "SynodalProt"},
		{"german", "German"}, {"luther", "Luther"},
		{"orthodox", "Orthodox"}, {"catholic", "Catholic"},
		{"catholic2", "Catholic2"}, {"segond", "Segond"},
		{"darbyfr", "DarbyFr"}, {"calvin", "Calvin"},
	};
	for (const auto &system : systems)
		if (stored == system.stored)
			return system.system;
	return nullptr;
}
