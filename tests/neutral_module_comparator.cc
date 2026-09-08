#include "neutral_module_comparator.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {

std::string referenceText(const BibleReference &reference)
{
	return std::to_string(reference.book) + "." +
		std::to_string(reference.chapter) + "." +
		std::to_string(reference.verse);
}

bool sameReference(const BibleReference &left, const BibleReference &right)
{
	return left.testament == right.testament && left.book == right.book &&
		left.chapter == right.chapter && left.verse == right.verse;
}

std::string referenceValue(const BibleReference &reference)
{
	return std::to_string(reference.testament) + ":" + referenceText(reference);
}

std::string boolValue(bool value) { return value ? "true" : "false"; }

void mismatch(NeutralComparisonResult &result, const std::string &reference,
	const std::string &field, const std::string &expected,
	const std::string &actual)
{
	++result.mismatches;
	std::cerr << "reference=" << reference << '\n'
		<< "field=" << field << '\n'
		<< "expected=" << expected << '\n'
		<< "actual=" << actual << '\n';
}

template <typename T>
std::string number(T value)
{
	return std::to_string(value);
}

std::vector<BibleReference> references(BibleBackend &backend,
	const std::string &module)
{
	std::vector<BibleReference> result;
	for (int book = 1; book <= 66; ++book) {
		const std::string firstKey = backend.setBook(module, "", 0, book);
		if (firstKey.empty()) continue;
		BibleKeyInfo first;
		if (!backend.resolveKey(module, firstKey, first)) continue;
		for (int chapter = 1; chapter <= first.chapterCount; ++chapter) {
			BibleReference key = first.reference;
			key.chapter = chapter;
			for (const BibleVerse &verse : backend.getChapter(module, key, false))
				result.push_back(verse.reference);
		}
	}
	return result;
}

void compareCapabilities(NeutralComparisonResult &result,
	const BibleModuleCapabilities &expected,
	const BibleModuleCapabilities &actual)
{
	struct Capability {
		const char *name;
		bool BibleModuleCapabilities::*member;
	};
	const Capability capabilities[] = {
		{ "capabilities.verses", &BibleModuleCapabilities::verses },
		{ "capabilities.search", &BibleModuleCapabilities::search },
		{ "capabilities.strong", &BibleModuleCapabilities::strongs },
		{ "capabilities.headings", &BibleModuleCapabilities::headings },
		{ "capabilities.footnotes", &BibleModuleCapabilities::footnotes },
		{ "capabilities.crossrefs", &BibleModuleCapabilities::crossrefs },
	};
	for (const Capability &capability : capabilities) {
		const bool expectedValue = expected.*(capability.member);
		const bool actualValue = actual.*(capability.member);
		if (expectedValue != actualValue)
			mismatch(result, "module", capability.name,
				boolValue(expectedValue), boolValue(actualValue));
	}
}

void compareVerse(NeutralComparisonResult &result, const BibleReference &reference,
	const BibleVerseContent &expected, const BibleVerseContent &actual)
{
	const std::string ref = referenceText(reference);
	if (expected.plainText != actual.plainText)
		mismatch(result, ref, "plainText", expected.plainText, actual.plainText);
	if (expected.paragraphBreak != actual.paragraphBreak)
		mismatch(result, ref, "paragraphBreak", boolValue(expected.paragraphBreak),
			boolValue(actual.paragraphBreak));
	if (expected.headings.size() != actual.headings.size())
		mismatch(result, ref, "headings.size", number(expected.headings.size()),
			number(actual.headings.size()));
	const std::size_t headingCount = std::min(expected.headings.size(), actual.headings.size());
	for (std::size_t index = 0; index < headingCount; ++index)
		if (expected.headings[index].text != actual.headings[index].text)
			mismatch(result, ref, "headings[" + number(index) + "].text",
				expected.headings[index].text, actual.headings[index].text);

	if (expected.spans.size() != actual.spans.size())
		mismatch(result, ref, "spans.size", number(expected.spans.size()),
			number(actual.spans.size()));
	const std::size_t spanCount = std::min(expected.spans.size(), actual.spans.size());
	for (std::size_t index = 0; index < spanCount; ++index) {
		const BibleTextSpan &left = expected.spans[index];
		const BibleTextSpan &right = actual.spans[index];
		if (left.start != right.start) mismatch(result, ref,
			"spans[" + number(index) + "].start", number(left.start), number(right.start));
		if (left.length != right.length) mismatch(result, ref,
			"spans[" + number(index) + "].length", number(left.length), number(right.length));
		if (left.style != right.style) mismatch(result, ref,
			"spans[" + number(index) + "].style", number(static_cast<int>(left.style)),
			number(static_cast<int>(right.style)));
	}

	if (expected.words.size() != actual.words.size())
		mismatch(result, ref, "words.size", number(expected.words.size()),
			number(actual.words.size()));
	const std::size_t wordCount = std::min(expected.words.size(), actual.words.size());
	for (std::size_t index = 0; index < wordCount; ++index) {
		const BibleWordInfo &left = expected.words[index];
		const BibleWordInfo &right = actual.words[index];
		const std::string field = "words[" + number(index) + "]";
		if (left.start != right.start) mismatch(result, ref, field + ".start",
			number(left.start), number(right.start));
		if (left.length != right.length) mismatch(result, ref, field + ".length",
			number(left.length), number(right.length));
		if (left.text != right.text) mismatch(result, ref, field + ".text",
			left.text, right.text);
		if (left.strongs.size() != right.strongs.size()) mismatch(result, ref,
			field + ".strongs.size", number(left.strongs.size()), number(right.strongs.size()));
		const std::size_t strongCount = std::min(left.strongs.size(), right.strongs.size());
		for (std::size_t strong = 0; strong < strongCount; ++strong) {
			const StrongId &leftStrong = left.strongs[strong];
			const StrongId &rightStrong = right.strongs[strong];
			const std::string leftValue = number(static_cast<int>(leftStrong.language)) + ":" + number(leftStrong.number);
			const std::string rightValue = number(static_cast<int>(rightStrong.language)) + ":" + number(rightStrong.number);
			if (leftValue != rightValue) mismatch(result, ref,
				field + ".strongs[" + number(strong) + "]", leftValue, rightValue);
		}
	}

	if (expected.footnotes.size() != actual.footnotes.size())
		mismatch(result, ref, "footnotes.size", number(expected.footnotes.size()),
			number(actual.footnotes.size()));
	const std::size_t footnoteCount = std::min(expected.footnotes.size(), actual.footnotes.size());
	for (std::size_t index = 0; index < footnoteCount; ++index) {
		const BibleFootnote &left = expected.footnotes[index];
		const BibleFootnote &right = actual.footnotes[index];
		const std::string field = "footnotes[" + number(index) + "]";
		if (left.label != right.label) mismatch(result, ref, field + ".caller", left.label, right.label);
		if (left.body != right.body) mismatch(result, ref, field + ".text", left.body, right.body);
		if (left.offset != right.offset) mismatch(result, ref, field + ".offset",
			number(left.offset), number(right.offset));
	}

	if (expected.crossReferences.size() != actual.crossReferences.size())
		mismatch(result, ref, "crossReferences.size", number(expected.crossReferences.size()),
			number(actual.crossReferences.size()));
	const std::size_t crossCount = std::min(expected.crossReferences.size(), actual.crossReferences.size());
	for (std::size_t index = 0; index < crossCount; ++index) {
		const BibleCrossReference &left = expected.crossReferences[index];
		const BibleCrossReference &right = actual.crossReferences[index];
		const std::string field = "crossReferences[" + number(index) + "]";
		if (left.displayText != right.displayText) mismatch(result, ref,
			field + ".displayText", left.displayText, right.displayText);
		if (left.offset != right.offset) mismatch(result, ref, field + ".offset",
			number(left.offset), number(right.offset));
		if (left.references.size() != right.references.size()) mismatch(result, ref,
			field + ".targets.size", number(left.references.size()), number(right.references.size()));
		const std::size_t targetCount = std::min(left.references.size(), right.references.size());
		for (std::size_t target = 0; target < targetCount; ++target)
			if (!sameReference(left.references[target], right.references[target]))
				mismatch(result, ref, field + ".targets[" + number(target) + "]",
					referenceValue(left.references[target]), referenceValue(right.references[target]));
	}
}

} // namespace

NeutralComparisonResult compareNeutralModules(
	BibleBackend &expectedBackend, const std::string &expectedModule,
	BibleBackend &actualBackend, const std::string &actualModule)
{
	NeutralComparisonResult result;
	if (expectedBackend.moduleType(expectedModule) != actualBackend.moduleType(actualModule))
		mismatch(result, "module", "type",
			number(static_cast<int>(expectedBackend.moduleType(expectedModule))),
			number(static_cast<int>(actualBackend.moduleType(actualModule))));
	if (expectedBackend.moduleLanguage(expectedModule) != actualBackend.moduleLanguage(actualModule))
		mismatch(result, "module", "language", expectedBackend.moduleLanguage(expectedModule),
			actualBackend.moduleLanguage(actualModule));
	if (expectedBackend.moduleDescription(expectedModule) != actualBackend.moduleDescription(actualModule))
		mismatch(result, "module", "description", expectedBackend.moduleDescription(expectedModule),
			actualBackend.moduleDescription(actualModule));
	compareCapabilities(result, expectedBackend.moduleCapabilities(expectedModule),
		actualBackend.moduleCapabilities(actualModule));

	for (int testament = 1; testament <= 2; ++testament) {
		const auto expectedBooks = expectedBackend.bookNames(expectedModule, testament);
		const auto actualBooks = actualBackend.bookNames(actualModule, testament);
		if (expectedBooks != actualBooks) {
			std::ostringstream expected, actual;
			for (const std::string &book : expectedBooks) expected << '[' << book << ']';
			for (const std::string &book : actualBooks) actual << '[' << book << ']';
			mismatch(result, "module", "books.testament" + number(testament),
				expected.str(), actual.str());
		}
	}

	const std::vector<BibleReference> expectedReferences = references(expectedBackend, expectedModule);
	const std::vector<BibleReference> actualReferences = references(actualBackend, actualModule);
	result.references = expectedReferences;
	if (expectedReferences.size() != actualReferences.size())
		mismatch(result, "module", "references.size", number(expectedReferences.size()),
			number(actualReferences.size()));
	const std::size_t count = std::min(expectedReferences.size(), actualReferences.size());
	for (std::size_t index = 0; index < count; ++index) {
		if (!sameReference(expectedReferences[index], actualReferences[index])) {
			mismatch(result, "module", "references[" + number(index) + "]",
				referenceValue(expectedReferences[index]), referenceValue(actualReferences[index]));
			continue;
		}
		compareVerse(result, expectedReferences[index],
			expectedBackend.getVerseContent(expectedModule, expectedReferences[index]),
			actualBackend.getVerseContent(actualModule, actualReferences[index]));
	}
	return result;
}

NeutralOffsetValidationResult validateNeutralOffsets(
	BibleBackend &backend, const std::string &module)
{
	NeutralOffsetValidationResult result;
	for (const BibleReference &reference : references(backend, module)) {
		const BibleVerseContent content = backend.getVerseContent(module, reference);
		++result.rowsChecked;
		for (const BibleWordInfo &word : content.words) {
			if (word.start > content.plainText.size() ||
			    word.length > content.plainText.size() - word.start) {
				++result.invalidOffsets;
				std::cerr << "reference=" << referenceText(reference)
					<< "\nfield=word.offset\nexpected=within plainText\nactual="
					<< word.start << '+' << word.length << '\n';
			} else if (content.plainText.substr(word.start, word.length) != word.text) {
				++result.substringMismatches;
				std::cerr << "reference=" << referenceText(reference)
					<< "\nfield=word.substring\nexpected=" << word.text
					<< "\nactual=" << content.plainText.substr(word.start, word.length) << '\n';
			}
		}
		for (const BibleTextSpan &span : content.spans)
			if (span.start > content.plainText.size() ||
			    span.length > content.plainText.size() - span.start)
				++result.invalidOffsets;
		for (const BibleFootnote &footnote : content.footnotes)
			if (footnote.offset > content.plainText.size()) ++result.invalidOffsets;
		for (const BibleCrossReference &crossReference : content.crossReferences)
			if (crossReference.offset > content.plainText.size()) ++result.invalidOffsets;
	}
	return result;
}
