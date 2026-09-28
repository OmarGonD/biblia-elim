#!/usr/bin/env python3
"""Generate the neutral Strong lexicon SQLite file (MORPH-109).

Converts ui/strongs-elim.xml -- the bundled, public-domain Strong 1890
data plus Open Scriptures glosses, already used at runtime by the legacy
interlinear path (see load_strongs() in src/main/interlineal.cc) -- into
the standalone `lexicon_entries` SQLite table documented in
src/backend/strong-lexicon-format.md and read by
SqliteStrongLexicon (src/backend/sqlite/sqlite_strong_lexicon.{h,cc}):

    CREATE TABLE lexicon_entries(
      strong TEXT PRIMARY KEY,
      lemma TEXT, transliteration TEXT, pronunciation TEXT, definition TEXT
    );

Field mapping, straight from the bundled XML -- nothing here is
invented for a field the source does not provide:

    <s n="G1615" l="..." t="..." g="..." r="..." d="..."/>
      n (Strong id, e.g. "G1615"/"H430") -> strong
      l (lemma)                          -> lemma
      t (transliteration)                -> transliteration
      d (definition)                     -> definition
      pronunciation                      -> left empty (the XML has no
                                             such field; NULL/empty rows
                                             are valid per the lexicon
                                             format doc)
      g (short gloss), r (related Strong)-> not part of lexicon_entries;
                                             left unused rather than
                                             stuffed into an unrelated
                                             column

Usage:

    ./generate_strongs_lexicon.py --xml ui/strongs-elim.xml \
        --output build/lexicon/strongs-elim.sqlite

Invoked from src/main/CMakeLists.txt at build time; not shipped as a
generated file in the source tree.
"""

import argparse
import sqlite3
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def generate(xml_path: Path, output_path: Path) -> int:
    tree = ET.parse(xml_path)
    root = tree.getroot()

    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists():
        output_path.unlink()

    connection = sqlite3.connect(str(output_path))
    try:
        connection.execute(
            "CREATE TABLE lexicon_entries("
            "strong TEXT PRIMARY KEY, lemma TEXT, transliteration TEXT, "
            "pronunciation TEXT, definition TEXT)"
        )
        count = 0
        for entry in root.findall("s"):
            strong = (entry.get("n") or "").strip()
            if not strong:
                continue
            lemma = entry.get("l") or ""
            transliteration = entry.get("t") or ""
            definition = entry.get("d") or ""
            connection.execute(
                "INSERT OR REPLACE INTO lexicon_entries"
                "(strong, lemma, transliteration, pronunciation, definition)"
                " VALUES (?, ?, ?, '', ?)",
                (strong, lemma, transliteration, definition),
            )
            count += 1
        connection.commit()
    finally:
        connection.close()
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xml", type=Path, required=True,
        help="path to ui/strongs-elim.xml")
    parser.add_argument("--output", type=Path, required=True,
        help="path of the SQLite lexicon file to (re)write")
    arguments = parser.parse_args()

    if not arguments.xml.is_file():
        print(f"error: no such file: {arguments.xml}", file=sys.stderr)
        return 1

    count = generate(arguments.xml, arguments.output)
    print(f"{count} lexicon entries written to {arguments.output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
