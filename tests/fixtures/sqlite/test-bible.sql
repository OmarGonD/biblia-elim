PRAGMA user_version = 1;
PRAGMA foreign_keys = ON;

CREATE TABLE metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE books (
    book_id INTEGER PRIMARY KEY,
    osis TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    short_name TEXT,
    testament INTEGER NOT NULL CHECK(testament IN (1, 2)),
    position INTEGER NOT NULL UNIQUE
);

CREATE TABLE verses (
    book_id INTEGER NOT NULL,
    chapter INTEGER NOT NULL CHECK(chapter > 0),
    verse INTEGER NOT NULL CHECK(verse > 0),
    text TEXT NOT NULL,
    PRIMARY KEY(book_id, chapter, verse),
    FOREIGN KEY(book_id) REFERENCES books(book_id)
);

CREATE INDEX verses_book_chapter ON verses(book_id, chapter, verse);

CREATE VIRTUAL TABLE verses_fts USING fts5(
    text,
    content='verses',
    content_rowid='rowid'
);

INSERT INTO metadata(key, value) VALUES
    ('schema_version', '1'),
    ('module_id', 'FakeBible'),
    ('name', 'SQLite Test Bible'),
    ('description', 'In-memory test Bible'),
    ('language', 'en'),
    ('module_type', 'bible'),
    ('versification', 'custom'),
    ('feature.verses', 'true'),
    ('feature.search', 'true'),
    ('feature.strong', 'true'),
    ('feature.morphology', 'true'),
    ('feature.headings', 'false'),
    ('feature.footnotes', 'false'),
    ('feature.crossrefs', 'false'),
    ('feature.dictionary', 'false'),
    ('source_format', 'fixture');

INSERT INTO books(book_id, osis, name, short_name, testament, position) VALUES
    (1, 'Gen', 'Genesis', 'Gen', 1, 1),
    (40, 'John', 'John', 'Jn', 2, 40);

INSERT INTO verses(book_id, chapter, verse, text) VALUES
    (1, 1, 1, 'In the beginning God created.'),
    (40, 3, 16, 'For God so loved the world.'),
    (40, 3, 17, 'God sent his Son to save the world.'),
    (40, 3, 18, 'The witness chose the light.'),
    (40, 4, 1, 'A traveler rested beside the well.');

INSERT INTO verses_fts(rowid, text)
SELECT rowid, text FROM verses;

CREATE TABLE verse_words (
    book_id INTEGER NOT NULL, chapter INTEGER NOT NULL, verse INTEGER NOT NULL,
    sequence INTEGER NOT NULL, start INTEGER NOT NULL, length INTEGER NOT NULL,
    text TEXT NOT NULL, strong TEXT,
    PRIMARY KEY(book_id, chapter, verse, sequence),
    FOREIGN KEY(book_id, chapter, verse) REFERENCES verses(book_id, chapter, verse)
);
CREATE TABLE verse_word_strongs (
    book_id INTEGER NOT NULL, chapter INTEGER NOT NULL, verse INTEGER NOT NULL,
    sequence INTEGER NOT NULL, strong TEXT NOT NULL,
    PRIMARY KEY(book_id, chapter, verse, sequence, strong)
);
CREATE INDEX verse_word_strongs_canonical ON verse_word_strongs(
    strong, book_id, chapter, verse, sequence
);
CREATE TABLE verse_word_morphology (
    book_id INTEGER NOT NULL, chapter INTEGER NOT NULL, verse INTEGER NOT NULL,
    word_sequence INTEGER NOT NULL, morphology_sequence INTEGER NOT NULL,
    scheme TEXT NOT NULL, code TEXT NOT NULL,
    PRIMARY KEY(book_id, chapter, verse, word_sequence, morphology_sequence),
    FOREIGN KEY(book_id, chapter, verse, word_sequence)
        REFERENCES verse_words(book_id, chapter, verse, sequence)
);
CREATE INDEX verse_word_morphology_lookup ON verse_word_morphology(
    scheme, code, book_id, chapter, verse, word_sequence, morphology_sequence
);
INSERT INTO verse_words VALUES
    (40,3,16,0,11,5,'loved','G25'),
    (40,3,16,1,21,5,'world',NULL),
    (40,3,17,0,0,3,'God','G25');
INSERT INTO verse_word_strongs VALUES(40,3,16,0,'G25');
INSERT INTO verse_word_morphology VALUES
    (40,3,16,0,0,'robinson','V-AAI-3S'),
    (40,3,16,0,1,'custom.alpha','opaque/code'),
    (40,3,16,1,0,'','HR/Ncfsa'),
    (40,3,17,0,0,'robinson','V-AAI-3S'),
    (40,3,17,0,1,'custom.alpha','V-AAI-3S');
