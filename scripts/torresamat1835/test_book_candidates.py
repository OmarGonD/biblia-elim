"""
TORRES-1835-BOOK-CANDIDATE-108: el candidato de apertura de libro espera.

    python3 test_book_candidates.py

Tres estados separados: candidato detectado, identidad confirmada,
frontera fijada. Sin red; geometría medida en el tomo 3 real.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import book_boundaries as bb
import structure
from layout import Column, PlacedLine, Zone
from source_ocr import SourceLine, SourcePage, SourceWord

W = 3402
CENTRED = (520, 2880)
TITLE = "LIBRO DE LA SABIDURIA"          # versales, como el impreso


def placed(index, text, x0=CENTRED[0], x1=CENTRED[1],
           column=Column.SPANNING, zone=Zone.BODY, y=600):
    word = SourceWord(text, (x0, y, x1, y + 80), 50)
    line = SourceLine(index, [word], (x0, y, x1, y + 80))
    return PlacedLine(line=line, column=column, zone=zone)


def page(number, blocks):
    return SourcePage(scan_page=number, width=W, height=4837, dpi=600), blocks


def run(pages, spans):
    return bb.resolve_with_candidates(spans, lambda: iter(pages))


def two_books(first=("Song", 300), second=("Wis", 340)):
    return [structure.BookSpan(first[0], first[1]),
            structure.BookSpan(second[0], second[1])]


# ---- A. El candidato aguanta mucho más de 10 planas --------------------
def test_candidate_survives_a_long_confirmation_delay():
    """El caso real Sir->Isa: título en la 497, confirmación en la 538.

    Con la ventana fija de 10 planas que había antes, este candidato se
    habría descartado por antigüedad y la frontera habría quedado junto a
    la confirmación. Aquí sobrevive.
    """
    spans = two_books(("Song", 300), ("Wis", 341))
    pages = [page(301, [placed(1, TITLE)])]
    pages += [page(n, []) for n in range(302, 342)]
    decisions, tracker = run(pages, spans)
    assert len(decisions) == 1
    assert decisions[0].effective_page == 301
    assert decisions[0].confirmation_page == 341
    assert decisions[0].confirmation_page - decisions[0].effective_page == 40
    assert decisions[0].review_required is False
    assert spans[1].first_page == 301
    assert all(c.disposition == "confirmed" for c in tracker.candidates)


def test_no_fixed_lookback_remains_in_the_boundary_code():
    """Nada puede descartar un candidato sólo por su edad en planas."""
    source = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "book_boundaries.py"), encoding="utf-8").read()
    assert "LOOKBACK_PAGES" not in source
    # Lo único que queda es adyacencia local entre bloques del mismo
    # frontmatter, y es pequeña.
    assert bb.CLUSTER_GAP_PAGES <= 5


# ---- B/C/D. Detección, confirmación y fijación son tres cosas ----------
def test_candidate_does_not_switch_the_book_by_itself():
    spans = two_books(("Song", 300), ("Wis", 320))
    tracker = bb.BookCandidateTracker()
    found = tracker.observe_page(*page(305, [placed(1, TITLE)]),
                                 current_book="Song", current_book_started=300)
    assert len(found) == 1
    assert found[0].disposition == "pending"
    assert spans[1].first_page == 320, "el tramo no se mueve al detectar"


def test_identity_still_needs_three_coherent_headers():
    base = [structure.HeaderReading(n, "LOS SALMOS", book="Ps")
            for n in range(6)]
    for extra, expected in ((1, ["Ps"]), (2, ["Ps"]), (3, ["Ps", "Prov"])):
        readings = base + [structure.HeaderReading(6 + i, "x", book="Prov")
                           for i in range(extra)]
        assert [s.osis for s in structure.book_spans(readings)] == expected


def test_confirmation_finalises_the_pending_candidate():
    spans = two_books(("Song", 300), ("Wis", 310))
    pages = [page(303, [placed(1, TITLE)])] + \
        [page(n, []) for n in range(304, 311)]
    decisions, _tracker = run(pages, spans)
    assert decisions[0].effective_block == "p0303l0001"
    assert "held pending for 7 pages" in " ".join(decisions[0].evidence)


# ---- El hallazgo a preservar: cabecera ruidosa ≠ candidato incompatible -
def test_noisy_headers_never_cancel_a_pending_candidate():
    """Entre las planas 498 y 533 del tomo las cabeceras se leen
    alternativamente como Song, Eccl e Isa. Ninguna de esas lecturas es
    evidencia estructural de apertura de libro, y por tanto ninguna puede
    tumbar el candidato ya visto."""
    spans = two_books(("Song", 300), ("Wis", 330))
    noisy = []
    for number, text in ((306, "LOS SALMOS"), (309, "ECLESIASTES"),
                         (312, "LA SABIDURIA"), (318, "CANTARES")):
        noisy.append(page(number, [placed(1, text, column=Column.SPANNING,
                                          zone=Zone.HEADER)]))
    pages = [page(303, [placed(1, TITLE)])] + noisy + \
        [page(n, []) for n in range(319, 331)]
    decisions, tracker = run(pages, spans)
    assert decisions[0].effective_page == 303
    assert decisions[0].review_required is False
    survivors = [c for c in tracker.candidates if c.disposition == "confirmed"]
    assert len(survivors) == 1
    assert not any(c.disposition == "rejected" for c in tracker.candidates)


def test_only_a_comparable_candidate_can_cancel_one():
    """Un candidato fuerte a un libro posterior sí lo invalida."""
    spans = two_books(("Song", 300), ("Wis", 330))
    pages = [page(303, [placed(1, TITLE)]),
             page(310, [placed(1, "LIBRO DEL ECCLESIASTICO")])] + \
        [page(n, []) for n in range(311, 331)]
    _decisions, tracker = run(pages, spans)
    rejected = [c for c in tracker.candidates if c.disposition == "rejected"]
    assert rejected
    assert rejected[0].to_book == "Wis"
    assert "stronger candidate for Sir" in rejected[0].rejection_reason


# ---- I. Un candidato falso no mueve nada sin confirmación --------------
def test_a_lone_candidate_without_confirmation_changes_nothing():
    spans = [structure.BookSpan("Song", 300)]
    pages = [page(305, [placed(1, TITLE)])] + \
        [page(n, []) for n in range(306, 320)]
    decisions, tracker = run(pages, spans)
    assert decisions == []
    assert spans[0].first_page == 300
    assert all(c.disposition == "pending" for c in tracker.candidates)


def test_sentence_case_argument_is_not_a_book_title():
    """«Profetiza Isaías contra una nación que no nombra» cruza el canal,
    va centrado y es corto, pero es el argumento del editor: va en caja
    de frase, no en versales. Era lo que anclaba Isaías en la 534."""
    argument = "Profetiza Isaias contra una nacion que no nombra"
    assert bb.is_book_title_block(placed(1, argument), W, book="Isa") == 0.0
    assert bb.is_book_title_block(placed(1, "LA profecia de ISAIAS."), W,
                                  book="Isa") > 0.0


def test_volume_front_page_cannot_open_a_later_book():
    """La portada del tomo nombra los siete libros de una vez."""
    tracker = bb.BookCandidateTracker()
    found = tracker.observe_page(
        *page(5, [placed(1, "LOS SALMOS Y LA PROFECIA DE ISAIAS")]),
        current_book="Ps", current_book_started=14)
    assert found == [], "una plana anterior al libro en curso no cuenta"


# ---- M. El canon no participa ------------------------------------------
def test_canon_limits_are_not_consulted():
    source = open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "book_boundaries.py"), encoding="utf-8").read()
    for token in ("chapter_limit", "POR_OSIS", "vulg", "canon"):
        assert token not in source, token


# ---- O/Q/R/S. Pertenencia, provenance, determinismo, offline -----------
def test_spans_stay_disjoint():
    spans = two_books(("Song", 300), ("Wis", 330))
    pages = [page(303, [placed(1, TITLE)])] + \
        [page(n, []) for n in range(304, 331)]
    run(pages, spans)
    assert spans[0].last_page == 302
    assert spans[1].first_page == 303
    for number in range(300, 335):
        owners = [s.osis for s in spans if s.first_page <= number and
                  (s.last_page is None or number <= s.last_page)]
        assert len(owners) == 1, (number, owners)


def test_candidate_keeps_its_provenance():
    tracker = bb.BookCandidateTracker()
    found = tracker.observe_page(*page(319, [placed(7, TITLE)]),
                                 current_book="Song", current_book_started=300)
    candidate = found[0]
    assert candidate.block_id == "p0319l0007"
    assert candidate.bbox and len(candidate.bbox) == 4
    assert candidate.raw_text and candidate.evidence
    assert candidate.score > 0
    assert candidate.from_book == "Song"


def test_deterministic_and_offline():
    def once():
        spans = two_books(("Song", 300), ("Wis", 330))
        pages = [page(303, [placed(1, TITLE)])] + \
            [page(n, []) for n in range(304, 331)]
        decisions, _t = run(pages, spans)
        return [(d.to_book, d.effective_page, d.confidence) for d in decisions]
    assert once() == once()
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
    print(f"torresamat1835_book_candidates_failures={failures}")
    sys.exit(1 if failures else 0)
