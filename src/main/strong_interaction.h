#ifndef XIPHOS_STRONG_INTERACTION_H
#define XIPHOS_STRONG_INTERACTION_H

#include <cstddef>
#include <string>
#include <vector>

#include "backend/bible_resources.h"

enum class AnnotatedWordAction {
	None,
	OpenDetail,
	ChooseStrong
};

struct AnnotatedWordResolution {
	AnnotatedWordAction action = AnnotatedWordAction::None;
	BibleAnnotatedWord context;
};

AnnotatedWordResolution resolveAnnotatedWordInteraction(BibleBackend &backend,
	const std::string &module, const BibleReference &reference,
	std::size_t byteOffset);

/* Render only the verse text. Annotations intentionally never enter the HTML:
 * each actionable span carries the authoritative UTF-8 byte offset emitted by
 * the backend. */
std::string renderAnnotatedVerseText(const BibleVerseContent &content,
	const std::string &module, const std::string &key, bool annotationsEnabled);

struct StrongDetailState {
	StrongId selected;
	LexiconEntry lexicon;
	std::vector<StrongOccurrence> occurrences;
	bool hasMore = false;
	bool selectedValid = false;
	long long lexiconMicroseconds = 0;
	long long pageMicroseconds = 0;
};

class StrongDetailSession
{
public:
	StrongDetailSession(BibleBackend &backend,
		const BibleApplicationResources &resources, std::string module,
		BibleAnnotatedWord context, std::size_t pageSize = 50);

	bool selectStrong(const StrongId &strong);
	bool loadMore();
	const BibleAnnotatedWord &word() const { return context_; }
	const StrongDetailState &state() const { return state_; }

private:
	BibleBackend &backend_;
	const BibleApplicationResources &resources_;
	std::string module_;
	BibleAnnotatedWord context_;
	std::size_t pageSize_;
	StrongDetailState state_;
};

#endif /* XIPHOS_STRONG_INTERACTION_H */
