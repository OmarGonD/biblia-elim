# Bible SQLite Module Format v1

## Purpose

Version 1 is a small, self-contained read-only SQLite format for Bible
modules. A module is distributed as one file named `<module-id>.sqlite`.
Importers produce this file; consumers do not need to know the source format.

## Versioning

Every file must contain `PRAGMA user_version = 1` and
`metadata.schema_version = 1`. These values describe the technical schema and
must agree. `PRAGMA user_version` is authoritative for opening the file;
`metadata.schema_version` is the human-readable declaration. Missing or
unsupported values cause controlled rejection. Future versions may use
another opener while v1 remains unchanged.

## Schema

```sql
CREATE TABLE metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE books(
  book_id INTEGER PRIMARY KEY,
  osis TEXT NOT NULL UNIQUE,
  name TEXT NOT NULL,
  short_name TEXT,
  testament INTEGER NOT NULL,
  position INTEGER NOT NULL UNIQUE
);
CREATE TABLE verses(
  book_id INTEGER NOT NULL,
  chapter INTEGER NOT NULL,
  verse INTEGER NOT NULL,
  text TEXT NOT NULL,
  PRIMARY KEY(book_id, chapter, verse),
  FOREIGN KEY(book_id) REFERENCES books(book_id)
);
```

Optional v1 extension tables may be present without changing the schema
version. The importer uses `verse_annotations` for paragraph breaks,
`headings` for neutral headings, and `verse_spans` for text styles such as
`style = 'added'`. `verse_words` stores optional word ranges and Strong
metadata; `strong` is comma-separated when a token has multiple IDs. Word
`start` and `length` are byte offsets into the UTF-8 `verses.text` string.
Readers that do not know these tables continue to read `verses` normally.

New imports may additionally include the optional derived
`verse_word_strongs` table. New files use the covering index
`(strong, book_id, chapter, verse, sequence)` so a reader can page in canonical
order before fetching word and verse text. It does not change `schema_version`;
readers retain the legacy `strong`-only query for older v1 modules, and modules
without the derived table remain readable.

New imports also include the optional `verse_word_morphology` child table.
Each row stores an opaque `scheme` and `code` for a `verse_words` row, while
`morphology_sequence` preserves source order and duplicates. An empty scheme
means the source supplied a valid unqualified code. Its composite foreign key
prevents orphan morphology. Readers that predate this v1 extension can ignore
the table. New writers add the covering `verse_word_morphology_lookup` index on
`(scheme, code, book_id, chapter, verse, word_sequence, morphology_sequence)`
for bounded exact-tag occurrence lookup. Older v1 morphology modules without
the index remain readable and use the same exact query semantics.

`book_id` is a module-global identifier and need not equal `position`.
`osis` is the neutral canonical identifier (`Gen`, `John`, `Rev`, etc.).
`position` controls presentation order; `testament` is only an ordering/grouping
hint, not the book identity.

## Required metadata

Mandatory keys are:

```text
schema_version
module_id
name
language
module_type = bible
versification = kjv | custom
feature.verses = true
feature.search = true
feature.strong
feature.morphology
feature.headings
feature.footnotes
feature.crossrefs
feature.dictionary
```

Feature values are explicit `true`/`false` (or `1`/`0`). `feature.strong=true`
means that valid Strong IDs can be returned by `BibleWordInfo.strongs` and that
the matching `verse_word_strongs` relation supports concordance. Table presence
alone never enables it. A declaration of `true` is downgraded to false if that
functional infrastructure cannot be validated. For compatibility, an absent,
malformed, or explicit false Strong flag is safely treated as false; other
required feature flags remain mandatory. A Strong lexicon is an independent
application resource and is never inferred from this flag.
`feature.morphology=true` means at least one valid morphology child row was
persisted; table presence alone does not enable it.

Optional metadata includes `description`, `abbreviation`, `publisher`,
`copyright`, `license`, `source`, `source_format`, and `content_version`.
Copyright and licensing are metadata; the format does not assume public-domain
content.

## Versification and validation

`kjv` and `custom` are accepted. No conversion is performed, and `custom` is
never silently treated as KJV. Opening validates required tables and metadata,
supported versions, boolean feature values, positive chapter/verse numbers,
existing book references, and foreign-key integrity. Invalid files are
ignored as modules.

## Search index

`verses_fts` is an optional derived FTS5 index. `verses` remains authoritative.
It can be rebuilt deterministically with:

```sql
DELETE FROM verses_fts;
INSERT INTO verses_fts(rowid, text) SELECT rowid, text FROM verses;
```

Index creation belongs to module creation/import, never normal navigation.
Readers open modules read-only.

## Example and scope

`footnotes` and `cross_references` are optional v1 tables. They use
`(book_id,chapter,verse,sequence)` keys plus a byte `offset`; footnotes store
caller/body and cross-references store cleaned display text. Their metadata
flags are enabled only when at least one valid row was imported, and readers
downgrade inconsistent declarations to false. USFM import currently supports
`\\f`/`\\x` with `\\ft`, `\\fq`, `\\fqa`, `\\fk`, `\\fl`, `\\fw`, `\\fp`, `\\fv`,
`\\xo`, `\\xk`, `\\xq`, and `\\xt` content; unsupported submarkers are cleaned
without entering `verses.text`. Cross-reference target resolution is best
effort and unresolved text remains in `display_text`.

`tests/fixtures/sqlite/test-bible.sql` is a complete baseline v1 example. The
format does not define comments, dictionaries, compression, packages, or other
import formats.
