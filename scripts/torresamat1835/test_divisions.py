"""
TORRES-1835-DIVISIONS-105: detectar la frontera, aparte de numerarla.

    python3 test_divisions.py

Sin red. Geometría medida en el tomo 3 real; texto sintético, porque el
escaneo del testigo no se puede redistribuir.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import divisions
import structure
from divisions import Boundary
from layout import Column, Zone

W = 3402                     # ancho de plana medido en el tomo 3
CENTRED = (1300, 400, 2100, 470)
LEFTCOL = (400, 400, 1650, 470)
RIGHTCOL = (1750, 400, 3000, 470)


def judge(raw, column=Column.SPANNING, zone=Zone.BODY, bbox=CENTRED, **kw):
    return divisions.judge(raw_text=raw, column=column, zone=zone, bbox=bbox,
                           page_width=W, **kw)


# ---- A. Frontera real ---------------------------------------------------
def test_real_boundary_is_confirmed():
    for raw in ("CAPITULO XIV.", "SALMO XXIV", "CAPÍTULO VIII."):
        verdict = judge(raw)
        assert verdict.classification is Boundary.CONFIRMED, raw
        assert "division_word" in verdict.signals
        assert "spans_gutter" in verdict.signals


# ---- B/C/D. Falsos positivos rechazados --------------------------------
def test_running_header_is_not_a_boundary():
    """El caso real de Song p.317 y Sir p.393: la cabecera repite el
    capítulo y se contaba como división."""
    # Dos vías, las dos reales en el tomo. La cabecera se compone en caja
    # de título («Capitulo VIII.»), que el detector de rótulos ya no
    # acepta porque el impreso pone la división en versales.
    title_case = judge("Capitulo VIII.", zone=Zone.HEADER)
    assert title_case.classification is Boundary.REJECTED
    assert "no division marker" in title_case.rejections
    # Y si una cabecera sale en versales, la banda la rechaza igual.
    upper = judge("CAPITULO VIII.", zone=Zone.HEADER)
    assert upper.classification is Boundary.REJECTED
    assert any("running header" in r for r in upper.rejections)


def test_duplicate_on_the_same_page_is_rejected():
    """Sir p.393 traía el mismo rótulo dos veces en una plana."""
    verdict = judge("CAPITULO XIIL", seen_on_page={"CAPITULO XIIL"})
    assert verdict.classification is Boundary.REJECTED
    assert any("duplicate" in r for r in verdict.rejections)


def test_single_column_block_is_not_a_boundary():
    for column, bbox in ((Column.LEFT, LEFTCOL), (Column.RIGHT, RIGHTCOL)):
        verdict = judge("CAPITULO IX", column=column, bbox=bbox)
        assert verdict.classification is Boundary.REJECTED
        assert any("single column" in r for r in verdict.rejections)


# ---- E. Regresión: el rótulo no aporta numeral --------------------------
def test_division_word_never_contributes_a_roman():
    """«CAPITULO» trae C y L; «SALMO» trae L y M. Contarlos inventaba
    capítulos: CAPITULO CL se leía como el capítulo 1."""
    for word, book in (("CAPITULO", "Eccl"), ("SALMO", "Ps"),
                       ("CAPITULO:", "Prov")):
        resolution = structure.resolve_chapter(
            raw_numeral=word, header_chapters=[], previous=None, book=book)
        assert resolution.parsed_candidate is None, word
        assert resolution.resolved is None, word
        assert resolution.review_required, word
    # y el candidato absurdo tampoco pasa
    over = structure.resolve_chapter(raw_numeral="CAPITULO CL",
                                     header_chapters=[], previous=None,
                                     book="Eccl")
    assert over.resolved is None


# ---- F/G. Frontera sin número --------------------------------------------
def test_boundary_survives_a_broken_or_absent_numeral():
    for raw in ("SALMO LIM", "CAPITULO XXVHulI", "CAPITULO:", "SALMO IL",
                "SALMO VIL", "CAPITULO"):
        verdict = judge(raw)
        assert verdict.is_boundary, raw
    # detectar no es numerar: el número puede quedarse sin resolver
    lost = structure.resolve_chapter(raw_numeral="CAPITULO XXVHulI",
                                     header_chapters=[], previous=None,
                                     book="Sir")
    assert lost.resolved is None and lost.review_required


# ---- H/I. Evidencia secundaria ------------------------------------------
def test_secondary_evidence_never_creates_a_boundary():
    """Reinicio de versículo y cabecera ayudan; solas no bastan."""
    for kw in ({"verse_restart": True}, {"header_chapters": [12]},
               {"verse_restart": True, "header_chapters": [12]}):
        verdict = judge("Texto corrido sin rotulo alguno", **kw)
        assert verdict.classification is Boundary.REJECTED, kw
        assert "no division marker" in verdict.rejections
    # y sobre un rótulo real, suman
    plain = judge("CAPITULO XIV.")
    helped = judge("CAPITULO XIV.", verse_restart=True, header_chapters=[14])
    assert helped.score >= plain.score
    assert "verse_restart" in helped.signals
    assert "running_header" in helped.signals


# ---- J/K/L/M. Aislamiento ----------------------------------------------
def test_apparatus_header_footer_and_ordinary_blocks_are_isolated():
    assert judge("CAPITULO IX", zone=Zone.APPARATUS).classification \
        is Boundary.REJECTED
    assert judge("CAPITULO IX", zone=Zone.FOOTER).classification \
        is Boundary.REJECTED
    # Un argumento del editor cruza el canal igual que una división, pero
    # no lleva rótulo: no es frontera.
    editorial = judge("Argumento que el editor pone bajo la division.")
    assert editorial.classification is Boundary.REJECTED
    assert "no division marker" in editorial.rejections
    # Una inscripción canónica va dentro de su columna y su versículo.
    title = judge("Inscripcion del salmo", column=Column.RIGHT, bbox=RIGHTCOL)
    assert title.classification is Boundary.REJECTED


# ---- Canon como señal, nunca como autoridad ----------------------------
def test_canon_limit_flags_but_does_not_delete():
    assert divisions.exceeds_book(59, structure.chapter_limit("Sir")) is True
    assert divisions.exceeds_book(9, structure.chapter_limit("Song")) is True
    assert divisions.exceeds_book(8, structure.chapter_limit("Song")) is False
    assert divisions.exceeds_book(None, 51) is False
    # Pasarse del canon no convierte el bloque en «no frontera»: lo que
    # queda en duda es el número, no la frontera.
    verdict = judge("CAPITULO LIX")
    assert verdict.is_boundary


# ---- Q/R/S. Provenance, determinismo, offline --------------------------
def test_verdict_keeps_its_evidence():
    verdict = judge("CAPITULO XIV.", header_chapters=[14], verse_restart=True)
    assert verdict.signals and verdict.score > 0
    rejected = judge("CAPITULO XIV.", zone=Zone.HEADER)
    assert rejected.rejections


def test_deterministic_and_offline():
    one = judge("SALMO LIM", header_chapters=[53])
    two = judge("SALMO LIM", header_chapters=[53])
    assert (one.classification, one.signals, one.score) == \
           (two.classification, two.signals, two.score)
    with open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "divisions.py"), encoding="utf-8") as handle:
        text = handle.read()
    assert "urllib" not in text and "requests" not in text


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
    print(f"torresamat1835_divisions_failures={failures}")
    sys.exit(1 if failures else 0)
