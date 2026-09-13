#ifndef XIPHOS_CONTENT_AVAILABILITY_H
#define XIPHOS_CONTENT_AVAILABILITY_H

#include "backend/bible_types.h"

/* Usable body: letters or digits remain after tags and entities. A verse
 * that exists but is only markup, whitespace, or a SWORD intro slot is
 * not usable body. OCR garbage with letters (e.g. "WMestas") counts as
 * present. A heading with no body is not usable body. */
bool hasUsableVerseBody(const std::string &text);

/* Pull introductory material out of the verse body so availability is
 * decided on the biblical body alone. SWORD preverse/HTML titles become
 * headings. A page-length prose dump stored in the verse slot (typical
 * of a reconstruction putting book apparatus on verse 1) is also a
 * heading, not a body. */
void splitIntroductoryMaterial(BibleVerseContent &content);

/* NotApplicable: chapter <= 0 or verse <= 0 (SWORD intros / placeholders).
 * Available: an applicable slot with usable body. Headings alone do not
 * make a verse Available.
 * Missing: an applicable slot with no usable body. Empty, invalid, and
 * heading-only verses are Missing. Missing does not mean "looks wrong". */
ContentAvailability classifyVerseContent(const BibleVerseContent &content,
					 const BibleReference &reference);

#endif
