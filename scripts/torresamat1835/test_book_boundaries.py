"""
TORRES-1835-BOOK-BOUNDARIES-106: confirmar el libro y localizar su frontera
son dos cosas distintas.

    python3 test_book_boundaries.py

Sin red. Geometría medida en el tomo 3 real; texto sintético.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import book_boundaries as bb
import structure
from layout import Column, PlacedLine, Zone
from source_ocr import SourceLine, SourcePage, SourceWord

W = 3402


def line(index, text, x0, x1, y=600):
    word = SourceWord(text, (x0, y, x1, y + 80), 50)
    return SourceLine(index, [word], (x0, y, x1, y + 80))


def placed(index, text, x0, x1, column=Column.SPANNING, zone=Zone.BODY, y=600):
    return PlacedLine(line=line(index, text, x0, x1, y), column=column,
                      zone=zone)


TITLE = "LIBRO DE LA SABIDURIA"        # rótulo: corto, ancho y centrado
CENTRED = (520, 2880)


def window(pages):
    return [(page, W, blocks) for page, blocks in pages]


# ---- A. Confirmación ≠ frontera ----------------------------------------
def test_confirmation_is_not_the_boundary():
    decision = bb.resolve_boundary(
        from_book="Song", to_book="Wis", confirmation_page=326,
        window=window([(319, [placed(1, TITLE, *CENTRED)]),
                       (320, []), (321, []), (322, []), (323, []),
                       (324, []), (325, []), (326, [])]))
    assert decision.effective_page == 319
    assert decision.confirmation_page == 326
    assert decision.lookback_distance == 7
    assert decision.review_required is False
    assert decision.effective_block == "p0319l0001"


# ---- B/C/D. El contrato de confirmación no se afloja -------------------
def test_book_identity_still_needs_three_headers():
    base = [structure.HeaderReading(n, "LOS SALMOS", book="Ps")
            for n in range(6)]
    one = base + [structure.HeaderReading(6, "x", book="Prov")]
    two = one + [structure.HeaderReading(7, "x", book="Prov")]
    three = two + [structure.HeaderReading(8, "x", book="Prov")]
    assert [s.osis for s in structure.book_spans(one)] == ["Ps"]
    assert [s.osis for s in structure.book_spans(two)] == ["Ps"]
    assert [s.osis for s in structure.book_spans(three)] == ["Ps", "Prov"]


# ---- E. Ventana acotada -------------------------------------------------
def test_no_age_based_discarding():
    far = bb.resolve_boundary(
        from_book="Song", to_book="Wis", confirmation_page=340,
        window=window([(300, [placed(1, TITLE, *CENTRED)])]))
    # El llamante decide qué ventana pasa; la función no descarta por
    # edad. La única constante que queda es adyacencia local entre
    # bloques del mismo frontmatter.
    assert bb.CLUSTER_GAP_PAGES <= 5
    assert far.effective_page == 300


def test_candidates_never_reach_the_previous_book():
    """Un bloque anterior al comienzo del libro en curso no es candidato."""
    tracker = bb.BookCandidateTracker()
    from source_ocr import SourcePage
    page = SourcePage(scan_page=302, width=W, height=4837, dpi=600)
    found = tracker.observe_page(
        page, [placed(1, "LIBRO DE LA SABIDURIA", *CENTRED)],
        current_book="Song", current_book_started=303)
    assert found == []


# ---- F. Geometría manda -------------------------------------------------
def test_geometry_decides_not_the_words():
    # Mismo texto, geometría equivocada: no es título.
    for column, zone, box in ((Column.RIGHT, Zone.BODY, (1750, 3000)),
                              (Column.SPANNING, Zone.HEADER, CENTRED),
                              (Column.SPANNING, Zone.APPARATUS, CENTRED)):
        assert bb.is_book_title_block(
            placed(1, TITLE, *box, column=column, zone=zone), W,
            book="Wis") == 0.0
    # Estrecho o descentrado tampoco.
    assert bb.is_book_title_block(placed(1, TITLE, 400, 1200), W,
                                  book="Wis") == 0.0
    # Y el bueno sí.
    assert bb.is_book_title_block(placed(1, TITLE, *CENTRED), W,
                                  book="Wis") > 0.0


def test_prose_is_not_a_title():
    """La advertencia de cada libro lo menciona en cada frase y ocupa el
    mismo ancho: sin el corte por palabras, cualquier renglón pasaría."""
    prose = ("Este Libro es llamado por los griegos la Sabiduria de "
             "Salomon y su autor saco la doctrina que ensena")
    assert bb.is_book_title_block(placed(1, prose, *CENTRED), W,
                                  book="Wis") == 0.0


def test_a_division_is_not_a_book_title():
    assert bb.is_book_title_block(placed(1, "CAPITULO PRIMERO.", *CENTRED), W,
                                  book="Wis") == 0.0


# ---- O. El canon no decide ----------------------------------------------
def test_canon_limits_never_move_a_boundary():
    source = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "book_boundaries.py"), encoding="utf-8").read()
    for token in ("chapter_limit", "vulg", "POR_OSIS", "canon"):
        assert token not in source, token
    # Un libro que se pasa del canon no mueve nada por sí solo.
    decision = bb.resolve_boundary(
        from_book="Wis", to_book="Sir", confirmation_page=370,
        window=window([(365, [placed(1, "LIBRO DEL ECLESIASTICO", *CENTRED)])]))
    assert decision.effective_page == 365


# ---- P. Frontera ambigua ------------------------------------------------
def test_ambiguous_boundary_is_flagged_not_guessed():
    decision = bb.resolve_boundary(
        from_book="Song", to_book="Wis", confirmation_page=326,
        window=window([(322, []), (323, []), (324, []), (325, [])]))
    assert decision.review_required is True
    assert decision.effective_block is None
    assert decision.effective_page == 326          # se queda donde estaba
    assert decision.confidence < 0.5
    assert "no book-title block" in decision.evidence[0]


def test_ambiguous_boundary_does_not_move_the_span():
    spans = [structure.BookSpan("Song", 303), structure.BookSpan("Wis", 326)]
    decisions, _t = bb.resolve_with_candidates(
        spans, lambda: iter([(SourcePage(scan_page=n, width=W, height=4837,
                                         dpi=600), [])
                             for n in range(303, 327)]))
    assert decisions[0].review_required is True
    assert spans[1].first_page == 326, "sin evidencia no se mueve nada"


# ---- J/K. Un bloque, un libro ------------------------------------------
def test_spans_stay_disjoint_and_ordered():
    spans = [structure.BookSpan("Song", 303), structure.BookSpan("Wis", 326)]
    pages = [(SourcePage(scan_page=n, width=W, height=4837, dpi=600),
              [placed(1, TITLE, *CENTRED)] if n == 319 else [])
             for n in range(303, 327)]
    bb.resolve_with_candidates(spans, lambda: iter(pages))
    assert spans[0].first_page == 303
    assert spans[0].last_page == 318
    assert spans[1].first_page == 319
    assert spans[0].last_page < spans[1].first_page
    # cada plana pertenece a un solo libro
    for page in range(303, 330):
        owners = [s.osis for s in spans
                  if s.first_page <= page and
                  (s.last_page is None or page <= s.last_page)]
        assert len(owners) == 1, (page, owners)


# ---- Q/R/S. Provenance, determinismo, offline --------------------------
def test_decision_keeps_its_evidence():
    decision = bb.resolve_boundary(
        from_book="Ps", to_book="Prov", confirmation_page=212,
        window=window([(210, [placed(1, "LIBRO DE LOS PROVERBIOS", *CENTRED)])]))
    assert decision.evidence and decision.confidence > 0
    assert decision.effective_block == "p0210l0001"
    assert decision.first_evidence_page == 210
    assert decision.confirmation_page == 212


def test_deterministic_and_offline():
    def run():
        return bb.resolve_boundary(
            from_book="Song", to_book="Wis", confirmation_page=326,
            window=window([(319, [placed(1, TITLE, *CENTRED)])]))
    one, two = run(), run()
    assert (one.effective_page, one.evidence, one.confidence) == \
           (two.effective_page, two.evidence, two.confidence)
    source = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "book_boundaries.py"), encoding="utf-8").read()
    assert "urllib" not in source and "requests" not in source


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
    print(f"torresamat1835_book_boundaries_failures={failures}")
    sys.exit(1 if failures else 0)
