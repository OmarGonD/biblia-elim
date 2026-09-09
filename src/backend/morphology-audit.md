# Neutral morphology audit (MORPH-101)

This audit covers morphology data already available in this repository on
2026-09-08. It did not download any corpus. Counts treat one `<w morph>` or
USFM `\w ...|x-morph="..."\w*` as an attribute-bearing token; a
morphology-bearing token has at least one usable parsed tag.

## Real dataset evidence

| Local data | Format | Attribute tokens | Morph-bearing tokens | Tags | Schemes | Unique codes | Multi-tag tokens | Max tags/token | Malformed |
|---|---|---:|---:|---:|---|---:|---:|---:|---:|
| Open Scriptures Hebrew Bible, Gen 1:1 extract | OSIS | 7 | 7 | 7 | unqualified | 7 | 0 | 1 | 0 |
| Torres Amat 1882, Ps 1:1-2 extract | OSIS | 0 | 0 | 0 | none | 0 | 0 | 0 | 0 |
| local generated Nácar-Colunga OSIS (6,682,045 bytes, 30,218 verses) | OSIS | 0 | 0 | 0 | none | 0 | 0 | 0 | 0 |

The MorphHB codes are `HR/Ncfsa`, `HVqp3ms`, `HNcmpa`, `HTo`,
`HTd/Ncmpa`, `HC/To`, and `HTd/Ncbsa`. They are unqualified values: their
leading `H` is part of the opaque code, not an inferred namespace. The same
words carry unqualified slash-separated lemma values, not Strong IDs. This is
concrete evidence that morphology and lemma/Strong parsing must be independent.

No real morphology-bearing USFM corpus is present locally. Consequently,
USFM morphology support below is supported by a deliberately synthetic
equivalence fixture, not claimed as real-data evidence.

## Synthetic regression evidence

`tests/fixtures/morphology/equivalent.usfm` and `equivalent.xml` express the
same three words and attributes. Each format has 3 morphology-attribute
tokens, 2 morphology-bearing tokens, 4 valid tags, 3 schemes (`robinson`,
`custom.alpha`, `oshb`), 4 unique codes (`N-NSM`, `opaque/code`, `HNcmpa`,
`HR/Ncfsa`), 2 multi-tag tokens, a maximum of 2 tags per token, and 2 malformed
values (`bad:`, `:orphan`). The OSIS fixture combines a Strong lemma and a
non-Strong lemma with morphology. The USFM fixture uses the `x-morph` spelling;
the importer also recognizes `morph` as an equivalent extension spelling,
though no local dataset currently uses it.

The existing operational OSIS test supplies one additional generated token:
`robinson:N-NSM oshm:He,Ncmsa`. It demonstrates that whitespace separates
tags while the comma remains part of the opaque `oshm` code.

## Neutral representation and parsing

`BibleWordInfo::morphologyTags` is an ordered vector of:

```cpp
struct MorphologyTag {
    std::string scheme; // empty for a valid unqualified value
    std::string code;   // opaque, non-empty source code
};
```

Whitespace separates multiple tags. The first colon separates a qualified
scheme from its code; commas, slashes, later colons, case, and punctuation in
the code are preserved. A scheme is accepted when it is non-empty and uses
ASCII letters, digits, `.`, `_`, or `-`. Unknown valid schemes are retained.
An unqualified non-empty value is valid because that is the only form in the
real MorphHB extract. Empty attributes, empty schemes, and empty codes are
audited as malformed and produce no tag. Valid neighboring tags, visible word
text, Strong IDs, and UTF-8 byte offsets remain unchanged.

The importers populate this neutral vector and aggregate audit counts.
MORPH-102 persists valid tags through the source-neutral writer, and MORPH-103
loads them into backend words through one ordered, verse-bounded query.

## SQLite v1-compatible storage

Observed data favors the implemented simple optional child table rather than
dictionaries or a schema-version change:

```sql
CREATE TABLE verse_word_morphology (
  book_id INTEGER NOT NULL,
  chapter INTEGER NOT NULL,
  verse INTEGER NOT NULL,
  word_sequence INTEGER NOT NULL,
  morphology_sequence INTEGER NOT NULL,
  scheme TEXT NOT NULL,
  code TEXT NOT NULL,
  PRIMARY KEY (book_id, chapter, verse, word_sequence, morphology_sequence),
  FOREIGN KEY (book_id, chapter, verse, word_sequence)
    REFERENCES verse_words (book_id, chapter, verse, sequence)
);
```

`morphology_sequence` preserves source order and duplicate tags if they occur.
The empty string stores an explicitly unqualified scheme without inventing
one. Existing v1 readers ignore the optional table, so `schema_version` and
`PRAGMA user_version` remain 1. The writer creates and populates the table in
its transaction, rejects malformed neutral tags, enforces the owning-word
foreign key, and sets `feature.morphology=true` only when valid rows exist. The
SQLite backend loads all tags for a verse with one ordered bounded join, never
one query per word. It advertises morphology only when metadata declares the
feature and the table contains readable, valid, word-owned rows.

## Integration and cost validation (MORPH-104)

No external corpus was downloaded. The largest morphology-bearing real data
available locally remains the source-attributed MorphHB Genesis 1:1 extract:
7 words, all 7 morphology-bearing, 7 rows, 7 unique codes, one observed scheme
(unqualified), no multi-morph words, and a maximum of one tag per word. Its
SQLite foreign-key check and all UTF-8 byte-range/substr checks report zero
errors, and all seven tags round-trip through `SqliteBibleBackend`.

The only scheme in real local morphology data is the unqualified MorphHB form.
The qualified `robinson`, `oshb`, and `custom.alpha` schemes are supported and
round-trip in the equivalent USFM/OSIS fixtures, but are synthetic evidence.
There is no real morphology-bearing USFM corpus in the repository. The local
30,218-verse Nácar-Colunga OSIS file and the real Torres Amat extract contain
no morphology, so they cannot provide morphology cost or round-trip evidence.

For a useful matched cost comparison,
`morphology_integration_benchmark` deterministically repeats the seven real
MorphHB code shapes over 1,500 synthetic verses. Both modules contain 12,000
words and 1,500 identical Strong rows; the morphology variant contains 10,500
tags on 10,500 words, with 7 unique codes, no multi-morph words, and at most
one tag per word. This scale projection is synthetic and is not presented as
additional corpus evidence.

Before the occurrence lookup index was introduced, the local x86-64 Debug
build on 2026-09-08 measured a 1,212,416-byte baseline and a 1,626,112-byte
morphology module: a 413,696-byte (34.1%) increase, or about 39 bytes per tag.
Three complete benchmark runs used nine warmed median batches per operation.
Their median relative ratios were:

| Operation | Morphology / baseline |
|---|---:|
| `getVerseContent()` | 1.24x |
| 30-verse `getChapter()` | 1.00x |
| ordinary FTS phrase search | 0.99x |
| paged Strong concordance | 1.00x |

Individual `getVerseContent()` ratios ranged from 1.22x to 1.28x, the expected
cost of materializing seven extra child rows. Chapter, ordinary search, and
Strong ratios ranged from 0.98x-1.01x, 0.94x-1.00x, and 0.96x-1.00x
respectively; those paths do not join the morphology table, and the small
differences are measurement noise rather than a regression. Normal chapter
reading, search, and Strong concordance therefore meet the existing project
expectation that unrelated paths stay unchanged. The enriched verse path uses
one verse-bounded prepared `LEFT JOIN` ordered by word and tag sequence; it
does not issue a query per word, so there is no N+1 behavior.

The backend morphology pipeline is ready to support separately scoped
higher-level features. The measured verse-read and storage costs do not
justify dictionary normalization or semantic changes. Future UI, morphology
search, or grammatical decoding should preserve opaque schemes and should be
validated against a larger real morphology-bearing corpus if one becomes
legally available; current real-data coverage is only seven Hebrew tokens and
does not demonstrate real multi-tag or qualified-scheme frequency.

## Exact occurrence lookup (MORPH-107)

The neutral backend now pages occurrences for one exact opaque `{scheme,
code}` pair. Results contain the reference, navigation key, word, verse
context, and the word's UTF-8 byte range. Different schemes are never treated
as equivalent. Each SQLite page is one prepared join fetching `limit + 1` rows
to derive `hasMore`, without `COUNT` or per-result queries.

The morphology table's original primary key begins with canonical word
coordinates, so `EXPLAIN QUERY PLAN` could not seek by `scheme` and `code`.
New v1 writers therefore add the covering
`verse_word_morphology_lookup(scheme, code, book_id, chapter, verse,
word_sequence, morphology_sequence)` index. Three runs on the matched
10,500-row scale module measured a common 1,500-occurrence code at offset 300,
limit 50 in 1.10-1.11 ms with the index and 4.90-4.92 ms without it: a
4.43x-4.47x speedup. The real MorphHB extract's rare one-occurrence code took
0.24 ms. Query-plan assertions confirm the lookup index is selected.

The indexed scale module is 1,900,544 bytes, 274,432 bytes larger than the
same morphology layout measured before the index (about 26 bytes per tag), and
56.2% above the no-morphology baseline. The measured lookup gain justifies
this optional index for new modules. Older v1 morphology modules without it
remain readable through the exact-query fallback. Across the same runs,
ordinary verse reads were 1.26x-1.28x the no-morphology baseline, while chapter
reads, text search, and Strong concordance remained within about 1% of the
baseline.
