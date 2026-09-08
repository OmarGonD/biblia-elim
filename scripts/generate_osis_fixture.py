#!/usr/bin/env python3
"""Generate deterministic, non-compressibility-trivial OSIS benchmark input."""

import argparse
from pathlib import Path


def verse_body(number: int) -> str:
    names = ("Moisés", "Jesús", "Jehová", "SEÑOR", "corazón")
    body = f"Texto {number} para {names[number % len(names)]} variante {number % 997}"
    if number % 97 == 0:
        body += f' <w lemma="strong:G{25 + number % 500}">palabra{number % 31}</w>'
    if number % 251 == 0:
        body += f'<note type="footnote" n="+">Nota sintética {number % 113}.</note> continúa'
    if number % 509 == 0:
        body += ('<note type="crossReference"><reference osisRef="Gen.1.1">'
                 'Genesis 1:1</reference></note> relacionada')
    return body + "."


def generate(output: Path, verses: int, malformed_near_eof: bool) -> None:
    if verses < 1:
        raise ValueError("--verses must be positive")
    with output.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        stream.write('<osis><osisText><div type="book"><chapter osisID="Gen.1"><p>\n')
        for number in range(1, verses + 1):
            stream.write(f'<verse osisID="Gen.1.{number}">{verse_body(number)}</verse>\n')
        if malformed_near_eof:
            stream.write("<broken>")
        else:
            stream.write("</p></chapter></div></osisText></osis>\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--verses", type=int, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--malformed-near-eof", action="store_true")
    arguments = parser.parse_args()
    generate(arguments.output, arguments.verses, arguments.malformed_near_eof)


if __name__ == "__main__":
    main()
