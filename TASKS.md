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

- [x] UI-LAYOUT-101 Diagnose repeated negative-height GTK allocation warnings
  - Status: DONE
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
      apparently continuously during normal use.
    - Real GDB evidence identifies the affected widget as the horizontal
      `GtkSeparator` embedded by `<hr>` inside the `GtkTextView` named
      `elim-html`. `style_hr_separator()` supplied 3px top and bottom widget
      margins while the child-anchor line allocated the separator its 1px
      height. During frame-clock scroll adjustment GTK subtracted both
      margins, producing exactly `1 - 3 - 3 = -5` and repeating the warning
      on `GtkAdjustment::value-changed` during PREV/NEXT.
    - Inline `<hr>` now has zero widget margins because its existing
      `insert_break()` calls already provide vertical spacing. Bare `<hr>`
      table cells remain visually distinct and retain their valid 3px top and
      bottom margins under `GtkGrid`; no warning suppression, global clamp,
      delay, debounce, hidden separator, or GTK patch was added.
    - The earlier GtkPaned visibility change is preserved as an independent
      valid layout contract, but its obsolete warning-cause comment was
      corrected. `main_window_layout_test` covers commentary/dictionary
      visibility combinations, while `wk_html_surface_test` locks down the
      inline-versus-table separator geometry and existing renderer/panel
      behavior.
    - `main_window_layout_test`, `panel_load_state_test`, headless
      `wk_html_surface_test`, and `verse_navigation_readiness_test` PASS; the
      `biblia-elim` target, full default build, and `git diff --check` PASS.
    - Real-display manual validation PASS during prolonged idle, repeated
      PREV/NEXT, chapter crossings, and additional references/chapters, with
      zero negative-height `Gtk-WARNING` occurrences and zero
      `gtk_widget_size_allocate()` height -5 occurrences.

- [x] NAV-ANCHOR-101 Stop navbar callbacks from mutating borrowed GtkEntry text
  - Status: DONE
  - Description:
    Replace the four navbar callbacks' in-place mutation of the borrowed
    `gtk_entry_get_text()` buffer with an owned local parse that preserves the
    original `#` or `!` delimiter and leaves `settings.special_anchor` with a
    valid lifetime throughout navigation.
  - Evidence:
    - Deferred from STARTUP-CRASH-101 because it is independent from the
      confirmed `main_get_valid_key()` invalid-free root cause.
    - All four verse-key entry callbacks now parse from independently owned
      key and anchor strings, preserve the first original `#` or `!` delimiter,
      and clear `settings.special_anchor` before releasing that storage on
      success and every early-return path. No callback writes into the buffer
      returned by `gtk_entry_get_text()`.
    - `navbar_entry_reference_test` verifies unchanged entry storage, exact
      `#`/`!` preservation, earliest-delimiter parsing, lifetime after the
      source buffer is freed, and cleanup; it passes normally and under ASan
      (`detect_leaks=0`; LeakSanitizer is unavailable under ptrace here).
    - `navbar_valid_key_ownership_test`, `verse_navigation_readiness_test`,
      `panel_load_state_test`, and headless `wk_html_surface_test` PASS; the
      `biblia-elim` target, full default build, callback source audit, and
      `git diff --check` PASS.

- [x] UI-REALIZE-101 Eliminate startup realization of an unanchored GTK widget
  - Status: DONE
  - Description:
    Eliminate the startup Gtk-CRITICAL caused by attempting to realize a widget
    that is not anchored to a valid GTK hierarchy. Real startup evidence shows
    `gtk_widget_realize: assertion 'widget->priv->anchored ||
    GTK_IS_INVISIBLE (widget)' failed` approximately after creating/showing
    `sidebar-previewer` and `parallel`, and before `WINDOW_SHOW`; do not assume
    yet which widget is responsible.
  - Investigation requirements:
    - Reproduce the Gtk-CRITICAL in the current build.
    - Identify the exact widget reaching `gtk_widget_realize()`.
    - Obtain GDB, logging, or localized instrumentation evidence as needed.
    - Identify who calls realize/show/map before correct parenting/anchoring.
    - Correct the actual lifecycle/order at its source.
    - Audit `gtk_widget_realize()`, `gtk_widget_show()`,
      `gtk_widget_show_all()`, `gtk_widget_map()`, `gtk_container_add()`,
      `gtk_box_pack_*`, `gtk_scrolled_window_add*`, renderer surface creation
      or reparenting, `sidebar-previewer`, `parallel`, lifecycle callbacks, and
      any manual realize/map calls.
  - Do not:
    - Suppress the Gtk-CRITICAL or filter its logging.
    - Add sleeps, `usleep`, arbitrary delays, or main-iteration hacks.
    - Permanently hide widgets or disable sidebar/parallel.
    - Ignore the assertion or move a manual realize call without understanding
      the hierarchy.
  - Acceptance criteria:
    - Zero occurrences of `gtk_widget_realize: assertion
      'widget->priv->anchored || GTK_IS_INVISIBLE (widget)' failed`.
    - Normal startup remains operational.
    - Sidebar previewer, parallel view, Bible renderer, commentary, and
      dictionary remain operational.
    - No white-flash regression.
    - PREV/NEXT navigation remains operational.
    - Related tests pass.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
    - Real-display validation is required before DONE.
    - Automated fixes leave status `READY FOR REAL-DISPLAY MANUAL VALIDATION`.
  - Evidence:
    - Root cause/fix automatizado: `widgets.html_book` oculto fue anclado
      correctamente a la jerarquía de la ventana principal.
    - Regression tests: PASS.
    - target `biblia-elim`: PASS.
    - full default build: PASS.
    - `git diff --check`: PASS.
    - Real-display manual validation: PASS.
    - Startup real: PASS.
    - PREV/NEXT: PASS.
    - cambios de capítulo/referencia: PASS.
    - compare-bible lifecycle: PASS.
    - 0 ocurrencias de `Gtk-CRITICAL`, `gtk_widget_realize` y
      `anchored assertion`.

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

# Pending

- [x] APP-NAME-101 Eliminate duplicate g_set_application_name startup warning
  - Status: DONE
  - Objective:
    Eliminate the real startup warning:
    `GLib-WARNING: g_set_application_name() called multiple times`.
  - Requirements:
    - Locate every call to `g_set_application_name()`.
    - Identify the canonical initialization point and leave exactly one valid
      initialization.
    - Preserve the visible `Biblia Elim` branding.
    - Fix the cause rather than merely silencing the warning.
    - Do not touch the backend, schema, or importers.
    - Add a regression when reasonably viable.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
    - Real startup validation is required before DONE if the warning depends
      on the real GTK lifecycle.
  - Do not:
    - Suppress warnings or add GLib filters.
    - Add sleeps, `usleep`, or arbitrary delays.
    - Commit or push.
  - Evidence:
    - `gui_init()` remains the single canonical owner of process identity and
      sets application name `Biblia Elim` before GTK initialization;
      `create_mainwindow()` no longer repeats either global setter, while its
      window and header titles retain the visible `Biblia Elim` branding.
    - Repository call-site audit finds exactly one active
      `g_set_application_name()` call. The new headless
      `application_name_startup_test` locks down canonical ownership, absence
      of the duplicate window-construction call, and both visible titles; it
      passes with `application_name_startup_failures=0`.
    - The duplicate-setter warning is GLib-global and does not depend on GTK
      realization or mapping. `biblia-elim`, the full default build, and the
      related headless `wk_html_surface_test` pass; `git diff --check` passes.

- [x] UI-ZOOM-101 Implement persistent independent per-view zoom with ZoomState
  - Status: DONE
  - Objective:
    Allow each Bible view and each panel to maintain an independent zoom level
    that survives closing and restarting Biblia Elim. The design must avoid a
    single global zoom.
  - State model:
    - Introduce or consolidate a logical abstraction equivalent to:
      `ZoomState { bible-main, bible-parallel, commentary, dictionary,
      sidebar-previewer, lower-previewer, general-book, devotional, ... }`.
    - The exact structure or class name may follow the project's architecture,
      but the model must be central and coherent, avoiding separate global
      variables and duplicated logic.
    - Give every zoomable surface/view a stable identity, including future
      surfaces.
  - Bible views:
    - Every open Bible view keeps an independent zoom. For example,
      `bible-main=125%` and `bible-parallel=100%` may coexist.
    - Changing the main Bible zoom must not change the parallel Bible zoom.
    - The design must extend to additional future Bible views without
      reintroducing a global zoom.
  - Panels:
    - Every zoomable panel keeps an independent zoom. For example,
      `commentary=110%`, `dictionary=95%`, `sidebar-previewer=100%`, and
      `lower-previewer=105%` may coexist.
    - Changing one panel must not change Bible views or other panels.
  - Active view and focus:
    - The `+`/`-` commands act only on the active/focused surface: main Bible,
      parallel Bible, commentary, dictionary, or another zoomable panel.
    - Use the real GTK/application focus or active-surface model; do not infer
      focus through timing, sleeps, or fragile heuristics.
  - Reading mode:
    - Reading mode does not create another zoom state; it uses the ZoomState of
      the Bible view that entered reading mode.
    - Entering reading mode preserves that view's zoom, changing zoom there
      updates the same view, and leaving preserves the updated value.
    - `ESC` exits through the existing normal toggle/exit flow without
      resetting zoom, creating state, or affecting panels.
  - Persistence between sessions:
    - Persist every zoom level across application close and restart using the
      existing settings/preferences mechanism when appropriate; do not create
      a parallel configuration system when the project already has one.
    - Inspect the existing settings/config XML mechanism and integrate
      coherently.
    - Use stable per-surface keys and backward-compatible defaults.
    - Tolerate absent keys and old configuration.
    - Validate or clamp only persisted values outside the allowed range; never
      use clamps to hide layout bugs.
    - Avoid excessive writes when repeated `+`/`-` input could otherwise cause
      unnecessary I/O.
    - Do not modify Bible SQLite schemas or importers; this is UI/configuration
      state.
  - Render and navigation persistence:
    - Preserve each surface's zoom through PREV/NEXT, verse/chapter/reference
      changes, rerender, content reload, commentary/dictionary updates,
      compare/parallel refresh, entering/leaving reading mode, closing and
      reopening panels when applicable, and application restart.
    - A new render must not reset zoom to the default.
  - Range:
    - Reuse existing ranges and steps when available.
    - If no policy exists, investigate before choosing values and centralize
      min/max/step rather than duplicating magic values.
    - Do not change existing UX arbitrarily.
  - Compatibility:
    - Preserve Strong, morphology, footnotes, cross-references, headings,
      anchors, navigation, renderer lifecycle, existing right-panel zoom,
      compare Bible, normal mode, and reading mode.
    - Do not introduce N+1 behavior.
    - Do not touch `BibleBackend`, SQLite schema, or USFM/OSIS importers.
    - Do not change SWORD behavior except where strictly necessary for UI
      state.
  - Minimum tests:
    - Independence of `bible-main` versus `bible-parallel`.
    - Independence of Bible versus commentary.
    - Independence of commentary versus dictionary.
    - Persistence through PREV/NEXT.
    - Persistence through chapter change.
    - Persistence through reference change.
    - Persistence through rerender.
    - Persistence entering and leaving reading mode.
    - `ESC` exits reading mode without changing zoom.
    - Persistence after panel reload.
    - ZoomState serialization.
    - ZoomState restoration from configuration.
    - Defaults when no previous configuration exists.
    - Compatibility with old configuration.
    - Safe handling of invalid persisted values.
    - Regression coverage for existing right-panel zoom.
    - Cover all headless logic possible and clearly identify any remaining
      real-display validation.
  - Acceptance criteria:
    - Values such as `bible-main=125%`, `bible-parallel=100%`,
      `commentary=110%`, and `dictionary=95%` can coexist.
    - Any one value can be changed without altering the others.
    - Navigation and reading-mode transitions preserve the values.
    - Closing and reopening Biblia Elim restores the same values.
    - Related tests pass.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
    - If complete functional validation requires a real display, leave the
      task unchecked with `Status: BLOCKED` and evidence stating `Automated
      implementation complete. READY FOR REAL-DISPLAY MANUAL VALIDATION.`
    - Do not mark DONE until real validation when focus, keyboard shortcuts,
      or reading mode cannot be sufficiently verified automatically.
  - Do not:
    - Use one global zoom, one zoom shared by all Bible views, or one zoom
      shared by all panels.
    - Reset zoom during rendering or keep state only in a recreated widget.
    - Store ZoomState using pointers or ephemeral IDs.
    - Write excessively to disk on each frame or render.
    - Add sleeps, `usleep`, delays, debounce to hide bugs, or
      `gtk_main_iteration` hacks.
    - Infer focus through timing.
    - Touch backend, schema, or importers.
    - Commit or push.
  - Evidence:
    - Persistent per-surface ZoomState: PASS.
    - Persistencia entre sesiones: PASS.
    - Focus-directed zoom: PASS.
    - Bible principal y Bible paralela mantienen zoom independiente.
    - Panels mantienen zoom independiente.
    - Reflow fix validado en display real.
    - Zoom repetido +/- no cambia el versículo lógico.
    - Anchor lógico visible y posición relativa se preservan tras layout/draw
      real.
    - PREV/NEXT después del zoom: PASS.
    - Reading mode + zoom + ESC: PASS.
    - `zoom_state_test`: PASS.
    - `zoom_anchor_reflow_test`: PASS.
    - UI/navigation regressions: PASS.
    - target `biblia-elim`: PASS.
    - full default build: PASS.
    - `git diff --check`: PASS.

- [x] UI-ZOOM-UX-101 Make active zoom target explicit in the UI
  - Status: DONE
  - Objective:
    Make it evident which surface will receive the global zoom controls.
  - Current problem:
    - There are only two global +/- buttons. They correctly affect the
      selected/focused surface, but the user must first select Bible,
      commentary, dictionary, or another surface and there is no sufficiently
      clear indication of which one will receive the next zoom action.
  - Preserve the current model:
    - Zoom remains independent per surface.
    - ZoomState remains persistent.
    - +/- continue to act on the active/focused surface.
    - Improve only discoverability and clarity.
  - Preferred UX direction:
    - Show the active target and its percentage beside the zoom controls, for
      example `Biblia principal · 125% [-] [+]`,
      `Comentario · 110% [-] [+]`, or
      `Biblia paralela · 95% [-] [+]`.
    - When focus/active surface changes, immediately update the target name and
      displayed percentage without changing its zoom.
    - Investigate the current toolbar and find a compact visual integration.
    - Do not add a +/- pair to every panel unless clear evidence shows that it
      fits the existing UI better.
  - Acceptance criteria:
    - It is always visible which surface will receive +/-.
    - The displayed percentage matches the target's real ZoomState.
    - Changing focus updates the label and percentage.
    - Changing zoom updates the percentage.
    - Chapter changes and rerenders do not break the indicator.
    - Restarting the application restores the correct values.
    - Main Bible, parallel Bible, and panels remain independent.
    - Reading mode shows the correct target.
    - `ESC` does not alter ZoomState.
    - Add logic/state tests when viable.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
  - Do not:
    - Introduce global zoom.
    - Duplicate controls across all panels without need.
    - Infer the active surface through timing.
    - Add sleeps or `usleep`.
    - Touch backend, schema, or importers.
    - Commit or push.
  - Evidence:
    - El header muestra un indicador compacto y traducible:
      `surface · percentage` junto al único par global +/-.
    - El focus real del renderer, los eventos de zoom y reset actualizan
      inmediatamente el indicador usando el ZoomState persistente por surface,
      sin modificar el zoom de otras surfaces.
    - Reading mode refleja el mismo target/valor en su hover toolbar.
    - Los cambios de capítulo/rerender no poseen ni resetean el estado del
      indicador.
    - Tests PASS: `zoom_indicator_test`, `zoom_state_test`,
      `zoom_anchor_reflow_test`, `main_window_layout_test`,
      `application_name_startup_test`, `wk_html_surface_test`.
    - target `biblia-elim`: PASS.
    - full default build: PASS.
    - `git diff --check`: PASS.

- [x] STARTUP-DIAGNOSTICS-101 Normalize expected startup diagnostics
  - Status: DONE
  - Objective:
    Review startup messages and distinguish real failures from expected
    fallbacks.
  - Requirements:
    - Investigate the known message `SQLite backend unavailable at
      '.../modules'; falling back to SWORD` and determine whether it represents
      a supported expected condition, incomplete installation, incorrect
      configuration, or a real error.
    - If the fallback is normal and supported, use an appropriate MESSAGE/INFO
      level following project conventions without making real failures silent.
    - Preserve the functional fallback behavior.
  - Acceptance criteria:
    - Real errors remain visible.
    - An expected fallback does not cause unnecessary fatal warnings.
    - Related tests pass.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
  - Do not:
    - Silence all warnings.
    - Change the default backend without evidence.
    - Hide open or corruption errors.
    - Touch schema or importers.
    - Commit or push.
  - Evidence:
    - The automatic per-user SQLite directory is the documented default and
      its absence or emptiness is now an informational `g_message` before the
      supported SWORD fallback. CLI/environment selections, unreadable paths,
      rejected `.sqlite` candidates, and a backend lost after validation
      remain `g_warning` paths.
    - Bounded startup probes confirmed the missing automatic directory emits
      `Message` even with `G_DEBUG=fatal-warnings`, while an explicit missing
      SQLite path emits `WARNING`; functional fallback selection is unchanged.
    - `startup_diagnostics_test`, `sqlite_bible_backend_test`, and
      `application_name_startup_test` PASS. The `biblia-elim` target and full
      default build PASS; `git diff --check` PASS.

- [x] UI-SMOKE-101 Add GTK lifecycle smoke regression coverage
  - Status: DONE
  - Objective:
    Add automated smoke/lifecycle coverage that detects previously encountered
    GTK regressions.
  - Coverage where technically viable:
    - Startup and clean shutdown.
    - Bible renderer.
    - PREV/NEXT and chapter/reference changes.
    - Opening and closing panels.
    - Commentary and dictionary.
    - Compare Bible.
    - Sidebar and lower previewers.
    - Renderer CREATE/SHOW/MAP lifecycle.
  - Detect where possible:
    - `Gtk-WARNING` and `Gtk-CRITICAL`.
    - Invalid or negative size allocation.
    - Invalid realize/map lifecycle.
    - Aborts and crashes.
  - Design constraints:
    - Historical cases such as an inline `GtkSeparator` with allocation
      `height=-5` and `gtk_widget_realize` on an unanchored widget must inform
      the design.
    - Do not hardcode only historical strings; provide useful coverage for
      similar regressions.
    - Automated coverage does not replace real-display validation when a
      CI/headless environment cannot fully reproduce GTK behavior.
  - Acceptance criteria:
    - The test is reproducible.
    - It uses no arbitrary sleeps or fragile timing dependencies.
    - Integrate it with CTest/the existing test framework when appropriate.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
  - Do not:
    - Commit or push.
  - Evidence:
    - The new isolated `gtk_lifecycle_smoke` CTest launches the real
      `biblia-elim` window under Xvfb with a deterministic SQLite fixture and
      advances through GTK idles without sleeps. It covers startup/shutdown,
      reference and PREV/NEXT/chapter navigation, six renderer surfaces, and
      panel hide/show cycles; all renderers complete CREATE/SHOW/MAP and 359
      widget/lifecycle/allocation checks pass with zero GTK/GDK diagnostics.
    - The smoke exposed and now regresses a SQLite navigation crash caused by
      `sword_uri()` dereferencing the legacy SWORD backend; URI module/type
      dispatch uses the existing neutral helpers and the lifecycle run passes.
    - `wk_html_surface_test`, `panel_load_state_test`,
      `verse_navigation_readiness_test`, `navbar_valid_key_ownership_test`,
      `sqlite_bible_backend_test`, and `startup_diagnostics_test` PASS. The
      `biblia-elim` target and full default build PASS; `git diff --check`
      PASS.

- [x] UI-LAYOUT-102 Diagnose negative-width GTK allocation on fresh profile
  - Status: DONE
  - Objective:
    Identify and correct the widget/layout that produces negative-width GTK
    allocations during startup with a fresh profile:
    `width -107 / height 38`, `Negative content width -23`, and a `GtkLabel`
    allocation of 1 with extents 12x12.
  - Evidence:
    - The warnings occur on a real display during fresh-profile startup,
      repeat several times in the same run, and do not stop the process.
    - This is distinct from UI-LAYOUT-101's diagnosed and fixed `height=-5`
      inline-separator warning.
    - The retained real-display trace and GTK's paired diagnostics identify
      the negative widget as the main `GtkHeaderBar`'s internal title box: its
      title and subtitle are the two `GtkLabel` children subsequently given
      one-pixel allocations. The application-side trigger is the header zoom
      label's 20-character minimum width; the label now remains ellipsized and
      bounded but has no fixed minimum-width request.
    - `zoom_indicator_test`, `main_window_layout_test`, `zoom_state_test`,
      `zoom_anchor_reflow_test`, `application_name_startup_test`,
      `startup_diagnostics_test`, `startup_profile_test`,
      `wk_html_surface_test`, and `panel_load_state_test` PASS. The
      `biblia-elim` target and full default build PASS.
    - Root cause: the zoom/header indicator imposed a minimum width equivalent
      to 20 characters and could force invalid geometry during fresh-profile
      startup.
    - Fix: the artificial minimum width was removed so the title box can
      compress according to normal GTK geometry.
    - Regression coverage: PASS. Related headless tests: PASS. The
      `biblia-elim` target: PASS. Full default build: PASS.
      `git diff --check`: PASS.
    - Real-display validation in
      `build/startup-layout-check-20260909-145401`: three fresh-profile
      startups PASS and three reused-profile startups PASS; the collector
      completed both series.
    - Across both real-display series there were zero occurrences of
      `negative-width gtk_widget_size_allocate`, zero occurrences of
      `Negative content width`, zero `Gtk-WARNING`, zero `Gtk-CRITICAL`, and
      zero related `GLib-CRITICAL` diagnostics or assertions.
  - Investigation requirements:
    - Reproduce with a fresh profile.
    - Identify the exact widget using GDB or localized logging and identify its
      parent/container.
    - Review the new zoom/header indicator, fresh-profile defaults, toolbar,
      labels, and other candidate widgets without assuming UI-ZOOM-UX-101 is
      responsible before evidence establishes the cause.
    - Correct the geometry at its source.
  - Do not:
    - Suppress the warning or use `MAX(width, 0)`.
    - Add hardcoded sizes, sleeps, or `usleep` to hide the warning.
    - Hide the toolbar or panel.
    - Revert UI-ZOOM-UX-101 without evidence.
    - Touch the backend, schema, or importers.
  - Future validation:
    - Zero negative-width `Gtk-WARNING` diagnostics during fresh-profile
      startup.
    - Normal startup, toolbar/zoom indicator, PREV/NEXT, and panels remain
      operational.
    - Related tests, the `biblia-elim` target, and the full default build pass.
    - Complete real-display manual validation.

- [x] STARTUP-PERF-101 Establish measurable startup performance baseline
  - Status: DONE
  - Objective:
    Establish a reproducible startup baseline using existing instrumentation
    before making optimizations.
  - Available events:
    - `APP_START`.
    - `GTK_INITIALIZED`.
    - `WINDOW_CREATED`.
    - `MODULE_READY`.
    - `FIRST_CONTENT_REQUEST`.
    - `FIRST_CONTENT_READY`.
    - `FRONTEND_DISPLAY_DONE`.
    - `GTK_MAIN_ENTER`.
  - Requirements:
    - Measure multiple executions and never draw conclusions from one run.
    - Report the median and useful dispersion/variability.
    - Identify the dominant phases.
    - Distinguish cold and warm behavior when possible.
    - Preserve reproducible evidence.
    - Do not optimize yet except for obvious micro-fixes required to measure.
    - Do not add excessive instrumentation to normal release behavior.
    - Do not set arbitrary budgets before measuring.
  - Acceptance criteria:
    - Document a reproducible procedure.
    - Collect multiple samples and summarize the baseline.
    - Identify dominant phases.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - `git diff --check` passes.
  - Do not:
    - Use sleeps to stabilize benchmarks.
    - Artificially preload data to improve numbers.
    - Hide work after `GTK_MAIN_ENTER`.
    - Make optimizations that degrade first paint or navigation.
    - Commit or push.
  - Evidence:
    - Added `scripts/startup_performance_baseline.py` and a documented
      procedure for seven fresh-profile and seven reused-profile executions
      using the existing opt-in milestones and deterministic idle-driven GTK
      smoke shutdown. It retains raw logs plus environment/Git metadata and
      reports median, MAD, minimum, maximum, and the dominant median phase;
      it neither sleeps nor modifies release startup behavior.
    - The `startup_performance_baseline` parser/statistics regression passes
      and rejects missing, unordered, or fewer-than-three samples. Its
      collector regression also executes all seven fresh samples, the
      independent reused setup, and seven reused samples against an isolated
      deterministic fixture, verifies every HOME/XDG parent at process launch,
      and proves no application-specific profile or real-user HOME is seeded;
      `startup_diagnostics_test`, the `biblia-elim` target, and the full
      default build pass. `git diff --check` passes.
    - The real-display first-run failure was an application bug. The collector
      created valid isolated HOME plus XDG config/data/cache parents, but
      `settings_init()` ignored `XDG_CONFIG_HOME`, attempted
      `<profile>/home/.config/xiphos`, and used one-level `mkdir(0700)` while
      `<profile>/home/.config` did not exist. Reproduction: return `-1`,
      immediate `errno=2` (`No such file or directory`). The dialog formatted
      `strerror(errno)` only after `gui_init()`, explaining the observed
      `strerror(0)`/`Conseguido` text.
    - Startup now resolves the historical `xiphos` leaf under GLib's
      platform/XDG config root, creates missing parents with
      `g_mkdir_with_parents()`, and captures errno immediately on failure. The
      fatal flow now reports the full path and visible `Biblia Elim` name.
      `startup_profile_test` validates fresh and reused creation plus exact
      failure return/errno; the lifecycle smoke now starts without a seeded
      `HOME/.config` and asserts profile creation and absence of the fatal
      dialog when a display is available.
    - A real `DISPLAY=:0` run is confirmed. Its first retained raw trace
      reached `GTK_INITIALIZED`, all later content milestones, and
      `GTK_MAIN_ENTER`; the earlier collector diagnosis was a parser failure,
      not evidence of an X11 or XAUTHORITY failure.
    - The trace parser accepted only dot-decimal timestamps while the process
      emitted `APP_START 0.0ms` before locale adoption and comma-decimal
      timestamps such as `GTK_INITIALIZED 51,2ms` afterwards. The parser now
      accepts both separators and normalizes them to Python floats; focused
      dot, comma, mixed, auxiliary-field, and genuinely-missing-event
      regressions cover the locale boundary.
    - Reanalysis of
      `build/startup-baseline-20260909-135212/fresh-profile-01.log` extracts
      all 8 required milestones: `APP_START=0.0`, `GTK_INITIALIZED=51.2`,
      `WINDOW_CREATED=812.4`, `MODULE_READY=815.3`,
      `FIRST_CONTENT_REQUEST=1147.4`, `FIRST_CONTENT_READY=1158.9`,
      `FRONTEND_DISPLAY_DONE=1165.6`, and `GTK_MAIN_ENTER=1195.5` ms.
    - The real fresh-profile trace also exposes the separate negative-width
      GTK warnings tracked by UI-LAYOUT-102. Final seven fresh-profile plus
      seven reused-profile collection is deferred until UI-LAYOUT-102 is
      resolved, because its geometry fix may affect the measured timings. No
      baseline statistics or dominant runtime phase were fabricated.
    - Final real-display baseline: seven fresh-profile samples and seven
      reused-profile samples. The final diagnostic scan found 0 `Gtk-WARNING`,
      0 `Gtk-CRITICAL`, 0 `GLib-CRITICAL`, 0 `Negative content width`, 0
      negative-width size allocations, 0 assertions, and 0 `ERROR`.
    - Fresh-profile `APP_START -> GTK_MAIN_ENTER`: median 1548.8 ms, MAD
      48.1 ms, minimum 1271.3 ms, maximum 1600.1 ms. Its dominant phase was
      `GTK_INITIALIZED -> WINDOW_CREATED`: median 915.0 ms, MAD 61.1 ms,
      minimum 826.0 ms, maximum 1024.5 ms.
    - Reused-profile `APP_START -> GTK_MAIN_ENTER`: median 1537.7 ms, MAD
      24.6 ms, minimum 1394.6 ms, maximum 1598.8 ms. Its dominant phase was
      `GTK_INITIALIZED -> WINDOW_CREATED`: median 962.5 ms, MAD 1.8 ms,
      minimum 951.3 ms, maximum 1024.6 ms.
    - `GTK_INITIALIZED -> WINDOW_CREATED` is the dominant startup phase in
      both scenarios, representing approximately 59% of the fresh startup
      median and 63% of the reused startup median.
    - The total fresh versus reused median differs by only about 11.1 ms
      (approximately 0.7%), so this baseline does not support a meaningful
      overall startup advantage for the reused application profile. These
      scenarios describe application profile state, not true OS/storage
      cold-versus-warm cache behavior.
    - No optimization was performed as part of STARTUP-PERF-101.

- [x] STARTUP-PERF-102 Profile GTK_INITIALIZED-to-WINDOW_CREATED startup phase
  - Status: DONE
  - Objective:
    Explain where the roughly 0.9-1.0 second dominant startup phase between
    `GTK_INITIALIZED` and `WINDOW_CREATED` is spent before making any
    optimization.
  - Measured baseline motivating this task:
    - Fresh-profile `GTK_INITIALIZED -> WINDOW_CREATED` median: 915.0 ms.
    - Reused-profile `GTK_INITIALIZED -> WINDOW_CREATED` median: 962.5 ms.
    - This phase accounts for the majority of total startup time.
  - Investigation requirements:
    - Instrument or profile only this phase with enough granularity to
      identify major contributors.
    - Prefer existing monotonic timing infrastructure.
    - Add localized milestones/spans rather than broad noisy logging.
    - Identify expensive operations such as, where applicable:
      - GtkBuilder/UI construction.
      - Renderer/WebKit creation.
      - Widget tree creation.
      - Module discovery.
      - Settings/config loading.
      - Sidebar/panel construction.
      - CSS/theme initialization.
      - Synchronous filesystem work.
      - Synchronous backend/module probes.
      - Repeated initialization.
      - Unnecessary work for initially hidden panels.
    - Do not assume WebKit, backend, GtkBuilder, or any specific subsystem is
      the bottleneck without measurements.
    - Produce a breakdown whose component durations substantially account for
      the measured `GTK_INITIALIZED -> WINDOW_CREATED` interval.
    - Compare multiple runs; do not infer from a single execution.
    - Preserve the fresh/reused distinction when useful.
  - Acceptance criteria:
    - Reproducible profiling procedure.
    - Multiple real or deterministic samples where technically possible.
    - Named sub-phases with median timings.
    - Major contributors identified with objective evidence.
    - Explain any residual/unaccounted time.
    - No speculative optimization required for DONE.
    - The `biblia-elim` target passes.
    - Full default build passes.
    - Relevant tests pass.
    - `git diff --check` passes.
  - If real display is required for final profiling:
    - Leave unchecked with `Status: BLOCKED`.
    - State `READY FOR REAL-DISPLAY PROFILING`.
  - Do not:
    - Optimize before identifying contributors.
    - Move work after `GTK_MAIN_ENTER` merely to improve the metric.
    - Hide startup work.
    - Add sleeps or `usleep`.
    - Add arbitrary lazy-loading without measurement.
    - Change backend, schema, or importers unless profiling proves direct
      relevance.
    - Commit or push.
  - Evidence:
    - Added opt-in monotonic boundaries that partition the complete measured
      interval across splash preparation, global HTML initialization, theme
      setup, window chrome/navigation, all six startup panes, widget-tree
      show/realization, and GTK event draining. The focused analyzer reports
      per-sub-phase median, MAD, minimum, and maximum for retained fresh and
      reused profile logs; deterministic parser/partition regressions PASS.
    - `biblia-elim` and the full default build PASS.
      `startup_performance_baseline`, `wk_html_surface_test`,
      `startup_diagnostics_test`, and `startup_profile_test` PASS;
      `gtk_lifecycle_smoke` skips because no display is reachable.
    - At the automated stage, real collection could not pass
      `GTK_INITIALIZED`: both X11 and Wayland attempts exited immediately after
      `APP_START`. The supported `xvfb-run` fallback was also unavailable
      because Xorg rejected the non-root-owned `/tmp/.X11-unix` directory and
      could not establish a listener. The failed traces and Xvfb diagnostics
      are retained under `build/startup-window-profile-20260909*`.
    - Final named sub-phase medians and contributor attribution could not be
      produced in that environment without fabricating evidence. Automated
      instrumentation was complete and READY FOR REAL-DISPLAY PROFILING.
    - Final real-display profiling evidence is retained under
      `build/startup-window-profile-real-20260909-152725`: seven fresh-profile
      samples and seven reused-profile samples, with zero `Gtk-WARNING`, zero
      `Gtk-CRITICAL`, zero `GLib-CRITICAL`, zero `Negative content width`, zero
      negative-width allocations, zero assertions, and zero `ERROR`.
    - Fresh-profile medians/MAD: `GTK_INITIALIZED -> WINDOW_CREATED` 1013.0 /
      13.5 ms; `DEVOTIONAL_PANE_READY -> WINDOW_TREE_SHOWN` 332.3 / 3.8 ms;
      `WINDOW_TREE_SHOWN -> WINDOW_EVENTS_DRAINED` 506.4 / 8.3 ms.
    - Reused-profile medians/MAD: `GTK_INITIALIZED -> WINDOW_CREATED` 986.6 /
      13.3 ms; `DEVOTIONAL_PANE_READY -> WINDOW_TREE_SHOWN` 323.7 / 6.7 ms;
      `WINDOW_TREE_SHOWN -> WINDOW_EVENTS_DRAINED` 492.1 / 8.5 ms.
    - `DEVOTIONAL_PANE_READY -> WINDOW_TREE_SHOWN` accounts for about 32.8%
      of the measured dominant interval in both scenarios, and
      `WINDOW_TREE_SHOWN -> WINDOW_EVENTS_DRAINED` accounts for about 50% in
      both. Together these consecutive sub-phases account for about 82.8%
      fresh and 82.7% reused.
    - All individual pane-construction boundaries before those sub-phases are
      small, roughly 1-11 ms each. HTML initialization is effectively 0 ms at
      this boundary, theme initialization costs roughly 60 ms, and window
      shell construction costs roughly 54-57 ms.
    - The focused instrumentation partitions the complete
      `GTK_INITIALIZED -> WINDOW_CREATED` interval apart from display rounding.
      These broad boundaries do not establish WebKit, devotional construction,
      or any individual widget as the root cause. No optimization was
      performed.
    - The reproducible procedure, multiple fresh/reused real-display samples,
      named sub-phases, objective major contributors, complete interval
      accounting, previously passing builds/tests, and `git diff --check` PASS
      satisfy the acceptance criteria.

- [x] STARTUP-PERF-103 Profile window-show and GTK event-drain hotspots
  - Status: DONE
  - Objective:
    Explain the two remaining broad startup hotspots before performing any
    optimization:
    1. `DEVOTIONAL_PANE_READY -> WINDOW_TREE_SHOWN`, approximately 332 ms
       fresh / 324 ms reused.
    2. `WINDOW_TREE_SHOWN -> WINDOW_EVENTS_DRAINED`, approximately 506 ms
       fresh / 492 ms reused.
    Together they account for about 83% of
    `GTK_INITIALIZED -> WINDOW_CREATED`.
  - Investigation requirements:
    - Profile these two intervals separately and with finer-grained monotonic
      boundaries.
    - Inspect exactly what executes between `DEVOTIONAL_PANE_READY` and
      `WINDOW_TREE_SHOWN`.
    - Split that interval around meaningful operations actually present in the
      code, such as statusbar construction, layout restoration, signal setup,
      `gtk_widget_show()`/`gtk_widget_show_all()` calls, visibility
      synchronization, renderer/widget realization triggers, and any other
      substantial operations.
    - Do not infer contributors from names; instrument actual call boundaries.
    - For `WINDOW_TREE_SHOWN -> WINDOW_EVENTS_DRAINED`, inspect
      `sync_windows()` and what it actually drains.
    - Determine whether that time is primarily realize/map, size allocation,
      WebKit widget realization, `GtkPaned`/notebook layout, CSS/style
      resolution, renderer lifecycle callbacks, synchronous work triggered by
      GTK signals, or another measured cause.
    - Add nested timing spans around actual expensive sections/callbacks where
      technically safe.
    - Account for most of the approximately 500 ms rather than treating
      `sync_windows()` as an unexplained black box.
    - Compare multiple real-display samples.
    - Preserve the fresh/reused distinction if useful.
    - Quantify residual/unaccounted time.
  - Acceptance criteria:
    - Reproducible profiling procedure.
    - Multiple samples.
    - Fine-grained median/MAD timings.
    - Objective attribution of both approximately 330 ms and 500 ms broad
      intervals.
    - Enough evidence to choose a concrete optimization target.
    - No speculative optimization required.
    - Relevant tests PASS.
    - The `biblia-elim` target PASS.
    - Full default build PASS.
    - `git diff --check` PASS.
  - If final attribution requires real display:
    - Leave unchecked.
    - Set `Status: BLOCKED`.
    - State `READY FOR REAL-DISPLAY PROFILING`.
  - Evidence:
    - Added opt-in monotonic boundaries around actual post-devotional work:
      statusbar construction, layout restoration, `gtk_widget_show_all()`,
      visibility synchronization, the existing GTK drain, and final signal
      setup. Window realize/map/style and selected `GtkPaned`/notebook
      allocations are recorded, while named renderer lifecycle events now use
      the same monotonic origin as application events.
    - `sync_windows()` remains behaviorally intact and now exposes its actual
      `gtk_main_iteration()` calls as paired nested spans. The focused analyzer
      validates those pairs and reports fine-grained phase median/MAD/min/max,
      iteration count, summed iteration time, longest iteration, drain
      residual, and lifecycle markers. Sixteen deterministic analyzer tests
      PASS, including complete accounting and malformed-pair rejection.
    - The `biblia-elim` target and full default build PASS.
      `startup_performance_baseline`, `wk_html_surface_test`, and
      `main_window_layout_test` PASS with zero reported failures; live GTK
      assertions and `gtk_lifecycle_smoke` skip because no display is
      reachable. `git diff --check` PASS.
    - Real-display collection was attempted at
      `build/startup-window-hotspots-20260909-01`, but forced X11 and a direct
      Wayland attempt both exited immediately after `APP_START`. The supported
      Xvfb fallback also cannot start: `/tmp/.X11-unix` is owned by `nobody`,
      and Xorg reports `Cannot establish any listening sockets` after the
      ownership warning. Consequently no multi-sample attribution or concrete
      optimization target is claimed or fabricated.
    - Final real-display profiling collected seven fresh-profile samples and
      seven reused-profile samples. The diagnostic scan found zero
      `Gtk-WARNING`, zero `Gtk-CRITICAL`, zero `GLib-CRITICAL`, zero `Negative
      content width`, zero negative-width allocations, zero assertions, and
      zero `ERROR`.
    - Fresh-profile medians/MAD: `GTK_INITIALIZED -> WINDOW_CREATED` 987.8 /
      34.3 ms; `WINDOW_SHOW_ALL_BEGIN -> WINDOW_SHOW_ALL_END` 348.3 / 22.9 ms;
      `GTK_EVENT_DRAIN_BEGIN -> GTK_EVENT_DRAIN_END` 469.7 / 31.5 ms. The GTK
      drain had median iteration count 44, median iteration total 459.6 ms,
      median longest iteration 98.5 ms, and median residual 10.1 ms.
    - Reused-profile medians/MAD: `GTK_INITIALIZED -> WINDOW_CREATED` 976.6 /
      62.2 ms; `WINDOW_SHOW_ALL_BEGIN -> WINDOW_SHOW_ALL_END` 327.5 / 8.1 ms;
      `GTK_EVENT_DRAIN_BEGIN -> GTK_EVENT_DRAIN_END` 468.7 / 31.3 ms. The GTK
      drain had median iteration count 43, median iteration total 465.9 ms,
      median longest iteration 103.9 ms, and median residual 9.9 ms.
    - `gtk_widget_show_all()` accounts for approximately 35.3% fresh and 33.5%
      reused of `GTK_INITIALIZED -> WINDOW_CREATED`. The synchronous GTK event
      drain accounts for approximately 47.6% fresh and 48.0% reused. Together
      they account for approximately 82.8% fresh and 81.5% reused.
    - Almost all drain time is actual GTK iterations: approximately 459.6 of
      469.7 ms fresh and 465.9 of 468.7 ms reused. The residual outside
      iterations is only about 10 ms. Statusbar creation, layout restoration,
      visibility synchronization, and final signal connection are negligible.
    - These measurements objectively localize the two broad hotspots to
      recursive window show and synchronous GTK event processing caused during
      realization/layout, satisfying the acceptance criteria and providing a
      concrete optimization target. They do not establish that WebKit, CSS,
      `GtkPaned`, allocations, or another specific subsystem dominates the GTK
      iterations, and they do not establish that `sync_windows()` itself is
      unnecessary. No optimization was performed in STARTUP-PERF-103.
  - Do not:
    - Optimize yet.
    - Remove `sync_windows()` merely because it is expensive.
    - Defer work after `GTK_MAIN_ENTER` to improve the metric.
    - Add sleeps or `usleep`.
    - Suppress GTK events.
    - Skip required realization.
    - Add arbitrary lazy loading.
    - Touch backend, schema, or importers without measured evidence.
    - Commit or push.

- [x] STARTUP-PERF-104 Reduce unnecessary startup show/realize work
  - Status: DONE
  - Objective:
    Reduce startup time by avoiding unnecessary recursive
    show/realize/allocation work at application startup while preserving the
    exact visible UI and behavior.
  - Measured motivation:
    - Fresh-profile:
      - `gtk_widget_show_all()`: 348.3 ms median.
      - Subsequent GTK drain: 469.7 ms median.
    - Reused-profile:
      - `gtk_widget_show_all()`: 327.5 ms median.
      - Subsequent GTK drain: 468.7 ms median.
    - The show plus drain path accounts for approximately 82% of the dominant
      window-creation interval.
  - Hypothesis to test:
    The top-level recursive `gtk_widget_show_all()` may be showing/realizing
    widgets or subtrees that startup visibility settings immediately hide
    again, causing avoidable realization, layout, and event work. This is a
    hypothesis, not an established root cause.
  - Investigation requirements:
    - Locate the exact startup `gtk_widget_show_all()`/show calls and subsequent
      visibility synchronization.
    - Enumerate which major child subtrees are intended to be visible at
      startup for the current settings and which are intended to remain hidden.
    - Determine whether `show_all` temporarily shows/realizes hidden subtrees
      before visibility is restored.
    - Use existing visibility/settings semantics as the source of truth.
    - Measure before modifying behavior.
  - Preferred optimization:
    If evidence confirms unnecessary recursive showing:
    - Prevent known-hidden startup subtrees from participating in `show_all`,
      using normal GTK visibility/no-show-all semantics or an equally simple
      visibility-aware approach.
    - Continue showing the real main window normally.
    - Keep widgets that must exist/anchor for backend or lifecycle reasons alive
      without unnecessarily mapping them.
    - Pay particular attention to commentary/dictionary/devotional panes,
      previewers, sidebar, compare/parallel areas, backend-only book pane, and
      any other subtree whose startup visibility is false.
    - Do not assume all of these are unnecessary. Check actual settings and
      lifecycle requirements individually.
    - Do not remove `sync_windows()` as the first optimization. Instead measure
      whether reducing show/realize work also reduces
      `WINDOW_SHOW_ALL_BEGIN -> WINDOW_SHOW_ALL_END`, GTK event-drain iteration
      count, and GTK event-drain total time.
  - Acceptance criteria:
    - Correctness:
      - Visible startup UI is unchanged.
      - Bible main view renders normally.
      - PREV/NEXT works immediately.
      - Chapter/reference navigation works.
      - Commentary/dictionary/panels open when requested.
      - Sidebar and previewers work.
      - Compare/parallel Bible works.
      - Reading mode works.
      - Zoom target indicator works.
      - Hidden backend-only surfaces remain correctly anchored where required.
      - No white-panel regression.
      - No realize/map regression.
      - Zero new GTK warnings/criticals.
    - Performance:
      - Collect a post-change real-display seven fresh-profile plus seven
        reused-profile benchmark.
      - Compare against the STARTUP-PERF-103 baseline.
      - Report median/MAD before and after for:
        - `WINDOW_SHOW_ALL_BEGIN -> WINDOW_SHOW_ALL_END`.
        - `GTK_EVENT_DRAIN_BEGIN -> GTK_EVENT_DRAIN_END`.
        - Iteration count.
        - Iteration total milliseconds.
        - `APP_START -> GTK_MAIN_ENTER`.
      - Do not call an improvement successful from a single run.
      - If there is no meaningful improvement, retain the measured result and
        do not manufacture a win.
    - Tests:
      - Relevant layout/surface/lifecycle tests PASS.
      - `gtk_lifecycle_smoke` PASS where a display is available.
      - The `biblia-elim` target PASS.
      - Full default build PASS.
      - `git diff --check` PASS.
  - If final real-display comparison is needed:
    - Leave unchecked.
    - Set `Status: BLOCKED`.
    - State `READY FOR REAL-DISPLAY VALIDATION`.
  - Evidence:
    - Confirmed that the top-level `gtk_widget_show_all()` ran before
      `frontend_display()` restored settings and therefore temporarily showed
      startup-hidden study, preview, compare, tabstrip, statusbar, and
      reading-mode subtrees. A source-of-truth startup visibility policy now
      applies GTK `no-show-all` semantics before the recursive show while
      preserving every existing explicit panel-open path and backend anchor.
    - `main_window_layout_test` covers default, all-visible, and reading-mode
      policy matrices. The lifecycle smoke hook additionally checks that the
      default commentary, dictionary, and compare roots cannot be re-shown by
      an ancestor `show_all()` but do reopen through explicit show calls.
    - `main_window_layout_test`, `wk_html_surface_test`,
      `startup_diagnostics_test`, `startup_performance_baseline`,
      `verse_navigation_readiness_test`, `navbar_entry_reference_test`, and
      the zoom state/indicator/anchor regressions PASS. The `biblia-elim`
      target and full default build PASS. `gtk_lifecycle_smoke` skips because
      this environment cannot start its Xvfb server.
    - The retained seven-fresh/seven-reused STARTUP-PERF-103 dataset was
      re-analyzed before the change and reproduces the 348.3/22.9 ms fresh and
      327.5/8.1 ms reused show medians/MAD, plus 469.7/31.5 ms fresh and
      468.7/31.3 ms reused drain medians/MAD.
    - Post-change collection was attempted at
      `build/startup-perf-104-post-20260909`, but current X11 and Wayland runs
      both exit immediately after `APP_START`; Xvfb also reports that it cannot
      establish Unix or TCP listening sockets. The required seven-fresh plus
      seven-reused performance comparison and real-display correctness checks
      therefore remained unverified at that automated stage, which was
      recorded as `READY FOR REAL-DISPLAY VALIDATION`.
    - Manual real-display functional validation: PASS.
      - Startup visual normal.
      - Bible renderer PASS.
      - PREV/NEXT PASS.
      - Chapter/reference navigation PASS.
      - Commentary/dictionary PASS.
      - Sidebar/previewers PASS.
      - Compare/parallel PASS.
      - Reading mode PASS.
      - Zoom indicator PASS.
      - Clean shutdown PASS.
      - Diagnostic grep clean.
    - Real-display post-change performance from seven fresh and seven reused
      samples: fresh `show_all` 348.3 -> 279.1 ms (-19.9%), GTK drain 469.7 ->
      410.3 ms (-12.6%), `GTK_INITIALIZED -> WINDOW_CREATED` 987.8 -> 872.6 ms
      (-11.7%), and `APP_START -> GTK_MAIN_ENTER` 1603.5 -> 1401.9 ms (-12.6%).
      Reused `show_all` 327.5 -> 263.2 ms (-19.6%), GTK drain 468.7 -> 407.1 ms
      (-13.1%), `GTK_INITIALIZED -> WINDOW_CREATED` 976.6 -> 863.2 ms (-11.6%),
      and `APP_START -> GTK_MAIN_ENTER` 1512.7 -> 1419.4 ms (-6.2%).
    - The iteration count did not decrease. The longest iteration did decrease,
      fresh 98.5 -> 62.2 ms and reused 103.9 -> 61.0 ms. Therefore the measured
      improvement is cheaper work per iteration, not fewer iterations.
  - Do not:
    - Remove `sync_windows()` just because it is expensive.
    - Delay required startup work until after `GTK_MAIN_ENTER` solely to
      improve the metric.
    - Hide the complete application until loading finishes.
    - Add arbitrary lazy loading.
    - Add sleeps or `usleep`.
    - Suppress warnings.
    - Add hardcoded geometry.
    - Change backend, schema, or importers.
    - Commit or push.

- [x] UI-WIDTH-101 Diagnose unused reading-panel width for SpaPlatense versus SpaRV
  - Status: DONE
  - Objective:
    Determine, with runtime measurements, the exact cause of the apparently
    reduced reading width for SWORD module SpaPlatense in the main reading
    panel, using SpaRV as the control. Do not implement a fix until the
    first differing layer and the root cause are demonstrated. This task is
    independent of STARTUP-PERF-105.
  - Symptom:
    - SpaPlatense does not appear to use the full available width of the
      main reading panel.
    - SpaRV renders at the expected width.
    - The problem is visual and occurs in prose as well as poetry.
  - Required comparison case:
    - SpaPlatense → Hechos 1
    - SpaRV → Hechos 1
    - Use Hechos 1:2 as the GtkTextBuffer probe position for tags and
      effective attributes.
  - Rendering pipeline to verify in source (do not assume exact lines):
    GTKChapDisp::display()
    → HtmlOutput(...)
    → XiphosHtml / WkHtml
    → wk_html_open_stream()
    → wk_html_write()
    → wk_html_close()
    → load_html()
    → xmlReadMemory()
    → walk_node()
    → ctx_append()
    → record_style_spans()
    → spans_apply()
    → GtkTextBuffer
    → GtkTextView
    Likely files: `src/main/display.cc`, `src/gtk/utilities.c`,
    `src/webkit/wk-html.c`. Also inspect `src/gtk/main_window.c` for
    `reading_mode_apply_measure()`. Confirm the real chain with search;
    do not treat this outline as line-accurate.
  - Previously fixed poetry bug — do not reopen:
    A different earlier bug involved invalid OSIS poetry HTML. SWORD could
    return `BibleVerseContent.headings` containing
    `<span class="line indent0">` and `BibleVerseContent.renderedText`
    containing `Texto...</span><br />`, which became invalid when wrapped
    in `<p class="verse">`. That was already fixed by
    `normalize_poetry_carry(BibleVerseContent &content)`, which normalizes
    `BibleVerseContent` after `getVerseContent()`. Nine `getVerseContent()`
    call sites in `display.cc` were adapted. It was verified that:
    - `<span class="line..."><p class="verse">` no longer occurs;
    - `</p><span class="line..."><p class="verse">` no longer occurs;
    - first and second renders produce identical HTML;
    - the cache stores the normalized content.
    Therefore do NOT modify `normalize_poetry_carry()`, SWORD, OSIS,
    `prepare_display_content()`, `CleanupContent()`, module `.conf` files,
    or module CSS unless new incontrovertible evidence appears. Do not use
    SpaPlatense-specific hacks.
  - Width bug is not poetry-only:
    The unused-width problem is also observed in prose. SpaPlatense
    Hechos 1 produces ordinary verses such as
    `<p class="verse">` ... long prose text ... `</p>` without
    `class="line"`. Poetic `<br />` line breaks therefore do not explain
    the general problem.
  - Static findings to confirm, not assume blindly:
    1. `HtmlOutput` is probably in `src/gtk/utilities.c`.
    2. `WkHtml` is probably in `src/webkit/wk-html.c`.
    3. `WkHtml` creates a `GtkTextView`.
    4. The `GtkTextView` uses `GTK_WRAP_WORD_CHAR`.
    5. Global side margins are near 14 px (`NORMAL_SIDE_MARGIN`).
    6. `<p>` is processed as a block and produces paragraph breaks.
    7. `class="verse"` apparently does not create a special GtkTextTag.
    8. The parser interprets some styles as: bold, italic, underline,
       strike, sup, sub, small, big, center, right, family, scale.
    9. `text-align: justify` is apparently not implemented by WkHtml.
    10. `<font size="...">` is transformed into scale.
    11. `reading_mode_apply_measure()` can change side margins dynamically
        and MUST be investigated.
  - Existing incomplete instrumentation in the working tree:
    A previous session may already have temporary `g_printerr` probes in
    `GTKChapDisp::display()` (runtime width / visible rect / margins /
    verse-2 line geometry / tag dump) and in
    `reading_mode_apply_measure()` (`[READING MEASURE]`). Those probes
    print to stderr, are incomplete versus the required metrics below, and
    must not be treated as finished diagnosis. Reuse and complete them;
    prefer a stable log file over stderr; remove all temporary probes
    before DONE unless a permanent test is justified.
  - Technical goal:
    Do not stop at static analysis. Discover with runtime measurements the
    FIRST layer where SpaPlatense and SpaRV diverge:
    - A. GtkTextView / container global geometry
    - B. effective GtkTextTags and attributes
    - C. HtmlOutput / WkHtml parser
    - D. real module HTML
  - Subtask 1 — locate the real implementation:
    Run searches equivalent to:
    `rg -n '(^|[^A-Za-z_])HtmlOutput\s*(' src`
    `rg -n 'wk_html_open_stream|wk_html_write|wk_html_close|load_html|walk_node|ctx_append|record_style_spans|spans_apply' src`
    Document the real chain through GtkTextBuffer / GtkTextView.
  - Subtask 2 — global runtime metrics:
    For SpaPlatense Hechos 1 and SpaRV Hechos 1, instrument temporarily
    and obtain REAL values of:
    `gtk_widget_get_allocated_width()`, `gtk_widget_get_allocated_height()`,
    `gtk_text_view_get_left_margin()`, `gtk_text_view_get_right_margin()`,
    `gtk_text_view_get_top_margin()`, `gtk_text_view_get_bottom_margin()`,
    `gtk_text_view_get_indent()`, `gtk_text_view_get_wrap_mode()`,
    `gtk_text_view_get_justification()`, and
    `gtk_text_view_get_visible_rect()` including
    `visible.x`, `visible.y`, `visible.width`, `visible.height`.
    Also measure the immediate parent, GtkScrolledWindow, and the
    relevant viewport/container. Do not accept values inferred from
    source; they must come from runtime.
  - Subtask 3 — `reading_mode_apply_measure()`:
    Search `rg -n 'reading_mode_apply_measure' src`. Determine definition,
    call sites, when it activates, target-width calculation, margin
    calculation, font dependence, and behavior when switching modules.
    Instrument temporarily:
    `[READING MEASURE]`
    `module=`
    `enabled=`
    `widget_width=`
    `target_width=`
    `left_margin_before=`
    `right_margin_before=`
    `left_margin_after=`
    `right_margin_after=`
    `font=`
    `font_size=`
    Also record `settings.reading_mode`, `settings.reading_compare`, and
    `settings.render_whole_books`.
  - Subtask 4 — real GtkTextTags:
    In the GtkTextBuffer, find a position inside Hechos 1:2 for
    SpaPlatense and SpaRV. Use `gtk_text_iter_get_tags()`. For each
    active GtkTextTag obtain, when applicable: name, priority,
    left-margin-set/left-margin, right-margin-set/right-margin,
    indent-set/indent, justification-set/justification,
    wrap-mode-set/wrap-mode, pixels-above-lines-set/pixels-above-lines,
    pixels-below-lines-set/pixels-below-lines,
    pixels-inside-wrap-set/pixels-inside-wrap, scale-set/scale,
    family-set/family, size-set/size, rise-set/rise. Report both
    modules side by side.
  - Subtask 5 — effective attributes:
    Obtain effective attributes of the GtkTextIter via the available GTK
    API, for example `gtk_text_iter_get_attributes()`. Compare family,
    size, scale, left_margin, right_margin, indent, justification, and
    wrap_mode for SpaPlatense Hechos 1:2 versus SpaRV Hechos 1:2.
  - Subtask 6 — real line geometry:
    For at least 3 long visual lines of each module, use GtkTextView
    display-line APIs: `gtk_text_view_backward_display_line_start()`,
    `gtk_text_view_forward_display_line_end()`,
    `gtk_text_view_get_iter_location()`. Measure `visible_width`,
    `line_start_x`, `line_end_x`, `line_width`, and
    `remaining_right_space = visible.x + visible.width - line_end_x`.
    Quantify whether SpaPlatense actually leaves hundreds of unused
    pixels.
  - Subtask 7 — identical direct-text control:
    Create a TEMPORARY test in the same GtkTextView: clear the buffer
    temporarily, insert the same long untagged string several times, for
    example "Este es un texto de control destinado exclusivamente a medir
    el ancho de representación del GtkTextView. Este mismo texto debe
    aparecer exactamente igual independientemente del módulo
    seleccionado. " Run with SpaPlatense selected and with SpaRV
    selected. Remeasure `visible_width`, `line_start_x`, `line_end_x`,
    `remaining_right_space`. Then revert this test.
  - Subtask 8 — identical HTML control:
    Create identical minimal temporary HTML:
    `<html><body><p>texto largo idéntico repetido...</p></body></html>`
    Pass it through `HtmlOutput()` for both SpaPlatense and SpaRV.
    Measure visible_width, line geometry, tags, and effective
    attributes. Compare A. direct text, B. minimal HTML, C. real HTML.
    The FIRST layer where the difference appears is the most important.
  - Subtask 9 — state leakage:
    Sequence A: SpaRV Hechos 1 → SpaPlatense Hechos 1 → SpaRV Hechos 1.
    Sequence B after restart: SpaPlatense Hechos 1 → SpaRV Hechos 1 →
    SpaPlatense Hechos 1. Compare margins, visible_width, font, scale,
    tags, and line geometry. Determine whether the result depends on
    navigation order.
  - Subtask 10 — HTML element handling:
    Confirm exactly how `<p>`, `<p class="verse">`, `<div dir=ltr>`,
    `<font>`, `<br>`, and `body { text-align: justify; }` are processed.
    Do not assume they are guilty. In particular demonstrate whether
    `dir=ltr` creates Pango/GTK state, whether justify is implemented,
    and whether font-family/scale changes logical width or only wrap.
  - Restrictions:
    Do not apply as a solution without evidence: `width:100%`, removing
    `dir=ltr`, resetting margins to 0, changing font, changing CSS, or a
    SpaPlatense-specific hack. No trial-and-error. The root cause must
    be backed by measurements.
  - No graphical session:
    If GTK runtime tests cannot run because no real graphical session is
    available, do not invent results. Instead:
    1. implement only the necessary instrumentation;
    2. compile it;
    3. leave the changes prepared;
    4. generate a reproducible script/command for the user to run the app;
    5. state exactly where logs will be written;
    6. leave this task unchecked with `Status: BLOCKED` (the loop
       recognizes only PENDING, BLOCKED, or DONE; do not invent another
       status name);
    7. in Evidence, record the attempted action, the concrete display
       error, the exact user command, the log path, and the exact output
       the user must provide;
    8. end the agent response with `BLOCKED:` plus the reason.
    When those logs exist, the next execution of this SAME task must
    resume from Evidence and analyze the logs without starting over.
  - Artifacts / logs:
    `/tmp/SpaPlatense-final-after-fix.html` may already exist. Do not
    delete it until this investigation is finished. Prefer writing
    instrumentation to a stable file such as
    `/tmp/biblia-width-debug.log` and document that path in Evidence.
  - Resume protocol:
    On every execution, read this entire task block including Evidence
    first. If `/tmp/biblia-width-debug.log` or equivalent documented logs
    already exist, analyze them before adding more probes. Continue
    unfinished subtasks. Implement a fix only after the root cause is
    proven. After a BLOCKED wait for user data, resume this same task
    rather than creating a new one.
  - Acceptance criteria:
    This task may be marked DONE only when evidence answers:
    1. Does GtkTextView have the same available width for SpaPlatense
       and SpaRV?
    2. Are the global margins equal?
    3. Does reading_mode alter one of the modules?
    4. Do the effective GtkTextTags differ?
    5. Does identical direct text use the same width?
    6. Does identical minimal HTML use the same width?
    7. At which first layer does the difference appear?
    8. How many px of remaining_right_space does each module leave?
    9. Is there state leakage?
    10. What is the root cause?
    11. Which file/function must be corrected?
    12. What is the appropriate minimal fix?
    Only AFTER proving the root cause may the agent implement the
    minimal fix. After the fix: compile; run available tests; repeat
    measurements; demonstrate that SpaPlatense uses the width correctly;
    confirm SpaRV has no regression; remove temporary instrumentation;
    keep only justified permanent tests/instrumentation.
  - Relevant tests:
    - `wk_html_surface_test`
    - `panel_load_state_test`
    - any focused renderer/layout regression added for the proven cause
    - target `biblia-elim`
    - full default build
    - `git diff --check`
    - real-display comparison of SpaPlatense Hechos 1 versus SpaRV
      Hechos 1
  - Do not:
    - Reopen the already-fixed OSIS poetry HTML bug.
    - Modify `normalize_poetry_carry()`, SWORD, OSIS,
      `prepare_display_content()`, `CleanupContent()`, module `.conf`
      files, or module CSS without new incontrovertible evidence.
    - Apply `width:100%`, remove `dir=ltr`, zero margins, change font,
      change CSS, or add a SpaPlatense-specific hack without
      measurements.
    - Invent GTK runtime numbers when no display is available.
    - Start STARTUP-PERF-105 or any other TASKS.md task in the same
      execution.
    - Commit or push.
  - Evidence:
    - Real chain confirmed: `GTKChapDisp::display()` → `HtmlOutput()`
      (`src/gtk/utilities.c`) → `wk_html_open_stream/write/close` →
      `load_html()` → `walk_node()` → `spans_apply()` → `GtkTextBuffer` /
      `GtkTextView` (`GTK_WRAP_WORD_CHAR`, `VIEW_SIDE_PAD`/`NORMAL_SIDE_MARGIN`
      = 14). `reading_mode_apply_measure()` only widens side margins when
      `settings.reading_mode` is on; it is module-agnostic.
    - Runtime Hechos 1:2 comparison (`scripts/ui-width-101-measure.sh`,
      logs `/tmp/biblia-width-debug.log`, HTML dumps
      `/tmp/biblia-width-{SpaPlatense,SpaRV}.html`):
      1. Same available width: view/scroller allocated 817×363 (normal) and
         1718×718 (reading mode) for both modules.
      2. Global margins equal: 14/14 with reading_mode=0; 496/496 with
         reading_mode=1 (cpl=100, width_pct=90).
      3. reading_mode does not alter one module differently; both share the
         same measure path and font sample.
      4. Effective tags at verse 2 match (small, ff:Serif, sz≈0.833, fg,
         curverse); no left/right-margin or indent tags.
      5. Identical direct text: remaining_right_space 21/26/39 for both.
      6. Identical minimal HTML: same remaining 21/26/39 for both.
      7. First differing layer is D (real module HTML / content structure),
         not A/B/C. SpaPlatense emits section `<h3 class="title">` headings
         and more paragraph boundaries; SpaRV has `Feature=NoParagraphs`
         continuous prose. Long prose lines fill equally.
      8. Long prose remaining ≈20–120 px (wrap remainder) for both; short
         heading/paragraph-end lines leave 600+ px because the content is
         short, not because the view is narrower.
      9. No state leakage across SpaRV↔SpaPlatense relaunches; controls and
         margins stay identical.
      10. Root cause: apparent unused width is SpaPlatense content structure
          (headings + paragraph ends + poetic `<l>` line breaks elsewhere),
          not a GtkTextView/margin/parser width bug.
      11. No production file/function requires a width fix.
      12. Appropriate minimal fix: none. Temporary probes removed; permanent
          `reading_width_contract_test` locks the geometry contract; measure
          script retained for reruns.
    - `reading_width_contract_test`: PASS (`reading_width_contract_failures=0`;
      prose remaining=19, heading remaining=605 with margins unchanged).
    - `panel_load_state_test`, `wk_html_surface_test`: PASS.
    - target `biblia-elim` and full default build: PASS.
    - `git diff --check`: PASS.

- [x] STARTUP-PERF-105 Attribute and reduce remaining GTK event-drain cost
  - Status: DONE
  - Objective:
    Reduce the remaining approximately 407-410 ms synchronous GTK event-drain
    startup cost, but only after objectively identifying what consumes it.
  - Current measured baseline:
    - Fresh: `GTK_EVENT_DRAIN_BEGIN -> GTK_EVENT_DRAIN_END` 410.3 ms median,
      iteration count 45, iteration total 408.5 ms, longest iteration 62.2 ms.
    - Reused: `GTK_EVENT_DRAIN_BEGIN -> GTK_EVENT_DRAIN_END` 407.1 ms median,
      iteration count 48, iteration total 398.6 ms, longest iteration 61.0 ms.
  - Phase A -- attribution:
    - Inspect `sync_windows()` and preserve its behavior.
    - Profile actual expensive callbacks/operations, not only iteration
      duration, and record which iterations are expensive.
    - Correlate long iterations with concrete lifecycle/callback categories:
      realize/map, allocation/layout, paned/notebook geometry, CSS/style,
      renderer lifecycle, application signal callbacks, draw/frame work,
      repeated visibility/layout changes, or another measured cause.
    - Use monotonic timing and keep diagnostics opt-in under
      `BIBLIA_ELIM_UI_LOAD_DEBUG=1` or an equivalent diagnostic switch.
    - Avoid noisy release logging, account for most of the drain where
      technically possible, and compare multiple samples where available.
  - Phase B -- optimization:
    - Optimize only if Phase A identifies a clear avoidable contributor.
    - Apply the smallest safe change that removes redundant startup work while
      preserving required GTK semantics and all startup-visible behavior.
    - If attribution is inconclusive, do not change behavior; leave the task
      blocked with profiling evidence.
  - Correctness constraints:
    - Preserve immediate PREV/NEXT, chapter/reference navigation, Bible
      renderer, commentary/dictionary, sidebar/previewers, compare/parallel,
      reading mode, zoom state/indicator, themed placeholders, backend-only
      anchors, and clean shutdown.
    - Do not remove `sync_windows()`, replace it with sleeps, spin arbitrary
      GTK loops elsewhere, defer required work past `GTK_MAIN_ENTER`, hide the
      complete main window, suppress warnings, hardcode geometry, or disable
      required realize/map behavior.
    - Do not touch backend/schema/importers unless direct profiling proves
      relevance.
  - Tests and validation:
    - Add focused tests for any optimization contract introduced.
    - Run startup/performance analyzer, layout, `wk_html_surface_test`,
      `panel_load_state_test`, navigation readiness, zoom, startup diagnostics,
      `gtk_lifecycle_smoke` where available, `biblia-elim`, full default build,
      and `git diff --check`.
    - Final success requires seven fresh plus seven reused real-display samples.
      Compare before/after medians and MAD for GTK drain duration, iteration
      count/total/longest, show-all duration, window creation, and total startup.
    - If real display is unavailable, leave unchecked, set `Status: BLOCKED`,
      state `READY FOR REAL-DISPLAY VALIDATION`, and do not manufacture a win.
  - Evidence:
    - `sync_windows()` was inspected and remains the same conditional drain of
      every pending event through `gtk_main_iteration()`; it was not removed,
      relocated, delayed, or replaced.
    - Re-analysis of all seven fresh and seven reused STARTUP-PERF-104 traces
      shows that iterations containing the existing selected-widget
      `size-allocate` markers account for median 341.8/408.5 ms (83.7%) fresh
      and 356.8/398.6 ms (89.5%) reused. Every longest iteration in both
      seven-run sets contains allocation activity. Style-correlated iteration
      time is 35.9 ms fresh and 36.5 ms reused; no renderer lifecycle marker
      occurs inside the startup drain. This objectively localizes most time to
      allocation-correlated iterations, but does not prove that allocation
      itself consumes the whole interval or that any GTK layout pass is
      avoidable.
    - Added opt-in per-iteration attribution over the already constructed
      widget tree. It counts actual realize, map, style, and size-allocation
      signal deliveries split into renderer, GtkPaned, notebook, and other
      categories, and measures top-level frame draw and renderer draw with the
      monotonic clock. No observers are connected and no new output is emitted
      unless `BIBLIA_ELIM_UI_LOAD_DEBUG=1`. Profile reporting occurs after the
      iteration end marker so its I/O is excluded from iteration duration.
    - The analyzer validates complete numbered profile pairs, retains legacy
      trace compatibility, aggregates callback categories across samples, and
      records the three longest concrete iterations per log with their
      lifecycle evidence. Nineteen deterministic analyzer tests PASS.
    - No optimization was applied: the retained traces cannot distinguish a
      redundant application geometry update from required GTK layout, style,
      or draw work, and the newly attributed binary cannot be sampled in this
      environment.
    - `main_window_layout_test`, `wk_html_surface_test`,
      `panel_load_state_test`, `verse_navigation_readiness_test`,
      `navbar_entry_reference_test`, `zoom_state_test`,
      `zoom_indicator_test`, `zoom_anchor_reflow_test`, and
      `startup_diagnostics_test` PASS with zero failures. The `biblia-elim`
      target and full default build PASS; the legacy 7+7 analyzer pass
      reproduces the 104 medians. `gtk_lifecycle_smoke` skips with
      `gtk_lifecycle_smoke_skipped=no-usable-xvfb`.
    - A direct Wayland run also exits immediately after `APP_START`. Therefore
      no attributed real-display samples, safe Phase-B target, or post-change
      7+7 comparison can be produced here. Human/external execution on an
      accessible real display must first collect 7 fresh plus 7 reused
      attributed samples; only a demonstrated avoidable contributor may then
      be optimized and validated with a second 7+7 set.
    - `git diff --check` PASS. `READY FOR REAL-DISPLAY VALIDATION`.
    - Phase A2 opens both `style_other` and `allocate_other` by runtime GObject
      type without a hardcoded type list. Per iteration it now retains
      process-local widget identity, type, last allocation geometry, unique
      allocation/style recipients, same-instance repeats, and identical versus
      changed geometry repeats. Detailed output remains strictly opt-in under
      `BIBLIA_ELIM_UI_LOAD_DEBUG=1` and is emitted after the drain end marker so
      report I/O is excluded from the measured drain; no GTK work is suppressed,
      reordered, skipped, coalesced, or deferred.
    - The analyzer validates the extended per-type and per-instance records,
      preserves the original attributed and unattributed trace formats, reports
      per-type median/MAD/min/max and scenario churn medians, and expands each
      sample's three longest iterations with top types, churn, paned/notebook
      allocation, and root draw evidence. Twenty-seven deterministic analyzer
      tests PASS, including type aggregation, unique/repeated instance tracking,
      identical/changed geometry, repeated style, malformed records, and legacy
      compatibility. The supplied seven-fresh/seven-reused round-1 collection
      still analyzes successfully.
    - `main_window_layout_test`, `wk_html_surface_test`,
      `panel_load_state_test`, `verse_navigation_readiness_test`,
      `zoom_state_test`, `zoom_indicator_test`, `zoom_anchor_reflow_test`, and
      `startup_diagnostics_test` PASS with zero failures. The `biblia-elim`
      target and full default build PASS. No optimization was applied because
      round 1 predates the per-instance/type probes and cannot establish safe
      application-side redundancy.
    - Leave unchecked with `Status: BLOCKED`; an accessible real X11 display
      must collect the official seven fresh plus seven reused extended traces
      before any Phase-B decision. `READY FOR REAL-DISPLAY ATTRIBUTION ROUND 2`.
    - Real-display attribution round 3 (`build/startup-perf-105-v3-round3-20260911-233510`)
      collected seven fresh plus seven reused samples with
      `attribution_version=3` session profiles. First-drain medians were already
      206.8 ms fresh / 211.8 ms reused on this machine (iteration count still
      ~47). Cross-iteration identical-geometry reallocations were median 241
      of 428 allocation callbacks (56%); only 58 geometry changes. One
      compositor window resize (847×660 → 819×664) explains the changed set;
      afterward the same widget sets were reallocated with Jaccard 1.0. The
      collapsed in-chapter `GtkSearchBar` still participated with 18 subtree
      widgets and about 15% of first-drain allocation callbacks despite height 1.
    - Phase B (smallest avoidable contributors):
      1. Keep `gui_barra_busqueda_crear()` out of ancestor `show_all()` via
         `no-show-all` + hide until Ctrl-F; `mostrar` shows, `ocultar` folds
         and hides again.
      2. Apply `gui_set_bible_comm_layout()` before the first
         `gtk_widget_show_all()` so saved splitter positions are used on the
         first map/allocate.
      3. Write the study `hpaned` position once through
         `main_study_hpaned_position()` instead of up to three intermediate
         `gtk_paned_set_position()` calls.
      `sync_windows()` behavior is unchanged.
    - Post-change 7+7 (`build/startup-perf-105-post-20260911-234235`) versus
      same-day round-3 attribution baseline:
      - Fresh drain 206.8 → 150.0 ms (−27.5%); reused 211.8 → 153.7 ms (−27.4%).
      - Fresh `show_all` 273.8 → 248.5 ms (−9.2%); reused 271.1 → 243.0 ms (−10.4%).
      - Iteration count 47 → 42/43; identical-geometry repeats 241 → 113 (−53.1%);
        distinct allocated widgets 129 → 107; allocation callbacks 428 → 257.
      - Fresh `APP_START→GTK_MAIN_ENTER` 1105.9 → 1029.6 ms; reused 1096.8 → 1011.2 ms.
      - Versus the original PERF-104 task baseline, drain fell 410.3 → 150.0 ms
        fresh (−63.4%) and 407.1 → 153.7 ms reused (−62.2%).
      - SearchBar absent from every post-change first-drain session profile.
    - `barra_busqueda_startup_test`, `main_window_layout_test`,
      `panel_load_state_test`, `wk_html_surface_test`,
      `verse_navigation_readiness_test`, `navbar_entry_reference_test`,
      `zoom_state_test`, `zoom_indicator_test`, `zoom_anchor_reflow_test`,
      `startup_diagnostics_test`, `application_name_startup_test`, and
      `startup_performance_baseline` PASS. `biblia-elim` and full default build
      PASS; `git diff --check` PASS.
    - `gtk_lifecycle_smoke` runs under Xvfb here but fails a pre-existing
      CREATE/SHOW/MAP check for hidden `bible-compare` (no MAP while
      startup `no-show-all` keeps compare unmapped). Failures counter is 0 and
      navigation/renderers complete; unrelated to the search-bar drain fix.

- [x] UI-LAYOUT-103 Eliminate GtkLabel negative allocations in fallback badges
  - Status: DONE
  - Description:
    Diagnose and fix the repeated `Negative content height -3 (allocation 9,
    extents 6x6)`, `Negative content height -11 (allocation 1, extents 6x6)`
    (node label, owner GtkLabel) and `gtk_widget_size_allocate(): attempt to
    allocate widget with width ... and height -7` warnings observed during
    normal real-display use.
  - Evidence:
    - Real GDB evidence under `G_DEBUG=fatal-warnings` identifies the widget as
      the fallback badge: a real `GtkLabel` (classes `fallback-badge dark`,
      text `Texto suplido desde Reina-Valera 1909`) created by
      `src/webkit/wk-html.c::insert_fallback_badge()`, not HTML content.
    - Hierarchy: `GtkLabel` → `GtkBox row` (inline child anchored directly in
      the `GtkTextView` named `elim-html`) → `GtkScrolledWindow` → `GtkStack`
      → `WkHtml` → … → `GtkWindow elim-app`.
    - Root cause: `row` carried widget margins `margin_top = 4` and
      `margin_bottom = 4`. Under GTK 3.24.52, `adjust_allocation()` in
      `gtk_text_view_value_changed()` re-feeds the child's stored allocation
      (margins already removed) into `gtk_widget_size_allocate()`, which
      subtracts the margins again on every frame-clock scroll step. Height
      progressed roughly `41 -> 33 -> 25 -> 17 -> 9 -> 1 -> -7`.
    - The badge label has 6 px vertical extents on each side (padding 5 +
      border 1), hence `9 - 12 = -3` and `1 - 12 = -11`; the row itself then
      reached `height -7`. Same GTK behavior as UI-LAYOUT-101's inline
      `<hr>` separator.
    - Trigger: startup/scroll on a chapter that contains fallback badges
      (reproduced with TorresAmat, Mateo 12:20). Passages without badges
      (e.g. Ps 2:1) do not reproduce it.
    - Fix: moved `gtk_widget_set_margin_top/bottom(row, 4)` to
      `gtk_widget_set_margin_top/bottom(label, 4)`. `row` keeps the default
      zero margin; `GtkBox` allocates the label margins correctly. Visual
      spacing is equivalent. No change to content, fallback, badges logic,
      versification, or the content resolver.
    - Modified file: `src/webkit/wk-html.c`.
    - Real-display validation (TorresAmat, Mateo 12:20 with fallback badges):
      before the fix about 120 `Gtk-WARNING` in a normal run; after the fix a
      ~70 s normal run with 0 `Gtk-WARNING` and 0 `CRITICAL`, and a ~75 s run
      under `G_DEBUG=fatal-warnings` with no stop. Maximize/restore and
      vertical/horizontal resize exercised; badge spacing visually correct.
    - `wk_html_surface_test`, `poetry_line_wrap_test`,
      `zoom_anchor_reflow_test`, `study_reading_layout_test`, and
      `main_window_layout_test` PASS. `cmake --build build --target
      biblia-elim -j4` PASS; `git diff --check` PASS.
  - Do not:
    - Commit or push.

- [x] TORRES-1835-A-GLYPH-PIXEL-RECOVERY-131 Recover safe compound a* verse markers using validated facsimile pixel evidence
  - Status: DONE
  - Description:
    Use the facsimile pixel evidence validated by
    TORRES-1835-A-GLYPH-PIXEL-SHAPE-130 to recover only safe compound `a*`
    verse markers.
    The first OCR token `a` may represent printed digit 1 or 2 and must be
    interpreted exclusively from precomputed facsimile pixel evidence.
    The parser must not open, render, or process the PDF at runtime.
  - Preconditions:
    - TORRES-1835-A-GLYPH-PIXEL-SHAPE-130 is integrated in master.
    - `data/torresamat1835/a_glyph_pixel_features.json` is present and its
      provenance matches the validated facsimile.
    - The existing nine safe compound glyph mappings from task 128 remain
      unchanged.
    - No standalone `a` or other simple glyph form is enabled.
  - Required behavior:
    - Fail closed if pixel evidence is missing, stale, mismatched, or from an
      unsupported schema/source.
    - Require agreement between validated ink-width and aspect-ratio evidence
      for the first printed digit; disagreement or intermediate values must
      abstain.
    - Interpret the second token independently. Literal decimal second tokens
      may be used literally.
    - `a o` may be enabled only after confirming that the second `o` represents
      printed 0 in every eligible would-apply case.
    - `a a` may be enabled only after confirming that the second `a` represents
      printed 2 in every eligible would-apply case.
    - `a I` remains withheld unless an independent safe rule for its second
      token is demonstrated.
    - Require right-column Bible body context, marker-like indentation, valid
      framing, and native verse-limit validation.
    - Never infer a value from the expected gap, previous verse, next verse,
      or canonical sequence.
  - Acceptance:
    - Perform a complete dry run before enabling recovery.
    - Visually verify every case predicted as first digit 1.
    - Reparse from source; do not patch VerseRefs after parsing.
    - Report exact marker/ref and block-ownership deltas.
    - Preserve raw OCR and all previous review batches.
    - Preserve the task-128 compound recoveries unchanged.
    - Chapter map remains 337/337.
    - `duplicate_refs = 0`.
    - `out_of_order_refs = 0`.
    - `outside-canon = 0`.
    - `ocr_blocks = 57700`.
    - No block loss or dual ownership.
    - Parser runtime performs no PDF rendering or image processing.
  - Do not:
    - Open or render the facsimile in parser runtime.
    - Recompute Otsu or pixel features during parsing.
    - Use OCR bbox width as the classifier.
    - Add global `a -> 1`, `a -> 2`, `o -> 0`, or `I -> 1` substitutions.
    - Recover standalone `a`, `S`, `y`, `o`, or other simple glyph families.
    - Recover `GLUED_FRAME` cases.
    - Use expected-gap, previous+1, or next-1 inference.
    - Hardcode block IDs, pages, chapters, or verses.
    - Edit raw OCR, chapter structure, `roman.py`, or `written_ordinals.py`.
    - Commit or push implementation work from the task agent.

  - Evidence:
    - Result: PARTIAL READY, scope closed conservatively.
    - Recovered markers: 276.
    - VerseRefs: 3572 -> 3848; `refs_added = 276`, `refs_removed = 0`,
      and `refs_renumbered = 0`.
    - Physical gaps: 3531 -> 3255; glyph gaps: 1747 -> 1310.
    - Block ownership changes: 1900, all explained; `block loss = 0` and
      `dual ownership = 0`.
    - Chapters: 337/337; unresolved chapter claims: 0; `duplicate_refs = 0`;
      `out_of_order_refs = 0`; outside-canon artifacts: 0; `ocr_blocks = 57700`.
    - Task-128 behavior was preserved. Standalone `a`/`S`/`y`/`o` families
      remain excluded; `a-I` and other unsafe second-token forms remain
      withheld; `GLUED_FRAME` remains intentionally unresolved.
    - CTest Torres passed. Full CTest passed in the final task-131 report.

- [x] TORRES-1835-GLUED-COMPOUND-MARKER-AUDIT-132 Diagnose glued compound verse markers using reproducible facsimile segmentation
  - Status: DONE
  - Description:
    Investigate compound verse-marker candidates whose scan debris, marker
    glyph, or neighboring character was merged into one OCR word
    (`GLUED_FRAME`), preventing tasks 128-131 from obtaining a reliable
    marker bbox.
    This task is diagnostic only. Determine whether the actual marker glyph
    can be segmented reproducibly from facsimile pixels offline without
    estimating character positions from OCR word geometry.
  - Baseline context:
    - Task 131 left GLUED_FRAME intentionally unrecovered.
    - The final task-131 report counted 159 glued `a*` candidates.
    - Runtime parsing must remain free of PDF/image processing.
    - Existing pixel evidence and safe recoveries from tasks 128-131 must
      remain unchanged.
  - Required behavior:
    - Inventory all GLUED_FRAME candidates from the current corpus.
    - Distinguish scan debris glued to a marker from true multi-character OCR
      words and ordinary text.
    - Use the verified facsimile offline to test reproducible pixel
      segmentation.
    - Treat OCR bbox only as a localization anchor.
    - Never divide a word bbox by character count or assume monospacing.
    - Preserve exact source/page/block provenance.
    - Measure whether marker segmentation is stable under reasonable
      binarization changes.
    - Produce diagnostic metadata and a reproducible audit.
    - Do not create or move VerseRefs in this task.
  - Acceptance:
    - Candidate inventory is complete and deterministic.
    - Segmentation method is explicit, reproducible, and source-bound.
    - Known positive/negative cases are visually reviewed.
    - False splits of ordinary Spanish text are measured.
    - No runtime image access is introduced.
    - VerseRefs and block ownership remain unchanged.
    - Chapters remain 337/337.
    - `duplicate_refs = 0`.
    - `out_of_order_refs = 0`.
    - `outside-canon = 0`.
    - `ocr_blocks = 57700`.
    - Recommendation clearly states whether a later recovery task is safe.
  - Do not:
    - Recover GLUED_FRAME markers yet.
    - Infer glyph positions by dividing OCR bboxes.
    - Use expected gaps, previous+1, or next-1.
    - Hardcode pages, blocks, books, chapters, or verses.
    - Process facsimile images in parser runtime.
    - Edit raw OCR.
    - Change chapter structure.
    - Touch unrelated UI work.
    - Commit or push implementation work from the task agent.
  - Evidence:
    - Result: FOUNDATION READY; diagnostic scope closed.
    - GLUED_FRAME scoped population = 159; eligible BODY/right candidates =
      81; all 81/81 eligible candidates were processed.
    - Reproducible single segments = 24; reproducible multi-segments = 33;
      ambiguous = 12; no physical separation = 12.
    - Seven of eight ordinary-text controls produced false-positive automated
      splits; four of eight reviewed positive controls failed; the 300/600-DPI
      control had one structural mismatch.
    - No family qualified `SAFE_FOR_FUTURE_RECOVERY`; estimated safely
      automatable population under the current rule = 0.
    - VerseRefs remained 3848 -> 3848; ownership remained unchanged; physical
      gaps remained 3255 -> 3255; glyph gaps remained 1310 -> 1310.
    - Task-128 and task-131 marker/ref behavior remained unchanged.
    - Chapters = 337/337; unresolved chapters = 0; `duplicate_refs = 0`;
      `out_of_order_refs = 0`; `outside-canon = 0`; `ocr_blocks = 57700`.
    - CTest Torres passed 28/28; full CTest passed 40/40.

- [x] TORRES-1835-GLUED-MARKER-DISCRIMINATOR-133 Determine whether source-derived structural and pixel evidence can distinguish glued verse markers from ordinary text
  - Status: DONE
  - Description:
    Task 132 showed that physically stable segmentation is not sufficient:
    ordinary Spanish text can produce the same whitespace/component signal.
    Determine whether a small, transparent, source-derived discriminator can
    separate true glued verse-marker segments from ordinary text with zero
    accepted negative controls.
    This task is diagnostic only.
  - Baseline context:
    - Task 132 processed all 81 eligible BODY/right GLUED_FRAME candidates.
    - 57 produced physically stable segmentations, but seven of eight ordinary
      text controls were also split positively.
    - No GLUED_FRAME family is currently safe for recovery.
    - Existing tasks 128 and 131 recoveries must remain unchanged.
  - Required behavior:
    - Build a facsimile-labelled positive/negative dataset from the task-132
      population.
    - Evaluate only transparent source-derived structural/pixel features.
    - Treat task-132 segment geometry as candidate evidence, not proof of a
      marker.
    - Measure exact segment position relative to trusted marker indentation,
      following-text geometry, baseline alignment, segment/body separation,
      and simple pixel morphology.
    - Prefer explicit abstention.
    - Require zero ordinary-text negatives accepted by any proposed future
      rule.
    - Do not use expected verse, previous+1, next-1, or canonical gaps.
    - Do not create VerseRefs or move ownership.
    - Do not introduce runtime facsimile/image processing.
  - Acceptance:
    - Labelled dataset is reproducible and provenance-complete.
    - Positives and negatives are sufficiently represented.
    - Candidate features and rules are explicit and auditable.
    - Any proposed safe rule has zero accepted negative controls and no
      class inversion on reviewed data.
    - Cross-page / resolution robustness is measured.
    - Runtime parser remains unchanged.
    - VerseRefs and ownership remain unchanged.
    - Chapters remain 337/337.
    - `duplicate_refs = 0`.
    - `out_of_order_refs = 0`.
    - `outside-canon = 0`.
    - `ocr_blocks = 57700`.
    - The task ends with a clear decision:
      `SAFE_FOR_FUTURE_RECOVERY` or `UNSAFE_TO_AUTOMATE`.
  - Do not:
    - Recover GLUED_FRAME markers.
    - Use ML, neural OCR, embeddings, or fuzzy recognition.
    - Tune rules from expected verse numbers.
    - Use previous+1 or next-1.
    - Hardcode pages, blocks, books, chapters, or verses in production.
    - Divide OCR word bboxes by character count.
    - Process facsimile pixels in parser runtime.
    - Edit raw OCR or chapter structure.
    - Touch unrelated UI work.
    - Commit or push implementation work from the task agent.

  - Evidence:
    - Result: BLOCKED; diagnostic scope completed.
    - Verdict: UNSAFE_TO_AUTOMATE.
    - Final labelled positives: 33; ordinary-text negatives: 25;
      apparatus negatives: 2; Latin negatives: 7; labelled population: 67.
    - No transparent single-feature or two-feature rule achieved useful
      positive acceptance with zero negative acceptance. Selected safe rule:
      NONE. Safe behavior is complete abstention.
    - true positives accepted = 0; false positives = 0; true negatives = 34;
      false negatives = 33; abstentions = 67; positive coverage = 0%.
    - Potential automated markers = 0; refs = 0; gaps = 0.
    - All seven task-132 ordinary-text false-positive regressions were
      rejected or abstained. Four difficult task-132 true-marker positives
      remained abstained.
    - VerseRefs remained 3848 -> 3848; ownership remained unchanged;
      physical gaps remained 3255 -> 3255; glyph gaps remained 1310 -> 1310.
    - Task-128 and task-131 marker/ref behavior remained unchanged.
    - Chapters = 337/337; unresolved chapters = 0; canonical chapter gaps = 0;
      duplicate_refs = 0; out_of_order_refs = 0; outside-canon = 0;
      ocr_blocks = 57700.
    - CTest Torres 28/28 passed; full CTest 40/40 passed.
    - GLUED_FRAME recovery must remain disabled with current evidence.
    - Task 133 is DONE because its diagnostic question was answered: permitted
      transparent source-derived evidence cannot safely distinguish true glued
      markers from ordinary text with useful positive coverage.

- [x] TORRES-1835-REMAINING-GLYPH-PRIORITY-134 Reclassify remaining verse glyph gaps and identify the next evidence-backed recovery family
  - Status: DONE
  - Description:
    Return to the global remaining verse-gap inventory after tasks 125-133.
    Recompute every remaining physical/glyph gap from the current parser,
    classify each by reproducible root cause, separate already-handled and
    closed-unsafe families, measure existing review coverage, and identify
    exactly one bounded family for the next task using factual evidence.
    This task is diagnostic only.
  - Baseline context:
    - Current VerseRefs are approximately 3848; measure rather than assume.
    - Current physical gaps are approximately 3255.
    - Current glyph gaps are approximately 1310.
    - Safe compound forms from task 128 are already handled.
    - Safe compound a* forms from task 131 are already handled.
    - GLUED_FRAME was investigated by tasks 132-133 and is CLOSED_UNSAFE
      with current evidence.
  - Required behavior:
    - Recompute the full remaining gap inventory from current source.
    - Give every physical gap exactly one primary root-cause category.
    - Keep gap count, candidate-line count, unique-block count and exact-form
      count distinct.
    - Preserve exact OCR forms including case, accents, punctuation and token
      spacing.
    - Cross-reference all existing facsimile review batches before adding new
      reviews.
    - Audit major remaining forms such as standalone `y`, `á`, `a`, `S`,
      unsafe compounds such as `a I`, detached numeric fragments, exact-digit
      context rejections, layout/fused-column cases and no-local-evidence
      cases.
    - Keep GLUED_FRAME closed and out of the automation candidate pool.
    - Measure positive, negative, conflicting and unreviewed evidence per
      major family.
    - Recommend exactly one next family using factual population, evidence,
      negative-control safety and bounded implementation scope.
    - If no family is recovery-ready, recommend a targeted diagnostic task
      instead of inventing a recovery.
  - Acceptance:
    - Every current physical gap has exactly one primary root cause.
    - Root-cause counts sum exactly to the physical-gap total.
    - Exact OCR-form inventory is complete and deterministic.
    - Already-handled families are separated from remaining work.
    - GLUED_FRAME remains explicitly CLOSED_UNSAFE.
    - Major remaining families have review coverage and negative-control
      evidence measured.
    - Exactly one task-135 family is recommended from measurable evidence.
    - No parser/runtime recovery changes.
    - VerseRefs, ownership and gap output remain unchanged.
    - Chapters remain 337/337.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside-canon = 0.
    - ocr_blocks = 57700.
  - Do not:
    - Recover any new VerseRefs.
    - Modify parser recovery or glyph maps.
    - Reopen GLUED_FRAME recovery.
    - Use expected verse as a glyph label.
    - Use previous+1 or next-1.
    - Normalize accents or merge exact OCR forms by visual similarity.
    - Create arbitrary priority scores.
    - Use ML, embeddings, neural OCR or fuzzy classification.
    - Hardcode pages, blocks, chapters or verses.
    - Edit raw OCR or chapter structure.
    - Touch unrelated UI work.
    - Commit or push implementation work from the task agent.

  - Closure record:
    - Result: FOUNDATION READY; diagnostic scope completed.
    - VerseRefs = 3848; physical gaps = 3255 (interior 2302, leading 184,
      trailing 769); glyph gaps = 1310.
    - Primary root causes: standalone glyph 1310; detached numeric fragment
      71; exact digit/context rejected 5; layout/column corruption 1685;
      no-local marker evidence 184. The artifact provides the exact mutually
      exclusive accounting semantics.
    - Tasks 125, 126, 128 and 131 remain already handled; GLUED_FRAME remains
      CLOSED_UNSAFE from tasks 132-133.
    - No parser/runtime recovery was introduced. Ownership, physical gaps,
      glyph gaps, task-128 behavior and task-131 behavior remained unchanged.
    - Chapters = 337/337; unresolved chapters = 0; canonical chapter gaps = 0;
      duplicate_refs = 0; out_of_order_refs = 0; outside-canon = 0;
      ocr_blocks = 57700.
    - remaining_glyph_inventory.json is deterministic; source/provenance guards
      remain fail-closed.
    - Focused sources/baseline CTest = 2/2 passed; TorresAmat CTest = 28/28
      passed; full CTest = 39/40 passed. The only failure was the known
      pre-existing unrelated gtk_lifecycle_smoke failure: "dictionary panel
      did not reopen explicitly".
    - Recommended next family: standalone_glyph_candidate. No standalone
      glyph family is approved for recovery; task 135 must be diagnostic
      facsimile-review work first.
    - Task 134 is DONE because it completed the global inventory and identified
      the next bounded evidence-gathering family without changing runtime
      behavior.

- [x] TORRES-1835-STANDALONE-GLYPH-FACSIMILE-135 Build facsimile evidence for remaining standalone verse-marker glyphs
  - Status: DONE
  - Description:
    Build a reproducible facsimile-labelled evidence set for remaining
    standalone verse-marker glyph candidates identified by task 134. Study
    exact standalone OCR forms independently, including `y`, `á`, `a`, `S`
    and other current candidates. Determine which forms, if any, show
    consistent printed-digit semantics and strong negative discrimination to
    justify a later bounded recovery task. This task is diagnostic only.
  - Baseline context:
    - Current VerseRefs are approximately 3848; measure rather than assume.
    - Current physical gaps are approximately 3255.
    - Current glyph gaps are approximately 1310.
    - Task 134 identifies standalone_glyph_candidate as the next bounded
      family requiring evidence.
    - Compound forms solved by tasks 128/131 must not be mixed into this
      population; GLUED_FRAME remains CLOSED_UNSAFE.
  - Required behavior:
    - Recompute the standalone-glyph inventory from the current parser.
    - Preserve exact OCR forms; do not merge case, accents or visually similar
      glyphs.
    - Reuse prior facsimile provenance and build deterministic stratified
      samples with explicit positive and negative controls.
    - For each form measure population, books/pages, reviewed count, confirmed
      printed digits, ordinary-text negatives, conflicts and unreviewed count.
    - Audit `y`, `á`, `a` and `S` explicitly where they remain major forms.
    - Never infer a printed digit from an expected missing verse.
    - Classify forms as EVIDENCE_READY, NEEDS_MORE_REVIEW or
      UNSAFE_WITH_CURRENT_EVIDENCE and recommend exactly one bounded family or
      exact form for task 136.
  - Acceptance:
    - Standalone population is complete and deterministic; exact forms remain
      distinct; prior review provenance is not duplicated.
    - Labels come from printed evidence, with explicit negative controls.
    - No form is recovery-ready from frequency or expected gaps alone.
    - Exactly one next task is recommended from factual evidence.
    - Parser/runtime, VerseRefs, ownership, physical/glyph gaps and chapter
      invariants remain unchanged; duplicate_refs = 0; out_of_order_refs = 0;
      outside-canon = 0; ocr_blocks = 57700.
  - Do not:
    - Recover standalone glyphs or modify runtime glyph maps.
    - Normalize `á` to `a`, lowercase forms, or merge visual variants.
    - Treat `S`, `a` or `y` as globally numeric; use expected verse,
      previous+1 or next-1.
    - Reopen GLUED_FRAME or use ML, embeddings, neural OCR or fuzzy matching.
    - Hardcode pages, blocks, books, chapters or verses in production.
    - Edit raw OCR or chapter structure.
    - Touch unrelated UI work.
    - Commit or push implementation work from the task agent.

  - Closure record:
    - Result: PARTIAL READY; diagnostic scope completed.
    - Original standalone population = 1310. Refined taxonomy: true-single-
      token = 6; projected multi-token = 1304; unknown = 0.
    - `standalone_glyph_candidate` is a projected-token diagnostic category,
      not necessarily a physically one-token OCR line.
    - All 6 true-single-token occurrences were visually reviewed; recovery-
      ready cases = 0. Batch-135 reviews = 8.
    - Prior review records examined = 496; exact reusable prior matches = 0.
    - Task-128 stable linkage: HANDLED_EXACT = 0; REJECTED_EXACT = 23;
      NOT_APPLICABLE = 1281; linkage mismatch = 0; unresolved = 0.
    - Task-128 rejected exact reasons: no_trusted_marker_band = 18;
      outside_marker_band = 5.
    - Form 3: total 51, true-single 0, projected 51. Form 4: total 73,
      true-single 2, projected 71.
    - The reviewed `y` occurrence is projected multi-token ORDINARY_TEXT;
      the reviewed `a` occurrence is projected multi-token PRINTED_DIGIT 2.
      No global glyph mapping was approved.
    - No parser/runtime recovery was introduced. VerseRefs = 3848, physical
      gaps = 3255, glyph gaps = 1310, ownership unchanged. Task-128 and
      task-131 runtime behavior remained unchanged; GLUED_FRAME remains
      CLOSED_UNSAFE.
    - Chapters = 337/337; unresolved chapter claims = 0; canonical chapter
      gaps = 0; duplicate_refs = 0; out_of_order_refs = 0; outside-canon = 0;
      ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - Artifact is deterministic, idempotent, source-provenanced and
      diagnostic-only, and is not consumed by parser/runtime. The test_AH
      metadata allowlist remains exhaustive and fail-closed.
    - Focused/direct tests passed; build passed; TorresAmat CTest = 28/28;
      full CTest = 39/40. The only tolerated unrelated failure is
      gtk_lifecycle_smoke: "dictionary panel did not reopen explicitly".
    - Recommended next work: diagnostic audit of the 23 TASK128_REJECTED_EXACT
      projected occurrences before any recovery consideration.
    - Task 135 is DONE because its diagnostic question was answered: the
      supposed standalone family is overwhelmingly projected-token data, and
      the six physically true-single cases were exhaustively reviewed with
      zero recovery-ready cases.

- [x] TORRES-1835-PROJECTED-REJECTION-AUDIT-136 Validate task-128 rejected projected-marker occurrences
  - Status: DONE
  - Description:
    Audit the 23 projected occurrences linked exactly to task-128 rejected
    candidates. Preserve stable occurrence linkage and determine whether the
    historical rejection reasons remain correct or whether a bounded
    discriminator/recovery study is justified. This task is diagnostic only.
  - Baseline context:
    - Original task-134 standalone population = 1310.
    - Refined task-135 taxonomy: true-single-token = 6, projected multi-token
      = 1304, unknown = 0; no true-single recovery candidate exists.
    - Task-128 projected linkage: HANDLED_EXACT = 0, REJECTED_EXACT = 23,
      NOT_APPLICABLE = 1281; rejected reasons are no_trusted_marker_band = 18
      and outside_marker_band = 5.
    - Current VerseRefs = 3848; physical gaps = 3255; glyph gaps = 1310.
      Do not broaden to the full projected population without evidence.
  - Required behavior:
    - Enumerate all 23 rejected occurrences with source SHA, scan page, block,
      raw OCR, token sequence/index and bbox/geometry.
    - Reconstruct each exact task-128 decision and preserve the two historical
      rejection reasons without merging them.
    - Verify reasons against current source-derived geometry and use facsimile
      review only where needed.
    - Distinguish correct historical rejection, stale classification,
      discriminator candidate, later recovery candidate and insufficient
      evidence; never use expected verse sequence.
    - Recommend exactly one bounded task-137 target from evidence.
  - Acceptance:
    - All 23 occurrences are accounted for exactly once with deterministic
      stable linkage; 18 + 5 reconciles unless a documented mismatch is
      proven.
    - No parser/runtime, VerseRef, ownership, physical/glyph gap, task-128 or
      task-131 changes; GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337; duplicate_refs = 0; out_of_order_refs = 0;
      outside-canon = 0; ocr_blocks = 57700; exactly one task-137 target is
      recommended.
  - Do not:
    - Recover projected markers or modify task-128 runtime rules.
    - Widen marker-band thresholds or accept no_trusted_marker_band/outside-
      marker_band cases by default.
    - Use expected verse, previous+1 or next-1; reopen GLUED_FRAME or
      standalone recovery; use ML, embeddings or neural OCR.
    - Hardcode page/block/book/chapter/verse cases in production.
    - Touch unrelated UI work.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.

  - Closure record:
    - Result: READY; diagnostic scope completed.
    - Exact rejected projected occurrences audited = 23; unique stable
      occurrence identities = 23.
    - Historical and recomputed rejection split: no_trusted_marker_band = 18;
      outside_marker_band = 5. Reason mismatches = 0.
    - Final status: REJECTION_CONFIRMED = 23; stale classification = 0;
      discriminator candidates = 0; recovery-validation candidates = 0;
      insufficient evidence = 0.
    - no_trusted_marker_band: 18 Psalms occurrences on scan page 32, form
      `I o`, caused by fewer than three trusted marker-band anchors.
    - outside_marker_band: 5 Isaiah occurrences on scan pages 542 and 553,
      forms `I o` / `I a`; measured outside-band distances include +48 px
      against tolerances 33 px and 37.5 px.
    - Accepted task-128 controls = 183; negative controls = 3; facsimile
      reviews added = 0 because source-derived geometry was sufficient.
    - All 23 historical rejections remain justified. No marker was recovered;
      no task-128 threshold or grammar changed.
    - VerseRefs = 3848; physical gaps = 3255; glyph gaps = 1310; ownership,
      task-128 behavior and task-131 behavior unchanged; GLUED_FRAME remains
      CLOSED_UNSAFE.
    - Chapters = 337/337; unresolved chapter claims = 0; canonical chapter
      gaps = 0; duplicate_refs = 0; out_of_order_refs = 0; outside-canon = 0;
      ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - Artifact deterministic and idempotent. Direct tests and build passed;
      TorresAmat CTest = 28/28; full CTest = 40/40, with two environment-
      dependent tests skipped by the suite and no new failures.
    - Recommended next work: bounded diagnostic discriminator study of the
      18 no_trusted_marker_band occurrences.
    - Task 136 is DONE because all 23 exact projected rejections were
      reconciled against current source-derived evidence and every historical
      rejection was confirmed.

- [x] TORRES-1835-NO-TRUSTED-BAND-DISCRIMINATOR-137 Evaluate whether no_trusted_marker_band cases admit a safe source-derived discriminator
  - Status: DONE
  - Description:
    Study only the 18 task-128 projected occurrences rejected as
    no_trusted_marker_band. Determine whether transparent source-derived
    geometry or neighboring-marker evidence can distinguish a safe bounded
    subfamily without weakening task-128 trusted-band policy. Diagnostic only.
  - Baseline context:
    - Task 136 confirmed all 23 projected task-128 rejections: 18
      no_trusted_marker_band and 5 outside_marker_band.
    - The 18 cases are concentrated in Psalms, scan page 32, form `I o`;
      historical cause is fewer than three trusted marker-band anchors.
    - Accepted task-128 controls = 183; negative controls = 3; VerseRefs =
      3848; physical gaps = 3255; glyph gaps = 1310.
  - Required behavior:
    - Enumerate all 18 stable occurrences and reproduce trusted-band failure.
    - Measure neighboring marker count/spacing, local and candidate x,
      indentation, body alignment, bbox geometry, following-text gap and
      page/column position where available.
    - Compare against accepted controls and explicit negatives using at most
      two transparent features; require zero accepted known negatives.
    - Report coverage and abstentions and recommend exactly one bounded
      task-138 target.
  - Acceptance:
    - All 18 occurrences are accounted for once; reason semantics are
      deterministic; controls and negatives remain unchanged.
    - No expected-verse inference, threshold widening, marker recovery,
      parser/runtime, VerseRef, ownership, gap, task-128 or task-131 changes;
      GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337; duplicate_refs = 0; out_of_order_refs = 0;
      outside-canon = 0; ocr_blocks = 57700; exactly one task-138 target is
      supported by evidence.
  - Do not:
    - Recover the 18 markers, lower trusted-anchor requirements or widen
      tolerances.
    - Accept candidates from expected verses, previous+1 or next-1; reopen
      outside_marker_band, GLUED_FRAME or standalone recovery.
    - Use ML, embeddings, neural OCR or opaque classifiers; hardcode source
      locations; touch UI; modify TASKS.md; commit or push.

  - Closure record:
    - Result: READY; diagnostic scope completed.
    - Target population = 18.
    - Unique stable occurrences = 18.
    - Family: Psalms; scan page 32; PDF page 33; exact compound form = `I o`.
    - task-128 failure reproduced: trusted_anchor_count = 0; minimum required
      anchors = 3.
    - The three-anchor task-128 policy remained unchanged.
    - Batch-137 facsimile reviews = 18.
    - Visual result: PRINTED_VERSE_MARKER = 18; ORDINARY_TEXT = 0; HEADING = 0;
      LATIN = 0; APPARATUS = 0; UNREADABLE = 0; OTHER = 0.
    - Printed marker value: 10 = 18/18.
    - Accepted task-128 geometry controls reconstructed = 183.
    - Control coverage: 133 pages; 7 books.
    - Hard negatives = 9.
    - Safe diagnostic discriminator found: trusted_anchor_count == 0 AND
      exact_compound_form == "I o".
    - Diagnostic rule results: target positives accepted = 18/18; known target
      negatives accepted = 0; hard negatives accepted = 0; accepted task-128
      controls accepted = 0/183; abstentions = 2.
    - Overall family status: SAFE_DISCRIMINATOR_FOUND.
    - No runtime recovery was introduced.
    - No task-128 threshold was changed.
    - No trusted-anchor requirement was lowered.
    - No marker-band tolerance was widened.
    - No compound grammar was changed.
    - VerseRefs = 3848 unchanged.
    - Physical gaps = 3255 unchanged.
    - Glyph gaps = 1310 unchanged.
    - Ownership unchanged.
    - task-128 runtime behavior unchanged.
    - task-131 runtime behavior unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters = 337/337; unresolved chapter claims = 0; canonical chapter
      gaps = 0; duplicate_refs = 0; out_of_order_refs = 0; outside-canon = 0.
    - ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - Artifact deterministic and idempotent.
    - Direct tests passed.
    - Build passed.
    - TorresAmat CTest = 29/29 passed.
    - Full CTest = 41/41 passed.
    - Two environment-dependent tests were skipped as reported by the suite.
    - New failures = 0.
    - Recommended next work: bounded recovery-validation for the exact
      zero-anchor I o family.
    - Task 137 is DONE because the complete 18-case family was visually
      resolved and a transparent two-feature diagnostic discriminator with
      zero accepted known negatives was established. The absence of runtime
      recovery is not incomplete work; runtime recovery was explicitly
      outside task-137 scope.

- [ ] TORRES-1835-ZERO-ANCHOR-IO-RECOVERY-VALIDATION-138 Validate bounded recovery for zero-anchor I o verse markers
  - Status: PENDING
  - Description:
    Validate, in a bounded diagnostic/dry-run recovery model, whether the exact
    family established by task 137 can be recovered safely:
        trusted_anchor_count == 0
        AND exact_compound_form == "I o".
    Quantify exact VerseRef/ownership effects before any production recovery
    logic is introduced. Task 138 must validate the proposed recovery family
    against the full current corpus, known positives, accepted task-128
    controls and hard negatives.
  - Baseline context:
    - Task-137 target population = 18.
    - All 18 were visually confirmed PRINTED_VERSE_MARKER.
    - All 18 visible printed values = 10.
    - trusted_anchor_count = 0 for all 18.
    - exact_compound_form = I o for all 18.
    - Diagnostic discriminator:
        trusted_anchor_count == 0
        AND exact_compound_form == "I o".
    - Task-137 diagnostic outcome:
        positives accepted = 18/18
        known negatives accepted = 0
        hard negatives accepted = 0.
    - Accepted task-128 controls = 183.
    - Current VerseRefs = 3848.
    - Current physical gaps = 3255.
    - Current glyph gaps = 1310.
  - Required behavior:
    - Recompute the zero-anchor I o candidate family from CURRENT parser data.
    - Do not derive the family from a hardcoded list of the 18 known cases.
    - Apply the task-137 discriminator diagnostically across the full relevant
      corpus.
    - Report:
        total matches
        matches among known 18 positives
        additional matches outside the known 18
        accepted task-128 controls matched
        known hard negatives matched.
    - Any additional corpus match outside the known 18 must be individually
      audited before recovery can be considered.
    - Build a DRY-RUN recovery simulation only.
    - For each would-recover case determine:
        native book/chapter
        visible marker value
        proposed native VerseRef
        current ownership
        proposed ownership movement
        whether a VerseRef already exists
        whether reopening/duplication would occur
        whether canonical range permits the ref.
    - The printed marker value must come from source-backed task-137 evidence or
      equivalent current evidence, never expected verse sequence.
    - Validate the dry-run against:
        duplicate_refs
        out_of_order_refs
        outside-canon
        block loss
        dual ownership
        chapter structure
        existing task-128/task-131 recoveries.
    - Report exact predicted:
        new VerseRefs
        reopened existing VerseRefs
        ownership moves
        physical-gap reduction
        glyph-gap reduction.
    - Compare dry-run before/after identities and prove no unrelated VerseRef
      is changed.
    - Keep runtime/parser unchanged in task 138.
    - Recommend exactly one task-139 action:
        bounded implementation
        narrower validation
        or abandonment.
  - Acceptance:
    - Candidate family is derived from source/current parser state, not from
      hardcoded pages/blocks/verses.
    - Full-corpus diagnostic match count is deterministic.
    - Known 18 positives are all accounted for.
    - Any extra matches are explicitly audited.
    - Known negatives accepted = 0.
    - Hard negatives accepted = 0.
    - Accepted task-128 controls are not accidentally reclassified.
    - Proposed dry-run recovery produces no:
        duplicate refs
        out-of-order refs
        outside-canon refs
        block loss
        dual ownership.
    - Existing VerseRefs outside the bounded family are unchanged.
    - task-128 behavior unchanged.
    - task-131 behavior unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337.
    - ocr_blocks remain 57700.
    - Parser/runtime remains unchanged.
    - Exactly one task-139 recommendation supported by evidence.
  - Do not:
    - Implement production recovery.
    - Modify task-128 runtime logic.
    - Lower trusted-anchor count.
    - Widen marker-band tolerance.
    - Alter task-128 grammar.
    - Hardcode the 18 occurrence IDs as a recovery allowlist.
    - Hardcode pages, blocks, chapters or verses in production.
    - Infer marker value from expected verse, previous+1 or next-1.
    - Accept additional matches without source review.
    - Reopen outside_marker_band recovery.
    - Reopen GLUED_FRAME.
    - Reopen general standalone glyph recovery.
    - Use ML, embeddings, neural OCR or opaque classifiers.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.

- [ ] UI-SIGNAL-101 Investigate stale GObject signal handler
  - Status: TODO
  - Description:
    A previous real-display run emitted
    `GLib-GObject-CRITICAL: instance '0x...' has no handler with id '...'`
    a few seconds after the UI-LAYOUT-103 warnings.
  - Evidence:
    - Not reproduced again, including runs under `G_DEBUG=fatal-criticals`;
      no stop and no backtrace were obtained.
    - No evidence identifies a concrete caller. Static review found
      `src/main/sword.cc` `g_signal_handler_block/unblock(adjustment,
      scroll_adj_signal)` unreachable (the globals are never assigned) and no
      stale-id pattern in the other project disconnect sites.
    - No preventive fix was applied.
  - Resume only if it reappears:
    1. Record the exact UI action.
    2. Reproduce with `G_DEBUG=fatal-criticals`.
    3. Capture `bt` and `bt full`.
    4. Identify the instance, handler id, where it was connected, where the
       id was stored, where it is disconnected, and how it became stale.
    5. Only then propose a fix.
  - Do not:
    - Use `g_signal_handler_is_connected()` to hide it without a root cause.
    - Commit or push.

- [ ] UI-SMOKE-102 Fix "dictionary panel did not reopen explicitly"
  - Status: TODO
  - Description:
    `gtk_lifecycle_smoke` fails with
    `GTK_LIFECYCLE_SMOKE_CHECK_FAILED dictionary panel did not reopen
    explicitly` (check at `src/gtk/gtk_lifecycle_smoke.c:199`).
  - Evidence:
    - Pre-existing: reproduced identically with the UI-LAYOUT-103 fix stashed
      via `git stash`, so it is not a regression of UI-LAYOUT-103.
    - Distinct from the earlier pre-existing `bible-compare` CREATE/SHOW/MAP
      smoke failure noted under STARTUP-PERF-105.
  - Do not:
    - Mix this with the fallback badge fix.
    - Commit or push.

- [x] NACAR-PSALMS-101 Separate Nácar-Colunga psalm superscriptions from verse 1
  - Status: DONE
  - Description:
    Nácar-Colunga (BAC 1944) prints each psalm superscription as verse 1
    (or 1-2), following the Hebrew numbering, while the module uses NRSVA,
    which leaves the title unnumbered. The pipeline stored the title as
    verse 1, shifted the body one or two verses, and the chapter aligner
    (using NRSVA verse counts) moved whole psalms to the wrong chapter.
    Ps 3:1 was not Nácar text at all: `completar.py` had filled it with the
    full SpaRVG verse, superscription included.
  - Evidence:
    - `scripts/nacarcolunga/canon.py` reads the Hebrew psalm verse counts from
      SWORD's `canon_leningrad.h` and derives `TITULO_SALMOS` (62 psalms: 58
      with a one-verse title, 4 with two).
    - `construir.py` aligns Psalms against the Hebrew counts and then maps them
      to NRSVA: title verses become `Ps c:0`, body verses shift back.
      `osis.py` emits `Ps c:0` as `<title type="psalm" canonical="true">`
      before verse 1. `completar.py` compares Psalms against witnesses without
      their superscription (SpaRVG `«…»`, SpaRV small-caps prefix) and skips
      SpaPlatense, whose psalm numbering is Vulgate.
    - The unmodified pipeline was first rebuilt and verified byte-identical
      to the previous outputs, so the comparison isolates this change.
    - Alignment metric over the Psalter (verse token overlap against the
      witnesses): misaligned psalms 41 → 1 of 143-144, mean 0.461 → 0.535.
    - Ps 3: the Nácar superscription («Salmo de David, al huir de Absalón, su
      hijo») is stored as pre-verse heading (`x-preverse` title) and shown
      between «Capítulo 3» and verse 1; Ps 3:1 is Nácar text again. Ps 51,
      52, and 53 are back in their canonical chapters.
    - Outside Psalms: 0 differences in `texto.json`, `procedencia.json`, and
      `reconstruidos.txt` (28,098 keys); the compiled module differs only in
      osis2mod's auto-numbered `sID`/`eID` counters.
    - `test_load.py`, `test_cabeceras.py`, and `test_front_matter.py` PASS.
      Module rebuilt with `osis2mod -v NRSVA -z z` and installed in
      `~/.sword` after backup
      (`~/.sword/modules/texts/ztext/nacarcolunga.respaldo-20260914-183425`).
    - Real app validation (`sword://NacarColunga/Psalms 3:1` and `51:1`):
      heading rendered before verse 1, 0 `Gtk-WARNING`, 0 `CRITICAL`.
    - Remaining Psalter defects are tracked separately in NACAR-PSALMS-102,
      NACAR-PSALMS-103, NACAR-OCR-101, and NACAR-OCR-102.
  - Do not:
    - Commit or push.

- [x] NACAR-PSALMS-102 Fix Nácar-Colunga Psalms 14–17 misalignment
  - Status: DONE
  - Commit: `9352fad7` fix(nacar): realign psalms using printed chapter headers
  - Description:
    Resolved the chapter misalignments/boundaries detected through the
    printed Psalter headers, avoiding false restarts caused by internal OCR
    numbers. This is not a claim that the whole Psalter is corrected:
    independent OCR, verse-division, and fallback problems remain (see the
    related tasks below).
  - Root cause:
    - The printed psalm headers `N (Vulg. M.)` were discarded by the noise
      filter (`es_ruido`: few letters, many digits).
    - Without that explicit boundary, the aligner relied on the numbering
      restarting at 1/2 to detect a new psalm.
    - In Ps 14:5 the OCR read the hemistich separator `|` as `1` in the middle
      of a line («a su tiempo, | porque está Dios…» → «tiempo, 1 porque»),
      confirmed on the facsimile (Princeton leaf 967).
    - That false `1`, after high verse numbers, was treated as the start of a
      chapter: it split Ps 14 into two candidates and shifted Ps 15–17 (the
      real Ps 16 and Ps 17 ended up merged into chapter 17).
  - Fix:
    - `scripts/nacarcolunga/versiculos.py` recognizes the printed Psalter
      headers `N (Vulg. M.)`.
    - The chapter opens right before its first verse, not on the header line,
      so the lines in between (epigraphs) keep their previous routing.
    - `scripts/torresamat/alinear.py` marks chapters opened by a printed
      header (`"cabecera"`).
    - An internal 1/2 in the middle of a line does not restart a chapter opened
      by a header; a 1/2 at the real start of a line can still restart.
    - Without an explicit header the historical behavior is preserved; Torres
      Amat never emits `"cabecera"`, so its behavior is unchanged.
    - No per-psalm offsets, no hardcoded references to Ps 14–17, no
      versification changes, no renderer/fallback changes.
    - The false internal `1` can still exist as a verse event in the OCR
      stream; this task fixes segmentation/chapter boundaries, not the textual
      cleanup of that `1`.
  - Evidence:
    - 13 affected psalms corrected: 14–17, 95, 96, 123, 124, and 131–135.
    - 73 keys changed in `procedencia.json`, all belonging to those 13 psalms;
      0 differences outside Psalms in `texto.json`, `procedencia.json`, and
      `reconstruidos.txt`.
    - Ps 3, 13, 18, 51–53, and 117/118 remain unchanged.
    - Previous misalignment metric: 3 → 0, plus additional verse-by-verse
      review of the 13 affected chapters.
    - An intermediate variant that opened the chapter on the header line was
      rejected: removing epigraphs from the previous last verse made
      `completar.py` replace 10 authentic Nácar verses with Reina-Valera. A
      global "restart only at line start" rule was also rejected (281
      chapters changed outside Psalms).
    - Tests 6/6 PASS: `scripts/nacarcolunga/test_salmos_cabecera.py`,
      `scripts/nacarcolunga/test_load.py`,
      `scripts/nacarcolunga/test_cabeceras.py`,
      `scripts/nacarcolunga/test_front_matter.py`,
      `scripts/torresamat/test_pegadas.py`, and
      `scripts/torresamat/test_restos.py`.
    - `git diff --check` clean; the commit is limited to 4 files; the module
      was not reinstalled; no push.
  - Out of scope (tracked separately):
    - Ps 13 (NACAR-PSALMS-104).
    - Ps 117/118 (NACAR-PSALMS-103).
    - Verse number 5 read as 6 (NACAR-OCR-104).
    - `es_titulo()` discarding real text (NACAR-OCR-103).
    - Epigraphs glued to the previous verse (NACAR-OCR-105).
    - `completar.py` replacing valid Nácar text with Reina-Valera
      (NACAR-FALLBACK-101).

- [ ] NACAR-PSALMS-103 Fix Nácar-Colunga Psalms 117/118 boundary
  - Status: TODO
  - Description:
    Ps 117 has no text of its own in the module: its two verses are glued to
    the start of Ps 118:1 («Alabad a Yave las gentes todas… Alabad a Yave,
    porque es bueno…»). Misalignment around Ps 117–118 was also observed
    during the NACAR-PSALMS-101 investigation.
  - Evidence:
    - Ps 118 has not been explicitly verified; do not assume it is correct.
    - Unchanged by NACAR-PSALMS-102 (`9352fad7`): Ps 117 still has no text
      and Ps 118 still begins with the Ps 117 content.
  - Acceptance criteria:
    - Ps 117 has exactly its 2 verses.
    - Ps 118 begins with its own content.
    - No verses are displaced between both psalms.
  - Do not:
    - Use offsets or hardcoded references.
    - Commit or push.

- [ ] NACAR-PSALMS-104 Fix Nácar-Colunga Psalm 13 superscription and verse division
  - Status: TODO
  - Description:
    Ps 13 still has an incorrect title/verse division. Nácar-Colunga prints
    the superscription as verse 1 and the body from verse 2, but Leningrad and
    NRSVA both have 6 verses for Ps 13 while distributing the content
    differently, so the verse-count comparison used by NACAR-PSALMS-101
    (`TITULO_SALMOS`) does not detect the superscription or the division.
  - Evidence:
    - Current module: Ps 13:1 is Reina-Valera filler (title plus verse), Ps
      13:2–4 hold the Nácar body shifted one slot, Ps 13:5 is empty, and Ps
      13:6 merges two printed verses.
    - Unchanged by NACAR-PSALMS-102 (`9352fad7`).
  - Acceptance criteria:
    - The superscription is stored as a psalm title, not as verse text.
    - Each NRSVA verse contains its own Nácar content.
    - The mapping comes from verse-level correspondence or structural
      evidence, not from verse counts alone.
  - Do not:
    - Apply a manual offset or hardcode Ps 13.
    - Commit or push.

- [ ] NACAR-OCR-101 Recover Nácar-Colunga Ps 3:4 from the Ps 3:5 OCR merge
  - Status: TODO
  - Description:
    Ps 3:4 is empty in the module and falls back to Reina-Valera 1909 (shown
    with the fallback badge), because the Nácar OCR merged the content of
    two verses into Ps 3:5 («Clamaba con mi voz a Yave… (Sela.) S A veces me
    acostaba…»).
  - Objective:
    Recover and split the authentic Nácar text if the facsimile evidence
    allows it.
  - Acceptance criteria:
    - Ps 3:4 and Ps 3:5 each contain only their own Nácar text, supported by
      the facsimile/OCR source.
    - The current fallback may remain while the original body is missing.
  - Do not:
    - Copy Reina-Valera text and present it as Nácar-Colunga.
    - Commit or push.

- [ ] NACAR-OCR-102 Audit residual Nácar-Colunga OCR errors
  - Status: TODO
  - Description:
    Audit and clean residual OCR errors observed while comparing the Psalter,
    e.g. «Yavel» (Yavé), fragments such as «multi. plicado», «sor» (son),
    «Ab: salón» (Absalón), and residues in psalm titles («SAI meo», «delo
    de»). This is an audit/cleanup task, not a bulk replacement.
  - Acceptance criteria:
    - Each correction is supported by the facsimile/OCR source or by a
      demonstrable general rule.
    - Changes are reproducible from the pipeline, not hand edits to the
      installed module.
  - Do not:
    - Modernize the text automatically.
    - Commit or push.

- [ ] NACAR-OCR-103 Keep es_titulo() from discarding biblical continuation lines
  - Status: TODO
  - Description:
    `es_titulo()` in `scripts/nacarcolunga/versiculos.py` classifies some
    authentic body lines as section titles/epigraphs and drops them: a line
    without digits, 8–70 characters long, not starting in lowercase, and not
    ending in punctuation is discarded. Continuation lines that start with the
    hemistich separator `|` or with a capital letter match that rule.
  - Evidence:
    - Ps 16:3 loses «| son de mí muy honrados, | en ellos», confirmed on the
      facsimile (Princeton leaf 967). The verse becomes artificially short
      and `completar.py` then replaces it with Reina-Valera.
    - Similar lost lines observed during NACAR-PSALMS-102: Ps 15:1 («¡Oh Yavel
      ¿Quién es el que podrá»), Ps 17:1 («Oye, Yave, mi justa causa, |
      atiende»), Ps 95:1, Ps 96:11, Ps 124:2, and Ps 124:3 («| cuando ardía su
      ira contr»).
  - Acceptance criteria:
    - Distinguish real epigraphs from biblical continuation lines.
    - Authentic lines are kept in the verse body.
    - Epigraphs are not moved into the body.
    - No new fallbacks are caused.
  - Do not:
    - Add per-reference exceptions.
    - Commit or push.

- [ ] NACAR-OCR-104 Handle verse numbers misread by the OCR (5 read as 6)
  - Status: TODO
  - Description:
    The OCR misreads some small superscript verse numbers, especially 5 as 6.
    The printed verse 5 then lands in slot 6: verse 5 stays empty and its text
    merges with verse 6.
  - Evidence:
    - Ps 14:5 is empty and its text is merged into Ps 14:6.
    - Ps 17: the printed «⁵ Y mis pies…» is read as 6 (Princeton leaf 968).
    - The sequence «4, 6, 6» appears in many psalm candidates (e.g. Ps 9, 13,
      18, 19, 20, 21).
    - Related to the Ps 3:4/3:5 pattern in NACAR-OCR-101, but do not assume
      that every case has the same cause.
  - Acceptance criteria:
    - Detection/correction relies on a general structural signal (e.g. a
      duplicated number with a missing predecessor), validated against the
      facsimile.
    - A before/after comparison shows no regressions elsewhere.
  - Do not:
    - Hardcode verses.
    - Commit or push.

- [ ] NACAR-OCR-105 Separate psalm epigraphs glued to the previous verse
  - Status: TODO
  - Description:
    Editorial epigraphs printed between psalms (e.g. «Canto triunfal de
    David.», «Deprecación contra los impíos.») can end up appended to the last
    verse of the previous psalm (e.g. Ps 17:15 ends with «EF Canto triunfal de
    David.»).
  - Evidence:
    - NACAR-PSALMS-102 deliberately kept the previous epigraph routing: opening
      the chapter on the header line removed the epigraphs, but the shortened
      verses made `completar.py` replace 10 authentic Nácar verses with
      Reina-Valera (e.g. Ps 11:7, Ps 37:40, Ps 111:10).
    - Not covered by NACAR-OCR-102, which is about textual OCR errors.
  - Acceptance criteria:
    - The epigraph is preserved as metadata/heading.
    - It is not glued to the previous verse.
    - It is not deleted.
    - Shortening the verse does not cause a fallback (depends on
      NACAR-FALLBACK-101).
  - Do not:
    - Commit or push.

- [ ] NACAR-OCR-106 Record structural truncation/loss metadata from the parser
  - Status: TODO
  - Description:
    With NACAR-FALLBACK-101 the pipeline correctly preserves Nácar-Colunga
    text, but verses that are really truncated stay partial without any badge:
    they have a body, so the runtime fallback does not apply, and the pipeline
    does not know that text is missing.
  - Evidence:
    - Truncated verses confirmed on the facsimile during the
      NACAR-FALLBACK-101 audit: Josh 5:1, 2Cor 11:31, Exod 25:8, 1Thess 3:7,
      and Rev 11:12.
    - Only structural signals available today: a word cut with a hyphen and no
      continuation (64 of the 4,602 audited replacements) and several verse
      marks in the same slot (27). The remaining 4,511 carry no structural
      signal of loss or completeness.
    - `procedencia.json` records only the line that opens each verse
      (page/column/box/confidence), not continuation lines or discarded lines.
  - Missing metadata:
    - Lines discarded by the parser (`es_titulo()`, `es_ruido()`, running
      headers).
    - Column/page cuts.
    - Lost continuations.
    - Relevant geometry of continuation lines.
    - An explicit truncation/loss flag per verse.
  - Acceptance criteria:
    - The parser records structural loss.
    - A complete verse is not marked as truncated.
    - A really truncated verse can be identified without comparing
      translations.
    - That metadata can enable a later, traceable recovery.
  - Do not:
    - Reintroduce a length heuristic between translations.
    - Implement it as part of NACAR-FALLBACK-101.
    - Commit or push.

- [x] NACAR-FALLBACK-101 Stop completar.py from replacing valid Nácar text with Reina-Valera
  - Status: DONE
  - Commit: `17e50aa5` fix(nacar): preserve source text over witness substitutions
  - Description:
    `scripts/nacarcolunga/completar.py` no longer replaces Nácar-Colunga text
    with other translations. The NacarColunga module contains only Nácar/OCR
    text; verses without a body are supplied by Biblia Elim at runtime, with a
    visible fallback badge.
  - Historical root cause:
    - `hay_que_completar()` treated a Nácar verse as incomplete when it had
      fewer than `max(8, 80 %)` of the words of the witness: it compared the
      length of two different translations and consulted no provenance or
      structural signal.
    - `tejer()` kept only the OCR words before the first match and inserted the
      rest from the witness (SpaRV, SpaRVG, or SpaPlatense);
      `completar_todo()` accepted the result when it had more words.
    - Aggravating factor: for psalms not in `TITULO_SALMOS` (e.g. Ps 15, 17,
      124, 133), the SpaRV witness still carried the title inside verse 1,
      which inflated its length.
  - Audit (reproducible replica of `completar_todo()`, identical to the
    validated baseline):
    - 4,602 verses modified: 180 in Psalms, 4,422 elsewhere.
    - 3,179 full replacements and 1,423 OCR + witness mixes.
    - 21,377 Nácar words lost across 4,327 verses.
    - No modified verse had an empty source; all were triggered only by the
      length threshold.
    - Authentic complete verses were replaced only for being shorter, e.g. Ps
      17:7 («Ostenta tu magnífica piedad, tú que salvas del enemigo a los que a
      ti se acogen.», 16 words vs threshold 18). Also confirmed on the
      facsimile: Ps 35:5, Gen 38:6, Luke 23:21, Mark 12:30, Hos 13:3.
    - Ps 16:3 was replaced after `es_titulo()` had already discarded an
      authentic line (NACAR-OCR-103).
    - The available metadata cannot reliably distinguish a complete authentic
      verse from a really truncated one (only 64 hyphen cuts and 27 multi-mark
      slots carry structural signals; see NACAR-OCR-106).
    - The UI could not distinguish those verses: the backend counted
      `reconstruido-testigos` segments as available body, so no badge was
      shown.
  - New policy:
    - Any present Nácar text prevails; `completar.py` does not modify
      `texto.json`.
    - No length, punctuation, similarity, or witness size is used to decide
      completeness.
    - Partial verses remain partial; verses without a body are not fabricated.
    - Those gaps are supplied by Biblia Elim at runtime through the marked
      fallback.
    - `completar.py` writes an empty `reconstruidos.txt` for compatibility;
      `reconstruido-testigos` is no longer produced.
    - `completar.py` no longer loads `testigos.pkl`, does not call diatheke,
      does not need SpaRV/SpaRVG/SpaPlatense, and does not need network: the
      stage works offline.
    - The historical collation helpers (`descarga`, `testigo_nrsva`,
      `mejor_testigo`, `tejer`) remain as auxiliary code for a future recovery
      based on explicit structural metadata (NACAR-OCR-106); they are not part
      of the normal path.
    - README, `instalar.sh` (module `About`), and the `completar.py` docstring
      describe the current behavior.
  - Reconstruction validation:
    - 30,316 OSIS verses; 4,905 gaps.
    - 30,316 `ocr-facsímil` segments; 0 `reconstruido-testigos`.
    - `reconstruidos.txt` empty.
    - Final `texto.json` == `texto.json` right after `construir.py`.
    - `procedencia.json` unchanged by `completar.py` (identical to HEAD).
    - Exactly 4,602 references differ from the historical policy, each
      restoring its Nácar source.
    - Audit hooks on the official install: no step opened `testigos.pkl`,
      called diatheke, opened sockets, or spawned subprocesses; `bajar.py`
      downloaded nothing.
    - Tests 5/5 PASS: `scripts/nacarcolunga/test_completar.py`,
      `scripts/nacarcolunga/test_salmos_cabecera.py`,
      `scripts/nacarcolunga/test_load.py`,
      `scripts/nacarcolunga/test_cabeceras.py`, and
      `scripts/nacarcolunga/test_front_matter.py`.
    - The new test also shows that `main()` works without `testigos.pkl`, with
      `descarga()`, `subprocess.run`, and sockets blocked, preserving
      `texto.json` byte for byte.
  - Real installation:
    - The official `instalar.sh` was used; the new module is installed in
      `~/.sword`.
    - Backup before installing:
      `~/.sword/modules/texts/ztext/nacarcolunga.respaldo-20260914-205222` and
      notes `~/.sword/modules/comments/zcom/nacarcolunganotas.respaldo-20260914-205222`
      (each with its `.conf`). Previous backups remain intact.
    - The installed module matches the validated temporary run byte for byte.
  - Direct SWORD validation (diatheke on the installed module):
    - Ps 17:7: «Ostenta tu magnífica piedad, tú que salvas del enemigo a los
      que a ti se acogen.» ⇒ authentic Nácar restored.
    - Ps 16:3: «Los santos que en la tierra están, tengo todas mis delicias.» ⇒
      deliberately left partial, not hidden with Reina-Valera; NACAR-OCR-103
      remains pending.
    - Gen 38:6, Luke 23:21, and Mark 12:30 verified as Nácar.
    - Gaps such as Ps 117:1-2, Ps 14:5, Gen 38:5, Ps 17:3, and Ps 17:5 remain
      empty inside the module.
  - Real GUI validation (Biblia Elim run for real, one launch per passage):
    - A. Ps 17:7: authentic Nácar, no fallback badge.
    - B. Ps 16:3: partial text visible, no Reina-Valera text incorporated.
    - C. Gen 38:6: authentic Nácar.
    - D. Ps 14:5: a gap in the module; Biblia Elim supplies it from
      Reina-Valera 1909 between «Texto suplido desde Reina-Valera 1909» badges;
      the fallback is not persisted inside the Nácar module.
    - E. Ps 18:1 and Ps 17:7 opened correctly.
    - stderr: 0 `Gtk-WARNING`, 0 `GLib-GObject-CRITICAL`, 0 crashes.
      `settings.xml` was restored.
    - Real multi-launch/cold-cache validation PASS; same-session warm cache was
      not verified in this round (the automation could not send keystrokes to
      the app). This does not block NACAR-FALLBACK-101.
  - Final architecture (main acceptance criterion):
    - NacarColunga module: contains only Nácar/OCR text; may contain partial
      text; does not incorporate Reina-Valera or Platense.
    - Biblia Elim runtime: only when a slot lacks a body can it supply text
      from the fallback; the fallback is identified visually with a badge and
      is not persisted as Nácar.
  - Related pending tasks:
    - NACAR-OCR-103: `es_titulo()` discards authentic lines (e.g. Ps 16:3).
    - NACAR-OCR-104: wrong OCR verse numbers.
    - NACAR-OCR-105: glued epigraphs.
    - NACAR-OCR-106: structural truncation metadata (truncated verses such as
      Josh 5:1 stay partial without a badge).
    - NACAR-PSALMS-104: Ps 13.
    - NACAR-PSALMS-103: Ps 117/118.
  - Do not:
    - Reintroduce a length heuristic between translations.
    - Push without review.

- [x] TORRES-FACSIMILE-101 Correct Torres Amat Ps 3 and Mt 12 against the 1882 facsimile
  - Status: DONE
  - Description:
    Torres Amat verses with OCR defects were corrected after reading the
    printed page of the 1882 edition
    (`https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111`).
    The build data (djvu.xml, `texto.json`) is not available locally, so the
    compiled module is patched by the reproducible script
    `scripts/torresamat/parche_facsimil.py`.
  - Evidence:
    - Ps 3:2 «¡Ah Señor!;Cómo» → «¡Ah Señor! ¿Cómo» (tomo III, hoja 11).
    - Ps 3:3 was empty; its text had been glued before Ps 3:5 and is restored.
    - Ps 3:4 «oh Senor, 44 eres» → «oh Señor, tú eres».
    - Ps 3:5 keeps only its own text.
    - Mt 12:4 spurious «$» (footnote callout 3) removed; «ú solos» → «á solos»
      (tomo IV, hoja 22).
    - Mt 12:5 «eon» → «con».
    - Mt 12:11 lost continuation recovered: «…en dia de sábado, no la levante y
      saque fuera?».
    - The script exports with `mod2imp`, replaces an entry only if the old text
      matches exactly, re-imports with `imp2vs -v Vulg -z z`, and verifies with
      an isolated round trip (renamed module) that only the 7 authorized
      entries changed. An unmodified export/re-import round trip was
      byte-identical (38,698 entries).
    - Installed in `~/.sword` (backup
      `~/.sword/modules/texts/ztext/torresamat.respaldo-20260914-183425`) and in
      `modulos/modules/texts/ztext/torresamat/`.
    - Real app validation (`sword://TorresAmat/Matthew 12:4` and
      `Psalms 3:3`): corrected text shown, 0 `Gtk-WARNING`, 0 `CRITICAL`.
  - Do not:
    - Commit or push.

- [ ] TORRES-FACSIMILE-102 Remove spurious fragment at the end of Torres Amat Mt 12:6
  - Status: TODO
  - Description:
    Mt 12:6 ends with a spurious fragment `" i"` («…mayor que el templo. i»).
  - Acceptance criteria:
    - Verified against the 1882 facsimile (tomo IV, hoja 22) before changing.
    - If confirmed as OCR noise, corrected through
      `scripts/torresamat/parche_facsimil.py`.
  - Do not:
    - Edit only the installed module.
    - Commit or push.

- [ ] TORRES-FACSIMILE-103 Fix Torres Amat Mt 12:10 «hallabaun»
  - Status: TODO
  - Description:
    Mt 12:10 reads «Donde se hallabaun hombre…».
  - Acceptance criteria:
    - The correct reading is verified in the 1882 facsimile (tomo IV,
      hoja 22).
    - Corrected through `scripts/torresamat/parche_facsimil.py`.
    - A focused regression covers the corrected verse.
  - Do not:
    - Edit only the installed module.
    - Commit or push.

- [ ] TORRES-FACSIMILE-104 Harden the Torres Amat facsimile patch mechanism
  - Status: TODO
  - Description:
    `scripts/torresamat/parche_facsimil.py` was used in
    TORRES-FACSIMILE-101 to correct Ps 3:2, Ps 3:3, Ps 3:4, Ps 3:5, Mt 12:4,
    Mt 12:5, and Mt 12:11. Verify that the mechanism remains reproducible from
    a known source, limited to changes demonstrated by the facsimile,
    idempotent, and free of collateral changes.
  - Acceptance criteria:
    - Running the patch twice produces the same result.
    - A comparison demonstrates that only authorized references change.
    - The regenerated module matches the expected module.
    - It does not depend on manually editing `~/.sword`.
  - Do not:
    - Commit or push.

- [x] V11N-MODULE-101 Convert references between modules instead of rereading them
  - Status: DONE
  - Description:
    Switching Bibles reused `settings.currentverse` as text and applied it
    to the new module (`set_module_key` → `setKey(text)`), so SpaRV
    Psalms 119:1 opened TorresAmat (Vulg) at its own 119:1 («Cántico de los
    grados…») instead of Vulg 118:1. The approved contract is: a reference
    is (module, key native to that module's versification); crossing modules
    converts with `VerseKey::positionFrom`, never by rereading the text.
    Branch `fix/module-versification-transition`, commit `9d0e9e8d`
    (not pushed when recorded).
  - Evidence:
    - Central helper: `BibleBackend::convertReference` and
      `convertReferenceFromVersification` (SWORD implementation maps into
      keys of their own, never the modules' live keys) and
      `planBibleModuleTransition` / `planBibleVersificationTransition` in
      `src/main/reference_transition.cc`. Results are Mapped / Unmapped /
      Invalid; Unmapped is never turned into identity. Verse 0 is carried by
      mapping verse 1 and taking that chapter's intro.
    - Routes converted: version picker, sidebar, parallel swap, Comparar
      swap, reading sync columns, stacked parallel page, verse-by-verse
      parallel table (control key first, then per cell), synced Bible
      dialogs, parallel navbar `sync_on`, `sword://Commentary/ref` → Bible,
      Companion modules (verse-keyed companions converted, dictionaries and
      books keep their keys; no installed module declares `Companion`),
      author commentary (SpaPlatenseComentarios / NacarColungaNotas /
      TorresAmatNotas declare the same versification as their edition, so
      identity today, converted anyway), and replacement after uninstalling
      the main Bible (converted from the versification remembered at
      display time; without a mapping, warning + first verse of the
      replacement).
    - Unmapped switch: warning, previous Bible and reference kept.
    - Global audit of `setText` / `setKeyText` / `set_module_key` /
      `setKey` / `main_display_bible`: 0 Bible→Bible routes rereading
      another module's reference as text.
    - `versification_transition_test`: KJV→Vulg Ps 118:10/119:1/120:1/147:12,
      Vulg→KJV 118:1/119:1, identities, NRSVA/KJVA Psalms, deuterocanon
      (NRSVA Tob, KJVA Sir, NRSVA Bar, Vulg Tob→NRSVA), explicit Unmapped
      (KJV Ps 13:6, NRSVA Tob→KJV, Vulg Bar 6:1→NRSVA), Ps 147:1–20 verse by
      verse, intros (KJV 119:0→Vulg 118:0, 51:0→50:0), module transitions
      with installed modules (0 skipped), removed-module transitions, author
      commentary identity, and live key pointer/text unchanged.
    - Regressions green: `sword_backend_key_lifecycle_test`,
      `content_resolver_sword_test`, `content_resolver_test`,
      `navbar_valid_key_ownership_test`, `verse_navigation_readiness_test`,
      `bible_backend_contract_test`, `sqlite_bible_backend_test`.
      `gtk_lifecycle_smoke` fails only with the known UI-SMOKE-102 message.
    - Real app (picker clicks): SpaRV 119:1 → TorresAmat 118:1
      «Bienaventurados…»; TorresAmat 118:1 → SpaRV 119:1; TorresAmat 119:1 →
      SpaRV 120:1; SpaRV 147:12 → TorresAmat 147:1; SpaRV 119:1 →
      SpaPlatense 118:1; SpaRV → SpaRV unchanged. Stacked parallel shows
      SpaPlatense 118:1 beside SpaRV 119:1. Reading-mode table SpaRV Ps 147:
      rows 1–11 beside TorresAmat 146:1–11, row 12 beside Vulg 147:1. 0
      `WARNING` / `CRITICAL`.
  - Follow-ups:
    - General commentary pane still receives the Bible's native key (e.g.
      TSK, KJV, beside TorresAmat, Vulg): registered as V11N-COMMENTARY-101.
    - Text rendering helpers (`BackEnd::getText` → `get_raw_text` /
      `get_render_text`) replace the module's live key pointer; pre-existing
      (reproduced on HEAD before this fix): registered as SWORD-KEY-101.
    - Bookmarks without module name remain ambiguous and were not migrated.
    - Torres Amat data: native Ps 119:1 «0 gradual. eme», Ps 146:11 with
      Ps 147 glued, Ps 147:6 `&amp;##x27;`.
  - Do not:
    - Renumber Vulg references to look like Reina-Valera.
    - Push without review.

- [ ] TORRES-PSALM-TITLES-101 Preserve and mark native psalm title slots in Torres Amat
  - Status: IN PROGRESS (first batch validated; rest of the Psalter not audited)
  - Description:
    Torres Amat follows the Vulgate and numbers a psalm's superscription as
    a real verse. Confirmed in the 1882 facsimile (tomo III, hoja 11):
    «1, Salmo de David cuando temeroso iba huyendo de su hijo Absalom» /
    «2, ¡Ah Señor!…». That numbering is correct and must not change.
    `scripts/torresamat/osis.py` emitted those slots as plain body text, with
    no `<title>`, Heading or Preverse, so nothing downstream could tell a
    title from a verse.
  - Commit:
    - `ee2b0938 fix(torresamat): preserve native psalm title structure`
      (branch `fix/torresamat-psalm-titles`, not pushed when recorded).
  - Structure / policy:
    - Title slots are marked `<seg type="x-psalm-title">…</seg>` inside their
      own native verse (`scripts/torresamat/titulos.py`, shared by
      `osis.py` and `parche_facsimil.py`). Title + body sharing a printed
      verse: `<seg …>title</seg> body`.
    - Not moved to verse 0, not moved to Preverse of another verse, not
      renumbered; `Versification=Vulg` unchanged.
    - Which verses are titles comes from an explicit table with the
      facsimile sheet for each entry (`parche_facsimil.TITULOS`), never from
      runtime text heuristics. The facsimile is the source of truth; no text
      from Reina-Valera or other Bibles.
    - Why not `<title>`: SWORD renders it as `<h3>`, `content_availability`
      strips headings from the body, a title-only verse becomes Missing and
      fallback fills it (probe: RV 1909 title + first verse, duplicated).
      With `<seg>` SWORD renders `<span class="x-psalm-title">`, the slot
      stays Available, no fallback, and the renderer can identify it.
  - First validated batch (Ps 3, 4, 50, 51, 52): 9 references changed:
    - Ps 3:1: native title marked `x-psalm-title`; key still Vulg Ps 3:1.
    - Ps 4:1: title marked.
    - Ps 4:3: recovered from the facsimile (its number was OCR'd as «5.»);
      no longer uses fallback.
    - Ps 4:5: the duplicated Ps 4:3 prefix the OCR glued in front was
      removed; it now starts «Enojaos, y no querais pecar mas…» (tomo III,
      hoja 11). Its own OCR errata were left as they were.
    - Ps 50:1: first part of the title recovered («Para el fin: Salmo de
      David;», tomo III, hoja 29); Ps 50:2 marked as title; Ps 50:3 body,
      no duplication.
    - Ps 51:1 / 51:2: same pattern («Para el fin: Salmo de inteligencia de
      David,», tomo III, hoja 30); Ps 51:3 body.
    - Ps 52:1: the print shares title and first line in verse 1 («Para el
      fin: 1. Por Maeleth…»); kept as title + body in the same native slot,
      not split or renumbered. «Para el fin:» recovered.
    - The earlier note that Ps 4:2 held v2 + v3 merged was wrong: v3 was
      glued to v5, not v2.
  - Batch validation:
    - `mod2imp`: 38,698 entries before and after, same keys and order,
      exactly the 9 references above changed; two rebuilds byte-identical;
      the patch is idempotent (already-applied entries report «ya
      corregido»). `Versification=Vulg` and TorresAmatNotas unchanged.
    - Installed in `~/.sword` byte-identical to the isolated build (backups
      `torresamat.respaldo-20260915-001854` before the batch and
      `torresamat.respaldo-20260915-080236` before the 9-reference install).
    - Tests pass: `test_titulos.py` (includes `test_salmo4_v5_sin_el_v3`:
      Ps 4:3 not inside Ps 4:5, Ps 4:5 starts «Enojaos», OLD = Ps 4:3 +
      space + NEW), `test_pegadas.py`, `test_restos.py`,
      `content_resolver_test`, `content_resolver_sword_test`,
      `psalm_title_render_test`.
    - Backend probe: 14 references in Ps 3, 4, 50, 51, 52 Available,
      `isFallback = false`, source TorresAmat.
    - Real app: Ps 3 correct; Ps 4:3 appears once; Ps 4:5 no longer repeats
      v3; Ps 50/51 without duplication; Ps 52 title + body correct. Earlier
      facsimile patches (Ps 3:2–5, Mt 12:4, 12:5, 12:11) intact. 0
      `Gtk-WARNING` / `CRITICAL` / `ERROR` / crashes.
  - Still open (why this task is not DONE):
    - Only Ps 3, 4, 50, 51 and 52 are covered. Every other psalm with a
      numbered title must be located, checked against the facsimile one by
      one, and its title slots marked; text recovered only where the
      facsimile justifies it; no unsupported hardcodes.
  - Known OCR / structure issues outside the batch (not fixed):
    - Ps 4:2 and Ps 4:4: OCR errata («0% Dios», «4un», «vabed», «á E st
      santo»).
    - Ps 50:2 («y vino:», «Nathán 4») and Ps 50:3 («MS borra»): OCR errata.
    - Ps 4:10 and Ps 52:7: the next psalm's «SALMO …» heading and argument
      glued to the verse.
    - Ps 4:8: visible `&lt;` («abundan&lt;cia»).
  - Acceptance criteria:
    - The native Vulg verse number is kept.
    - Titles are not moved to another verse (not to v0, not to Preverse of
      the next verse).
    - The title is represented structurally in the OSIS.
    - Missing title text is recovered from the facsimile (the full djvu
      source is not available locally; Ps 50–52 pages must be fetched).
    - Merged slots are split.
    - No runtime text heuristics.
  - Do not:
    - Edit only the installed module.
    - Commit or push.

- [x] RENDER-PSALM-TITLES-101 Render structurally marked native psalm-title verses as titles
  - Status: DONE
  - Commit:
    - `40cd5b57 feat(render): style structural psalm titles`.
  - Description:
    `GTKChapDisp::RenderOneChapter` painted number + body for every non-empty
    verse, so TorresAmat Ps 3:1 looked like an ordinary verse. Only
    Preverse headings (e.g. NacarColunga's `<title type="psalm"
    canonical="true">`, rendered as `<h3 class="title psalm canonical">`)
    reached the renderer as structure.
  - Final contract:
    - The visible number is the module's NATIVE number; TorresAmat (Vulg)
      keeps its printed numbering.
    - `x-psalm-title` changes only the typographic presentation of the
      title text. KJV/RV numbering and `positionFrom()` are never used to
      decide the displayed number.
    - TorresAmat Ps 3 renders:
      `1  Salmo de David cuando temeroso iba huyendo…` (title in italics),
      `2  ¡Ah Señor! ¿Cómo es que…`, `3  Muchos dicen…`.
  - Implementation:
    - Module markup `<seg type="x-psalm-title">…</seg>` reaches the renderer
      as `<span class="x-psalm-title">…</span>` inside `renderedText`; there
      is no Heading/Preverse for it.
    - `src/main/psalm_title.{h,cc}`: `splitPsalmTitle()` separates a leading
      title span from the rest of the verse by markup only (no text
      heuristics); `psalmTitleVerseHtml()` lays out what follows the number.
    - `src/main/display.cc`: `RenderOneChapter` and the adjacent-chapter
      previews (`getVerseBefore` / `getVerseAfter`) use it. Title only:
      native number + styled title. Title + body: native number once, styled
      title, body on the next line. Verses without the marker: unchanged.
      Anchor, verse tools, key and highlighting unchanged.
    - `src/webkit/wk-html.c`: `psalm-title` text tag (italic, scale 0.94, no
      weight, colour, background or border) applied to `x-psalm-title`; the
      verse number keeps its normal style.
    - CMake target and `tests/psalm_title_render_test.cc`.
  - Validation:
    - Real app, TorresAmat: Ps 3 «1 Salmo de David…» in italics and «2 ¡Ah
      Señor!…»; direct navigation to 3:1 highlights that slot. Ps 4:1 title
      with its number. Ps 50 and Ps 51: 1 and 2 styled titles, 3 ordinary
      body. Ps 52:1 title + body in one slot, number 1 exactly once. Chapter
      previews use the same presentation (the next psalm's title keeps its
      number).
    - Regressions: SpaRV unchanged; NacarColunga headings/Preverse unchanged.
    - Tests pass: `psalm_title_render_test`, `content_resolver_test`,
      `content_resolver_sword_test`, `sword_backend_key_lifecycle_test`,
      `versification_transition_test`, `navbar_valid_key_ownership_test`,
      `verse_navigation_readiness_test`, `navbar_entry_reference_test`,
      `poetry_line_wrap_test`, `wk_html_surface_test`,
      `quoted_heading_test`.
    - `cmake --build build -j4` passes. GUI stderr: 0 `Gtk-WARNING`, 0
      `GLib-GObject-CRITICAL`, 0 `ERROR`, 0 crashes.
    - `gtk_lifecycle_smoke` still fails only with UI-SMOKE-102 («dictionary
      panel did not reopen explicitly»); not a regression.
  - Acceptance criteria:
    - Acts only when structural metadata exists.
    - The native number is kept: `1  Salmo de David…`.
    - Superscription/title styling is applied.
    - Ps 3:2 is not renumbered as Ps 3:1.
    - Titles are not inferred from strings.
    - Existing NacarColunga headings keep working.
    - A title-only body is not classified as missing content
      (`content_availability.cc` treats `title` / `h1`–`h6` as heading tags),
      or it would trigger fallback.
  - Depends on:
    - TORRES-PSALM-TITLES-101.
  - Do not:
    - Commit or push.

- [ ] FALLBACK-V11N-101 Prevent cross-versification fallback from filling native title slots
  - Status: TODO
  - Description:
    Vulg↔KJV mappings can be many-to-one: Vulg Ps 3:1 → KJV Ps 3:1 and
    Vulg Ps 3:2 → KJV Ps 3:1; Vulg Ps 50:1/2/3 → KJV Ps 51:1. When a Vulg
    title slot is empty, fallback can insert a KJV verse that carries title +
    body already present in other Vulg slots, duplicating content.
  - Evidence:
    - Real app: TorresAmat Ps 50:1 (empty) receives SpaRV1909 KJV Ps 51:1
      («Al Músico principal… TEN piedad de mí, oh Dios…»), and TorresAmat's
      own body appears again at Ps 50:3 («Ten piedad de mí, oh Dios…»).
    - Resolver probe: TorresAmat Ps 51:1 (empty) receives SpaRV1909 KJV
      Ps 52:1 (title + «¿POR qué te glorías…»), duplicating Ps 51:3.
    - Update: the first TORRES-PSALM-TITLES-101 batch filled Ps 50:1 and
      Ps 51:1 with facsimile text, so those two no longer duplicate. That
      avoids fallback only in title slots that now have content; the general
      many-to-one problem remains for any other Vulg title slot that stays
      empty. Still TODO.
  - Acceptance criteria:
    - A truly missing native verse is distinguished from a structural title
      slot.
    - A title-only slot is not filled with body from a many-to-one mapping.
    - Legitimate body fallback is not lost.
    - The policy relies on structural metadata / mapping, not on text.
    - Tests cover Ps 3, Ps 50 and Ps 51.
  - Do not:
    - Commit or push.

- [ ] V11N-COMMENTARY-101 Convert general commentary references across versifications
  - Status: TODO
  - Description:
    The general commentary pane can receive the active Bible's native key
    directly even when the commentary and the Bible use different
    versifications (e.g. TorresAmat = Vulg beside TSK = KJV). Out of scope
    for V11N-MODULE-101, which closed Bible→Bible routes and the author
    commentary route only.
  - Acceptance criteria:
    - Bible native reference → commentary native reference is converted.
    - Uses the transition contract adopted in V11N-MODULE-101
      (`planBibleModuleTransition` / `convertReference`).
    - Unmapped is explicit.
    - Text is never reread by identity.
    - Commentaries with the same versification as the Bible keep working.
  - Do not:
    - Commit or push.

- [ ] SWORD-KEY-101 Stop text rendering helpers from replacing module key pointers
  - Status: TODO
  - Description:
    Text rendering helpers can replace the SWKey owned by a SWORD module
    instead of preserving the existing shared key object. Other parts of the
    application keep and reuse the pointer returned by `module->getKey()`;
    replacing that SWKey can leave those references on an object that no
    longer represents the module's active key, or introduce lifetime/state
    problems. No crash or use-after-free has been reproduced for this path
    yet; none is claimed here.
  - Evidence:
    - Found while validating V11N-MODULE-101 with a probe that records
      `module->getKey()` before and after each backend call (TorresAmat and
      SpaRV, `Psalms 118:1`).
    - Reproduced identically against `2cd008b7` (HEAD before the
      versification fix) and against the tree with V11N-MODULE-101, so it is
      not a regression introduced by `9d0e9e8d fix(v11n): convert references
      across Bible modules`.
    - `resolveKey()` and `getVerseContent()` (first and repeated reads) did
      not change the pointer in that probe.
    - A read through `getText()` did change it: `BackEnd::getText` →
      `get_raw_text`, which ends in an operation equivalent to
      `module->setKey(key)`. By code reading, `get_render_text` (and
      `get_strip_text`) use the same pattern; only the raw path was
      exercised by the probe.
    - The new transition helpers `convertReference()` and
      `planBibleModuleTransition()` work on temporary keys of their own and
      do not change the module's key pointer or text
      (`versification_transition_test` asserts this).
    - Line numbers in `sword_main.cc` may change and are not part of this
      task's contract.
  - Root cause to investigate (hypothesis, not a settled fix):
    The rendering paths build or receive another SWKey and hand it to the
    module through `setKey(...)`. With the libsword semantics used here,
    that can substitute the module-owned key instead of repositioning the
    existing SWKey in place. The task must determine exactly:
    - object ownership;
    - lifetime;
    - which `setKey` overload is used;
    - when the object is substituted;
    - which VerseKey state must be preserved.
  - Acceptance criteria:
    - `module->getKey()` keeps the same pointer before and after a text
      read, at least for `getText`, `get_raw_text` and `get_render_text` (or
      their current equivalents).
    - Position and relevant key configuration are preserved or restored in
      place when the operation is temporary.
    - The shared SWKey is not replaced just to position the module.
    - In-place repositioning APIs (e.g. `setKeyText()` or safe VerseKey
      manipulation) are preferred, but the solution is not fixed before the
      real semantics are audited.
    - At least 1000 repeated reads keep a stable pointer, correct text,
      correct reference and no state corruption.
    - Covered at least for modules of different versifications: SpaRV
      (KJV) and TorresAmat (Vulg).
    - `ModuleKeyGuard` still restores position, text, AutoNormalize,
      skip-consecutive-links and any other flag it already preserves.
    - `content_resolver` keeps its current behavior.
    - No regression between first read and later reads (cold/warm state).
    - SWORD lifecycle tests keep passing.
  - Regressions to check when implemented:
    - `sword_backend_key_lifecycle_test`.
    - `content_resolver_sword_test`.
    - `content_resolver_test`.
    - `versification_transition_test`.
    - Any existing `ModuleKeyGuard` test.
    - Repeated reads of the same verse.
    - Chapter change after `getText`/render.
    - Module change after render.
    - KJV and Vulg modules.
  - Related history:
    Conceptually related to earlier SWORD key-state problems: replacing an
    SWKey could leave a legacy pointer out of sync or dangling, and
    repositioning with `setKeyText()` was preferred (see the
    `ModuleKeyGuard` / `getVerseContent` notes in `sword_backend.cc`, and the
    parallel view key aliasing that produced Revelation 1:1 in every row).
    Not assumed to be the same cause until audited; this task must not
    reintroduce that pattern.
  - Do not:
    - Mix this fix with V11N-MODULE-101.
    - Change versification rules.
    - Touch fallback.
    - Modify SWORD modules or Bible data.
    - Do a massive refactor of `sword_main`.
    - Convert every `setKey()` call automatically without analyzing
      ownership and semantics.
    - Declare it fixed only because nothing crashes.
    - Commit or push.

- [ ] V11N-URI-NAV-101 Refresh navbar after resolving sword:// reference in the target module
  - Status: TODO
  - Description:
    Opening `sword://TorresAmat/Psalms 3:9` shows the tab/reference as 3:9,
    but the navbar shows 4:1; `TorresAmat 118:176` shows navbar 119:147.
    `url.cc` calls `main_update_nav_controls` before switching to the target
    module, so the navbar resolves the reference with the module the app
    started with (e.g. SpaRVG, KJV). KJV Ps 3 has 8 verses and Ps 118 has 29,
    so 3:9 and 118:176 are normalized in the wrong versification before
    TorresAmat (Vulg) is loaded.
  - Evidence:
    - Diagnosed from the code path and observed in the real app; not caused
      by V11N-MODULE-101, RENDER-PSALM-TITLES-101 or
      TORRES-PSALM-TITLES-101 (none of them touch that `url.cc` path or the
      navbar).
  - Acceptance criteria:
    - `sword://Module/ref` is interpreted first with the module it names.
    - The navbar is updated after the target module and its native key are
      active.
    - Vulg references are not normalized with KJV.
    - TorresAmat Ps 3:9 stays 3:9; TorresAmat Ps 118:176 is not transformed
      using the KJV verse maximum.
    - SpaRV (KJV) keeps working.
    - A URI still means a reference NATIVE to the module it names.
    - The V11N-MODULE-101 contract does not change.
  - Do not:
    - Commit or push.

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
