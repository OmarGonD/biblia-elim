# Neutral Strong lexicon (experimental)

Strong metadata is represented by `StrongId` (`Hebrew`/`Greek` plus a positive
number). A standalone SQLite lexicon may contain:

```sql
CREATE TABLE lexicon_entries(
  strong TEXT PRIMARY KEY,
  lemma TEXT, transliteration TEXT, pronunciation TEXT, definition TEXT
);
```

`SqliteStrongLexicon` opens this file read-only and implements `BibleLexicon`.
An existing row is valid even when some nullable text fields are empty. A
missing ID returns `LexiconEntry.valid=false`; this has no effect on Bible
Strong tokens or concordance. `BibleApplicationResources` optionally binds a
lexicon beside a `BibleBackend` without transferring ownership or coupling the
two implementations.
The format intentionally carries no Bible-module or SWORD dependency and is
not yet a distributed lexicon specification. Bible modules keep their v1
`verse_words.strong` text for compatibility and optionally expose the derived
`verse_word_strongs` index for paginated concordance queries.

The measured future `word_id/strong_id` prototype reduced RV1909 from 53.7 MB
to about 35.2 MB, but reconstructing IDs in normal verse reads roughly doubled
that enriched read path. It remains a possible post-v1 evolution and is not
part of the official schema.
