# Completed foundations

- [x] BASE-001 Extract source-agnostic SqliteModuleWriter
  - Status: DONE
  - Description:
    Shared neutral persistence boundary for importer output.
  - Acceptance criteria:
    - USFM and OSIS use the shared writer without source-specific SQL.
  - Relevant tests:
    - `usfm_importer_test`, `osis_importer_test`, `sqlite_bible_backend_test`.
  - Evidence:
    - Shared writer, SQLite v1 compatibility, atomic rename, FTS and capability handling validated; listed tests PASS.

- [x] BASE-002 Migrate OSIS away from temporary USFM
  - Status: DONE
  - Description:
    OSIS imports directly into the neutral import model.
  - Acceptance criteria:
    - OSIS and USFM remain peer importers.
  - Relevant tests:
    - `osis_importer_test`, `usfm_importer_test`.
  - Evidence:
    - Direct OSIS-to-neutral path validated; both importer tests PASS.

- [x] BASE-003 OSIS container ↔ milestone semantic equivalence
  - Status: DONE
  - Description:
    Container `osisID` and verse `sID`/`eID` forms expose equal neutral content.
  - Acceptance criteria:
    - Supported container and milestone fixtures compare without mismatches.
  - Relevant tests:
    - `osis_semantic_equivalence_test`.
  - Evidence:
    - `container_milestone_mismatches=0`.

- [x] BASE-004 USFM ↔ OSIS semantic equivalence
  - Status: DONE
  - Description:
    Equivalent USFM and OSIS fixtures expose equal neutral content.
  - Acceptance criteria:
    - Content and capability comparison has no mismatches.
  - Relevant tests:
    - `osis_semantic_equivalence_test`, `usfm_importer_test`.
  - Evidence:
    - `usfm_osis_mismatches=0`; both importer regressions PASS.

- [x] BASE-005 Deterministic mixed-content whitespace and UTF-8 offsets
  - Status: DONE
  - Description:
    Pretty and compact XML normalize consistently with byte-valid offsets.
  - Acceptance criteria:
    - Mixed-content comparisons and neutral offset validation pass.
  - Relevant tests:
    - `osis_semantic_equivalence_test`.
  - Evidence:
    - `mixed_content_failures=0`, `invalid_offsets=0`, `substring_mismatches=0`.

- [x] BASE-006 Operational hardening of documented OSIS subset
  - Status: DONE
  - Description:
    Offline XML, audit behavior, atomic failure and representative scale are validated.
  - Acceptance criteria:
    - Operational regression suite passes without failures or temporary artifacts.
  - Relevant tests:
    - `osis_operational_test`, `sqlite_bible_backend_test`.
  - Evidence:
    - `operational_failures=0`; XXE/offline, unknown elements, attributes, morphology audit, partial ranges, atomicity and DOM benchmark validated; decision `KEEP DOM`.

# Next

- [x] OSIS-102 Malformed milestone regression matrix
  - Status: DONE
  - Description:
    Add a compact table-driven matrix for invalid verse milestone sequences not covered by the existing malformed-XML near-EOF case.
  - Acceptance criteria:
    - Cover unmatched `eID`, mismatched IDs, nested starts, missing IDs, and EOF with an open milestone.
    - Each case fails deterministically, reports a useful error, creates no output database, and leaves no `.tmp` file.
    - Valid container and milestone imports remain unchanged.
  - Relevant tests:
    - Extend `osis_operational_test`; run `osis_importer_test` and `osis_semantic_equivalence_test` as regressions.
  - Evidence:
    - Five malformed milestone cases fail twice with exact useful errors and no database or `.tmp` artifacts; `malformed_milestone_failures=0` and `operational_failures=0`.
    - Valid container/milestone imports pass; `container_milestone_mismatches=0`, with importer and semantic-equivalence regressions PASS.

- [x] OSIS-103 Nested note/reference mixed-content regressions
  - Status: DONE
  - Description:
    Exercise safe visible text and target extraction when notes and references contain nested inline markup and formatting whitespace.
  - Acceptance criteria:
    - Footnote and cross-reference display text is deterministic for compact and pretty XML.
    - Reference targets remain ordered and note text does not leak into verse plain text.
    - Footnote and cross-reference byte offsets are within bounds and point at the intended insertion position.
  - Relevant tests:
    - Extend `osis_semantic_equivalence_test`; run `osis_operational_test` as a regression.
  - Evidence:
    - Compact and pretty nested-inline notes compare identically; exact footnote and cross-reference text, no verse-text leakage, UTF-8 insertion offsets, and three ordered nested targets are asserted with `nested_note_failures=0`, `invalid_offsets=0`, and `substring_mismatches=0`.
    - `osis_semantic_equivalence_test`, `osis_operational_test`, and `osis_importer_test` PASS.

- [x] OSIS-101 Real-world regression fixtures
  - Status: DONE
  - Description:
    Add small, redistributable, source-attributed fixture extracts representative of real OSIS producer output within the documented 66-book subset.
  - Acceptance criteria:
    - Fixtures are legally redistributable, minimal, documented, and retain producer-specific structure relevant to the regression.
    - Imported neutral content, metadata/capabilities, and byte offsets are asserted end to end through `SqliteBibleBackend`.
    - Unsupported canon or module types are documented rather than silently accepted or mapped.
  - Relevant tests:
    - Extend `osis_importer_test` or add a focused OSIS fixture test target using actual CMake target names.
  - Evidence:
    - Minimal source-attributed MorphHB (WLC public domain / annotations CC BY 4.0) and public-domain Torres Amat 1882 producer extracts retain their headers, namespaces, word/morphology, and container structures; unsupported `Tob` is documented and rejected without output artifacts.
    - `osis_importer_test` validates persisted metadata, backend metadata/type/capabilities, exact neutral content, morphology audit behavior, and seven UTF-8 word byte ranges/substrings with `real_world_fixture_failures=0`.
    - `osis_semantic_equivalence_test`, `osis_operational_test`, and `sqlite_bible_backend_test` PASS; `invalid_offsets=0`, `substring_mismatches=0`, and `operational_failures=0`.

- [x] OSIS-201 Unicode and whitespace edge-case matrix
  - Status: DONE
  - Description:
    Expand deterministic coverage beyond the current accented UTF-8 and ASCII XML-formatting whitespace cases.
  - Acceptance criteria:
    - Cover multi-byte scripts, combining marks, non-breaking or Unicode spacing where XML permits it, CDATA boundaries, and punctuation adjacency.
    - Define expected preservation/normalization explicitly and validate all neutral byte offsets and substrings.
    - Compact and pretty representations remain semantically equivalent where formatting whitespace is insignificant.
  - Relevant tests:
    - Extend `osis_semantic_equivalence_test`; run `usfm_importer_test` when neutral equivalence data changes.
  - Evidence:
    - Five table-driven compact/pretty cases preserve Greek, Hebrew, CJK, Arabic, Devanagari and Cyrillic UTF-8, decomposed combining marks, NBSP/em-space, CDATA text, and multibyte closing punctuation with `unicode_edge_failures=0`.
    - Exact plain text and every tagged word's UTF-8 byte start, length, text, bounds, and substring are asserted; `invalid_offsets=0`, `substring_mismatches=0`, and compact/pretty comparisons have no mismatches.
    - `osis_semantic_equivalence_test`, `osis_importer_test`, `osis_operational_test`, `usfm_importer_test`, and `sqlite_bible_backend_test` PASS.

- [x] OSIS-202 Bounded deterministic property testing
  - Status: DONE
  - Description:
    Add a fast, reproducible generated-case harness for supported mixed-content combinations after the explicit edge-case matrices exist.
  - Acceptance criteria:
    - Use a fixed seed and strict case/time bounds suitable for local regression runs.
    - Check import determinism, valid byte ranges/substrings, atomic failure, and compact-versus-pretty equivalence.
    - A failure prints the seed and a reproducible minimized or serialized case.
  - Relevant tests:
    - Integrate with an existing OSIS test target or add one explicitly in `tests/CMakeLists.txt`.
  - Evidence:
    - New `osis_property_test` uses fixed seed `5919234`, 24 cases, all seven generated atom kinds, and a 20-second bound; two runs produced the same 72 valid imports, 48 deterministic atomic rejections, and 72 offset rows with `property_failures=0` in about 2 seconds.
    - Compact imports repeated twice match exactly through the neutral backend, pretty variants are equivalent, all byte ranges/substrings validate, and every failure path emits the seed, case index, and serialized compact, pretty, and malformed XML.
    - `osis_importer_test`, `osis_semantic_equivalence_test`, and `osis_operational_test` PASS; semantic and operational counters report zero failures.

- [x] OSIS-301 Heading and paragraph boundary regressions
  - Status: DONE
  - Description:
    Cover pending heading and paragraph state at chapter, wrapper, empty-element, and consecutive-boundary transitions.
  - Acceptance criteria:
    - Headings attach exactly once to the intended following verse.
    - Paragraph breaks neither leak across unrelated structure nor disappear at supported boundaries.
    - Container and milestone representations expose equivalent heading and paragraph state.
  - Relevant tests:
    - Extend `osis_semantic_equivalence_test`; run `osis_importer_test` as a regression.
  - Evidence:
    - Container and milestone boundary fixtures expose identical neutral content across eight verses, with seven ordered headings attached exactly once and six paragraph markers scoped through chapter, wrapper, empty-element, and consecutive-boundary transitions; `heading_paragraph_boundary_failures=0`.
    - Empty paragraphs do not leak to following bare verses, paragraph state survives a supported chapter milestone inside its wrapper, and heading counts include only headings attached to verses.
    - `osis_semantic_equivalence_test`, `osis_importer_test`, `osis_operational_test`, and fixed-seed `osis_property_test` PASS; `invalid_offsets=0`, `substring_mismatches=0`, `operational_failures=0`, and `property_failures=0`.

- [x] OSIS-302 Lock down partial cross-reference range behavior
  - Status: DONE
  - Description:
    Add regression coverage for the currently documented partial OSIS cross-reference range behavior without implementing structured range expansion.
  - Acceptance criteria:
    - Range displayText is preserved.
    - Range references are audited according to the existing documented behavior.
    - No fabricated structured endpoint targets are emitted for unsupported ranges.
    - Valid simple targets surrounding a range remain preserved and ordered.
    - Existing simple cross-reference behavior does not regress.
    - Documentation/tests continue to identify structured ranges as partial or unsupported.
    - This task must NOT implement a new structured range model.
  - Relevant tests:
    - Extend `osis_operational_test` and `osis_semantic_equivalence_test`; run `osis_importer_test` as a regression.
  - Evidence:
    - Operational assertions validate two exact audited range tokens, two ordered surrounding simple targets, one separately audited invalid target, preserved display text, and no fabricated endpoints; `ranges_detected=2` and `operational_failures=0`.
    - Compact and pretty range-note imports are neutral-equivalent with three ordered simple targets around two unsupported ranges, exact display text and insertion offset, and `partial_range_failures=0`, `invalid_offsets=0`, and `substring_mismatches=0`.
    - `osis_operational_test`, `osis_semantic_equivalence_test`, `osis_importer_test`, and fixed-seed `osis_property_test` PASS; documentation continues to identify structured ranges as unsupported and display/audit-only.

# Morphology

- [x] MORPH-101 Audit real morphology data and validate the neutral model
  - Status: DONE
  - Description:
    Audit morphology syntax and data actually present in available USFM/OSIS fixtures and local datasets, then define the smallest source-agnostic neutral representation required to preserve it correctly.
    Morphology must remain independent from Strong identifiers.
  - Acceptance criteria:
    - Audit USFM word attributes relevant to morphology, including actual forms such as `x-morph` or equivalents present in repository/local data.
    - Audit OSIS `<w morph="...">` and relevant lemma/namespace combinations.
    - Report total morphology-bearing tokens, unique schemes/namespaces, unique codes, multi-value cases and malformed/unusable values for available data.
    - Distinguish real dataset evidence from synthetic fixture-only evidence.
    - Define a neutral morphology type capable of preserving an opaque scheme and code without assuming one universal grammar system.
    - Support multiple morphology tags per word when source data requires it.
    - Unknown-but-valid schemes are preserved rather than rejected.
    - Malformed values are audited without corrupting visible text or Strong data.
    - Strong and morphology remain independent fields on neutral word content.
    - UTF-8 word offsets remain byte-based and unchanged.
    - Add equivalent small USFM/OSIS morphology fixtures and demonstrate neutral morphology equivalence where both source formats express the same data.
    - Produce a documented SQLite storage proposal based on observed data, but do NOT persist morphology yet.
    - `feature.morphology` remains false.
  - Relevant tests:
    - Parser/unit tests for neutral morphology parsing.
    - USFM importer tests.
    - OSIS importer tests.
    - Semantic equivalence tests where applicable.
    - UTF-8/offset validation.
  - Evidence:
    - The documented local-data audit distinguishes the real MorphHB, Torres Amat, and generated Nácar-Colunga OSIS evidence from synthetic fixtures; it reports attribute/morphology-bearing token totals, schemes, unique opaque codes, multi-tag maxima, and malformed values. The real MorphHB extract contains 7 morphology-bearing tokens, 7 unique unqualified codes, no multi-value tokens, and no malformed values.
    - `MorphologyTag { scheme, code }` and its source-neutral parser preserve ordered multiple tags, opaque unknown schemes/codes, and valid unqualified MorphHB codes while auditing empty schemes/codes independently from Strong IDs and visible UTF-8 word data. Equivalent USFM `x-morph` and OSIS `w morph` fixtures yield identical neutral tags and audit counts.
    - Morphology remains in memory only: the SQLite v1 child-table proposal is documented but unimplemented, persisted words expose no morphology, and `feature.morphology=false`. `morphology_audit_test` passes in normal and ASan builds; `usfm_importer_test`, `osis_importer_test`, `osis_semantic_equivalence_test`, `osis_operational_test`, fixed-seed `osis_property_test`, and `sqlite_bible_backend_test` pass with zero reported failures.

- [x] MORPH-102 Persist neutral morphology through SqliteModuleWriter
  - Status: DONE
  - Depends on: MORPH-101
  - Description:
    Implement backward-compatible SQLite v1 persistence for the morphology model validated in MORPH-101 and make both USFM and OSIS importers deliver neutral morphology to SqliteModuleWriter.
  - Acceptance criteria:
    - Use the storage design selected from MORPH-101 based on real measured data.
    - Preserve current SQLite schema_version and user_version compatibility unless a proven technical blocker makes that impossible.
    - Prefer an optional v1-compatible table/extension over schema v2.
    - SqliteModuleWriter remains source-agnostic.
    - USFM and OSIS parsers produce the same neutral morphology for equivalent fixtures.
    - No SQL is added to source importers.
    - Multiple morphology tags per word round-trip correctly.
    - Morphology without Strong is preserved.
    - Strong without morphology continues to work.
    - Strong + morphology on the same word preserves both independently.
    - Unknown valid morphology schemes round-trip losslessly.
    - Malformed morphology does not generate invalid persisted rows.
    - Writer validates relationships between morphology rows and their owning word.
    - Failed imports remain atomic with no final output or stale `.tmp`.
    - Existing modules without morphology remain valid and readable.
    - `feature.morphology` becomes true only for modules containing valid persisted morphology data and only if the complete persisted path is operational.
    - Existing Strong, headings, paragraphs, footnotes, crossrefs, FTS and search semantics remain unchanged.
    - Do not normalize morphology into integer dictionaries solely for space savings unless benchmarks show a benefit without harming normal verse-read performance.
    - Normal reading performance takes priority over compactness, and no N+1 queries may be introduced.
  - Relevant tests:
    - SqliteModuleWriter round-trip test.
    - USFM importer test.
    - OSIS importer test.
    - Semantic equivalence test.
    - SQLite integrity / foreign key validation.
    - Atomic failure regression.
  - Evidence:
    - `SqliteModuleWriter` now validates neutral tags and persists ordered morphology in the optional v1-compatible `verse_word_morphology` child table with an owning-word foreign key; schema/user versions remain 1 and `feature.morphology` reflects valid persisted rows.
    - Focused writer coverage verifies multiple and duplicate tags, morphology with/without Strong, Strong without morphology, unknown and unqualified schemes, exact SQL round-trip order, integrity/foreign keys, legacy no-morph output, malformed-tag rejection, and transactional cleanup with `sqlite_module_writer_morphology_failures=0`.
    - Equivalent USFM and OSIS fixtures persist the same four neutral rows and exclude malformed values; `morphology_audit_test`, `usfm_importer_test`, `osis_importer_test`, `osis_semantic_equivalence_test`, `osis_operational_test`, fixed-seed `osis_property_test`, and `sqlite_bible_backend_test` PASS with zero reported failures.

- [x] MORPH-103 Load morphology through SqliteBibleBackend and extend neutral backend contracts
  - Status: DONE
  - Depends on: MORPH-102
  - Description:
    Expose persisted morphology through the existing neutral BibleBackend/BibleVerseContent path and extend Fake/SQLite contract coverage without leaking SQLite or source-format details.
  - Acceptance criteria:
    - SqliteBibleBackend loads morphology into the neutral word representation.
    - No OSIS-specific or USFM-specific read paths are added.
    - Existing modules that lack morphology continue to load without errors.
    - `feature.morphology=false` when metadata/data does not support a valid morphology path.
    - `feature.morphology=true` only when valid morphology data is present and readable.
    - Multiple morphology tags preserve source-defined order where meaningful.
    - Unknown schemes and opaque codes survive backend round-trip unchanged.
    - Strong and morphology remain independently queryable on the same BibleWordInfo.
    - No per-word SQL query is introduced.
    - `getVerseContent()` uses bounded prepared queries and avoids N+1 behavior.
    - FakeBibleBackend gains representative morphology data if needed for the shared contract suite.
    - Shared backend contract tests verify morphology independently of SQLite implementation details.
    - Neutral USFM↔OSIS morphology equivalence remains zero-mismatch through SqliteBibleBackend.
    - Do not add UI changes or a morphology search/concordance API.
  - Relevant tests:
    - BibleBackend contract tests.
    - FakeBibleBackend contract.
    - SqliteBibleBackend contract.
    - SqliteBibleBackend tests.
    - Morphology round-trip/equivalence tests.
    - Existing Strong regressions.
  - Evidence:
    - `SqliteBibleBackend` validates declared morphology against readable, valid, word-owned rows and loads every word/tag for a verse with one bounded ordered `LEFT JOIN`; legacy/no-table, empty, malformed, orphaned, and undeclared data keep `feature.morphology=false` without preventing module loading.
    - Shared Fake/SQLite backend contracts assert ordered multiple tags, unknown and unqualified schemes/codes, morphology without Strong, and Strong plus morphology on the same word. Equivalent USFM/OSIS fixtures round-trip identical neutral tags with `morphology_audit_failures=0`.
    - `sqlite_bible_backend_test`, `bible_backend_contract_test`, `morphology_audit_test`, `sqlite_module_writer_morphology_test`, `usfm_importer_test`, `osis_importer_test`, `osis_semantic_equivalence_test`, `osis_operational_test`, fixed-seed `osis_property_test`, and Strong regressions PASS; focused ASan contract/morphology runs PASS and all standalone SQLite-backend targets link.

- [x] MORPH-104 Validate morphology with real data and measure size/performance regressions
  - Status: DONE
  - Depends on: MORPH-103
  - Description:
    Validate the complete morphology pipeline using the largest representative local datasets available and measure storage/read-performance impact before considering any UI or search functionality.
  - Acceptance criteria:
    - Use real local morphology-bearing USFM/OSIS data when available.
    - Do not download external copyrighted datasets as part of this task.
    - Clearly distinguish real-data measurements from synthetic fixtures.
    - Report word count, morphology-bearing word count, morphology row count, unique schemes and unique codes.
    - Report multi-morph word count and maximum morphology tags per word.
    - Validate zero invalid foreign-key relations.
    - Validate zero invalid word offsets/substrings.
    - Validate morphology round-trip for representative references.
    - Confirm equivalent USFM/OSIS data produces equal neutral morphology.
    - Measure SQLite module size before/after morphology when a comparable baseline is available.
    - Measure `getVerseContent()` before/after morphology.
    - Measure representative chapter reads.
    - Confirm ordinary text search performance is not materially regressed.
    - Confirm Strong concordance performance is not materially regressed.
    - Confirm no N+1 behavior.
    - If a significant regression exists, optimize based on measured evidence rather than changing semantics.
    - Update morphology documentation with supported schemes observed, known limitations and measured costs.
    - Produce an explicit final recommendation about whether morphology is ready for higher-level features.
    - Report quantitative evidence where available: word and morphology rows, unique schemes/codes, multi-morph counts, size delta, verse/chapter read measurements, Strong/search regressions, invalid offsets, foreign-key errors, and semantic mismatches.
    - Do not invent an arbitrary performance threshold; use measured relative regression and existing project performance expectations.
  - Relevant tests:
    - Morphology integration test.
    - Semantic equivalence test.
    - Backend contracts.
    - SQLite integrity / foreign-key checks.
    - Strong regressions.
    - Importer regressions.
    - Performance benchmark(s).
    - Full build.
    - `git diff --check`.
  - Evidence:
    - The real attributed MorphHB Genesis 1:1 extract imports and round-trips 7 words, 7 morphology-bearing words, 7 rows, 7 unique unqualified codes, no multi-morph words, and at most 1 tag per word; SQLite reports zero foreign-key, UTF-8 offset, or substring errors. The repository has no real morphology-bearing USFM corpus; qualified and multi-tag coverage remains explicitly synthetic, with equivalent USFM/OSIS neutral morphology producing zero mismatches.
    - A matched 1,500-verse synthetic scale projection contains 12,000 words and 10,500 MorphHB-shaped rows. Morphology increases SQLite size from 1,212,416 to 1,626,112 bytes (34.1%, about 39 bytes/tag). Across three warmed Debug runs, enriched verse reads are 1.22x-1.28x (median 1.24x); chapter reads are 0.98x-1.01x, ordinary search 0.94x-1.00x, and Strong concordance 0.96x-1.00x, showing no material regression on unrelated paths and no N+1 query behavior.
    - New `morphology_integration_benchmark` passes with `morphology_integration_failures=0`; morphology audit/writer/backend contracts, USFM/OSIS importers, semantic/operational/property suites, and Strong smoke regressions pass. The full `xiphos` build and `git diff --check` pass. Documentation records observed schemes, data limitations, measured costs, and recommends the backend pipeline as ready for separately scoped higher-level features.

# Startup / panel loading UX

- [x] UI-LOAD-101 Audit startup panel lifecycle and define explicit readiness states
  - Status: DONE
  - Description:
    Inspect how the main GTK window and every visible WebKit-backed panel are created, shown, populated and rendered during application startup (including the SUPER+B launch path). Establish a shared, explicit loading/readiness model before changing presentation behavior.
  - Acceptance criteria:
    - Identify every large content panel that can appear blank before its initial content is ready.
    - Identify which panels are Gtk/WebKit-backed and which are native GTK.
    - Trace the initialization sequence from application startup through window show, backend/module readiness, HTML submission and WebKit load completion.
    - Record which existing callbacks/signals are currently available, including `WebKitLoadEvent` / `WEBKIT_LOAD_FINISHED` or equivalent.
    - Determine whether the current white flash comes from WebKit default background, premature widget visibility, delayed HTML submission, delayed rendering, or a combination.
    - Define a small shared panel state model such as EMPTY/LOADING/READY/ERROR without introducing source/backend dependencies.
    - Define which signal/event should cause each panel to transition to READY.
    - Do not hide the entire main window until all panels load.
    - Preserve immediate window/chrome response after SUPER+B.
    - Add temporary instrumentation only if necessary; remove or gate noisy diagnostics before completion.
    - Document the measured current startup sequence sufficiently for UI-LOAD-102.
    - Preserve the intended startup flow: SUPER+B → immediate window/chrome → intentional per-panel LOADING → independent READY transitions; never delay the entire window for all panels.
    - Do not optimize backend performance unless a concrete bug is discovered.
  - Relevant tests:
    - existing GTK/WebKit startup smoke path where available
    - full build
    - manual SUPER+B startup validation
    - git diff --check
  - Evidence:
    - `doc/startup-panel-lifecycle.md` inventories every startup-visible large panel, distinguishes native GTK from the native `WkHtml`/`GtkTextView` renderer, traces startup from `main()` through backend initialization and restored-tab HTML submission, and identifies early `gtk_widget_show_all()` plus delayed real submission as the blank interval; no `WebKitWebView` or `WEBKIT_LOAD_FINISHED` exists in the main reading path.
    - Added the backend/renderer-independent `PanelLoadModel` with EMPTY/LOADING/READY/ERROR states and generation tokens; current native panels become READY when `HtmlOutput()`/`wk_html_close()` returns, while native GTK panels use completion of their synchronous model update. The documented contract keeps window chrome immediate and panels independent.
    - `panel_load_state_test` passes with `panel_load_state_failures=0`; the `xiphos` target and full default build pass, and `git diff --check` passes. The repository headless startup harness was also attempted, but sandbox D-Bus socket creation was denied before application launch; direct display launch was unavailable, so no presentation claim beyond the source audit was made.

- [x] UI-LOAD-102 Eliminate white WebKit surfaces before first content render
  - Status: DONE
  - Depends on: UI-LOAD-101
  - Description:
    Ensure WebKit-backed content regions never expose the WebKit default white surface while their first document is loading. Use theme-coherent background handling and the readiness model defined in UI-LOAD-101.
  - Acceptance criteria:
    - Every affected WebKitWebView receives an intentional initial background matching or coherently derived from the application/GTK theme.
    - Do not hardcode a single light-only or dark-only color if the application supports theme variation.
    - Prefer GTK style/theme-derived colors where practical.
    - Initial WebKit creation must not visibly flash white before HTML/CSS loads.
    - Existing HTML-rendered page backgrounds continue to work after content becomes ready.
    - Changing the WebKit background must not modify Bible HTML semantics.
    - Existing printing/export/rendering behavior must not be changed.
    - No backend or importer logic is modified for this UI problem.
    - Light/dark behavior is checked if the application currently supports both.
    - Existing startup and content regressions continue to pass.
    - Do not consider the task complete merely because the background is no longer pure white if an obviously empty panel remains visible.
  - Relevant tests:
    - GTK/WebKit build
    - existing WebKit rendering tests if present
    - manual cold startup via SUPER+B
    - manual dark/light theme validation where supported
    - git diff --check
  - Evidence:
    - The startup-visible native `WkHtml` panels now put their container, scroller, loading layer, and text renderer on the live `elim_paper`/`elim_ink` GTK palette, show a quiet loading surface until the generation-aware `PanelLoadModel` reaches READY at `wk_html_close()`, and retain the previous ready document during later synchronous reloads. Cold-start flushing no longer replaces that surface with a blank HTML document.
    - The sole real `WebKitWebView` constructor (the optional WebKit1 editor) applies the same GTK surface class and enables transparency before the widget is shown; explicit document body backgrounds remain unchanged, and no print/export, HTML semantics, backend, or importer code was altered.
    - `wk_html_surface_test`, `panel_load_state_test`, and `strong_interaction_test` pass; the `xiphos` target and full default build pass, and `git diff --check` passes. The production stylesheet regression validates its theme-derived light/dark palette references and rejects a literal-white fallback. Cold-start/light-dark presentation was attempted, but this sandbox denies X11 and D-Bus socket creation (`Cannot establish any listening sockets`; `Operation not permitted`), so no unsupported visual claim is made.

- [x] UI-LOAD-103 Add per-panel loading placeholders and readiness-based content reveal
  - Status: DONE
  - Depends on: UI-LOAD-102
  - Description:
    Prevent users from seeing empty content panels while WebKit is loading by introducing a reusable per-panel loading presentation and switching each panel independently to its real content only after the panel is ready.
  - Acceptance criteria:
    - Use an appropriate GTK3 mechanism such as GtkStack, GtkOverlay or an existing project abstraction rather than hiding the complete application window.
    - Each affected content panel has at least LOADING and CONTENT states.
    - The placeholder fills the same panel region and uses theme-coherent styling.
    - Prefer a quiet placeholder/background over multiple distracting spinners unless existing application design strongly favors spinners.
    - Tabs, toolbar, navigation controls and other application chrome remain visible and responsive immediately.
    - Each WebKit panel loads while its placeholder is visible.
    - Each panel transitions independently to content.
    - One slow panel does not prevent another ready panel from becoming visible.
    - Container sizing must remain stable when switching LOADING → CONTENT; no significant layout jump.
    - Existing split-pane sizing and user-resized positions are preserved.
    - Reloading/changing modules/content must not unnecessarily flash back to an empty white panel.
    - If a full reload genuinely requires a loading state, the transition is intentional and non-white.
    - Existing navigation, notes, commentary, dictionary/devotional and Bible displays continue to work.
    - Do not introduce backend knowledge into GTK loading widgets.
    - Inspect the existing UI first and reuse its abstractions where possible; a conceptual GtkStack with loading and WebKitWebView children is acceptable, but avoid duplicating stack/readiness code when a small reusable helper fits.
  - Relevant tests:
    - full GTK build
    - startup smoke
    - Bible panel navigation
    - commentary/notes loading
    - dictionary/devotional loading
    - split pane regression
    - manual SUPER+B cold start
    - git diff --check
  - Evidence:
    - Every `XiphosHtml` content instance now uses the reusable `WkHtml` panel wrapper: a full-allocation, theme-painted loading child and the renderer child share a homogeneous `GtkStack`, with no animated transition or window-level readiness gate. Each instance owns its own `PanelLoadModel`, reveals content only when its current generation reaches READY at `wk_html_close()`, and retains an existing document during later synchronous reloads.
    - The quiet `elim-html-loading` presentation derives its background and subdued text from `elim_paper`/`elim_ink`; the stack remains horizontally and vertically expanding, so Bible, commentary, dictionary/devotional, previewer, parallel, synchronized-reading, and general-book panes preserve their container and split-pane allocations.
    - `wk_html_surface_test`, `panel_load_state_test`, and `strong_interaction_test` PASS; the `xiphos` target and full default build PASS. The GTK regression covers named loading/content children, independent transitions for two panels, homogeneous preferred sizing, expansion, no animation, and production CSS wiring; live widget assertions skip cleanly without a display. The startup harness was attempted but this sandbox rejected D-Bus socket creation before application launch, so no unsupported manual-presentation claim is made.

- [x] UI-LOAD-104 Validate first-paint behavior and loading-state regressions
  - Status: DONE
  - Depends on: UI-LOAD-103
  - Description:
    Harden the panel-loading solution against first-paint timing, slow content, failed loads and reload scenarios so no visible blank frame or permanently hidden content remains.
  - Acceptance criteria:
    - Validate whether WEBKIT_LOAD_FINISHED alone is sufficient to reveal a panel visually.
    - If WebKit can still expose an unpainted frame after LOAD_FINISHED, use the smallest reliable main-loop/paint-safe handoff supported by the current architecture.
    - Do not add arbitrary multi-second sleeps or timing hacks.
    - Do not block the GTK main thread waiting for WebKit.
    - Add explicit ERROR behavior for failed initial loads so a panel cannot remain permanently in LOADING.
    - Error state must be visible and intentional rather than an empty white region.
    - Successful reload after an error can transition to CONTENT.
    - Rapid navigation/reload does not leave stale readiness state.
    - Window close during loading does not cause use-after-free callbacks.
    - Startup with the normal SQLite default backend remains stable.
    - SWORD compatibility startup does not regress.
    - Parallel/SWORD-only views remain safely guarded under SQLite.
    - Measure relevant startup milestones before/after sufficiently to confirm the visual fix did not introduce a major startup delay.
    - Perform repeated cold-ish/manual startup validation with SUPER+B.
    - Record concise evidence that no blank panel is visible during normal startup.
    - Do not postpone `gtk_widget_show()` for the whole window until every WebKit finishes; retain responsive chrome and deliberate per-panel loading states.
  - Relevant tests:
    - GTK/WebKit loading-state regression test where practical
    - application startup smoke
    - SQLite backend startup
    - SWORD compatibility startup
    - navigation/reload regression
    - full xiphos build
    - git diff --check
  - Evidence:
    - Revalidation confirms that production reading panels use synchronous native `GtkTextView`, not `WebKitWebView`: `load_html()` now validates the document before disturbing an existing buffer, attaches its completed off-screen buffer before `wk_html_close()` reveals CONTENT, and schedules no idle/load/paint callback. Thus no `WEBKIT_LOAD_FINISHED` delay is applicable, no main-thread wait was added, and closing a LOADING panel leaves no new callback that could retain it.
    - Every reusable panel now has a full-allocation, theme-derived ERROR child. Initial parse/acquisition failure reaches it through `wk_html_load_failed()` instead of remaining LOADING; failed reloads retain ready content, later success reaches CONTENT, and generation tests reject stale rapid-navigation success/error completions. SQLite/SWORD selection and SQLite guards around SWORD-only parallel views were unchanged.
    - `panel_load_state_test`, `wk_html_surface_test`, `strong_interaction_test`, and `sqlite_bible_backend_test` PASS; focused ASan runs PASS with leak detection disabled because LeakSanitizer cannot operate under this sandbox's ptrace setup. The `xiphos` target and full default build PASS (incremental full build 3.433 s), and `git diff --check` passes.
    - Live GtkStack validation and three SQLite cold-start attempts plus the SQLite/SWORD startup matrix were attempted, but the sandbox denied X11/D-Bus socket creation before application launch. The regression therefore records the source-level paint contract and headless CSS/state results without claiming an unavailable manual screenshot; no arbitrary delay or whole-window readiness gate was introduced.

- [x] UI-LOAD-105 Fix confirmed real-startup white content panels
  - Status: DONE
  - Description:
    Correct the white content rectangles confirmed during a real GTK startup,
    re-auditing rather than trusting the UI-LOAD-101–104 headless results.
  - Acceptance criteria:
    - Map every startup content surface and its actual renderer/container lifecycle.
    - Install a theme-coherent initial surface before the first map.
    - Keep a full-allocation placeholder selected until that panel's first content is ready.
    - Ensure `gtk_widget_show_all()` cannot expose content prematurely.
    - Preserve immediate GTK chrome and independent per-panel readiness.
    - Keep existing content visible during ordinary reload/navigation.
    - Add useful lifecycle diagnostics behind an opt-in debug switch.
    - Build `xiphos`, run focused UI loading regressions, and run `git diff --check`.
    - Record `Automated regression: PASS` only with real command evidence.
    - Record `Manual real-display validation: REQUIRED` unless checked in a real GTK session.
  - Relevant tests:
    - `panel_load_state_test`
    - `wk_html_surface_test`
    - full `xiphos` build
    - manual SUPER+B startup validation
  - Evidence:
    - Root cause corrected: the selected dynamic palette is now installed before `create_mainwindow()` reaches `gtk_widget_show_all()` and drains the first mapped frame; loading surfaces use `@elim_chrome` rather than document `@elim_paper`.
    - Every startup WkHtml surface has an independent named lifecycle trace, a full-allocation placeholder, a renderer child protected by `no-show-all`, and a generation-checked idle reveal after the native off-screen buffer reaches `LOAD_FINISHED`. Existing content remains visible during reloads.
    - Opt-in SQLite startup traces (`BIBLIA_ELIM_UI_LOAD_DEBUG=1`) showed Bible/lower-preview/commentary/dictionary placeholders mapping at 348.4–349.6 ms with no prior renderer map; the sidebar preview handoff was `LOAD_FINISHED 749.7 ms` to `RENDERER_VISIBLE 802.1 ms`. The existing SQLite startup smoke survived its 8-second window.
    - Automated regression: PASS — `panel_load_state_test`, live-Xvfb `wk_html_surface_test`, and `strong_interaction_test`; `xiphos` target builds successfully and `git diff --check` passes.
    - Manual real-display validation: PASS — SUPER+B startup was repeated in the real GTK session; themed placeholders appeared while content loaded, the transition was clean, and no visual regressions were observed.
    - The white-panel startup flash is no longer reproducible.

# Morphology higher-level integration

- [x] MORPH-105 Generalize word interaction from Strong-only to neutral annotated words
  - Status: DONE
  - Depends on: MORPH-104
  - Description:
    Refactor the existing Strong-oriented word interaction path so the UI can interact with any neutrally annotated Bible word, including words that have morphology but no Strong ID, while preserving all current Strong behavior.
  - Acceptance criteria:
    - Inspect the current Strong rendering/click path before changing it.
    - Introduce or reuse a neutral word-annotation interaction abstraction rather than creating a parallel morphology-only click system.
    - HTML/UI interaction continues to identify a word through module/reference/UTF-8 byte offset or the existing neutral mechanism; do not embed backend-specific data into HTML.
    - Words with Strong continue to behave exactly as before.
    - Words with morphology-only data can be resolved through the neutral backend path.
    - Words containing both Strong and morphology expose both sets of neutral annotations.
    - Multi-Strong order remains preserved.
    - Multiple morphology tags remain preserved.
    - Text selection, copy, context menu and double-click behavior do not regress.
    - No SQLite, OSIS, USFM or SWORD types leak into GTK/WebKit interaction code.
    - Existing Strong dialog/concordance functionality remains operational.
    - Use one neutral annotated-word interaction architecture fed by `BibleWordInfo` strongs and morphology; do not create separate duplicated Strong and morphology click paths.
  - Relevant tests:
    - Strong interaction tests
    - neutral word resolution tests
    - morphology backend contracts
    - WebKit interaction regressions
    - full build
    - git diff --check
  - Evidence:
    - Added the source-neutral `BibleAnnotatedWord`/`resolveAnnotatedWord()` contract carrying the owning reference, UTF-8 byte range, visible word, ordered Strong IDs, and ordered morphology tags. The single interaction resolver now accepts Strong-only, morphology-only, and combined annotations while preserving multi-Strong choice behavior and the existing Strong detail/concordance session.
    - Neutral verse rendering now marks every annotated word when either module capability is available and emits only module, passage, and authoritative byte offset through `showNeutralWord`; no Strong, morphology, SQLite, OSIS, USFM, or SWORD data enters the HTML/UI protocol. The GTK text renderer uses the same generalized link path while retaining drag selection, right-click context menus, delayed single-click activation, double-click behavior, and compatibility with existing `showNeutralStrong` URIs.
    - `strong_interaction_test`, Fake and SQLite backend contracts, `sqlite_bible_backend_test`, `wk_html_surface_test`, `morphology_audit_test`, and `morphology_integration_benchmark` PASS; focused ASan interaction/backend-contract runs PASS, all Strong UI/benchmark targets compile, and the full `xiphos` build PASS. Live `strong_ui_smoke_test` execution was attempted, but this sandbox could not open its Xvfb display; the headless interaction regression passes with `wk_html_surface_failures=0`.

- [x] MORPH-106 Display neutral morphology information in word details
  - Status: DONE
  - Depends on: MORPH-105
  - Description:
    Extend the existing word/Strong detail experience to present morphology data from BibleWordInfo without interpreting opaque morphology codes beyond what the backend actually provides.
  - Acceptance criteria:
    - Existing Strong word detail remains intact.
    - When a word has morphology, display scheme and code clearly.
    - Support morphology-only words with no Strong ID.
    - Support Strong-only words.
    - Support words containing both Strong and morphology.
    - Support multiple morphology tags without silently choosing one.
    - Unknown/custom schemes are displayed rather than discarded.
    - Do not decode opaque morphology codes into grammatical labels unless an existing validated decoder/resource already exists.
    - Do not falsely label unknown schemes as Robinson/OSHM.
    - Preserve selectable/copyable detail text where consistent with current dialog behavior.
    - Dialog remains usable with keyboard and Escape/Close behavior.
    - No source-format-specific UI wording such as “OSIS morphology” or “USFM morphology”.
    - Lexicon absence must not prevent morphology from being shown.
    - Existing Strong concordance loading remains independent.
    - Do not add morphology search in this task.
  - Relevant tests:
    - Strong UI/interaction tests
    - morphology annotated-word tests
    - morphology-only fixture
    - Strong+morph fixture
    - multi-morph fixture
    - full GTK build
    - git diff --check
  - Evidence:
    - The existing neutral annotated-word dialog now opens for morphology-only words and displays every morphology scheme and opaque code in source order; unqualified schemes are shown as `Sin especificar`, while unknown/custom scheme names are preserved verbatim without grammatical decoding or source-format wording.
    - Strong-only behavior and the existing lexicon/concordance path remain intact. Combined Strong+morphology words retain all morphology rows with or without a lexicon, and morphology-only details omit the unrelated concordance controls while retaining selectable values and the standard Close/dialog response behavior.
    - `strong_interaction_test`, Fake/SQLite backend contracts, `sqlite_bible_backend_test`, and `morphology_audit_test` PASS; the focused interaction test also passes under ASan. `strong_ui_smoke_test`, `sqlite_strong_ui_smoke_test`, the `xiphos` target, and the full default build compile successfully. The expanded GTK smoke test could not execute because the sandbox denies X11/D-Bus socket creation (`cannot open display: :99`), but it compiles assertions for morphology-only, Strong+morphology, multiple/custom/unqualified tags, selectable values, and concordance independence.

- [x] MORPH-107 Add neutral morphology occurrence lookup with bounded pagination
  - Status: DONE
  - Depends on: MORPH-106
  - Description:
    Add a source-agnostic backend API for locating occurrences of an exact morphology scheme+code, using measured data from MORPH-104 and preserving normal reading performance. This is an exact-tag occurrence lookup, not grammatical interpretation.
  - Acceptance criteria:
    - First inspect actual persisted morphology schema and indexes produced by MORPH-102/MORPH-104.
    - Define a neutral query using exact `{scheme, code}` semantics.
    - Do not infer equivalence between different morphology schemes.
    - Support bounded `limit/offset` or the project’s existing page abstraction.
    - Use limit+1 or equivalent for `hasMore`; avoid COUNT when unnecessary.
    - Query returns references and word context sufficient for a future UI.
    - No per-result/N+1 query behavior.
    - Add appropriate composite index only if query-plan/benchmark evidence justifies it.
    - Measure common and rare morphology codes using representative local morphology data.
    - Verify EXPLAIN/query plan where practical.
    - Existing getVerseContent, chapter reads, text search and Strong concordance remain effectively unchanged.
    - Backend API remains source-agnostic.
    - Fake/SQLite contract coverage is added if this capability belongs in the shared backend contract.
    - Do not add a new UI browser/search screen unless already trivially supported by the existing word-details UI.
    - If initial inspection demonstrates that an occurrence API has no current consumer and would add substantial unnecessary complexity, narrow the task to documenting that evidence and adding only the smallest reusable backend capability required by the existing word-detail UX.
    - Do not redesign persistence or modify the SQLite schema by default; any composite index requires demonstrated query-plan/benchmark evidence.
  - Relevant tests:
    - morphology occurrence backend test
    - Fake/SQLite contract tests where applicable
    - morphology real-data benchmark
    - Strong concordance regressions
    - search regressions
    - getVerseContent benchmark
    - full build
    - git diff --check
  - Evidence:
    - Added source-neutral exact `{scheme, code}` occurrence pages with ordered reference/key, word, verse context and UTF-8 byte range; Fake and SQLite shared contracts verify `limit+1`/`hasMore`, offset boundaries, missing tags/modules, and strict cross-scheme separation. Older SQLite v1 morphology modules without the new optional index retain a tested exact-query fallback.
    - `EXPLAIN QUERY PLAN` confirms the v1-compatible covering lookup index is used. Across three warmed runs on 10,500 MorphHB-shaped rows, a common 1,500-occurrence tag at offset 300/limit 50 took 1.10-1.11 ms indexed versus 4.90-4.92 ms without the index (4.43x-4.47x faster); a rare real MorphHB tag took about 0.24 ms. Chapter reads, text search and Strong concordance remained within about 1% of baseline.
    - `bible_backend_contract_test`, `sqlite_bible_backend_test`, `sqlite_module_writer_morphology_test`, `morphology_audit_test`, `morphology_integration_benchmark`, `usfm_importer_test`, `osis_importer_test`, `osis_semantic_equivalence_test`, `strong_interaction_test`, and the non-display SQLite Strong smoke path PASS; full default build and `git diff --check` PASS. GTK smoke execution was attempted directly and under Xvfb, but this sandbox cannot open a display; its target compiled successfully.

- [x] STARTUP-CRASH-101 Diagnose and fix immediate exit of current biblia-elim build
  - Status: DONE
  - Description:
    Diagnose and fix the immediate startup exit/crash of the current `build/src/gtk/biblia-elim` executable before continuing navigation-readiness work. Determine whether the process is crashing, aborting, exiting cleanly by mistake, or failing only through the launcher. Use concrete runtime evidence such as exit status, signal, logs, coredump/backtrace, ASAN where useful, and lifecycle instrumentation. Apply the smallest coherent fix and preserve current UI-load improvements.
  - Acceptance criteria:
    - Confirm `~/.local/bin/biblia-elim` resolves to `build/src/gtk/biblia-elim`.
    - Compare direct execution and launcher execution.
    - Determine exact exit status and signal, if any.
    - Check recent coredumps and obtain a backtrace when available.
    - Distinguish crash from accidental clean quit.
    - Identify the concrete startup path/function responsible.
    - Audit recent UI-load lifecycle callbacks, deferred callbacks, widget ownership and destroy/quit paths when relevant.
    - Apply the minimum fix supported by evidence.
    - Add a regression test for the identified cause when practical.
    - Run bounded startup/lifecycle stress where possible.
    - The current `biblia-elim` build must remain alive instead of exiting immediately.
    - Preserve the current no-immediate-white-panel startup behavior.
    - Full `biblia-elim` build passes.
    - Relevant UI-load regressions pass.
    - `git diff --check` passes.
    - Final automated state must be `READY FOR CURRENT-BUILD MANUAL STARTUP VALIDATION`.
  - Relevant tests:
    - panel_load_state_test
    - wk_html_surface_test
    - strong_interaction_test
    - crash-specific regression test if added
    - bounded startup smoke/stress
    - full biblia-elim build
  - Evidence:
    - Four recent launcher runs ended with SIGSEGV (not a clean quit or OOM); both current-symbol backtraces converge on `GTKChapDisp::getVerseBefore()` via `main_display_bible()` during synchronous startup rendering. `~/.local/bin/biblia-elim` resolves to the rebuilt `build/src/gtk/biblia-elim`; direct and symlink execution behave identically in the sandbox, while real `uwsm-app` execution is blocked here by its read-only runtime lock/user-bus isolation.
    - The SWORD neutral adapter replaced the module-owned key during `getVerseContent()`, invalidating the `VerseKey *` cached by `GTKChapDisp`; the next `key->getVerse()` caused the crash. It now uses SWORD's in-place `setKeyText()` API. A focused normal/ASan regression preserves key identity through 1,000 repeated reads, and the broader SWORD backend workload passes.
    - `panel_load_state_test`, headless `wk_html_surface_test`, `strong_interaction_test`, `sword_backend_key_lifecycle_test` (normal and ASan), and the full default build PASS. Xvfb and real-display startup automation were attempted but this sandbox cannot create X11 sockets or access the desktop user bus; UI lifecycle/source checks preserve the deferred loading surface and find no startup quit path. Automated state: `READY FOR CURRENT-BUILD MANUAL STARTUP VALIDATION`.
    - A later current-build coredump proves a distinct SIGABRT after navigation: glibc reported `free(): invalid size`, and the first project frame is `on_entry_activate()` at `src/gtk/navbar_versekey.c:370`. Root cause: `main_get_valid_key()` returned borrowed `static std::string::c_str()` storage while multiple C callers treated it as owned and freed it.
    - All eight callers were audited. `main_get_valid_key()` now returns an independent `g_strdup()` allocation documented as caller-owned; every temporary caller uses exactly one `g_free()`, and every `settings.cvparallel` assignment now maintains stable GLib-owned storage rather than aliasing `currentverse`, caller buffers, or the removed static string.
    - New `navbar_valid_key_ownership_test` proves three consecutive results remain independent and valid and may be freed in any order. It and `verse_navigation_readiness_test` pass normally and under ASan (`detect_leaks=0`); `panel_load_state_test`, headless `wk_html_surface_test`, `strong_interaction_test`, and the full `biblia-elim` build pass. The bounded Xvfb startup smoke remains unavailable because sandbox D-Bus socket creation fails before launch. Automated state: `READY FOR REAL-DISPLAY MANUAL VALIDATION`.
    - Real-display current-build validation survived for more than 23 seconds with 23 accepted PREV/NEXT navigations, continuing heartbeats, and no `free(): invalid size`, SIGABRT, or SIGSEGV. The ownership fix therefore appears effective in this session. Multiple fresh launches remain required before DONE; current state: `READY FOR FINAL MULTI-LAUNCH VALIDATION`.
    - Final real-display validation opened the current build 20 times with 0/20 unexpected exits; no launch closed by itself, and immediate plus repeated PREV/NEXT navigation produced no `free(): invalid size`, SIGABRT, or SIGSEGV. Together with the confirmed borrowed `static std::string::c_str()` root cause, the owned `g_strdup()` API, all eight GLib-consistent callers, and the 1,000-iteration normal/ASan regression, this completes the task.

- [x] NAV-LOAD-101 Investigate startup verse-navigation readiness
  - Depends on: STARTUP-CRASH-101
  - Status: DONE
  - Description:
    Measure and decouple previous/next verse navigation readiness from the
    startup renderer lifecycle. Keep this separate from UI-LOAD-105's
    placeholder/reveal fix and do not alter backend/importer behavior.
  - Evidence:
    - `prepareVerseNavigation()` now owns the backend/module/reference/lock
      readiness decision without any renderer or `PanelLoadModel` input; the
      GTK handler applies only accepted targets and reports exact early exits.
      `verse_navigation_readiness_test` proves identical readiness through
      renderer EMPTY/LOADING/READY/ERROR, error cases, and NEXT/NEXT/NEXT.
    - Opt-in `BIBLIA_ELIM_UI_LOAD_DEBUG=1` timestamps cover backend readiness,
      arrow sensitivity, window/main-loop milestones, each navigation decision,
      reference transition, display completion duration, and GTK heartbeat.
      SQLite and SWORD now share the completion/unblock path.
    - Focused tests pass normally and under ASan; panel/surface, fake/SQLite
      backend contract and boundary, SWORD key-lifecycle, strong interaction,
      and the full `biblia-elim` build pass. Live GtkStack assertions report
      `no display`; direct X11 startup exits before `GTK_INITIALIZED`, and Xvfb
      cannot keep its server alive in this sandbox. Required real-display
      startup and rapid-click validation remains unavailable here; a human must
      run the instrumented build on an accessible desktop and verify startup
      plus rapid NEXT/NEXT/NEXT before this task can be marked DONE.
    - Real-display repeated navigation now passes after startup: all 23
      PREV/NEXT clicks reached the handler and were accepted, navigation
      readiness followed each click by approximately 0.5–0.7 ms, display
      completion was normally about 5–7 ms, and heartbeats continued after
      navigation without instability. This session does not validate PREV/NEXT
      immediately as the window appears because its first click occurred
      several seconds after startup, so the task remained pending and BLOCKED
      until the final immediate-startup validation below.
    - Final real-display validation used PREV/NEXT immediately after launching
      through SUPER+B: immediate-startup navigation passed without a crash.
      Navigation readiness remains decoupled from renderer/WebKit state; the
      earlier repeated-navigation session measured sub-millisecond
      `NAVIGATION_READY` and normally about 5–7 ms `DISPLAY_COMPLETE`.
  - Required validation:
    - Automated regression: PASS
    - Manual real-display validation: PASS (immediate startup and repeated PREV/NEXT)

- [ ] UI-LAYOUT-101 Diagnose repeated negative-height GTK allocation warnings
  - Status: PENDING
  - Description:
    Investigate and fix repeated `gtk_widget_size_allocate(): attempt to
    allocate widget with positive width and height -5` warnings observed
    continuously during normal real-display use. Treat this as a separate
    layout bug and do not assume it caused the previous invalid free.
  - Acceptance criteria:
    - Identify the exact widget/container generating the negative allocation.
    - Identify the caller/layout calculation responsible.
    - Determine why the height becomes `-5`.
    - Confirm whether repetition is caused by a size-allocation/layout feedback loop.
    - Fix the geometry calculation at its source.
    - Do not add a clamping hack unless the actual layout contract requires it.
    - The warning disappears during idle.
    - The warning disappears during repeated PREV/NEXT.
    - No visual regression is introduced.
    - No white-panel regression is introduced.
    - No navigation regression is introduced.
    - Relevant GTK/WebKit tests pass.
    - Full `biblia-elim` build passes.
    - `git diff --check` passes.
    - Real-display validation is required if the sandbox has no display.
  - Relevant tests:
    - panel_load_state_test
    - wk_html_surface_test
    - verse_navigation_readiness_test
    - bounded idle and repeated-navigation GTK smoke
    - full biblia-elim build
  - Evidence:
    - A real-display current-build session emitted hundreds of repeated
      `gtk_widget_size_allocate()` warnings for width 396 and height -5,
      apparently continuously during normal use. Pending diagnosis.

- [ ] NAV-ANCHOR-101 Stop navbar callbacks from mutating borrowed GtkEntry text
  - Status: PENDING
  - Description:
    Replace the four navbar callbacks' in-place mutation of the borrowed
    `gtk_entry_get_text()` buffer with an owned local parse that preserves the
    original `#` or `!` delimiter and leaves `settings.special_anchor` with a
    valid lifetime throughout navigation.
  - Evidence:
    - Deferred from STARTUP-CRASH-101 because it is independent from the
      confirmed `main_get_valid_key()` invalid-free root cause.

- [x] BRAND-101 Rename runtime executable from xiphos to biblia-elim
  - Status: DONE
  - Description:
    Make the primary CMake target and development runtime artifact use the
    `biblia-elim` executable name without renaming historical source symbols,
    resource paths, or compatibility data directories.
  - Evidence:
    - The primary target is now `biblia-elim` and builds
      `build/src/gtk/biblia-elim`; CMake no longer generates a target named
      `xiphos` for the main executable. Existing `build/src/gtk/xiphos` is a
      stale artifact from the previous build graph and was not deleted.
    - `~/.local/bin/biblia-elim` resolves to the new build artifact. The
      preserved backup `~/.local/bin/biblia-elim.installed-before-build` was
      not removed.
    - SUPER+B still invokes `launch-or-focus-class biblia-elim
      'uwsm-app -- biblia-elim'`, so it resolves through the updated launcher.
    - Startup/backend regression scripts and the development install helper
      now reference `build/src/gtk/biblia-elim` and target `biblia-elim`.
    - Desktop launch metadata uses visible name `Biblia Elim` and
      `Exec`/`TryExec` `biblia-elim`; the existing `StartupWMClass=biblia-elim`
      metadata was left unchanged because it already matches the runtime
      application class.
    - Runtime/config compatibility paths and historical/internal `xiphos`
      symbols remain intentionally unchanged for a separate cleanup phase.
  - Tests:
    - `cmake -S . -B build`
    - `cmake --build build --target biblia-elim -j$(nproc)`
    - `git diff --check`

# Future / not scheduled

- Human-readable grammatical decoding of morphology codes.
- Dedicated morphology search/browser UI beyond MORPH-107's bounded backend capability.
- Cross-scheme grammatical equivalence.
- Deuterocanonical OSIS support.
- Structured OSIS range expansion.
- `xmlTextReader` migration; measured decision remains `KEEP DOM`.
- Non-Bible OSIS modules.
- Importer-specific UI.
- e-Sword importer.
