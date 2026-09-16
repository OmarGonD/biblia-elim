"""
TORRES-1835-PARSER-103: el OCR con coordenadas al modelo neutral.

    python3 test_parser.py

Sin red y sin el testigo real: el fixture reproduce la geometría medida
sobre las páginas reales del tomo 3 (ver su bloque `provenance`). El
testigo es un escaneo de Google que no se puede redistribuir, así que su
OCR no viaja en el repositorio; contra el tomo real se valida con la
pasada de auditoría, no con este fixture.
"""
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import osis_out
import page_parser
import source_ocr
from layout import Column, Zone, measure, split_columns
from model import BlockKind
from parser import mark_canonical_title

DIR = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(DIR, "fixtures", "volume3_layout.json")
GUTTER = 1690           # medido en las páginas reales del tomo


def _fixture():
    with open(FIXTURE, encoding="utf-8") as handle:
        return json.load(handle)


def _pages():
    return list(source_ocr.pages_from_fixture(_fixture()))


def _parse():
    return page_parser.parse_volume(
        source_ocr.pages_from_fixture(_fixture()),
        witness="fixture:volume3_layout", volume="3", gutter_hint=GUTTER)


def _placed():
    return [(page, split_columns(page, gutter_hint=GUTTER))
            for page in _pages()]


def _chapters(edition):
    return {n: c for n, c in edition.books["Ps"].chapters.items() if n}


# ---- A/B. Segmentación y orden de columnas ----------------------------
def test_column_segmentation_and_order():
    for page, placed in _placed():
        layout = measure(page, gutter_hint=GUTTER)
        assert layout.gutter is not None
        for entry in placed:
            box = entry.line.bbox
            if entry.column is Column.LEFT:
                assert box[2] <= layout.gutter, entry.line.raw_text
            elif entry.column is Column.RIGHT:
                assert box[0] >= layout.gutter, entry.line.raw_text
            elif entry.column is Column.SPANNING:
                assert box[0] < layout.gutter < box[2]
        # Orden de lectura dentro de cada columna: de arriba abajo.
        for column in (Column.LEFT, Column.RIGHT):
            tops = [e.line.bbox[1] for e in placed if e.column is column]
            assert tops == sorted(tops), column


def test_latin_left_spanish_right():
    """El reparto lo decide la geometría; el idioma sólo lo audita."""
    from layout import language_audit
    for page, placed in _placed():
        audit = language_audit(placed, ("verbum", "latinum", "tertium"),
                               ("texto", "espanol", "versiculo"))
        if audit["left"]["latin"] or audit["right"]["spanish"]:
            assert audit["left"]["latin"] >= audit["left"]["spanish"]
            assert audit["right"]["spanish"] >= audit["right"]["latin"]


# ---- C. Sin fugas de latín --------------------------------------------
def test_no_latin_in_spanish_body():
    edition, _stats = _parse()
    for chapter in _chapters(edition).values():
        for verse in chapter.verses.values():
            low = verse.body.lower()
            for token in ("verbum", "latinum", "latina", "alterum", "tertium"):
                assert token not in low, verse.body
        # y el latín se conserva aparte, no se tira
        assert isinstance(chapter.parallel_latin, list)
    total_latin = sum(len(c.parallel_latin) for c in _chapters(edition).values())
    assert total_latin > 0


# ---- D. Continuación ---------------------------------------------------
def test_continuation_joins_the_right_verse():
    edition, stats = _parse()
    first = _chapters(edition)[1]
    assert "que sigue en la linea siguiente" in first.verses[1].body
    assert "primer versiculo" in first.verses[1].body
    assert stats.get("continuations", 0) >= 1


# ---- E/F. Barrera de encabezado y numeral ilegible ---------------------
def test_heading_is_a_barrier_and_survives_a_broken_numeral():
    edition, stats = _parse()
    chapters = _chapters(edition)

    headings = [b for c in chapters.values() for b in c.paratext
                if b.kind is BlockKind.CHAPTER_HEADING]
    assert headings
    broken = [b for b in headings if b.review_required]
    assert broken, "el numeral roto tiene que quedar marcado"
    assert stats.get("chapter_headings_unresolved", 0) >= 1

    editorial = [b for c in chapters.values() for b in c.paratext
                 if b.kind is BlockKind.EDITORIAL_HEADING]
    assert editorial

    # Nada de eso puede haber acabado en un versículo, y se comprueba
    # por estructura: ningún bloque del cuerpo puede venir de un renglón
    # que cruza el canal, ni ser de una clase de paratexto. (Un
    # versículo legítimo sí puede decir «salmo»: comprobarlo por
    # palabras sería el mismo error que se quiere evitar.)
    for chapter in chapters.values():
        for verse in chapter.verses.values():
            for block in verse.blocks:
                assert block.is_canonical, block.kind
                assert block.provenance.column == "right", block.provenance
                assert block.provenance.zone == "body", block.provenance
    spanning_texts = {b.text for c in chapters.values() for b in c.paratext
                      if b.provenance.column == "spanning"}
    assert spanning_texts
    for text in spanning_texts:
        for chapter in chapters.values():
            for verse in chapter.verses.values():
                assert text not in verse.body


# ---- G/H. Inscripción canónica, mismo versículo -----------------------
def test_canonical_title_keeps_its_verse_and_body():
    edition, _stats = _parse()
    chapters = sorted(_chapters(edition))
    second = chapters[1]
    slot = mark_canonical_title(edition, "Ps", second, 1)
    assert slot.number == 1 and slot.has_canonical_title

    xml = osis_out.generate(edition)
    assert f'<seg type="{osis_out.TITLE_SEG_TYPE}">' in xml
    assert f'<verse osisID="Ps.{second}.0"' not in xml
    body = xml[xml.index("</header>"):]
    assert "<title" not in body
    # título y cuerpo en el mismo versículo, cada uno una vez
    assert xml.count("Inscripcion del salmo") == 1
    assert xml.count("aqui empieza el cuerpo") == 1


# ---- I/J. Notas y mobiliario de página fuera del cuerpo ----------------
def test_footnotes_headers_and_footers_are_isolated():
    edition, stats = _parse()
    assert stats.get("footnote", 0) >= 1
    assert stats.get("pageheader", 0) >= 1
    assert stats.get("pagefooter", 0) >= 1
    for chapter in _chapters(edition).values():
        for verse in chapter.verses.values():
            low = verse.body.lower()
            for token in ("nota al pie", "segunda nota", "los salmos",
                          "libro de", "digitized"):
                assert token not in low, verse.body
    kinds = {b.kind for c in _chapters(edition).values() for b in c.paratext}
    assert BlockKind.FOOTNOTE in kinds
    assert BlockKind.PAGE_HEADER in kinds


# ---- K. Continuación entre páginas -------------------------------------
def test_cross_page_continuation():
    edition, _stats = _parse()
    first = _chapters(edition)[1]
    assert "continuacion del versiculo abierto en la pagina anterior" in \
        first.verses[2].body
    # y el versículo que abre después, en su sitio
    assert "tercer versiculo" in first.verses[3].body


# ---- M. Bloque ambiguo -------------------------------------------------
def test_ambiguous_block_goes_to_review():
    edition, _stats = _parse()
    queued = [b for b in edition.review_queue
              if b.kind is BlockKind.UNCLASSIFIED]
    assert queued
    assert all(b.review_required for b in queued)
    for block in queued:
        for chapter in _chapters(edition).values():
            for verse in chapter.verses.values():
                assert block.text not in verse.body


# ---- N. Provenance -----------------------------------------------------
def test_every_block_keeps_its_provenance():
    edition, _stats = _parse()
    blocks = []
    for chapter in _chapters(edition).values():
        blocks.extend(chapter.paratext)
        blocks.extend(chapter.parallel_latin)
        for verse in chapter.verses.values():
            blocks.extend(verse.blocks)
    blocks.extend(edition.review_queue)
    assert blocks
    for block in blocks:
        prov = block.provenance
        assert prov.witness, block.text
        assert prov.volume == "3"
        assert prov.page is not None
        assert prov.column in ("left", "right", "spanning", "unknown")
        assert prov.zone in ("header", "body", "apparatus", "footer")
        assert prov.bbox and len(prov.bbox) == 4
        assert prov.block_id
        # El OCR crudo se conserva aparte del normalizado.
        assert block.raw_text is not None


# ---- O. Determinismo ---------------------------------------------------
def test_deterministic():
    one, stats_one = _parse()
    two, stats_two = _parse()
    assert stats_one == stats_two
    xml_one, xml_two = osis_out.generate(one), osis_out.generate(two)
    assert xml_one == xml_two
    assert (hashlib.sha256(xml_one.encode()).hexdigest()
            == hashlib.sha256(xml_two.encode()).hexdigest())


# ---- P/Q. Neutralidad y offline ---------------------------------------
def test_backend_neutral_and_offline():
    for name in ("source_ocr.py", "layout.py", "page_parser.py"):
        with open(os.path.join(DIR, name), encoding="utf-8") as handle:
            text = handle.read()
        assert "urllib" not in text, name
        assert "sword" not in text.lower(), name
    edition, _stats = _parse()
    rows = [(b, c, v, verse.body)
            for b, book in edition.books.items()
            for c, chapter in book.chapters.items()
            for v, verse in chapter.verses.items()]
    assert rows and all(isinstance(r[1], int) for r in rows)


# ---- Regresión del bug de 1882 ----------------------------------------
def test_legacy_absorption_bug_cannot_return():
    """canonical verse / SALMO <numeral roto> / argumento / siguiente verso.

    Esto es lo que destruyó 158 versículos de la edición de 1882: la
    división no se reconoció, el renglón se trató como continuación y se
    concatenó al versículo abierto. Si alguien reintroduce esa
    arquitectura, este test cae.
    """
    edition, _stats = _parse()
    chapters = _chapters(edition)
    first, second = sorted(chapters)[:2]

    last_verse = chapters[first].verses[max(chapters[first].verses)]
    assert "SALMO" not in last_verse.body
    assert "Argumento" not in last_verse.body
    assert "argumento" not in last_verse.body.lower()

    # La división existe, como paratexto, y marcada para revisión.
    headings = [b for b in chapters[second].paratext
                if b.kind is BlockKind.CHAPTER_HEADING]
    assert headings

    # El primer versículo del capítulo siguiente está limpio.
    opening = chapters[second].verses[1]
    assert "SALMO" not in opening.body
    assert opening.body.startswith("Inscripcion del salmo")


if __name__ == "__main__":
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"ok {name}")
            except AssertionError as exc:
                failures += 1
                print(f"FAIL {name}: {exc}")
    print(f"torresamat1835_parser_failures={failures}")
    sys.exit(1 if failures else 0)
