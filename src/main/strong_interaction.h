#ifndef XIPHOS_STRONG_INTERACTION_H
#define XIPHOS_STRONG_INTERACTION_H

#include <cstddef>
#include <string>
#include <vector>

#include "backend/bible_resources.h"

enum class StrongWordAction {
	None,
	OpenDetail,
	ChooseStrong
};

struct StrongWordResolution {
	StrongWordAction action = StrongWordAction::None;
	StrongWordContext context;
};

StrongWordResolution resolveStrongInteraction(BibleBackend &backend,
	const std::string &module, const BibleReference &reference,
	std::size_t byteOffset);

/* Render only the verse text. Strong IDs intentionally never enter the HTML:
 * each actionable span carries the authoritative UTF-8 byte offset emitted by
 * the backend. */
std::string renderStrongVerseText(const BibleVerseContent &content,
	const std::string &module, const std::string &key, bool strongEnabled);

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
		StrongWordContext context, std::size_t pageSize = 50);

	bool selectStrong(const StrongId &strong);
	bool loadMore();
	const StrongWordContext &word() const { return context_; }
	const StrongDetailState &state() const { return state_; }

private:
	BibleBackend &backend_;
	const BibleApplicationResources &resources_;
	std::string module_;
	StrongWordContext context_;
	std::size_t pageSize_;
	StrongDetailState state_;
};

#endif /* XIPHOS_STRONG_INTERACTION_H */
