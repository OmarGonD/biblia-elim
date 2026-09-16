/*
 * Biblia Elim - carrying a reference from one module to another.
 *
 * A reference is (module, key native to that module's versification).
 * Crossing to another module converts the key through the backend; the
 * text of the key is never reread in the other module's versification.
 */
#ifndef BIBLIA_ELIM_REFERENCE_TRANSITION_H
#define BIBLIA_ELIM_REFERENCE_TRANSITION_H

#include <string>

#include "backend/bible_backend.h"

enum class BibleModuleTransition {
	/* Same module: the key is already native, used as given. */
	SameModule,
	Converted,
	/* The verse has no counterpart in the target versification. */
	Unmapped,
	Invalid,
};

struct BibleModuleTransitionPlan {
	BibleModuleTransition status = BibleModuleTransition::Invalid;
	/* Native to the target module; empty unless SameModule/Converted. */
	std::string key;
};

BibleModuleTransitionPlan planBibleModuleTransition(
	BibleBackend &backend, const std::string &source_module,
	const std::string &source_key, const std::string &target_module);

/* For a source module that no longer exists: only the versification it
 * declared, remembered while it was on screen, identifies its numbering. */
BibleModuleTransitionPlan planBibleVersificationTransition(
	BibleBackend &backend, const std::string &source_versification,
	const std::string &source_key, const std::string &target_module);

/*
 * The key a module-less "sword:///KEY" URI has to carry.
 *
 * Such a URI names no module, so it navigates whatever Bible is
 * currently selected: KEY must be native to that Bible. An emitter whose
 * key comes from elsewhere -- the verse list built for the module that
 * published a cross reference, the Strong's concordance, a Bible dialog
 * syncing the main window -- has to carry the reference across first,
 * because letting the selected Bible reparse the text reinterprets one
 * versification's numbering as another's.
 *
 * SameModule (identity) when source and main module agree, and also when
 * the source is not verse-keyed: a dictionary or genbook declares no
 * versification, so there is nothing to convert from and those routes
 * keep behaving exactly as they did.
 */
BibleModuleTransitionPlan planUriKeyForMainBible(
	BibleBackend &backend, const std::string &source_module,
	const std::string &source_key, const std::string &main_module);

/* The commentary module that carries an edition's own notes, or NULL. */
const char *authorCommentaryForBible(const char *bible);

#endif /* BIBLIA_ELIM_REFERENCE_TRANSITION_H */
