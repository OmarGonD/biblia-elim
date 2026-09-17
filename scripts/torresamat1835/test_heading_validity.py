"""
TORRES-1835-FALSE-HEADING-CLAIMS-116: casar la palabra no es ser un rótulo.

    python3 test_heading_validity.py

Sin red y sin el testigo. El caso que motivó la task -- la inscripción
«Salmo de David, cuando le perseguía su hijo Absalón» de la plana 196,
aceptada como Ps 1 -- vive aquí como fixture con su texto y su geometría
literales: el código de producción no nombra ninguna plana.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chapter_claims as cc
import heading_validity as hv
import image_reviews as ir
import page_parser
import recovery
import roman
import source_ocr
from layout import Column, Zone
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690
PAGE_WIDTH = 3402
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")

#: Un rótulo del impreso: cruza el canal, centrado sobre él.
HEADING_BBOX = (1205, 932, 2118, 995)
#: Una línea de la columna española: encerrada en su columna.
COLUMN_BBOX = (1799, 2648, 3041, 2723)


def _judge(raw, *, column=Column.SPANNING, zone=Zone.BODY, bbox=None,
           review=None):
    return hv.judge(raw_text=raw, column=column, zone=zone,
                    bbox=bbox or HEADING_BBOX, page_width=PAGE_WIDTH,
                    image_review_outcome=review)


# ======================================================================
# A. Un rótulo de verdad se acepta -- en Salmos y en cualquier libro
# ======================================================================
def test_A_a_real_structural_heading_is_a_heading():
    for raw in ("SALMO PRIMERO.", "SALMO CXLII.", "CAPÍTULO XXV.",
                "CAPITULO PRIMERO.", "SALMO CL."):
        evidence = _judge(raw)
        assert evidence.is_heading, raw
        assert not evidence.review_required, raw
        assert not evidence.rejections, (raw, evidence.rejections)
        assert evidence.source == hv.FROM_STRUCTURE

    # y los rótulos que el reconocimiento dejó maltrechos siguen siéndolo
    for raw in ("SALMO CXLIL", "'; :/ SALMO c XX VII I.", "SALMO LVfir; '•",
                "-- SALMO XXXIt", "CAPÍTULO X«.", "SALMO ex."):
        assert _judge(raw).is_heading, raw


# ======================================================================
# B/C/D. Prosa que contiene la palabra, y hasta un romano impecable
# ======================================================================
def test_B_prose_carrying_the_word_and_a_numeral_is_not_a_heading():
    """La inscripción de la plana 196, con su geometría y su texto."""
    evidence = _judge("Salmo de Bavíd 1 enando Je persegnia",
                      column=Column.RIGHT, bbox=COLUMN_BBOX)
    assert not evidence.is_heading
    assert not evidence.review_required
    # y se dice TODO lo que lo sostiene, no sólo la primera comprobación
    assert len(evidence.rejections) >= 3, evidence.rejections
    assert any("single column" in r for r in evidence.rejections)
    assert any("reads as prose" in r for r in evidence.rejections)
    assert any("lower case" in r for r in evidence.rejections)


def test_C_a_sentence_mentioning_capitulo_does_not_become_a_heading():
    for raw in ("CAPÍTULO XXV de Isaías, donde el Señor prepara un convite",
                "véase el SALMO L, que es de David",
                "SALMO XXX. 14 hare yo, dice el Señor, con los que me temen",
                "SALMO DE DAVID 1 CUANDO LE PERSEGUIA SU HIJO ABSALON"):
        evidence = _judge(raw)
        assert not evidence.is_heading, raw
        assert len(evidence.rejections) >= 2, (raw, evidence.rejections)


def test_D_a_valid_roman_numeral_cannot_make_prose_a_heading():
    """El numeral está bien escrito y no basta: la frase sigue siendo frase."""
    raw = "CAPÍTULO XXV de Isaías, donde el Señor prepara un convite"
    shape = hv.shape_of(raw)
    assert shape["numeral_after_marker"] == 25      # se lee sin dudas
    assert roman.to_int("XXV") == 25
    assert not _judge(raw).is_heading
    # el numeral sí cuenta como señal a favor; lo que no puede es mandar
    assert "numeral_adjacent" in _judge(raw).signals


def test_the_geometry_is_not_measured_twice():
    """La composición la juzga divisions.judge; aquí no hay otra medida."""
    import ast
    tree = ast.parse(open(os.path.join(DIR, "heading_validity.py"),
                          encoding="utf-8").read())
    imported = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imported |= {a.name for a in node.names}
        elif isinstance(node, ast.ImportFrom):
            imported.add(node.module or "")
    assert "divisions" in imported
    # ni el canon ni la secuencia entran en esta decisión
    assert "canon" not in imported
    assert "structure" not in imported
    assert "roman" not in imported


def test_a_heading_outside_the_body_or_inside_one_column_is_rejected():
    for column, zone in ((Column.RIGHT, Zone.BODY),
                         (Column.LEFT, Zone.BODY),
                         (Column.SPANNING, Zone.APPARATUS),
                         (Column.SPANNING, Zone.HEADER),
                         (Column.UNKNOWN, Zone.BODY)):
        evidence = _judge("SALMO CXLII.", column=column, zone=zone,
                          bbox=COLUMN_BBOX)
        assert not evidence.is_heading, (column, zone)


# ======================================================================
# E/F/G/H. Lo que el ledger hace con un rótulo falso
# ======================================================================
def _page(lines, *, scan_page=10):
    """`lines` es [(texto, bbox, columna)]; se respeta el orden dado."""
    out = [{"text": "LIBRO DE LOS SALMOS. 72", "bbox": [1697, 195, 2117, 255]}]
    for text, bbox, _column in lines:
        out.append({"text": text, "bbox": list(bbox)})
    return {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                       "height": 4837, "lines": out}]}


def _blocks(fixture, texts):
    page = fixture["pages"][0]
    return {line["text"]: f"p{page['scan_page']:04d}l{index:04d}"
            for index, line in enumerate(page["lines"])
            if line["text"] in texts}


def _parse(fixture, *, reviews=(), book="Ps"):
    return page_parser.parse_volume(
        source_ocr.pages_from_fixture(copy.deepcopy(fixture)),
        witness="fx", volume="3", book=book, gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        numeral_reviews={r.target_block: r for r in reviews})


#: Un rótulo real, su argumento, y versículos en la columna española.
def _psalm(number_text, *, y=600, verses=("1 Primer verso.",)):
    lines = [(number_text, (1205, y, 2118, y + 63), "spanning"),
             ("Argumento del editor sobre este salmo.",
              (662, y + 120, 2661, y + 200), "spanning")]
    for index, verse in enumerate(verses):
        top = y + 260 + index * 90
        lines.append((verse, (1799, top, 3041, top + 70), "right"))
    return lines


def test_E_sequence_cannot_turn_a_false_heading_into_a_chapter():
    """Lo que pasaba en la plana 196: la cabecera y la secuencia decían 1."""
    fixture = _page(_psalm("SALMO CXLI.", y=600)
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1100, 3041, 1175), "right")])
    blocks = _blocks(fixture, {"Salmo de Bavíd 1 enando Je persegnia",
                               "SALMO CXLI."})
    _ed, _s, walker = _parse(fixture)
    claims = {c.block_id: c for c in walker.ledger.claims}
    false_claim = claims[blocks["Salmo de Bavíd 1 enando Je persegnia"]]

    assert false_claim.disposition == cc.REJECTED_FALSE_HEADING
    assert false_claim.accepted_number is None
    assert not false_claim.is_structural_heading
    # el reclamo NO desaparece: conserva todo lo que hace falta para
    # auditarlo
    assert false_claim.raw_heading == "Salmo de Bavíd 1 enando Je persegnia"
    assert false_claim.bbox == (1799, 1100, 3041, 1175)
    assert false_claim.block_id and false_claim.scan_page == 10
    assert false_claim.heading["rejections"]
    assert false_claim.reason
    # 1 no se le da a nadie por secuencia
    assert 1 not in {c.accepted_number for c in walker.ledger.accepted()}


def test_F_a_false_heading_cannot_anchor_the_sequence():
    """Un rótulo falso con numeral válido tampoco sostiene al siguiente."""
    fixture = _page(
        [("SALMO L de los que esperan en el Señor, dice David",
          (662, 600, 2661, 680), "spanning")]
        + _psalm("SALMO LI.", y=800))
    blocks = _blocks(fixture, {"SALMO L de los que esperan en el Señor, "
                               "dice David", "SALMO LI."})
    _ed, _s, walker = _parse(fixture)
    claims = {c.block_id: c for c in walker.ledger.claims}
    false_claim = claims[blocks["SALMO L de los que esperan en el Señor, "
                                "dice David"]]
    later = claims[blocks["SALMO LI."]]

    assert false_claim.disposition == cc.REJECTED_FALSE_HEADING
    # el siguiente no se apoya en él: ni como ancla ni como número
    assert later.anchor_claim != false_claim.claim_id
    assert later.anchor_number != 50
    for claim in walker.ledger.accepted():
        assert claim.anchor_claim != false_claim.claim_id
        assert claim.is_structural_heading


def test_G_a_false_heading_does_not_take_the_number_of_a_real_one():
    """Los dos reclamos se conservan; sólo el rótulo se queda el número."""
    fixture = _page(_psalm("SALMO I.", y=600)
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1100, 3041, 1175), "right")])
    blocks = _blocks(fixture, {"SALMO I.",
                               "Salmo de Bavíd 1 enando Je persegnia"})
    edition, _s, walker = _parse(fixture)
    claims = {c.block_id: c for c in walker.ledger.claims}
    real = claims[blocks["SALMO I."]]
    false_claim = claims[blocks["Salmo de Bavíd 1 enando Je persegnia"]]

    # los dos reclamantes existen antes de resolver: eso es lo que
    # permite ver el conflicto en vez de esconderlo
    assert len(walker.ledger.claims) == 2
    assert real.disposition == cc.ACCEPTED and real.accepted_number == 1
    assert false_claim.disposition == cc.REJECTED_FALSE_HEADING
    assert len(walker.ledger.collisions()) == 0
    # y sólo el rótulo llega a la estructura
    assert 1 in edition.books["Ps"].chapters
    assert false_claim.slot is None


def test_H_a_real_claimant_survives_a_false_one_on_the_same_number():
    fixture = _page(
        [("Salmo de Bavíd 1 enando Je persegnia",
          (1799, 400, 3041, 475), "right")]
        + _psalm("SALMO I.", y=600))
    _ed, _s, walker = _parse(fixture)
    accepted = walker.ledger.accepted()
    assert [c.accepted_number for c in accepted] == [1]
    assert accepted[0].raw_heading == "SALMO I."
    assert accepted[0].disposition == cc.ACCEPTED


# ======================================================================
# I. La imagen manda sobre la clasificación automática
# ======================================================================
def test_I_an_image_confirmed_heading_stays_a_heading():
    """Una lectura del facsímil del MISMO bloque gana a la geometría."""
    confirmed = _judge("SALMO CXLII.", column=Column.RIGHT,
                       bbox=COLUMN_BBOX, review=True)
    assert confirmed.is_heading
    assert confirmed.source == hv.FROM_IMAGE_REVIEW
    assert not _judge("SALMO CXLII.", column=Column.RIGHT,
                      bbox=COLUMN_BBOX).is_heading, "sin la imagen, no"

    # y al revés: si el facsímil dice que ahí no hay rótulo, no lo hay
    denied = _judge("SALMO CXLII.", review=False)
    assert not denied.is_heading
    assert denied.source == hv.FROM_IMAGE_REVIEW

    # una revisión que sólo habla del NÚMERO no decide sobre el rótulo
    for outcome in (ir.UNREADABLE, ir.STILL_AMBIGUOUS):
        review = ir.NumeralReview(
            id="x", book="Ps", scan_page=10, target_block="p0010l0003",
            outcome=outcome, raw_heading="SALMO CXLII.", raw_numeral=None,
            observed_printed_text=None, observed_printed_numeral=None,
            recovered_chapter=None, confidence=0.5, rationale="r",
            reviewer_method="render")
        assert page_parser._heading_says(review) is None, outcome
    resolving = ir.NumeralReview(
        id="x", book="Ps", scan_page=10, target_block="p0010l0003",
        outcome=ir.NUMERAL_CORRECTED, raw_heading="SALMO CXLIL",
        raw_numeral="CXLIL", observed_printed_text="SALMO CXLII.",
        observed_printed_numeral="CXLII", recovered_chapter=142,
        confidence=0.95, rationale="r", reviewer_method="render")
    assert page_parser._heading_says(resolving) is True
    false_claim = ir.NumeralReview(
        id="y", book="Ps", scan_page=10, target_block="p0010l0004",
        outcome=ir.FALSE_CLAIM, raw_heading="Salmo de David",
        raw_numeral="IC", observed_printed_text=None,
        observed_printed_numeral=None, recovered_chapter=None,
        confidence=0.95, rationale="r", reviewer_method="render")
    assert page_parser._heading_says(false_claim) is False


def test_a_numeral_review_of_another_block_says_nothing_about_this_one():
    """La autoridad de la imagen es del bloque mirado, no de su vecindad."""
    fixture = _page(_psalm("SALMO CXLII.", y=600)
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1100, 3041, 1175), "right")])
    blocks = _blocks(fixture, {"SALMO CXLII.",
                               "Salmo de Bavíd 1 enando Je persegnia"})
    review = ir.NumeralReview(
        id="ps-142", book="Ps", scan_page=10,
        target_block=blocks["SALMO CXLII."], outcome=ir.NUMERAL_CORRECTED,
        raw_heading="SALMO CXLII.", raw_numeral="CXLII",
        observed_printed_text="SALMO CXLII.",
        observed_printed_numeral="CXLII", recovered_chapter=142,
        confidence=0.95, rationale="leído en la plana",
        reviewer_method="render", bbox=HEADING_BBOX, pdf_page=11)
    _ed, _s, walker = _parse(fixture, reviews=[review])
    claims = {c.block_id: c for c in walker.ledger.claims}
    assert claims[blocks["SALMO CXLII."]].accepted_number == 142
    neighbour = claims[blocks["Salmo de Bavíd 1 enando Je persegnia"]]
    assert neighbour.disposition == cc.REJECTED_FALSE_HEADING


# ======================================================================
# J. El crudo del reconocimiento no se toca
# ======================================================================
def test_J_raw_ocr_is_immutable():
    fixture = _page(_psalm("SALMO CXLII.", y=600)
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1100, 3041, 1175), "right")])
    before = copy.deepcopy(fixture)
    edition, _s, walker = _parse(fixture)
    assert fixture == before
    rejected = next(c for c in walker.ledger.claims
                    if c.disposition == cc.REJECTED_FALSE_HEADING)
    assert rejected.raw_heading == "Salmo de Bavíd 1 enando Je persegnia"
    # y la línea sale a la cola de revisión con su texto entero
    queued = [b for b in edition.review_queue
              if b.raw_text == "Salmo de Bavíd 1 enando Je persegnia"]
    assert len(queued) == 1
    assert queued[0].kind is BlockKind.UNCLASSIFIED
    assert "not a chapter heading" in queued[0].review_reason


def test_the_rejected_line_is_never_attached_to_the_previous_verse():
    """El fallo de 1882 con otro disfraz: pegarlo al versículo de antes."""
    fixture = _page(_psalm("SALMO CXLI.", y=600,
                           verses=("1 Primer verso.", "2 Segundo verso."))
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1200, 3041, 1275), "right")])
    edition, _s, _w = _parse(fixture)
    for chapter in edition.books["Ps"].chapters.values():
        for verse in chapter.verses.values():
            for block in verse.blocks:
                assert "Bavíd" not in block.raw_text


# ======================================================================
# K/L/M. Las regresiones del tomo real, medidas sobre el audit
# ======================================================================
def _audit():
    path = os.path.join(DIR, "..", "..", "build", "torresamat1835-audit",
                        "volume3.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def test_K_the_false_psalm_one_of_the_volume_is_rejected():
    report = _audit()
    if report is None:
        print("  (saltado: no hay audit en build/)")
        return
    section = report["heading_claim_validation"]
    rows = {r["block_id"]: r for r in section["false_headings"]}
    assert "p0196l0050" in rows, sorted(rows)
    row = rows["p0196l0050"]
    assert row["book"] == "Ps"
    assert row["accepted_number"] is None
    assert row["disposition"] == cc.REJECTED_FALSE_HEADING
    # la política de numeración le habría dado el 1: eso es el daño
    # que la validación evita, y queda escrito
    assert row["number_the_policy_would_have_given"] == 1
    assert row["raw_heading"].startswith("Salmo de")
    assert row["heading_evidence"]["rejections"]
    assert row["bbox"] and row["column"] == "right"
    # y ningún reclamo aceptado se queda con Ps 1 por su culpa
    claims = report["chapter_claims"]["claims"]
    ones = [c for c in claims
            if c["book"] == "Ps" and c["accepted_number"] == 1]
    assert not [c for c in ones if c["block_id"] == "p0196l0050"]


def test_L_the_true_psalm_one_is_preserved_as_its_own_claim():
    report = _audit()
    if report is None:
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    real = claims["p0015l0003"]
    assert real["raw_heading"].upper().startswith("SALMO PRIMERO")
    assert real["column"] == "spanning"
    # sigue siendo un reclamo vivo y estructuralmente válido...
    assert real["heading"]["is_heading"] is True
    assert real["disposition"] != cc.REJECTED_FALSE_HEADING
    # ...y NO se le regala el número por haber caído el falso: su
    # numeral es la palabra «PRIMERO», que el lector de romanos no lee
    if real["accepted_number"] is None:
        assert real["numeral"]["status"] == "no_numeral"


def test_M_verse_ownership_around_the_false_heading_is_accounted_for():
    """Los versículos del salmo 142 son del salmo 142."""
    report = _audit()
    if report is None:
        return
    # nada se pierde en silencio: el informe separa lo materializado de
    # lo que sigue en un hueco de trabajo
    assert report["materialized_verse_refs"] + \
        report["verse_refs_in_review_slots"] == report["verse_refs"]
    assert report["materialized_verse_refs"] > 0
    chapters = report["structure_resolution"]["per_book"]["Ps"]
    assert chapters["verses"] > 0
    # el rótulo de la plana 196 se acepta y es el que tiene el número
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    assert claims["p0196l0047"]["accepted_number"] == 142
    assert claims["p0196l0047"]["disposition"] == cc.ACCEPTED


def test_N_the_numeral_reviews_of_the_previous_batches_are_unaffected():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    assert nr["schema_problems"] == []
    assert nr["reviews_attempted"] == 113
    assert nr["reviews_resolved"] == 111
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for review in ir.load()["numeral_reviews"]:
        claim = claims.get(review["target_block"])
        assert claim is not None, review["id"]
        if review["outcome"] in ir.NUMERAL_RESOLVING:
            assert claim["accepted_number"] == review["recovered_chapter"], \
                review["id"]
            assert claim["heading"]["is_heading"] is True, review["id"]
        else:
            assert claim["accepted_number"] is None, review["id"]


def test_OP_the_volume_keeps_its_reference_invariants():
    report = _audit()
    if report is None:
        return
    assert report["duplicate_refs"] == []
    assert report["out_of_order_refs"] == []
    assert report["metrics"]["ocr_blocks"] == 57700
    section = report["heading_claim_validation"]
    assert section["total_claims_checked"] == section["claims_evaluated"]
    assert section["false_heading_count"] == len(section["false_headings"])


def test_a_rejected_false_heading_is_out_of_the_numeral_queues():
    report = _audit()
    if report is None:
        return
    rejected = {r["block_id"]
                for r in report["heading_claim_validation"]["false_headings"]}
    nr = report["numeral_image_review"]
    queued = {e["block_id"] for e in nr["review_queue_next"]}
    assert not (rejected & queued)
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for block in rejected:
        assert claims[block]["disposition"] == cc.REJECTED_FALSE_HEADING
        # no cuenta como numeral pendiente: no hay numeral que recuperar
        assert claims[block]["accepted_number"] is None


# ======================================================================
# Q/R. Determinista y sin red; y sin ninguna plana escrita en el código
# ======================================================================
def test_Q_deterministic():
    fixture = _page(_psalm("SALMO CXLII.", y=600)
                    + [("Salmo de Bavíd 1 enando Je persegnia",
                        (1799, 1100, 3041, 1175), "right")])

    def run():
        _ed, _s, walker = _parse(fixture)
        return json.dumps(walker.ledger.report(), sort_keys=True, default=str)
    assert run() == run()

    raw = "Salmo de Bavíd 1 enando Je persegnia"
    assert _judge(raw).as_dict() == _judge(raw).as_dict()


def test_R_offline_and_no_page_or_book_hardcoded_in_production():
    import ast
    source = open(os.path.join(DIR, "heading_validity.py"),
                  encoding="utf-8").read()
    for forbidden in ("urllib", "requests", "http://", "https://", "socket"):
        assert forbidden not in source, forbidden

    tree = ast.parse(source)
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef,
                             ast.AsyncFunctionDef)):
            doc = ast.get_docstring(node, clean=False)
            if doc is not None:
                docstrings.add(doc)
    # Las docstrings documentan el caso real y no son lógica; lo que no
    # puede haber es una plana, un libro o un texto del testigo metidos
    # en una comprobación.
    literals = [n.value for n in ast.walk(tree)
                if isinstance(n, ast.Constant)
                and not (isinstance(n.value, str) and n.value in docstrings)]
    numbers = [l for l in literals if isinstance(l, (int, float))
               and not isinstance(l, bool)]
    for page in (15, 64, 101, 133, 135, 190, 196):
        assert page not in numbers, page
    texts = [l for l in literals if isinstance(l, str)]
    for book in ("Ps", "Prov", "Eccl", "Song", "Wis", "Sir", "Isa"):
        assert book not in texts, book
    for word in ("Bav", "Absalon", "David", "Salmo de"):
        assert not any(word in t for t in texts), word

    # el parser tampoco puede nombrar la plana del caso
    parser_source = open(os.path.join(DIR, "page_parser.py"),
                         encoding="utf-8").read()
    for marker in ("p0196", "0196l0050", "Salmo de Bav"):
        assert marker not in parser_source, marker


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
    print(f"torresamat1835_heading_validity_failures={failures}")
    sys.exit(1 if failures else 0)
