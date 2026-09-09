# Semantic equivalence fixtures

`semantic-container.xml` and `semantic-milestone.xml` express the same OSIS
semantics using container and milestone verse/chapter forms. The two USFM
files express that same portable subset for cross-importer comparison.

Added text is intentionally excluded: the current OSIS importer has no
unambiguous mapping to the neutral `BibleTextStyle::Added` span produced by
USFM `\add`, so it is not a semantically comparable feature yet.

## Real-world producer extracts

The files under `real-world/` are deliberately tiny extracts. They retain the
namespace, header, nesting, attributes, and inline structure that matter to
the regression; unrelated books, chapters, verses, and header fields were
removed.

### Open Scriptures Hebrew Bible

`morphhb-genesis-1-1.xml` is an extract of `wlc/Gen.xml` from the
[Open Scriptures Hebrew Bible](https://github.com/openscriptures/morphhb).
It retains the producer's namespaced OSIS header, `<w>` segmentation,
slash-qualified lemma values, morphology attributes, and sof-pasuq `<seg>`.
The Westminster Leningrad Codex consonants and vowels are public domain; the
Open Scriptures lemma and morphology annotations are redistributed under
[CC BY 4.0](https://github.com/openscriptures/morphhb/blob/master/LICENSE.md),
with attribution to the Open Scriptures Hebrew Bible project and its
contributors. This repository's copy is the stable regression artifact.

### Torres Amat 1882

`torres-amat-1882-psalm-1.xml` is the first two verses emitted in the exact
form produced by `scripts/torresamat/osis.py`. The verse text is the manual
transcription recorded in `scripts/torresamat/rescatar.py` from volume III,
sheet 10 of the 1882 edition. The source facsimile is
[Internet Archive item la-sagrada-biblia-vulgata-tomo-iiv_202111](https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111).
Félix Torres Amat died in 1847 and the 1882 edition is public domain. The
fixture retains the generator's OSIS namespace, schema declaration, work
metadata, Vulgate ref-system declaration, and container verse structure.

### Inputs outside the supported module scope

`unsupported-tobit-structure.xml` is a text-free negative control derived
from the Torres Amat generator's structure, not a source extract. It proves
that the deuterocanonical `Tob` identifier is rejected and is never silently
mapped into the documented canonical 66-book subset. Deuterocanonical works
remain future scope.

Only Bible modules in that 66-book subset are supported. Commentary,
dictionary, general-book, and other OSIS work types are not fixture candidates
and must not be imported as Bibles; they remain documented as unsupported in
`src/backend/osis-importer.md`.
