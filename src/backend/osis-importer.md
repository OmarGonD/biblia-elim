# OSIS importer

`biblia-osis-import` accepts the supported Bible subset of OSIS and emits the
SQLite module format v1 used by `SqliteBibleBackend`. Parsing is offline with
libxml2 `XML_PARSE_NONET`; external entities and network access are disabled.

Supported initially: canonical 66-book Bible `<chapter>`/`<verse>` elements,
`osisID` (and basic `sID` verse milestones), paragraph/title text, supplied
word text, `lemma="strong:H.../G..."`, footnote/cross-reference notes and
OSIS-style book/chapter/verse references. Morphology is parsed, persisted, and
loaded through the neutral backend word model. Unknown attributes are ignored.
Unknown inline elements retain their
textual content. Output keeps `schema_version=1`, `user_version=1`,
`source_format=osis`, and uses the same FTS and optional note tables as USFM.

The adapter intentionally has no OSIS knowledge in GTK or the backend reader;
future import extensions should converge through the existing SQLite writer.

Visible mixed content follows one parser-level whitespace policy. XML
whitespace is held as a separator candidate, collapsed to one ASCII space
before the next word, and discarded before `, . : ; ? !` (and closing
brackets). Thus indentation and line wrapping do not change `plainText`, and
all word/note/cross-reference positions remain UTF-8 byte offsets into the
final `std::string`.

## Support matrix

| Feature | Status |
|---|---|
| Bible OSIS / 66 books | supported subset |
| verse `osisID` | supported |
| verse/chapter `sID`/`eID` | supported for validated milestones |
| Strong / multi-Strong | supported subset |
| footnotes / crossrefs | supported subset |
| `<title>` / `<p>` | basic subset |
| UTF-8 offsets | supported by converged writer |
| morphology | parsed/audited, persisted, and loaded neutrally |
| non-Strong lemmas | audited by scheme, not imported |
| unknown inline | text preserved and element counted |
| unknown structural wrapper | children traversed and element counted |
| ignored attributes | counted as `element.attribute` |
| structured cross-reference ranges | unsupported; display and audit preserved, no structured target |
| external entities | not substituted; file and network regressions pass |
| deuterocanonical books | unsupported (canonical 66-book map) |
| commentaries/dictionaries | unsupported |

The current parser uses libxml2's tree API (`xmlReadFile`), not
`xmlTextReader`; memory usage is therefore proportional to the parsed XML
document. The measured scaling and resulting decision are recorded below.

## Validated behavior

Semantic equivalence is validated for the documented subset: container and
milestone OSIS produce identical neutral content, and equivalent USFM and OSIS
fixtures produce identical content through `SqliteBibleBackend`. The shared
comparison covers books, references, capabilities, text, paragraphs,
headings, spans, Strong words and ordered IDs, notes, cross-references and
UTF-8 byte offsets.

The XML parser is DOM-based `xmlReadFile` with exactly `XML_PARSE_NONET`.
`XML_PARSE_NOENT`, `XML_PARSE_DTDLOAD`, `XML_PARSE_XINCLUDE`,
`XML_PARSE_RECOVER` and `XML_PARSE_HUGE` are not enabled. Executable
regressions verify that a controlled `file://` external entity does not put
its sentinel in verse output, an intercepted localhost HTTP entity causes no
external-loader request, and an ordinary internal entity is not substituted.

Unknown elements inside a verse preserve their visible descendant text and
are counted by element name. Unknown structural wrappers are counted and
their children continue to be visited, allowing supported chapters and verses
inside them to import. Unconsumed attributes on known elements are aggregated
as `element.attribute`. Morph attributes, ordered opaque codes, schemes,
multi-value cases, and malformed values are audited. Valid tags are persisted
in source order and enable `feature.morphology`; malformed values create no
rows. Non-Strong lemma schemes are also audited while valid Strong tokens in
the same `lemma` remain available. The measured data, neutral representation,
parsing rules, and SQLite design are documented in `morphology-audit.md`.

Cross-reference `osisRef` values are tokenized into simple references and
range tokens. Valid simple targets surrounding a range are retained in source
order. Range tokens are counted and their note display text is preserved, but
neither endpoint is emitted as a structured target because schema v1 has no
representation for a range. Invalid simple targets are aggregated separately.
Structured range support therefore remains explicitly unsupported; overall
range handling is partial and display/audit only.

## DOM memory benchmark

The benchmark uses `scripts/generate_osis_fixture.py` for deterministic XML
and `scripts/benchmark_osis_import.py` for three isolated runs per size. The
runner obtains wall/user/system time and peak RSS from Linux `wait4(2)`, then
checks `PRAGMA integrity_check`, `PRAGMA foreign_key_check`, the verse count,
and absence of a writer `.tmp` file. Values below are medians; peak RSS also
shows the maximum of the three runs.

| Verses | Input bytes | Wall | User | System | Peak RSS median/max | SQLite bytes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| fixture (6) | 1,304 | 0.009 s | 0.004 s | 0.004 s | 10.9 / 11.2 MiB | 131,072 |
| 10,000 | 732,930 | 0.201 s | 0.190 s | 0.009 s | 18.8 / 19.4 MiB | 1,228,800 |
| 50,000 | 3,753,575 | 0.986 s | 0.947 s | 0.033 s | 58.9 / 58.9 MiB | 6,258,688 |
| 100,000 | 7,529,333 | 1.994 s | 1.907 s | 0.077 s | 108.9 / 109.0 MiB | 12,087,296 |

Relative to the 10.9 MiB process baseline, observed RSS growth was about
11.31, 13.40 and 13.64 bytes per input byte at 10k, 50k and 100k, or about
829, 1,006 and 1,027 bytes per verse. This is roughly proportional to the
full XML/verse count, as expected from DOM plus the neutral in-memory model.
All large runs completed successfully and produced usable SQLite modules.
Backend sanity checks cover startup, a 10,000-row chapter,
`getVerseContent`, and search. A 10,000-verse document malformed near EOF
failed cleanly with neither final output nor `.tmp` residue.

The repository contains a 6,682,045-byte, 30,218-verse local OSIS document,
but it includes `Tob`, outside the importer's documented canonical 66-book
subset. The unmodified real input was attempted and correctly rejected at
`Tob.1.1`; it is therefore not reported as a successful real-data benchmark.

### Decision

**KEEP DOM.** Peak RSS was approximately 109 MiB for 100,000 short verses,
over three times a typical Bible's verse count, with sub-two-second median
import time on the measured machine. The multiplier confirms DOM scaling, but
the absolute memory use remains practical for this occasional importer on
modest contemporary machines, while a streaming rewrite would carry parser
state and semantic-regression risk. Extremely text-heavy or non-Bible OSIS
documents can still grow proportionally and are outside this validated
subset; measurements should be repeated if that scope expands.

For the documented subset, the security, audit, semantic, large-import,
atomic-failure and backend-read checks support the status:

```text
OSIS PRODUCTION-READY FOR DOCUMENTED SUBSET
```
