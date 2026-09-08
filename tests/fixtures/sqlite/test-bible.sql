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
    ('feature.strong', 'false'),
    ('feature.morphology', 'false'),
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
