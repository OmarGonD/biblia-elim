/* Optional neutral resources used alongside, never inside, a Bible backend. */
#ifndef XIPHOS_BIBLE_RESOURCES_H
#define XIPHOS_BIBLE_RESOURCES_H

#include "backend/bible_backend.h"
#include "backend/bible_lexicon.h"

struct BibleResourceCapabilities {
	bool strongLexicon = false;
};

struct BibleApplicationResources {
	BibleBackend *bible = nullptr;
	BibleLexicon *strongLexicon = nullptr;

	BibleResourceCapabilities capabilities() const
	{
		BibleResourceCapabilities result;
		result.strongLexicon = strongLexicon != nullptr;
		return result;
	}

	LexiconEntry lookupStrong(const StrongId &id) const
	{
		if (strongLexicon) return strongLexicon->lookupStrong(id);
		LexiconEntry missing;
		missing.id = id;
		return missing;
	}
};

#endif /* XIPHOS_BIBLE_RESOURCES_H */
