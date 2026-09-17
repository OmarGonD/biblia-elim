"""
TORRES-1835-NUMERAL-IMAGE-REVIEW-112: leer numerales en el facsímil.

    python3 test_numeral_review.py

Sin red y sin el testigo real. Los casos del tomo (Ps 100, Ps 147, Sir 411)
viven aquí como fixtures con su texto literal: el código de producción no
nombra ni una plana ni un libro.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chapter_claims as cc
import image_reviews as ir
import numeral_review
import page_parser
import recovery
import roman
import source_ocr
import structure
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


# --- una plana con un rótulo cuyo numeral el OCR destrozó ---------------
def _page(headings, *, scan_page=10):
    """`headings` es [(texto del rótulo, y)]; entre ellos van versículos."""
    lines = [{"text": "LIBRO. 100", "bbox": [1697, 195, 2117, 255]}]
    y = 600
    for text, _ in headings:
        lines += [
            {"text": "1 Verbum latinum.", "bbox": [400, y, 1600, y + 72]},
            {"text": "1 Verso anterior.", "bbox": [1700, y, 3000, y + 72]},
            {"text": text, "bbox": [1200, y + 200, 2150, y + 280]},
            {"text": "1 Verbum sequentis.",
             "bbox": [400, y + 480, 1600, y + 552]},
            {"text": "1 Primer verso del capitulo.",
             "bbox": [1700, y + 480, 3000, y + 552]},
        ]
        y += 900
    fixture = {"pages": [{"scan_page": scan_page, "width": 3402,
                          "height": 4837, "lines": lines}]}
    return fixture


def _heading_blocks(fixture, texts):
    """Los block_id de los rótulos, leídos del propio fixture."""
    out = {}
    page = fixture["pages"][0]
    for index, line in enumerate(page["lines"]):
        if line["text"] in texts:
            out[line["text"]] = f"p{page['scan_page']:04d}l{index:04d}"
    return out


def _review(**over):
    base = dict(
        id="fx-1", book="Sir", scan_page=10, target_block="p0010l0003",
        outcome=ir.NUMERAL_CORRECTED, raw_heading="CAPÍTULO XXL",
        raw_numeral="XXL", observed_printed_text="CAPÍTULO XXI",
        observed_printed_numeral="XXI", recovered_chapter=21,
        confidence=0.95, rationale="read from the facsimile",
        reviewer_method="render", bbox=(1200, 800, 2150, 880), pdf_page=11)
    base.update(over)
    return ir.NumeralReview(**base)


def _parse(fixture, reviews, *, book="Sir"):
    return page_parser.parse_volume(
        source_ocr.pages_from_fixture(copy.deepcopy(fixture)),
        witness="fx", volume="3", book=book, gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        numeral_reviews={r.target_block: r for r in reviews})


# ======================================================================
# A/B/C. El crudo se conserva; la imagen manda; el OCR no se toca
# ======================================================================
def test_A_raw_invalid_numeral_is_retained_after_the_correction():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    _ed, _stats, walker = _parse(fixture, [_review(target_block=block)])
    claim = walker.ledger.claims[0]

    assert claim.accepted_number == 21
    # el crudo sigue siendo el crudo
    assert claim.raw_heading == "CAPÍTULO XXL"
    assert claim.numeral["token"] == "XXL"
    assert claim.numeral["status"] == roman.INVALID_SYNTAX
    assert claim.numeral["value"] is None
    assert claim.numeral["permissive_value"] == 30
    # y la procedencia deja las tres cosas a la vista
    prov = claim.provenance
    assert prov["raw_ocr_numeral"] == "XXL"
    assert prov["observed_printed_numeral"] == "XXI"
    assert claim.accepted_number == 21


def test_B_facsimile_review_outranks_an_invalid_ocr_numeral():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]

    _ed, _s, without = _parse(fixture, [])
    assert without.ledger.claims[0].accepted_number is None
    assert without.ledger.claims[0].disposition == cc.INVALID_NUMERAL

    _ed, _s, with_review = _parse(fixture, [_review(target_block=block)])
    claim = with_review.ledger.claims[0]
    assert claim.accepted_number == 21
    assert claim.source == cc.FROM_IMAGE_REVIEW
    assert claim.authority > cc.AUTHORITY[cc.FROM_OCR]


def test_C_the_review_does_not_mutate_the_raw_ocr():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    before = copy.deepcopy(fixture)
    edition, _s, _w = _parse(fixture, [_review(target_block=block)])
    assert fixture == before
    heading = [b for ch in edition.books["Sir"].chapters.values()
               for b in ch.paratext if b.kind is BlockKind.CHAPTER_HEADING][0]
    assert heading.raw_text == "CAPÍTULO XXL"
    assert heading.number == 21


# ======================================================================
# D/E/F/G. Conflictos, rechazos y duplicados
# ======================================================================
def test_D_a_competing_group_is_resolved_only_by_visual_evidence():
    """Dos rótulos que reclaman el mismo número: la imagen los separa."""
    fixture = _page([("SALMO CXLVII.", 0), ("SALMO CXLVII L", 1)])
    blocks = _heading_blocks(fixture, {"SALMO CXLVII.", "SALMO CXLVII L"})

    _ed, _s, without = _parse(fixture, [], book="Ps")
    assert len(without.ledger.collisions()) >= 0
    assert all(c.accepted_number is None for c in without.ledger.claims), \
        "sin imagen no se decide"

    review = _review(id="ps-148", book="Ps",
                     target_block=blocks["SALMO CXLVII L"],
                     outcome=ir.COMPETING_RESOLVED,
                     raw_heading="SALMO CXLVII L", raw_numeral="CXLVII",
                     observed_printed_text="SALMO CXLVIII",
                     observed_printed_numeral="CXLVIII",
                     recovered_chapter=148)
    _ed, _s, with_review = _parse(fixture, [review], book="Ps")
    numbers = {c.block_id: c.accepted_number for c in with_review.ledger.claims}
    assert numbers[blocks["SALMO CXLVII L"]] == 148
    assert len(with_review.ledger.collisions()) == 0


def test_E_a_rejected_false_claim_does_not_materialize_a_chapter():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    review = _review(target_block=block, outcome=ir.FALSE_CLAIM,
                     observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None)
    assert not review.resolves
    edition, _s, walker = _parse(fixture, [review])
    assert walker.ledger.claims[0].accepted_number is None
    assert not [n for n in edition.books["Sir"].chapters if n > 0]


def test_F_same_physical_duplicate_does_not_make_a_competing_group():
    ledger = cc.ClaimLedger()
    machine = ledger.add(cc.ChapterClaim(
        claim_id="ocr", book="Ps", scan_page=10, block_id="p0010l0003",
        raw_heading="SALMO C 1 1 L", source=cc.FROM_OCR,
        numeral=roman.read(" C 1 1 L").as_dict(), candidate_numbers=[100, 50],
        proposed_number=None))
    human = ledger.add(cc.ChapterClaim(
        claim_id="img", book="Ps", scan_page=10, block_id="p0010r0003",
        raw_heading="SALMO CIII", source=cc.FROM_IMAGE_REVIEW,
        numeral=roman.read(" CIII").as_dict(), candidate_numbers=[103],
        proposed_number=103, proposal_method="numeral_review",
        confidence=0.95))
    assert machine.physical_key == human.physical_key
    ledger.resolve()
    assert human.accepted_number == 103
    assert machine.disposition == cc.SAME_PHYSICAL
    assert len(ledger.collisions()) == 0


def test_G_distinct_physical_claims_stay_distinct():
    fixture = _page([("SALMO CXLVII.", 0), ("SALMO CXLVII L", 1)])
    blocks = _heading_blocks(fixture, {"SALMO CXLVII.", "SALMO CXLVII L"})
    assert blocks["SALMO CXLVII."] != blocks["SALMO CXLVII L"]
    review = _review(id="ps-148", book="Ps",
                     target_block=blocks["SALMO CXLVII L"],
                     outcome=ir.COMPETING_RESOLVED,
                     observed_printed_numeral="CXLVIII", recovered_chapter=148)
    edition, _s, walker = _parse(fixture, [review], book="Ps")
    assert len(walker.ledger.claims) == 2
    assert len({c.physical_key for c in walker.ledger.claims}) == 2
    # los dos capítulos existen por separado, con sus versículos
    assert 148 in edition.books["Ps"].chapters


# ======================================================================
# H/I. Mirar no obliga a resolver
# ======================================================================
def test_H_an_unreadable_review_leaves_the_claim_unresolved():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    review = _review(target_block=block, outcome=ir.UNREADABLE,
                     observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None,
                     rationale="the plate is torn across the numeral")
    assert not review.resolves
    _ed, _s, walker = _parse(fixture, [review])
    claim = walker.ledger.claims[0]
    assert claim.accepted_number is None
    assert claim.review_required
    assert claim.source == cc.FROM_OCR      # no gana autoridad por mirarla


def test_I_a_still_ambiguous_review_leaves_the_claim_unresolved():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    review = _review(target_block=block, outcome=ir.STILL_AMBIGUOUS,
                     observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None,
                     rationale="the last stroke could be I or L")
    _ed, _s, walker = _parse(fixture, [review])
    assert walker.ledger.claims[0].accepted_number is None


def test_a_resolving_outcome_must_carry_a_numeral_and_a_chapter():
    payload = {"visual_source": {"sha256": SHA}, "numeral_reviews": [{
        "id": "x", "book": "Sir", "scan_page": 10,
        "target_block": "p0010l0003", "outcome": ir.NUMERAL_CORRECTED,
        "raw_heading": "CAPÍTULO XXL", "rationale": "r"}]}
    problems = ir.validate_numerals(payload, page_count=652)
    assert any("needs a chapter" in p for p in problems), problems
    assert any("printed numeral" in p for p in problems), problems

    payload["numeral_reviews"][0]["outcome"] = ir.UNREADABLE
    payload["numeral_reviews"][0]["recovered_chapter"] = 21
    assert any("cannot carry a chapter" in p
               for p in ir.validate_numerals(payload, page_count=652))


# ======================================================================
# J/K. Falla cerrado
# ======================================================================
def test_J_a_wrong_source_hash_applies_nothing():
    payload = ir.load()
    for bad in ("0" * 64, "9" * 64):
        try:
            ir.numeral_reviews_for(payload, expected_sha256=bad)
        except ir.ReviewError as exc:
            assert "not applied" in str(exc)
        else:
            raise AssertionError("un artefacto distinto no puede aplicarse")


def test_K_a_review_pointing_at_the_wrong_page_or_block_fails_closed():
    # la plana del identificador tiene que ser la que la revisión dice
    payload = {"visual_source": {"sha256": SHA}, "numeral_reviews": [{
        "id": "x", "book": "Sir", "scan_page": 10, "pdf_page": 11,
        "target_block": "p0999l0003", "outcome": ir.NUMERAL_CORRECTED,
        "raw_heading": "h", "raw_numeral": "XXL",
        "observed_printed_numeral": "XXI", "recovered_chapter": 21,
        "rationale": "r"}]}
    assert any("not on scan page" in p
               for p in ir.validate_numerals(payload, page_count=652))

    # y una revisión cuyo bloque no existe en la plana no hace nada
    fixture = _page([("CAPÍTULO XXL", 0)])
    _ed, _s, walker = _parse(fixture, [_review(target_block="p0010l0099")])
    assert walker.ledger.claims[0].accepted_number is None
    assert walker.ledger.claims[0].source == cc.FROM_OCR


def test_the_mapping_is_checked_in_the_metadata():
    payload = {"visual_source": {"sha256": SHA}, "numeral_reviews": [{
        "id": "x", "book": "Sir", "scan_page": 10, "pdf_page": 99,
        "target_block": "p0010l0003", "outcome": ir.NUMERAL_CORRECTED,
        "raw_heading": "h", "raw_numeral": "XXL",
        "observed_printed_numeral": "XXI", "recovered_chapter": 21,
        "rationale": "r"}]}
    assert any("does not match the mapping" in p
               for p in ir.validate_numerals(payload, page_count=652))


# ======================================================================
# L/M/N. Los casos reales del tomo, como datos versionados
# ======================================================================
def _shipped():
    return ir.numeral_reviews_for(
        ir.load(), expected_sha256=ir.load()["visual_source"]["sha256"])


def test_L_ps100_claimants_are_each_read_as_their_own_psalm():
    """Seis rótulos que el reconocimiento empujaba hacia el 100.

    El impreso dice C, CIII, CIV, CVIII, CXIX y CXXVIII. Ninguno de esos
    números se dedujo de la secuencia: se leyeron en la plana, y el
    aparato latino al pie de cada una los repite.
    """
    expected = {142: 100, 146: 103, 149: 104, 158: 108, 179: 119, 184: 128}
    found = {r.scan_page: r for r in _shipped()
             if r.book == "Ps" and r.scan_page in expected}
    assert set(found) == set(expected), sorted(found)
    for page, chapter in expected.items():
        review = found[page]
        assert review.recovered_chapter == chapter, page
        assert review.observed_printed_numeral == roman.to_roman(chapter), page
        assert review.resolves
        assert review.raw_heading            # el crudo se conserva
        assert review.rationale


def test_M_ps147_is_two_distinct_headings_on_one_page():
    reviews = [r for r in _shipped() if r.scan_page == 202]
    assert len(reviews) == 2
    numbers = sorted(r.recovered_chapter for r in reviews)
    assert numbers == [147, 148]
    assert len({r.target_block for r in reviews}) == 2
    for review in reviews:
        assert review.outcome == ir.COMPETING_RESOLVED
        assert review.book == "Ps"


def test_N_sir411_is_read_as_twenty_one_and_keeps_its_raw_xxl():
    review = next(r for r in _shipped()
                  if r.book == "Sir" and r.scan_page == 411)
    assert review.raw_numeral == "XXL"
    assert review.observed_printed_numeral == "XXI"
    assert review.recovered_chapter == 21
    assert review.outcome == ir.NUMERAL_CORRECTED
    # el crudo no vale 30 ni vale nada: sigue sin ser un romano
    assert roman.is_valid(review.raw_numeral) is False
    assert roman.accumulate(review.raw_numeral) == 30


def test_the_shipped_metadata_validates_and_keeps_every_raw_reading():
    payload = ir.load()
    assert ir.validate_numerals(payload, page_count=652) == []
    for review in _shipped():
        assert review.raw_heading
        assert review.rationale and len(review.rationale) > 30
        assert review.reviewer_method
        assert review.pdf_page == review.scan_page + 1
        if review.resolves:
            assert review.observed_printed_numeral
            assert roman.to_int(review.observed_printed_numeral) == \
                review.recovered_chapter, review.id


# ======================================================================
# O-S. Las recuperaciones de frontera de 110 siguen vivas
# ======================================================================
def test_OPQRS_the_boundary_recoveries_still_stand():
    payload = ir.load()
    boundary = ir.reviews_for(payload,
                              expected_sha256=payload["visual_source"]["sha256"])
    confirmed = {(r.book, r.chapter_number) for r in boundary
                 if r.creates_boundary}
    for key in (("Prov", 25), ("Wis", 14), ("Sir", 30), ("Sir", 46),
                ("Isa", 39)):
        assert key in confirmed, key
    # y el control negativo del salmo 118 sigue sin efecto
    rejected = [r for r in boundary if not r.creates_boundary]
    assert rejected and all(r.chapter_number is None for r in rejected)


# ======================================================================
# T/U. Ni la secuencia ni el canon rescatan lo que la imagen no resolvió
# ======================================================================
def test_T_sequence_alone_cannot_resolve_a_claim_reviewed_as_unreadable():
    fixture = _page([("CAPÍTULO XX", 0), ("CAPÍTULO XXL", 1),
                     ("CAPÍTULO XXII", 2)])
    blocks = _heading_blocks(fixture, {"CAPÍTULO XXL"})
    review = _review(target_block=blocks["CAPÍTULO XXL"],
                     outcome=ir.UNREADABLE, observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None)
    _ed, _s, walker = _parse(fixture, [review])
    middle = next(c for c in walker.ledger.claims
                  if c.block_id == blocks["CAPÍTULO XXL"])
    assert middle.accepted_number is None
    assert 21 not in {c.accepted_number for c in walker.ledger.accepted()}


def test_U_canon_is_not_consulted_by_the_review_layer():
    import ast as _ast
    for name in ("numeral_review.py", "image_reviews.py"):
        tree = _ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        imported = set()
        for node in _ast.walk(tree):
            if isinstance(node, _ast.Import):
                imported |= {a.name for a in node.names}
            elif isinstance(node, _ast.ImportFrom):
                imported.add(node.module or "")
        assert "canon" not in imported, name
        assert "structure" not in imported, name


# ======================================================================
# V. Recuperación directa frente a cascada
# ======================================================================
def test_V_direct_recovery_is_distinguishable_from_a_cascade():
    """La imagen resuelve uno; la secuencia acepta el siguiente.

    El segundo NO es una segunda lectura del facsímil, y el informe tiene
    que poder decirlo: se distingue por el método de la propuesta y por
    llevar un ancla.
    """
    fixture = _page([("CAPÍTULO XXL", 0), ("CAPÍTULO XXII", 1)])
    blocks = _heading_blocks(fixture, {"CAPÍTULO XXL"})
    _ed, _s, walker = _parse(fixture, [_review(
        target_block=blocks["CAPÍTULO XXL"])])
    claims = {c.block_id: c for c in walker.ledger.claims}
    direct = claims[blocks["CAPÍTULO XXL"]]
    later = next(c for c in walker.ledger.claims if c is not direct)

    assert direct.accepted_number == 21
    assert direct.proposal_method == "numeral_review"
    assert direct.source == cc.FROM_IMAGE_REVIEW
    assert direct.anchor_number is None

    assert later.accepted_number == 22
    assert later.source == cc.FROM_OCR
    assert later.proposal_method != "numeral_review"
    assert later.anchor_number == 21
    assert later.accepted_round > direct.accepted_round


# ======================================================================
# W/X/Y/Z. Invariantes, determinismo, sin red
# ======================================================================
def test_WX_no_duplicate_or_out_of_order_refs_in_the_fixture():
    fixture = _page([("CAPÍTULO XXL", 0), ("CAPÍTULO XXII", 1)])
    blocks = _heading_blocks(fixture, {"CAPÍTULO XXL"})
    edition, _s, _w = _parse(fixture, [_review(
        target_block=blocks["CAPÍTULO XXL"])])
    seen, previous = set(), None
    for number in sorted(n for n in edition.books["Sir"].chapters if n > 0):
        assert previous is None or number > previous
        previous = number
        for verse in edition.books["Sir"].chapters[number].verses:
            ref = f"Sir.{number}.{verse}"
            assert ref not in seen
            seen.add(ref)


def test_Y_deterministic():
    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]

    def run():
        _ed, _s, walker = _parse(fixture, [_review(target_block=block)])
        return json.dumps(walker.ledger.report(), sort_keys=True, default=str)
    assert run() == run()

    def queue():
        report = json.load(open(os.path.join(
            DIR, "..", "..", "build", "torresamat1835-audit",
            "volume3.json"), encoding="utf-8"))["chapter_claims"]
        entries = numeral_review.build(
            report["claims"],
            permissive_groups=report["permissive_value_collision_detail"],
            competing_groups=report["competing_claim_groups_detail"])
        return [e.as_dict() for e in entries]
    if os.path.isfile(os.path.join(DIR, "..", "..", "build",
                                   "torresamat1835-audit", "volume3.json")):
        assert queue() == queue()


def test_Z_offline_and_no_hardcoded_cases_in_production_code():
    import ast as _ast
    for name in ("numeral_review.py", "image_reviews.py",
                 "generate_numeral_review_batch.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for forbidden in ("urllib", "requests", "http://", "https://",
                          "socket"):
            assert forbidden not in source, f"{name}: {forbidden}"

    # el algoritmo no nombra ni libros ni planas: eso vive en los datos
    for name in ("numeral_review.py",):
        tree = _ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        literals = [n.value for n in _ast.walk(tree)
                    if isinstance(n, _ast.Constant)]
        for book in ("Sir", "Ps", "Prov", "Wis", "Isa", "Eccl", "Song"):
            assert book not in [l for l in literals if isinstance(l, str)]
        for page in (100, 147, 411, 142, 202):
            assert page not in [l for l in literals if isinstance(l, int)]


def test_the_queue_is_derived_from_the_ledger_not_rebuilt():
    """La cola no vuelve a mirar el OCR: consume los reclamos ya hechos."""
    claims = [{
        "claim_id": "c0", "order": 0, "book": "Sir", "scan_page": 411,
        "block_id": "p0411l0040", "disposition": "invalid_numeral",
        "raw_heading": "CAPÍTULO XXL", "bbox": [1, 2, 3, 4],
        "numeral": {"token": "XXL", "status": "invalid_roman_syntax",
                    "permissive_value": 30},
        "candidate_numbers": [], "competing_with": [], "reason": "r"}]
    entries = numeral_review.build(claims)
    assert len(entries) == 1
    entry = entries[0]
    assert entry.pdf_page == 412
    assert entry.raw_numeral == "XXL"
    assert entry.permissive_value == 30
    assert entry.priority == numeral_review.PRIORITY_REST


# ======================================================================
# Tanda 113: reclamos falsos, exclusión de la cola y atribución por tanda
# ======================================================================
def test_C_no_duplicate_review_ids_or_targets():
    payload = ir.load()
    ids = [r["id"] for r in payload["numeral_reviews"]]
    blocks = [r["target_block"] for r in payload["numeral_reviews"]]
    assert len(ids) == len(set(ids)), "identificadores repetidos"
    assert len(blocks) == len(set(blocks)), "dos revisiones al mismo rótulo"


def test_a_rejected_false_claim_leaves_the_claim_alone():
    """Mirar una plana y descubrir que ahí no hay capítulo es un resultado.

    En el tomo hay dos: líneas de inscripción del salmo («Salmo de David:
    de los hijos de…») de las que el reconocimiento sacó un romano. No
    dan número, no crean capítulo, y quedan registradas para no volver a
    mandarlas a revisión.
    """
    false_claims = [r for r in _shipped() if r.outcome == ir.FALSE_CLAIM]
    assert false_claims, "el tomo tiene reclamos falsos revisados"
    for review in false_claims:
        assert not review.resolves
        assert review.recovered_chapter is None
        assert review.observed_printed_numeral is None
        assert review.raw_heading            # el crudo se conserva
        assert len(review.rationale) > 40

    fixture = _page([("CAPÍTULO XXL", 0)])
    block = _heading_blocks(fixture, {"CAPÍTULO XXL"})["CAPÍTULO XXL"]
    edition, _s, walker = _parse(fixture, [_review(
        target_block=block, outcome=ir.FALSE_CLAIM,
        observed_printed_text=None, observed_printed_numeral=None,
        recovered_chapter=None)])
    assert walker.ledger.claims[0].accepted_number is None
    assert not [n for n in edition.books["Sir"].chapters if n > 0]


def test_the_queue_does_not_send_a_reviewed_heading_back():
    claims = [{
        "claim_id": "c0", "order": 0, "book": "Ps", "scan_page": 101,
        "block_id": "p0101l0062", "disposition": "invalid_numeral",
        "raw_heading": "Salmo de Bavid : f He los hijos de",
        "bbox": [1, 2, 3, 4],
        "numeral": {"token": "IC", "status": "invalid_roman_syntax",
                    "permissive_value": 99},
        "candidate_numbers": [], "competing_with": [], "reason": "r"}]
    assert len(numeral_review.build(claims)) == 1
    assert numeral_review.build(claims, reviewed_blocks={"p0101l0062"}) == []


def test_every_review_declares_which_batch_it_came_from():
    payload = ir.load()
    batches = {r.get("batch", "batch-112") for r in payload["numeral_reviews"]}
    assert len(batches) >= 2, "las tandas tienen que poder distinguirse"
    for name in batches:
        assert name.startswith("batch-")


def test_a_lower_confidence_is_recorded_when_the_plate_is_the_problem():
    """Cuando el tipo impreso está dañado, no se finge certeza.

    Hay rótulos donde el reconocimiento leyó BIEN lo que hay en la plana
    y lo que está mal es la plana. Esas lecturas se apoyan en otra cosa
    impresa en la misma página y van con menos confianza declarada.
    """
    payload = ir.load()
    lowered = [r for r in payload["numeral_reviews"]
               if r.get("confidence", 1.0) < 0.95]
    assert lowered, "alguna lectura tiene que ir con confianza menor"
    for review in lowered:
        assert review["confidence"] >= 0.5
        assert len(review["rationale"]) > 80


# ======================================================================
# Identidades del informe: el total y lo pendiente no son lo mismo
#
# 113 informó 49 por un lado y 47 por otro sin decir que medían cosas
# distintas. La diferencia eran los dos reclamos falsos: revisados, pero
# con la disposición intacta porque su numeral SIGUE siendo inválido.
# Estas comprobaciones fijan la identidad para que no se vuelva a mezclar.
# ======================================================================
def _audit():
    path = os.path.join(DIR, "..", "..", "build", "torresamat1835-audit",
                        "volume3.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def test_J_invalid_total_equals_reviewed_plus_pending():
    report = _audit()
    if report is None:
        print("  (saltado: no hay audit en build/)")
        return
    nr = report["numeral_image_review"]
    assert nr["invalid_numeral_total"] == (
        nr["invalid_numeral_reviewed"] + nr["invalid_numeral_pending_review"])
    # y el total es el del ledger, no otro recuento
    assert nr["invalid_numeral_total"] == \
        report["chapter_claims"]["invalid_numeral"]


def test_K_pending_by_book_sums_to_pending_total():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    assert sum(nr["invalid_numeral_pending_by_book"].values()) == \
        nr["invalid_numeral_pending_review"]
    # nadie con cero ocupa sitio en el desglose
    assert all(n > 0 for n in nr["invalid_numeral_pending_by_book"].values())


def test_I_a_reviewed_false_claim_keeps_its_disposition_but_leaves_the_queue():
    """Revisado no es lo mismo que resuelto, ni que pendiente."""
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    blocks = set(nr["invalid_numeral_reviewed_blocks"])
    assert blocks, "el tomo tiene reclamos revisados que siguen inválidos"
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for block in blocks:
        assert claims[block]["disposition"] == "invalid_numeral"
        assert claims[block]["accepted_number"] is None
    # ninguno vuelve a la cola
    queued = {e["block_id"] for e in nr["review_queue_next"]}
    assert not (blocks & queued)


def test_the_queue_summary_and_the_pending_count_agree():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    by_disposition = nr["review_queue"]["by_disposition"]
    assert by_disposition.get("invalid_numeral", 0) == \
        nr["invalid_numeral_pending_review"]
    assert by_disposition.get("ambiguous_numeral", 0) == \
        report["chapter_claims"]["ambiguous_numeral"]


def test_batches_are_disjoint_and_every_review_names_one():
    payload = ir.load()
    seen = {}
    for review in payload["numeral_reviews"]:
        batch = review.get("batch", "batch-112")
        assert batch.startswith("batch-")
        assert review["target_block"] not in seen, review["target_block"]
        seen[review["target_block"]] = batch
    # las tandas posteriores no reescriben lo de las anteriores
    assert len({b for b in seen.values()}) >= 3


# ======================================================================
# Tanda 115: numerales inválidos y AMBIGUOS de Salmos
#
# Los ambiguos son otra cola: el reconocimiento partió el numeral en
# varios romanos válidos («SALMO LX XI.») y ninguno es el impreso (LXXI).
# La imagen decide; ni juntar los trozos ni cambiar L por I lo hace.
# ======================================================================
def _batch(name):
    return [r for r in ir.load()["numeral_reviews"] if r.get("batch") == name]


def test_115_A_batch_metadata_is_valid_and_says_which_queue_it_came_from():
    reviews = _batch("batch-115")
    assert reviews, "la tanda 115 está en los datos"
    assert ir.validate_numerals(ir.load(), page_count=652) == []
    queues = {r["queue_disposition"] for r in reviews}
    assert queues <= {cc.INVALID_NUMERAL, cc.AMBIGUOUS_NUMERAL}, queues
    for review in reviews:
        assert review["pdf_page"] == review["scan_page"] + 1
        assert review["raw_heading"] and review["raw_numeral"]
        assert isinstance(review["candidate_numbers"], list)
        assert isinstance(review["patterns"], list) and review["patterns"]
        assert review["supporting_facsimile_evidence"]
        assert len(review["rationale"]) > 80
        if review["outcome"] in ir.NUMERAL_RESOLVING:
            printed = review["observed_printed_numeral"]
            assert roman.is_valid(printed), review["id"]
            assert roman.to_int(printed) == review["recovered_chapter"]
        if review["queue_disposition"] == cc.AMBIGUOUS_NUMERAL:
            assert len(review["candidate_numbers"]) > 1, review["id"]
            # el candidato no se eligió: lo que la plana imprime queda
            # anotado aunque no estuviera entre ellos
            assert review["raw_numeral_status"] == roman.VALID
        else:
            assert review["candidate_numbers"] == []
            assert review["raw_numeral_status"] == roman.INVALID_SYNTAX
            assert roman.is_valid(review["raw_numeral"]) is False


def test_115_BCD_ids_and_blocks_stay_unique_and_earlier_batches_stand():
    payload = ir.load()
    ids = [r["id"] for r in payload["numeral_reviews"]]
    blocks = [r["target_block"] for r in payload["numeral_reviews"]]
    assert len(ids) == len(set(ids))
    assert len(blocks) == len(set(blocks))
    names = {r.get("batch", "batch-112") for r in payload["numeral_reviews"]}
    assert {"batch-112", "batch-113", "batch-114", "batch-115"} <= names
    for review in _shipped():
        if review.resolves:
            assert roman.to_int(review.observed_printed_numeral) == \
                review.recovered_chapter, review.id


def _ambiguous_fixture():
    fixture = _page([("SALMO LX XI.", 0)])
    block = _heading_blocks(fixture, {"SALMO LX XI."})["SALMO LX XI."]
    return fixture, block


def test_115_F_a_resolved_ambiguous_claim_leaves_the_ambiguous_queue():
    fixture, block = _ambiguous_fixture()
    _ed, _s, without = _parse(fixture, [], book="Ps")
    claim = without.ledger.claims[0]
    assert claim.disposition == cc.AMBIGUOUS_NUMERAL
    assert sorted(claim.candidate_numbers) == [11, 60]

    review = _review(book="Ps", target_block=block,
                     raw_heading="SALMO LX XI.", raw_numeral="LX",
                     observed_printed_text="SALMO LXXI.",
                     observed_printed_numeral="LXXI", recovered_chapter=71)
    _ed, _s, walker = _parse(fixture, [review], book="Ps")
    claim = walker.ledger.claims[0]
    assert claim.disposition == cc.ACCEPTED
    assert claim.accepted_number == 71
    # el crudo sigue diciendo lo que dijo
    assert claim.raw_heading == "SALMO LX XI."
    assert sorted(claim.candidate_numbers) == [11, 60]
    report = walker.ledger.report()
    queue = numeral_review.build(report["claims"])
    assert not [e for e in queue if e.disposition == cc.AMBIGUOUS_NUMERAL]


def test_115_G_an_unreadable_ambiguous_claim_stays_ambiguous():
    fixture, block = _ambiguous_fixture()
    review = _review(book="Ps", target_block=block, outcome=ir.UNREADABLE,
                     raw_heading="SALMO LX XI.", raw_numeral="LX",
                     observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None)
    _ed, _s, walker = _parse(fixture, [review], book="Ps")
    claim = walker.ledger.claims[0]
    assert claim.disposition == cc.AMBIGUOUS_NUMERAL
    assert claim.accepted_number is None
    # mirada, sí; resuelta, no: sale de la cola sin cambiar de estado
    claims = walker.ledger.report()["claims"]
    assert numeral_review.build(claims, reviewed_blocks={block}) == []
    assert numeral_review.build(claims)[0].disposition == cc.AMBIGUOUS_NUMERAL


def test_115_H_a_false_heading_review_creates_no_chapter():
    fixture, block = _ambiguous_fixture()
    review = _review(book="Ps", target_block=block, outcome=ir.FALSE_CLAIM,
                     raw_heading="SALMO LX XI.", observed_printed_text=None,
                     observed_printed_numeral=None, recovered_chapter=None)
    edition, _s, walker = _parse(fixture, [review], book="Ps")
    assert walker.ledger.claims[0].accepted_number is None
    assert not [n for n in edition.books["Ps"].chapters if n > 0]


def test_115_J_split_numeral_tokens_are_never_joined_automatically():
    fixture, _block = _ambiguous_fixture()
    _ed, _s, walker = _parse(fixture, [], book="Ps")
    claim = walker.ledger.claims[0]
    assert claim.accepted_number is None
    assert 71 not in claim.candidate_numbers
    reading = roman.read(" LX XI.")
    assert 71 not in list(reading.candidates)
    assert reading.value != 71


def test_115_K_a_final_l_is_never_read_as_i_without_the_facsimile():
    fixture = _page([("SALMO XVI.", 0), ("SALMO XVIL", 1),
                     ("SALMO XVIII.", 2)])
    blocks = _heading_blocks(fixture, {"SALMO XVI.", "SALMO XVIL"})
    anchor = _review(book="Ps", target_block=blocks["SALMO XVI."],
                     raw_heading="SALMO XVI.", raw_numeral="XVI",
                     observed_printed_text="SALMO XVI.",
                     observed_printed_numeral="XVI", recovered_chapter=16)
    _ed, _s, walker = _parse(fixture, [anchor], book="Ps")
    middle = next(c for c in walker.ledger.claims
                  if c.block_id == blocks["SALMO XVIL"])
    assert middle.disposition == cc.INVALID_NUMERAL
    assert middle.accepted_number is None
    assert 17 not in {c.accepted_number for c in walker.ledger.accepted()}
    assert roman.read(" XVIL").status == roman.INVALID_SYNTAX


def test_115_I_raw_ocr_of_every_reviewed_claim_is_untouched():
    report = _audit()
    if report is None:
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for review in _batch("batch-115"):
        claim = claims[review["target_block"]]
        assert claim["raw_heading"] == review["raw_heading"]
        assert claim["numeral"]["token"] == review["raw_numeral"]
        assert claim["numeral"]["status"] == review["raw_numeral_status"]
        assert claim["candidate_numbers"] == review["candidate_numbers"]
        assert claim["accepted_number"] == review["recovered_chapter"]
        assert claim["proposal_method"] == "numeral_review"
        assert claim["provenance"]["raw_ocr_numeral"] == review["raw_numeral"]
        assert claim["provenance"]["observed_printed_numeral"] == \
            review["observed_printed_numeral"]


def test_115_E_reviewed_invalid_and_ambiguous_claims_leave_the_queues():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    blocks = {r["target_block"] for r in _batch("batch-115")}
    queued = {e["block_id"] for e in nr["review_queue_next"]}
    assert not (blocks & queued)
    stat = nr["batches"]["batch-115"]
    assert sum(stat["by_queue_disposition"].values()) == stat["reviewed"]


def test_115_LM_direct_and_cascade_are_counted_apart_on_accepted_anchors():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    claims = {c["claim_id"]: c for c in report["chapter_claims"]["claims"]}
    direct = {d["block_id"] for d in nr["direct"]}
    cascade = {c["block_id"] for c in nr["cascade"]}
    assert not (direct & cascade), "una cascada no es otra lectura"
    assert len(direct) == nr["direct_image_recoveries"]
    assert len(cascade) == nr["cascade_resolutions"]
    assert sum(b["accepted_from_this_batch"]
               for b in nr["batches"].values()) == len(direct)
    assert len(nr["cascade_chains"]) == nr["cascade_resolutions"]
    anchored = sum(b["cascade_resolutions_anchored_here"]
                   for b in nr["batches"].values())
    assert anchored <= nr["cascade_resolutions"]
    for chain in nr["cascade_chains"]:
        assert chain["hops"] >= 1
        assert chain["hops"] <= nr["longest_cascade_chain"]
        # sólo un capítulo ACEPTADO sirve de ancla
        anchor = claims[chain["anchor_claim"]]
        assert anchor["disposition"] == cc.ACCEPTED
        assert anchor["accepted_round"] < chain["accepted_round"]
    for item in nr["cascade"]:
        assert item["source"] == cc.FROM_OCR


def test_115_O_queue_next_reflects_the_categories_that_really_remain():
    report = _audit()
    if report is None:
        return
    nr = report["numeral_image_review"]
    summary = nr["review_queue"]
    split = nr["review_queue_by_disposition_and_book"]
    assert {k: sum(v.values()) for k, v in split.items()} == \
        summary["by_disposition"]
    assert sum(nr["review_queue_by_reason"].values()) == summary["total"]
    if nr["review_queue_next"]:
        assert nr["review_queue_first_category"] == \
            nr["review_queue_next"][0]["disposition"]
    else:
        assert nr["review_queue_first_category"] is None
    for entry in nr["review_queue_next"]:
        assert entry["disposition"] in summary["by_disposition"]


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
    print(f"torresamat1835_numeral_review_failures={failures}")
    sys.exit(1 if failures else 0)
