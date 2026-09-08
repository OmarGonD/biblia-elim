#ifndef XIPHOS_TESTS_STRONG_BACKEND_CONTRACT_H
#define XIPHOS_TESTS_STRONG_BACKEND_CONTRACT_H

#include <string>

class BibleBackend;

struct StrongBackendContractFixture {
	std::string module = "FakeBible";
	std::string hebrewReference = "Genesis 1:1";
	std::string hebrewWord = "God";
	std::string greekReference = "John 3:16";
	std::string greekWord = "loved";
	std::string multipleReference = "John 3:17";
	std::string multipleWord = "Son";
	std::string noStrongReference = "John 3:16";
	std::string noStrongWord = "world";
};

void runStrongBackendContractTests(BibleBackend &backend,
	const StrongBackendContractFixture &fixture = StrongBackendContractFixture());

#endif /* XIPHOS_TESTS_STRONG_BACKEND_CONTRACT_H */
