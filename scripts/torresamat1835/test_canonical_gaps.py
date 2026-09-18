"""
TORRES-1835-MISSING-CHAPTER-NUMBERS-123: los números que el canon esperaba.

    python3 test_canonical_gaps.py

Sin red y sin el testigo. Cerrados todos los reclamos, el tomo seguía sin
ocho números que la Vulgata numera: Ps 15, 16, 20, 25, 33, 129 y Sir 2,
48. Ninguno era un reclamo pendiente --no había nada que resolver--, y
ahí está el peligro de esta tanda:

    CANON GAP IS A QUESTION, NOT AN ANSWER

Que una versificación espere un número no obliga a esta edición a
imprimir esa división. Podía haberlos fundido, saltado o numerado de otra
manera, y cualquiera de esas cosas habría sido la respuesta correcta. Lo
que decide es la plana.

En este tomo la plana dijo que los ocho están impresos, y que no llegaron
al mapa por dos motivos distintos: seis renglones cayeron en la banda de
la cabecera corrida --donde ningún buscador de rótulos mira, porque ahí
vive el mobiliario de la página-- y dos no los produjo el
reconocimiento. Por eso se comprueban aquí las dos vías de recuperación,
y también --con fixtures, porque el tomo no los necesitó-- los desenlaces
que NO crean capítulo: una edición que omite o funde una división tiene
que poder quedar explicada sin inventarle un capítulo.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chapter_claims as cc
import image_reviews as ir
import page_parser
import recovery
import source_ocr
import structure
from layout import split_columns

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
METADATA = os.path.join(ROOT, "data", "torresamat1835",
                        "chapter_image_reviews.json")
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
GUTTER = 1690
PAGE_WIDTH = 3402
PAGE_HEIGHT = 4837
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


def _page(*, heading_at_top=None, scan_page=10, gap=False):
    """Una plana como las del tomo.

    `heading_at_top` pone un rótulo en la banda alta --donde la edición
    empieza salmo en cabeza de página y donde vive la cabecera corrida--.
    `gap` deja el hueco sin ningún renglón, que es el otro caso real.
    """
    rows = []
    if heading_at_top is not None:
        rows.append((heading_at_top, (1370, 240, 1995, 304)))
    else:
        rows.append(("LIBRO DE LOS SALMOS. 22", (1697, 195, 2117, 255)))
    rows.append(("Argumento del editor sobre este salmo.", (411, 400, 3029, 495)))
    if not gap:
        rows.append(("1 Verbum latinum psalmi.", (400, 700, 1600, 772)))
        rows.append(("1 Verso castellano del salmo.", (1700, 700, 3000, 772)))
    rows += [("2 Alterum verbum latinum.", (400, 900, 1600, 972)),
             ("2 Segundo verso castellano.", (1700, 900, 3000, 972)),
             ("3 Tertium verbum latinum.", (400, 1000, 1600, 1072)),
             ("3 Tercer verso castellano.", (1700, 1000, 3000, 1072))]
    fixture = {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                          "height": PAGE_HEIGHT,
                          "lines": [{"text": text, "bbox": list(bbox)}
                                    for text, bbox in rows]}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _review(**over):
    base = dict(
        id="fx-gap", book="Ps", scan_page=10,
        outcome=ir.BOUNDARY_AND_NUMBER, chapter_number=15,
        boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block=None, insert_before_block="p0010l0001",
        observed_printed_text="SALMO XV.", crop_bbox=(0, 0, 3402, 900),
        confidence=0.95, rationale="la plana imprime SALMO XV.",
        reviewer_method="render", pdf_page=11, heading_block="p0010l0000")
    base.update(over)
    return ir.ChapterImageReview(**base)


def _apply(page, reviews, **kw):
    entries = split_columns(page, gutter_hint=GUTTER)
    kw.setdefault("book", "Ps")
    return recovery.apply_verified_image_reviews(
        page, entries, reviews, source=SOURCE, **kw)


def _parse(page, reviews=()):
    _ed, _stats, walker = page_parser.parse_volume(
        [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        image_reviews={page.scan_page: list(reviews)})
    return walker


def _metadata():
    with open(METADATA, encoding="utf-8") as handle:
        return json.load(handle)


def _gap_reviews(data=None):
    data = data or _metadata()
    return [r for r in data["reviews"]
            if r.get("discovered_by") == "canonical_chapter_gap"]


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


# ======================================================================
# A-B. Un hueco del canon es una pregunta
# ======================================================================
def test_A_a_canonical_gap_alone_never_creates_a_chapter():
    # La plana no imprime nada donde el canon espera un número: sin
    # revisión no aparece ningún capítulo, por mucho que falte.
    walker = _parse(_page(gap=True))
    assert walker.ledger.claims == [], "sin rótulo no hay nada que numerar"

    # Y con rótulo cuyo numeral no se lee tampoco: el canon se conoce
    # --sirve de cota de cordura sobre lo que SÍ se lee-- pero no puede
    # rellenar el único número que falta.
    assert structure.chapter_limit("Ps") == 150
    walker = _parse(_page(heading_at_top="SALMO X?«"))
    numbered = [c for c in walker.ledger.claims
                if c.accepted_number is not None]
    assert numbered == [], "el hueco del canon no completa un numeral ilegible"


def test_B_a_reviewed_gap_closes_without_creating_a_chapter():
    # Una edición que no imprime la división deja el hueco explicado y
    # el mapa intacto: eso es lo que hace `no_boundary`.
    page = _page(gap=True)
    review = _review(outcome=ir.NO_BOUNDARY, chapter_number=None,
                     boundary_confirmed=False, numeral_confirmed=False,
                     heading_block=None, insert_before_block=None,
                     observed_printed_text=None,
                     rationale="la edición no imprime aquí ninguna división")
    out, records = _apply(page, [review])
    assert [r.action for r in records] == [recovery.REJECTED]
    assert len(out) == len(split_columns(page, gutter_hint=GUTTER))
    assert all(getattr(e, "recovery", None) is None for e in out)
    assert _parse(page, [review]).ledger.claims == []


# ======================================================================
# C-G. Las dos vías de recuperación
# ======================================================================
def test_C_a_printed_heading_from_the_image_creates_a_claim():
    page = _page(heading_at_top="SALMO XV.")
    walker = _parse(page, [_review()])
    claims = [c for c in walker.ledger.claims
              if c.source == cc.FROM_IMAGE_REVIEW]
    assert len(claims) == 1
    assert claims[0].accepted_number == 15
    assert claims[0].disposition == cc.ACCEPTED


def test_D_an_existing_block_is_marked_and_never_duplicated():
    page = _page(heading_at_top="SALMO XV.")
    entries = split_columns(page, gutter_hint=GUTTER)
    out, records = _apply(page, [_review()])
    assert len(out) == len(entries), "no se añade ningún renglón"
    assert records[0].action in (recovery.RESOLVED, recovery.CONFIRMED)
    assert records[0].found_by == recovery.BY_NAME
    assert records[0].target_block == "p0010l0000"


def test_E_a_heading_with_no_recognition_line_is_inserted():
    page = _page(gap=True)
    entries = split_columns(page, gutter_hint=GUTTER)
    review = _review(heading_block=None,
                     insert_after_block="p0010l0001",
                     insert_before_block="p0010l0002",
                     crop_bbox=(0, 300, 3402, 1000))
    out, records = _apply(page, [review])
    assert [r.action for r in records] == [recovery.INSERTED]
    assert len(out) == len(entries) + 1
    added = [e for e in out if getattr(e, "recovery", None)]
    assert len(added) == 1 and added[0].line.raw_text == "SALMO XV."


def test_F_a_derived_event_is_not_a_recognition_block():
    page = _page(gap=True)
    review = _review(heading_block=None,
                     insert_after_block="p0010l0001",
                     insert_before_block="p0010l0002",
                     crop_bbox=(0, 300, 3402, 1000))
    _ed, stats, _walker = page_parser.parse_volume(
        [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        image_reviews={10: [review]})
    plain_ed, plain_stats, _w = page_parser.parse_volume(
        [_page(gap=True)], witness="fx", volume="3", book="Ps",
        gutter_hint=GUTTER, with_walker=True, recovery_source=SOURCE)
    assert stats["ocr_blocks"] == plain_stats["ocr_blocks"]
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_G_the_raw_ocr_is_never_rewritten():
    page = _page(heading_at_top="SALMO XV,")   # lo que dejó la máquina
    walker = _parse(page, [_review()])
    claim = next(c for c in walker.ledger.claims
                 if c.source == cc.FROM_IMAGE_REVIEW)
    assert claim.provenance["raw_ocr_heading"] == "SALMO XV,"
    assert claim.provenance["observed_printed_text"] == "SALMO XV."
    for entry in _gap_reviews():
        if entry["raw_ocr_heading"] is not None:
            assert entry["observed_printed_text"], entry["id"]


# ======================================================================
# H-K. Lo que no puede decidir un número
# ======================================================================
def test_H_canon_is_not_authority():
    page = _page(gap=True)
    walker = _parse(page)
    assert walker.ledger.claims == []
    audit = _audit()
    gaps = audit["canonical_chapter_gap_reviews"]
    for row in gaps["gaps"]:
        if row["present_in_chapter_map"]:
            assert row["review_id"], row
            assert row["number_source"] == "image_review_roman", row


def test_I_sequence_is_not_authority():
    resolution = structure.resolve_chapter(
        raw_numeral="", header_chapters=[], previous=14, book="Ps",
        sequence_available=False)
    assert resolution.resolved is None


def test_J_previous_plus_one_is_not_authority():
    resolution = structure.resolve_chapter(
        raw_numeral="SALMO", header_chapters=[], previous=14, book="Ps",
        sequence_available=False)
    assert resolution.resolved is None


def test_K_next_minus_one_is_not_authority():
    source = open(os.path.join(DIR, "structure.py"), encoding="utf-8").read()
    for word in ("next_accepted", "following_chapter", "next_number"):
        assert word not in source, word


# ======================================================================
# L-P. Los desenlaces que NO crean capítulo
# ======================================================================
def _absence(kind):
    return _review(outcome=ir.NO_BOUNDARY, chapter_number=None,
                   boundary_confirmed=False, numeral_confirmed=False,
                   heading_block=None, insert_before_block=None,
                   observed_printed_text=None,
                   rationale=f"la plana muestra {kind}")


def test_L_omitted_in_print_has_no_structural_effect():
    page = _page(gap=True)
    out, records = _apply(page, [_absence("omitted_in_print")])
    assert [r.action for r in records] == [recovery.REJECTED]
    assert len(out) == len(split_columns(page, gutter_hint=GUTTER))


def test_M_merged_with_previous_has_no_structural_effect():
    page = _page(gap=True)
    out, _records = _apply(page, [_absence("merged_with_previous")])
    assert all(getattr(e, "recovery", None) is None for e in out)


def test_N_merged_with_next_has_no_structural_effect():
    page = _page(gap=True)
    out, _records = _apply(page, [_absence("merged_with_next")])
    assert all(getattr(e, "recovery", None) is None for e in out)


def test_O_local_renumbering_renumbers_nothing_by_itself():
    page = _page(heading_at_top="SALMO XV.")
    before = _parse(page).ledger.claims
    after = _parse(page, [_absence("local_renumbering_difference")]).ledger.claims
    assert [c.accepted_number for c in before] == [c.accepted_number for c in after]


def test_P_an_editorial_division_without_number_creates_no_numbered_chapter():
    page = _page(gap=True)
    review = _review(outcome=ir.BOUNDARY_ONLY, chapter_number=None,
                     numeral_confirmed=False, heading_block=None,
                     insert_after_block="p0010l0001",
                     insert_before_block="p0010l0002",
                     crop_bbox=(0, 300, 3402, 1000),
                     observed_printed_text="§. II.")
    walker = _parse(page, [review])
    claims = [c for c in walker.ledger.claims
              if c.source == cc.FROM_IMAGE_REVIEW]
    assert len(claims) == 1
    assert claims[0].accepted_number is None, "frontera sí, número no"
    assert claims[0].disposition == cc.UNRESOLVED


# ======================================================================
# Q-T. Evidencia de apoyo y semántica de la cola
# ======================================================================
def test_Q_latin_evidence_is_support_and_not_the_number():
    entries = _gap_reviews()
    assert entries, "la tanda dejó sus revisiones"
    for entry in entries:
        # el número sale del numeral del rótulo castellano
        assert entry["observed_printed_numeral"] in entry["observed_printed_text"]
        assert entry["chapter_number"] == entry["expected_chapter"]


def test_R_a_running_header_does_not_create_a_chapter():
    # Un rótulo en la banda alta sólo llega a capítulo porque una
    # revisión del facsímil lo nombra; sin ella es mobiliario de plana.
    page = _page(heading_at_top="SALMO XV.")
    assert _parse(page).ledger.claims == [], "sin revisión no hay capítulo"
    assert _parse(page, [_review()]).ledger.claims


def test_S_a_reviewed_gap_stays_in_history_but_not_pending():
    gaps = _audit()["canonical_chapter_gap_reviews"]
    assert gaps["candidates_total"] == 8
    assert gaps["reviewed"] == 8
    assert all(row["review_id"] for row in gaps["gaps"])
    assert all(row["still_pending"] is False for row in gaps["gaps"])


def test_T_the_canonical_gap_queue_is_empty_after_the_review():
    gaps = _audit()["canonical_chapter_gap_reviews"]
    assert gaps["pending_review"] == 0
    assert gaps["unexplained"] == 0


# ======================================================================
# U-X. Nada de lo anterior se ha movido
# ======================================================================
def test_U_the_previously_accepted_claims_are_stable():
    audit = _audit()
    claims = {c["block_id"]: c for c in audit["chapter_claims"]["claims"]}
    assert claims["p0015l0003"]["accepted_number"] == 1
    assert claims["p0015l0003"]["number_source"] == "written_ordinal"
    assert claims["p0319r0002"]["number_source"] == "image_review_ordinal"
    assert claims["p0196l0047"]["accepted_number"] == 142


def test_V_unresolved_claims_stay_at_zero():
    audit = _audit()
    assert audit["chapter_claims"]["unresolved"] == 0
    queue = audit["numeral_image_review"]["review_queue_next"]
    assert queue == []
    assert audit["image_review_queue"]["total"] == 0


def test_W_the_false_headings_stay_rejected():
    audit = _audit()
    assert audit["chapter_claims"]["rejected_false_heading"] == 6
    rejected = {r["block_id"]
                for r in audit["chapter_claims"]["rejected_false_heading_detail"]}
    assert "p0196l0050" in rejected
    claims = {c["block_id"]: c for c in audit["chapter_claims"]["claims"]}
    for block in rejected:
        assert claims[block]["accepted_number"] is None


def test_X_no_chapter_number_is_claimed_twice():
    audit = _audit()
    accepted = [c for c in audit["chapter_claims"]["claims"]
                if c["disposition"] == "accepted"]
    keys = collections.Counter((c["book"], c["accepted_number"]) for c in accepted)
    assert not [k for k, n in keys.items() if n > 1]
    per_book = audit["canonical_chapter_gap_reviews"]["accepted_physical"]
    assert sum(per_book.values()) == audit["chapter_claims"]["accepted"]


# ======================================================================
# Y-AJ. Integridad del tomo y del código
# ======================================================================
def test_Y_no_block_was_lost():
    audit = _audit()
    assert (audit["materialized_verse_refs"]
            + audit["verse_refs_in_review_slots"]) == audit["verse_refs"]
    assert audit["metrics"]["ocr_blocks"] == 57700


def test_Z_no_block_has_two_owners():
    audit = _audit()
    assert audit["duplicate_refs"] == []
    blocks = [c["block_id"] for c in audit["chapter_claims"]["claims"]]
    assert len(blocks) == len(set(blocks))


def test_AA_no_chapter_overlaps_another():
    audit = _audit()
    assert audit["out_of_order_chapters"] == []
    assert audit["chapter_claims"]["collision_groups"] == []
    assert audit["chapter_claims"]["competing_claim"] == 0


def test_AB_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AC_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_AD_ocr_blocks_unchanged():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AE_the_source_hashes_are_the_reviewed_ones():
    source = _metadata()["visual_source"]
    assert source["sha256"] == (
        "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346")
    assert not source.get("may_produce_release_artifact")
    spans = {s["book"]: s["first_page"]
             for s in _audit()["structure_resolution"]["book_spans"]}
    assert spans == {"Ps": 14, "Prov": 209, "Eccl": 275, "Song": 300,
                     "Wis": 318, "Sir": 362, "Isa": 496}


def test_AF_applying_the_reviews_twice_changes_nothing():
    page = _page(heading_at_top="SALMO XV.")
    once, first = _apply(page, [_review()])
    twice, second = recovery.apply_verified_image_reviews(
        page, once, [_review()], source=SOURCE, book="Ps")
    assert len(once) == len(twice)
    assert [r.action for r in second] == [recovery.REDUNDANT]
    inserted = _page(gap=True)
    review = _review(heading_block=None, insert_after_block="p0010l0001",
                     insert_before_block="p0010l0002",
                     crop_bbox=(0, 300, 3402, 1000))
    one, _r = _apply(inserted, [review])
    two, records = recovery.apply_verified_image_reviews(
        inserted, one, [review], source=SOURCE, book="Ps")
    assert len(one) == len(two), "no se inserta dos veces"
    assert [r.action for r in records] == [recovery.REDUNDANT]


def test_AG_the_same_input_gives_the_same_result():
    page = _page(heading_at_top="SALMO XV.")
    first = _parse(page, [_review()]).ledger.claims[0].as_dict()
    second = _parse(page, [_review()]).ledger.claims[0].as_dict()
    assert first == second


def test_AH_the_review_layer_works_offline():
    for name in ("recovery.py", "image_reviews.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for word in ("urllib", "requests", "socket."):
            assert word not in source, f"{name} usa {word!r}"


def test_AI_no_production_module_decides_by_page():
    pages = {29, 30, 37, 43, 53, 184, 368, 480}
    for name in ("recovery.py", "page_parser.py", "chapter_claims.py",
                 "image_reviews.py", "structure.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            for operand in [node.left] + list(node.comparators):
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, f"{name}: {operand.value}"


def test_AJ_no_expected_chapter_table_lives_in_the_code():
    numbers = {15, 16, 20, 25, 33, 129, 48}
    for name in ("recovery.py", "page_parser.py", "chapter_claims.py",
                 "image_reviews.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        tree = ast.parse(source)
        docstrings = set()
        for node in ast.walk(tree):
            if isinstance(node, (ast.Module, ast.ClassDef, ast.FunctionDef,
                                 ast.AsyncFunctionDef)):
                first = node.body[0] if node.body else None
                if isinstance(first, ast.Expr) and \
                        isinstance(first.value, ast.Constant) and \
                        isinstance(first.value.value, str):
                    docstrings.add(id(first.value))
        for node in ast.walk(tree):
            if isinstance(node, ast.Compare):
                for operand in [node.left] + list(node.comparators):
                    if isinstance(operand, ast.Constant) and \
                            isinstance(operand.value, int) and \
                            not isinstance(operand.value, bool):
                        assert operand.value not in numbers, \
                            f"{name}: decide por el capítulo {operand.value}"
            if isinstance(node, ast.Constant) and id(node) not in docstrings \
                    and isinstance(node.value, str):
                for marker in ("expected_gap", "missing_chapters",
                               "chapter_deficit"):
                    assert marker not in node.value, f"{name}: {marker}"


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
    print(f"torresamat1835_canonical_gaps_failures={failures}")
    sys.exit(1 if failures else 0)
