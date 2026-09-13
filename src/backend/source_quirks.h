/*
 * Encoding quirks of specific external sources. These are not Bible
 * semantics and must not run on every verse of every module.
 *
 * CrossWire SpaRVG stores Hebrew psalm superscriptions as a leading
 * «title» in the verse body, with no OSIS <title> and no Preverse
 * heading. The transform lives in quoted_heading.cc; this file only
 * says which modules may use it.
 */
#ifndef XIPHOS_SOURCE_QUIRKS_H
#define XIPHOS_SOURCE_QUIRKS_H

#include <string>

#include "backend/bible_types.h"

enum SourceQuirk {
	SOURCE_QUIRK_NONE = 0,
	SOURCE_QUIRK_LEADING_QUOTED_SUPERSCRIPTION = 1 << 0
};

unsigned sourceQuirksForModule(const std::string &module_id);
void applySourceQuirks(const std::string &module_id, BibleVerseContent &content);

#endif
