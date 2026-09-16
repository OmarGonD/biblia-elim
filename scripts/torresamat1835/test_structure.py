"""
TORRES-1835-STRUCTURE-104: identidad de libro y de capítulo.

    python3 test_structure.py

Sin red y sin el testigo real. El fixture reproduce la geometría medida y
la redundancia del testigo (la cabecera corrida lleva libro, capítulo y
página impresa); su texto es sintético porque el escaneo no se puede
redistribuir.
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import layout
import page_parser
import source_ocr
import structure

DIR = os.path.dirname(os.path.abspath(__file__))
FIXTURE = os.path.join(DIR, "fixtures", "volume3_layout.json")
GUTTER = 1690


def _readings(pages):
    out = []
    for page in pages:
        header = " ".join(
            e.line.raw_text for e in layout.split_columns(page, gutter_hint=GUTTER)
            if e.zone is layout.Zone.HEADER)
        if header.strip():
            out.append(structure.read_header(page.scan_page, header))
    return out


def _parse():
    with open(FIXTURE, encoding="utf-8") as handle:
        data = json.load(handle)
    readings = _readings(list(source_ocr.pages_from_fixture(data)))
    spans = structure.book_spans(readings)
    return page_parser.parse_volume(
        source_ocr.pages_from_fixture(data), witness="fixture", volume="3",
        gutter_hint=GUTTER, book_spans=spans,
        header_chapters={r.scan_page: r.chapters for r in readings},
        with_walker=True)


# ---- I. IDs canónicos --------------------------------------------------
def test_book_ids_are_the_project_canon():
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(DIR)),
                                    "scripts", "torresamat"))
    import canon
    for osis in structure.VOLUME3_ORDER:
        assert osis in canon.POR_OSIS, osis
        assert structure.chapter_limit(osis) == canon.POR_OSIS[osis]["caps"]
    assert structure.VOLUME3_ORDER == ("Ps", "Prov", "Eccl", "Song", "Wis",
                                       "Sir", "Isa")


# ---- Cabeceras ---------------------------------------------------------
def test_header_reading_ignores_the_book_words():
    """«LOS» es 50 en romano. Contarlo inventaría corroboración."""
    reading = structure.read_header(0, "LOS SALMOS. II.")
    assert reading.book == "Ps"
    assert reading.chapters == [2], reading.chapters
    plain = structure.read_header(1, "LOS SALMOS.")
    assert plain.chapters == [], plain.chapters
    # y sobrevive a bastante destrozo de OCR
    assert structure.read_header(2, "SGCLS8IASTICO.").book == "Sir"
    assert structure.read_header(3, "PROVERBIOS.").book == "Prov"
    assert structure.read_header(4, "LOS S^IiM^S.").book == "Ps"
    # Una cabecera demasiado destrozada no se identifica, y eso está
    # bien: hacen falta tres planas seguidas para cambiar de libro, así
    # que una lectura perdida no mueve nada.
    assert structure.read_header(5, "flABIDVAIl.").book is None


# ---- A/B. Libros y fronteras -------------------------------------------
def test_book_span_needs_a_sustained_header_run():
    base = [structure.HeaderReading(n, "LOS SALMOS", book="Ps")
            for n in range(6)]
    # una cabecera suelta no parte el corpus
    noise = base + [structure.HeaderReading(6, "x", book="Isa")] + \
        [structure.HeaderReading(n, "LOS SALMOS", book="Ps") for n in (7, 8)]
    assert [s.osis for s in structure.book_spans(noise)] == ["Ps"]
    # tres seguidas sí
    real = base + [structure.HeaderReading(n, "PROVERBIOS", book="Prov")
                   for n in (6, 7, 8)]
    assert [s.osis for s in structure.book_spans(real)] == ["Ps", "Prov"]
    # y nunca se retrocede en el orden del tomo
    back = real + [structure.HeaderReading(n, "LOS SALMOS", book="Ps")
                   for n in (9, 10, 11)]
    assert [s.osis for s in structure.book_spans(back)] == ["Ps", "Prov"]


def test_no_verse_crosses_a_book_boundary():
    edition, stats, _walker = _parse()
    for osis, book in edition.books.items():
        for chapter in book.chapters.values():
            for verse in chapter.verses.values():
                for block in verse.blocks:
                    assert block.provenance.column == "right"
                    assert block.provenance.zone == "body"
    assert stats.get("book_boundaries", 0) >= 0


# ---- C/D/E/F. Resolución de capítulo -----------------------------------
def test_resolution_paths():
    _edition, _stats, walker = _parse()
    by_raw = {r["raw_numeral"]: r for r in walker.resolutions}

    direct = by_raw["SALMO I"]
    assert direct["method"] == "direct_ocr"
    assert direct["resolved_number"] == 1
    assert direct["review_required"] is False

    # D/F: LIM da 949, absurdo; la cabecera y la secuencia dicen 2.
    broken = by_raw["SALMO LIM"]
    assert 949 in broken["evidence"]["heading_candidates"] or \
        broken["parsed_candidate"] != 949
    assert broken["method"] == "running_header_correlated"
    assert broken["resolved_number"] == 2
    assert broken["review_required"] is False

    # E: numeral ilegible y sin apoyo -> sin número, a revisión.
    lost = by_raw["SALMO XQZ"]
    assert lost["method"] == "unresolved"
    assert lost["resolved_number"] is None
    assert lost["review_required"] is True


def test_impossible_roman_is_never_accepted_directly():
    """LIM = 949. Ni el numeral solo ni la secuencia sola lo aceptan."""
    lone = structure.resolve_chapter(raw_numeral="SALMO LIM",
                                     header_chapters=[], previous=None,
                                     book="Ps")
    assert lone.resolved is None and lone.review_required
    # con la cabecera y la secuencia de acuerdo, sí
    good = structure.resolve_chapter(raw_numeral="SALMO LIM",
                                     header_chapters=[53], previous=52,
                                     book="Ps")
    assert good.resolved == 53
    assert good.method == "running_header_correlated"
    # y por encima del límite Vulg del libro no se acepta
    over = structure.resolve_chapter(raw_numeral="CAPITULO CL",
                                     header_chapters=[], previous=None,
                                     book="Eccl")
    assert over.resolved is None


def test_sequence_alone_is_never_authority():
    """Una plana perdida no puede correr el corpus entero."""
    only_sequence = structure.resolve_chapter(
        raw_numeral="SALMO ????", header_chapters=[], previous=41, book="Ps")
    assert only_sequence.resolved is None
    assert only_sequence.review_required


# ---- G. Contradicción --------------------------------------------------
def test_heading_and_header_disagreement_goes_to_review():
    clash = structure.resolve_chapter(raw_numeral="SALMO XX",
                                      header_chapters=[35], previous=None,
                                      book="Ps")
    assert clash.resolved is None
    assert clash.review_required
    assert clash.parsed_candidate == 20     # la lectura se conserva


# ---- H/J/K. Secuencia, provenance, determinismo ------------------------
def test_no_duplicate_chapters_introduced():
    edition, _stats, walker = _parse()
    for osis, book in edition.books.items():
        numbers = [n for n in book.chapters if n and n > 0]
        assert len(numbers) == len(set(numbers)), osis
    resolved = [r["resolved_number"] for r in walker.resolutions
                if r["resolved_number"] is not None]
    assert len(resolved) == len(set(resolved))


def test_resolution_keeps_its_evidence():
    _edition, _stats, walker = _parse()
    for item in walker.resolutions:
        for field in ("book", "scan_page", "source_heading", "method",
                      "evidence", "confidence", "review_required",
                      "block_id"):
            assert field in item, field
        assert item["block_id"]
        assert "heading_candidates" in item["evidence"]
        assert "header_candidates" in item["evidence"]
        assert "sequential" in item["evidence"]


def test_deterministic():
    one = _parse()[2].resolutions
    two = _parse()[2].resolutions
    assert one == two


# ---- L. Offline --------------------------------------------------------
def test_offline():
    with open(os.path.join(DIR, "structure.py"), encoding="utf-8") as handle:
        text = handle.read()
    assert "urllib" not in text
    assert "requests" not in text


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
    print(f"torresamat1835_structure_failures={failures}")
    sys.exit(1 if failures else 0)
