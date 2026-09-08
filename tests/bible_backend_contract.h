#ifndef XIPHOS_TESTS_BIBLE_BACKEND_CONTRACT_H
#define XIPHOS_TESTS_BIBLE_BACKEND_CONTRACT_H

#include <cstddef>
#include <string>

class BibleBackend;

struct BibleBackendContractFixture {
	std::string module = "FakeBible";
	std::string description = "In-memory test Bible";
	std::string language = "en";
	std::string reference = "John 3:16";
	std::string nextReference = "John 3:17";
	std::string verseText = "For God so loved the world.";
	std::string searchText = "world";
	std::size_t chapterSize = 2;
	std::size_t searchResults = 2;
	int testament = 2;
	int bookId = 4;
	bool enrichedWords = true;
	std::string dictionaryModule = "FakeDictionary";
};

/* Shared assertions for every implementation of the common backend API. */
void runBibleBackendContractTests(
	BibleBackend &backend,
	const BibleBackendContractFixture &fixture = BibleBackendContractFixture());

#endif
