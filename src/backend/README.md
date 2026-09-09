# Bible backend boundary

The normal application flow is now:

```text
GTK / display / main
        -> BibleBackend
        -> SqliteBibleBackend (default when modules are available)
        -> SwordBackend (compatibility / legacy)
```

`bible_backend.h` and `bible_types.h` are format-neutral. They must not include
SWORD headers or retain pointers/references to backend-native objects.
`sword/sword_backend.cc` owns translations between these values and SWORD,
including `EntryAttributes`, module-type codes, render filters and
global-option names.

## Ownership

The application owns its primary implementation as a
`std::unique_ptr<BibleBackend>`. Today that object is a `SwordBackend` whose
base `BackEnd` subobject owns the existing long-lived `SWMgr`. The global
`bible_backend` pointer is a non-owning neutral view. The global `backend`
pointer is only a non-owning downcast retained for the legacy SWORD paths; it
must never be deleted or replaced directly. Module-manager reload destroys the
old neutral owner before constructing its replacement, then refreshes both
views, so two primary `SWMgr` instances never overlap.

`BackEnd` has a protected constructor, so new application implementations are
created as `SwordBackend`, not as the old infrastructure class. Existing
isolated managers used by advanced search, parallel windows and special module
dialogs remain local legacy owners; this stage did not add another primary
manager or change their lifecycle.

## Preferred API

Normal verse display uses `getVerseContent(module, BibleReference)`. A
`BibleVerseContent` supplies plain and rendered text, word metadata
(`lemma`, Strong number, morphology and gloss), rendered headings and whether
SWORD already supplied footnote numbers. Text and entry attributes are
obtained from one verse render. Plain-text stripping is optional and disabled
by default so the HTML display path does not pay for data it does not consume.

`getCurrentEntryFootnote()` and `getCurrentEntryCrossReferences()` isolate
footnote attribute lookup. `lookupDictionary()` isolates ordinary SWORD
dictionary rendering. `setOption(BibleOption, bool)` and
`configureRendering()` isolate SWORD option/filter names.

`BibleModuleType` replaces the public integer module codes. The SWORD adapter
maps native type/configuration strings to `Bible`, `Commentary`, `Dictionary`,
`Lexicon`, `GeneralBook`, `PersonalCommentary`, `PrayerList` or `Unknown`.
`BibleModuleCapabilities` exposes only the capabilities currently needed by a
portable implementation: verses, search, Strong data, morphology, dictionary
lookup and editability.

Normal renderers receive `BibleVerseContent` through their `BibleBackend*` and
then apply the existing HTML/layout code locally. `renderedText` can contain
the historical XHTML-compatible output for behavioral compatibility; it is a
temporary compatibility field, not a requirement that future backends own UI
or CSS decisions. `configureRendering()` likewise preserves SWORD filter
ordering and note-number behavior and is a no-op by default.

## Legacy compatibility APIs

`getText(module, key, rendered)` is retained as a legacy string-key adapter.
New verse-reading code should use `BibleReference` plus `BibleVerseContent`.
The string-based `setGlobalOption()` and `get_entry_attribute()` methods are
also compatibility APIs for dynamic search options and the pending parallel
view; their SWORD implementation nevertheless lives in `backend/sword/`.
The common interface supplies conservative defaults for legacy option and
current-entry helpers so a future backend does not have to implement SWORD-only
behavior.

`SWDisplay` callbacks remain the outer adapter that starts historical display
rendering, and advanced search still uses a separate `BackEnd*` view for SWORD
index/range/cancellation operations. Normal content retrieval and search calls
on those paths use `BibleBackend*`.

## Migrated enriched-reading paths

- **A — Bible text:** the normal chapter pane, its adjacent-verse previews,
  chapter/book intro material, commentary chapter panes and dialog chapter
  panes obtain rendered verse text through `BibleBackend`.
- **B/C/D — Strong, morphology, lemma/gloss:** word attributes are converted
  to `BibleWordInfo` only in `SwordBackend`. Normal display consumes the
  backend-rendered text and no longer accesses SWORD filters. The specialized
  interlinear occurrence index remains a separate legacy path.
- **E — footnotes:** display receives numbering metadata in
  `BibleVerseContent`; normal URL/dialog note bodies and cross-references use
  neutral footnote operations. The parallel view remains legacy.
- **F — headings:** normal and print chapter displays receive rendered
  `BibleHeading` values from the same backend content read. Parallel headings
  remain legacy.
- **G — dictionaries:** ordinary SWORD dictionary display uses
  `lookupDictionary()`. `main/diccionario.cc` is the independent bundled XML
  dictionary plus commentary excerpts; it had no direct SWORD types and its
  format was intentionally left unchanged.
- **H — special paths left native:** `SWDisplay` callback signatures,
  VerseKey-based bookmark colour/canonical ordering, complex GeneralBook
  `TreeKey` display levels, editable personal modules, prayer lists, printing
  of arbitrary entries, parallel views, export, pulpito and advanced URLs.

## Audit for this stage

The reproducible reference metric is occurrences of
`sword::|SWMgr|SWModule|SWKey|VerseKey|EntryAttributes|#include <sword/`.

| Scope | Before | After |
| --- | ---: | ---: |
| `src/main/display.cc` | 47 | 43 |
| `src/main/diccionario.cc` | 0 | 0 |
| `src/gtk/` | 0 | 0 |
| `src/webkit/` | 0 | 0 |

The broad metric above includes required `SWDisplay` method signatures and
comments. The more relevant direct content/filter calls in `display.cc`
(`getEntryAttributes`, `getRenderFilters`, `setGlobalOption`, `getRawEntry`,
`renderText`) fell from **46 to 23**; `renderText` specifically fell from
**28 to 8**, `getRenderFilters` from **4 to 0**, and `SWMgr` from **1 to 0**.
SWORD-related includes in that file fell from **8 to 2**. The remaining eight
direct `renderText` calls are confined to GeneralBook/TreeKey, arbitrary-entry
and print paths, not the normal Bible chapter route.

No importer or index rebuild is part of this stage. The existing
module-manager reload now replaces the neutral owner instead of
deleting/constructing `BackEnd` directly.

## Minimal SQLite backend

`sqlite/SqliteBibleBackend` is the first alternative implementation. It opens
one read-only SQLite connection per discovered `.sqlite` file and keeps those
connections and prepared statements for the backend lifetime. Discovery reads
the module identity from `metadata`; invalid, corrupt or incomplete files are
ignored and never reach the core as modules. Access is intentionally confined
to the thread that constructed the backend, matching the current GTK/main-loop
usage and avoiding unnecessary locks.

The stable `Bible SQLite Module Format v1` is documented in
[`sqlite-module-format-v1.md`](sqlite-module-format-v1.md). Its schema is:

```sql
metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL)
books(book_id, osis, name, short_name, testament, position)
verses(book_id, chapter, verse, text,
       PRIMARY KEY(book_id, chapter, verse))
verses_fts USING fts5(text, content='verses', content_rowid='rowid')
```

Every module declares schema version, module identity, versification and
explicit feature flags. `book_id` is global within the module and `osis` is
the canonical book identifier; no reader assumes a fixed 66-book canon.
SQLite modules currently advertise only `verses` and textual `search`;
enriched attributes, dictionary lookup, editing, headings and footnotes are
not fabricated. Phrase, multi-word and indexed queries use FTS5 when present;
regex and attribute modes return an empty neutral result for now.

The fixture is [test-bible.sql](../../tests/fixtures/sqlite/test-bible.sql).
The SQLite contract and error tests create temporary databases, so CI does not
need `~/.sword/`. Backend selection is performed once during startup. Explicit
`--backend=...` has priority, followed by `BIBLIA_ELIM_SQLITE_MODULES`, then
the application data directory (`biblia-elim/modules`). If SQLite has no valid
modules, startup logs a warning and falls back to SWORD; invalid Bible
references do not change the selected backend.

```text
xiphos                         # SQLite default, with SWORD fallback
xiphos --backend=sword
xiphos --backend=sqlite:/path/to/modules
BIBLIA_ELIM_SQLITE_MODULES=/path/to/modules xiphos --backend=sqlite
```

`tests/sqlite_bible_backend_benchmark` measures startup, 100 chapter reads,
1000 verse reads and one phrase search. It uses the same workload shape as a
future Sword comparison; Sword remains a manual comparison because its
application lifecycle requires the full GTK/SWORD runtime.

## USFM importer

`biblia-usfm-import` is an independent command-line importer. It accepts one
or more `.usfm`/`.sfm` files or a directory, reads `\\id`, `\\h`, `\\toc1-3`,
`\\c` and `\\v`, and writes a v1 module transactionally through a temporary
file. The canonical book table in `bible_book_map.*` supplies stable IDs and
OSIS codes for all 66 Protestant books. Inline visual markers are removed
while their text is retained; `\\f` and `\\x` blocks are omitted and counted.
Unsupported markers are grouped in the importer statistics. Example:

```text
biblia-usfm-import ./usfm --module-id rv1909 --name "Reina-Valera 1909" \\
  --language es --versification kjv --output rv1909.sqlite
```

The importer has no SWORD dependency. Its end-to-end test imports USFM,
opens the result with `SqliteBibleBackend`, and exercises reading, navigation
and search.

RV1909 word annotations are stored in the optional `verse_words` table. They
provide UTF-8 byte offsets, visible token text and ordered comma-separated
Strong IDs. Imports containing valid IDs now declare `feature.strong=true`;
`SqliteBibleBackend` validates both word exposure and concordance before
advertising it. Morphology remains false, and no lexicon is implied by the
Bible capability.

The neutral annotated-word interaction is:

```text
clicked UTF-8 byte offset -> resolveAnnotatedWord() -> BibleAnnotatedWord
no annotations -> no word action
morphology only -> resolve the annotated word for the neutral detail path
1 Strong ID -> open that ID
N Strong IDs -> present every ID, in source order, as independent choices
chosen ID -> optional BibleApplicationResources::lookupStrong()
          + findStrongOccurrencePage()
```

Each concordance occurrence already contains the requested ID, token,
reference, navigation key and verse context. Pagination fetches `limit + 1` in the same
prepared statement to expose `hasMore`; an exact total is deliberately not
calculated on every page.

The parallel neutral morphology occurrence API accepts one exact opaque
`{scheme, code}` tag and returns its token, UTF-8 byte range, reference,
navigation key, and verse context. It never equates codes from different
schemes. The SQLite implementation also fetches `limit + 1` in one prepared
join and uses the optional v1-compatible `verse_word_morphology_lookup`
covering index when present; older v1 morphology modules retain a scan fallback.

The neutral GTK display marks words when either the Strong or morphology
capability is true and the word has at least one corresponding annotation.
It emits `BibleWordInfo.start` as `data-offset` and as the click payload; it
never derives byte positions from the rendered text and never embeds Strong or
morphology values as the source of truth. A click without drag resolves the
word again through `BibleBackend`, returning both ordered Strong IDs and ordered
morphology tags. Existing Strong words retain their lightweight detail dialog;
multiple IDs remain unselected until the user chooses one in source order. The
dialog uses an optional application-level `BibleLexicon`, loads concordance
pages in groups of 50, and navigates with the existing neutral module/key path.
Morphology detail presentation is deliberately deferred to MORPH-106.

The final ownership-stage audit uses the requested expression
`BackEnd*|SWDisplay|SWMgr|SWModule|SWKey|VerseKey|TreeKey|EntryAttributes|include <sword/>`.
Its exact remaining occurrence counts are:

| Scope | Before ownership stage | After |
| --- | ---: | ---: |
| `src/main/display.cc` | 48 | 48 |
| `src/main/diccionario.cc` | 0 | 0 |
| `src/gtk/` | 0 | 0 |
| `src/webkit/` | 0 | 0 |

`display.cc` is unchanged by this metric because its remaining native types
belong to `SWDisplay` callback signatures, `VerseKey` iteration and the
explicitly deferred GeneralBook/TreeKey and arbitrary-entry paths. Its normal
verse data calls are through `BibleBackend`; a small
`prepare_display_content()` hand-off now makes retrieval and visual cleanup
separate operations.

Across normal ownership declarations, the primary application owner, nine
display constructor/member declarations and two search pointers changed from
**12 `BackEnd*` dependencies to 0**. Three explicitly named non-owning legacy
views remain elsewhere: the global SWORD adapter and one in each search UI for
index/range/cancellation support.

## Backend-independent contract

`tests/fake_bible_backend.*` implements the common interface entirely in
memory with Genesis 1:1, John 3:16-17, one enriched word and one dictionary
entry. `tests/bible_backend_contract.*` checks module discovery, metadata,
neutral type/capabilities, key resolution, chapter access, navigation,
enriched content, search, invalid input and dictionary lookup. Its executable
does not compile or link the SWORD adapter and never reads `~/.sword/`.

## SQLite module management

User-installed modules use `$(g_get_user_data_dir())/biblia-elim/modules`.
The neutral module manager validates the existing SQLite v1 contract, copies
`.sqlite` files to `<module_id>.sqlite` via a temporary file and atomic rename,
and rejects duplicates. USFM imports call the shared `UsfmImporter` used by
`biblia-usfm-import`, then follow the same validation/install path. The GTK
dialog only calls this API and refreshes the active backend after changes.

`--backend=sword` forces the compatibility backend and
`--backend=sqlite:/path` selects an explicit directory. If the default SQLite
directory has no valid modules, startup falls back safely to SWORD.

`BibleVerseContent` carries optional `footnotes` and `crossReferences`. The
renderer emits only lightweight sequence markers; dialogs resolve the selected
entry through the neutral backend API. Footnote callers `+`/`-` are displayed
as generated per-verse numbers, while explicit callers are preserved. Cross
reference targets are shown in source order and only resolved targets are
navigable; unresolved display text remains selectable. No marker is inserted
into the stored verse text.

`biblia-osis-import` supports the initial Bible subset of OSIS and converges
through the same SQLite v1 writer as USFM (`source_format=osis`). OSIS parsing
is offline and non-networked; commentaries, dictionaries and morphology are
outside the current subset.

## SQLite module writer

`SqliteModuleWriter` is the source-agnostic persistence boundary for SQLite
module format v1. Importers produce neutral books and enriched verses, then
the writer owns schema creation, prepared inserts, feature derivation, FTS
construction, offset validation, transaction rollback and atomic final rename.
It has no dependency on USFM, OSIS or SWORD code; OSIS migration to this
interface is intentionally a separate step.
