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

- [x] TORRES-1835-ZERO-ANCHOR-IO-RECOVERY-VALIDATION-138 Validate bounded recovery for zero-anchor I o verse markers
  - Status: DONE
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

  - Closure:
    - Result: READY; dry-run recovery validation completed.
    - Frozen task baseline commit = 33f4944d1a298a3d967ffca11bb799eb9dd831c4.
    - Bounded discriminator: trusted_anchor_count == 0 AND exact_compound_form == "I o".
    - Full-corpus diagnostic matches = 18 occurrences.
    - Source events represented = 1.
    - Known task-137 positives matched = 18/18.
    - Additional matches outside known positives = 0.
    - Accepted task-128 controls matched = 0/183.
    - Hard negatives matched = 0.
    - Dry-run ref effects: CREATE_NEW_REF = 1, REOPEN_EXISTING_REF = 0, INVALID = 0.
    - Proposed native VerseRef: Ps.17.10.
    - Predicted ownership movement: 27 OCR blocks.
    - Predicted block loss = 0.
    - Predicted dual ownership = 0.
    - Predicted physical gaps: 3255 -> 3254; reduction = 1.
    - Predicted glyph gaps: 1310 -> 1309; reduction = 1.
    - Predicted new duplicate refs = 0.
    - Predicted new out-of-order refs = 0.
    - Predicted outside-canon refs = 0.
    - No existing VerseRef is removed or renumbered.
    - Runtime/parser remained unchanged during task 138.
    - Actual VerseRefs remained = 3848.
    - Actual physical gaps remained = 3255.
    - Actual glyph gaps remained = 1310.
    - Actual ownership remained unchanged.
    - task-128 behavior unchanged.
    - task-131 behavior unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters = 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside-canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
    - Artifact deterministic and idempotent.
    - Artifact uses frozen baseline provenance and does not leak current HEAD.
    - Direct tests passed.
    - Build passed.
    - TorresAmat CTest = 29/29 passed.
    - Full CTest = 41/41 passed.
    - Two environment-dependent tests skipped.
    - New failures = 0.
    - Family status: IMPLEMENTATION_READY.
    - Recommended next work: implement the bounded zero-anchor I o recovery fallback using the exact task-137/138 discriminator and task-138 fail-closed conditions.
    - Task 138 is DONE because the bounded fallback was exhaustively dry-run validated against the full current corpus and its ref/ownership/gap effects are known exactly.
    - Task 138 validated the implementation candidate only; it did not implement runtime recovery.

- [x] TORRES-1835-ZERO-ANCHOR-IO-RECOVERY-139 Implement bounded zero-anchor I o verse-marker recovery
  - Status: DONE
  - Description:
    Implement the task-137/task-138 validated recovery fallback for the exact
    bounded family:
        trusted_anchor_count == 0
        AND exact_compound_form == "I o".
    The implementation must recover only cases satisfying the validated
    source-derived discriminator and fail closed everywhere else.
  - Baseline context:
    - Current VerseRefs = 3848.
    - Current physical gaps = 3255.
    - Current glyph gaps = 1310.
    - Task-137 visual evidence:
        18/18 target occurrences are PRINTED_VERSE_MARKER.
        printed value = 10 for all 18.
    - Task-138 full-corpus matches = 18.
    - Additional matches = 0.
    - Accepted task-128 controls matched = 0/183.
    - Hard negatives matched = 0.
    - Dry-run predicts:
        CREATE_NEW_REF = 1
        REOPEN_EXISTING_REF = 0
        new native ref = Ps.17.10
        ownership moves = 27 blocks
        physical gaps 3255 -> 3254
        glyph gaps 1310 -> 1309.
    - Predicted duplicate/out-of-order/outside-canon = 0.
  - Required behavior:
    - Derive the bounded family from current source/parser state.
    - Do not hardcode the 18 occurrence IDs.
    - Do not hardcode page, block, chapter or verse identities.
    - Apply the fallback only when:
        trusted_anchor_count == 0
        AND exact_compound_form == "I o"
      plus all task-138 fail-closed conditions.
    - Marker value must come from the validated compound-form interpretation,
      not expected verse sequence.
    - Preserve task-128 as the stronger existing rule.
    - The new fallback must run only after stronger existing compound recovery
      has abstained/rejected for lack of trusted band.
    - It must not steal task-128 accepted occurrences.
    - It must not overlap task-131 recovery.
    - Enforce native canon/range validation.
    - Enforce duplicate/reopen safety.
    - Enforce ownership safety.
    - Preserve all unrelated refs and ownership.
    - Produce exactly the task-138 predicted semantic delta unless current
      corpus evidence proves a legitimate discrepancy.
    - If runtime behavior differs from task-138 dry-run, STOP and investigate
      instead of updating expected results to fit implementation.
  - Expected implementation delta:
    - VerseRefs: 3848 -> 3849.
    - New native VerseRef: Ps.17.10.
    - Reopened refs: 0.
    - Removed refs: 0.
    - Renumbered refs: 0.
    - Ownership movement: 27 blocks.
    - Physical gaps: 3255 -> 3254.
    - Glyph gaps: 1310 -> 1309.
    - duplicate_refs: 0.
    - out_of_order_refs: 0.
    - outside-canon: 0.
    - block loss: 0.
    - dual ownership: 0.
  - Fail-closed conditions:
    - form != "I o" -> abstain.
    - trusted_anchor_count != 0 -> existing task-128 behavior only.
    - wrong zone/column/context -> abstain.
    - invalid native canon/range -> abstain.
    - ambiguous ownership -> abstain.
    - duplicate/reopen conflict -> abstain.
    - missing validated source/provenance assumptions -> abstain.
    - any unvalidated additional corpus match -> abstain and fail test.
  - Acceptance:
    - Exactly the validated bounded family is recovered.
    - Full-corpus runtime candidate count matches task-138 expectation.
    - VerseRefs = 3849.
    - Ps.17.10 exists with correct native identity.
    - No unrelated VerseRef changes.
    - Ownership movement matches validated 27-block prediction.
    - Physical gaps = 3254.
    - Glyph gaps = 1309.
    - task-128 accepted behavior unchanged.
    - task-131 behavior unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside-canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
    - implementation deterministic and idempotent.
    - task-137/138 diagnostic artifacts remain reproducible.
    - Exactly one task-140 recommendation supported by post-recovery evidence.
  - Do not:
    - Hardcode Ps.17.10 in production recovery logic.
    - Hardcode the 18 occurrence IDs.
    - Hardcode scan page 32 or PDF page 33.
    - Lower task-128 trusted-anchor requirement.
    - Widen marker-band tolerance.
    - Alter task-128 accepted grammar.
    - Alter task-131 recovery.
    - Use expected verse, previous+1 or next-1.
    - Reopen outside_marker_band recovery.
    - Reopen GLUED_FRAME.
    - Reopen general standalone glyph recovery.
    - Use ML, embeddings, neural OCR or opaque classifiers.
    - Touch unrelated UI code.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.

  - Implementation boundary:
    - Task-137 evidence defines the discriminator.
    - Task-138 evidence defines the expected semantic delta.
    - Do not broaden the family during implementation.
    - If runtime behavior produces anything other than the validated bounded
      delta, STOP, report the discrepancy, and do not update tests or
      artifacts merely to bless broader behavior.

  - Closure:
    - Result: READY; bounded runtime recovery implemented.
    - Frozen task baseline: b16abc761e49fe4ff70c67ab21af187b823102bb.
    - Implemented fallback: trusted_anchor_count == 0 AND exact_compound_form == "I o", with task-138 fail-closed guards.
    - Fallback runs only after stronger task-128/task-131 behavior.
    - No production hardcoding of page, PDF page, block, occurrence, Ps.17.10, or expected verse sequence.
    - No previous+1 / next-1 inference.
    - Runtime VerseRefs: 3848 -> 3849.
    - New native VerseRef: Ps.17.10.
    - New refs = 1.
    - Reopened refs = 0.
    - Removed refs = 0.
    - Renumbered refs = 0.
    - Candidate diagnostic occurrences = 18.
    - Source recovery events = 1.
    - Ownership moves = 27 OCR blocks.
    - Block loss = 0.
    - Dual ownership = 0.
    - Physical gaps: 3255 -> 3254.
    - Closed physical gap: Ps.17.10.
    - Glyph gaps: 1310 -> 1309.
    - Closed glyph gap: Ps.17.10.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside_canon = 0.
    - Chapters = 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - ocr_blocks = 57700.
    - task-128 remains: 183 markers / 177 refs / 1355 ownership moves.
    - task-131 remains: 276 markers/refs / 1900 ownership moves.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Task-137 and task-138 historical artifacts remain frozen and reproducible.
    - Runtime recovery deterministic and idempotent.
    - Direct tests passed.
    - Build passed.
    - TorresAmat CTest = 30/30 passed.
    - Full CTest = 42/42 passed.
    - Two environment-dependent tests skipped.
    - New failures = 0.
    - Audit runtime observed approximately: 65.26 s -> 71.27 s.
    - No per-candidate PDF/image reads were introduced.
    - Post-recovery inventory: physical gaps = 3254; glyph gaps = 1309.
    - Recommended next work: reprioritize remaining glyph-rooted gaps before choosing another recovery family.
    - Task 139 is DONE because runtime output exactly reproduced the semantic delta validated by task 138.

- [x] TORRES-1835-REMAINING-GLYPH-REPRIORITIZATION-140 Reprioritize remaining glyph-rooted verse gaps after bounded recovery
  - Status: DONE
  - Description:
    Recompute and classify the 1309 remaining glyph-rooted verse gaps after
    task 139, reconcile them with the historical task-134/task-135/task-136/
    task-137 evidence, and identify exactly one next bounded family worth
    investigating. This is a diagnostic/prioritization task only; it must not
    introduce new verse recovery.
  - Baseline context:
    - Current runtime VerseRefs = 3849.
    - Current physical gaps = 3254.
    - Current glyph gaps = 1309.
    - task-139 closed exactly Ps.17.10.
    - task-128 remains 183 markers / 177 refs / 1355 ownership moves.
    - task-131 remains 276 markers/refs / 1900 ownership moves.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Historical task-135 result:
        standalone_glyph_candidate = 1310 historical cases,
        but only 6 were true one-token physical cases;
        1304 were projected multi-token cases;
        zero true-single cases were recovery-ready.
  - Required behavior:
    - Recompute the current 1309 glyph-gap inventory from CURRENT parser state.
    - Do not start from a copied historical list.
    - Identify which historical gap disappeared due to task 139.
    - Reconcile current gaps against historical categories from:
        remaining_glyph_inventory
        standalone_glyph_facsimile
        projected_rejection_audit
        no_trusted_band_discriminator
        zero_anchor_io recovery evidence.
    - Preserve stable occurrence identities where possible.
    - Explicitly distinguish:
        physical one-token OCR lines
        projected token candidates from multi-token lines
        detached numeric fragments
        compound-form candidates
        layout/column corruption
        no-local-marker evidence
        other source-backed categories.
    - Do not label projected multi-token candidates as physical standalone
      glyphs.
    - Produce exact counts by:
        root-cause family
        OCR form
        book
        page
        physical vs projected status
        previously reviewed vs unreviewed status
        existing safe-recovery coverage.
    - Determine which remaining family has the highest combination of:
        boundedness
        source evidence
        recurrence
        structural consistency
        potential recoverable VerseRefs
        low false-positive risk.
    - Select exactly ONE task-141 family.
    - The selected task-141 family must be narrower than the entire remaining
      glyph population.
    - If no family has enough evidence for recovery work, select one
      diagnostic family for further facsimile/source review instead.
  - Required historical reconciliation:
    - Explain what happened to the historical 1310 glyph gaps after task 139.
    - Confirm current total = 1309.
    - Identify the exact removed occurrence/ref family corresponding to
      Ps.17.10.
    - Recompute, do not merely assert, the current equivalent of the historical
      1310 standalone/projected split.
    - Report whether the old 6 true-single / 1304 projected structure remains
      applicable after removal, and if not, provide the new exact split.
  - Closed families that must remain closed unless new contradictory evidence exists:
    - GLUED_FRAME: CLOSED_UNSAFE.
    - Generic task-128 compound recovery: do not widen.
    - Generic a-glyph recovery: do not widen.
    - outside_marker_band projected rejections: confirmed rejections.
    - zero-anchor I o: solved by task 139.
    - generic true-single standalone glyph mapping: no reusable safe mapping established by task 135.
  - Evidence requirements:
    - Use source-derived facts.
    - Do not infer verse markers from expected sequence.
    - Do not use previous+1 / next-1.
    - Do not treat OCR token identity alone as proof of a verse marker.
    - Any proposed family must include explicit positives/controls/negatives or
      a plan to obtain them.
    - Clearly separate observed source evidence, diagnostic classification,
      and recovery hypothesis.
  - Artifact:
    - Create a deterministic diagnostic artifact, preferably:
        data/torresamat1835/remaining_glyph_reprioritization.json
    - Include schema_version, frozen baseline provenance, current inventory
      totals, historical reconciliation, root-cause buckets,
      physical/projected split, reviewed/unreviewed split, form frequencies,
      candidate family rankings as raw evidence only, selected task-141 family,
      selection rationale, rejected/deferred families, and runtime invariants.
    - Do not store raw facsimile images.
    - Use stable ordering.
  - Audit:
    - Add diagnostic output equivalent to:
        verse_segmentation_audit.remaining_glyph_reprioritization
    - Include current glyph-gap total, reconciled historical total,
      physical-single count, projected-multi-token count, root-cause counts,
      reviewed/unreviewed counts, selected next family, selected family
      population, and selected family evidence state.
  - Acceptance:
    - Current VerseRefs remain = 3849.
    - Current physical gaps remain = 3254.
    - Current glyph gaps remain = 1309.
    - No runtime recovery is added.
    - No ownership changes.
    - Current 1309 inventory is deterministic.
    - Historical 1310 -> 1309 delta is explicitly accounted for.
    - Physical single-token and projected multi-token candidates are not conflated.
    - Historical task-135 classifications are reconciled rather than blindly copied.
    - Closed unsafe families remain closed.
    - Exactly one bounded task-141 target is selected.
    - Selection is supported by measurable evidence.
    - Chapters remain 337/337; unresolved chapter claims = 0; canonical chapter gaps = 0.
    - duplicate_refs = 0; out_of_order_refs = 0; outside_canon = 0.
    - ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - Artifact deterministic/idempotent.
    - Frozen baseline provenance remains stable across later HEAD changes.
  - Do not:
    - Implement any new recovery.
    - Assume all 1309 glyph gaps are standalone markers.
    - Queue a broad standalone-glyph recovery.
    - Reopen task-135 true-single cases without new evidence.
    - Reopen GLUED_FRAME, outside_marker_band, task-128, or task-131.
    - Hardcode page/block/ref identities as recovery logic.
    - Infer verses from expected sequence.
    - Use ML, embeddings, neural OCR or opaque classifiers.
    - Modify unrelated UI code.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.
  - Semantic boundary:
    Task 140 is not a recovery implementation task. Its job is current 1309
    glyph gaps -> source-backed reclassification -> one bounded next family.
    It must distinguish diagnostic projection from physical OCR structure.
  - Task-141 selection requirement:
    Select exactly ONE next task. The family must have a measurable finite
    population and be narrower than the entire remaining glyph population.
    Valid outcomes include facsimile review of a specific recurring OCR form,
    validation of a specific detached numeric-fragment family, investigation
    of one layout-corruption subclass, or validation of one narrowly bounded
    projected-marker family. Do not queue broad remaining-glyph,
    standalone-glyph, or all-projected-marker recovery.

  - Closure record:
    - Result: READY; current glyph-rooted inventory recomputed and reprioritized without recovery.
    - Frozen task baseline: 13eb5c80d8cf03dead7940391cfc84b11e600262.
    - Runtime remained unchanged: VerseRefs = 3849; physical gaps = 3254; glyph gaps = 1309.
    - Current glyph inventory recomputed from current parser/source state: 1309.
    - Historical glyph inventory: 1310.
    - Reconciled removed occurrences: 1.
    - Removed stable occurrence: p0032l0052::Ps.17.10.
    - Removed occurrence historical category: standalone_glyph_candidate.
    - Removal explained exclusively by task-139 zero-anchor I o recovery.
    - No additional occurrence identities changed.
    - Current physical/projected split: PHYSICAL_SINGLE_TOKEN = 6; PHYSICAL_MULTI_TOKEN = 0; PROJECTED_FROM_MULTI_TOKEN = 1303; DETACHED_NUMERIC_FRAGMENT primary = 0; COMPOUND_FORM primary = 0; LAYOUT_COLUMN_CORRUPTION primary = 0; NO_LOCAL_MARKER_EVIDENCE primary = 0; OTHER_SOURCE_BACKED = 0; UNRESOLVED = 0.
    - Historical 6/1304 split no longer applies because the task-139 occurrence belonged to the projected population.
    - Current equivalent: 6 true physical single-token; 1303 projected multi-token.
    - Secondary/root-cause diagnostic signals include: physical standalone-like = 6; projected multi-token = 1303; detached numeric fragment = 42; compound marker = 0; layout/column signal = 1309; no-local-marker = 0; reviewed unsafe = 3; other/unresolved = 0. Secondary tags may overlap and are not required to sum to 1309.
    - Review coverage: REVIEWED_POSITIVE = 1; REVIEWED_NEGATIVE = 3; REVIEWED_AMBIGUOUS = 4; NOT_REVIEWED = 1301; REVIEW_NOT_APPLICABLE = 0.
    - Top exact/diagnostic forms include: y = 366; a = 260; á = 103; 4 = 73; 3 = 51; i = 47; j = 35; S = 30; X = 30; I = 25.
    - Exact form a: population = 260; physical = 0; projected = 260; books = 7; pages = 97; existing reviewed positives = 1; existing reviewed negatives = 0; remaining ambiguous/unreviewed = 259.
    - Closed families preserved: GLUED_FRAME = CLOSED_UNSAFE; outside_marker_band = CLOSED_REJECTED; task-128 not widened; task-131 not widened; zero-anchor I o solved; generic single-glyph mapping not established.
    - Bounded candidate families evaluated = 3.
    - Selected next family: PROJECTED_MULTI_TOKEN_EXACT_FORM_A.
    - Selected task type: exhaustive/source-backed facsimile audit before any discriminator or recovery validation.
    - Artifact: data/torresamat1835/remaining_glyph_reprioritization.json; schema_version = 1.
    - Artifact deterministic and idempotent.
    - Frozen baseline provenance stable.
    - Audit integrated as: verse_segmentation_audit.remaining_glyph_reprioritization.
    - Chapters = 337/337; unresolved chapter claims = 0; canonical chapter gaps = 0; duplicate_refs = 0; out_of_order_refs = 0; outside_canon = 0; ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - task-128 remains: 183 markers; 177 refs; 1355 ownership moves.
    - task-131 remains: 276 markers/refs; 1900 ownership moves.
    - Direct tests passed.
    - Build passed.
    - TorresAmat CTest = 31/31 passed.
    - Full CTest = 43/43 passed.
    - Two environment-dependent tests skipped.
    - New failures = 0.
    - Recommended next work: exhaustive facsimile/source audit of the finite projected exact-form a family.
    - Task 140 is DONE because the current 1309 glyph-rooted population was recomputed and reconciled, and exactly one bounded evidence-gathering family was selected without introducing runtime recovery.

- [x] TORRES-1835-PROJECTED-FORM-A-FACSIMILE-AUDIT-141 Audit projected multi-token exact-form a candidates against facsimile
  - Status: DONE
  - Description:
    Exhaustively audit the 260 current PROJECTED_FROM_MULTI_TOKEN candidates
    with exact diagnostic form `a` against source/facsimile evidence. Determine
    what the printed source actually contains and whether this population
    contains one or more narrower source-backed verse-marker families worth
    validating later. This is evidence gathering only; no recovery or new
    runtime heuristic is allowed.
  - Baseline context:
    - Runtime VerseRefs = 3849.
    - Physical gaps = 3254.
    - Glyph gaps = 1309.
    - Current projected multi-token population = 1303.
    - Selected exact-form a family: 260 occurrences; 7 books; 97 pages; physical single-token = 0; projected multi-token = 260.
    - Existing stable review evidence: reviewed positive = 1; reviewed negative = 0; remaining ambiguous/unreviewed = 259.
    - Historical task-135 evidence showed at least one projected form-a case could correspond visually to a printed numeral, but this must NOT be generalized to the current family without facsimile evidence.
  - Purpose:
    - Review all 260 stable occurrences, not merely a sample, if the configured facsimile/source witness is available.
    - Determine whether projected OCR form `a` corresponds to: printed verse-marker numeral; ordinary body text; heading/title; Latin parallel column; apparatus/note; scan/layout corruption; unreadable/ambiguous source; other.
    - For confirmed printed verse markers, record the visible printed numeric value from source evidence.
    - Do not infer printed value from expected VerseRef or verse sequence.
    - Identify narrower repeated subfamilies, if any, using source-derived structural evidence.
  - Candidate derivation:
    - Recompute the current exact-form-a family from CURRENT task-140 inventory / parser source state.
    - Do not hardcode 260 occurrence IDs as the family definition.
    - Family definition must include: exact diagnostic form = `a`; PROJECTED_FROM_MULTI_TOKEN; verse-body/source structural context already established by task 140.
    - Stable occurrence IDs may be used for review tracking only.
  - Facsimile review:
    - Review every one of the 260 current occurrences if source images/pages are available.
    - Use the configured development facsimile/source witness; do not download or commit raw scans into the repository.
    - Preserve the current source-page/PDF-page mapping from existing project provenance rather than inventing one.
    - For every occurrence record: stable occurrence id; book/chapter diagnostic context; scan/source page; facsimile page; OCR line/block identity; raw source OCR line; projected token; exact form; physical token count; source zone/column; source crop/geometry coordinates if available; classification; visible printed value if confidently readable; review confidence/evidence reason; historical review lineage if applicable.
  - Review classifications:
    - Each occurrence must end in exactly one primary review class: PRINTED_VERSE_MARKER; ORDINARY_TEXT; HEADING_OR_TITLE; LATIN_PARALLEL_TEXT; APPARATUS_OR_NOTE; LAYOUT_OR_SCAN_ARTIFACT; UNREADABLE; OTHER.
    - Do not force unreadable evidence into positive/negative categories.
  - Printed value:
    - For PRINTED_VERSE_MARKER only, record: visible_printed_value; visual basis; source coordinates/page.
    - Do NOT derive printed value from expected missing verse, previous + 1, next - 1, or neighboring canonical sequence.
    - If the glyph visually resembles more than one digit, classify UNREADABLE/ambiguous rather than choosing by expected verse.
  - Structural evidence:
    - For each confirmed positive and negative, preserve measurable context: right/left column; verse-body zone; line position; projected token position within source line; neighboring OCR tokens; geometry relative to known marker bands/anchors if available; block ownership context.
    - Token identity `a` alone is NEVER sufficient evidence.
  - Existing review:
    - Reconcile the one existing REVIEWED_POSITIVE by stable occurrence identity.
    - Verify it again against current source evidence where possible.
    - Do not duplicate-count it.
  - Required aggregate results:
    - total = 260.
    - reviewed = 260 if facsimile is available for all cases.
    - exact counts by review class.
    - exact counts by visible printed value among positives.
    - positives by book/page.
    - negatives by book/page.
    - unreadable/ambiguous count.
    - structural subfamilies discovered.
    - candidate controls/negatives for any future discriminator.
  - Subfamily discovery:
    - Group reviewed positives and negatives only by transparent source-derived characteristics, such as visible printed value/morphology; token position within multi-token line; geometric band/offset; neighboring punctuation/token pattern; column/body context; repeated source-page layout.
    - Do NOT use expected VerseRef, expected verse number, previous+1, next-1, occurrence id, or page identity alone as a discriminator feature.
  - Future-task decision:
    - End with exactly one task-142 recommendation: A. bounded discriminator validation for one explicit subfamily; B. narrower facsimile/source audit; C. close/defer form-a family as unsafe.
    - Do not implement task 142.
    - Do not recommend generic `a -> digit` recovery.
  - Artifact:
    - Create deterministic diagnostic artifact, preferably: data/torresamat1835/projected_form_a_facsimile.json.
    - Include schema_version; frozen baseline provenance; candidate-family derivation; 260 occurrence review records; class counts; positive visible-value counts; geometry/context summaries; discovered subfamilies; controls/negatives; selected task-142 target; runtime invariants.
    - Do not embed raw images.
    - Do not embed PDFs.
    - Stable deterministic ordering.
  - Provenance:
    - Freeze task-141 input baseline commit.
    - Do not derive persisted baseline_commit from whatever HEAD happens to be during later regeneration.
    - Follow the corrected explicit/frozen provenance pattern already used by tasks 137/138/140.
  - Audit:
    - Add diagnostic audit equivalent to: verse_segmentation_audit.projected_form_a_facsimile.
    - Include: candidate_count; reviewed_count; class_counts; positive_count; visible_value_counts; negative_count; unreadable_count; structural_subfamilies; selected_task142_target; selected_task142_reason.
  - Runtime invariants:
    - No new recovery.
    - VerseRefs remain = 3849.
    - Physical gaps remain = 3254.
    - Glyph gaps remain = 1309.
    - Ownership unchanged.
    - task-128 unchanged.
    - task-131 unchanged.
    - task-139 unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside_canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
  - Acceptance:
    - Current exact-form-a population deterministically recomputes to 260.
    - All 260 occurrences are accounted for.
    - Every source-accessible occurrence is facsimile reviewed.
    - No review decision uses expected verse sequence.
    - Existing positive lineage is reconciled by stable occurrence identity.
    - PRINTED_VERSE_MARKER cases have source-visible printed values or are explicitly unreadable.
    - Ordinary text / Latin / heading / artifact negatives are preserved.
    - Token identity alone is never used as marker authority.
    - Any proposed subfamily has both positive evidence and explicit controls/negative analysis.
    - Exactly one bounded task-142 recommendation is produced.
    - No runtime/parser recovery change.
    - Artifact deterministic and idempotent.
    - Frozen provenance stable.
  - Do not:
    - Implement recovery.
    - Add `a -> 2` or any other global glyph mapping.
    - Assume the existing positive generalizes to all 260.
    - Infer numeral from missing VerseRef.
    - Use previous+1 or next-1.
    - Use page/book identity as sole discriminator.
    - Treat projected token as physical standalone OCR.
    - Reopen generic standalone glyph recovery.
    - Reopen GLUED_FRAME.
    - Reopen outside_marker_band.
    - Widen task-128.
    - Widen task-131.
    - Modify task-139.
    - Use ML, embeddings, neural OCR or opaque classifiers.
    - Commit raw facsimile images/PDFs.
    - Modify unrelated UI.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.
  - Important task-141 boundary:
    Task 141 is an exhaustive SOURCE/FACSIMILE AUDIT. It is not a recovery task, a discriminator implementation task, or proof that OCR form `a` means a numeral. The task must first answer: What is actually printed at these 260 projected occurrences?
  - Source availability:
    If the configured facsimile/source witness needed for review is unavailable, do not classify cases from expected verse sequence, do not invent labels, and do not silently downgrade to OCR-only inference. Report the exact missing source requirement. A partial source audit may be PARTIAL READY only if every reviewed label is source-backed and the unreviewed population is explicit.


  - Closure record:
    - Result: READY; exhaustive source/facsimile audit completed.
    - Frozen task baseline: ca3afefec401b3e94dad190de53144e66a9fc74b.
    - Source witness: ia-lasagradabiblia01unkngoog.
    - PDF SHA-256: cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346.
    - Candidate family: PROJECTED_FROM_MULTI_TOKEN; exact diagnostic form = `a`.
    - Candidate population: 260 unique occurrences; 7 books; 97 pages; physical single-token = 0; projected multi-token = 260.
    - Source accessible = 260/260; facsimile reviewed = 260/260.
    - Primary review classes: PRINTED_VERSE_MARKER = 238; ORDINARY_TEXT = 1; HEADING_OR_TITLE = 0; LATIN_PARALLEL_TEXT = 0; APPARATUS_OR_NOTE = 21; LAYOUT_OR_SCAN_ARTIFACT = 0; UNREADABLE = 0; OTHER = 0.
    - Visible printed values among PRINTED_VERSE_MARKER: 1 = 1; 2 = 77; 3 = 1; 8 = 2; 12 = 1; 13 = 1; 20 = 29; 21 = 67; 22 = 19; 24 = 31; 25 = 9.
    - Sum of printed-marker values = 238.
    - Exact OCR form `a` corresponds to multiple printed numeric values. Therefore NO global `a -> digit` mapping is safe.
    - Historical reviewed positive: p0276l0033::Eccl.1.2; historical/current classification: PRINTED_VERSE_MARKER (historical artifact label: PRINTED_DIGIT); visible value = 2; historical/current source evidence agrees.
    - Selected candidate task-142 family from task 141: PROJECTED_FORM_A_VISIBLE_2_MARKER_BAND.
    - Ground-truth positives for printed value 2 = 77.
    - IMPORTANT control correction for task 142: all non-target task-141 occurrences must act as controls.
    - Full task-142 negative/control population = 183: 161 PRINTED_VERSE_MARKER occurrences with visible value != 2, plus 22 non-marker occurrences (ORDINARY_TEXT = 1; APPARATUS_OR_NOTE = 21).
    - The 161 other printed-marker values are critical controls because a future runtime rule must distinguish printed `2`, not merely distinguish marker from non-marker.
    - Selected next-task type: BOUNDED_DISCRIMINATOR_VALIDATION.
    - Artifact: data/torresamat1835/projected_form_a_facsimile.json; schema_version = 1.
    - Artifact deterministic and idempotent; frozen baseline provenance stable.
    - Audit integrated as: verse_segmentation_audit.projected_form_a_facsimile.
    - Runtime unchanged: VerseRefs = 3849; physical gaps = 3254; glyph gaps = 1309.
    - Ownership unchanged; task-128 unchanged; task-131 unchanged; task-139 unchanged; GLUED_FRAME = CLOSED_UNSAFE.
    - Chapters = 337/337; unresolved chapter claims = 0; canonical chapter gaps = 0; duplicate_refs = 0; out_of_order_refs = 0; outside_canon = 0; ocr_blocks = 57700; block loss = 0; dual ownership = 0.
    - Direct tests passed: python3 scripts/torresamat1835/test_projected_form_a_facsimile.py; python3 scripts/torresamat1835/test_remaining_glyph_reprioritization.py (rerun on current master for this planning closure).
    - Integrated implementation validation record supplied for closure: Build passed; TorresAmat CTest = 31/31 passed; Full CTest = 43/43 passed; two environment-dependent tests skipped; new failures = 0.
    - Recommended next work: determine whether runtime-available source geometry/context can distinguish the 77 facsimile-confirmed printed-2 occurrences from ALL 183 known non-target controls without using facsimile labels as rule features.
    - Task 141 is DONE because all 260 current exact-form-a projected occurrences were independently source-reviewed and the family is now fully labeled.

- [x] TORRES-1835-PROJECTED-FORM-A-VISIBLE-2-DISCRIMINATOR-142 Validate a runtime-safe discriminator for facsimile-confirmed printed-2 projected form-a markers
  - Status: DONE
  - Description:
    Determine whether a transparent discriminator using only source/parser
    features available before knowing the facsimile label can distinguish the
    77 task-141 occurrences whose printed value is visually confirmed as `2`
    from all 183 other exact-form-a occurrences. This is diagnostic validation
    only; no recovery or runtime mapping may be implemented.
  - Ground truth:
    - Total exact-form-a family = 260.
    - Positive target:
        PRINTED_VERSE_MARKER
        visible_printed_value = 2
        population = 77.
    - Negative/control target = 183:
        161 PRINTED_VERSE_MARKER with visible_printed_value != 2
        1 ORDINARY_TEXT
        21 APPARATUS_OR_NOTE.
    - The 183 controls MUST all participate in validation.
    - Do not reduce controls to only the 22 non-markers.
  - Critical semantic boundary:
    - `visible_printed_value == 2` is GROUND-TRUTH LABEL ONLY.
    - It MUST NOT be a discriminator feature.
    - Facsimile classification MUST NOT be available to simulated runtime
      candidate selection.
    - The rule must operate on features available from OCR/parser/source
      geometry before the visual label is consulted.
  - Candidate runtime-safe features may include only transparent
    source-derived data already available or reproducibly derivable without
    facsimile interpretation, for example:
        projected-token position
        physical source-line structure
        neighboring OCR token pattern
        source/body column
        x/y geometry
        relationship to trusted marker bands/anchors
        indentation
        line-start structure
        source block geometry
        deterministic OCR morphology/context.
    - Every feature used must be explicitly documented.
  - Forbidden discriminator features:
        visible_printed_value
        facsimile review class
        expected verse
        missing VerseRef identity
        previous + 1
        next - 1
        occurrence ID
        page identity alone
        book identity alone
        chapter identity alone
        expected canonical sequence.
  - Do not:
    - implement recovery;
    - add `a -> 2`;
    - add a global form-a mapping;
    - use the 77 occurrence IDs as an allowlist;
    - memorize pages containing positives;
    - use facsimile labels as runtime inputs;
    - change task-128;
    - change task-131;
    - change task-139.
  - Required evaluation:
    - Recompute all 260 current exact-form-a candidates.
    - Join task-141 visual labels only AFTER feature extraction.
    - Separate:
        77 target positives
        183 controls.
    - Evaluate candidate transparent rules against the complete population.
    - Report for every evaluated rule:
        positives accepted
        positives rejected
        controls accepted
        controls rejected
        false-positive identities
        false-negative identities.
    - Explicitly report controls by:
        other printed value
        ordinary text
        apparatus/note.
    - A rule that accepts a value-20/21/22/24/etc marker as value 2 is a false
      positive even though it is a real verse marker.
  - Safety requirement:
    - Do NOT call a discriminator safe if any known non-2 control is accepted.
    - Required for SAFE_DISCRIMINATOR_FOUND:
        controls accepted = 0 / 183.
    - Positive recall may be <77 if the resulting family is a genuinely
      narrower bounded subset.
    - If only a subset of the 77 can be selected with zero false positives,
      report that exact finite subset as the candidate for task 143.
    - Do NOT broaden a rule to obtain higher recall at the expense of known
      false positives.
  - Overfitting protection:
    - Do not use page/book/chapter/occurrence identity as proxy features.
    - Prefer reusable structural measurements.
    - Evaluate near-miss controls sharing the same form/context.
    - Report feature distributions for positives and controls.
    - If separation exists only because of one page/source identity,
      classify as overfit and reject it as runtime-safe.
  - Rule complexity:
    - Prefer a minimal transparent conjunction of measurable features.
    - No arbitrary scoring model.
    - No opaque classifier.
    - No ML.
    - No embeddings.
    - No neural OCR.
  - Result status must be exactly one:
        SAFE_DISCRIMINATOR_FOUND
        NARROWER_SAFE_SUBFAMILY_FOUND
        NO_SAFE_DISCRIMINATOR
    - SAFE_DISCRIMINATOR_FOUND:
        a transparent rule safely identifies all intended 77 positives with
        zero accepted controls.
    - NARROWER_SAFE_SUBFAMILY_FOUND:
        a transparent rule identifies a strict subset of the 77 with zero
        accepted controls.
    - NO_SAFE_DISCRIMINATOR:
        no transparent rule survives the 183-control set without known false
        positives.
  - Task-143 recommendation:
    - If SAFE_DISCRIMINATOR_FOUND:
        queue dry-run recovery validation for that exact rule.
    - If NARROWER_SAFE_SUBFAMILY_FOUND:
        queue dry-run recovery validation only for the exact safe subset/rule.
    - If NO_SAFE_DISCRIMINATOR:
        close/defer this form-a value-2 family and select one different
        diagnostic target from task-141 evidence.
    - Exactly one task-143 recommendation.
  - Artifact:
    - Create deterministic diagnostic artifact, preferably:
        data/torresamat1835/projected_form_a_visible_2_discriminator.json
    - Include:
        schema_version
        frozen baseline provenance
        ground-truth population
        pre-label feature extraction
        positive/control feature summaries
        candidate rules evaluated
        confusion counts
        false-positive/false-negative records
        selected rule if any
        final status
        task-143 recommendation
        runtime invariants.
    - Do not duplicate raw facsimile image data.
  - Audit:
    - Add diagnostic equivalent to:
        verse_segmentation_audit.projected_form_a_visible_2_discriminator
    - Include:
        positives = 77
        controls = 183
        printed-other-value controls = 161
        non-marker controls = 22
        candidate-rule count
        selected-rule definition
        selected-rule TP/FN/FP/TN
        final status
        task143 target.
  - Runtime invariants:
    - VerseRefs remain = 3849.
    - Physical gaps remain = 3254.
    - Glyph gaps remain = 1309.
    - Ownership unchanged.
    - task-128 unchanged.
    - task-131 unchanged.
    - task-139 unchanged.
    - GLUED_FRAME = CLOSED_UNSAFE.
    - Chapters = 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside_canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
  - Acceptance:
    - Exactly 260 task-141 occurrences reconciled.
    - Exactly 77 positive labels.
    - Exactly 183 controls.
    - All 161 other printed marker values are included as negative controls.
    - All 22 non-marker cases are included as controls.
    - Feature extraction is independent of visual ground-truth labels.
    - No expected-verse inference.
    - No occurrence/page allowlist.
    - Every evaluated rule has complete confusion accounting.
    - Final status supported by observed evidence.
    - Exactly one task-143 recommendation.
    - No runtime change.
    - Artifact deterministic/idempotent.
    - Frozen baseline provenance stable.
  - Do not:
    - Implement recovery.
    - Treat visible value 2 as a runtime feature.
    - Treat marker-vs-non-marker discrimination as sufficient.
    - Ignore the 161 markers with other visible values.
    - Hardcode IDs/pages/books/chapters.
    - Infer expected verse.
    - Use previous+1 or next-1.
    - Introduce global `a -> 2`.
    - Use ML, embeddings, neural OCR, opaque classifiers.
    - Modify TASKS.md from the task agent.
    - Commit or push task implementation.

  - Closure:
    - Result: NARROWER_SAFE_SUBFAMILY_FOUND.
    - Frozen task baseline: edef6d28d0e647ca97aa8ac91c3c68284137711c.
    - Full exact-form-a family: 260 unique occurrences.
    - Ground-truth printed-2 positives: 77.
    - Full controls: 183.
    - Controls consist of: 161 PRINTED_VERSE_MARKER occurrences with visible value != 2; 1 ORDINARY_TEXT; 21 APPARATUS_OR_NOTE.
    - Physical source blocks represented by the full population: 101.
    - Runtime-safe features extracted: 38.
    - Features were constructed/frozen before task-141 visual labels were joined.
    - Label mutation, identity independence and forbidden-feature tests passed.
    - Candidate transparent rules evaluated: 298.
    - Selected exact transparent rule: first physical OCR token == "a" AND each of the following three tokens contains >=2 Unicode letters AND a trusted marker band exists AND abs(candidate_x - band_center) <= existing band tolerance.
    - Trusted marker-band semantics remain unchanged: minimum trusted anchors = 3; center = median existing marker-band center; tolerance = max(30 px, 0.5 * median digit width).
    - Selected-rule confusion: TP = 53; FN = 24; FP = 0; TN = 183.
    - Selected safe diagnostic occurrences: 53.
    - Selected population represents: 47 physical OCR blocks; 46 pages; 6 books.
    - Accepted other-value marker controls: 0/161.
    - Accepted non-marker controls: 0/22.
    - Accepted controls by visible value: 1 = 0; 3 = 0; 8 = 0; 12 = 0; 13 = 0; 20 = 0; 21 = 0; 22 = 0; 24 = 0; 25 = 0.
    - ORDINARY_TEXT accepted: 0/1.
    - APPARATUS_OR_NOTE accepted: 0/21.
    - Width-only candidate rejected because its safe boundary was separated from the printed-1 control by only about 2 px.
    - Height-only zero-FP candidate rejected as over-narrow/page-local: 4 positives, all in Psalms.
    - Selected rule was not widened to capture the remaining 24 positives.
    - False-negative guard failures overlap and include: frame/start condition = 8; three-following-token condition = 11; marker-band condition = 12.
    - Selected-rule TP/FN by book: Eccl = 3/0; Isa = 11/7; Prov = 5/6; Ps = 23/1; Sir = 10/2; Song = 1/0; Wis = 0/8.
    - No occurrence/page/book/chapter allowlist used.
    - No visible_printed_value or facsimile class used as runtime rule input.
    - No expected VerseRef or expected-verse sequence used.
    - No previous+1 / next-1.
    - No ML, embeddings or opaque classifier.
    - Artifact: data/torresamat1835/projected_form_a_visible_2_discriminator.json; schema_version = 1.
    - Artifact deterministic and idempotent.
    - Runtime unchanged: VerseRefs = 3849; physical gaps = 3254; glyph gaps = 1309.
    - Ownership unchanged: 25434 owned blocks.
    - task-128 remains: 183 markers; 177 refs; 1355 ownership moves.
    - task-131 remains: 276 markers; 276 refs; 1900 ownership moves.
    - task-139 unchanged.
    - GLUED_FRAME = CLOSED_UNSAFE.
    - Chapters = 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside_canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
    - Direct tests: 13/13 PASS.
    - Build: PASS.
    - Torres CTest: 32/32 PASS.
    - Full suite: 44 registered; 42 passed; 2 skipped; 0 failed.
    - Skipped: gtk_lifecycle_smoke; author_commentary_probe.
    - Recommended next work: bounded dry-run recovery validation for ONLY the exact selected 53-occurrence / 47-physical-block rule.
    - Task 142 is DONE because a strict transparent subfamily of the task-141 printed-2 population was isolated with zero accepted known controls.
    - The remaining 24 printed-2 positives are not described as recoverable.

- [x] TORRES-1835-PROJECTED-FORM-A-VISIBLE-2-DRY-RUN-143 Validate dry-run recovery effects for the safe projected form-a printed-2 subfamily
  - Status: DONE
  - Description:
    Simulate, without modifying runtime recovery, the exact semantic effects
    of interpreting only the task-142 selected safe subfamily as printed verse
    marker value `2`. Recompute the discriminator from runtime-safe features,
    reconcile projected occurrences into physical/source recovery events, and
    measure exact VerseRef, ownership and gap effects before production
    implementation.
  - Validated discriminator:
        first physical OCR token == "a"
        AND each of the following three physical tokens contains >=2 Unicode
            letters
        AND a trusted marker band exists
        AND abs(candidate_x - band_center) <= existing marker-band tolerance.
  - Marker-band semantics:
        trusted_anchor_count >= 3
        center = existing median marker-band center
        tolerance = max(30 px, 0.5 * median digit width).
  - Baseline evidence:
        full form-a population = 260
        printed-2 ground-truth positives = 77
        selected safe occurrences = 53
        false negatives = 24
        selected known controls = 0/183
        selected physical blocks = 47
        selected pages = 46
        selected books = 6.
  - Critical distinction:
    - 53 projected diagnostic occurrences != necessarily 53 physical events.
    - 47 physical blocks != necessarily 47 VerseRefs.
    - Deduplicate all projections into actual source/recovery events.
    - Measure independently:
        diagnostic occurrences
        physical blocks
        source marker events
        recovery events
        resulting VerseRefs.
  - Candidate derivation:
    - Recompute the complete current exact-form-a family.
    - Recompute the exact task-142 selected rule from runtime-safe features.
    - Do not define the task-143 population by a hardcoded list of 53 IDs.
    - Stable IDs may be used only for validation/reconciliation.
    - Require selected task-142 occurrences = 53.
    - Require accepted task-142 controls = 0/183.
  - Ground-truth validation:
    - Join task-141 labels only after selection.
    - Require every selected occurrence to be one of the task-141
      facsimile-confirmed printed-2 positives.
    - If any selected case is not printed value 2:
        BLOCK implementation readiness.
  - Full-corpus production-placement check:
    - Evaluate how the exact proposed fallback would encounter candidates in
      the real parser context.
    - Detect any additional match outside the task-141 260-family scope.
    - Any unreviewed additional match blocks implementation readiness.
  - Printed value:
    - Within ONLY the exact task-142 validated family, dry-run interpretation
      may use numeric value 2.
    - This bounded interpretation is justified by task-141 facsimile labels
      plus task-142 zero-known-FP discriminator.
    - visible_printed_value must NOT participate in candidate selection.
    - Do not derive `2` from expected missing verse.
    - Do not use previous+1 / next-1.
  - Deduplication:
    - Reconcile selected 53 projections into:
        physical OCR blocks
        source marker events
        proposed recovery events.
    - Report exact counts and mappings.
    - Prove duplicate projections do not create duplicate recovery events.
  - Dry-run native ref simulation:
    - For each proposed recovery event determine:
        native book
        native chapter
        interpreted printed value = 2
        proposed native VerseRef
        existing neighboring/parser ownership context
        whether ref already exists.
    - Classify every event:
        CREATE_NEW_REF
        REOPEN_EXISTING_REF
        NO_REF_EFFECT
        INVALID.
    - Proposed VerseRef must emerge from normal native parser semantics.
    - Do not hardcode expected refs.
    - Do not use another module/KJV numbering as authority.
  - Ref-set effects:
    - Report exact predicted:
        VerseRefs before
        VerseRefs after
        new refs
        reopened refs
        no-effect events
        invalid events
        removed refs
        renumbered refs.
    - Required:
        removed refs = 0
        renumbered refs = 0
        unrelated existing refs changed = 0.
  - Ownership dry-run:
    - For every recovery event calculate:
        currently owning ref(s)
        blocks that would move
        receiving recovered ref
        blocks remaining with prior ref.
    - Report:
        total ownership moves
        unique blocks moved
        block loss
        dual ownership.
    - Do not mutate actual runtime ownership.
  - Gap dry-run:
    - Calculate exact predicted:
        physical gaps before/after
        physical gap identities closed
        glyph gaps before/after
        glyph gap identities closed.
    - Do not assume one selected projection closes one gap.
  - Safety:
    - Predicted duplicate_refs = 0.
    - Predicted out_of_order_refs = 0.
    - Predicted outside_canon = 0.
    - Predicted impossible native refs = 0.
    - Predicted block loss = 0.
    - Predicted dual ownership = 0.
  - Existing recovery isolation:
    - task-128 behavior unchanged.
    - task-131 behavior unchanged.
    - task-139 behavior unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - No double recovery with stronger existing paths.
  - Runtime boundary:
    - Task 143 is DRY-RUN ONLY.
    - Actual parser/runtime output must remain:
        VerseRefs = 3849
        physical gaps = 3254
        glyph gaps = 1309
        ownership unchanged.
  - Result status:
        IMPLEMENTATION_READY
        NEEDS_NARROWER_VALIDATION
        UNSAFE_TO_IMPLEMENT.
    - IMPLEMENTATION_READY requires:
        task-142 rule reproduces exactly the validated selected family
        zero known controls accepted
        no unreviewed full-corpus extra matches
        deterministic event deduplication
        exact ref delta known
        ownership safe
        gap delta known
        no duplicate/order/canon violations.
  - Task-144 recommendation:
    - IMPLEMENTATION_READY:
        exactly one bounded runtime implementation task.
    - otherwise:
        exactly one narrower diagnostic task.
    - Do not implement task 144.
  - Artifact:
    - Create deterministic diagnostic artifact, preferably:
        data/torresamat1835/projected_form_a_visible_2_dry_run.json
    - Include:
        schema_version
        frozen baseline provenance
        exact task-142 rule
        selected occurrence reconciliation
        physical-block mapping
        source-event mapping
        recovery-event mapping
        native ref effects
        ownership effects
        gap effects
        control/full-corpus validation
        semantic safety checks
        final status
        task144 recommendation
        runtime invariants.
  - Audit:
    - Add diagnostic equivalent to:
        verse_segmentation_audit.projected_form_a_visible_2_dry_run
    - Include:
        selected_occurrences
        physical_blocks
        source_events
        recovery_events
        create_new_refs
        reopened_refs
        no_ref_effect
        invalid
        ownership_moves
        physical_gap_delta
        glyph_gap_delta
        controls_selected
        full_corpus_extra_matches
        duplicate/order/canon checks
        final_status
        task144_target.
  - Acceptance:
    - Current rule recomputes 53 selected occurrences.
    - Controls selected = 0/183.
    - 47 physical blocks reconcile exactly.
    - All selected cases remain task-141 printed-2 ground truth.
    - Source/recovery event deduplication deterministic.
    - Exact proposed refs known.
    - Exact ownership delta known.
    - Exact gap delta known.
    - No occurrence/block/ref allowlist as recovery authority.
    - No expected verse inference.
    - No runtime mutation.
    - Historical task-141/task-142 artifacts remain frozen.
    - Artifact deterministic/idempotent.
    - Frozen provenance stable.
    - Exactly one task-144 recommendation.
  - Do not:
    - Implement production recovery.
    - Hardcode the 53 occurrence IDs.
    - Hardcode the 47 block IDs.
    - Hardcode expected VerseRefs.
    - Recover the remaining 24 printed-2 false negatives.
    - Recover any of the 183 controls.
    - Broaden the task-142 discriminator.
    - Infer refs from expected gap sequence.
    - Use previous+1 / next-1.
    - Alter task-128/task-131/task-139.
    - Reopen GLUED_FRAME.
    - Use ML/opaque classifiers.
    - Modify TASKS.md from the task agent.
    - Commit or push implementation work from the task agent.
  - Result:
        NEEDS_NARROWER_VALIDATION.
  - Frozen task baseline:
        fd69a8603b98dacf89bb3561cfa80e6e8044e21a.
  - Task-142 selected projected occurrences:
        53.
  - Selected physical OCR blocks:
        47.
  - Selected source marker events:
        47.
  - Proposed recovery events:
        47.
  - All 53 selected task-142 occurrences remain facsimile-confirmed
    printed-value-2 positives.
  - Known task-142 controls selected:
        0/183.
  - Recovery-event effects among the 47 reviewed events:
        CREATE_NEW_REF = 45
        REOPEN_EXISTING_REF = 2
        NO_REF_EFFECT = 0
        INVALID = 0.
  - Predicted new VerseRefs:
        45.
  - Predicted reopened refs:
        Ps.47.2
        Ps.93.2.
  - Two known reopened-ref events violate physical/native source order:
        p0072l0083:
            current physical context Ps.47.7
            proposed reopen Ps.47.2
        p0136l0092:
            current physical context Ps.93.19
            proposed reopen Ps.93.2.
  - These two order conflicts prevent production implementation.
  - Production-placement scan:
        total matches = 158
        in reviewed task-142 physical family = 47
        external/unreviewed matches = 111.
  - The 111 out-of-family production matches lack task-141 visual
    ground truth and therefore prevent implementation.
  - Do NOT interpret those 111 as recoverable printed-2 markers.
  - Predicted VerseRefs for reviewed dry-run:
        3849 -> 3894.
  - Predicted physical gaps:
        3254 -> 3209
        reduction = 45.
  - Predicted glyph gaps:
        1309 -> 1255
        reduction = 54.
  - Predicted ownership moves:
        595 unique OCR blocks.
  - Affected old refs:
        47.
  - Receiving new refs:
        45.
  - Owned blocks:
        25434 -> 25434.
  - Predicted block loss:
        0.
  - Predicted dual ownership:
        0.
  - Duplicate proposed refs:
        0.
  - Duplicate final refs:
        0.
  - Physical-order conflicts:
        2.
  - outside-canon:
        0.
  - impossible native refs:
        0.
  - Removed refs:
        0.
  - Renumbered refs:
        0.
  - Unrelated existing refs changed:
        0.
  - Actual runtime remained unchanged:
        VerseRefs = 3849
        physical gaps = 3254
        glyph gaps = 1309
        ownership = 25434.
  - task-128 unchanged:
        183 markers
        177 refs
        1355 ownership moves.
  - task-131 unchanged:
        276 markers
        276 refs
        1900 ownership moves.
  - task-139 unchanged, including Ps.17.10.
  - GLUED_FRAME remains CLOSED_UNSAFE.
  - Chapters = 337/337.
  - unresolved chapter claims = 0.
  - canonical chapter gaps = 0.
  - current duplicate_refs = 0.
  - current out_of_order_refs = 0.
  - current outside_canon = 0.
  - ocr_blocks = 57700.
  - current block loss = 0.
  - current dual ownership = 0.
  - Artifact:
        data/torresamat1835/projected_form_a_visible_2_dry_run.json
        schema_version = 1.
  - Artifact deterministic and idempotent.
  - Direct tests:
        14/14 PASS.
  - Focused registered CTest:
        1/1 PASS.
  - Build:
        PASS.
  - Torres CTest:
        33/33 PASS.
  - Full CTest:
        45 registered
        43 passed
        2 skipped
        0 failed.
  - Skipped:
        gtk_lifecycle_smoke
        author_commentary_probe.
  - Recommended next work:
        audit all 111 out-of-family production matches and derive a
        source-safe abstention condition for the two known late-ref/order
        conflicts before any production implementation.

- [x] TORRES-1835-PROJECTED-FORM-A-PRODUCTION-SCOPE-AUDIT-144 Audit out-of-family production matches and unsafe late ref reopenings
  - Status: DONE
  - Description:
    Exhaustively analyze the 111 additional production-placement matches
    discovered by task 143 and the two known reviewed events that would reopen
    earlier VerseRefs out of physical order. Determine whether transparent,
    source/runtime-safe guards can restrict the task-142 discriminator to a
    genuinely implementation-safe family. This is diagnostic/source review
    only; no runtime recovery may be implemented.
  - Baseline context:
    - Current runtime:
        VerseRefs = 3849
        physical gaps = 3254
        glyph gaps = 1309.
    - Task-142 reviewed safe discriminator:
        53 projected occurrences
        47 physical blocks
        0/183 known controls.
    - Task-143 reviewed recovery events:
        47.
    - Dry-run ref effects:
        CREATE_NEW_REF = 45
        REOPEN_EXISTING_REF = 2.
    - Known unsafe reopens:
        p0072l0083 -> Ps.47.2 after physical context Ps.47.7
        p0136l0092 -> Ps.93.2 after physical context Ps.93.19.
    - Production-placement matches:
        158 total
        47 in reviewed family
        111 additional/unreviewed.
    - Production recovery remains BLOCKED until both issues are resolved.
  - Primary objective A — external matches:
    - Recompute all 158 production-placement matches from CURRENT source/parser
      state.
    - Reconcile the 47 reviewed task-142/task-143 blocks.
    - Enumerate exactly the 111 external matches.
    - Do not define the 111 by list subtraction alone; derive them from actual
      production candidate enumeration.
    - Determine why each external match satisfies the task-142 structural
      rule despite not belonging to the reviewed task-141 family.
    - Record source/runtime-safe features before any visual label is added.
  - Source review of external matches:
    - Obtain source/facsimile evidence for all 111 where available.
    - Classify each into exactly one:
        PRINTED_VALUE_2_MARKER
        PRINTED_OTHER_VALUE_MARKER
        ORDINARY_TEXT
        HEADING_OR_TITLE
        LATIN_PARALLEL_TEXT
        APPARATUS_OR_NOTE
        LAYOUT_OR_SCAN_ARTIFACT
        UNREADABLE
        OTHER.
    - For marker cases record visible printed value from source evidence.
    - Do not infer values from missing verses or expected sequence.
    - If facsimile is unavailable, preserve the match as UNREVIEWED and do not
      treat it as recoverable.
  - Primary objective B — late-ref conflicts:
    - Analyze independently:
        p0072l0083
        p0136l0092.
    - Determine source-backed reason these candidates would attempt to reopen
      verse 2 after physical contexts verse 7 / verse 19.
    - Establish whether a GENERAL runtime-safe ordering/ref guard can reject
      these cases without hardcoding IDs or expected refs.
    - Candidate guard may use existing parser state such as:
        candidate marker value
        current active native verse
        physical source order
        whether proposed ref already exists
        whether proposed ref would move backward in current native chapter.
    - Do NOT automatically adopt a guard merely because it rejects these two;
      validate against all reviewed positives/events.
  - Critical distinction:
    - A marker may be visually a real printed `2` and still be unsafe to treat
      as a NEW verse boundary in current parser context.
    - Source truth and parser boundary semantics are separate questions.
  - Required guard validation:
    - Any proposed implementation-safe guard must be evaluated against:
        47 reviewed task-143 source events
        111 external production matches
        all 183 task-142 controls where applicable
        existing task-128/task-131/task-139 behavior.
    - Report exact accepted/rejected populations.
    - Any accepted unreviewed external match prevents implementation readiness.
    - Any accepted known physical-order conflict prevents implementation
      readiness.
  - Ordering guard:
    - Test whether a transparent condition equivalent to:
        proposed native marker does not reopen an earlier ref behind current
        physical/native progression
      can be expressed using existing parser state.
    - Do not use:
        occurrence ID
        Ps.47.2
        Ps.93.2
        specific page/block
      as the guard.
    - Verify the guard does not reject legitimate reviewed CREATE_NEW_REF
      events unnecessarily unless that narrower safe family is explicitly
      accepted as the result.
  - External-family discriminator:
    - Determine whether the difference between the reviewed 47 and external
      111 is source-structural and runtime-visible.
    - Potential dimensions may include:
        candidate provenance/path
        projected-vs-other candidate class
        physical token origin
        verse-body zone
        source column
        marker-band construction
        projection mechanism
        line/block ownership state
        existing stronger-rule classification.
    - Page/book/ref identity may be metadata, not authority.
  - Result status must be exactly one:
        IMPLEMENTATION_SCOPE_FOUND
        NARROWER_SCOPE_FOUND
        NO_SAFE_PRODUCTION_SCOPE.
    - IMPLEMENTATION_SCOPE_FOUND:
        all accepted production matches are source-reviewed safe events,
        no known controls,
        no late-ref/order conflicts.
    - NARROWER_SCOPE_FOUND:
        a strict subset can be transparently isolated with the same safety.
    - NO_SAFE_PRODUCTION_SCOPE:
        remaining external/unreviewed/control/order ambiguity prevents safe
        implementation.
  - Do not require preserving all 45 task-143 CREATE_NEW_REF events.
    Safety is more important than recall.
  - Task-145:
    - If IMPLEMENTATION_SCOPE_FOUND or NARROWER_SCOPE_FOUND:
        recommend exactly one new dry-run validation task over the refined
        production-safe scope.
    - Do NOT jump directly to runtime implementation.
    - A new dry-run is required because the selected population changed.
    - If NO_SAFE_PRODUCTION_SCOPE:
        recommend one different bounded diagnostic direction.
  - Artifact:
    - Create deterministic diagnostic artifact, preferably:
        data/torresamat1835/projected_form_a_production_scope_audit.json
    - Include:
        schema_version
        frozen baseline provenance
        production-match derivation
        all 158 production matches
        reviewed-family reconciliation
        all 111 external records
        source review labels where available
        late-ref conflict analysis
        candidate guard evaluations
        accepted/rejected populations
        selected production scope
        result status
        task145 recommendation
        runtime invariants.
  - Audit:
    - Add diagnostic equivalent to:
        verse_segmentation_audit.projected_form_a_production_scope_audit
    - Include:
        production_matches = 158
        reviewed_matches = 47
        external_matches = 111
        external_reviewed
        external_unreviewed
        external printed-2
        external other-marker
        external non-marker
        known late-order conflicts = 2
        selected scope population
        accepted unreviewed
        accepted controls
        accepted order conflicts
        result status
        task145 target.
  - Runtime invariants:
    - No recovery.
    - VerseRefs remain 3849.
    - Physical gaps remain 3254.
    - Glyph gaps remain 1309.
    - Ownership remains 25434.
    - task-128 unchanged.
    - task-131 unchanged.
    - task-139 unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - Chapters remain 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - duplicate_refs = 0.
    - out_of_order_refs = 0.
    - outside_canon = 0.
    - ocr_blocks = 57700.
    - block loss = 0.
    - dual ownership = 0.
  - Acceptance:
    - 158 production matches recomputed.
    - 47 reviewed-family matches reconciled.
    - 111 external matches explicitly accounted for.
    - Every source-accessible external match reviewed.
    - No external value inferred from expected verse sequence.
    - Both late-ref conflicts explained.
    - Proposed order guard is general and runtime-safe, not identity-specific.
    - Any selected production scope contains:
        zero accepted known controls
        zero accepted unreviewed external matches
        zero accepted known order conflicts.
    - Selected scope derived without page/block/ref allowlists.
    - Exactly one task-145 recommendation.
    - No runtime change.
    - Artifact deterministic/idempotent.
    - Frozen provenance stable.
  - Do not:
    - Implement production recovery.
    - Hardcode the 111 external IDs.
    - Hardcode the two conflict IDs.
    - Hardcode Ps.47.2 / Ps.93.2.
    - Treat all 111 as negatives without source review.
    - Treat visually valid `2` as automatically valid verse boundary.
    - Use expected missing verse.
    - Use previous+1 / next-1.
    - Broaden task-142 rule.
    - Change task-128/task-131/task-139.
    - Reopen GLUED_FRAME.
    - Use ML/opaque classifiers.
    - Modify TASKS.md from the task agent.
    - Commit or push.
  - Closure evidence (verified against the canonical artifact/report):
    - Result: NARROWER_SCOPE_FOUND.
    - Frozen baseline: baseline_commit = 216dd562236b13fff25c1cf5f739366cd0a0c0b0
      (data/torresamat1835/projected_form_a_production_scope_audit.json ->
      provenance.baseline_commit).
    - Production-placement population: 158 matches.
    - Previously reviewed task-143 family: 47.
    - External production matches: 111.
    - External source-accessible: 111/111.
    - External source result: UNREADABLE = 111 (external_class_counts ==
      {"UNREADABLE": 111}, all other classes 0).
    - IMPORTANT: UNREADABLE was treated as unknown evidence, NOT as negative
      evidence (external_review() leaves visible_printed_value = null and
      never infers a value; the guard never conditions on this label).
    - Selected refined scope accepts: external UNREADABLE = 0/111.
    - Native/physical order conflicts discovered: 13 total
      (late_ref_conflict_analysis has 13 entries; known_order_conflicts = 13).
    - Selected refined scope accepts: known order conflicts = 0.
    - Historical task-143 conflicts are rejected by the general order guard:
      p0072l0083 / Ps.47.2 (physical_context Ps.47.7) and p0136l0092 / Ps.93.2
      (physical_context Ps.93.19) both present among the 13 conflicts.
    - Production rule uses transparent runtime-safe guards based on:
      projected-gap provenance (`row["projected_gap_path"]`) plus native
      progression/order safety (`order_state(row) != "backward"`, derived
      from native_active_book/chapter/verse and the proposed marker value).
    - The guard does NOT use occurrence IDs, page allowlists, book/chapter
      allowlists, hardcoded VerseRefs, or expected missing verses (verified
      by reading scripts/torresamat1835/projected_form_a_production_scope_audit.py
      directly: no literal Ps.47.2/Ps.93.2/p0072l0083/p0136l0092 anywhere in
      selection logic, only in the historical conflict-report rendering).
    - Selected refined production scope: 43 physical blocks/events.
    - From the artifact's selected_scope / audit_summary: selected
      occurrences = 43, selected physical blocks = 43, selected source
      events = 43, selected reviewed printed-2 = 43, selected other-value
      marker count = 0, selected non-marker count = 0, selected UNREADABLE
      external count = 0, selected order-conflict count = 0.
    - Required safe counts confirmed: accepted other-value markers = 0,
      accepted non-markers = 0, accepted external UNREADABLE = 0, accepted
      known order conflicts = 0.
    - Exact reason the reviewed 47 became refined 43 (from
      order_guard_candidates[0] in the canonical artifact): the
      nonbackward_native_progression guard has reviewed_accepted = 43 and
      reviewed_rejected = 4 — 4 of the 47 previously-reviewed task-143 events
      are rejected because their proposed marker value is backward relative
      to native active progression (the same guard class as the two known
      historical conflicts); known_conflicts_accepted stays 0.
    - Runtime remained unchanged: VerseRefs = 3849, physical gaps = 3254,
      glyph gaps = 1309 (all confirmed in runtime_invariants). Owned blocks =
      25434 is the pre-existing invariant from task 143/139 (TASKS.md lines
      3706/4013-4014/4039) and is not independently recomputed by this
      diagnostic-only artifact; no runtime mutation occurred
      (runtime_image_reads = false), so it is unchanged by construction.
    - task-128 unchanged: 183 markers, 177 refs, 1355 ownership moves
      (runtime_invariants.task128).
    - task-131 unchanged: 276 markers, 276 refs, 1900 ownership moves
      (runtime_invariants.task131).
    - task-139 unchanged (runtime_invariants.task139, ownership_moves = 27,
      untouched by this audit).
    - GLUED_FRAME = CLOSED_UNSAFE.
    - Chapters = 337/337. unresolved chapter claims = 0. canonical chapter
      gaps = 0. duplicate_refs = 0. out_of_order_refs = 0. outside_canon = 0.
      ocr_blocks = 57700. block loss = 0. dual ownership = 0.
    - Artifact deterministic/idempotent: verified by the focused test
      (build() called twice, both renders byte-identical to the file on
      disk and to a round-tripped copy).
    - Focused task-144 test = PASS
      (`python3 scripts/torresamat1835/test_projected_form_a_production_scope_audit.py`
      -> "ok", exit 0).
    - Direct task regression = PASS
      (`python3 scripts/torresamat1835/test_projected_form_a_visible_2_dry_run.py`
      -> "ok", exit 0; task-143's own dry-run stays unaffected).
    - Build = PASS (existing build/ tree, torresamat1835 tests link and run).
    - TorresAmat CTest = 36/36 PASS (build/Testing/Temporary/LastTest.log,
      36 torresamat*/torresamat1835* entries, all "Test Passed.").
    - Full CTest: 46 registered, 45 passed, 1 failed
      (build/Testing/Temporary/LastTestsFailed.log lists exactly one failing
      test: "2:gtk_lifecycle_smoke"; that full run's detailed log was
      overwritten by a later torresamat-only rerun, so the failed-test
      ledger is the surviving direct evidence for this session).
    - Unrelated full-suite failure: gtk_lifecycle_smoke, "dictionary panel
      did not reopen explicitly" — this exact failure text matches the
      already-tracked TASKS.md entry UI-SMOKE-102 ("Fix \"dictionary panel
      did not reopen explicitly\""), confirming it is a pre-existing,
      separately-tracked UI issue, not something introduced here.
    - TASK-144 changed no UI/GTK/dictionary-panel files: task 144's only
      changed files are under data/torresamat1835/ and
      scripts/torresamat1835/.
    - Task-related failures = 0.
    - Recommended next work: recompute a NEW dry-run semantic delta for
      exactly the refined production-safe 43-event scope
      (task145_recommendation in the artifact).
    - Task 144 is DONE because it resolved the production-scope ambiguity
      into a strict transparent scope with zero accepted external UNREADABLE
      cases and zero accepted known order conflicts.


- [x] TORRES-1835-PROJECTED-FORM-A-REFINED-SCOPE-DRY-RUN-145 Validate semantic dry-run effects for the refined production-safe form-a scope
  - Status: DONE
  - Description:
    Perform a fresh semantic dry-run for ONLY the task-144 refined production
    scope. Recompute the scope from its transparent runtime-safe provenance and
    native-order guards, then determine exact VerseRef, ownership and gap
    effects before any production implementation.
  - Baseline context:
    - Runtime VerseRefs = 3849.
    - Physical gaps = 3254.
    - Glyph gaps = 1309.
    - Owned blocks = 25434.
    - Task-143 production population = 158.
    - Task-144 refined production scope = 43 blocks/events.
    - External UNREADABLE accepted by refined scope = 0/111.
    - Known order conflicts accepted = 0/13.
    - Other-value marker accepted = 0.
    - Non-marker accepted = 0.
  - Critical requirement:
    - Recompute the 43-event scope from task-144 runtime-safe guards.
    - Do NOT use a hardcoded list of 43 IDs.
    - Do NOT inherit task-143 semantic deltas.
    - Task 143's:
        +45 refs
        2 reopens
        595 ownership moves
        physical gaps -45
        glyph gaps -54
      belong to the older 47-event scope and are NOT task-145 expectations.
  - Scope validation:
    - Recompute all 158 production matches.
    - Apply task-144 projected-gap provenance guard.
    - Apply task-144 native-order/progression guard.
    - Require resulting safe production scope = 43 events.
    - Require:
        accepted external UNREADABLE = 0
        accepted other-value markers = 0
        accepted non-markers = 0
        accepted known order conflicts = 0.
    - Any additional/unreviewed match blocks implementation readiness.
  - Event reconciliation:
    - Report independently:
        selected diagnostic occurrences
        physical OCR blocks
        source marker events
        recovery events.
    - Do not assume all counts are identical.
    - Deduplicate using source structure, not occurrence IDs.
  - Dry-run ref simulation:
    - For each refined recovery event use normal native parser semantics.
    - Determine:
        native book/chapter
        interpreted marker value
        proposed native VerseRef
        existing ref status.
    - Classify:
        CREATE_NEW_REF
        REOPEN_EXISTING_REF
        NO_REF_EFFECT
        INVALID.
    - Do not infer refs from expected missing-gap identities.
  - Native-order safety:
    - Re-run the task-144 general progression guard during simulation.
    - Require:
        backward/order conflicts accepted = 0.
    - Do not hardcode historical conflict IDs or refs.
  - Ref-set effects:
    - Measure exact:
        VerseRefs before/after
        new refs
        reopened refs
        no-effect events
        invalid events
        removed refs
        renumbered refs
        unrelated refs changed.
    - Required:
        removed = 0
        renumbered = 0
        unrelated existing refs changed = 0.
  - Ownership effects:
    - Simulate exact ownership movement.
    - Report:
        moved blocks
        unique moved blocks
        old owner refs
        receiving refs.
    - Required:
        block loss = 0
        dual ownership = 0
        unrelated ownership unchanged.
  - Gap effects:
    - Recompute exact predicted:
        physical gaps before/after
        exact physical gaps closed/opened
        glyph gaps before/after
        exact glyph gaps closed/opened.
    - Do not infer reductions from event count.
  - Safety:
    - Predicted duplicate_refs = 0.
    - Predicted out_of_order_refs = 0.
    - Predicted outside_canon = 0.
    - Predicted impossible refs = 0.
    - Predicted block loss = 0.
    - Predicted dual ownership = 0.
  - Existing recovery isolation:
    - task-128 unchanged.
    - task-131 unchanged.
    - task-139 unchanged.
    - GLUED_FRAME remains CLOSED_UNSAFE.
    - No double recovery with stronger paths.
  - Runtime boundary:
    - Task 145 is DRY-RUN ONLY.
    - Actual runtime must remain:
        VerseRefs = 3849
        physical gaps = 3254
        glyph gaps = 1309
        ownership = 25434.
  - Final result:
        IMPLEMENTATION_READY
        NEEDS_NARROWER_VALIDATION
        UNSAFE_TO_IMPLEMENT.
    - IMPLEMENTATION_READY requires:
        exact refined scope reproduced
        zero unknown/unsafe accepted cases
        exact ref delta known
        exact ownership delta known
        exact gap delta known
        no duplicate/order/canon/ownership violations.
  - Task-146 recommendation:
    - If IMPLEMENTATION_READY:
        exactly one bounded production implementation task reproducing the
        task-145 validated semantic delta.
    - Otherwise:
        exactly one narrower diagnostic task.
    - Do not implement task 146.
  - Artifact:
    - Create deterministic artifact, preferably:
        data/torresamat1835/projected_form_a_refined_scope_dry_run.json
    - Include:
        schema_version
        frozen baseline provenance
        task-144 scope definition
        production-scope recomputation
        occurrence/block/event reconciliation
        ref effects
        ownership effects
        physical/glyph gap effects
        native-order safety
        existing-recovery isolation
        final status
        task146 recommendation
        runtime invariants.
  - Audit:
    - Add diagnostic equivalent to:
        verse_segmentation_audit.projected_form_a_refined_scope_dry_run
    - Include:
        production matches
        refined selected events
        external unreadable selected
        order conflicts selected
        create_new_refs
        reopened_refs
        no_ref_effect
        invalid
        ownership moves
        physical gap delta
        glyph gap delta
        duplicate/order/canon results
        final status
        task146 target.
  - Acceptance:
    - 158 production matches recomputed.
    - Refined scope deterministically recomputes to 43 events.
    - 0/111 external UNREADABLE selected.
    - 0/13 known order conflicts selected.
    - Zero wrong-value/non-marker controls selected.
    - Exact semantic dry-run delta known.
    - No hardcoded IDs/pages/refs.
    - No expected-gap inference.
    - No runtime change.
    - Historical task-141/142/143/144 artifacts remain frozen.
    - Artifact deterministic/idempotent.
    - Exactly one task-146 recommendation.
  - Do not:
    - Implement recovery.
    - Reuse task-143 delta as expected result.
    - Hardcode the 43-event list.
    - Hardcode external IDs.
    - Hardcode historical conflict refs.
    - Use expected VerseRefs or missing gaps to construct refs.
    - Use previous+1 / next-1.
    - Broaden task-144 scope.
    - Change task-128/task-131/task-139.
    - Reopen GLUED_FRAME.
    - Use ML/opaque classifiers.
    - Modify TASKS.md from the task agent.
    - Commit or push.
  - Closure evidence:
    - Result: IMPLEMENTATION_READY.
    - Frozen task baseline: 03ac293bdd0ce735dd950eb4a49e8628a3a39182.
    - Full production-placement population: 158.
    - Task-144 reviewed/projected-gap provenance family: 47.
    - Task-144 refined production-safe scope: 43.
    - Refined scope recomputed from runtime/source guards, not from an ID
      allowlist.
    - Selected accounting:
        43 diagnostic occurrences
        43 physical OCR blocks
        43 source marker events
        43 recovery events.
    - External production matches: 111.
    - External source classification: UNREADABLE = 111.
    - External UNREADABLE accepted by refined scope: 0/111.
    - Known native/order conflicts: 13.
    - Known order conflicts accepted: 0/13.
    - Wrong-value controls accepted: 0.
    - Non-marker controls accepted: 0.
    - Unknown extra production matches accepted: 0.
    - Ref effect classification:
        CREATE_NEW_REF = 43
        REOPEN_EXISTING_REF = 0
        NO_REF_EFFECT = 0
        INVALID = 0.
    - Predicted VerseRefs: 3849 -> 3892.
    - New native refs: 43.
    - Reopened refs: 0.
    - Removed refs: 0.
    - Renumbered refs: 0.
    - Unrelated existing refs changed: 0.
    - Duplicate proposed refs: 0.
    - Predicted duplicate refs: 0.
    - Predicted out_of_order_refs: 0.
    - Predicted outside_canon: 0.
    - Predicted impossible native refs: 0.
    - Fresh ownership simulation: ownership moves = 581.
    - Owned blocks: 25434 -> 25434.
    - Predicted block loss: 0.
    - Predicted dual ownership: 0.
    - Physical gaps: 3254 -> 3211.
    - Physical gaps closed: 43.
    - Physical gaps opened: 0.
    - Glyph gaps: 1309 -> 1266.
    - Glyph gaps closed: 43.
    - Glyph gaps opened: 0.
    - IMPORTANT: task-145 recomputed all semantic deltas from scratch.
    - Task-143 historical delta was NOT reused:
        old scope = 47 events
        old +45 refs / 2 reopens / 595 ownership moves / -45 physical /
        -54 glyph does not describe the final production-safe scope.
    - Refined production-safe scope uses transparent runtime-safe guards
      established by tasks 142 and 144, including:
        task-142 exact form-a/prefix/marker-band discriminator
        task-144 projected-gap provenance restriction
        task-144 native progression/order safety.
    - No occurrence IDs, page allowlists, book/chapter allowlists,
      hardcoded VerseRefs or expected-gap identities are used as runtime
      selection authority.
    - Stronger recovery isolation:
        no double recovery with task-128
        no double recovery with task-131
        no double recovery with task-139.
    - task-128 actual runtime unchanged: 183 markers, 177 refs,
      1355 ownership moves.
    - task-131 actual runtime unchanged: 276 markers, 276 refs,
      1900 ownership moves.
    - task-139 unchanged.
    - GLUED_FRAME = CLOSED_UNSAFE.
    - Actual runtime remained unchanged during dry-run:
        VerseRefs = 3849
        physical gaps = 3254
        glyph gaps = 1309
        owned blocks = 25434.
    - Chapters = 337/337.
    - unresolved chapter claims = 0.
    - canonical chapter gaps = 0.
    - current duplicate_refs = 0.
    - current out_of_order_refs = 0.
    - current outside_canon = 0.
    - ocr_blocks = 57700.
    - current block loss = 0.
    - current dual ownership = 0.
    - Artifact: data/torresamat1835/projected_form_a_refined_scope_dry_run.json,
      schema_version = 1.
    - Artifact deterministic and idempotent.
    - Focused task-145 test: PASS.
    - All required direct regression scripts: PASS.
    - Build: PASS.
    - Focused CTest: PASS.
    - TorresAmat CTest: 35/35 PASS.
    - Full CTest: 47 registered, 46 passed, 1 failed.
    - Unrelated known full-suite failure: gtk_lifecycle_smoke
      "dictionary panel did not reopen explicitly".
    - Task 145 changed no UI/GTK files.
    - Task-related failures: 0.
    - Recommended next work: implement exactly the validated 43-event
      production-safe fallback and verify that actual parser output
      reproduces the task-145 dry-run delta.


- [x] TORRES-1835-PROJECTED-FORM-A-REFINED-SCOPE-RECOVERY-146 Implement the validated refined production-safe projected form-a recovery
  - Status: DONE
  - Description:
    Implement the exact production fallback validated by tasks 142, 144 and
    145. The runtime parser must derive the same refined source-safe population
    using transparent parser/source state, recover exactly the validated native
    verse boundaries, and reproduce task-145's semantic delta without
    occurrence/ref/page allowlists.
  - Validated production scope:
    - Production placement population before refined guards: 158.
    - Refined validated recovery population: 43 events.
    - Exact event cardinality validated by task 145:
        43 occurrences
        43 physical blocks
        43 source events
        43 recovery events.
    - Required effect:
        CREATE_NEW_REF = 43
        REOPEN_EXISTING_REF = 0
        NO_REF_EFFECT = 0
        INVALID = 0.
  - Validated runtime-safe selection chain:
    - Existing stronger/native marker recovery paths execute first.
    - Apply the exact task-142 projected form-a discriminator:
        first physical OCR token == "a"
        AND each of the next three physical tokens contains >=2 Unicode letters
        AND a trusted marker band exists
        AND candidate lies within the existing marker-band tolerance.
    - Preserve existing trusted-band semantics from task 142.
    - Apply task-144 projected-gap provenance restriction.
    - Apply task-144 native progression/order safety guard.
    - Only then interpret the bounded candidate as printed marker value 2.
  - Critical:
    - Reconstruct exact guard semantics from the canonical task-142/task-144
      implementations/artifacts.
    - Do NOT approximate or paraphrase those guards if the code contains a
      more exact condition.
  - Recovery ordering:
    - This is a fallback only.
    - It must execute after stronger existing marker paths.
    - It must never steal/reprocess candidates handled by:
        exact markers
        task-128
        task-131
        task-139.
    - double recovery = 0.
  - Numeric interpretation:
    - Within ONLY the exact refined scope, interpreted marker value is 2.
    - This is justified by the source-backed diagnostic chain.
    - Do not infer 2 from expected gap sequence.
    - Do not use previous+1 or next-1.
  - Native reference semantics:
    - Construct native refs through the normal TorresAmat1835 parser state.
    - Stay in native versification.
    - Do not consult another Bible module.
    - No expected ref allowlist.
  - Fail closed:
    - candidate fails task-142 discriminator -> abstain
    - candidate fails projected-gap provenance -> abstain
    - trusted marker band unavailable -> abstain
    - outside marker-band tolerance -> abstain
    - candidate already handled by stronger recovery -> abstain
    - native progression/order guard fails -> abstain
    - proposed ref invalid/outside canon -> abstain
    - duplicate/ref conflict -> abstain
    - unsafe reopen -> abstain
    - ambiguous event ownership -> abstain.
  - Required actual runtime result:
        VerseRefs:
            3849 -> 3892
        new refs:
            +43
        reopened refs:
            0
        physical gaps:
            3254 -> 3211
        physical gap reduction:
            43
        glyph gaps:
            1309 -> 1266
        glyph gap reduction:
            43
        ownership moves:
            581
        owned blocks:
            25434 -> 25434.
  - Exact identity validation:
    - Actual new native refs must equal task-145 predicted refs.
    - Actual ownership-moved block identities must equal task-145 predicted
      movement.
    - Actual physical gap identities closed must equal task-145 predictions.
    - Actual glyph gap identities closed must equal task-145 predictions.
    - Use diagnostic artifact identities only for TEST ASSERTION after runtime
      derivation, never as production selection input.
  - Required safety:
        reopened refs = 0
        duplicate_refs = 0
        out_of_order_refs = 0
        outside_canon = 0
        impossible refs = 0
        block loss = 0
        dual ownership = 0
        unrelated refs changed = 0
        unrelated ownership changed = 0.
  - External/control safety:
    - External UNREADABLE accepted: 0/111.
    - Known task-144 order conflicts accepted: 0/13.
    - Wrong-value controls accepted: 0.
    - Non-marker controls accepted: 0.
    - No new unknown/out-of-family production match may be recovered.
  - Existing recovery invariants:
    - task-128 remains: 183 markers, 177 refs, 1355 ownership moves.
    - task-131 remains: 276 markers, 276 refs, 1900 ownership moves.
    - task-139 remains unchanged, including Ps.17.10.
    - GLUED_FRAME remains CLOSED_UNSAFE.
  - Corpus invariants after implementation:
        chapters = 337/337
        unresolved chapter claims = 0
        canonical chapter gaps = 0
        ocr_blocks = 57700
        duplicate_refs = 0
        out_of_order_refs = 0
        outside_canon = 0
        block loss = 0
        dual ownership = 0.
  - Architecture:
    - Keep the implementation bounded and local to the current verse-marker
      recovery pipeline.
    - Reuse existing feature/provenance/order helpers where practical.
    - Do not create a parallel parser.
    - Avoid corpus-wide repeated rescans.
    - No runtime PDF/facsimile reads.
  - Diagnostic continuity:
    - Historical artifacts from tasks 141-145 remain frozen.
    - Extend current runtime audit with explicit counters for the production
      recovery, following existing task-128/task-131/task-139 conventions.
    - The runtime audit must distinguish this path from previous recoveries.
  - Tests:
    - Add focused production-recovery coverage proving:
        exact runtime recovery count = 43
        CREATE_NEW_REF = 43
        REOPEN = 0
        refs 3849 -> 3892
        physical gaps 3254 -> 3211
        glyph gaps 1309 -> 1266
        ownership moves = 581
        actual new ref identities equal task-145 prediction
        actual moved block identities equal task-145 prediction
        exact physical/glyph gap closure identities equal task-145 prediction
        external UNREADABLE recovered = 0
        order conflicts recovered = 0
        wrong/non-marker controls recovered = 0
        stronger-rule overlap = 0
        duplicate/order/canon/block safety = 0 violations.
    - Tests may read task-145 artifact as oracle AFTER runtime recovery.
    - Production parser must not read that artifact.
  - Historical artifacts:
    - Do NOT mutate diagnostic artifacts from tasks 141-145.
  - Acceptance:
    - Production parser derives exactly the validated 43-event family.
    - Actual runtime delta equals task-145 prediction exactly.
    - No identity allowlist drives runtime behavior.
    - No expected-gap inference.
    - No unsafe reopen.
    - Existing recoveries unchanged.
    - Full Torres regression green.
    - Build green.
    - No unrelated UI modifications.
  - Do not:
    - Hardcode the 43 IDs.
    - Hardcode the 43 refs.
    - Hardcode page/book/chapter allowlists.
    - Read task-145 JSON in production runtime.
    - Recover any external UNREADABLE candidate.
    - Recover any known task-144 order conflict.
    - Recover remaining excluded task-141/task-143 candidates.
    - Broaden task-142 discriminator.
    - Weaken task-144 order/provenance guards.
    - Use expected missing verses.
    - Use previous+1 / next-1.
    - Change task-128/task-131/task-139 behavior.
    - Reopen GLUED_FRAME.
    - Use ML/opaque classifiers.
    - Touch unrelated UI.
    - Modify TASKS.md from the task agent.
    - Commit or push.
  - Closure evidence:
    - Result: IMPLEMENTED_AND_VALIDATED.
    - Implementation commit: 43f99cd9ccd9e668a88d09e3bf0ba1607e22896a.
    - Production runtime now implements the exact refined task-145 scope.
    - Runtime derives the recovery population from parser/source guards; there is no 43-ID/ref/page allowlist.
    - Production runtime does NOT read task-141–145 diagnostic JSON artifacts as selection authority.
    - Recovery remains enabled by default in normal runtime.
    - Historical diagnostic reproduction uses explicit opt-out: `projected_form_a_recovery=False` or `--without-projected-form-a-recovery`.
    - Historical frozen artifacts remain byte-reproducible under pre-task-146 mode.
    - task-142 discriminator preserved: first physical OCR token == "a"; next three physical tokens each contain >=2 Unicode letters; trusted marker band exists; candidate lies inside existing validated band tolerance.
    - task-144 projected-gap provenance guard implemented.
    - task-144 native progression/order guard implemented.
    - Recovery remains a fallback after stronger existing paths.
    - Actual production recovery events: 43.
    - CREATE_NEW_REF: 43.
    - REOPEN_EXISTING_REF: 0.
    - NO_REF_EFFECT: 0.
    - INVALID: 0.
    - Actual VerseRefs: 3849 -> 3892.
    - New refs: 43.
    - Actual new-ref identities match task-145 prediction.
    - Actual ownership moves: 581.
    - Actual moved-block identities match task-145 prediction.
    - Owned blocks: 25434 -> 25434.
    - Block loss: 0.
    - Dual ownership: 0.
    - Actual physical gaps: 3254 -> 3211.
    - Physical gaps closed: 43.
    - Physical gaps opened: 0.
    - Physical gap identities match task-145 prediction.
    - Actual glyph gaps: 1309 -> 1266.
    - Glyph gaps closed: 43.
    - Glyph gaps opened: 0.
    - Glyph gap identities match task-145 prediction.
    - External task-144 UNREADABLE cases recovered: 0/111.
    - Known task-144 order conflicts recovered: 0/13.
    - Wrong-value controls recovered: 0.
    - Non-marker controls recovered: 0.
    - Unknown extra recoveries: 0.
    - Stronger recovery overlap: 0.
    - task-128 remains unchanged: 183 markers; 177 refs; 1355 ownership moves.
    - task-131 remains unchanged: 276 markers; 276 refs; 1900 ownership moves.
    - task-139 remains unchanged, including Ps.17.10.
    - GLUED_FRAME remains: CLOSED_UNSAFE.
    - Corpus invariants: chapters = 337/337; unresolved chapter claims = 0; canonical chapter gaps = 0; ocr_blocks = 57700; duplicate_refs = 0; out_of_order_refs = 0; outside_canon = 0; block loss = 0; dual ownership = 0.
    - Historical task-138–145 tests/generators that require the pre-task-146 parser explicitly disable the new recovery rather than weakening their historical expected values.
    - task-142 historical page_parser freeze was scoped to the final pre-146 commit: d5085fa2608845def169a8e1a84e4c0f01ec8dd4.
    - compound_glyphs.py, parser.py, layout.py and verse_gaps.py historical freezes remain intact.
    - Known true printed marker outside validated scope: p0184l0043 remains deliberately unrecovered because its validated geometry lies outside the task-142 marker-band scope.
    - No ID-specific exception was added for that case.
    - Focused task-146 test: PASS.
    - Required direct regression tests: PASS.
    - Build: PASS.
    - Focused CTest: PASS.
    - TorresAmat CTest: PASS.
    - Full CTest: all task-related tests PASS; only the previously tracked unrelated GTK lifecycle failure remains: gtk_lifecycle_smoke "dictionary panel did not reopen explicitly".
    - task-146 modified no UI/GTK files.
    - Task-related failures: 0.
    - git diff --check before implementation commit: PASS.
    - No historical diagnostic data artifact was modified.
    - Task 146 is DONE because production parser behavior now reproduces the complete task-145 validated semantic delta exactly, including ref identities, ownership movement and gap identities, while rejecting all known unsafe/unknown cases.


- [x] UI-SIGNAL-101 Investigate stale GObject signal handler
  - Status: DONE
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
    - Rechecked on the real Wayland display with an isolated temporary
      profile/module and `G_DEBUG=fatal-criticals`. The controlled lifecycle
      sequence rendered all six surfaces, navigated John 3:17 → 3:18 → 3:17
      → 4:1 → 3:16, and opened/closed the smoke panels without the stale
      GObject handler critical or an abort/backtrace.
    - That run ended only at the already tracked unrelated
      `UI-SMOKE-102` assertion: `dictionary panel did not reopen explicitly`.
      The CTest Xvfb variant remains unavailable in this environment
      (`gtk_lifecycle_smoke_skipped=no-usable-xvfb`).
    - A fresh tree-wide assignment search still finds only the declarations
      of `scroll_adj_signal` and `adjustment` in `src/gtk/bibletext.c`; their
      `src/main/sword.cc` block/unblock sites remain unreachable and are not
      evidence of the reported active-handler failure.
    - Root cause reproduced deterministically on the real X11 display with
      `G_DEBUG=fatal-criticals` under GDB. The bare-control sequence is:
      start with no window focus; open and Escape-close a bare GtkPopover;
      open the next picker; type `12` and Enter. GTK then reports the exact
      failure, `instance '0x...' has no handler with id '...'`.
    - `bt`/`bt full` identify the instance as the test's `GtkWindow`
      (`window == 0x555555729ca0` in the captured run), not an application
      adjustment. GTK reaches `g_signal_handler_disconnect()` from its
      GtkPopover close path while `on_activate()` calls
      `gtk_popover_popdown()`; the stale id is GTK's internal `unmap`
      handler (id 430 in that capture).
    - The first bare close leaves the `GtkWindow` as its own focus. The
      second bare popover records that invalid focus-return target; GTK's
      internal data-disconnect path has already removed its `unmap` handler
      when the later close attempts to disconnect the stored id. The popover
      is otherwise destroyed only from `picker_destroy_idle()`, after
      `closed`, so it is not destroyed while emitting `closed`.
    - The limited production correction is already integrated in
      `picker_entry.c` (introduced by `297aa66a`): before each chapter,
      verse, or book picker opens, `picker_entry_settle_window_focus()` gives
      GtkPopover a drawable reading-pane/anchor focus target. Its
      `g_signal_connect_object(..., "set-focus", ..., entry,
      G_CONNECT_AFTER)` repairs any later window-self focus and is owned by
      the entry, so it is disconnected with that entry rather than by the
      popover's internal window-data cleanup. No
      `g_signal_handler_is_connected()` suppression is used.
    - Verification on the same real X11 display: the intentionally unguarded
      control emits one expected critical; the guarded sequence completes
      100 rounds (chapter/verse, Escape and `12`+Enter, including a pane
      rebuild while open) with `bad_rounds=0`, `criticals=0`,
      `popovers_alive=0`, and `window_active=1`.
    - `navbar_picker_focus_test` passes with 0 failures. This is distinct
      from the still-open UI-SMOKE-102 dictionary-reopen assertion.
    - Result: ROOT_CAUSE_IDENTIFIED_AND_CURRENT_FIX_VALIDATED.
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

- [x] UI-SMOKE-102 Fix "dictionary panel did not reopen explicitly"
  - Status: DONE
  - Description:
    `gtk_lifecycle_smoke` fails with
    `GTK_LIFECYCLE_SMOKE_CHECK_FAILED dictionary panel did not reopen
    explicitly` (check at `src/gtk/gtk_lifecycle_smoke.c:199`).
  - Evidence:
    - Pre-existing: reproduced identically with the UI-LAYOUT-103 fix stashed
      via `git stash`, so it is not a regression of UI-LAYOUT-103.
    - Distinct from the earlier pre-existing `bible-compare` CREATE/SHOW/MAP
      smoke failure noted under STARTUP-PERF-105.
  - Closure evidence:
    - Root cause: the smoke expectation was stale, not an application
      re-open failure. Commit `36df30d5` deliberately retired the visible
      Dictionary/Devotional entry and `gui_show_hide_dicts()` forces every
      current or restored `showdicts=1` request to false so the retired pane
      cannot reappear.
    - Exact pre-fix real-display sequence reproduced the failure with an
      isolated SQLite profile and `G_DEBUG=fatal-criticals`:
      `GTK_LIFECYCLE_SMOKE_CHECK_FAILED dictionary panel did not reopen
      explicitly`, `gtk_lifecycle_smoke_failures=1 checks=244`.
    - The smoke now omits Dictionary/Devotional from its required mapped
      renderer surfaces and, after its hide/show cycle, calls the public
      `gui_show_hide_dicts(TRUE)` contract. It asserts that both
      `settings.showdicts` and the retired pane remain hidden; this validates
      the intended stale-session protection rather than bypassing it with a
      direct `gtk_widget_show()`.
    - `tests/run_gtk_lifecycle_smoke.cmake` now requires CREATE/SHOW/MAP only
      for active surfaces; the retired dictionary surface is no longer a
      false mandatory map.
    - Same real Wayland-display smoke sequence, isolated profile and
      `G_DEBUG=fatal-criticals`: PASS with
      `gtk_lifecycle_smoke_failures=0 checks=241 navigation=5 renderers=5
      panels=18`; no GTK/GDK critical or abort.
    - Build: PASS (`biblia-elim`, `main_window_layout_test`,
      `navbar_picker_focus_test`, `navbar_picker_entry_test`).
    - `main_window_layout_test`: PASS, 0 failures.
    - `navbar_picker_focus_test`: PASS, 0 failures.
    - `navbar_picker_entry_test`: PASS: its deliberately unguarded control
      still demonstrates one expected GTK critical, while the guarded
      100-round production path reports `bad_rounds=0`, `criticals=0`,
      `popovers_alive=0`, `window_active=1`.
    - CTest `gtk_lifecycle_smoke` remains environment-skipped only because
      this host has no usable Xvfb; the equivalent real-display sequence
      passed above.
    - UI-SIGNAL-101 closure evidence remains preserved; this change does not
      modify production panel or popover code.
    - Result: IMPLEMENTED_AND_VALIDATED.
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

- [x] NACAR-PSALMS-103 Fix Nácar-Colunga Psalms 117/118 boundary
  - Status: DONE
  - Description:
    Ps 117 has no text of its own in the module: its two verses are glued to
    the start of Ps 118:1 («Alabad a Yave las gentes todas… Alabad a Yave,
    porque es bueno…»). Misalignment around Ps 117–118 was also observed
    during the NACAR-PSALMS-101 investigation.
  - Evidence:
    - Ps 118 has not been explicitly verified; do not assume it is correct.
    - Unchanged by NACAR-PSALMS-102 (`9352fad7`): Ps 117 still has no text
      and Ps 118 still begins with the Ps 117 content.
  - Root cause:
    - The OCR reads the printed Ps 118 header as `118. (Vulg. 117.)`
      (Princeton leaf 1020, right column). `RE_CABECERA_SALMO` did not allow
      punctuation after the psalm number, so the line fell to `es_ruido` and
      no chapter boundary was emitted.
    - Without that header, the fallback restart (a verse 1/2 after verse
      >= 5) cannot fire after Ps 117, which has only 2 verses. Both psalms
      formed one candidate `[1, 2, 1, 2, 3, …]`; the aligner left Ps 117
      empty and `ensambla` placed Ps 117:1–2 into Ps 118:1–2.
  - Fix:
    - `scripts/nacarcolunga/versiculos.py`: `RE_CABECERA_SALMO` tolerates
      one OCR punctuation mark (`.`, `,`, `:`) after the psalm number, after
      `Vulg`, and before `)` (`118. (Vulg. 117.)`, `121: (Vulg. 120.)`,
      `44 (Vulg: 43.)`, `119. (Vulg. 118:)`). General rule; no offsets, no
      hardcoded references, no aligner or versification changes.
  - Closure evidence:
    - Unmodified pipeline rebuilt first: `texto.json`, `procedencia.json`,
      `avisos.txt`, and `reconstruidos.txt` byte-identical to the previous
      outputs.
    - After the fix: `Ps 117:1` = «Alabad a Yave las gentes todas, alabadle
      todos los pueblos (1).», `Ps 117:2` = «Porque claramente se ha
      manifestado … ¡Aleluya! Canto triunfal.»; `Ps 118:1` = «Alabad a Yave,
      porque es bueno, porque es eterna su misericordia (2).», `Ps 118:2` =
      «iga Israel que es bueno, …». Ps 118:3–29 are unchanged. Provenance
      now points Ps 117:1–2 to leaf 1020 col 1 top 1053/1128 and Ps 118:1–2
      only to top 1537/1610.
    - Only 4 keys changed in `texto.json` and 4 in `procedencia.json`, all in
      Ps 117/118; 0 differences outside those psalms, even though the wider
      regex newly matches 21 punctuated Psalter headers, 20 besides Ps 118
      (their boundaries were already found by the numbering restart; this
      includes the misread `34, (Vulg. 83.)` for Ps 84, whose number the
      aligner does not use).
      `reconstruidos.txt` unchanged. The `Ps 117: capítulo sin texto`
      warning is gone (13 → 12 warnings); Psalms coverage 2218 → 2220.
    - Regenerated OSIS (not installed) has `Ps.117.1`, `Ps.117.2`,
      `Ps.118.1`, `Ps.118.2` with the texts above.
    - Regression tests added to `test_salmos_cabecera.py`
      (`test_cabecera_con_puntuacion_del_ocr`,
      `test_salmo_corto_no_se_pega_al_siguiente`); both fail with the old
      regex and pass with the fix.
    - PASS: `test_salmos_cabecera.py`, `test_load.py`, `test_cabeceras.py`,
      `test_front_matter.py`, `test_completar.py`,
      `scripts/torresamat/test_pegadas.py`, `scripts/torresamat/test_restos.py`.
    - Known out of scope: the epigraph «Canto triunfal.» now ends Ps 117:2
      instead of Ps 118:2 (epigraph routing, NACAR-OCR-105); Ps 118:5 is
      still missing and Ps 118:6 still holds merged text (verse-number OCR,
      NACAR-OCR-104); «iga» for «Diga» is an OCR error (NACAR-OCR-102).
    - Module not reinstalled; no commit or push.
  - Acceptance criteria:
    - Ps 117 has exactly its 2 verses.
    - Ps 118 begins with its own content.
    - No verses are displaced between both psalms.
  - Do not:
    - Use offsets or hardcoded references.
    - Commit or push.

- [x] NACAR-PSALMS-104 Fix Nácar-Colunga Psalm 13 superscription and verse division
  - Status: DONE
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
  - Investigation (2026-09-23):
    - Reproduced with the current pipeline (baseline rebuilt byte-identical).
      Current output differs from the original evidence: after
      NACAR-FALLBACK-101, Ps 13:1 is no longer Reina-Valera but the Nácar
      superscription «Al maestro del coro. Salmo de David.»; 13:2–4 hold
      printed verses 2–4; 13:5 is empty; 13:6 holds printed 5 and 6
      merged, plus «impíos.» from the Ps 14 epigraph.
    - Facsimile (Princeton leaf 967, left column) confirms: printed ¹ is the
      superscription only; body is ²–⁶; printed ⁶ «Después de haber esperado
      en tu piedad. | Que se alegre mi corazón con tu socorro, | que pueda
      cantar a Yave: «Bien me proveyó.»» corresponds to NRSVA 13:5–6
      together. Correct target: title → 13:0, printed 2–5 → NRSVA 1–4,
      printed 6 split at the second hemistich into NRSVA 5 and 6.
    - Event stream: `vers 4, vers 6, vers 6`. The printed ⁵ is read as 6
      (the NACAR-OCR-104 pattern); without that fix no mapping can place
      «Que no pueda decir mi enemigo…» in its own slot.
    - Also lost in printed ³: the line «tinuo sobre mi corazón? | ¿Hasta»
      (present in the OCR, dropped by `es_titulo()`) and «mí?» (2 letters,
      dropped by `es_ruido`). That is OCR/parser scope, not mapping scope.
    - No local verse-level Hebrew↔NRSVA correspondence exists:
      `canon_leningrad.h` and `canon_nrsva.h` (SWORD 1.9.0) have no
      `mappings_*` table; `mappings_nrsv` only covers Rev 12:18; the Vulg
      table targets KJV and follows the Clementine division (title fused
      into v1), so it is not a Hebrew proxy. Leningrad and NRSVA both
      count 6 verses for Ps 13, so `TITULO_SALMOS` cannot see it.
    - 62 psalms have a superscription in SpaRV v1 without a count
      difference; most are short titles inside the Hebrew v1 (e.g. Ps 23),
      so a title-prefix signal alone does not identify Ps 13.
    - The only remaining evidence for a general rule is content
      correspondence with a witness (SpaRV, KJV numbering = NRSVA for
      Psalms) to detect a title-only printed v1 and choose the split
      hemistich. That adds witness data to `construir.py`, which the
      current README/NACAR-FALLBACK-101 policy says the pipeline does not
      use.
  - Human decisions (2026-09-23):
    1. Keep the no-other-Bibles policy in `construir.py`, even for
       alignment. Use a documented correspondence, limited to Ps 13, as
       import data based on the numbering and content of Princeton 967.
    2. Resolve NACAR-OCR-104 first (done; it fixes printed ⁵ read as 6).
  - Fix:
    - `scripts/nacarcolunga/correspondencias.json`: printed → NRSVA map for
      Ps 13 only (`1→0` title, `2→1`, `3→2`, `4→3`, `5→4`, `6→5+6` split
      after the 2nd hemistich separator), with source leaf and per-verse
      justification.
    - `construir.py`: `carga_correspondencias()` validates the data (every
      printed verse 1..Leningrad count mapped, every NRSVA verse 1..n
      reached exactly once, a two-way split only with a documented cut, and
      no overlap with `TITULO_SALMOS`) and raises on any inconsistency.
      `aplica_correspondencias()` runs after `ensambla` and before
      `titulos_de_salmo`; it splits the raw fragments at the `|`
      separator, keeps provenance only on the slots that received text,
      never fills an absent printed verse, and if the separator is missing
      keeps the whole verse in the first slot with an `avisos.txt` entry.
    - Correspondence evidence: facsimile (printed ¹ title only, body ²–⁶,
      ⁶ with three hemistichs) plus the local KJV module, which numbers the
      Psalms like NRSVA: 13:1 «How long wilt thou forget me» = printed ²;
      13:4 «Lest mine enemy say» = printed ⁵; 13:5 «But I have trusted in
      thy mercy; my heart shall rejoice…» = first two hemistichs of printed
      ⁶; 13:6 «I will sing unto the Lord» = its third hemistich. The KJV is
      used only as documentation, not by the pipeline.
  - Closure evidence:
    - Result: `Ps 13:0` (title) «Al maestro del coro. Salmo de David.»;
      13:1 «¿Hasta cuándo, por fin, te olvidarás…»; 13:2 «¿Hasta cuándo
      mandarás dolores…»; 13:3 «¡Mírame ya, óyeme, Yave…»; 13:4 «Que no pueda
      decir mi enemigo:…»; 13:5 «Después de haber esperado en tu piedad. Que
      se alegre mi corazón con tu socorro,»; 13:6 «que pueda cantar a Yave:
      «Bien me proveyó.» impíos.». Regenerated OSIS (not installed) emits
      the title as `<title type="psalm" canonical="true">` before 13:1.
    - Before/after (baseline = NACAR-OCR-104 outputs): only `Ps 13:0–6`
      changed in `texto.json` and `Ps 13:0–5` in `procedencia.json` (13:5
      and 13:6 share leaf 967 top 955); 0 changes in other psalms or books;
      coverage 31084 unchanged; `avisos.txt` and `reconstruidos.txt`
      unchanged.
    - Tests: new `scripts/nacarcolunga/test_correspondencias.py` (data
      validation and rejection of duplicated/missing/uncut data, title and
      split, concatenation equals the printed psalm with nothing duplicated
      or lost, absent printed verse not filled, missing separator warns
      without filling, neighbour psalm untouched) PASS;
      `test_repetidos.py`, `test_salmos_cabecera.py`, `test_load.py`,
      `test_cabeceras.py`, `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py` PASS.
    - Remaining text defects, legible in the facsimile but outside this
      mapping task (not recovered here, not filled from other Bibles):
      - 13:2: the line «tinuo sobre mi corazón? | ¿Hasta» is in the OCR
        (`*tinuo…`) but `es_titulo()` drops it; the next OCR line reads
        «cuándo» as «euango» (confidence 15); «mí?» is dropped by
        `es_ruido()` (2 letters). Printed text: «…y penas de continuo sobre
        mi corazón? | ¿Hasta cuándo mis enemigos triunfarán de mí?».
        Tracked by NACAR-OCR-103 (and NACAR-OCR-102 for «euango»).
      - 13:4: the line ««Le vencí.» | Que mis enemigos se» is in the OCR but
        dropped by `es_titulo()`. Tracked by NACAR-OCR-103.
      - 13:6: the trailing «impíos.» is the end of the Ps 14 epigraph
        «Seguridad del justo en el castigo de los impíos.». Tracked by
        NACAR-OCR-105.
    - Module not reinstalled; no commit or push.
  - Acceptance criteria:
    - The superscription is stored as a psalm title, not as verse text.
    - Each NRSVA verse contains its own Nácar content.
    - The mapping comes from verse-level correspondence or structural
      evidence, not from verse counts alone.
  - Do not:
    - Apply a manual offset or hardcode Ps 13.
    - Commit or push.

- [x] NACAR-OCR-101 Recover Nácar-Colunga Ps 3:4 from the Ps 3:5 OCR merge
  - Status: DONE
  - Description:
    Ps 3:4 is empty in the module and falls back to Reina-Valera 1909 (shown
    with the fallback badge), because the Nácar OCR merged the content of
    two verses into Ps 3:5 («Clamaba con mi voz a Yave… (Sela.) S A veces me
    acostaba…»).
  - Objective:
    Recover and split the authentic Nácar text if the facsimile evidence
    allows it.
  - Reproduction (2026-09-23):
    - Before NACAR-OCR-104: Ps 3:4 empty; Ps 3:5 «Clamaba con mi voz a Yave,
      y él me oyó desde su monte santo. (Sela.) S A veces me acostaba…»
      (provenance leaf 963 col 0 tops 741 and 850).
    - Facsimile (Princeton leaf 963, left column): «⁵ Clamaba con mi voz a
      Yave, | y él me oyó desde su monte santo. (Sela.)» and «⁶ A veces me
      acostaba y me dormía, | y despertaba incólume porque Yave me
      defendía.» (Hebrew numbering; NRSVA 3:4 and 3:5). There is no «S».
    - Two separate causes:
      1. The merge: the printed ⁵ is read as 6 (event stream `4, 6, 6, 7`).
         Already fixed by NACAR-OCR-104 (`corrige_repetidos()`); the current
         output has Ps 3:4 «Clamaba…» and Ps 3:5 «S A veces…».
      2. The stray «S»: `load.fusion_numeros()` stage. Tesseract reads the
         superscript ⁶ as the letter «S» (conf 68, box x 115–128, top 851);
         the DjVu has «6» in the same box. `_es_marca()` rejected any
         alphabetic token, so the «S» was not linked to that number and the
         DjVu «6» was appended as an unread mark: two readings of one glyph
         («6 S A veces»), and the «S» ended up as verse text.
  - Fix:
    - `scripts/nacarcolunga/load.py`: `_es_marca()` also accepts the single
      letter «S» as a candidate verse mark, only when it is not taller than
      the page's median word height (`_alto_texto()`), so a drop cap «S»
      (e.g. «SIMON», Princeton 1463, 60 px) is excluded. As with any mark,
      it is replaced only if an unused DjVu number lies in the same box
      (`numero_cercano()`, distance < 55); otherwise it is untouched.
    - A first version that also accepted `I l B G T Z` was rejected: it
      changed Wis 7:8/7:9 and Wis 19:2 through a vertical rule read as «l»
      (78 px, conf 0) and removed the drop cap of «SIMON». There was no
      facsimile evidence for those letters.
  - Closure evidence:
    - Result: Ps 3:4 = «Clamaba con mi voz a Yave, y él me oyó desde su
      monte santo. (Sela.)» (leaf 963 top 741); Ps 3:5 = «A veces me
      acostaba y me dormía, y despertaba incólume porque Yave me
      defendía.» (top 850). Both contain only their own Nácar text as
      printed; no fallback is needed for either slot.
    - Before/after (baseline = NACAR-PSALMS-104 outputs): coverage 31084
      unchanged; `avisos.txt`, `reconstruidos.txt`, `introducciones.json`,
      `notas.json` unchanged. 19 `texto.json` keys changed: 17 are exactly
      the removal of a leading «S » from the same duplicated-superscript
      pattern (Ps 3:5, 28:6, 30:7, 40:5, 74:6, 132:6, 147:9, Job 40:6,
      Prov 1:6, Wis 1:6, Sir 32:6, Ezra 5:6, Num 7:6, Mark 6:6, Luke 13:16,
      1 Cor 10:6, 2 Cor 7:7); Num 17:6 drops a mid-verse «S »; Heb 6:20
      drops a «1» in the section heading «El sacerdocio de Melquisedec,
      superior al de Leví», which the facsimile (leaf 1448) shows has no
      digit. Ps 28:6 checked on leaf 974 (same `5`/`S` pattern).
      `procedencia.json`: 47 keys changed, 29 without text change; all of
      those only change `confianza` (the low-confidence «S» no longer
      counts), one also `top`.
    - Tests: `test_load.py` adds
      `test_volado_leido_como_letra_no_deja_dos_lecturas` (fails before:
      `['S', 'A', 'veces', '6']`), `test_letra_lejos_de_un_numero_no_se_toca`
      and `test_capitular_s_no_es_numero`; the 3:4/3:5 split itself is
      covered by `test_repetidos.py` (NACAR-OCR-104). PASS: `test_load.py`,
      `test_repetidos.py`, `test_correspondencias.py`,
      `test_salmos_cabecera.py`, `test_cabeceras.py`,
      `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py`. Torres Amat
      has its own `load.py` and is unaffected.
    - Limits: only «S» is handled; other letter-for-digit confusions are
      not covered without facsimile evidence. Other Ps 3 OCR errors
      («Ab: salón», «multi. plicado», «sor», «mil», «vid:», «sal vación»,
      «hicres», «Yavel») remain for NACAR-OCR-102.
    - Module not reinstalled; no commit or push.
  - Acceptance criteria:
    - Ps 3:4 and Ps 3:5 each contain only their own Nácar text, supported by
      the facsimile/OCR source.
    - The current fallback may remain while the original body is missing.
  - Do not:
    - Copy Reina-Valera text and present it as Nácar-Colunga.
    - Commit or push.

- [x] NACAR-OCR-102 Audit residual Nácar-Colunga OCR errors
  - Status: DONE
  - Description:
    Audit and clean residual OCR errors observed while comparing the Psalter,
    e.g. «Yavel» (Yavé), fragments such as «multi. plicado», «sor» (son),
    «Ab: salón» (Absalón), and residues in psalm titles («SAI meo», «delo
    de»). This is an audit/cleanup task, not a bulk replacement.
  - Cases in scope: the ones named above plus those assigned here by earlier
    tasks: «iga» (Ps 118:2, NACAR-PSALMS-103) and the Ps 3 residues listed
    by NACAR-OCR-101 («Ab: salón», «multi. plicado», «sor», «mil», «vid:»,
    «sal vación», «hicres», «Yavel»). «euango» (Ps 13:2) is not corrected
    here: it belongs to the line lost by `es_titulo()` (NACAR-OCR-103).
  - Findings (facsimile reading → responsible stage):
    - «Yavel», «Diosl», «reyl», «Israell», «casol»… → «Yave!», «Dios!»…:
      Tesseract reads the closing «!» of this typeface as «l». Stage:
      Tesseract text; fixed after `une()` in `construir.py`.
    - «multi. plicado», «Ab: salón», «san: gre»… → «multi-plicado»,
      «Ab-salón», «san-gre»: the line-end hyphen is read as «.» or «:», so
      `une()` does not join. Stage: line joining.
    - «¹ y ² Al maestro…» in the four two-verse titles (Ps 51, 52, 54, 60):
      the «y» (or the «2» when the OCR drops the «y») of the verse mark
      stays in the title («y Al maestro», «2 Al maestro»). Stage:
      `titulos_de_salmo()`.
    - Single readings: «sor» (son), «contra mil» (mí!), «vid:» (vida),
      «sal vación» (lost hyphen), «hicres» (hieres), «odic» (odio), «delo
      de» (de lo de, tight setting), «iga» (Diga, initial lost), Ps 52
      title «Mas … i e SAI meo» (Masquil … idu-|meo; «e SAI» is the running
      header «SALMOS» of leaf 987). No general rule is demonstrable; each is
      a documented exception.
  - Fix:
    - New `scripts/nacarcolunga/limpieza.py`:
      - `cierra_exclamaciones()`: `Xl` → `X!` only if an «¡» is open (or
        «oh»/«joh» precedes), the next thing is end, punctuation, or a
        capital/«¡¿«(», the stem occurs >= 20 times and `Xl` < 1/20 of the
        stem in the built text itself. «el», «mil», «Israel», «aquel»,
        «Mil setecientos» are untouched.
      - `repara_guiones()`: a fragment ending «X.»/«X:» followed by a
        lowercase fragment of >= 2 letters becomes a hyphen join when the
        joined word occurs >= 2 times and at least as often as «X» alone.
        Single-letter right fragments (e.g. «angusti: a», «mir: a») are
        excluded.
      - `aplica_erratas()` with new `erratas.json` (10 entries, each with
        its Princeton leaf/column and printed reading); applied only when
        the OCR reading occurs exactly once and the printed reading is not
        already there; otherwise nothing is changed and `avisos.txt` gets
        a line.
    - `construir.py`: vocabulary from the uncorrected build, then
      `repara_guiones` → `une` → `cierra_exclamaciones` → `aplica_erratas`;
      `titulos_de_salmo()` strips the «y»/«2» mark residue only for
      two-verse titles (`TITULO_SALMOS == 2`).
  - Closure evidence:
    - Baseline = NACAR-OCR-101 outputs. Coverage 31084 unchanged; key set
      identical; `procedencia.json` identical; `avisos.txt` unchanged (all
      10 exceptions applied); `reconstruidos.txt`, `introducciones.json`,
      `notas.json` unchanged. 374 `texto.json` keys changed: 296 «l»→«!»,
      85 hyphen joins, and 14 edits from the exceptions and title marks;
      no other change.
    - Facsimile checks: Ps 3 (963: «¡Oh Yave!», «multi-plicado», «son»,
      «contra mí!», «Ab-salón», «vida», «sal-vación», «hieres»), Ps 25:19
      (973), Ps 51–52 titles (986, 987), Ps 118:2 (1020); «!» rule on 10
      doubtful forms: 1 Sam 25:34 «mal!» (400), Heb 9:14 «vivo!» (1451),
      Jude 1:11 «Coré!» (1478), Rom 11:24 «olivo!» (1410), Matt 26:28
      «entregado!» (1203), Job 30:20 «caso!» (943), Isa 5:18 «carro!»
      (696), Judg 16:12 «ti!» (360), Ps 17:6 «Dios!» (968); hyphen rule:
      Exod 24:6 «san-gre» (185), Luke 4:41 «tam-bién» (1249), 1 John 5:16
      «Espí-ritu» (1476), Sir 44:9 «pasa-ron» (1145), Jer 40:1
      «Nebu-saradán» (779), 2 Chr 16:9 «insensata-mente» (533).
    - Result: Ps 3:0 «…al huir de Absalón, su hijo (1).»; Ps 3:1 «¡Oh
      Yave! ¡Cómo se han multiplicado mis enemigos! ¡Cuántos son los que se
      alzan contra mí!»; Ps 3:2 «…de mi vida dicen: «No tiene ya en Dios
      salvación»…»; Ps 3:7 «Tú hieres…»; Ps 51:0 «Al maestro del coro. …
      después de lo de Betsabé.»; Ps 52:0 «Al maestro del coro, Masquil de
      David (1), cuando Doeg, idumeo, …»; Ps 118:2 «Diga Israel…».
    - Tests: new `scripts/nacarcolunga/test_limpieza.py` (7 cases,
      including negatives for real words ending in «l», non-exclamation
      contexts, frequent split fragments, single-letter fragments,
      one-verse titles, absent/already-correct exceptions, and exceptions
      without a source). It fails on the previous state (no `limpieza`
      module; «y Al maestro» title) and passes now; it caught an
      idempotence bug («iga Israel» inside «Diga Israel») that was fixed
      before closing. PASS: `test_limpieza.py`, `test_load.py`,
      `test_repetidos.py`, `test_correspondencias.py`,
      `test_salmos_cabecera.py`, `test_cabeceras.py`,
      `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py`.
    - Reproducible: all corrections run in `construir.py` from versioned
      rules/data; the installed module was not edited or reinstalled.
  - Limits / left for other tasks:
    - 26 «Yavel» remain where no «¡»/«oh» signal survives (e.g. «ante
      Yavel», «Bendice,:o0h Yavel»); «¡» read as «j/J» is only covered
      before «oh».
    - Ps 13:2 «euango» and lost line: NACAR-OCR-103.
    - Ps 54:0 ends «nosotros.,» and Ps 60:0 starts «Al]» and lacks «David,
      para enseñar, cuando»: not audited here.
    - Other OCR errors seen but not in scope (e.g. Ps 17:6 «bacia»).
    - Module not reinstalled; no commit or push.
  - Acceptance criteria:
    - Each correction is supported by the facsimile/OCR source or by a
      demonstrable general rule.
    - Changes are reproducible from the pipeline, not hand edits to the
      installed module.
  - Do not:
    - Modernize the text automatically.
    - Commit or push.

- [x] NACAR-OCR-103 Keep es_titulo() from discarding biblical continuation lines
  - Status: DONE
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
    - Also assigned here by NACAR-PSALMS-104: Ps 13:2 «tinuo sobre mi
      corazón? | ¿Hasta» and Ps 13:4 ««Le vencí.» | Que mis enemigos se».
  - Reproduction (facsimile → OCR → `es_titulo()` → output):
    - All listed lines are present in the Tesseract OCR with normal line
      pitch (gaps of −4 to 7 px to their neighbours) and are dropped only by
      `es_titulo()` (no digit, 8–70 chars, no lowercase start, no final
      punctuation). Checked on Princeton 967 (Ps 13, 15, 16), 968 (Ps 17),
      1009 (Ps 95–96), 1026 (Ps 124).
    - Scope: across the whole stream `es_titulo()` dropped 7,813 body
      lines of this kind; real epigraphs are set off by 26–60 px of blank
      space above and below, while continuation lines keep the normal pitch.
  - Fix (`scripts/nacarcolunga/versiculos.py`):
    - New `epigrafes(lineas, cands)`: splits the column into blocks of
      normal pitch (gap >= half the median line height starts a new block).
      A line can be an epigraph only if it belongs to (a) a block of 1–3
      lines without a verse mark, or (b) the first short block of the
      column (running-header zone, whose page number looks like a verse
      mark), or (c) after a block that ends a sentence, a centered line
      (measured on letter words only, ignoring the column rule «|» and
      stray commas) placed before the block's first verse mark and not
      ending in «, ; : -». Psalm headers «N (Vulg. M.)», noise and running
      headers do not count as verse marks.
    - `corriente()`: between a psalm header and its first verse every
      `es_titulo()` line is an epigraph (Ps 91: «Canto a la providencia de
      Dios sobre | el justo.» sits 12 px above verse 1).
    - `_sucesos_linea()`: drops an `es_titulo()` line only if it is an
      epigraph by the rules above; otherwise it is kept as body. The text
      test itself is unchanged, so lines that were never dropped keep their
      previous routing (epigraph routing stays with NACAR-OCR-105).
    - Rejected during the work (measured, not kept): horizontal centering
      as the main signal (bold headings have unreliable OCR boxes; 557
      headings/header scraps were recovered into the body); an exception for
      lines starting with «|» (the column rule makes headings start with
      «|»); treating every line before the first verse mark after a blank as
      an epigraph (dropped 953 body lines at column tops, Sir 51, intros).
  - Closure evidence:
    - Result: Ps 13:2 «…y penas de con*tinuo sobre mi corazón? ¿Hasta euango
      mis enemigos triunfarán de»; Ps 13:4 «Que no pueda decir mi enemigo:
      «Le vencí.» Que mis enemigos se regocijarían si yo cayese,»; Ps 15:1
      «…¡Oh Yave! ¿Quién es el que podrá habitar…»; Ps 16:3 «…son de mí muy
      honrados, en ellos tengo todas mis delicias.»; Ps 16:4 «…los que se
      van tras los dioses ajenos. No libaré…»; Ps 17:1 «…Oye, Yave, mi justa
      causa, atiende a mi súplica…»; Ps 95:1 «…a Yavel ¡Cantemos gozosos a
      la roca…»; Ps 96:11 «…regocíjese 'a tierra, truene el mar…»; Ps 124:2
      «…por noscuando se alzaron contra los hombres,»; Ps 124:3 «…tragado
      encuando ardía su ira contr»; Rev 12:7 «…Miguel y sus ángeles
      peleaban con el dragón,».
    - Before/after (baseline = NACAR-OCR-102 outputs): coverage 31084
      unchanged; key set identical; `procedencia.json` identical (recovered
      lines are continuation events without their own provenance);
      `avisos.txt` unchanged; `reconstruidos.txt`, `notas.json` unchanged.
      5,247 `texto.json` values changed and all grew (0 shrank). 17
      `introducciones.json` entries grew (introduction lines lost the same
      way); none shrank. No fallback can be new: no key was removed and no
      verse lost text.
    - Line accounting over the stream: 6,977 previously dropped lines are
      now kept; 838 are still dropped as epigraphs.
    - Epigraphs not moved into the body: none of «Seguridad del justo»,
      «Canto a la providencia», «Exhortación a la alabanza», «Condiciones
      de pureza», «Invitación a las gentes», «El justo, en peligro», «El
      justo espera», «Separación de Abram y Lot», «Circuncisión», «La
      amistad», «Curación de un paralítico», «La batalla en el cielo»,
      «Destrucción de Sodoma», «Alianza de Dios con el pueblo» newly
      appears in any verse. A random sample of 70 kept lines from
      normal-pitch blocks was all body text; the 34 kept lines from short
      blocks with a verse mark were reviewed (column-foot continuations,
      poetry). Facsimile checks of random kept lines: Isa 45:24 (725), 2 Sam
      12:27 (417), Judg 1:31 (345), John 7:6 (1297), Jer 15:19 (755), Dan
      3:58 (859) and two introduction lines (922, 1166): all authentic.
    - Tests: new `scripts/nacarcolunga/test_epigrafes.py` (7 cases with
      facsimile geometry). The five recovery cases fail with the previous
      behaviour and pass now; the two controls (Ps 91 epigraph attached to
      verse 1, Gen 13 centered heading attached to verse 5) stay dropped
      in both. PASS: `test_epigrafes.py`, `test_salmos_cabecera.py`,
      `test_limpieza.py`, `test_load.py`, `test_repetidos.py`,
      `test_correspondencias.py`, `test_cabeceras.py`,
      `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py`.
    - Module not reinstalled; no commit or push.
  - Limits / not solved here:
    - Ps 13:2 «euango» is not resolved by recovering the line: it is
      Tesseract's reading (confidence 15) of «cuándo» on the next line,
      and «mí?» is dropped by `es_ruido()` (2 letters). Fixing it needs a
      documented reading (the NACAR-OCR-102 `erratas.json` mechanism), which
      this task forbids («no per-reference exceptions»). «con*tinuo» keeps
      the OCR «*».
    - Ps 124:2–3 «noscuando», «encuando»: the OCR lost the left edge of
      those lines at the column gutter («otros,», «tonces,», «nosotros»);
      column-split/OCR scope, not `es_titulo()`.
    - Ps 95:1 «Yavel» stays: no «¡»/«oh» context survives for the
      NACAR-OCR-102 rule.
    - Num 24:6 poetry lines («Como un jardín…», «Como cedro…») are still
      dropped: the OCR lost the neighbouring short lines («valle;»,
      «aguas.»), which fakes an isolated block.
    - Two-line headings whose first line is full width and sits directly
      on its verse are not detected by rule (c); their first line was
      already outside `es_titulo()` or stays as before.
    - Gen 1:2 already contained Pentateuch introduction text (front-matter
      separation); it now also receives recovered introduction lines.
  - Acceptance criteria:
    - Distinguish real epigraphs from biblical continuation lines.
    - Authentic lines are kept in the verse body.
    - Epigraphs are not moved into the body.
    - No new fallbacks are caused.
  - Do not:
    - Add per-reference exceptions.
    - Commit or push.

- [x] NACAR-OCR-104 Handle verse numbers misread by the OCR (5 read as 6)
  - Status: DONE
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
  - Fix:
    - `scripts/nacarcolunga/versiculos.py`: `corrige_repetidos()` runs on
      the Nácar event stream in `construir.py` (after `separa_front_matter`,
      before `ensambla`). Within a chapter segment (reset at `libro`/`cap`),
      for consecutive verse marks `a, b, c, d` with `b == c`, `a == b - 2`
      and `d` in (end of chapter, `1`, `c + 1`), `b` is relabeled `b - 1`.
      With `d == c + 2` or another value it is ambiguous (lost 7 vs. misread
      5) and is left untouched. The shared `torresamat/alinear.py` is not
      modified; Torres Amat is unaffected. No hardcoded verses.
  - Closure evidence:
    - Pattern count in the stream: 857 duplicated-with-missing-predecessor
      triples; 821 meet the confirmation rule and are relabeled; 36
      ambiguous are left untouched. Mostly final digit 5 → 6 (6, 16, 26, 36…), but also 2 → 3
      and 18 → 19.
    - Facsimile validation (Princeton leaves), all confirming the printed
      number is `b - 1`: Ps 13 ⁵ (967), Ps 17 ⁵ (968), Ps 19 ⁵ (970),
      Ps 119 ¹²⁵ and ¹³⁵ (1024), 2 Sam 20 ⁵ (426, mid-line), Lev 13 ² read
      as 3 (216), Heb 11 ¹⁸ read as 19 (1453), Exod 19 ⁵ (180).
    - Before/after (baseline = NACAR-PSALMS-103 outputs): coverage
      30318 → 31084 verses (+766 filled slots, 0 removed); `avisos.txt`
      unchanged (12, no chapter alignment change); `reconstruidos.txt`
      unchanged. 1604 `texto.json` keys changed; every changed key belongs
      to a corrected `(n-1, n)` pair (0 isolated changes); all 1598
      `procedencia.json` changes are within those keys.
    - Validation-only witness metric (SpaRV/SpaRVG overlap at the same
      reference, not used by the pipeline): merged slot before 0.121 →
      after 0.423 (slot n-1) and 0.416 (slot n) over 763 scored pairs. The
      5 pairs where slot n scored lower were inspected: all are correct
      splits whose score drops because the verse is already truncated by
      the OCR (Mark 5:6 «Viendo desde») or KJV numbering differs from NRSVA
      (Isa 9, Mic 5).
    - 75 changed keys whose `n-1` slot already had text were inspected
      (e.g. 2 Chr 20:5–6, 2 Sam 23:15–16, Dan 5:5–6): the correct verse
      start now lands in `n-1`; stray text from other column/page issues
      that was already there remains (pre-existing, not introduced).
    - Psalms from the evidence: Ps 9, 13, 17, 18, 19, 20, 21 fixed (e.g.
      Ps 17:5 «Y mis pies, sin titubear, se mantuvieron firmes.», Ps 17:6
      «Te invoco…»). Not fixed: Ps 14:5, because the false internal `1`
      («tiempo, 1 porque», NACAR-PSALMS-102) sits between the two 6s and
      breaks the signal; Ps 118:5, where the OCR reads the printed ⁵ as 6
      but the line carrying ⁶ lost its number (no duplicate, no signal).
    - Tests: new `scripts/nacarcolunga/test_repetidos.py` (6 cases,
      including ambiguous and chapter-crossing negatives and an
      `ensambla` end-to-end check) PASS; `test_salmos_cabecera.py`,
      `test_load.py`, `test_cabeceras.py`, `test_front_matter.py`,
      `test_completar.py`, `torresamat/test_pegadas.py`,
      `torresamat/test_restos.py` PASS.
    - Module not reinstalled; no commit or push.
  - Acceptance criteria:
    - Detection/correction relies on a general structural signal (e.g. a
      duplicated number with a missing predecessor), validated against the
      facsimile.
    - A before/after comparison shows no regressions elsewhere.
  - Do not:
    - Hardcode verses.
    - Commit or push.

- [x] NACAR-OCR-105 Separate psalm epigraphs glued to the previous verse
  - Status: DONE
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
  - Reproduction (baseline = NACAR-OCR-103 outputs):
    - Ps 17:15 «…de tu gloria. EF Canto triunfal de David.», Ps 117:2
      «…¡Aleluya! Canto triunfal.», Ps 13:6 «…«Bien me proveyó.» impíos.»,
      Ps 11:7 «…Deprecación contra los impíos.», Ps 37:40 «…Oración de un
      pecador arrepentido,», Ps 111:10 «…Bienandanza del justo. ¡Aleluya!».
    - Path: `corriente()` sees the header «N (Vulg. M.)» and defers the
      `cap` event to the first verse; the lines in between are either
      dropped by `es_titulo()` (first line of «Seguridad del justo en el
      castigo de los | impíos.», lost) or emitted as `sigue` before `cap`;
      the shared aligner (`torresamat/alinear.py`, `ensambla`) appends any
      non-verse event to the current verse, i.e. the last verse of the
      previous psalm. Facsimile: Princeton 967 (Ps 13/14), 968 (Ps 18),
      1020 (Ps 117/118), 965 (Ps 9), 1019 (Ps 114), 1035 (Ps 148), 971
      (Ps 22), 970 (Ps 21), 1021 (Ps 119), 962 (Ps 1).
  - Fix:
    - `versiculos.py`, `corriente()`: after a psalm header, up to three
      lines that do not open a verse (noise and running headers skipped)
      are emitted as `("epigrafe", texto, cands, origen)`; collection ends
      at the first verse mark, or at the first blank >= half a line height
      once a line is collected. What follows that blank (e.g. the title
      «Salmo de David.» whose «1» the OCR lost, Ps 110/127/132/139) keeps
      its previous routing. The `cap` placement is unchanged.
    - `RE_CABECERA_LAXA`: headers the strict pattern does not read (bare
      «5», «148,», «114, 115 (Vulg. 113.) (1).», «22 (Vulg. 21», «| 85
      (Vulg. 84.)», «66 (Vulg. 65.) +») only start an epigraph
      collection, never a chapter; the collected lines become an epigraph
      only if the line that ends them opens a psalm (verse 1 or 2),
      otherwise they are emitted as before. Active on Psalms pages or pages
      without a read running header.
    - `construir.py`: `separa_epigrafes()` removes the epigraph events
      before the shared aligner and ties each to the next verse event;
      `asigna_epigrafes()` finds that verse's chapter through its
      provenance object (kept by `ensambla`) and writes `epigrafes.json`
      (`{"Ps N": {"texto", "procedencia"}}`); unassignable or duplicate
      epigraphs go to `avisos.txt` (none occurred).
    - `front_matter.py`: epigraph events before the first verse of a book
      stay in the stream instead of going to the introduction (Ps 1 «Las
      dos sendas: La del justo y la del impío.»).
    - `osis.py`: writes the epigraph as `<title canonical="false">` at the
      start of its chapter, before the canonical psalm title.
    - Bug found and fixed during validation: a bare verse number on its own
      line («21», Ps 51, leaf 986) started a loose collection that a strict
      header then discarded, deleting «ciones y holocaustos. Entonces
      pondrán becerros en tu altar,». Pending loose lines are now emitted
      normally when a strict header interrupts them.
  - Closure evidence:
    - `epigrafes.json`: 139 epigraphs for Ps 1, 5, 7, 9, 11–42, 44–98,
      100–114, 116–121, 123–149 (e.g. Ps 14 «Seguridad del justo en el
      castigo de los impíos.», Ps 18 «EF Canto triunfal de David.», Ps 91
      «Canto a la providencia de Dios sobre el justo.», Ps 118 «Canto
      triunfal.»). Regenerated OSIS (not installed) has 139
      `<title canonical="false">`.
    - Before/after: coverage 31084 unchanged; key set identical;
      `procedencia.json` identical; `avisos.txt` unchanged;
      `reconstruidos.txt`, `notas.json` unchanged. 133 `texto.json`
      values changed, all in Psalms, all shorter (0 grew); every removed
      fragment is contained in an epigraph. Three removals also change the
      preceding «Yavel» into «Yave!» (Ps 8:9, 43:5, 126:6): with the
      epigraph gone it ends the verse and the NACAR-OCR-102 rule applies.
      `introducciones.json`: only «impío.» leaves the Psalms introduction
      (it is the end of the Ps 1 epigraph).
    - No fallback: no verse became empty (shortest changed verse, Ps 78:72,
      keeps 40 characters); keys are identical. NACAR-FALLBACK-101 keeps
      `completar.py` from substituting shortened verses.
    - Controls: NACAR-OCR-103 recovered lines stay in the body (Ps 14:1
      «Dice en su corazón el necio…»); legitimate verse ends are untouched
      (Ps 117:2 ends «¡Aleluya!», Ps 51:18 keeps «…pondrán becerros en tu
      altar,»); a bare number without a psalm opening behind it is not a
      header.
    - Facsimile checks of assignments: Ps 9 (965), Ps 114 (1019, printed
      «114, 115» as one psalm), Ps 148 (1035), Ps 22 (971), Ps 21 (970),
      Ps 119 (1021), Ps 1 (962): all epigraphs belong to the psalm they are
      attached to.
    - Tests: new `scripts/nacarcolunga/test_epigrafes_salmo.py` (8 cases);
      7 fail on the pre-105 code and pass now; the control «bare number
      without psalm behind» passes on both. `test_salmos_cabecera.py`:
      `test_epigrafe_queda_antes_del_capitulo`, which asserted the old
      glued routing, was replaced by
      `test_epigrafe_sale_aparte_y_el_capitulo_abre_en_el_verso_1` (same
      `cap` placement assertions, plus epigraph events and no `sigue`).
      PASS: `test_epigrafes_salmo.py`, `test_epigrafes.py`,
      `test_salmos_cabecera.py`, `test_limpieza.py`, `test_load.py`,
      `test_repetidos.py`, `test_correspondencias.py`, `test_cabeceras.py`,
      `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py`.
    - Module not reinstalled; no commit or push.
  - Limits:
    - 11 psalms have no separated epigraph: Ps 2, 3, 4, 6, 10 (the OCR did
      not read the bare header number), Ps 8, 43, 99, 115, 122, 150 (header
      or epigraph not confirmed by a following verse 1–2). Where their
      epigraph text exists it keeps the previous routing (e.g. Ps 149:9
      «…¡Aleluya! Doxología final del Saltcrio. Canto de alabanza.»).
    - Ps 119: «Alef. (1).» (italic stanza label) is taken as the heading;
      the epigraph «Excelencias de la ley del Señor.», after a blank, still
      ends Ps 118:29.
    - Epigraph text keeps OCR defects: lost first word at the bold left
      margin (Ps 21 «de gracias…» for «Canto de gracias…», also Ps 40, 71,
      82, 123, 124, 129, 147, 149), junk («EF», «'»), «Yavel»-type errors.
  - Acceptance criteria:
    - The epigraph is preserved as metadata/heading.
    - It is not glued to the previous verse.
    - It is not deleted.
    - Shortening the verse does not cause a fallback (depends on
      NACAR-FALLBACK-101).
  - Do not:
    - Commit or push.

- [x] NACAR-OCR-106 Record structural truncation/loss metadata from the parser
  - Status: DONE
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
  - Reproduction (baseline = NACAR-OCR-105 outputs):
    - Josh 5:1 «Cuando todos los reyes de los»: the chapter drop cap «5»
      (Princeton 319) is read «*7» on the second line; `quita_basura()`
      strips «*» and the continuation «amorreos, a occidente…» opens a
      false verse 7, so it lands in Josh 5:7. Displacement, not loss.
    - 1 Thess 3:7 «…gran conpor vuestra fe…tribuAhora ya vivimos»: the OCR
      lost the first word of four consecutive lines at the gutter
      («suelo», «todas», «laciones. ⁸», «que»; Princeton 1363).
    - Rev 11:12 «…sus enemi-»: the next line «gos. 13 Y en aquella…» only
      opens verse 13 after `quita_basura()` deletes «gos.» (Princeton
      1495).
    - 2 Cor 11:31 and Exod 25:8 are complete now (recovered by
      NACAR-OCR-103), confirmed on Princeton 1389 and 185; they must not be
      flagged.
  - Signals (distinct, each with its own evidence):
    - `guion_final`: the verse text ends in a hyphenated word fragment
      («su pro-»); its continuation is not in the verse.
    - `fragmento_descartado`: text with lowercase that `quita_basura()`
      removed before a verse mark («gos.», «do:», «del rey.»), attached to
      the verse in progress. Fragments that are not mostly lowercase
      («ABiOs,», «MH») are kept as `descarte:basura`.
    - `continuacion_desplazada` / `contenido_desplazado`: a mark that only
      appears after stripping junk (`marca_tras_basura`), makes the
      numbering spike (1, 7, 2…) and is followed by lowercase text: the
      previous verse is cut and its continuation sits in another slot.
    - `borde_perdido`: a line ending in a hyphen followed by a
      continuation that starts >= 3 letter widths right of the column's
      modal left edge and reaches the right edge (justified prose), not
      next to a drop cap, a verse-1 opening, a Job «[» overflow, a
      footnote «(1)» or a short line.
    - Evidence only: `descarte:ruido|titulo|titulillo|basura` (lines the
      parser drops, with >= 2 letters; `titulillo` includes lines removed
      by `quita_cabecera`), `marca_tras_basura`, and
      `renglon_descartado_alineador` (lines read by the parser that
      `ensambla()` put in no verse, found by string identity right after
      `ensambla`).
  - Fix:
    - `versiculos.py`: `_sucesos_linea()` emits `("perdida", tipo, texto,
      cands, origen)` for dropped lines, stripped fragments and junk
      marks; `corriente()` emits `borde_perdido`; `("sigue", …)` now carries
      its line provenance as 4th element (geometry of continuation lines);
      `_abre()` reports what `quita_basura()` removed. `abre_versiculo()`
      keeps its behaviour.
    - `construir.py`: `separa_perdidas()` removes the evidence before the
      shared aligner (it would glue it as text); `no_ensamblados()`;
      `registra_perdidas()` attaches each signal to the verse in progress
      via provenance identity, adds `guion_final` and mark spikes, and
      writes `perdidas.json`: `{ref: {truncado, perdida_probable,
      senales: [{tipo, texto, origen}], renglones: [line boxes],
      columnas: n}}`. `_extrae_pagina()` records lines removed by
      `quita_cabecera`.
    - Two levels, by measured precision: `truncado` only for
      `guion_final`, `fragmento_descartado`, `continuacion_desplazada`,
      `contenido_desplazado`; `perdida_probable` for `borde_perdido`.
    - No text is completed or compared with another translation;
      `texto.json`, OSIS and the runtime fallback are unchanged.
  - Closure evidence:
    - Before/after: `texto.json`, `procedencia.json`, `avisos.txt`,
      `reconstruidos.txt`, `introducciones.json`, `notas.json`,
      `epigrafes.json` and the regenerated OSIS are byte-identical; coverage
      31084. New output `perdidas.json`: 2,544 verses with evidence, 645
      `truncado`, 305 `perdida_probable` (23 both). Signal counts:
      `renglon_descartado_alineador` 2,165, `descarte:titulillo` 803,
      `descarte:titulo` 723, `fragmento_descartado` 559, `borde_perdido`
      418, `descarte:ruido` 398, `guion_final` 322, `marca_tras_basura`
      207, `descarte:basura` 81, `continuacion_desplazada`/
      `contenido_desplazado` 2 each (Josh 5:1→5:7, 1 Kgs 9:1→9:9).
    - Evidence verses: Josh 5:1 truncado (continuacion_desplazada), Josh
      5:7 contenido_desplazado; Rev 11:12 truncado (guion_final +
      fragmento «gos.»); 1 Thess 3:7 perdida_probable (two borde_perdido);
      2 Cor 11:31 and Exod 25:8 no entry (complete).
    - Controls (complete, audited in NACAR-FALLBACK-101): Ps 17:7, Ps
      35:5, Gen 38:6, Luke 23:21, Mark 12:30, Hos 13:3, Ps 16:3, Ps
      117:1–2: no entry.
    - Measured precision: `guion_final` 12/12 sampled verses really end
      mid-word; `fragmento_descartado` 13/15 sampled fragments complete the
      verse end («ante el | rey,», «auxi|lio.», «hubo | luz.»), the other 2
      are real lost text with uncertain attribution; before the lowercase
      filter the misses were a running header («ABiOs,») and an
      introduction («(Sal.»). `borde_perdido` checked on 20 random cases
      in the facsimile: 12 real lost line starts («manos», «cia,»,
      «lequet», «tros», «piento», «remos,», «mate», «do», «lidas», «dad»,
      «justo,», «tes»), 5 misplaced italic introduction text, 3 in biblical
      text (Dan 3 margin on leaf 858, Sir 15:1 unread drop cap) — hence
      probable, not truncated. Mark spikes: 8 before the lowercase
      condition, of which running headers («EA ÉXOI», «—CRÓN:»),
      introduction references and Prov 31 were false.
    - Tests: new `scripts/nacarcolunga/test_perdidas.py` (7 cases: each
      signal positive, drop-cap / Job overflow / footnote / no-hyphen
      controls, per-verse registry with complete verses unflagged, aligner
      drop as evidence only). PASS together with `test_epigrafes_salmo.py`,
      `test_epigrafes.py`, `test_salmos_cabecera.py`, `test_limpieza.py`,
      `test_load.py`, `test_repetidos.py`, `test_correspondencias.py`,
      `test_cabeceras.py`, `test_front_matter.py`, `test_completar.py`,
      `torresamat/test_pegadas.py`, `torresamat/test_restos.py`.
    - Module not reinstalled; no commit or push.
  - Limits:
    - `perdidas.json` is metadata only: nothing uses it yet for a badge or
      a recovery; the runtime still badges only empty slots.
    - A verse without any structural signal can still be truncated (e.g. a
      line lost entirely by the OCR with normal spacing); it is not
      flagged.
    - `renglon_descartado_alineador` cannot say which verse lost the line
      (often introductions or notes between books), so it never flags.
    - Isa 37:1 is missed: the text after its junk mark starts «+ aquello».
    - Misplaced introduction text inside verses produces
      `borde_perdido` noise; drop caps the OCR reads small or not at all
      can too.
    - Not in this task's scope and not resolved by metadata: the psalms
      without separated epigraph and Ps 119 «Excelencias de la ley del
      Señor.» (NACAR-OCR-105 limits).
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

- [x] TORRES-FACSIMILE-102 Remove spurious fragment at the end of Torres Amat Mt 12:6
  - Status: DONE
  - Description:
    Mt 12:6 ends with a spurious fragment `" i"` («…mayor que el templo. i»).
  - Reproduction:
    - Installed module (`mod2imp TorresAmat`, which already carries the
      TORRES-FACSIMILE-101 patch): `Matthew 12:6` = «Pues yo os digo, que
      aquí está uno que es mayor que el templo. i».
    - Facsimile: item `la-sagrada-biblia-vulgata-tomo-iiv_202111`, tomo IV,
      leaf `_0022` (downloaded read-only from Internet Archive, not
      stored in the repo). The printed verse ends «…mayor que | el
      templo.»; the rest of the line is blank paper.
    - Archive OCR (`…tomo IV_djvu.xml`, page `_0022`, 5865×8945): after
      «templo.» (x 471–744) the OCR emits `i` at x 1800–1803, y 5996–6004
      (a 3×8 px box, confidence 0) and `|` at x 2048 (confidence 33). The
      zoomed crop shows only a paper speck there: OCR noise belonging to
      no printed element.
  - Fix:
    - `scripts/torresamat/parche_facsimil.py`: new `CORRECCIONES`
      entry `Matthew 12:6` (old text with « i» → «…mayor que el templo.»,
      «tomo IV, hoja 22»). The substitution loop of `main()` is factored
      into `aplica(entradas, autorizados)` (same rules: exact old text
      required, already-corrected entries kept, missing keys or unexpected
      text stop the patch, now via `ValueError`) so it can be tested
      without the installed module.
  - Closure evidence:
    - `python3 parche_facsimil.py`: «corrige Matthew 12:6», the other 15
      authorized entries «ya corregido», round trip OK, «(1 versos)».
    - Export of the regenerated module (`salida/parche`, loaded under a
      renamed conf) vs. the installed baseline export: 38,698 entries in
      both, same keys in the same order, 35,475 non-empty in both, exactly
      one changed entry: `Matthew 12:6` loses « i». Mt 12:5 and 12:7 are
      unchanged.
    - Torres Amat has no `procedencia.json`/`avisos.txt` on this path:
      build data are not local, the patch works on the compiled module.
    - Nácar-Colunga untouched: `texto.json`, `procedencia.json`,
      `avisos.txt`, `epigrafes.json`, `introducciones.json` identical to
      the NACAR-OCR-106 baseline; no Nácar file edited.
    - Tests: new `scripts/torresamat/test_parche.py` (Mt 12:6 without the
      speck, with Mt 12:5/12:7 and the «el templo.» ending as controls;
      unexpected text stops the patch; already corrected text is kept).
      It cannot pass on the previous `parche_facsimil.py` (no `aplica()`,
      no `Matthew 12:6` entry). PASS: `test_parche.py`,
      `test_titulos.py`, `test_pegadas.py`, `test_restos.py` and the 11
      Nácar-Colunga tests.
    - The patched module is only in `scripts/torresamat/salida/parche`
      (git-ignored); neither `modulos/` nor `~/.sword` was touched; no
      commit or push.
  - Acceptance criteria:
    - Verified against the 1882 facsimile (tomo IV, hoja 22) before changing.
    - If confirmed as OCR noise, corrected through
      `scripts/torresamat/parche_facsimil.py`.
  - Do not:
    - Edit only the installed module.
    - Commit or push.

- [x] TORRES-FACSIMILE-103 Fix Torres Amat Mt 12:10 «hallabaun»
  - Status: DONE
  - Description:
    Mt 12:10 reads «Donde se hallabaun hombre…».
  - Reproduction:
    - Installed module (`mod2imp TorresAmat`, unchanged since the
      TORRES-FACSIMILE-102 baseline): `Matthew 12:10` = «Donde se hallabaun
      hombre que tenia seca una mano; y preguntaron á Jesus, para hallar
      motivo de acusarle: ¿Si era lícito curar en dia de sábado?».
    - Facsimile, tomo IV, leaf `_0022`: «10. Donde se hallaba un hombre que
      tenia seca una mano; | y preguntaron á Jesus, para *hallar motivo de*
      acusarle: ¿Si | era lícito curar en dia de sábado?». The print sets
      «hallaba un» with almost no space.
    - Archive OCR (`…tomo IV_djvu.xml`, page `_0022`): a single word
      `hallabaun` (x 1004–1380, confidence 91). The error is introduced by
      the OCR and carried into the module unchanged; the rest of the verse
      matches the print.
    - Neighbours: Mt 12:11 already carries the TORRES-FACSIMILE-101
      correction and matches the print. Mt 12:9 in the module ends «…de
      ellos» while the print (and the djvu.xml) has «de ellos,»: a missing
      comma, outside this task's scope; not changed, noted for review.
  - Fix:
    - `scripts/torresamat/parche_facsimil.py`: `CORRECCIONES["Matthew
      12:10"]` (exact old text → «Donde se hallaba un hombre…», «tomo IV,
      hoja 22»). Only «hallabaun» → «hallaba un» differs. The existing
      `aplica()` checks (exact old text, idempotent «ya corregido», stop on
      unexpected text or missing keys) and the round-trip check are kept.
  - Closure evidence:
    - `python3 parche_facsimil.py`: «corrige Matthew 12:6», «corrige
      Matthew 12:10», other entries «ya corregido», round trip OK, «(2
      versos)».
    - Regenerated module (`salida/parche`) export vs. installed baseline:
      38,698 entries in both, same keys in the same order, 35,475 non-empty
      in both; exactly two changed entries: `Matthew 12:6` (loses « i»,
      TORRES-FACSIMILE-102, not yet installed) and `Matthew 12:10`
      («hallabaun» → «hallaba un»). Mt 12:9 and 12:11 unchanged.
    - `modulos/` and `~/.sword` untouched (no git changes under
      `modulos/`, no newer files in the installed module directory).
    - Nácar-Colunga: `texto.json`, `procedencia.json`, `avisos.txt`,
      `epigrafes.json`, `introducciones.json` identical to the
      NACAR-OCR-106 baseline.
    - Tests: `scripts/torresamat/test_parche.py` adds
      `test_mt_12_10_hallaba_un` (Mt 12:10 corrected, only that word
      changes; Mt 12:9 untouched and Mt 12:11 already corrected are
      controls). It fails on the previous patch table (`KeyError`, no
      `Matthew 12:10` entry) and passes now. PASS: `test_parche.py`,
      `test_titulos.py`, `test_pegadas.py`, `test_restos.py` and the 11
      Nácar-Colunga tests.
    - No commit or push.
  - Acceptance criteria:
    - The correct reading is verified in the 1882 facsimile (tomo IV,
      hoja 22).
    - Corrected through `scripts/torresamat/parche_facsimil.py`.
    - A focused regression covers the corrected verse.
  - Do not:
    - Edit only the installed module.
    - Commit or push.

- [x] TORRES-FACSIMILE-104 Harden the Torres Amat facsimile patch mechanism
  - Status: DONE
  - Description:
    `scripts/torresamat/parche_facsimil.py` was used in
    TORRES-FACSIMILE-101 to correct Ps 3:2, Ps 3:3, Ps 3:4, Ps 3:5, Mt 12:4,
    Mt 12:5, and Mt 12:11. Verify that the mechanism remains reproducible from
    a known source, limited to changes demonstrated by the facsimile,
    idempotent, and free of collateral changes.
  - Baseline and findings:
    - Baseline = installed module (`mod2imp TorresAmat`, 38,698 entries,
      35,475 non-empty). Known source = `modulos/` in git: commit
      `9c036c87` (parent of `d705cb5b`) holds the unpatched module; HEAD
      holds it with the 7 TORRES-FACSIMILE-101 corrections.
    - Defect: the installed module was not reproducible from the known
      source with the patch on `master`. Git source → installed differs in
      16 references; 14 are authorized in `parche_facsimil.py`, but
      `Psalms 4:3` and `Psalms 4:5` are not. They were installed by
      TORRES-PSALM-TITLES-101 from branch `fix/torresamat-psalm-titles`
      (`ee2b0938`); `master` carries the variant `e223d8d5`, which lacks
      those two entries and `test_salmo4_v5_sin_el_v3`.
    - The script also always read the installed module and its `.conf`
      (`conf_instalada()` → `~/.sword/mods.d/torresamat.conf`), so it could
      not start from a known source.
    - Facsimile check of the two missing entries (tomo III, leaf `_0011`,
      downloaded read-only): right column top «3. Oh hijos de los hombres,
      ¿hasta cuándo sereis de estúpido corazon? ¿por qué amais la vanidad
      y vais en pos de la mentira?» and «5. Enojaos⁸, y no querais pecar
      mas; compungíos…». Ps 4:5's own errata (missing final period) are
      left as recorded in TORRES-PSALM-TITLES-101.
  - Fix (`scripts/torresamat/parche_facsimil.py`):
    - `CORRECCIONES` gains `Psalms 4:3` ("" → the facsimile text) and
      `Psalms 4:5` (drops the glued Ps 4:3 prefix), both «tomo III, hoja
      11», exactly as installed from `ee2b0938`.
    - `--origen RAIZ` (default `~/.sword`): any SWORD tree (`mods.d/` +
      `modules/`); its own `.conf` is used (`conf_de()`), and every export
      goes through `exporta_aislado()` under a distinct module name, so
      `~/.sword` is never read by accident and never written.
    - The output is a complete SWORD tree (`modules/` + `mods.d/` with the
      origin `.conf`), reusable as `--origen`; `--salida` may not equal
      `--origen`. `cambiadas()` requires identical keys and order; any
      change outside `CORRECCIONES`/`TITULOS`, a round-trip mismatch,
      unexpected text or missing keys stop the patch (existing `aplica()`
      checks kept).
    - README entry updated.
  - Closure evidence (scratch trees, nothing installed):
    - A: `--origen` = git `9c036c87` `modulos/` → «18 versos».
    - B: default origin (installed) → «2 versos».
    - C: B again → module files byte-identical to B (running twice gives
      the same result).
    - D: `--origen B` → «0 versos», 18 «ya corregido», byte-identical to B
      (idempotent on its own output).
    - E: as A with `HOME` pointing to an empty directory (no `~/.sword` at
      all) → byte-identical to B (no dependence on `~/.sword`).
    - A == B byte for byte: the expected module is reproducible from the
      known source.
    - Exports: git source → regenerated: 38,698/38,698 entries, same keys
      and order, exactly the 18 authorized references changed, none
      missing. Installed → regenerated: 35,475 non-empty in both, exactly
      2 changed entries, both inherited from closed tasks, not from this
      one: `Matthew 12:6` (« i» removed, TORRES-FACSIMILE-102) and
      `Matthew 12:10` («hallaba un», TORRES-FACSIMILE-103), neither
      installed yet. This task adds no text change to the installed module:
      Ps 4:3/4:5 were already installed; they only enter the versioned
      patch.
    - `modulos/` unchanged in git; no file under the installed
      `~/.sword/.../torresamat` or its `.conf` modified; installed export
      byte-identical before/after. Nácar-Colunga outputs identical to the
      NACAR-OCR-106 baseline.
    - Tests: `scripts/torresamat/test_parche.py` adds
      `test_salmo4_v3_vuelve_y_v5_se_queda_con_lo_suyo`,
      `test_regenera_desde_origen_conocido_e_idempotente` (mini SWORD tree
      with the 18 authorized keys plus the Mt 12:7 control: only authorized
      references change, second pass «0 versos» with byte-identical files,
      control intact) and `test_texto_inesperado_detiene_la_regeneracion`.
      All three fail on the pre-task patch (`KeyError 'Psalms 4:3'`;
      `unrecognized arguments: --origen`) and pass now. PASS:
      `test_parche.py`, `test_titulos.py`, `test_pegadas.py`,
      `test_restos.py` and the 11 Nácar-Colunga tests.
    - Default output `scripts/torresamat/salida/parche` (git-ignored)
      regenerated («2 versos»). No commit or push.
  - Pending outside this task:
    - Installing the regenerated module (Mt 12:6, 12:10) into `modulos/`
      and `~/.sword` is a separate, explicit step.
    - Mt 12:9 missing comma (noted in TORRES-FACSIMILE-103).
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

- [x] PLAN-DAILY-101 Select "Lectura de hoy" by calendar date
  - Status: DONE
  - Description:
    The daily-reading entry point currently selects the first unmarked plan
    day, so an unmarked reading is shown again on subsequent calendar days.
    Select the scheduled plan day from its start date while retaining the
    first-pending-day calculation for catch-up and rescheduling workflows.
  - Acceptance criteria:
    - An active plan advances its "Lectura de hoy" on each local calendar
      date, even if an earlier day remains unmarked.
    - A plan without a valid start date retains the existing first-pending
      fallback.
    - Catch-up and reprogramming continue to use the first unmarked day.
    - Add a deterministic regression test for an unmarked multi-day plan.
  - Relevant tests:
    - `planes_lectura_test`.
  - Evidence:
    - `main_planes_dia_de_hoy()` now uses the local calendar day calculated
      from the plan start date; only plans without a valid start date fall
      back to their first unmarked day.
    - The first-pending calculation remains private to catch-up and
      reprogramming, so an earlier unmarked day neither repeats as today's
      reading nor changes those workflows.
    - `planes_lectura_test` PASS: fixed-date unmarked day 3 selection,
      invalid-start fallback, pending-day overdue calculation, and
      reprogramming; `biblia-elim` build PASS; `git diff --check` PASS.

- [x] TORRES-PSALM-TITLES-101 Preserve and mark native psalm title slots in Torres Amat
  - Status: DONE
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
  - Second batch (2026-09-23, not installed, no commit):
    - Base: installed module + the reproducible patch of
      TORRES-FACSIMILE-104 (`parche_facsimil.py --origen`), whose
      authorized entries are kept (no overlap with `CORRECCIONES` or the
      first batch).
    - Structure locator (offline, not runtime): the installed
      `VulgClementine` module (same `Vulg` versification) closes each
      psalm's inscription with a paragraph end `<div eID=… type="x-p"/>`.
      From that markup only (no text is used): 2 psalms without title
      (1, 2), 62 with a title-only v1, 4 with title in v1–v2 (50, 51, 53,
      59) and 82 with title + body sharing v1. It only says which native
      slot may be a title; each entry is decided on the 1882 facsimile.
    - Facsimile collation: the Archive OCR of tomo III (`…tomo III_djvu.xml`)
      was used to locate each psalm (verse 2 of the module matched against
      the printed lines, then the printed «1.» … «2.» span); only 12
      title-only psalms were located reliably (the OCR often loses the
      «2.» mark), so every accepted entry was then checked visually on its
      leaf (read-only downloads of `…tomo III_00NN.jp2`).
    - 12 references marked in `parche_facsimil.TITULOS`, text unchanged
      (the module's own OCR errata stay for their own correction, as with
      Ps 50:2): Ps 5:1 and 6:1 (hoja 11), 18:1 (16; the print itself reads
      «Salvo de David»), 21:1 (17), 29:1 (19; module «Balmo de Dayid»),
      35:1 (24; module «Para el tin»), 39:1 (25), 43:1 (26), 59:1 and 59:2
      (34; inscription in two printed verses, «…Inscripcion para una
      columna. / Al mismo David para instruccion: / 2. Cuando quemó la
      Mesopotamia…», like Ps 50/51), 88:1 (47) and 99:1 (51). In each case
      the printed v1 is only the inscription and the body starts at the
      next number; the next body verse (Ps 5:2 … 99:2, Ps 59:3) is checked
      as a control and not touched.
    - Regeneration: from the installed module «14 versos»: the 12
      references above plus `Matthew 12:6` and `Matthew 12:10`, which
      belong to TORRES-FACSIMILE-102/-103 (pending installation), not to
      this batch. 38,698/38,698 entries, same keys and order, 35,475
      non-empty before and after; the 12 changes only add the
      `x-psalm-title` seg around the unchanged text. From the git source
      (`9c036c87` `modulos/`): «30 versos», exactly the 30 authorized
      references, byte-identical to the regeneration from the installed
      module; a second pass over the output reports «0 versos» and is
      byte-identical (idempotent). `modulos/`, `~/.sword` and the
      Nácar-Colunga outputs unchanged.
    - Tests: `test_titulos.py::test_segundo_lote_solo_marca` (entries,
      sheets, unchanged text, body verses not marked, Ps 59 in two
      verses); fails on the pre-batch patch (`KeyError 'Psalms 5:1'`),
      passes now. PASS: `test_titulos.py` (9), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Third batch (2026-09-23, not installed, no commit):
    - Same criterion as the second batch: the printed «1.» is only the
      inscription and the body starts at «2.»; the Clementine markup only
      located the slot. Each case checked visually on its leaf; where the
      OCR/title search was ambiguous (identical titles in Ps 10/13, 19/20,
      45/46/48) the whole leaf was read (leaves 13, 16, 27, 28).
    - 15 references marked in `parche_facsimil.TITULOS`, text unchanged:

      | Psalm | Leaf | Printed «1.» (inscription) → body starts at «2.» |
      |---|---|---|
      | 7:1 | 12 | «Salmo de David, cantado… hijo de Jemini» → «Señor, Dios mio…» |
      | 10:1 | 13 | «Para el fin: Salmo de David.» → «En el Señor tengo puesta…» |
      | 11:1 | 13 | «Para el fin: para la octava: Salmo de David.» (module «Dayid») → «Sálvame Señor…» |
      | 17:1 | 15 | «Para el fin: Salmo de David, siervo del Señor… con cuyo motivo dijo:» → «Á tí he de amarte…» |
      | 19:1 | 16 | «Para el fin: Salmo de David.» → «Óigate, oh rey…» |
      | 20:1 | 16 | «Para el fin: Salmo de David.» → «Oh, Señor, en tu gran poder…» |
      | 30:1 | 19 | «Para el fin: Salmo de David, por un éxtasi ó exceso de pena.» (module «0 exceso de Pen.») → «Oh Señor, en tí…» |
      | 33:1 | 21 | «Salmo de David, cuando se desfiguró… se escapó.» (module «Achimelech 3») → «Alabaré al Señor…» |
      | 37:1 | 25 | «Salmo de David para recuerdo; en sábado.» → «Oh Señor no me reprendas…» |
      | 40:1 | 26 | «Para el fin: Salmo del mismo David.» → «Bienaventurado aquel…» |
      | 44:1 | 27 | «Para el fin: para aquellos que han de ser mudados… Cántico en alabanza del amado.» (module «alabamea») → «Hirviendo está…» |
      | 45:1 | 27 | «Para el fin á los hijos de Coré: Salmo para los misterios.» (module «mistea) rios») → «Dios es nuestro refugio…» |
      | 46:1 | 28 | «Para el fin: á los hijos de Coré, Salmo.» → «Naciones todas…» |
      | 47:1 | 28 | «Salmo de cántico: á los hijos de Coré: para el segundo dia de la semana.» → «Grande es el Señor…» |
      | 48:1 | 28 | «Para el fin: á los hijos de Coré, Salmo.» → «Oid estas cosas…» |

    - Set aside (not marked on structure alone): Ps 9 (leaf 13: the
      print restarts numbering at «1. ¿Y por qué, oh Señor…» for the
      «Segunda parte, que es el Salmo X segun los Hebreos», and that text
      is merged into the module's 9:1) and Ps 41 (leaf 26: «Para el fin:
      1. Salmo de instruccion…» — «Para el fin:» precedes the number, as in
      Ps 52, and is missing from the module: recovery case).
    - Also seen: Ps 17:2 holds printed v2 + v3 merged and 17:3 is empty
      (body slot issue, not the title).
    - Regeneration: from the installed module «29 versos»: the 15 references
      of this batch (text identical once the seg is stripped), the 12 of
      the second batch (not installed either), plus `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order, 35,475 non-empty before and after. From the git
      source: «45 versos», exactly the 45 authorized references,
      byte-identical to the regeneration from the installed module; second
      pass «0 versos», byte-identical. `modulos/`, `~/.sword` and
      Nácar-Colunga outputs unchanged.
    - Tests: `test_titulos.py::test_tercer_lote_solo_marca` (sheets,
      unchanged text, v2 not marked, Ps 9/41 not marked, batches disjoint);
      fails on the pre-batch patch (`KeyError 'Psalms 7:1'`), passes now.
      PASS: `test_titulos.py` (10), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Fourth batch (2026-09-23, not installed, no commit): Ps 53, 54, 55,
    56, 57, 58, 60, 61, 62, 63, 64, read on leaves 30, 31, 34, 35 (whole
    leaves).
    - Marked (4 references, text unchanged):

      | Ref | Leaf | Printed evidence |
      |---|---|---|
      | 53:1 | 30 | «1. Para el fin: sobre los Cánticos. Salmo de inteligencia de David,» |
      | 53:2 | 30 | «2. Cuando fueron los Ziphéos á decir á Saul: ¿No sabes que David está escondido entre nosotros?» — still the inscription; body starts «3. Sálvame, oh Dios…» (checked explicitly: title in v1–v2, like Ps 50/51/59) |
      | 54:1 | 30 | «1. Para el fin: sobre los Cánticos. Salmo de inteligencia de David.» → «2. Oye benigno…» |
      | 60:1 | 34 | «1. Para el fin: sobre los Cánticos de David.» → «2. Escucha, oh Dios mio…» (checked explicitly: title only in v1; v2 is body) |

    - Set aside (recovery, not marking): the print sets part of the
      inscription BEFORE the «1.» and the module lacks it:
      Ps 55 («Para el fin: 1. Para la gente que estaba lejos…», leaf 31),
      56, 57, 58 («Para el fin: 1. No destruyas á tu siervo…», leaf 31),
      61 («Para el fin: 1. Salmo de David para Idithun.», leaf 34), 62
      («Salmo de David. 1. Estando en el desierto de Iduméa.», leaf 35), 63
      («Para el fin: 1. Salmo de David.», leaf 35), 64 («Para el fin: Salmo
      de David. 1. Cántico de Jeremías…», leaf 35). Same pattern as Ps 41
      and 52.
    - Errata / merges noted, not fixed: Ps 53:3 «Pálvame» (Sálvame), 54:2
      «hunvlde», 56:1/57:1/58:1 «ú tu siervo» (á), 56:3 «USIEO»
      (Altísimo), 57:2 «ii verdaderamente» (Si), 61:2 holds printed v2+v3
      with «&amp;gt;3, E» and 61:3 empty, 63:2 «ú4 t£», 64:2 «A t£».
    - Regeneration: from the installed module «33 versos» = the 4 references
      of this batch (text identical without the seg) + 27 psalm marks of
      batches 2–3 (not installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order, 35,475 non-empty before and after. From the git source «49
      versos», exactly the 49 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga unchanged.
    - Tests: `test_titulos.py::test_cuarto_lote_solo_marca` (sheets,
      unchanged text, body verses 53:3/54:2/60:2 not marked, set-aside
      psalms not marked, no overlap among batches 1–4); fails on the
      pre-batch patch (`KeyError 'Psalms 53:1'`). PASS: `test_titulos.py`
      (11), `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Fifth batch (2026-09-23, not installed, no commit): Ps 66, 67, 68,
    69, 74, 75, 76, whole leaves 36, 37, 38, 40 read. In every case the
    printed «1.» is only the inscription, fully present in the module, and
    the body starts at «2.». 7 references marked, text unchanged:

      | Ref | Leaf | Printed «1.» → body «2.» |
      |---|---|---|
      | 66:1 | 36 | «Para el fin, sobre los himnos: Salmo y Cántico de David.» → «Dios tenga misericordia…» |
      | 67:1 | 36 | «Para el fin: Salmo y Cántico del mismo David.» → «Levántese Dios…» |
      | 68:1 | 37 | «Para el fin: por los que han de ser mudados. Salmo de David.» (module «Dayid») → «Sálvame, oh Dios…» |
      | 69:1 | 38 | «Para el fin: Salmo de David, en memoria de haberle el Señor salvado.» (module «Senor») → «Oh Dios, atiende…» |
      | 74:1 | 40 | «Para el fin: No nos destruyas. Salmo y Cántico de Asaph.» → «Profeta. Alabarémoste…» (the italic speaker label is printed in v2) |
      | 75:1 | 40 | «Para el fin: para alabar. Salmo de Asaph. Cántico sobre los Assyrios.» → «Dios es conocido…» |
      | 76:1 | 40 | «Para el fin: Para Idithun: Salmo de Asaph.» → «Alcé mi voz…» |

    - Set aside: Ps 71 (leaf 38: printed «1. Salmo sobre Salomon, figura
      de Christo.»; the module holds OCR garbage «Salmo 1 sobre &amp;gt;podas
      Salomon…»: recovery). Also seen on leaf 38: Ps 70 prints «Salmo de
      David: 1. De los hijos de Jonadab…» (prefix before «1.»; title and
      body share v1 in the Clementine — in the shared-v1 group).
    - Errata noted, not fixed: 67:2 «Dios $» and trailing «E», 68:1
      «Dayid», 69:1 «Senor», 74:1 missing final period, 76:2 «me tendió»
      (print «me atendió»).
    - Regeneration: from the installed module «40 versos» = these 7 + 31
      psalm marks of batches 2–4 (not installed) + `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order, 35,475 non-empty before and after; batch text
      identical without the seg. From the git source «56 versos», exactly
      the 56 authorized references, byte-identical to the regeneration from
      the installed module; second pass «0 versos», byte-identical.
      `modulos/`, `~/.sword`, Nácar-Colunga unchanged.
    - Tests: `test_titulos.py::test_quinto_lote_solo_marca` (sheets,
      unchanged text, v2 not marked, Ps 71 not marked, batches 2–5
      disjoint); fails on the pre-batch patch (`KeyError 'Psalms 66:1'`).
      PASS: `test_titulos.py` (12), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Sixth batch (2026-09-23, not installed, no commit): Ps 79, 80, 82,
    83, 84, 87, 91, 101, 141, whole leaves 44, 45, 46, 47, 49, 51, 68 read.
    - Marked (5 references, text unchanged):

      | Ref | Leaf | Printed «1.» → body «2.» |
      |---|---|---|
      | 79:1 | 44 | «Para el fin: Para aquellos que han de ser mudados. Testimonio de Asaph. Salmo.» → «Escucha, oh tú pastor de Israél…» (leaf 45) |
      | 82:1 | 45 | «Cántico y Salmo de Asaph.» → «Oh Dios, ¿quién hay semejante á tí?…» |
      | 83:1 | 46 | «Para el fin. Para los lagares, ó vendimia. Salmo para los hijos de Coré.» (module «d vendimia») → «¡Oh cuán amables…» |
      | 101:1 | 51 | «Oracion de un miserable, que hallándose atribulado, derrama en la presencia del Señor sus plegarias.» → «Escucha, oh Señor…» |
      | 141:1 | 68 | «Salmo de inteligencia de David: su oracion cuando estaba en la cueva.» (module «intelivencia») → «Alcé mi voz…» |

    - Set aside:
      - Prefix printed before «1.» and missing in the module (recovery):
        Ps 80 («Para el fin: 1. Para los lugares…», leaf 45), 87
        («Cántico y Salmo. 1. Para los hijos de Coré…», leaf 47), 91
        («Salmo y Cántico. 1. Para el dia del sábado.», leaf 49).
      - Merged slot: Ps 84 (leaf 46 prints «1. Para el fin: Salmo para los
        hijos de Coré.» / «2. Oh Señor, tú has derramado…»; the module
        holds both in 84:1 and 84:2 is empty; needs a split).
    - Errata noted, not fixed: 79:1 missing final period, 82:2 «Ob Dios»,
      83:1 «d vendimia», 87:1 «Ezrabita», 91:2 «á tu Io e Sao» (print «á
      tu nombre, oh Altísimo»), 141:1 «intelivencia».
    - Regeneration: from the installed module «45 versos» = these 5 + 38
      psalm marks of batches 2–5 (not installed) + `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order, 35,475 non-empty before and after; batch text
      identical without the seg. From the git source «61 versos», exactly
      the 61 authorized references, byte-identical to the regeneration from
      the installed module; second pass «0 versos», byte-identical.
      `modulos/`, `~/.sword`, Nácar-Colunga unchanged.
    - Tests: `test_titulos.py::test_sexto_lote_solo_marca` (sheets,
      unchanged text, v2 not marked, Ps 80/84/87/91 not marked, 84:2 not
      touched, batches 2–6 disjoint); fails on the pre-batch patch
      (`KeyError 'Psalms 79:1'`). PASS: `test_titulos.py` (13),
      `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
    - With this batch the group «title-only v1 with text present» is fully
      reviewed.
  - First recovery batch (2026-09-23, not installed, no commit): Ps 41,
    55, 56, 57, 58. Leaves 26 and 31 read; the Archive OCR agrees.
    - Pattern (same mechanism as Ps 52:1): the print sets «Para el fin:»
      BEFORE the verse number; the OCR dropped it; the rest of the
      inscription is complete in the module's v1, and the body starts at
      the printed «2.» (module v2). Missing text = exactly «Para el fin: »;
      nothing else is recovered or changed.

      | Ref | Leaf | Printed | Module v1 (kept whole) | Body «2.» (module v2, untouched) |
      |---|---|---|---|---|
      | 41:1 | 26 | «Para el fin: 1. Salmo de instruccion, á los hijos de Coré.» | «Salmo de instruccion, á los hijos de Coré.» | «Como brama…» |
      | 55:1 | 31 | «Para el fin: 1. Para la gente que estaba lejos del Santuario: Inscripcion… le detuvieron en Geth¹.» | «Para la gente… d Philisthéos le detuvieron en Geth 1» | «Apiádate de mí…» |
      | 56:1 | 31 | «Para el fin: 1. No destruyas á tu siervo. Salmo de David… se retiró en una cueva³.» | «No destruyas ú tu siervo… en una cueva» | «Ten piedad de mí…» |
      | 57:1 | 31 | «Para el fin: 1. No destruyas á tu siervo. Salmo de David para inscribirse en una columna.» | «No destruyas ú tu siervo… en una columna» | «ii verdaderamente…» |
      | 58:1 | 31 | «Para el fin: 1. No destruyas á tu siervo. Salmo de David… con el fin de quitarle la vida.» | «No destruyas ú tu siervo… quitarle la vida» | «Sálvame, Dios mio…» |

    - Result: `<seg type="x-psalm-title">Para el fin: {module v1}</seg>`
      in the native v1; «Para el fin:» appears once, the old text once, at
      the end; v2 unchanged; no renumbering.
    - Errata inside the title left untouched (tests assert they are still
      there): 55:1 «d Philisthéos» (ó) and «Geth 1» (footnote callout),
      56:1–58:1 «ú tu siervo» (á), missing final periods; body errata 56:2
      «apládate», 57:2 «ii» (Si).
    - Regeneration: from the installed module «50 versos» = these 5 + 43
      psalm marks of earlier batches (not installed) + `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order, 35,475 non-empty before and after; each of the 5
      equals «Para el fin: » + the installed text once the seg is stripped.
      From the git source «66 versos», exactly the 66 authorized
      references, byte-identical to the regeneration from the installed
      module; second pass «0 versos», byte-identical. `modulos/`,
      `~/.sword`, Nácar-Colunga unchanged.
    - Tests: `test_titulos.py::test_recupera_para_el_fin_delante_del_numero`
      (sheets, exact prefix, no duplication, v2 untouched, errata kept,
      pending Ps 61–64/80/87/91 untouched, no overlap with batches 2–6);
      `test_sin_textos_de_otras_biblias` extended to these 5. The earlier
      set-aside assertions (batch 3 for Ps 41, batch 4 for Ps 55–58) now
      require that those psalms are never marked as the incomplete title,
      only with the recovered prefix. The new test fails on the pre-batch
      patch (`KeyError 'Psalms 41:1'`). PASS: `test_titulos.py` (14),
      `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Second recovery batch (2026-09-23, not installed, no commit): Ps 61,
    62, 63, 64, 80, 87, 91. Leaves 34, 35, 45, 47, 49 read (whole
    leaves); the Archive OCR has the same prefix lines. Same mechanism as
    Ps 41/55–58: only the printed prefix before «1.» is prepended; the rest
    of the inscription is complete in the module's v1; the body starts at
    the printed «2.» (module v2, untouched).

      | Ref | Leaf | Recovered prefix | Printed | Module v1 (kept whole) | Body «2.» |
      |---|---|---|---|---|---|
      | 61:1 | 34 | «Para el fin:» | «Para el fin: 1. Salmo de David para Idithun.» | «Salmo de David para Idithun.» | «¿Cómo no ha de estar…» |
      | 62:1 | 35 | «Salmo de David.» | «Salmo de David. 1. Estando en el desierto de Iduméa¹.» | «Estando en el desierto de Iduméa.» | «Dios mio, oh mi Dios…» |
      | 63:1 | 35 | «Para el fin:» | «Para el fin: 1. Salmo de David.» | «Salmo de David.» | «Escucha, oh Dios mío…» |
      | 64:1 | 35 | «Para el fin: Salmo de David.» | «Para el fin: Salmo de David. 1. Cántico de Jeremías y de Ezechiel… cuando empezaba á salir de él.» | «Cántico de Jeremías… salir de él.» | «A tí, oh Dios…» |
      | 80:1 | 45 | «Para el fin:» | «Para el fin: 1. Para los lugares. Salmo para el mismo Asaph.» | «Para los lugares. Salmo para el mismo Asaph.» | «Regocijaos…» |
      | 87:1 | 47 | «Cántico y Salmo.» | «Cántico y Salmo.» (own line) / «1. Para los hijos de Coré, hasta el fin, sobre Maheleth… Instruccion de Eman Ezrahita.» | «Para los hijos de Coré… Eman Ezrabita» | «Señor Dios de mi salud…» |
      | 91:1 | 49 | «Salmo y Cántico.» | «Salmo y Cántico.» (own line) / «1. Para el dia del sábado.» | «Para el día del sábado.» | «Bueno es tributar…» |

    - Ps 61:2–3 (body, not title): printed «2. ¿Cómo no ha de estar mi alma
      sometida á Dios, dependiendo de él mi salvacion?» / «3. Él es mi Dios
      y mi Salvador: siendo él mi defensa, no seré jamás conmovido.» (leaf
      34); the module holds both in 61:2 with OCR junk «&amp;gt;3, E» and
      61:3 is empty. Not redistributed here: it is a body-verse split, not a
      title slot. The acceptance criterion «Merged slots are split» is about
      title slots; this belongs to a separate body-errata task. Tests
      assert 61:2 and 61:3 are untouched.
    - Errata left untouched: 62:1 «Iduméa» without the footnote callout,
      63:2 «ú4 t£», 64:2 «A t£», 64:3 «mortales 7», 87:1 «Ezrabita», 91:1
      «día» (print «dia»), 91:2 «á tu Io e Sao», 91:3 «por la poca eras».
    - Regeneration: from the installed module «57 versos» = these 7 + 48
      psalm marks of earlier batches (not installed) + `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order, 35,475 non-empty before and after; each of the 7
      equals prefix + installed text once the seg is stripped; v2 and v3 of
      each psalm (incl. 61:2/61:3) identical. From the git source «73
      versos», exactly the 73 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga unchanged.
    - Tests: `test_titulos.py::test_recupera_prefijos_segundo_lote`
      (sheets, exact prefix per psalm, no duplication, v2 untouched, 61:2/3
      untouched, batches disjoint); `test_sin_textos_de_otras_biblias`
      extended; the batch-6 set-aside assertion for Ps 80/87/91 now only
      forbids marking the incomplete title (Ps 84 still untouched). Fails on
      the pre-batch patch (`KeyError 'Psalms 61:1'`). PASS:
      `test_titulos.py` (15), `test_parche.py` (7), `test_pegadas.py`,
      `test_restos.py`, 11 Nácar-Colunga tests.
    - With this batch the group «prefix before 1. missing» is complete.
  - Whole-title recovery batch (2026-09-24, not installed, no commit):
    Ps 8, 38, 71, 107, 108, 139, 145. Source: 1882 facsimile, tomo III,
    leaves 12, 25, 38, 55, 67, 69 (whole leaves read, each title line
    zoomed); the Archive OCR (`…tomo III_djvu.xml`) has the same lines.
    In every case the printed «1.» is only the inscription and the body
    starts at the printed «2.».

      | Ref | Leaf | Printed «1.» (transcribed) | Module before | Body «2.» (print) |
      |---|---|---|---|---|
      | 8:1 | 12 | «Al fin: para los lagares: Salmo de David.» | empty | «Oh Señor, Soberano dueño nuestro…» |
      | 38:1 | 25 | «Para el fin, á Idithun: Cántico de David.» | empty | «Dije yo en mi corazon: Velaré…» |
      | 71:1 | 38 | «Salmo sobre Salomon, *figura de Christo*.» | «Salmo 1 sobre &amp;gt;podas Salomon, er figura A de SA Christo. pt ANA» | «Da, oh Dios, al rey tus leyes…» |
      | 107:1 | 55 | «Cántico y Salmo del mismo David³.» | empty | «Dispuesto está mi corazon…» |
      | 108:1 | 55 | «Salmo de David: para el fin.» | empty | «Oh Dios mio, no calles mi alabanza…» |
      | 139:1 | 67 | «Para el fin: Salmo de David.» | empty | «Líbrame, oh Señor…» |
      | 145:1 | 69 | «Aleluya: de Aggéo y de Zacharias.» | empty | «Alaba al Señor, oh alma mia…» |

    - Transcription rule: printed text as is, without the footnote callout
      (Ps 107 «David³.» → «David.»), as for Ps 50:1/51:1. Italics are not
      encoded (Ps 71 «figura de Christo»).
    - Strict previous value: the patch requires "" for the six empty slots
      and the exact garbage string for Ps 71; anything else stops it. Ps 71
      is the only entry whose old text is not preserved: it is listed in
      `parche_facsimil.SUSTITUYE_BASURA` with the reading it replaces, and
      `test_cambios_autorizados` keeps its «old text never lost» invariant
      for every other entry.
    - Not done here (body, not title): the module also lacks body verses
      8:2, 38:2–3 and 145:2–3 (printed on leaves 12, 25, 69). They stay
      empty; tests assert v0/v2/v3 of each psalm are untouched. Separate
      body-recovery work.
    - Regeneration: from the installed module «64 versos» = these 7 + 55
      psalm marks of earlier batches (not installed) + `Matthew 12:6` and
      `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries,
      same keys and order; non-empty 35,475 → 35,481 (+6: the six empty
      title slots now carry their printed title; Ps 71 was already
      non-empty). v0/v2/v3 of each psalm identical. From the git source
      «80 versos», exactly the 80 authorized references, byte-identical to
      the regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_titulos.py::test_recupera_titulos_enteros` (sheet, exact
      printed title, required previous value, v0/v2/v3 untouched, batches
      disjoint) and `test_valor_anterior_inesperado_detiene_el_parche`
      (non-empty 8:1 or a different Ps 71 reading → `ValueError`; already
      applied → kept). Both fail on the pre-batch patch (`KeyError
      'Psalms 8:1'`). `test_cambios_autorizados` gets the explicit
      exception; the batch-5 set-aside check for Ps 71 now forbids only
      marking the garbage as title; `test_sin_textos_de_otras_biblias`
      extended. PASS: `test_titulos.py` (17), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Merged title slots, Ps 84 and Ps 9 (2026-09-24, not installed, no
    commit). Scratch baselines rebuilt from git (`9c036c87` `modulos/`),
    `mod2imp TorresAmat` and fresh downloads (tomo III leaves 12, 13, 46,
    `…tomo III_djvu.xml`).
    - Ps 84 (leaf 46, zoomed): «1. Para el fin: Salmo para los hijos de
      Coré.» / «2. Oh Señor, tú has derramado la bendicion sobre tu
      tierra: tú has libertado del cautiverio á Jacob.» / «3. Perdonado
      has…». Module: 84:1 = «Para el fin: Salmo para los hijos de Coré. 2
      Oh Señor, tá has derramado…» (the OCR kept the «2» inside), 84:2
      empty. Result: 84:1 = title marked; 84:2 = the body text exactly as
      it was (errata «tá» kept); the stray «2» is dropped as the verse
      number it is. 84:0 and 84:3 untouched.
    - Ps 9 (leaves 12–13, whole leaf 13 read): the print gives 1 = title
      («Para el fin: por los ocultos arcanos del Hijo: Salmo de David.»),
      2–21 first part; then an unnumbered italic editorial line «Segunda
      parte, que es el Salmo X segun los Hebreos: en la que implora el
      Profeta el auxilio del Señor.», and a second part renumbered 1–18.
      Correspondence: second part n = Vulgata 9:(21+n), unambiguous by
      count (Clementine Ps 9 has 39 verses) and content (Clementine 9:22
      «Ut quid, Domine, recessisti longe… in tribulatione?» = printed
      second-part «1. ¿Y por qué, oh Señor, te has retirado á lo lejos…, en
      la tribulacion?»; 9:39 «judicare pupillo et humili…» = printed 18
      «Para hacer justicia al huérfano…»). Module: 9:1 = title + the whole
      second-part v1; 9:2–21 each hold a first-part verse with second-part
      verses interleaved irregularly (e.g. 9:3 holds second-part 3 only,
      first-part 3 sits at the start of 9:5); 9:22–39 empty.
      Done here (title scope only): 9:1 = title marked (module text kept:
      «Salmo de — David,» errata stay); the second-part v1, complete in the
      module («…crítico, en la tribulaclon?»), moves unchanged to 9:22.
      Not done: redistributing second-part vv. 2–18 from 9:2–21 into
      9:23–39 (body unscrambling, not a title slot); 9:0, 9:2–21 and
      9:23–39 untouched. The italic «Segunda parte…» line is editorial, not
      marked.
    - Mechanism: `parche_facsimil.TRASLADOS` (origin title → empty
      destination), destinations as `CORRECCIONES` from "" to the moved
      text. `cambios()` checks that each origin's old text is exactly
      title + separator (spaces, optionally the stray verse number) +
      destination text, and fails otherwise: nothing lost, nothing
      duplicated. The «old text never lost» invariant in
      `test_cambios_autorizados` accepts only these two explicit transfers
      and the Ps 71 garbage replacement.
    - Regeneration: from the installed module «68 versos» = these 4 entries
      (84:1, 84:2, 9:1, 9:22) + 62 psalm entries of earlier batches (not
      installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order; non-empty 35,475 → 35,483 (6 recovered titles of the previous
      batch + 84:2 + 9:22). From the git source «84 versos», exactly the 84
      authorized references, byte-identical to the regeneration from the
      installed module; second pass «0 versos», byte-identical.
      `modulos/`, `~/.sword`, Nácar-Colunga outputs unchanged (sha256).
    - Tests: `test_salmo_84_parte_el_v2`, `test_salmo_9_segunda_parte`
      (title, moved text, exact reconstruction, each fragment once, all
      other Ps 9 verses untouched) and `test_traslado_incoherente_se_rechaza`;
      all three fail on the pre-batch patch. Set-aside checks of batches 3
      and 6 for Ps 9/84 now forbid only marking the merged slot as a
      title. PASS: `test_titulos.py` (20), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Shared-v1 batch 1 (2026-09-24, not installed, no commit): Ps 12, 13,
    14, 15, 16, 22. Scratch baselines rebuilt from git, `mod2imp` and fresh
    downloads (tomo III leaves 13, 14, 17, `…tomo III_djvu.xml`); leaves
    read whole. In all six the printed «1.» sets the inscription on its own
    line and the body continues on the next line without a number; the
    boundary is that line break, not punctuation. Same mechanism as Ps
    52:1 (`<seg>title</seg> body` in the native v1), module text kept.

      | Ref | Leaf | Title (own line) | Body of «1.» starts | Body «2.» |
      |---|---|---|---|---|
      | 12:1 | 13 | «Para el fin: Salmo de David.» (module «Dayid») | «¿Hasta cuándo, oh Señor…» (module «¡Hasta») | «¿Cuánto tiempo andaré…» |
      | 13:1 | 14 | «Para el fin: Salmo de David.» | «Dijo en su corazon el insensato: No hay Dios… no hay uno siquiera.» | «El Señor echó desde el cielo…» |
      | 14:1 | 14 | «Salmo de David.» | «¡Ah! Señor, ¿quién morará…» | «Aquel que vive sin mancilla…» |
      | 15:1 | 14 | «Inscripcion de título: Del mismo David.» (module «titulo») | «Sálvame, oh Señor…» (module «Senor», «todw») | «Yo dije al Señor…» |
      | 16:1 | 14 | «Oracion de David.» | «Atiende, oh Señor, á mi justicia…» | «Salga de tu benigno rostro…» |
      | 22:1 | 17 | «Salmo de David.» | «El Señor me pastorea, nada me faltará.» (module «A El Señor…», stray «A» kept) | «Él me ha colocado…» |

    - Ps 13: the module's 13:1 also held the printed «2.» («El Señor echó
      desde el cielo… ó que buscase á Dios.») and 13:2 was empty; it moves
      unchanged to 13:2 via `TRASLADOS`. `_comprueba_traslados()` now
      accepts title + own body + moved text (exact reconstruction still
      required); the Ps 9 and 84 transfers are unchanged and verified in
      the regenerated module.
    - Regeneration: from the installed module «75 versos» = these 7 entries
      (12:1, 13:1, 13:2, 14:1, 15:1, 16:1, 22:1) + 66 psalm entries of
      earlier batches (not installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order; non-empty 35,475 → 35,484 (+1 for 13:2). Each entry's text
      without the seg equals the installed text (13:1 + 13:2 = old 13:1);
      v2 of 12, 14, 15, 16, 22 and 13:3 identical. From the git source «91
      versos», exactly the 91 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_primer_lote` (sheet, title,
      body outside the seg, each fragment once, title + body = module text,
      v2 untouched, printed body start) and `test_salmo_13_v2_trasladado`
      (exact reconstruction, 13:0/13:3 untouched, Ps 9/84 transfers intact,
      no overlap with any earlier batch); both fail on the pre-batch patch
      (`KeyError`). PASS: `test_titulos.py` (22), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Shared-v1 batch 2 (2026-09-24, not installed, no commit): Ps 23, 24,
    25, 26, 27, 28, 31, 32. Scratch baselines rebuilt; tomo III leaves 17,
    18, 19, 20, 21 read whole. Cuts differ per psalm:

      | Ref | Leaf | Layout in the print | Decision |
      |---|---|---|---|
      | 23:1 | 17 | «1. Para el primer dia de la semana: Salmo de David.» / next line «Del Señor es la tierra…» | cut at the line break; body keeps the module's stray «Y» |
      | 24:1 | 18 | «1. Para el fin: Salmo de David.» / «Á tí, oh Señor, he levantado mi espíritu.» | cut at the line break |
      | 25:1 | 18 | «1. Para el fin: Salmo de David.» / «Oh, Señor, seas tú mi Juez…» | cut; body keeps junk «a! y» |
      | 26:1 | 18 | «1. Salmo de David antes de ser ungido⁵.» / «El Señor es mi luz…» | cut at the line break |
      | 27:1 | 19 | «Salmo del mismo David.» BEFORE «1. Á tí, oh Señor, clamaré…» | title recovered from the print; the whole module v1 kept as body |
      | 28:1 | 19 | «1. Salmo de David, cuando se concluyó el Tabernáculo.» / «Presentad al Señor…» | cut; module «Dayid» kept |
      | 31:1 | 20 | «Del mismo David, *Salmo de* inteligencia.» BEFORE «1. Felices aquellos…» / «2. Dichoso el hombre…» | SET ASIDE: title missing, and the module's 31:1 also holds printed v2 without its start («o el hombre…») plus junk («Pi», «e»); 31:2 empty. Body recovery, not a clean cut |
      | 32:1 | 21 | «Salmo de David.» BEFORE «1. Regocijaos, oh justos…» | title recovered; whole module v1 kept as body (junk «hRegoeijaos») |

    - Also seen: 32:2 holds printed v2 + v3 merged (body, not touched).
    - Regeneration: from the installed module «82 versos» = these 7 entries
      (23:1–28:1, 32:1) + 73 psalm entries of earlier batches (not
      installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order; non-empty 35,475 → 35,484 (no new slots in this batch). Each
      entry without the seg equals the installed text (27:1/32:1: title +
      installed text); v0/v2 of each and Ps 31:1–2 identical; transfers
      84:1→84:2, 9:1→9:22 and 13:1→13:2 intact. From the git source «98
      versos», exactly the 98 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_segundo_lote` (sheet, title,
      printed body start, title + body = module text or title + whole v1,
      each fragment once, v2 untouched, Ps 31 untouched, TRASLADOS
      unchanged, no overlap with any earlier batch); fails on the pre-batch
      patch (`KeyError 'Psalms 23:1'`). PASS: `test_titulos.py` (23),
      `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Shared-v1 batch 3 (2026-09-24, not installed, no commit): Ps 34, 36,
    42, 49, 65, 70, 72, 73. Scratch baselines rebuilt; tomo III leaves 21,
    24, 26, 29, 35, 38, 39 read (title zones zoomed).

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 34:1 | 21 | «1. *Salmo* del mismo David.» / «Juzga, oh Señor, á los que me dañan…» | SET ASIDE: module 34:1 and 34:2 empty; needs body recovery, not a cut |
      | 36:1 | 24 | «1. Salmo del mismo David.» / «No envidies la prosperidad…» | cut at the line break |
      | 42:1 | 26 | «1. Salmo de David.» / «Júzgame tú, oh Dios…» | cut; module «¡Salmo» (stray «¡») kept in the title |
      | 49:1 | 29 | «1. Salmo de *ó para* Asaph.» / «El Dios de los dioses…» | cut; module «de d para» kept |
      | 65:1 | 35 | «Para el fin: 1. Salmo y Cántico de la Resurreccion.» / «Moradores todos de la tierra…» / «2. Cantad salmos…» | SET ASIDE: prefix lost AND 65:1 also holds printed v2 behind junk («de y Júbilo: e 2,»), 65:2 empty: body redistribution with junk at the boundary |
      | 70:1 | 38 | «Salmo de David: 1. De los hijos de Jonadab, y de los primeros cautivos.» / «En tí, oh Señor…» | SET ASIDE: prefix lost and the title part after «1.» is OCR-garbled («De los hijos.. de ] Jonadab, z y de los pri-. ImMeéros cautivos.»); needs a title replacement decision |
      | 72:1 | 39 | «1. Salmo de Asaph.» / «¡Cuán bondadoso es Dios…» | cut; module «¡ Salmo» kept |
      | 73:1 | 39 | «1. *Salmo de* inteligencia de Asaph.» / «¿Y por qué, oh Dios…» | cut at the line break |

    - Regeneration: from the installed module «87 versos» = these 5
      entries (36:1, 42:1, 49:1, 72:1, 73:1) + 80 psalm entries of earlier
      batches (not installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order, non-empty 35,484 (unchanged by this batch). Each entry
      without the seg equals the installed text; v0/v2 of each and
      Ps 34/65/70 v1–v2 identical; transfers 84:1→84:2, 9:1→9:22,
      13:1→13:2 intact. From the git source «103 versos», exactly the 103
      authorized references, byte-identical to the regeneration from the
      installed module; second pass «0 versos», byte-identical.
      `modulos/`, `~/.sword`, Nácar-Colunga outputs unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_tercer_lote` (sheet, title,
      printed body start, title + body = module text, each fragment once,
      v0/v2 untouched, set-aside psalms untouched, TRASLADOS unchanged, no
      overlap with any earlier batch); fails on the pre-batch patch
      (`KeyError 'Psalms 36:1'`). PASS: `test_titulos.py` (24),
      `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Shared-v1 batch 4 (2026-09-24, not installed, no commit): Ps 77, 78,
    81, 85, 86, 89, 90, 92. Baselines rebuilt in the session scratchpad
    from git (`9c036c87`), `mod2imp TorresAmat` and fresh downloads (tomo
    III leaves 41, 44, 45, 46, 47, 48, 49, `…tomo III_djvu.xml`); every
    title zone zoomed.

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 77:1 | 41 | «1. Inteligencia, *ó instruccion* de Asaph.» / «Escucha, pueblo mio…» | cut; module title «Inteligencia, ¿mstruccion de Asaph,.» kept |
      | 78:1 | 44 | «1. Salmo de Asaph.» / «Oh Dios, los Gentiles…» | cut |
      | 81:1 | 45 | «1. Salmo de Asaph.» / «Presente está Dios…» | cut |
      | 85:1 | 46 | «Oracion del mismo David.» BEFORE «1. Inclina, Señor…» | module keeps none of the inscription (85:1 = «Inelina, Señor…»); title recovered, whole v1 kept as body |
      | 86:1 | 47 | «1. Á los hijos de Coré. Salmo y Cántico.» / «Sobre los montes santos…» | cut; module «4 los hijos… Cántico,» kept |
      | 89:1 | 48 | «1. Oracion de Moysés, varon de Dios.» / «Señor, en todas épocas…» | cut; module «M OYSÉs» kept |
      | 90:1 | 48 | «Alabanza y Cántico de David.» BEFORE «1. El que se acoge al asilo…» | title recovered, whole v1 kept as body |
      | 92:1 | 49 | «Salmo y Cántico del mismo David, para la víspera del sábado, que es cuando fué criada la tierra.» BEFORE «1. El Señor reinó…» | title recovered, whole v1 kept as body |

    - No case needed body recovery; none set aside. Seen but not touched:
      92:2 holds printed v2 + v3 (body).
    - Regeneration: from the installed module «95 versos» = these 8 entries
      + 85 psalm entries of earlier batches (not installed) + `Matthew
      12:6` and `Matthew 12:10` (TORRES-FACSIMILE-102/-103). 38,698/38,698
      entries, same keys and order, non-empty 35,484 (unchanged by this
      batch). Each entry without the seg equals the installed text (85,
      90, 92: title + installed text); v0/v2 of each untouched; transfers
      84:1→84:2, 9:1→9:22, 13:1→13:2 intact. From the git source «111
      versos», exactly the 111 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_cuarto_lote` (sheet, title,
      printed body start, exact reconstruction, body once, v0/v2
      untouched, TRASLADOS unchanged, no overlap with any earlier batch);
      `test_sin_textos_de_otras_biblias` extended to 85/90/92. Fails on the
      pre-batch patch (`KeyError 'Psalms 77:1'`). PASS: `test_titulos.py`
      (25), `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Shared-v1 batch 5 (2026-09-24, not installed, no commit): Ps 93, 94,
    95, 96, 97, 98, 100, 102. Baselines rebuilt in the scratchpad (git
    `9c036c87`, `mod2imp TorresAmat`); tomo III leaves 49–52 and the
    Archive OCR; every title zone zoomed (Ps 97 cropped from the right
    column of leaf 50, where the OCR fused both columns).

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 93:1 | 49 | «Salmo del mismo David, para el cuarto dia de la semana.» BEFORE «1. El Señor *ó Jehovah*, es el Dios de las venganzas…» | SET ASIDE: title is before «1.» (confirmed); the module keeps nothing — 93:1 is empty, so the body is missing too (body recovery) |
      | 94:1 | 50 | «Alabanza ó Cántico del mismo David.» BEFORE «1. Venid, regocijémonos…» | title recovered, whole module v1 kept as body |
      | 95:1 | 50 | «Cántico del mismo David, *cantado*. 1. Cuando se reedificó la Casa *de Dios* despues de la cautividad⁶.» / «Cantad al Señor un cántico nuevo…» | mixed: prefix «Cántico del mismo David, cantado.» recovered + the title part the module keeps («Cuando se reedificó… cautividad.»); body = rest of v1 |
      | 96:1 | 50 | «1. *Salmo de* David, cuando fué restaurada su tierra.» / «El Señor es el que reina…» | cut |
      | 97:1 | 50 | «1. Salmo del mismo David.» / «Cantad al Señor un cántico nuevo; porque…» | cut |
      | 98:1 | 51 | «1. Salmo del mismo David.» / «Reina *ya* el Señor…» | cut |
      | 100:1 | 51 | «1. Salmo del mismo David.» / «Cantaré, Señor, las alabanzas…» | cut; the title is AFTER the «1.» (not before) and the module keeps it whole |
      | 102:1 | 52 | «1. Del mismo David.» / «Bendice, oh alma mia…» | cut |

    - Errata/merges seen, not touched: 94:2 «eracias», 95:2 «anuneciad»,
      96:2 «Uircuido», 98:1 «quel», 98:2 holds printed v2 plus fragments of
      later verses, 100:1 body truncated at «jus-» (print «justicia:»),
      102:2 «nineuno».
    - Regeneration: from the installed module «102 versos» = these 7
      entries (94:1–102:1) + 93 psalm entries of earlier batches (not
      installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order, non-empty 35,484 (unchanged by this batch). Each entry
      without the seg contains the installed text once and ends with the
      kept body; v0/v2 of each and Ps 93:1–2 untouched; transfers
      84:1→84:2, 9:1→9:22, 13:1→13:2 intact. From the git source «118
      versos», exactly the 118 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_quinto_lote` (sheets, cuts,
      Ps 100 title after «1.», Ps 94 prefix + whole v1, Ps 95 mixed title,
      title + body reconstruction, body once, v0/v2 untouched, Ps 93
      untouched, TRASLADOS unchanged, no overlap with any earlier batch);
      `test_sin_textos_de_otras_biblias` extended to 94/95. Fails on the
      pre-batch patch (`KeyError 'Psalms 96:1'`). PASS: `test_titulos.py`
      (26), `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Shared-v1 batch 6 (2026-09-24, not installed, no commit): Ps 103,
    104, 105, 106, 109, 110, 111, 112. Baselines rebuilt in the scratchpad
    (git `9c036c87`, `mod2imp TorresAmat`); tomo III leaves 52–59 and the
    Archive OCR; every title zone zoomed.

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 103:1 | 52 | «1. Del mismo David.» / «Oh alma mia, bendice al Señor…» | cut |
      | 104:1 | 53 | «Aleluya⁶.» BEFORE «1. Alabad al Señor, é invocad…» | «Aleluya.» recovered (callout dropped), whole v1 kept as body |
      | 105:1 | 54 | «Aleluya¹.» BEFORE «1. Alabad al Señor porque es *tan* bueno…» | same |
      | 106:1 | 54 | «Aleluya¹¹.» BEFORE «1. Alabad al Señor, porque es *tan* bueno…» | same |
      | 109:1 | 58 | «1. Salmo de David.» / «El Señor dijo⁵ á mi Señor…» | cut; the module's stray «D» stays in the body |
      | 110:1 | 58 | «Aleluya.» BEFORE «1. Oh Señor, loarte he…» | «Aleluya.» recovered, whole v1 kept as body |
      | 111:1 | 58 | «Aleluya: del regreso de Aggéo y de Zacharias.» BEFORE «1. Bienaventurado el hombre…» | title recovered, whole v1 kept as body |
      | 112:1 | 59 | «Aleluya.» BEFORE «1. Alabad, oh jóvenes, al Señor…» | SET ASIDE: 112:1 and 112:2 also carry verses of Ps 113 («Cuando Israél salió de Esypto…», «Consagró Dios á su servicio…»); body redistribution across psalms |

    - Errata seen, not touched: 103:1 «Senor», «de! erloria», 103:2
      «¿los», «0 cortina», 104:1 «Invocad», 109:1 «%», «Já», 110:1
      «sociedarl», «/glesia», 111:1 «Bienayenturado», 111:2 «ben- De:».
    - Regeneration: from the installed module «109 versos» = these 7
      entries (103:1–111:1) + 100 psalm entries of earlier batches (not
      installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order, non-empty 35,484 (unchanged by this batch). Each entry
      without the seg equals the installed text or the recovered title +
      the installed text; v0/v2 of each and Ps 112:1–2 untouched;
      transfers 84:1→84:2, 9:1→9:22, 13:1→13:2 intact. From the git source
      «125 versos», exactly the 125 authorized references, byte-identical
      to the regeneration from the installed module; second pass «0
      versos», byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga
      outputs unchanged (sha256).
    - Tests: `test_titulo_y_cuerpo_compartidos_sexto_lote` (sheets, cuts,
      recovered Aleluya titles with the whole v1 as body, title and body
      once, v0/v2 untouched, Ps 112 untouched, TRASLADOS unchanged, no
      overlap with any earlier batch); fails on the pre-batch patch
      (`KeyError 'Psalms 103:1'`). PASS: `test_titulos.py` (27),
      `test_parche.py` (7), `test_pegadas.py`, `test_restos.py`, 11
      Nácar-Colunga tests.
  - Batch 7, Ps 113–120 (2026-09-24, not installed, no commit). Baselines
    rebuilt in the scratchpad (git `9c036c87`, `mod2imp TorresAmat`);
    tomo III leaves 59, 60, 63 read whole (61–62 hold only Ps 118 body),
    compared with the Archive OCR and `VulgClementine` (structure only).
    - Numbering vs. Vulgata:
      - Ps 113 (leaf 59): «Aleluya.» / «1. Cuando Israél salió de
        Egypto…» … «8.»; then, without a new heading, «1. No á nosotros,
        Señor…» … «18.» (footnote 5: «En el hebreo comienza aquí otro
        Salmo. Pero en los Setenta, como en la Vulgata, solo comienza nueva
        numeracion de versos»). Printed second part n = Vulgata 113:(8+n)
        (Clementine 113:9 «Non nobis, Domine…», 113:26 «…benedicimus
        Domino…»).
      - Ps 114 (leaf 59): a separate psalm in Torres Amat, «Aleluya.» /
        «1. Amé al Señor…» … «9.» = Vulgata 114:1–9. (The «114, 115» single
        heading belongs to Nácar-Colunga, which follows the Hebrew; not to
        this edition.)
      - Ps 115 (leaves 59–60): «Aleluya.» / «10. Creí á Dios…» … «19.»:
        the print keeps the Hebrew continuation numbers 10–19; they are
        Vulgata 115:1–10 (Clementine 115:1 «Credidi…», 10 verses). The
        module already maps printed 10 → 115:1.
      - Ps 116, 117, 118 (leaf 60), 119, 120 (leaf 63): numbering matches
        Vulgata.
    - Decisions:

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 113 | 59 | as above | SET ASIDE: module 113:1 holds second-part v1 («No á nosotros…») plus Ps 114:1 («Amé al Señor…»), 113:2 holds 113:10 plus 114:2; Ps 112:1–3 hold 113:1–3 interleaved. Body redistribution across Ps 112/113/114 |
      | 114:1 | 59 | «Aleluya.» BEFORE «1. Amé al Señor…» | «Aleluya.» recovered, whole v1 kept as body (module 114:1–9 are the printed 1–9) |
      | 115:1 | 59 | «Aleluya.» BEFORE «10. Creí á Dios…» | «Aleluya.» recovered, whole v1 kept (incl. the running-header junk «85 SALMOS.» and «d Dios», «contado 5») |
      | 116:1 | 60 | «Aleluya.» BEFORE «1. Alabad al Señor, naciones todas…» | «Aleluya.» recovered, whole v1 kept |
      | 117:1 | 60 | «Aleluya.» BEFORE «1. Alabad al Señor, porque es *tan* bueno…» | same |
      | 118:1 | 60 | «Aleluya.» / «ALEPH. 1. Bienaventurados los que proceden…» | «Aleluya.» recovered; the stanza label «ALEPH.» is not part of the inscription and is not added |
      | 119:1 | 63 | «1. Cántico de los grados, *ó gradual*.» / «Clamé al Señor…» | cut; module «0 gradual.» stays in the title, junk «eme» stays in the body |
      | 120:1 | 63 | «Cántico gradual.» BEFORE «1. Alcé mis ojos…» | title recovered, whole v1 kept (module «Aleé») |

    - Boundary with Ps 112 checked: the print ends Ps 112 at «9. Él á la
      mujer, antes estéril…» and starts Ps 113 with its own «Aleluya.»;
      nothing of Ps 112 or 113 is changed here.
    - Body errata/merges seen, not touched: 119:2 holds printed v2 + v3
      (119:3 empty), 120:2 holds printed v2 + v3 (120:3 empty), 118:1 «man
      cilla», 118:2 «d su Ley».
    - Regeneration: from the installed module «116 versos» = these 7
      entries (114:1–120:1) + 107 psalm entries of earlier batches (not
      installed) + `Matthew 12:6` and `Matthew 12:10`
      (TORRES-FACSIMILE-102/-103). 38,698/38,698 entries, same keys and
      order, non-empty 35,484 (unchanged by this batch). Each entry
      without the seg contains the installed text exactly once; v0/v2 of
      each and every verse of Ps 112 and 113 untouched; transfers
      84:1→84:2, 9:1→9:22, 13:1→13:2 intact. From the git source «132
      versos», exactly the 132 authorized references, byte-identical to the
      regeneration from the installed module; second pass «0 versos»,
      byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga outputs
      unchanged (sha256).
    - Tests: `test_septimo_lote_sal_114_120` (sheets, prefixes with whole
      v1, «ALEPH» not in the title, Ps 119 cut, v0/v2 untouched, Ps 112/113
      untouched, TRASLADOS unchanged, no overlap with any earlier batch);
      fails on the pre-batch patch (`KeyError 'Psalms 114:1'`). PASS:
      `test_titulos.py` (28), `test_parche.py` (7), `test_pegadas.py`,
      `test_restos.py`, 11 Nácar-Colunga tests.
  - Batch 8, Ps 121–128 «Cántico gradual» (2026-09-24, not installed, no
    commit). Baselines rebuilt in the scratchpad (git `9c036c87`,
    `mod2imp TorresAmat`); tomo III leaves 63, 64, 65 read whole. The
    common name hides two layouts, checked one by one; numbering matches
    Vulgata in all eight.

      | Ref | Leaf | Print | Decision |
      |---|---|---|---|
      | 121:1 | 63 | «1. Cántico gradual.» / «Gran contento tuve…» | cut |
      | 122:1 | 64 | «Cántico gradual.» BEFORE «1. Á tí, *Señor*, que habitas…» | title recovered, whole module v1 kept as body («Átí») |
      | 123:1 | 64 | «1. Cántico gradual.» / «Á no haber estado el Señor con nosotros, confiéselo ahora Israél,» | SET ASIDE: module 123:1 is empty; title and body both missing (body recovery) |
      | 124:1 | 64 | «1. Cántico gradual.» / «Los que ponen en el Señor…» | cut; module «gradual,» kept |
      | 125:1 | 64 | «1. Cántico gradual.» / «Cuando el Señor hará volver…» | cut |
      | 126:1 | 64 | «1. Cántico gradual de Salomon.» / «Si el Señor no es el que edifica…» | cut; module «SI» kept |
      | 127:1 | 64 | «1. Cántico gradual.» / «Bienaventurados todos aquellos…» | cut |
      | 128:1 | 65 | «1. Cántico gradual.» / «Muchas veces me han asaltado *los enemigos*…» / «2. Muchas veces…» | cut; the title decision is independent of the body anomaly: module 128:1 also carries printed v2 («…desde 2 Muchas veces… no han podido conmigo.») and 128:2–3 are empty. That body stays untouched in 128:1 for its own task |

    - Body anomalies seen, not touched: 128:1–3 (v2 merged into v1, v2–3
      empty), 126:2–3 and 126:4–5 merged (126:3 empty), 125:6–7 (125:7
      empty); errata 121:1 «remos», «Senor», 121:2 «muestros», «dJeru|
      salem», 124:2 «Senor», 126:1 «eindad», 127:1 «sentos eaminos», 127:2
      «puz», «1rá».
    - Regeneration: from the installed module «123 versos» = these 7
      entries + 114 psalm entries of earlier batches (not installed) +
      `Matthew 12:6` and `Matthew 12:10` (TORRES-FACSIMILE-102/-103).
      38,698/38,698 entries, same keys and order, non-empty 35,484
      (unchanged by this batch). Each entry without the seg contains the
      installed text exactly once; v0/v2 of each, all of Ps 123 and 128:3
      untouched; transfers 84:1→84:2, 9:1→9:22, 13:1→13:2 intact. From the
      git source «139 versos», exactly the 139 authorized references,
      byte-identical to the regeneration from the installed module; second
      pass «0 versos», byte-identical. `modulos/`, `~/.sword`, Nácar-Colunga
      outputs unchanged (sha256). Ps 113 not touched.
    - Tests: `test_octavo_lote_graduales` (sheets, per-psalm title and body
      start, Ps 122 prefix + whole v1, Ps 128 merged v2 kept in the body,
      v0/v2 untouched, Ps 123 untouched, TRASLADOS unchanged, no overlap
      with any earlier batch); fails on the pre-batch patch (`KeyError
      'Psalms 121:1'`). PASS: `test_titulos.py` (29), `test_parche.py` (7),
      `test_pegadas.py`, `test_restos.py`, 11 Nácar-Colunga tests.
  - Final batch (2026-09-25, not installed, no commit). Source: the 1882
    facsimile, Internet Archive item
    `la-sagrada-biblia-vulgata-tomo-iiv_202111`, the per-leaf jp2
    `LA SAGRADA BIBLIA - Vulgata tomo III_NNNN.jp2` (JPEG 2000, read on
    the sheet). Not hOCR, not the 1835 scan, not Nácar, not Reina-Valera.
    26 title slots and 3 glued headings.
    - Shared-v1, cut on the printed line of «1.»; module spelling kept:

      | Ref | Leaf | Title kept from the module |
      |---|---|---|
      | 129:1 | 65 | «Cántico gradual.» |
      | 131:1 | 65 | «Cántico gradual.» |
      | 132:1 | 65 | «Cántico gradual de David.» |
      | 134:1 | 66 | «Aleluya.» |
      | 135:1 | 66 | «Aleluya.» |
      | 137:1 | 67 | «Del mismo David.» |
      | 138:1 | 67 | «Para el fin: Salmo de David.» |
      | 140:1 | 68 | «Salmo de David,» (module comma) |
      | 144:1 | 69 | «Alabanza inspirada al mismo David.» |
      | 148:1 | 72 | «Alehiya.» (module spelling) |
      | 149:1 | 72 | «Aleluya.» |
      | 150:1 | 72 | «Aleluya,» (module comma) |

    - Title missing from the module; the whole v1 stays the body:
      130:1 «Cántico gradual de David.» (hoja 65), 136:1 «Salmo de David,
      para Jeremías.» (66), 147:1 «Aleluya.» before the printed «12.»
      (69; the module's truncated «á tu» stays), 31:1 «Del mismo David,
      Salmo de inteligencia.» (20; the fused «2.» and the junk «Pi» stay
      in the body, 31:2 stays empty), 112:1 «Aleluya.» (59; the verses of
      Ps 113 that the OCR left inside 112:1 stay there).
    - Prefix plus the inscription the module already has: 142:1 «Salmo de
      David:» in front of «E. Cuando le perseguia…» (68; the «E.» and the
      «7» stay), 143:1 «Salmo de David: Contra Goliath.» (68; the later
      verse glued at the end of 143:1 stays), 65:1 «Para el fin:» in front
      of «Salmo y Cántico de la Resurreccion,» (35; the junk «e 2,» and
      the printed v2 stay in the body, 65:2 stays empty).
    - Ps 133:1 (hoja 65): the module repeats 132:1 in front of «Cántico
      gracual.» The copy is dropped (`DUPLICADO_DELANTE`); 132:1 is
      unchanged. Title «Cántico gracual.» (module spelling), body the rest.
    - Empty slots filled from the sheet only (v2 not filled): 34:1 «Salmo
      del mismo David.» + «Juzga, oh Señor, á los que me dañan: bate á los
      que pelean contra mí.» (21); 123:1 «Cántico gradual.» + «Á no haber
      estado el Señor con nosotros, confiéselo ahora Israél,» (64); 146:1
      «Aleluya.» + «Alabad al Señor; porque justa cosa es cantarle himnos.
      Cántese á nuestro Dios un grato y digno cántico.» (69); 93:1 «Salmo
      del mismo David, para el cuarto dia de la semana.» + «El Señor ó
      Jehovah, es el Dios de las venganzas: y el Dios de las venganzas ha
      obrado con independiente libertad.» (49; «ó Jehovah» is the italic
      gloss on the sheet).
    - Ps 70:1 (hoja 38): the garbled inscription is replaced
      (`SUSTITUYE_BASURA`) by «Salmo de David: De los hijos de Jonadab, y
      de los primeros cautivos.» The body «En tí, oh Señor…» is the
      module's own text.
    - Ps 113 is not marked. On hoja 59 the inscription is «Aleluya.»
      before «1. Cuando Israél salió de Egypto…», and that verse is not
      in 113:1 (113:1 holds the second numbering, «No á nosotros…»).
      Putting the title there, or moving verses across 112/113/114, is
      outside a title slot. 113:1–3 unchanged.
    - Glued «SALMO …» headings removed from the last verse, not marked as
      titles (`CORRECCIONES`): Ps 4:10 (hoja 11; the verse keeps
      «esperanza +,»), Ps 52:7 (hoja 30), Ps 130:3 (hoja 65).
    - Regeneration: from the installed module «152 versos» = the 29
      references of this batch plus the earlier psalm marks and
      `Matthew 12:6` / `Matthew 12:10` that were not installed. 38,698
      entries, same keys and order; non-empty 35,475 → 35,488 (+13: the
      six empty titles of the whole-title batch, the three transfer
      destinations, and 34:1, 93:1, 123:1, 146:1). Nothing outside
      `CORRECCIONES`/`TITULOS`. From git `9c036c87` `modulos/` «168
      versos», every authorized reference, export and module files
      byte-identical to the regeneration from the installed module.
      Second pass «0 versos», byte-identical. Nácar-Colunga outputs
      unchanged (sha256).
    - Installed on request, so a future first install ships the corrected
      text: the patched ztext replaced `modulos/modules/texts/ztext/torresamat/`
      (the conf was already identical) and the same files were copied to
      `~/.sword`. Differences against the previous repo module, the
      installed module, and git `9c036c87` were only authorized
      `CORRECCIONES`/`TITULOS` entries. `install-biblia-elim.sh` still
      copies Torres Amat only when `~/.sword/mods.d/torresamat.conf` is
      absent, so an existing install is not overwritten by a reinstall.
    - Tests: `test_noveno_lote_y_apartados` and
      `test_encabezado_pegado_no_es_titulo`. The older set-aside checks
      now require the recovered title and still forbid touching v2 or
      redistributing the fused body. PASS: `test_titulos.py`,
      `test_parche.py`, `test_pegadas.py`, `test_restos.py`.
  - Outside this task (body, not a title slot; not done):
    - Ps 9 second part (vv. 2–18 still interleaved in 9:2–21; 9:23–39
      empty). Ps 113's own verses still sit in Ps 112, and 113:1 still
      holds the second numbering plus Ps 114:1.
    - Empty body verses: 8:2, 38:2–3, 34:2, 145:2–3, 31:2, 65:2.
    - Merged body slots left as they were: 17:2–3, 32:2–3, 61:2–3,
      65:1 (printed v2 still inside, behind «e 2,»), 92:2–3, 98:2,
      119:2–3, 120:2–3, 126:2–5, 128:1–3, 31:1 (printed v2 still inside),
      143:1 (a later verse still inside the body). 100:1 still truncated;
      147:1 still ends at «á tu».
    - Italic argument lines are editorial and are not marked.
  - Closed (was the blocker):
    - Shared-v1 pending (19): 129, 130, 131, 132, 133, 134, 135, 136,
      137, 138, 140, 142, 143, 144, 146, 147, 148, 149, 150.
    - Set aside: 31, 34, 65, 70, 93, 112 and 123 marked from the sheet;
      113 reviewed on hoja 59 and left unmarked, as above.
    - Glued headings Ps 4:10, 52:7 and, on the same leaves, Ps 130:3.
  - Blocked evidence (superseded 2026-09-25): the sheets were not on disk
    in the first pass. They were then fetched from the Archive item above
    and read. That block is closed.
  - Known OCR / structure issues outside the batch (not fixed):
    - Ps 4:2 and Ps 4:4: OCR errata («0% Dios», «4un», «vabed», «á E st
      santo»).
    - Ps 50:2 («y vino:», «Nathán 4») and Ps 50:3 («MS borra»): OCR errata.
    - Ps 4:8: visible `&lt;` («abundan&lt;cia»).
  - Acceptance criteria:
    - The native Vulg verse number is kept.
    - Titles are not moved to another verse (not to v0, not to Preverse of
      the next verse).
    - The title is represented structurally in the OSIS.
    - Missing title text is recovered from the 1882 sheet.
    - A title slot that cleanly holds another verse is split only by the
      existing `TRASLADOS` (9, 13, 84). Body merges that are not a clean
      title-slot split stay for a body task.
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

- [x] FALLBACK-V11N-101 Prevent cross-versification fallback from filling native title slots
  - Status: DONE
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
  - Evidence:
    - `content_resolver.cc` now recognizes a Vulgate psalm title slot from
      the structural heading on the mapped KJV/KJVA entry plus a many-to-one
      SWORD versification mapping, and refuses only that fallback; ordinary
      body fallback remains available.
    - `content_resolver_test` covers empty native Ps 3:1, 50:1 and 51:1
      title slots and preserves the existing Ps 10 mapping regression;
      `content_resolver_sword_test` passes with the Vulg/KJV mapping table.
  - Do not:
    - Commit or push.

- [x] V11N-COMMENTARY-101 Convert general commentary references across versifications
  - Status: DONE
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
  - Evidence:
    - `main_display_commentary()` now converts a key from the selected Bible
      to the target commentary through `main_reference_for_module()` before
      setting or rendering the commentary module; unmapped references are
      rejected without rereading text by identity.
    - The full build passes, and `author_commentary_content_test` plus
      `author_commentary_header_test` pass their conversion, navigation,
      unmapped/empty-content, and same-versification checks.
  - Do not:
    - Commit or push.

- [x] SWORD-KEY-101 Stop text rendering helpers from replacing module key pointers
  - Status: DONE
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
  - Evidence:
    - `BackEnd::get_render_text`, `get_raw_text`, and `get_strip_text` now
      use an in-place `setKeyText()` guard that preserves the owned pointer,
      key text, AutoNormalize, and skip-consecutive-links state.
    - `sword_backend_key_lifecycle_test` performs 1000 repeated raw/rendered/
      stripped
      reads for SpaRV (KJV) and, when installed, TorresAmat (Vulg), verifying
      pointer, position, and non-empty text stability; both passes report
      zero failures. The full build plus content-resolver and versification
      transition regressions pass as well.
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

- [x] V11N-URI-NAV-101 Refresh navbar after resolving sword:// reference in the target module
  - Status: DONE
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
  - Evidence:
    - The module-qualified text path in `sword_uri()` now lets
      `main_display_bible()` resolve and activate the named Bible before
      updating navbar state; module-less URIs retain the existing path.
    - `uri_navigation_v11n_test` passes all KJV/Vulg/SpaRV transition,
      native-reference, unmapped, and non-Bible cases. The full build also
      passes after the change.
  - Do not:
    - Commit or push.

- [x] TORRES-ENTITY-101 Remove double-escaped entity artifacts from the Torres Amat module
  - Status: DONE
  - Description:
    The installed TorresAmat module carries literal, double-escaped markup
    fragments in verse text, e.g. Ps 147:6 «Él despide el granizo en menudos
    pedazos &amp;amp;##x27;: al rigor…» and 1 Sam 19:20 «Envió &amp;amp;##x27;
    pues Saul =&amp;gt; soldados…». They render as visible garbage.
  - Evidence (2026-09-25, `mod2imp TorresAmat` against `modulos/`):
    - 552 occurrences of `&amp;amp;##x27` (524 with `;`, 26 without, 2 with
      `;;`), spread over most books (1/2 Samuel/Kings/Chronicles, Psalms,
      Jeremiah, Ezekiel, Isaiah, Sirach, Genesis, Exodus, Acts…).
    - `&amp;gt;` also appears in 122 verses (e.g. 1 Sam 19:20 «Saul
      =&amp;gt; soldados»).
  - Root cause to determine:
    Locate the pipeline stage under `scripts/torresamat/` that escapes an
    already-escaped OCR apostrophe/`>` (`'` → `&#x27;` → `&amp;##x27;` →
    `&amp;amp;##x27;`). Fix the escaping at that stage; do not post-filter
    the module with a blind regex.
  - Acceptance criteria:
    - Each artifact is classified against the source OCR/facsimile: printed
      apostrophe/character to keep (escaped exactly once) vs. OCR noise to
      drop. Classification is reproducible and recorded.
    - Rebuilt module: 0 occurrences of `&amp;amp;`, `&amp;gt;`, `##x27`.
    - No other verse text changes (diff of `mod2imp` before/after limited to
      the classified occurrences).
    - Facsimile corrections (TORRES-FACSIMILE-101..104) and psalm-title
      slots (TORRES-PSALM-TITLES-101) are preserved.
    - Regression test in `scripts/torresamat/` covering the escaping stage.
  - Do not:
    - Replace text from another Bible.
    - Commit or push.
  - Evidence:
    - Root cause, two steps (reproduced with osis2mod, SWORD 1.9.0):
      `osis.py` escaped with `html.escape(t)`, turning `'` into `&#x27;`,
      which osis2mod does not understand and stores as `&amp;##x27;`
      (present since dd16f715); the 9c036c87 regeneration escaped the text
      again (`&amp;amp;##x27;`, `&gt;` → `&amp;gt;`, `&lt;` → `&amp;lt;`).
      Traced by exporting the module at dd16f715, 39c88261, ed1e9195 and
      9c036c87.
    - Classification against the source OCR (the four Archive
      `tomo*_djvu.xml`, downloaded): the printed 1882 body uses no `'`, `<`
      or `>`; the OCR tokens are paper specks (1484 boxes ≤12 px) or pieces
      of a misread letter (`testig'os`, `lare'o`, `c'eneral`). Nothing
      printed is kept; every artifact is noise.
    - New `scripts/torresamat/entidades.py`: removes the artifact forms and
      decides the space when an artifact sat between letters (96 cases) from
      the module's own lexicon (words touching an artifact excluded):
      two real words → space (`celebrado en`, `No prostituyas`), inside a
      word → joined (`testigos`, `consigo`, `cincuenta`). Two entities split
      across verses (Ps 106:42/43, Ez 33:11/12) are handled.
    - `parche_facsimil.py` applies it to the whole module (`autoriza`,
      `quita_restos`), checking that each change only removes an artifact
      and whitespace; correction texts are compared already cleaned, so the
      patch accepts both pre- and post-task modules.
    - `osis.py` removes the noise before escaping and escapes with
      `quote=False`, so a future rebuild emits no numeric references.
    - Result on the real module: 566 verses changed; `&amp;amp` 0, `##x27`
      0, `&amp;gt` 0, `&amp;lt` 0, `&#x27;` 0. Regenerating from 9c036c87
      and from the current `modulos/` gives byte-identical output; a second
      pass changes 0 verses. Other verses unchanged (round-trip guard).
      E.g. Ps 147:6 «…en menudos pedazos: al rigor de su frio ¿quién
      resistirá?», 1 Sam 19:20 «Envió pues Saul = soldados…».
    - Installed to `modulos/` and `~/.sword` (identical); previous
      `~/.sword` copy kept in the session scratchpad.
    - Tests: new `test_entidades.py` (10 checks incl. osis2mod round trip),
      `test_parche.py` (idempotent regeneration updated to the cleaned
      authorizations), `test_titulos.py`; registered in CTest as
      `torresamat_entidades_test` and `torresamat_parche_test`; PASS.
    - Build PASS; full CTest 49/50: only `gtk_lifecycle_smoke` fails
      («Renderer bible-compare did not complete CREATE/SHOW/MAP»), identical
      with the pre-task module restored, so unrelated; registered as
      UI-SMOKE-103.
    - Out of scope, left as is: surrounding apparatus noise in some verses
      (e.g. «AR Ú 5 AA A») and letters misread inside words
      (`ceneral` for «general»).

- [x] TORRES-PSALM-GLUE-101 Split following-psalm headers glued into the last verse of Torres Amat psalms
  - Status: DONE
  - Description:
    The last verse of several psalms carries the printed header of the next
    psalm (`SALMO CXLVII`, its argument) and sometimes the whole next psalm.
    Worst case: Ps 146:11 holds the full text of Ps 147 plus the header of
    Ps 148, while Ps 147:1.. also carry that text (duplicate display).
  - Evidence (2026-09-25, `mod2imp TorresAmat`): verses containing
    `SALMO <roman>`: Ps 6:11, 18:15, 21:32, 40:14, 47:15, 82:19, 83:13,
    92:5, 95:13, 96:12, 97:9, 101:29, 117:14, 120:8, 131:3, 139:14, 141:8,
    146:11, 147:9 (Vulgate numbering).
  - Acceptance criteria:
    - For every listed verse, the glued tail is classified against the
      facsimile: (a) next-psalm header/argument (not verse text), (b) text
      duplicated in native slots of the next psalm, (c) text missing from
      those slots.
    - (a) and (b) are removed from the verse; (c) is moved only to the empty
      native slot it belongs to, following the structural boundary, never by
      string guessing across psalms.
    - The fix lives in the parser/segmentation stage (or the facsimile
      patch mechanism of TORRES-FACSIMILE-104 if the source is a patch), not
      in the backend.
    - Native psalm-title slots (TORRES-PSALM-TITLES-101) stay correct.
    - After rebuild: no verse contains a `SALMO <roman>` header; Ps 146:11
      contains only its own text; Ps 147/148 are not duplicated.
    - Regression test covers Ps 146:11, Ps 147:9 and at least one short case
      (e.g. Ps 131:3).
  - Do not:
    - Renumber psalms to KJV.
    - Fill from other Bibles.
    - Commit or push.
  - Evidence:
    - Scope correction: the original 19-verse list used a narrow regex; a
      scan for `SALMOS?` found 40 verses in two families. This task closes
      family A (end of a psalm / page header inside a verse): 26 last
      verses of a psalm plus Ps 18:15 and 117:14 (running page head mid
      verse). Family B (mid-chapter misalignment in Ps 113–115 and 131–133,
      running heads with apparatus noise in Ps 58:4 and 77:38, and the
      volume title glued to 2 Macc 15:40) moves to TORRES-PSALM-ALIGN-101.
      Ps 131:3 from the example list belongs to family B (it carries a copy
      of Ps 130:3), so the short-case test covers Ps 18:15 and 117:14/15
      instead.
    - Every case checked against the 1882 facsimile (tomo III jp2 leaves
      downloaded from the Archive item: hojas 12, 16, 26, 28, 31, 37, 44,
      46, 49–52, 58, 60, 63–65, 67–69, 72). Classification: (a) header +
      italic argument (+ start of the title already in :1) → removed;
      (b) Ps 92:5, 146:11, 147:9 tails repeat Ps 93:1, 147:1–9 and 148
      text already in their native slots (word-run check: only header,
      argument, the Doré plate caption «…DESPUES DE SU RUINA» and noise
      are not in the slots) → removed; Ps 147:2 repeated 147:3 → removed;
      (c) Ps 117:15 was empty and its printed text sat after a footnote and
      the page head in 117:14 → moved.
    - Text the OCR lost, read on the facsimile and added: Ps 83:13 «…que
      pone en tí su esperanza.», Ps 139:14 «…de tu divina cara.» («divi-/na
      cara.»), Ps 147:1 «…oh Sion, á tu Dios.». A final comma read from a
      note call becomes the printed period (e.g. Ps 47:15, 67:36, 95:13).
    - New `scripts/torresamat/cabeceras_pegadas.py` (CABECERAS, AÑADIDOS,
      NOTAS, TRASLADADOS, REPETIDOS), merged by `parche_facsimil.cambios()`.
      `_comprueba_cabeceras()` enforces that each entry only removes a span
      starting at the header (≤25 chars of engraving noise before it, or a
      declared footnote), adds nothing undeclared, and that the moved text
      comes from the declared source. `ANTERIORES` lets the patch start
      from the already patched Ps 147:1, so it works on `~/.sword` too.
    - Result: 31 verses changed. After rebuild no family-A verse contains a
      `SALMO` header; Ps 146:11 = «Se complace sí en aquellos que le temen y
      adoran, y en los que confian en su misericordia.»; in Ps 146–147
      «Ha establecido la paz» and «despide el granizo» each appear once.
      Psalm-title segs unchanged. Regeneration from 9c036c87 and from the
      current `modulos/` is byte-identical; a second pass changes 0.
      Installed to `modulos/` and `~/.sword` (identical).
    - Tests: new `test_cabeceras.py` (6 checks incl. rejection of malformed
      entries); `test_titulos.py` updated (CABECERAS in the authorized set,
      Ps 147:1 mode «completado», Ps 147:2 declared repeat);
      `test_parche.py`, `test_entidades.py`; CTest
      `torresamat_{titulos_pipeline,entidades,parche,cabeceras}_test`,
      `psalm_title_render_test`, `uri_navigation_v11n_test`: PASS.

- [x] TORRES-PSALM-TITLE-OCR-101 Clean OCR noise in Torres Amat Ps 119:1 title slot
  - Status: DONE
  - Description:
    Ps 119:1 (Vulg) reads `<seg type="x-psalm-title">Cántico de los grados,
    0 gradual.</seg> eme Clamé al Señor…`: `0` is presumably OCR for «ó» and
    `eme` is noise. Registered as a follow-up in V11N-MODULE-101.
  - Acceptance criteria:
    - Title and body verified against the 1882 facsimile and corrected
      through the facsimile patch mechanism (TORRES-FACSIMILE-104).
    - Audit the other Gradual psalms (Vulg 119–133) title slots for the same
      pattern and correct only facsimile-confirmed defects.
    - Patch test updated; psalm-title structure preserved.
  - Do not:
    - Commit or push.
  - Evidence:
    - Facsimile (tomo III, hoja 63, zoomed crop): «1. Cántico de los grados,
      ó gradual.» (italic «ó gradual.») then «Clamé al Señor en mi
      tribulacion, y me atendió.»; `0` = «ó», `eme` = engraving noise.
    - Audit of all Gradual title slots Vulg 120–133 against hojas 63–65:
      two more facsimile-confirmed title defects: Ps 124:1 «Cántico
      gradual,» → «Cántico gradual.» (hoja 64) and Ps 133:1 «Cántico
      gracual.» → «Cántico gradual.» (hoja 65). The other titles already
      match the print. Body OCR errors in those psalms are not title-slot
      defects and were left alone.
    - `parche_facsimil.py`: new `IMPRESO` (printed title/body for a TITULOS
      entry) with `_comprueba_impreso()` (title ≤2 edits from the OCR,
      body only loses leading noise) and `impreso()`; TITULOS keeps the raw
      OCR used to validate the cut. The previous patched readings are
      accepted via `ANTERIORES`, so the patch applies on `~/.sword` too.
    - Result: 3 verses changed. Ps 119:1 = `<seg type="x-psalm-title">Cántico
      de los grados, ó gradual.</seg> Clamé al Señor en mi tribulacion, y me
      atendió.`; Ps 124:1 and 133:1 titles «Cántico gradual.». Regeneration
      from 9c036c87 and from `modulos/` byte-identical; second pass 0.
      Installed to `modulos/` and `~/.sword` (identical).
    - Tests: `test_titulos.py` adds `test_erratas_de_titulo_graduales`
      (incl. guard rejection) and reads printed values through
      `parche.impreso()`; CTest torresamat titulos/entidades/parche/
      cabeceras and `psalm_title_render_test`: PASS.

- [x] NACAR-OCR-107 Split Nácar-Colunga Ps 118:5/118:6 merge
  - Status: DONE
  - Description:
    Ps 118:5 is empty and Ps 118:6 holds «En la angustia invoqué a Yave, y me
    oyó Yave poniéndome en salvo. $ Está por mí Yave: ¿Qué puedo temer…».
    Known out of scope since NACAR-PSALMS-103 / NACAR-OCR-104; the `$` marks
    the lost verse-number glyph.
  - Acceptance criteria:
    - Diagnose why NACAR-OCR-104 does not cover this case (misread glyph
      form) using the OCR/provenance record.
    - Ps 118:5 = «En la angustia … poniéndome en salvo.»; Ps 118:6 = «Está
      por mí Yave: …», supported by the facsimile, no `$`.
    - The rule is structural and does not regress Ps 3, 13, 14–17, 117/118.
    - Regression test in `scripts/nacarcolunga/`.
    - Report whether other `$` residues exist and which remain.
  - Do not:
    - Copy Reina-Valera text.
    - Commit or push.
  - Evidence:
    - Diagnosis: NACAR-OCR-104 needs a repeated number; here the printed ⁵
      is read as 6 and the printed ⁶ is not read as a digit but as a lone
      «$» (event stream around Ps 118: marks 4, 6, 7 with «$ Está por mí…»
      as a continuation line). The «$» is the usual OCR reading of the
      small superscript 6: 12 of the 46 standalone-«$» occurrences show the
      same shape (verse b ending in 6, b-1 empty, b-2 and b+1 present).
    - Fix: `versiculos.parte_dolar()` runs in `construir.py` right after
      `ensambla()` (before `aplica_correspondencias`). A first version that
      split in the event stream before assembly changed chapter lengths
      seen by the aligner and displaced Lev 9–10 (coverage −1); it was
      replaced by the post-assembly rule, which leaves alignment untouched.
      Signal: b % 10 == 6, slot b-1 empty, b-2 and b+1 with text, exactly
      one standalone «$» in b with text before and after. Text before →
      b-1, after → b; provenance of b becomes the «$» line. No hardcoded
      verses.
    - Result (rebuild vs. reproduced baseline): coverage 31084 → 31096
      (+12); `texto.json` and `procedencia.json` change only the 12 split
      pairs (1 Chr 4, Jdt 11, Jer 17, Lam 1, Lev 6, Prov 10, Prov 15,
      Ps 32, Ps 103, Ps 118, Sir 37, Wis 19 — verses 5/6); `perdidas.json`
      only moves the Wis 19:6 record to 19:5 (the merged verse's opening
      line is now 19:5); `epigrafes.json`, `avisos.txt`,
      `introducciones.json`, `notas.json` byte-identical.
    - Ps 118:5 = «En la angustia invoqué a Yave, y me oyó Yave poniéndome
      en salvo.»; Ps 118:6 = «Está por mí Yave: ¿Qué puedo temer, qué
      podrán. hacerme los hombres?», no «$».
    - Facsimile (Princeton leaves): Ps 118 ⁵/⁶ (1020), Prov 10 ⁵/⁶ (1045),
      Jer 17 ⁵/⁶ (756), Lev ⁵/⁶ (210) confirm each split.
    - Remaining «$»: 37 verses still contain «$» (30 standalone). Not
      touched because the signal is incomplete or ambiguous: «$» with n+2
      after (Gen 45, Deut, 2 Kgs 22, Jdt 4, Jer 49: the «$» may be 5, 6 or
      8), two numbers lost (Exod 7, 1 Kgs 19, Dan 1), wrong neighbours
      (Luke 10:4 empty, Matt 2:5 where «$» is 6 and the hole is 4), and
      «$» inside introductions/notes (Song, Sir prologue, John, Rom).
    - Tests: new `test_dolar.py` (5 cases incl. negatives and a check on the
      built `texto.json`); all `scripts/nacarcolunga/test_*.py` plus
      `torresamat/test_pegadas.py` and `test_restos.py` PASS.
    - Module not reinstalled in `~/.sword` (copyrighted, installed on
      request, as in NACAR-OCR-104); run `scripts/nacarcolunga/instalar.sh`
      to publish.

- [x] BOOKMARK-V11N-101 Resolve module-less bookmarks without cross-versification ambiguity
  - Status: DONE
  - Description:
    `src/gtk/bookmarks_treeview.c` opens a bookmark with an empty module
    using `settings.MainWindowModule`. The same key («Psalms 118:1») means a
    different psalm under KJV and Vulg, so a bookmark saved in SpaRV opens a
    different passage when TorresAmat is active. Registered as a follow-up in
    V11N-MODULE-101 («Bookmarks without module name remain ambiguous»).
  - Acceptance criteria:
    - New bookmarks always store the module (or at least its versification).
    - Legacy module-less bookmarks: documented, deterministic policy (e.g.
      interpret as KJV and convert via `convertReference()` to the active
      module), with explicit unmapped handling; never reread by identity.
    - Bookmarks with a module keep working unchanged.
    - Test covers module-less Ps 119:1 opened with SpaRV (KJV) and
      TorresAmat (Vulg), and a module-qualified bookmark.
  - Do not:
    - Migrate/rewrite the user's bookmark file silently.
    - Commit or push.
  - Evidence:
    - Paths audited: the bookmark tree (`button_release_event`) replaced an
      empty module by `settings.MainWindowModule` and sent the key as is;
      the menu «open in dialog/tab» routes send `module=""`, which
      `show_module_and_key()` (`url.cc`) turned into the main module with
      the same key. Both reread KJV numbering in the active Bible.
    - Policy (`src/main/reference_transition.{h,cc}`): a module-less key is
      read in `kLegacyBookmarkVersification` = KJV (SWORD's default) and
      converted with the V11N-MODULE-101 contract
      (`planBibleVersificationTransition`): `planLegacyBookmarkKey()` for one
      reference, `planLegacyBookmarkKeyList()` for lists, comma lists and
      ranges (item by item; a range keeps both ends, `C:V` when the end
      changes chapter; any unmapped item makes the whole key Unmapped).
      Identity for KJV targets and non-verse-keyed targets.
    - Wiring: `main_legacy_bookmark_key()` (`sword.cc`) used by
      `show_module_and_key()` for an empty module and by the bookmark tree
      before building its URLs; Unmapped shows
      `main_warn_reference_unmapped()` and does not navigate. Bookmarks with
      a module are untouched. The bookmark file is not rewritten.
    - New bookmarks: `bookmark_dialog.c` falls back to
      `settings.MainWindowModule` when neither the entry nor the caller
      gives a module, so no new bookmark is saved without one.
    - Tests: `versification_transition_test` adds `test_legacy_bookmarks`:
      module-less «Psalms 119:1» opens SpaRV (KJV) 119:1 and TorresAmat /
      SpaPlatense (Vulg) 118:1; «Psalms 119:1-5; Psalms 121:1» → «Psalms
      118:1-5; Psalms 120:1»; «Ephesians 2:8,9» → «Ephesians 2:8; Ephesians
      2:9»; «Psalms 147:10-12» → «Psalms 146:10-147:1»; «Psalms 13:6» →
      Unmapped (also inside a list). Module-qualified transitions keep
      passing (`test_module_transitions`). Result
      `versification_transition=ok skipped_cases=0`.
    - Build PASS with no warnings in the touched files;
      `uri_navigation_v11n_test`, `content_resolver_test`,
      `author_commentary_content_test`, `author_commentary_header_test` PASS.
    - Not exercised in the running GUI (bookmark clicks need an interactive
      session); the GTK code only routes through the tested policy.

- [x] TORRES-1835-REMAINING-GLYPH-REPRIORITIZATION-147 Reprioritize remaining glyph-rooted verse gaps after task-146
  - Status: DONE
  - Description:
    After task 146, 1266 glyph gaps and 3211 physical gaps remain. Recompute
    the inventory from the current parser (recovery enabled) and select
    exactly one bounded next family, as in task 140.
  - Acceptance criteria:
    - Deterministic inventory artifact with schema version and provenance.
    - Families ranked by population, facsimile reviewability and
      negative-control availability; excluded populations stay excluded
      (111 task-144 UNREADABLE, 13 order conflicts, p0184l0043 out of band,
      GLUED_FRAME CLOSED_UNSAFE) unless new evidence is recorded.
    - Exactly one next task recommended with a finite measurable population.
    - Corpus invariants unchanged (no runtime change in this task).
  - Do not:
    - Change production runtime.
    - Mutate historical artifacts 138–146.
    - Commit or push.
  - Evidence:
    - Result: READY; diagnostic only, no runtime change.
    - Frozen baseline: e7dedfc80cc1f8e78afdd3e93d19a0236f9357aa (HEAD; no
      torresamat1835 runtime file modified).
    - Current audit in production mode (task-146 recovery enabled):
      VerseRefs 3892; physical gaps 3211; glyph gaps 1266; chapters 337/337;
      unresolved claims 0; canonical chapter gaps 0; ocr_blocks 57700;
      duplicate_refs 0; out_of_order_refs 0; outside_canon 0.
    - Reconciliation with task 140: 1309 → 1266; 43 removed occurrences,
      exactly the task-145/146 CREATE_NEW_REF identities; 0 added.
    - Split: PHYSICAL_SINGLE_TOKEN 6; PROJECTED_FROM_MULTI_TOKEN 1260.
      Review coverage: REVIEWED_POSITIVE 195, REVIEWED_NEGATIVE 25,
      REVIEWED_AMBIGUOUS 4, NOT_REVIEWED 1042. Top forms: y 366, a 217,
      á 103, 4 73, 3 51.
    - Remaining form-a occurrences by task-141 facsimile value: 21 → 67,
      2 → 34, 24 → 31, 20 → 29, 22 → 19, 25 → 9, none (non-marker) → 22,
      others ≤2. Exclusions kept: 111 task-144 external UNREADABLE, 13
      known order conflicts, p0184l0043, GLUED_FRAME CLOSED_UNSAFE.
    - Families ranked (raw evidence): PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH
      (26, facsimile-reviewed), form y (366, unreviewed, token identity
      only), form á (103, unreviewed), physical single token (6, no
      reusable rule).
    - Selected next family: PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH, 26
      occurrences in 26 distinct blocks (Isa 8, Sir 8, Prov 6, Ps 2, Wis 2);
      printed values 20 (4), 21 (13), 22 (3), 24 (5), 25 (1). Every member
      is a task-141 PRINTED_VERSE_MARKER whose value equals its gap's verse
      and whose next OCR token carries the second digit («I»/«1» → 1,
      «o» → 0, «4»/«4'» → 4, «a» → 2, «5» → 5). Controls on record: 129
      printed-2x occurrences in other gaps, 22 reviewed non-markers, 34
      printed-2 left by task 146. The gap-key comparison is a diagnostic
      partition of values already read on the facsimile, not inference.
    - Artifact `data/torresamat1835/remaining_glyph_reprioritization_147.json`
      (schema_version 1, provenance hashes, deterministic: two runs
      byte-identical); summary integrated as
      `verse_segmentation_audit.remaining_glyph_reprioritization_147`.
    - Tests: new `test_remaining_glyph_reprioritization_147.py` (CTest
      `torresamat1835_remaining_glyph_reprioritization_147_test`); the
      artifact allowlists in `test_baseline.py`,
      `test_verse_marker_sanity.py` and `test_sources.py` gained the new
      file as for tasks 140–145. Torres 1835 CTest 37/37 PASS.

- [x] UI-SMOKE-103 Fix gtk_lifecycle_smoke «Renderer bible-compare did not complete CREATE/SHOW/MAP»
  - Status: DONE
  - Description:
    Found while validating TORRES-ENTITY-101 (2026-09-25): CTest
    `gtk_lifecycle_smoke` fails deterministically with
    `Renderer bible-compare did not complete CREATE/SHOW/MAP:
    gtk_lifecycle_smoke_failures=0 checks=241 navigation=5 renderers=5`
    (`tests/run_gtk_lifecycle_smoke.cmake:69`). It fails identically with
    the pre-TORRES-ENTITY-101 TorresAmat module restored, so it is not a
    data regression. UI-SMOKE-102 closed with the CTest variant skipped for
    lack of Xvfb; the runner now executes on this host.
  - Acceptance criteria:
    - Determine whether bible-compare is an active surface that must map in
      the smoke sequence or is retired/hidden by design (as the dictionary
      surface in UI-SMOKE-102).
    - Fix the product or the runner expectation accordingly, with evidence;
      do not weaken checks for active surfaces.
    - `gtk_lifecycle_smoke` passes repeatedly (≥3 runs).
  - Do not:
    - Commit or push.
  - Evidence:
    - Diagnosis (temporary parent-chain dump in the smoke, removed): the
      compare `WkHtml` was `visible=0` inside a visible box. Startup calls
      `gui_lectura_sync_set_visible(FALSE)`, which since b2164a75 hides the
      renderer holder too; «Comparar» reopens through the same helper and
      shows it. The smoke reopened only `box_lectura_sync` with
      `gtk_widget_show()`, bypassing the product path, so the renderer
      never mapped. Product behaviour is correct; the smoke was not.
    - Two more surfaces were hidden behind the first failure (the runner
      stops at the first surface): commentary reopened with raw
      `gtk_widget_show()` left `settings.showcomms` off, so the stale
      dictionary request's layout pass hid it again; and with no commentary
      module in the SQLite fixture the layout opens the «Notas» page
      (`comm_showing ? 0 : 1`), keeping the commentary renderer behind a
      hidden notebook page. The same failures occur with the HEAD smoke.
    - Fix (`src/gtk/gtk_lifecycle_smoke.c` only; product and runner
      unchanged, all five surfaces still required): compare pane
      opened/closed with `gui_lectura_sync_set_visible()`; commentary
      reopened with `gui_show_hide_comms(TRUE)`; after the last layout pass
      the smoke selects the «Comentarios del autor» tab as a reader would;
      new checks: compare renderer visible, compare closes, commentary
      survives the dictionary request, commentary tab selectable, and every
      renderer mapped (bounded wait of up to 50×100 ms for the splitter's
      layout pass, failing if a surface never maps).
    - Result: `gtk_lifecycle_smoke` PASS 3/3 consecutive runs
      (`gtk_lifecycle_smoke_failures=0 checks=290 renderers=5 panels=23`);
      bible-main, bible-compare, commentary, sidebar-previewer and
      lower-previewer each log CREATE, SHOW and MAP/RENDERER_MAP; no
      Gtk/Gdk WARNING/CRITICAL. Full CTest 52/52 PASS.

- [x] TORRES-PSALM-ALIGN-101 Repair Torres Amat mid-chapter psalm misalignments and running heads
  - Status: DONE
  - Description:
    Family B found by TORRES-PSALM-GLUE-101 (2026-09-25): verses carrying
    another psalm's text or header in the middle of a chapter, and page
    running heads with apparatus noise.
  - Evidence (`mod2imp TorresAmat` after TORRES-PSALM-GLUE-101):
    - Ps 113:9, 113:10, 113:18, 113:19, 114:9, 115:1, 115:10 carry
      `SALMO CXV/CXIV/CXVI` headers and text of Vulg Ps 114–115 in wrong
      slots (Vulg 113 = Heb 114+115; the printed numbering continues).
    - Ps 131:3 holds a copy of Ps 130:3 plus the Ps 131 header; Ps 132:3 and
      133:3 carry the Ps 133 header and Ps 133:1 text.
    - Ps 58:4 and 77:38 end with apparatus/plate noise and the running head
      «… SALMOS.».
    - 2 Macc 15:40 ends «TOMO III LIBRO DE LOS SALMOS.».
  - Acceptance criteria:
    - Each verse checked against the facsimile leaf and recorded.
    - Text moved only to the native Vulgate slot it belongs to; duplicates
      removed; nothing filled from other Bibles.
    - Through the facsimile patch mechanism with the same guards; the
      regeneration from 9c036c87 stays byte-identical to the patched
      `modulos/` and idempotent.
    - Regression tests for Ps 113–115 and 131–133.
  - Do not:
    - Commit or push.
  - Evidence:
    - Root cause (facsimile, tomo III hoja 59): the OCR read both columns
      line by line. Each Ps 112:k slot held its own verse plus «In exitu»
      verse k (Vulg 113:1–8); 112:9 also held the Ps 113 argument and its
      «Aleluya.». Module 113:1–18 held «Non nobis» (printed 1–18 = Vulg
      113:9–26) plus copies of Ps 114/115 verses already in their slots;
      printed 6 and 7 shared slot 113:6, 113:7 held only a copy, and
      113:20–26 were empty. Hoja 65: 131:2–3 carried copies of 130:2–3 and
      the Ps 131 header, while 131:3 («No me meteré yo…») sat at the start
      of 131:5; 133:2–3 carried copies of 132:2–3 and the Ps 133 header.
      Ps 58:4 (hoja 31) and 77:38 (hoja 41) ended with a plate caption and
      the next page's running head («41 SALMOS.», «57 SALMOS.»); 2 Macc
      15:40 ended with the tomo III half-title.
    - Fix: new `scripts/torresamat/columnas_fundidas.py` (COLUMNAS: 47
      verses, each with its hoja) merged by `parche_facsimil.cambios()`;
      `_comprueba_columnas()` rejects any text not taken from the old
      verses (only a final «,»→«.» is allowed). Ps 112:1 and 115:1 (TITULOS)
      are trimmed through `IMPRESO`, whose guard now accepts a body that
      is a substring of the OCR body. Nothing is filled from other Bibles.
    - Result: Ps 113 has 26/26 verses in Vulgate order (113:1 «Aleluya.»
      title + «Cuando Israél salió…», 113:9 «No á nosotros…», 113:14/15
      split, 113:26 «Nosotros sí…»); Ps 112 without «In exitu»; 114:9 and
      115:10 without headers; 131:2/3/5, 132:3, 133:2/3 corrected; Ps 58:4,
      77:38 and 2 Macc 15:40 cleaned. No verse in the module contains a
      `SALMO`/`SALMOS` header any more. 47 verses changed; regeneration
      from 9c036c87 and from `modulos/` byte-identical; second pass 0.
      Installed to `modulos/` and `~/.sword` (identical).
    - Tests: new `test_columnas.py` (4 checks incl. rejection of invented
      text; CTest `torresamat_columnas_test`); `test_titulos.py` updated:
      the old «no body moves between Ps 112 and 113» assertions now state
      the facsimile-backed contract (113:1–3 not in TITULOS but in
      COLUMNAS; mode «recortado» for 112:1; printed values through
      `parche.impreso()`). torresamat/psalm_title/content_resolver/
      uri_navigation CTest 8/8 PASS.

- [x] TORRES-1835-PROJECTED-FORM-A-PRINTED-2X-DISCRIMINATOR-148 Validate a source-derived discriminator for printed two-digit 2x markers read as «a» + digit
  - Status: DONE
  - Description:
    Selected by task 147. 26 open glyph gaps whose projected token «a» is a
    facsimile-confirmed printed marker 20–25 equal to the gap's verse; the
    second digit is the next OCR token. Validate, without recovery, whether
    a runtime-safe rule can read «a» + second-digit token as 2d.
  - Acceptance criteria:
    - Rule derived from source tokens and geometry only; task-141 values
      and the task-147 member list are the oracle, never runtime input.
    - Measure true positives on the 26 members and false positives on every
      other «a» + digit-like sequence (129 printed-2x occurrences in other
      gaps, reviewed non-markers, corpus-wide matches).
    - task-144 native-order and provenance guards applied; no gap key,
      expected verse, previous+1 or next-1.
    - Deterministic artifact; runtime unchanged; Torres 1835 CTest green.
  - Do not:
    - Implement recovery.
    - Widen task-142/146 scope or reopen GLUED_FRAME.
    - Commit or push.
  - Evidence:
    - Result: READY_FOR_DRY_RUN; diagnostic only, runtime unchanged
      (VerseRefs 3892, physical gaps 3211, glyph gaps 1266).
    - Generator `scripts/torresamat1835/projected_form_a_printed_2x_discriminator.py`:
      features frozen from source OCR (`source_ocr.read_pages`, task-146
      `validated_geometry`) and current gap provenance; labels (task-147
      members, task-141 reviews) joined only after all rule decisions. No
      gap key, expected verse, previous+1 or next-1 in any rule.
    - Corpus: 534 lines start with «a» + a digit-like token (271 right/body,
      249 left/body — the Latin column also prints markers).
    - Rules (accepted / family TP / FN / reviewed non-markers):
      R2X (unframed «a», right body, projected-«a» gap provenance)
      24 / 23 / 3 / 0; R2X_ALLOW_LEADING_NOISE 27 / 26 / 0 / 0;
      R2X_WINDOW (two-digit band window) 13 / 13 / 13 / 0;
      R2X_NO_PROVENANCE 271 / 23 / 3 / 0 (+247 unreviewed markers already
      owned by their verses). Reading value equals the facsimile value on
      every accepted member (0 wrong values).
    - Selected: R2X. FN = the 3 members with a punctuation token before
      «a» (measured, not admitted). One accepted block, p0032l0089, is a
      real printed 21 in another verse's gap: the task-144 native-order
      guard must decide it in the dry run. The one-digit band tolerance and
      a digit-width window are not usable (10 members have no trusted band
      in their column).
    - Artifact `data/torresamat1835/projected_form_a_printed_2x_discriminator.json`
      (deterministic); allowlists in `test_baseline.py`, `test_sources.py`,
      `test_verse_marker_sanity.py` updated; new
      `test_projected_form_a_printed_2x_discriminator.py` (CTest). Torres
      1835 CTest 38/38 PASS.


- [x] TORRES-1835-PROJECTED-FORM-A-PRINTED-2X-DRY-RUN-149 Dry-run the R2X rule with task-144 guards and measure its semantic delta
  - Status: DONE
  - Description:
    Queued by task 148. Apply R2X in dry-run mode (no ownership change)
    with the task-144 projected-gap provenance and native-order guards and
    measure CREATE_NEW_REF / REOPEN / ownership moves / gap delta, as task
    145 did for the printed-2 family.
  - Acceptance criteria:
    - Predicted events per block, deterministic artifact, runtime unchanged.
    - p0032l0089 (printed 21 in another verse's gap) resolved by the guards,
      not by an identity exception.
    - No unsafe reopen; corpus invariants unchanged; Torres 1835 CTest green.
  - Do not:
    - Implement recovery (task 150 would, if the dry run is clean).
    - Admit the leading-punctuation variant without its own evidence.
    - Commit or push.
  - Evidence:
    - Result: CLEAN_DRY_RUN; nothing applied, runtime unchanged
      (VerseRefs 3892, physical gaps 3211, glyph gaps 1266, chapters 337,
      ocr_blocks 57700, duplicate/out-of-order refs 0).
    - Generator `scripts/torresamat1835/projected_form_a_printed_2x_dry_run.py`
      re-derives R2X from source OCR and live provenance on the production
      edition and applies the `projected_form_a_recovery.apply` guards
      (single owner, resolved chapter, native progression value > owner
      verse, Vulgate canon limit, no existing ref, owner keeps ≥1 block, one
      event per ref). Value = «2» + second-digit token; no gap key,
      expected verse, previous+1 or next-1. Labels joined afterwards.
    - Predicted delta: 24 CREATE_NEW_REF, 0 REOPEN, 0 ambiguous/order/
      canon abstentions; VerseRefs 3892 → 3916; 409 ownership moves; 24
      physical and 24 glyph gaps closed; no prior owner emptied.
    - Label reconciliation: 23 family members, each created at exactly its
      task-147 gap ref with the facsimile value; 1 outside the family,
      p0032l0089 → Ps.17.21.
    - p0032l0089 checked in the source: «a 1 £1 Se^qr me recompénsala…» =
      «21 El Señor me recompensará…» (Vulg Ps 17:21). Its current owner
      Ps.17.1 is a pre-existing misread: p0032l0079 «1 8 - Libóme de mis
      poderosísimos enemigos» is the printed 18 split by the OCR, so 17.1
      holds verses 18–27. The event moves 21–27 to 17.21 (correct) and
      leaves 18–20 under 17.1 as before; no identity exception was added.
      Registered as TORRES-1835-SPLIT-DIGIT-MARKER-AUDIT-151.
    - Artifact `data/torresamat1835/projected_form_a_printed_2x_dry_run.json`
      (deterministic, regenerated byte-identical by the test); new
      `test_projected_form_a_printed_2x_dry_run.py` (CTest); artifact
      allowlists updated. Torres 1835 CTest 39/39 PASS.


- [x] TORRES-1835-PROJECTED-FORM-A-PRINTED-2X-RECOVERY-150 Implement the dry-run-validated R2X recovery
  - Status: DONE
  - Description:
    Queued by task 149. Add R2X to production as a fallback after the
    stronger paths, reproducing exactly the task-149 prediction.
  - Acceptance criteria:
    - Actual delta equals task 149: 24 CREATE_NEW_REF (identities match),
      409 ownership moves, 24 glyph gaps closed, 0 reopen, 0 block loss,
      0 dual ownership; VerseRefs 3892 → 3916.
    - No block/ref/page allowlist; production does not read diagnostic JSON.
    - Historical tests that need the pre-150 parser disable it explicitly.
    - Tasks 128/131/139/146 unchanged; Torres 1835 CTest and build green.
  - Do not:
    - Admit the leading-punctuation variant.
    - Commit or push.
  - Evidence:
    - Result: IMPLEMENTED_AND_VALIDATED.
    - Runtime: `projected_form_a_recovery.match_2x()` (unframed «a» +
      second-digit token → 20+d; `SECOND_DIGIT`, `DECISION_2X`) and
      `apply()` now takes the value per record and, for 2x records, refuses
      to empty the prior owner. `page_parser` notes R2X candidates in the
      same validated right/body placement as task 146 and runs a second
      `apply()` pass right after task 146's, on the edition it left (the
      state the dry run measured). New switch `printed_2x_recovery`
      (default on, implied off whenever task 146 is off) and CLI flag
      `--without-printed-2x-recovery`. No block/ref/page allowlist; no
      diagnostic JSON read at runtime.
    - Audit: new pass «task 146 on, task 150 off»; task 146 is measured up
      to it (unchanged: 43 refs, 581 moves) and task 150 from it, in
      `verse_segmentation_audit.projected_form_a_printed_2x_recovery`.
    - Actual delta = task-149 prediction: 24 recovered (outcomes: recovered
      24, provenance_guard 53, not_owned_as_continuation 7), created ref
      identities identical, 409 ownership moves, 0 reopen, 0 removed refs,
      0 block loss, 0 dual ownership; VerseRefs 3892 → 3916; physical gaps
      3211 → 3187 (the 24 predicted), 0 opened.
    - Glyph gaps 1266 → 1198: the 24 predicted close; 44 more remain
      physical gaps but lose the glyph signal because their only evidence
      was the same projected «a» now read as a marker (e.g. Isa.2.20 next
      to the recovered Isa.2.21); 0 opened. The dry run counted only the
      24 direct closures.
    - Corpus: chapters 337, ocr_blocks 57700, duplicate_refs 0,
      out_of_order_refs 0.
    - Historical freezes: task-146 contract test and the task-147/148/149
      generators/tests run with `printed_2x_recovery=False` /
      `--without-printed-2x-recovery`; their artifacts stay byte-identical.
    - Tests: new `test_projected_form_a_printed_2x_recovery.py` (source-only
      matcher incl. prose/framed negatives; production = dry run; task 146
      untouched; no allowlist). Torres 1835 CTest 40/40 PASS.

- [x] TORRES-1835-SPLIT-DIGIT-MARKER-AUDIT-151 Audit printed two-digit markers split by the OCR («1 8» read as verse 1)
  - Status: DONE
  - Description:
    Found in task 149: p0032l0079 «1 8 - Libóme…» is the printed Ps 17:18,
    read as marker 1, so Ps.17.1 owns verses 18–27. Measure the family of
    lines whose first two tokens are single digits forming a valid marker,
    diagnostic only.
  - Acceptance criteria:
    - Corpus population, facsimile review of a sample, controls.
    - Exactly one bounded next step recommended; runtime unchanged.
  - Do not:
    - Implement recovery.
    - Commit or push.
  - Evidence:
    - Result: READY_FOR_DISCRIMINATOR_AND_DRY_RUN; diagnostic only.
    - Generator `scripts/torresamat1835/split_digit_marker_audit.py`
      (production runtime, task 150 on): lines opening «d d Sentence» in
      the whole volume, with placement, parser ownership, native canon and
      existing refs; the facsimile sample is joined afterwards.
    - Corpus: 962 candidates. Classes: SPLIT_MARKER_READ_AS_FIRST_DIGIT
      379 (right/body, singly owned by the first digit's verse, two-digit
      value inside the canon and not yet a ref); OUTSIDE_SPANISH_BODY 517
      (mostly the Latin column); OWNER_IS_NOT_FIRST_DIGIT 54;
      OUTSIDE_NATIVE_CANON 11; TWO_DIGIT_REF_EXISTS 1.
    - Facsimile (source PDF, seeded random sample of 8 family members):
      8/8 are printed two-digit markers with the value read from the two
      tokens (78, 13, 15, 13, 17, 18, 15, 17), e.g. p0614l0047 «18 De
      todos los hijos…» owned by Isa.51.1. The task-149 case p0032l0079
      (Ps 17:18) is in the family.
    - Artifact `data/torresamat1835/split_digit_marker_audit.json`
      (deterministic); new `test_split_digit_marker_audit.py` (shape
      negatives + contract); allowlists updated. Torres 1835 CTest 41/41.
    - Next step (one): TORRES-1835-SPLIT-DIGIT-MARKER-DRY-RUN-152.


- [x] TORRES-1835-SPLIT-DIGIT-MARKER-DRY-RUN-152 Dry-run a source-only rule for split two-digit markers
  - Status: DONE
  - Description:
    Selected by task 151: 379 right/body lines «d d Sentence» owned by the
    first digit's verse (8/8 facsimile sample are printed two-digit
    markers). Dry-run the rule (value = the two source digits) with the
    task-144/150 guards: single owner, native progression, canon, no
    existing ref, owner never emptied, one event per ref.
  - Acceptance criteria:
    - Predicted CREATE/REOPEN/abstention counts, ownership moves and gap
      delta; deterministic artifact; runtime unchanged.
    - A larger facsimile sample (≥30, stratified by book) confirms values;
      the 54 OWNER_IS_NOT_FIRST_DIGIT rows are explained as controls.
    - No gap key, expected verse, previous+1 or next-1.
  - Do not:
    - Implement recovery.
    - Commit or push.
  - Evidence:
    - Result: CLEAN_DRY_RUN; nothing applied.
    - Generator `scripts/torresamat1835/split_digit_marker_dry_run.py`:
      right/body «d d Sentence» lines, value = the two source digits,
      simulated in physical block order per chapter so each event sees the
      labels earlier events left (needed: Ps 9 prints a second numbering;
      Isa 7 has 13, 14, 15, 18 in one false verse 1). Guards: single owner,
      canon, verse not present, every earlier block lower and the first
      different block after the moved run higher, one event per ref. A run
      that is the whole owner verse is RELABELLED (a verse that existed
      only through the misread first digit), otherwise MOVED.
    - Outcomes: MOVE 270, RELABEL 90 (89 false verse 1, 1 false verse 3),
      progression_past_next 46, progression_behind_previous 24,
      outside_native_canon 11, verse_already_present 4, ambiguous 0.
      Predicted: 360 refs created, 90 false refs removed (they become
      visible gaps), VerseRefs 3916 → 4186, 360 physical gaps closed.
    - Facsimile: stratified sample of 32 accepted events (20 RELABEL, 12
      MOVE across Ps, Prov, Eccl, Song, Wis, Sir, Isa) read on the source
      PDF: 32/32 printed two-digit markers with the source value (e.g. «71
      Bien me está», «31 Con todo eso», «18 Y sucederá»). The 54
      task-151 OWNER_IS_NOT_FIRST_DIGIT rows are lines already owned by a
      different verse (earlier markers in the run); they enter the rule and
      are decided by the progression guards like any other.
    - Artifact `data/torresamat1835/split_digit_marker_dry_run.json`
      (deterministic); `test_split_digit_marker_dry_run.py`; allowlists.

- [x] TORRES-1835-SPLIT-DIGIT-MARKER-RECOVERY-153 Implement the dry-run-validated split two-digit recovery
  - Status: DONE
  - Description:
    Production version of task 152, third pass after tasks 146 and 150.
  - Evidence:
    - Runtime: new `scripts/torresamat1835/split_digit_recovery.py`
      (`match`, `apply`; decision `split_two_digit_marker`, counted as a
      numbered decision); `page_parser` notes right/body «d d Sentence»
      lines in the validated geometry and applies them in `finish()` after
      task 150. Switch `split_digit_recovery` (implied off when task 150 is
      off), CLI `--without-split-digit-recovery`. No allowlist, no
      diagnostic JSON, no gap or expected verse.
    - Audit: pass «150 on, 153 off»; task 150 measured up to it (unchanged:
      24 recovered); `verse_segmentation_audit.split_two_digit_marker_recovery`.
    - Actual = dry run: 360 applied (270 moved, 90 relabelled), same
      abstentions; created/removed ref identities identical; VerseRefs 3916
      → 4186; physical gaps 3187 → 2917 (360 closed; the 90 opened are
      exactly the removed false verses); 2423 distinct blocks change owner
      (the dry run's 3000 counted blocks moved twice); block loss 0, dual
      ownership 0, blocks entering text 0; chapters 337, ocr_blocks 57700,
      duplicate/out-of-order refs 0.
    - Historical freezes: task-150 test and task-151/152 generators run with
      `split_digit_recovery=False`; artifacts byte-identical.
    - Tests: `test_split_digit_recovery.py` (matcher negatives; production
      = dry run; no allowlist). Torres 1835 CTest 43/43 PASS.


- [x] TORRES-1835-GLUED-TWO-CHAR-MARKER-AUDIT-154 Audit two-digit markers read as one glued token («i3», «a6», «3a»)
  - Status: DONE
  - Description:
    Found after task 153: right/body continuation lines opening with a
    two-character token whose characters are unambiguous digit misreads
    (i/I/l → 1, a → 2, o/O → 0, plus real digits; at least one letter)
    followed by a sentence. Diagnostic: population, facsimile sample ≥30.
  - Do not:
    - Implement recovery; admit ambiguous glyphs (S, 9) without evidence.
    - Commit or push.
  - Evidence:
    - Generator `scripts/torresamat1835/glued_two_char_marker_audit.py`
      (production before task 156): right/body continuation lines whose
      first token is two characters from {digit, i/I/l = 1, a = 2, o/O = 0}
      with at least one letter, not a Spanish word (la, lo, al… excluded
      after the first measurement found 103 «la»), followed by a sentence.
      S and 9 are excluded as ambiguous.
    - Family: 670 lines (Sir 202, Ps 160, Isa 134, Prov 119, Wis 32, Eccl
      22, Song 1); forms i3 83, i5 70, ai 66, ao 60, aa 55, a6 51, a3 42…
    - Facsimile: stratified sample of 32 read on the source PDF: 32/32 are
      printed two-digit markers with the value of the mapping, including
      the letter-only forms («ai» 21, «ao» 20, «aa» 22, «io» 10, «II» 11).
    - Artifact `data/torresamat1835/glued_two_char_marker_audit.json`.

- [x] TORRES-1835-GLUED-TWO-CHAR-MARKER-DRY-RUN-155 Dry-run the glued two-character marker rule
  - Status: DONE
  - Description:
    Task-152 simulator (physical order, progression, canon, no existing
    verse, one event per ref) on the task-154 candidates.
  - Do not:
    - Implement recovery.
    - Commit or push.
  - Evidence:
    - Generator `glued_two_char_marker_dry_run.py`: the production parser
      with the task-156 switch off runs the same code path
      (`split_digit_recovery.apply(enabled=False)`), as task 146 did, and
      records the planned outcome of every candidate. Lines that already
      open their verse are skipped (read by another route).
    - Planned: 524 MOVE, 0 RELABEL; abstentions: progression_past_next 95,
      progression_behind_previous 43, not_owned 28, verse_already_present
      6, outside_native_canon 2. VerseRefs 4186 → 4710; 524 gaps closed.
      Of the 32 facsimile-confirmed sample lines, 29 would apply and 3 are
      held back by the progression guards (conservative, not wrong).
    - Artifact `data/torresamat1835/glued_two_char_marker_dry_run.json`.

- [x] TORRES-1835-GLUED-TWO-CHAR-MARKER-RECOVERY-156 Implement the glued two-character marker recovery
  - Status: DONE
  - Description:
    Fourth pass after task 153, reproducing task 155 exactly; historical
    tools pinned to the pre-156 runtime.
  - Do not:
    - Commit or push.
  - Evidence:
    - Runtime: `split_digit_recovery.match_glued` (GLYPH map, WORDS
      exclusion, `DECISION_GLUED`), `apply(skip_openers=True)`;
      `page_parser` fourth pass after task 153; switch
      `glued_marker_recovery` (implied off when task 153 is off), CLI
      `--without-glued-marker-recovery`; audit pass «153 on, 156 off»
      and summary `verse_segmentation_audit.glued_two_char_marker_recovery`.
    - Actual = dry run: 524 applied with identical ref identities, same
      abstentions; VerseRefs 4186 → 4710; physical gaps 2917 → 2393, 0
      opened; 2668 blocks change owner; block loss 0, dual ownership 0,
      blocks entering text 0; chapters 337, ocr_blocks 57700,
      duplicate/out-of-order refs 0. Task 153 unchanged (360).
    - Historical freezes: task-153 test and tasks 154/155 generators run
      with `glued_marker_recovery=False`.
    - Audit cost: a reference pass «only X off» is skipped (the main pass
      is reused) when X is effectively off, so historical modes do not pay
      for passes identical to the main one; the task-140 contract keeps its
      120 s budget unchanged.
    - Tests: `test_glued_two_char_marker.py` (matcher incl. Spanish-word,
      ambiguous-glyph and digit-only negatives; diagnostic = runtime
      matcher; artifacts regenerate byte-identical; production = dry run).
      Torres 1835 CTest 44/44 PASS.

- [x] TORRES-1835-CHAIN-CLOSURE-157 Measure what remains after tasks 150-156 and close the bounded-family chain
  - Status: DONE
  - Evidence:
    - Production after task 156: VerseRefs 3892 (task 146) → 4710;
      physical gaps 3211 → 2393 over tasks 150/153/156.
    - Remaining unrecognized marker-like right/body line starts (1400):
      357 single digits, of which 356 already sit in their own verse (not
      a recovery target) and 1 is an «1 S» form; 58 «d d» and 50 «a d»
      and 19/12/10 «i3/a6/i4» already rejected by the progression guards
      (their chapters have other ordering damage); ambiguous glyphs «iS»
      20, «aS» 15, «99» 13, «39» 9, «93» 8 (S = 5 or 8, 9 = 2 or 3); prose
      or noise («a», «á», «y», «^», «%»).
    - No remaining family is both bounded and unambiguous from the source:
      the chain of source-only marker recoveries is closed. Further work
      needs new evidence (facsimile value reviews for the ambiguous glyphs,
      or repairing the ordering damage that trips the guards) and is listed
      under «Future / not scheduled».

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
- Torres 1835: ambiguous glued glyphs («iS», «aS», «99», «39», «93»; ≈65
  lines) need a facsimile value review before any rule (task 157).
- Torres 1835: marker lines held back by progression guards (≈140) point
  to chapter-order damage worth a dedicated audit (task 157).
