/*
 * Split a leading «title» prefix out of verse body text. This is a
 * source-encoding transform for modules registered in source_quirks.cc,
 * not a general rule of Bible punctuation. Dialogue that begins with
 * «…» is verse text and must not go through this function.
 */
#ifndef XIPHOS_QUOTED_HEADING_H
#define XIPHOS_QUOTED_HEADING_H

#include "backend/bible_types.h"

void promoteLeadingQuotedHeading(BibleVerseContent &content);

#endif
