"""
TORRES-1835-CHAPTER-NUMERAL-COLLISIONS-111: numerales y reclamos.

    python3 test_chapter_claims.py

Sin red y sin el testigo real. Los casos del tomo (Sir 411, Ps 100) viven
aquí como fixtures con su texto literal: en el código de producción no hay
ni una página ni un libro escrito a mano.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chapter_claims as cc
import image_reviews as ir
import page_parser
import recovery
import roman
import source_ocr
import structure
from image_reviews import BOUNDARY_AND_NUMBER, BOUNDARY_ONLY
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256="f" * 64,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


# ======================================================================
# A/B + FASE 23. El validador romano
# ======================================================================
def test_valid_roman_numerals_parse():
    for token, value in (("I", 1), ("IV", 4), ("V", 5), ("IX", 9),
                         ("XIV", 14), ("XXV", 25), ("XXXIX", 39),
                         ("XLVI", 46), ("L", 50), ("XCIX", 99),
                         ("C", 100), ("CXIX", 119), ("CL", 150)):
        assert roman.is_valid(token), token
        assert roman.to_int(token) == value, token


def test_invalid_roman_syntax_is_rejected():
    """Los casos de la FASE 23, y lo que el converter viejo sacaba de ellos.

    Ninguno es un número romano. El converter antiguo les daba un valor a
    todos sin objetar, y ese valor se escribía como capítulo.
    """
    for token, permissive in (("XXL", 30), ("IXX", 19), ("VX", 5),
                              ("LC", 50), ("IC", 99), ("IL", 49),
                              ("VL", 45), ("IIL", 48), ("VIL", 44),
                              ("IM", 999), ("XXXX", 40), ("VV", 10),
                              ("IIII", 4), ("LL", 100), ("XIL", 39)):
        assert not roman.is_valid(token), token
        assert roman.to_int(token) is None, token
        # el valor permisivo se conserva para poder auditarlo, pero NO
        # se usa como número
        assert roman.accumulate(token) == permissive, token
        reading = roman.read(token)
        assert reading.status == roman.INVALID_SYNTAX, token
        assert reading.value is None, token
        assert reading.permissive_value == permissive, token
        assert str(permissive) in reading.reason


def test_the_real_heading_of_the_volume_is_rejected():
    """«CAPÍTULO XXL» es el rótulo que hundía el capítulo 21 del Sirácida."""
    reading = roman.read(structure._without_book_words("CAPÍTULO XXL"))
    assert reading.status == roman.INVALID_SYNTAX
    assert reading.value is None
    assert reading.permissive_value == 30
    assert structure.roman_candidates(
        structure._without_book_words("CAPÍTULO XXL")) == []


def test_typographic_normalisation_is_case_aware():
    """La ele minúscula es una I; la ele MAYÚSCULA no.

    Normalizar después de pasar a mayúsculas convertiría «XXL» en «XXI»
    -- que da la casualidad de ser el numeral correcto de esa plana, y
    por eso mismo es la trampa: sería adivinar y no leer.
    """
    assert roman.read("XXl").value == 21
    assert roman.read("XX1").value == 21
    assert roman.read("XX|").value == 21
    assert roman.read("XXL").value is None
    assert "L" not in roman.TYPOGRAPHIC
    assert "l" in roman.TYPOGRAPHIC


def test_confusable_corrections_are_marked_as_such():
    """Y por Y->V se lee, pero queda marcado como enmienda."""
    reading = roman.read("XXY")
    assert reading.value == 25
    assert reading.origin == roman.FROM_CONFUSABLE
    assert reading.needs_corroboration
    clean = roman.read("XXV")
    assert clean.value == 25
    assert clean.origin == roman.FROM_LITERAL
    assert not clean.needs_corroboration


def test_a_numeral_glued_to_another_character_is_not_read_short():
    """«XX1» no vale 20: el uno es la I que no se compuso."""
    assert roman.delimited_tokens("XX1") == []
    assert roman.delimited_tokens(" XXV ") == ["XXV"]
    assert roman.read("XX1").value == 21


# ======================================================================
# C/D. Ni la secuencia ni el canon arreglan un numeral roto
# ======================================================================
def _claim(**over):
    base = dict(claim_id="c1", book="Sir", scan_page=411,
                block_id="p0411l0040", raw_heading="CAPÍTULO XXL",
                source=cc.FROM_OCR,
                numeral=roman.read(" XXL").as_dict(),
                candidate_numbers=[], proposed_number=None,
                proposal_method="unresolved", proposal_evidence={},
                confidence=0.1)
    base.update(over)
    return cc.ChapterClaim(**base)


def test_sequence_alone_cannot_repair_an_invalid_numeral():
    """Vecinos 20 y 22 no convierten un numeral inválido en 21."""
    ledger = cc.ClaimLedger()
    ledger.add(_claim(claim_id="a", block_id="p0409l0001",
                      raw_heading="CAPÍTULO XX",
                      numeral=roman.read(" XX").as_dict(),
                      candidate_numbers=[20], proposed_number=20,
                      proposal_method="direct_ocr",
                      proposal_evidence={"agrees": ["sequence"]},
                      confidence=0.9))
    broken = ledger.add(_claim(claim_id="b"))
    ledger.add(_claim(claim_id="c", block_id="p0413l0001",
                      raw_heading="CAPÍTULO XXII",
                      numeral=roman.read(" XXII").as_dict(),
                      candidate_numbers=[22], proposed_number=22,
                      proposal_method="direct_ocr",
                      proposal_evidence={"agrees": ["sequence"]},
                      confidence=0.9))
    ledger.resolve()
    assert broken.accepted_number is None
    assert broken.disposition == cc.INVALID_NUMERAL
    assert broken.review_required
    assert 21 not in {c.accepted_number for c in ledger.accepted()}


def test_canon_is_not_consulted_to_resolve_a_claim():
    source = open(os.path.join(DIR, "chapter_claims.py"), encoding="utf-8").read()
    tree = __import__("ast").parse(source)
    imported = set()
    for node in __import__("ast").walk(tree):
        if isinstance(node, __import__("ast").Import):
            imported |= {a.name for a in node.names}
        elif isinstance(node, __import__("ast").ImportFrom):
            imported.add(node.module or "")
    assert "canon" not in imported
    assert "structure" not in imported
    # y un numeral por encima del máximo del libro no se rechaza por eso:
    # el canon señala anomalías, no decide autenticidad.
    ledger = cc.ClaimLedger()
    high = ledger.add(_claim(claim_id="h", book="Sir",
                             raw_heading="CAPÍTULO LXXX",
                             numeral=roman.read(" LXXX").as_dict(),
                             candidate_numbers=[80], proposed_number=80,
                             proposal_method="direct_ocr",
                             proposal_evidence={"agrees": ["running_header"]},
                             confidence=0.9))
    ledger.resolve()
    assert high.accepted_number == 80


def test_running_header_alone_cannot_convert_an_invalid_numeral():
    """FASE 13: una cabecera ruidosa no rescata un numeral que no existe.

    Es exactamente lo que pasaba en la plana 411: el rótulo decía «XXL» y
    la cabecera corrida de la MISMA plana traía el mismo error, así que
    las dos «señales independientes» eran el mismo trazo mal leído.
    """
    ledger = cc.ClaimLedger()
    claim = ledger.add(_claim(
        proposed_number=30, proposal_method="running_header_correlated",
        proposal_evidence={"agrees": ["running_header", "sequence"]},
        confidence=0.85))
    ledger.resolve()
    assert claim.accepted_number is None
    assert claim.disposition == cc.INVALID_NUMERAL
    assert "30" in claim.reason


# ======================================================================
# E. La ambigüedad puede quedarse sin resolver
# ======================================================================
def test_ambiguous_numeral_may_stay_unresolved():
    ledger = cc.ClaimLedger()
    claim = ledger.add(_claim(
        book="Ps", scan_page=146, block_id="p0146l0046",
        raw_heading="SALMO C 1 1 L",
        numeral=roman.read(" C 1 1 L").as_dict(),
        candidate_numbers=[100, 50], proposed_number=None,
        proposal_method="unresolved", confidence=0.4))
    ledger.resolve()
    assert claim.accepted_number is None
    assert claim.disposition == cc.AMBIGUOUS_NUMERAL
    assert "100" in claim.reason and "50" in claim.reason


def test_a_confusable_reading_without_corroboration_does_not_stand():
    ledger = cc.ClaimLedger()
    claim = ledger.add(_claim(
        book="Prov", raw_heading="CAPÍTULO XXY",
        numeral=roman.read(" XXY").as_dict(),
        candidate_numbers=[25], proposed_number=25,
        proposal_method="direct_ocr", proposal_evidence={}, confidence=0.9))
    ledger.resolve()
    assert claim.accepted_number is None
    assert claim.disposition == cc.UNCORROBORATED


# ======================================================================
# F/G. La revisión visual corrige y funde
# ======================================================================
def test_facsimile_review_can_correct_an_invalid_ocr_numeral():
    ledger = cc.ClaimLedger()
    review = ledger.add(_claim(
        claim_id="img", source=cc.FROM_IMAGE_REVIEW,
        raw_heading="CAPÍTULO XXI",
        numeral=roman.read(" XXI").as_dict(), candidate_numbers=[21],
        proposed_number=21, proposal_method="image_review",
        proposal_evidence={"review_id": "sir-21"}, confidence=0.95,
        provenance={"source": "image_review", "review_id": "sir-21"}))
    ledger.resolve()
    assert review.accepted_number == 21
    assert review.disposition == cc.ACCEPTED
    assert review.provenance["review_id"] == "sir-21"


def test_two_claims_at_the_same_physical_place_merge():
    """El mismo rótulo impreso leído por la máquina y por una persona.

    No son dos capítulos en disputa: son uno, y la imagen corrige a la
    máquina. Tratarlo como conflicto castigaría la mejor evidencia.
    """
    ledger = cc.ClaimLedger()
    machine = ledger.add(_claim(
        claim_id="ocr", block_id="p0475l0032",
        raw_heading="CAPÍTULO* XX vi",
        numeral=roman.read(" XX vi").as_dict(),
        candidate_numbers=[20, 6], proposed_number=None))
    human = ledger.add(_claim(
        claim_id="img", block_id="p0475r0032",
        source=cc.FROM_IMAGE_REVIEW, raw_heading="CAPÍTULO XLVI",
        numeral=roman.read(" XLVI").as_dict(), candidate_numbers=[46],
        proposed_number=46, proposal_method="image_review",
        confidence=0.95, provenance={"review_id": "sir-46-p475"}))
    assert machine.physical_key == human.physical_key
    ledger.resolve()
    assert human.accepted_number == 46
    assert machine.disposition == cc.SAME_PHYSICAL
    assert machine.merged_into == "img"
    assert machine.accepted_number is None
    assert len(ledger.collisions()) == 0


def test_a_review_settles_a_true_collision_in_favour_of_the_image():
    ledger = cc.ClaimLedger()
    machine = ledger.add(_claim(
        claim_id="ocr", scan_page=300, block_id="p0300l0010",
        raw_heading="CAPÍTULO XXX",
        numeral=roman.read(" XXX").as_dict(), candidate_numbers=[30],
        proposed_number=30, proposal_method="direct_ocr",
        proposal_evidence={"agrees": ["running_header"]}, confidence=0.9))
    human = ledger.add(_claim(
        claim_id="img", scan_page=435, block_id="p0435r0001",
        source=cc.FROM_IMAGE_REVIEW, raw_heading="CAPÍTULO XXX",
        numeral=roman.read(" XXX").as_dict(), candidate_numbers=[30],
        proposed_number=30, proposal_method="image_review",
        confidence=0.95, provenance={"review_id": "sir-30-p435"}))
    ledger.resolve()
    assert human.accepted_number == 30
    assert machine.accepted_number is None
    assert machine.disposition == cc.COMPETING
    assert "img" in machine.competing_with
    assert len(ledger.collisions()) == 1


# ======================================================================
# H/I/J. Conflictos reales: ni el primero ni el último gana
# ======================================================================
def _two_equal_claims():
    ledger = cc.ClaimLedger()
    first = ledger.add(_claim(
        claim_id="first", scan_page=375, block_id="p0375l0020",
        raw_heading="CAPÍTULO V.", numeral=roman.read(" V").as_dict(),
        candidate_numbers=[5], proposed_number=5,
        proposal_method="direct_ocr",
        proposal_evidence={"agrees": ["running_header"]}, confidence=0.9))
    second = ledger.add(_claim(
        claim_id="second", scan_page=506, block_id="p0506l0046",
        raw_heading='" CAPÍTULO V.', numeral=roman.read(" V").as_dict(),
        candidate_numbers=[5], proposed_number=5,
        proposal_method="direct_ocr",
        proposal_evidence={"agrees": ["running_header"]}, confidence=0.9))
    return ledger.resolve(), first, second


def test_two_distinct_headings_for_one_chapter_make_a_competing_group():
    ledger, _first, _second = _two_equal_claims()
    groups = ledger.collisions()
    assert len(groups) == 1
    group = groups[0]
    assert group.disposition == cc.COMPETING
    assert len(group.claims) == 2
    assert len(group.physical_places) == 2
    assert group.book == "Sir" and group.number == 5
    payload = group.as_dict()
    assert payload["claimant_count"] == 2
    assert payload["distinct_physical_places"] == 2
    assert {c["scan_page"] for c in payload["claimants"]} == {375, 506}


def test_no_last_write_wins():
    _ledger, first, second = _two_equal_claims()
    assert second.accepted_number is None


def test_no_first_write_wins():
    _ledger, first, second = _two_equal_claims()
    assert first.accepted_number is None
    assert first.disposition == cc.COMPETING
    assert second.disposition == cc.COMPETING


# ======================================================================
# K/L. Los dos casos reales del tomo
# ======================================================================
def test_ps100_multiple_claimants_are_visible_before_the_dict():
    """Los seis rótulos que reclamaban Ps 100, con su texto real.

    Ninguno se pierde: en el mapa de capítulos sólo cabría uno, aquí
    están los seis con su numeral y su motivo.
    """
    real = [
        (142, "p0142l0015", "SALMO C"),
        (146, "p0146l0046", "SALMO C 1 1 L"),
        (149, "p0149l0001", "SALMO C I y."),
        (158, "p0158l0022", "SALMO C V 1 1 1."),
        (179, "p0179l0001", "SALMO C X I X."),
        (184, "p0184l0016", "'; :/ SALMO c XX VII I."),
    ]
    ledger = cc.ClaimLedger()
    for page, block, heading in real:
        reading = roman.read(structure._without_book_words(heading))
        ledger.add(_claim(
            claim_id=f"ps{page}", book="Ps", scan_page=page,
            block_id=block, raw_heading=heading,
            numeral=reading.as_dict(),
            candidate_numbers=list(reading.candidates),
            proposed_number=None, proposal_method="unresolved",
            confidence=0.4))
    ledger.resolve()

    assert len(ledger.claims) == 6
    assert len({c.physical_key for c in ledger.claims}) == 6
    # todos siguen ahí, con su plana y su rótulo
    report = ledger.report()
    assert report["total_claims"] == 6
    assert {c["scan_page"] for c in report["claims"]} == {
        142, 146, 149, 158, 179, 184}
    # y ninguno recibe el 100 a ciegas
    assert all(c.accepted_number is None for c in ledger.claims)
    # la lectura permisiva antigua los ponía a todos encima del mismo
    # número, y el diccionario se quedaba con uno
    permissive = ledger.permissive_collisions()
    assert any(g["claimed_number"] == 100 and
               g["distinct_physical_places"] >= 5 for g in permissive), \
        permissive


def test_sir_scan411_is_not_silently_read_as_thirty():
    """El caso obligatorio: «CAPÍTULO XXL» en la plana 411 del Sirácida."""
    reading = roman.read(structure._without_book_words("CAPÍTULO XXL"))
    ledger = cc.ClaimLedger()
    claim = ledger.add(_claim(
        numeral=reading.as_dict(),
        candidate_numbers=list(reading.candidates),
        proposed_number=None, proposal_method="unresolved"))
    ledger.resolve()
    assert claim.accepted_number is None
    assert claim.disposition == cc.INVALID_NUMERAL
    assert claim.review_required
    assert claim.numeral["permissive_value"] == 30
    assert "NOT being read as 30" in claim.reason
    # y no le quita el 30 a nadie
    assert 30 not in {c.accepted_number for c in ledger.accepted()}


# ======================================================================
# M-P + V. Integración: las recuperaciones de 110 siguen vivas
# ======================================================================
def _recovered_fixture(*, heading, chapter_number, book="Sir"):
    """Una plana con su rótulo y una revisión visual sobre él."""
    fixture = {"pages": [{"scan_page": 10, "width": 3402, "height": 4837,
                          "lines": [
        {"text": "LIBRO. 100", "bbox": [1697, 195, 2117, 255]},
        {"text": "1 Verbum latinum.", "bbox": [400, 700, 1600, 772]},
        {"text": "1 Verso anterior.", "bbox": [1700, 700, 3000, 772]},
        {"text": "continuatio latina.", "bbox": [400, 790, 1600, 862]},
        {"text": "2 Otro verso anterior.", "bbox": [1700, 790, 3000, 862]},
        {"text": heading, "bbox": [1200, 1000, 2150, 1080]},
        {"text": "1 Verbum latinum sequentis.", "bbox": [400, 1300, 1600, 1372]},
        {"text": "1 Primer verso del capitulo.", "bbox": [1700, 1300, 3000, 1372]},
        {"text": "continuatio.", "bbox": [400, 1390, 1600, 1462]},
        {"text": "2 Segundo verso del capitulo.", "bbox": [1700, 1390, 3000, 1462]},
    ]}]}
    review = ir.ChapterImageReview(
        id=f"fx-{book.lower()}-{chapter_number}", book=book, scan_page=10,
        outcome=BOUNDARY_AND_NUMBER, chapter_number=chapter_number,
        boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block="p0010l0004", insert_before_block="p0010l0007",
        observed_printed_text=f"CAPÍTULO {roman.to_roman(chapter_number)}",
        crop_bbox=(0, 0, 3402, 2000), confidence=0.95,
        rationale="read from the facsimile", reviewer_method="render",
        pdf_page=11, printed_page=2)
    return fixture, review


def _run_recovery(heading, number, book="Sir"):
    fixture, review = _recovered_fixture(heading=heading,
                                         chapter_number=number, book=book)
    edition, _stats, walker = page_parser.parse_volume(
        source_ocr.pages_from_fixture(copy.deepcopy(fixture)),
        witness="fx", volume="3", book=book, gutter_hint=GUTTER,
        with_walker=True, image_reviews={10: [review]},
        recovery_source=SOURCE)
    return edition, walker


def _assert_recovered(heading, number, book):
    edition, walker = _run_recovery(heading, number, book)
    claims = [c for c in walker.ledger.claims
              if c.source == cc.FROM_IMAGE_REVIEW]
    assert len(claims) == 1, [c.as_dict() for c in walker.ledger.claims]
    claim = claims[0]
    assert claim.accepted_number == number, claim.reason
    assert claim.disposition == cc.ACCEPTED
    assert claim.provenance and claim.provenance["source"] == "image_review"
    # el capítulo existe de verdad, con sus versículos
    assert number in edition.books[book].chapters
    assert edition.books[book].chapters[number].verses
    # y el rótulo del modelo lleva la procedencia visual
    headings = [b for ch in edition.books[book].chapters.values()
                for b in ch.paratext if b.kind is BlockKind.CHAPTER_HEADING]
    recovered = [b for b in headings if b.is_recovered]
    assert len(recovered) == 1
    assert recovered[0].number == number
    return edition, walker


def test_sir46_recovery_still_works():
    _assert_recovered("CAPÍTULO* XX vi", 46, "Sir")


def test_prov25_recovery_still_works_and_keeps_its_provenance():
    """Aun cuando el OCR ya lee 25, la imagen sigue siendo la autoridad.

    «XXY» se lee 25 sólo tras enmendar la Y, así que degradar la revisión
    a «redundante» dejaría el capítulo apoyado en una enmienda en vez de
    en una lectura del impreso.
    """
    _assert_recovered("CAPÍTULO XXY.", 25, "Prov")


def test_wis14_recovery_still_works():
    _assert_recovered("CAPÍTULO XIV-", 14, "Wis")


def test_isa39_recovery_still_works():
    _assert_recovered("CAPÍTULO XXX'IX.", 39, "Isa")


def test_an_unresolved_claim_produces_no_chapter_reference():
    """Un reclamo sin resolver conserva sus versículos, pero no da un ref.

    Se queda en su hueco de trabajo negativo: sigue en la cola de
    revisión con su procedencia y sin inventar un número.
    """
    fixture = {"pages": [{"scan_page": 10, "width": 3402, "height": 4837,
                          "lines": [
        {"text": "1 Verbum latinum.", "bbox": [400, 700, 1600, 772]},
        {"text": "1 Verso anterior.", "bbox": [1700, 700, 3000, 772]},
        {"text": "CAPÍTULO XXL", "bbox": [1200, 1000, 2150, 1080]},
        {"text": "1 Verbum sequentis.", "bbox": [400, 1300, 1600, 1372]},
        {"text": "1 Primer verso.", "bbox": [1700, 1300, 3000, 1372]},
    ]}]}
    edition, _stats, walker = page_parser.parse_volume(
        source_ocr.pages_from_fixture(fixture), witness="fx", volume="3",
        book="Sir", gutter_hint=GUTTER, with_walker=True)
    claims = walker.ledger.claims
    assert len(claims) == 1
    assert claims[0].accepted_number is None
    assert claims[0].disposition == cc.INVALID_NUMERAL
    chapters = edition.books["Sir"].chapters
    assert not [n for n in chapters if n > 0], chapters
    # los versículos NO se pierden: viven en el hueco de trabajo
    assert any(ch.verses for n, ch in chapters.items() if n < 0)
    assert any(b.kind is BlockKind.CHAPTER_HEADING and b.review_required
               for b in edition.review_queue)


def test_every_claim_gets_its_own_working_slot_before_resolution():
    """Dos rótulos que dicen el mismo número no se pisan mientras se lee.

    Es la propiedad que hace posible detectar la colisión: si el segundo
    escribiera en capitulos[5] encima del primero, no quedaría nada que
    comparar.
    """
    fixture = {"pages": [{"scan_page": p, "width": 3402, "height": 4837,
                          "lines": [
        {"text": "1 Verbum latinum.", "bbox": [400, 300, 1600, 372]},
        {"text": "CAPÍTULO V.", "bbox": [1200, 500, 2150, 580]},
        {"text": "1 Latina.", "bbox": [400, 800, 1600, 872]},
        {"text": f"1 Verso de la plana {p}.", "bbox": [1700, 800, 3000, 872]},
    ]} for p in (10, 20)]}
    edition, _stats, walker = page_parser.parse_volume(
        source_ocr.pages_from_fixture(fixture), witness="fx", volume="3",
        book="Sir", gutter_hint=GUTTER, with_walker=True)
    assert len(walker.ledger.claims) == 2
    assert len({c.slot for c in walker.ledger.claims}) == 2
    assert all(c.slot < 0 for c in walker.ledger.claims)
    # ningún versículo se ha perdido por el camino
    verses = sum(len(ch.verses) for ch in edition.books["Sir"].chapters.values())
    assert verses == 2, verses


# ======================================================================
# W. La medida pre-colapso ve lo que el diccionario no puede
# ======================================================================
def test_pre_collapse_metric_sees_what_the_dict_cannot():
    ledger, _first, _second = _two_equal_claims()
    # antes del colapso: dos reclamantes visibles
    assert len(ledger.collisions()) == 1
    assert sum(len(g.claims) for g in ledger.collisions()) == 2
    # después del colapso: un diccionario indexado por número sólo puede
    # tener una entrada por clave, así que la medida vieja sale vacía
    collapsed = {}
    for claim in ledger.claims:
        collapsed[(claim.book, claim.proposed_number)] = claim
    duplicates = [k for k, _v in collapsed.items()
                  if sum(1 for c in ledger.claims
                         if (c.book, c.proposed_number) == k) > 1]
    assert len(collapsed) == 1
    assert duplicates          # existían...
    assert len(collapsed) < len(ledger.claims)   # ...pero el dict perdió uno


def test_materialization_writes_only_accepted_claims():
    ledger, first, second = _two_equal_claims()

    class _Chapter:
        def __init__(self): self.number = None; self.verses = {}

    class _Book:
        def __init__(self): self.chapters = {}

    class _Edition:
        def __init__(self): self.books = {"Sir": _Book()}

    edition = _Edition()
    for index, claim in enumerate(ledger.claims, start=1):
        claim.slot = -index
        edition.books["Sir"].chapters[-index] = _Chapter()
    outcome = cc.materialize(edition, ledger)
    assert outcome["materialized"] == 0
    assert outcome["left_in_review"] == 2
    assert not [n for n in edition.books["Sir"].chapters if n > 0]
    assert first.accepted_number is None and second.accepted_number is None


# ======================================================================
# X/Y. Determinismo y sin red
# ======================================================================
def test_deterministic():
    def run():
        ledger, _f, _s = _two_equal_claims()
        return json.dumps(ledger.report(), sort_keys=True, default=str)
    assert run() == run()

    def run_roman():
        return [roman.read(t).as_dict()
                for t in ("XXL", "XXV", "XXY", "XX1", "SALMO C 1 1 L")]
    assert run_roman() == run_roman()


def test_offline():
    for name in ("roman.py", "chapter_claims.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for forbidden in ("urllib", "requests", "http://", "https://",
                          "socket", "subprocess"):
            assert forbidden not in source, f"{name}: {forbidden}"


def test_no_hardcoded_pages_books_or_headings_in_production_code():
    """FASE 25: los casos reales viven en tests y datos, no en el algoritmo."""
    import ast as _ast
    for name in ("roman.py", "chapter_claims.py"):
        tree = _ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        literals = [n.value for n in _ast.walk(tree)
                    if isinstance(n, _ast.Constant)]
        for book in ("Sir", "Ps", "Prov", "Wis", "Isa", "Eccl", "Song"):
            assert book not in [l for l in literals if isinstance(l, str)], \
                f"{name} names the book {book}"
        for page in (411, 435, 475, 583, 259, 346, 142):
            assert page not in [l for l in literals if isinstance(l, int)], \
                f"{name} names the page {page}"


# ======================================================================
# El ancla de secuencia sólo la mueve un capítulo ACEPTADO
#
# Un reclamo detectado no es un capítulo aceptado. Mientras no lo sea, no
# puede servir de segunda señal a nadie. Estos son los seis casos del fix.
# ======================================================================
def _sequential_policy(header_by_claim=None):
    """Política de numeración de juguete, con la forma de la real.

    Acepta un numeral válido cuando concuerda con la cabecera corrida de
    su plana o con la secuencia (anterior + 1). Es la regla de las dos
    señales; lo que aquí se prueba es de dónde puede salir la segunda.
    """
    header_by_claim = header_by_claim or {}

    def propose(claim, previous):
        value = claim.numeral.get("value")
        if value is None:
            return cc.Proposal(None, "unresolved", {"why": "no numeral"}, 0.0)
        agrees = []
        if value in header_by_claim.get(claim.claim_id, []):
            agrees.append("running_header")
        if previous is not None and value == previous + 1:
            agrees.append("sequence")
        if previous is None and claim.first_in_book and value == 1:
            agrees.append("sequence")
        if not agrees:
            return cc.Proposal(None, "unresolved",
                               {"why": "no corroborating signal"}, 0.4)
        return cc.Proposal(value, "direct_ocr", {"agrees": agrees}, 0.9)
    return propose


def _chain(*headings, book="Sir"):
    """Una cadena de rótulos en orden de lectura."""
    ledger = cc.ClaimLedger()
    for index, (heading, page) in enumerate(headings):
        reading = roman.read(structure._without_book_words(heading))
        ledger.add(_claim(
            claim_id=f"c{index}", book=book, scan_page=page,
            block_id=f"p{page:04d}l0001", raw_heading=heading,
            numeral=reading.as_dict(),
            candidate_numbers=list(reading.candidates),
            proposed_number=None, proposal_method="unresolved",
            proposal_evidence={}, confidence=0.0))
    return ledger


def test_1_invalid_claim_between_two_valid_ones_does_not_advance_the_anchor():
    """Un numeral inválido en medio no corre la secuencia.

    Si «XXL» sirviera de ancla, el rótulo siguiente encontraría un 30
    detrás y un 31 se «corroboraría» solo. Al no contar, el siguiente
    sigue apoyado en el 20, que es el último capítulo de verdad.
    """
    # el primer eslabón se sostiene en su cabecera corrida, que es como
    # empieza una cadena de verdad
    ledger = _chain(("CAPÍTULO XX", 409), ("CAPÍTULO XXL", 411),
                    ("CAPÍTULO XXXI", 413))
    ledger.resolve(_sequential_policy({"c0": [20]}))
    first, broken, after = ledger.claims

    assert first.accepted_number == 20
    assert broken.accepted_number is None
    assert broken.disposition == cc.INVALID_NUMERAL
    # el de detrás no se apoya en el rechazado
    assert after.accepted_number is None, (
        "31 se aceptó: alguien usó el XXL rechazado como ancla")
    assert after.anchor_number is None

    # y el que SÍ sigue al 20 se acepta con el 20 de ancla
    ledger2 = _chain(("CAPÍTULO XX", 409), ("CAPÍTULO XXL", 411),
                     ("CAPÍTULO XXI", 413))
    ledger2.resolve(_sequential_policy({"c0": [20]}))
    twenty, _bad, twentyone = ledger2.claims
    assert twenty.accepted_number == 20
    assert twentyone.accepted_number == 21
    assert twentyone.anchor_number == 20
    assert twentyone.anchor_claim == twenty.claim_id


def test_2_ambiguous_claim_does_not_advance_the_anchor():
    ledger = _chain(("CAPÍTULO XX", 409), ("SALMO C X I X.", 411),
                    ("CAPÍTULO XXI", 413))
    ledger.resolve(_sequential_policy({"c0": [20]}))
    first, ambiguous, after = ledger.claims
    assert first.accepted_number == 20
    assert ambiguous.accepted_number is None
    assert ambiguous.disposition in (cc.AMBIGUOUS_NUMERAL, cc.UNRESOLVED)
    # el siguiente se ancla en el 20, no en nada que saliera del ambiguo
    assert after.accepted_number == 21
    assert after.anchor_number == 20
    assert after.anchor_claim == first.claim_id


def test_3_a_rejected_claim_cannot_corroborate_a_later_numeral():
    """El reclamo rechazado no aporta la segunda señal a nadie."""
    ledger = _chain(("CAPÍTULO XXL", 411), ("CAPÍTULO XXXI", 413))
    ledger.resolve(_sequential_policy())
    rejected, after = ledger.claims
    assert rejected.accepted_number is None
    assert after.accepted_number is None
    assert after.anchor_number is None
    assert not after.proposal_evidence.get("agrees")


def test_4_a_competing_claim_cannot_become_the_sequence_anchor():
    """Dos rótulos distintos piden el 5; ninguno ancla al siguiente."""
    ledger = _chain(("CAPÍTULO V", 375), ("CAPÍTULO V", 506),
                    ("CAPÍTULO VI", 510))
    ledger.resolve(_sequential_policy({"c0": [5], "c1": [5]}))
    first, second, after = ledger.claims
    assert first.disposition == cc.COMPETING
    assert second.disposition == cc.COMPETING
    assert first.accepted_number is None and second.accepted_number is None
    assert after.accepted_number is None, (
        "6 se aceptó: un reclamo en disputa sirvió de ancla")
    assert after.anchor_number is None
    assert len(ledger.collisions()) == 1


def test_5_an_accepted_claim_does_advance_the_anchor():
    ledger = _chain(("CAPÍTULO I", 100), ("CAPÍTULO II", 102),
                    ("CAPÍTULO III", 104), ("CAPÍTULO IV", 106))
    ledger.resolve(_sequential_policy())
    numbers = [c.accepted_number for c in ledger.claims]
    assert numbers == [1, 2, 3, 4], numbers
    anchors = [c.anchor_number for c in ledger.claims]
    assert anchors == [None, 1, 2, 3], anchors
    # una cadena se resuelve eslabón a eslabón, una ronda por eslabón
    assert ledger.rounds >= 4
    assert all(c.accepted_round == index + 1
               for index, c in enumerate(ledger.claims))


def test_6_sir_scan411_does_not_influence_the_next_chapter():
    """El caso real: XXL en la 411 y XXII en la 413.

    El impreso pone XXI en la 411. Con el numeral rechazado, el XXII de
    la 413 no tiene ancla y NO se acepta por secuencia: la plana 411
    espera revisión visual, y la 413 espera con ella. Inventarle un 21 al
    hueco para poder aceptar el 22 es exactamente lo que no se hace.
    """
    ledger = _chain(("CAPÍTULO XXVII", 405), ("CAPÍTULO XXL", 411),
                    ("Cavitül O XXII. 4o5", 413))
    ledger.resolve(_sequential_policy({"c0": [27]}))
    before, broken, after = ledger.claims
    assert before.accepted_number == 27

    assert broken.numeral["status"] == roman.INVALID_SYNTAX
    assert broken.numeral["permissive_value"] == 30
    assert broken.accepted_number is None
    assert broken.disposition == cc.INVALID_NUMERAL

    # nadie hereda de él
    assert after.anchor_number != 30
    assert after.anchor_claim != broken.claim_id
    # y el 30 sigue sin dueño
    assert 30 not in {c.accepted_number for c in ledger.accepted()}


def test_a_claim_never_enters_the_accepted_state_provisionally():
    """No hay marcha atrás porque no hay marcha adelante prematura.

    En ningún momento un reclamo que acaba rechazado tiene
    `accepted_number`. Se comprueba observando el ancla que vio cada
    reclamo aceptado: siempre es otro reclamo aceptado.
    """
    ledger = _chain(("CAPÍTULO I", 100), ("CAPÍTULO XXL", 102),
                    ("CAPÍTULO II", 104), ("CAPÍTULO IIL", 106),
                    ("CAPÍTULO III", 108))
    ledger.resolve(_sequential_policy())
    assert [c.accepted_number for c in ledger.claims] == [1, None, 2, None, 3]
    accepted = {c.claim_id for c in ledger.accepted()}
    for claim in ledger.claims:
        if claim.anchor_claim is not None:
            assert claim.anchor_claim in accepted, (
                f"{claim.claim_id} se ancló en {claim.anchor_claim}, "
                f"que no está aceptado")
        if claim.disposition != cc.ACCEPTED:
            assert claim.accepted_number is None
            assert claim.accepted_round is None


def test_resolution_terminates_and_is_deterministic():
    def run():
        ledger = _chain(*[(f"CAPÍTULO {roman.to_roman(n)}", 100 + 2 * n)
                          for n in range(1, 25)])
        ledger.resolve(_sequential_policy())
        return [c.accepted_number for c in ledger.claims], ledger.rounds
    first, second = run(), run()
    assert first == second
    assert first[0] == list(range(1, 25))


def test_the_running_header_does_not_override_a_legible_numeral():
    """La cabecera nombra el capítulo EN CURSO, no el que empieza.

    Cuando un capítulo arranca a media plana, la cabecera de arriba
    todavía es la del anterior. Dejarla mandar sobre un numeral que se
    lee hacía dos destrozos reales en el tomo: el rótulo «CAPÍTULO IV.»
    de la plana 372 acababa archivado como capítulo 1, y dos rótulos
    consecutivos (Prov 26 y Prov 27) reclamaban ambos el 26.
    """
    # numeral legible: manda él, y sin corroboración no se acepta
    alone = structure.resolve_chapter(
        raw_numeral="CAPÍTULO IV.", header_chapters=[1], previous=None,
        book="Sir", sequence_available=False)
    assert alone.resolved is None
    assert alone.review_required

    # el mismo rótulo con la cabecera de acuerdo: se acepta como 4, no 1
    agreed = structure.resolve_chapter(
        raw_numeral="CAPÍTULO IV.", header_chapters=[4], previous=None,
        book="Sir", sequence_available=False)
    assert agreed.resolved == 4
    assert agreed.method == "direct_ocr"

    # y con numeral ilegible la cabecera sí puede, que es para lo que está
    unreadable = structure.resolve_chapter(
        raw_numeral="CAPÍTULO", header_chapters=[26], previous=25,
        book="Prov")
    assert unreadable.resolved == 26
    assert unreadable.method in ("running_header_correlated", "multi_signal")


def test_sequence_is_unavailable_without_an_accepted_anchor():
    """Sin nada aceptado detrás no existe «lo que tocaba».

    Si no, cualquier rótulo cuyo numeral se lea «I» -- y el
    reconocimiento produce muchos -- recibiría corroboración de secuencia
    por el mero hecho de no tener nada delante.
    """
    first = structure.resolve_chapter(
        raw_numeral="CAPÍTULO I", header_chapters=[], previous=None,
        book="Sir", sequence_available=True)
    assert first.resolved == 1          # primero del libro: sí vale

    stray = structure.resolve_chapter(
        raw_numeral="CAPÍTULO I", header_chapters=[], previous=None,
        book="Sir", sequence_available=False)
    assert stray.resolved is None       # a media travesía: no
    assert stray.review_required


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
    print(f"torresamat1835_chapter_claims_failures={failures}")
    sys.exit(1 if failures else 0)
