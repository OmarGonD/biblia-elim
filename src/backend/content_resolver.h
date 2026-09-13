#ifndef XIPHOS_CONTENT_RESOLVER_H
#define XIPHOS_CONTENT_RESOLVER_H

#include "backend/bible_backend.h"
#include "backend/content_availability.h"

#include <vector>

/* Default missing-content policy. The SpaRV1909 identifier lives here
 * and nowhere else as a global default. */
FallbackPolicy defaultFallbackPolicy();

/* Cross-versification permission for missing-content fallback.
 * Mapped: SWORD has a conversion table; use translateVerse.
 * VerifiedIdentity: numeric book/chapter/verse is known-safe.
 * Unsupported: no table and no audit; do not assume identity. */
enum class FallbackMappingStatus {
	Mapped,
	VerifiedIdentity,
	Unsupported
};

FallbackMappingStatus classifyFallbackVersification(
	const std::string &source_v11n, const std::string &fallback_v11n);

/* Drop cached book-presence answers. Tests that reuse module ids with
 * different corpora must call this between cases. */
void resetContentResolverCache();

/* Discrete UI copy when the body was supplied by the fallback module. */
std::string missingContentFallbackNotice(
	const BibleVerseContent &content,
	const FallbackPolicy &policy = defaultFallbackPolicy());

/* Raw source → source quirks (already applied by the backend) →
 * availability → optional fallback → resolved verse. The renderer must
 * not look up the fallback module itself. */
BibleVerseContent resolveVerseContent(
	BibleBackend &backend, const std::string &module_id,
	const BibleReference &reference, bool include_plain_text = false,
	const FallbackPolicy &policy = defaultFallbackPolicy());

/* Chapter-level resolution: one emptiness probe on the selected module,
 * one book-presence check, then per-verse provenance. verse_count is
 * the selected module's verse max for that chapter (1-based size). */
std::vector<BibleVerseContent> resolveChapterContent(
	BibleBackend &backend, const std::string &module_id,
	const BibleReference &chapter_ref, int verse_count,
	bool include_plain_text = false,
	const FallbackPolicy &policy = defaultFallbackPolicy());

#endif
