# Semantic equivalence fixtures

`semantic-container.xml` and `semantic-milestone.xml` express the same OSIS
semantics using container and milestone verse/chapter forms. The two USFM
files express that same portable subset for cross-importer comparison.

Added text is intentionally excluded: the current OSIS importer has no
unambiguous mapping to the neutral `BibleTextStyle::Added` span produced by
USFM `\add`, so it is not a semantically comparable feature yet.
