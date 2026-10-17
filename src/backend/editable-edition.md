# Editable Bible editions

## Product contract

An editable edition is a complete, separate Bible project copied from an
approved installed source. It never writes into the source module. Authors can
edit verse text and add comments attached to books or verses. The resulting
project and its exports contain a complete installable Bible.

Only sources whose terms permit derivative works and redistribution may be
selected. Eligibility must come from an explicit verified source record, not
from an assumed permission based on free download or the age of the underlying
work. The source identity must include module ID, versification, license,
attribution, and a stable content fingerprint. A module ID alone is
insufficient because a module can be updated in place. Project metadata also
records title, contributors, and timestamps when known.

## Sharing and distribution

The first collaboration format is a portable package containing the complete
Bible for export, import, review, and distribution without a hosted service.
Import checks project identity, source identity, and versification. Distinct
edits merge; concurrent edits to the same reference are surfaced as conflicts
and never silently replace local work.

Creation and export are restricted to eligible sources, so the complete Bible
can be distributed as an installable module. Every package carries the source
attribution and license. If rights cannot be established, that source is not
offered as a starting point; the user may supply documented permission to add
it to the eligible-source catalog.

## Milestones

1. Persistent project model and source identity; verse overrides and
   book/verse comments.
2. GTK creation, editing, and reading workflow.
3. Portable package export/import with compatibility checks and conflict
   reporting.
4. License-aware installable module export and eligible-source catalog.

Projects should use stable OSIS book IDs and native chapter/verse references.
They must not infer equivalence across versifications; conversion requires an
explicit successful mapping.

## Working format

The first implementation stores a full Bible in the regular SQLite module
format v1 and adds project metadata plus `book_comments` and `verse_comments`
extension tables. `SqliteModuleWriter` owns construction of verses, search
index, capabilities, byte offsets, and atomic output; the project layer adds
its identity and comments afterward. The project file can therefore be opened
by the normal SQLite Bible reader while editing is developed.

The creator supplies the project name at creation and may rename it later.
Verse text, book comments, and verse comments are committed as individual
SQLite transactions so work can continue over many sessions. Editing a verse
clears only that verse's byte-offset-bound source apparatus (word tags,
formatting spans, footnotes, and cross-references), then rebuilds its search
entry and capability flags; headings and all other verses remain intact.

## Initial source eligibility audit

The repository ships Torres Amat in `modulos/sqlite/TorresAmat.sqlite`; its
SWORD configuration declares `DistributionLicense=Public Domain`, identifies
the 1823–1825 translation by Félix Torres Amat (d. 1847), and links the 1882
facsimile used for the OCR. This is an initial Spanish candidate, subject to
preserving the attribution and source notices.

The local SWORD installation has `SpaRV` (Reina-Valera 1909), whose module
configuration and CrossWire module page declare Public Domain. It is another
Spanish candidate if present in the user's module catalog. `SpaRVG` is
CC BY-NC-ND and cannot be a derivative-work source; `SpaRV1909` says
copyrighted with permission granted to CrossWire only; and `NacarColunga`
explicitly says not to distribute. Those are excluded. Although CrossWire
lists `SpaPlatense` as Public Domain, its copyright note relies on a term
calculated from publication year, while Argentina's current law states a
life-plus-70-year term and the module identifies translator Juan Straubinger
as dying in 1956. Exclude it pending rights clarification rather than rely on
conflicting metadata.

## Curated bilingual projects

`biblia-curated-edition SOURCE.sqlite OUTPUT.sqlite PLAN.json` builds a complete
project atomically from an approved module. Existing output is never replaced.
Plans bind source identity/fingerprint, optional language, exact expected verse
text, decisions, comments and glossary. English source modules receive English
comment labels; others retain Spanish labels. Evidence URLs are preserved in
both the decision and verse comment and exposed as links by the GTK editor.

An existing editable project may be cloned to a new ID/name while preserving
its original source metadata, revision history and editorial tables. A migration
plan can provide `expected_current` to validate the already-edited text while
retaining `expected_base` in the commentary. Reproducible fresh-source plans
omit `expected_current`. Bilingual plans share stable editorial reference IDs
and evidence, not verse wording or approval state. See `doc/manning/editions.json`.

## Original text and contribution review

Every new project stores immutable original verse text in the optional
`edition_base_verses` extension (book/chapter/verse primary key). SQLite format
v1 and `user_version=1` remain unchanged. On editing an older project, its
first recorded `old_text` reconstructs the original for changed verses; all
other verses use their existing text. Migration is atomic and leaves revision
history intact. Source-module updates or removal do not redefine this baseline.

`EditableEdition::comparisons` reads projects without migration or writes,
including legacy projects. It supports a chapter scope for reader badges.
A verse is edited only when current text differs byte-for-byte from the
original; restoring it removes that badge and retains history. Nonblank verse
comments have an independent badge. Pending proposals require both a nonempty
proposal and pending review status; already applied drafts awaiting approval
are included. Book comments are not projected onto every verse.

The reader displays provenance and links beside verse numbers. Comparison
shows escaped UTF-8 text with its changed interval underlined and bold, plus
comment, proposal, rationale and source links. The editor offers filters for
changed text, verse comments and pending proposals, with direct navigation.
Unsaved text/comments/evidence must be saved before comparison or navigation.

The reader's **Aportes** button opens a read-only contribution browser for the
selected editable Bible. Reading mode offers the same browser in its compact
toolbar. All contributed verses appear by default, once per reference, in book
order, including comment-only verses and pending proposals. Filters separate
modified text, verse comments and pending proposals. Search matches book names,
references, comments, rationale, proposals and evidence without case or accent
sensitivity. Each row shows its note title, reference and contribution types;
activating it opens the comment/change reader above the browser. Closing that
reader returns to the same list, search, filter, selection and scroll position.
The separate **Ir al versículo** action closes the browser and navigates to the
selected Bible's exact verse; it stays disabled until a result is selected.
Ordinary source Bibles hide the button, while empty editable projects explain
that there are no verse contributions yet. Each opening reloads the current
records without migrating the database. Book introductions are not counted as
verse contributions.

### PDF export

**Exportar PDF…** is available in the study-Bible manager and the **Aportes**
browser, for every saved editable edition. Choose the complete Bible (default)
or only verses with contributions. Browser search/type filters do not restrict
the export. Save pending editor changes before exporting; the PDF reads the
saved project and never changes its text or migrates legacy databases.

The self-contained A4 PDF includes book and contribution indexes, hierarchical
book/chapter and note bookmarks, current Bible text, book introductions and
verse commentary. Blue **Aporte** links open the corresponding note in the same
PDF; **Volver al versículo** links return to its passage. Sources remain
clickable external URLs. Actual text changes show original/current wording;
unapplied pending proposals are labelled separately. The cover preserves the
base Bible's attribution and license. Navigation labels follow the edition's
Spanish/English language; no specific author or module is hardcoded.

This navigation uses standard PDF links and destinations, without JavaScript,
online services or a connection to the app. Text and commentary are a snapshot
of the export; regenerate the PDF after editing the project. The exporter uses
Cairo PDF (1.16+) and Pango already available in GTK's platform stack, including
Windows/macOS builds; Poppler is only an optional independent test reader.

Generation runs in a worker, with cancellation checked during layout and before
publishing the file. A temporary file in the destination directory is moved
into place only after rendering and closing succeed; failures/cancellation
remove it and preserve an existing PDF. The project itself cannot be selected
as its output. This PDF export does not implement collaborative module packages
or project import (EDITION-103).

For scripted exports, build the optional `biblia-study-pdf` target and run:

```sh
build/src/gtk/biblia-study-pdf EDITION.sqlite OUTPUT.pdf
build/src/gtk/biblia-study-pdf EDITION.sqlite APORTES.pdf --contributions-only
```

PDF links/destinations and outlines follow the
[Cairo tags](https://www.cairographics.org/manual/cairo-Tags-and-Links.html) and
[PDF surface](https://www.cairographics.org/manual/cairo-PDF-Surfaces.html) APIs.

The contribution dialog prioritizes the commentary. An unchanged verse does
not repeat its Bible text or show a comparison card. Changed text remains
comparable below the commentary, and applied proposals are not repeated.
Notes with a short title line, lead line and blank-separated body receive
separate typographic elements. A final Fuente:/Source: paragraph appears with
source links in the footer, rather than repeating raw URLs throughout the view.
Provenance and qualified review state (comment versus translation) live in a
collapsed editorial-details section. Identical comment/rationale are displayed
once; distinct full records and raw evidence remain available in disclosures.
Labels keep the interface paragraph direction when notes begin with Hebrew,
while Pango still renders individual right-to-left runs correctly.

A curated passage may supply an optional `comment` string to preserve its
presentation verbatim, while rationale/evidence remain structured separately.
Without this field the command generates its usual editorial summary.
