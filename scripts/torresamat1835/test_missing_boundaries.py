"""
TORRES-1835-MISSING-PSALM-BOUNDARIES-117: el rótulo estaba, roto por dentro.

    python3 test_missing_boundaries.py

Sin red y sin el testigo. En las tres planas de esta tanda el
reconocimiento SÍ produjo el renglón del rótulo, pero destrozó la
PALABRA de división en vez del numeral:

    S A L M O X L.        (compuesta letra a letra)
    6'ALMO XCI.           (la S leída como «6'»)
    5ALMO CXXXVI.         (la S leída como «5»)

Ninguna lectura del texto puede ver ahí una frontera, así que el salmo
no se abría y su contenido esperaba en la cola de revisión. La revisión
del facsímil nombra ese bloque -- `heading_block` -- y la frontera se
marca sobre el renglón que ya existe, sin añadir ninguno.

Los textos y la geometría de las planas reales viven aquí como fixtures;
el código de producción no nombra ninguna plana.
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
import source_ocr
from image_reviews import BOUNDARY_AND_NUMBER, BOUNDARY_ONLY
from layout import split_columns
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")

#: La plana del fixture: dos columnas pobladas, el rótulo entre medias
#: con la palabra rota, su argumento debajo y los versículos del salmo
#: nuevo detrás.
HEADING_TEXT = "S A L M O X L."
#: La plana lleva un rótulo ANTERIOR que el reconocimiento sí escribió
#: bien: así se puede comprobar que lo de antes de la frontera
#: recuperada se queda donde estaba y sólo se mueve lo de después.
PREVIOUS_HEADING = "SALMO XXXIX."
_LINES = [
    ("header", "LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255)),
    ("previous_heading", PREVIOUS_HEADING, (1417, 500, 2031, 563)),
    ("previous_latin", "17 Verbum latinum psalmi prioris.", (400, 700, 1600, 772)),
    ("previous_verse", "17 Verso del salmo anterior.", (1700, 700, 3000, 772)),
    ("previous_latin2", "continuatio latina eiusdem versus.", (400, 790, 1600, 862)),
    ("previous_verse2", "18 Ultimo verso del salmo anterior.", (1700, 790, 3000, 862)),
    ("heading", HEADING_TEXT, (1417, 1000, 2031, 1063)),
    ("argument", "Argumento del editor sobre este salmo.", (411, 1200, 3029, 1295)),
    ("latin", "1 In finem, Psalmus ipsi David. XL.", (400, 1400, 1600, 1472)),
    ("verse", "2 Primer verso del salmo nuevo.", (1700, 1400, 3000, 1472)),
    ("latin2", "continuatio latina sequentis.", (400, 1490, 1600, 1562)),
    ("verse2", "3 Segundo verso del salmo nuevo.", (1700, 1490, 3000, 1562)),
]
BLOCK = {name: f"p0010l{index:04d}"
         for index, (name, _text, _bbox) in enumerate(_LINES)}
ANCHOR_AFTER = BLOCK["previous_verse2"]
HEADING_BLOCK = BLOCK["heading"]
ANCHOR_BEFORE = BLOCK["argument"]


def _page(*, heading=HEADING_TEXT, scan_page=10):
    lines = []
    for name, text, bbox in _LINES:
        if name == "heading":
            if heading is None:
                continue
            text = heading
        lines.append({"text": text, "bbox": list(bbox)})
    fixture = {"pages": [{"scan_page": scan_page, "width": 3402,
                          "height": 4837, "lines": lines}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _entries(page):
    return split_columns(page, gutter_hint=GUTTER)


def _review(**over):
    base = dict(
        id="fx-40", book="Ps", scan_page=10, outcome=BOUNDARY_AND_NUMBER,
        chapter_number=40, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block=ANCHOR_AFTER, insert_before_block=ANCHOR_BEFORE,
        heading_block=HEADING_BLOCK, observed_printed_text="SALMO XL.",
        crop_bbox=(0, 800, 3402, 1300), confidence=0.95,
        rationale="the facsimile prints SALMO XL. across the gutter",
        reviewer_method="render", pdf_page=11)
    base.update(over)
    return ir.ChapterImageReview(**base)


def _apply(page, entries, reviews, **kw):
    kw.setdefault("book", "Ps")
    return recovery.apply_verified_image_reviews(
        page, entries, reviews, source=SOURCE, **kw)


def _ids(entries, scan_page=10):
    return [recovery._entry_block_id(e, scan_page) for e in entries]


def _parse(page, reviews):
    return page_parser.parse_volume(
        [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        image_reviews={page.scan_page: list(reviews)})


# ======================================================================
# A/B/C/D. La frontera sale de la imagen, y se marca sobre lo que hay
# ======================================================================
def test_A_a_heading_the_recognition_broke_is_recovered_from_the_image():
    page = _page()
    entries = _entries(page)
    # sin la revisión la palabra rota no abre nada: sólo se ve el rótulo
    # anterior, el que la máquina escribió bien
    _ed, _s, plain = _parse(page, [])
    assert [c.raw_heading for c in plain.ledger.claims] == [PREVIOUS_HEADING]

    out, records = _apply(page, entries, [_review()])
    assert [r.action for r in records] == [recovery.RESOLVED]
    # no se añade ningún renglón: se marca el que ya existía
    assert len(out) == len(entries)
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert len(marked) == 1
    assert marked[0].line.raw_text == HEADING_TEXT
    assert marked[0].recovery.chapter_number == 40
    assert records[0].target_block == HEADING_BLOCK


def _recovered_claim(walker):
    return next(c for c in walker.ledger.claims
                if c.source == cc.FROM_IMAGE_REVIEW)


def test_B_the_recovered_claim_carries_visual_heading_evidence():
    page = _page()
    _ed, _s, walker = _parse(page, [_review()])
    claim = _recovered_claim(walker)
    assert claim.disposition == cc.ACCEPTED
    assert claim.accepted_number == 40
    assert claim.source == cc.FROM_IMAGE_REVIEW
    # el contrato de 116 no se esquiva: el reclamo lleva su evidencia de
    # rótulo, y es la de la imagen
    assert claim.heading is not None
    assert claim.heading["is_heading"] is True
    assert claim.heading["source"] == "image_review"
    assert claim.is_structural_heading
    assert claim.provenance["review_id"] == "fx-40"
    assert claim.provenance["source_sha256"] == SHA


def test_C_the_number_comes_from_the_image_and_not_from_the_sequence():
    """Los vecinos no deciden: sin numeral en la imagen no hay número."""
    page = _page()
    _ed, _s, walker = _parse(page, [_review(
        outcome=BOUNDARY_ONLY, chapter_number=None, numeral_confirmed=False)])
    claim = _recovered_claim(walker)
    assert claim.accepted_number is None
    assert claim.review_required
    # y con el numeral leído, el número es ESE, no el del vecino + 1
    _ed, _s, walker = _parse(page, [_review(chapter_number=91,
                                            observed_printed_text="SALMO XCI.")])
    assert _recovered_claim(walker).accepted_number == 91


def test_D_an_unreadable_recovered_numeral_stays_none():
    page = _page()
    out, records = _apply(page, _entries(page), [_review(
        outcome=BOUNDARY_ONLY, chapter_number=None, numeral_confirmed=False)])
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert marked[0].recovery.chapter_number is None
    assert marked[0].recovery.review_required
    assert records[0].chapter_number is None


# ======================================================================
# E/F/G. Las anclas, y el bloque señalado, fallan cerrado
# ======================================================================
def _fails(review, *, contains=None):
    page = _page()
    entries = _entries(page)
    out, records = _apply(page, entries, [review])
    assert records[0].action == recovery.FAILED, records[0].action
    assert len(out) == len(entries)
    assert not [e for e in out if getattr(e, "recovery", None)]
    if contains:
        assert contains in records[0].reason, records[0].reason
    return records[0]


def test_E_anchors_are_validated():
    page = _page()
    problem = recovery.validate_anchors(_review(), _entries(page), page,
                                        book="Ps")
    assert problem is None


def test_F_a_missing_anchor_fails_closed():
    _fails(_review(insert_after_block=None), contains="no insert_after_block")
    _fails(_review(insert_before_block=None), contains="no insert_before_block")
    _fails(_review(insert_after_block="p0010l0099"), contains="is not a block")


def test_G_reversed_anchors_fail_closed():
    _fails(_review(insert_after_block=ANCHOR_BEFORE,
                   insert_before_block=ANCHOR_AFTER),
           contains="out of reading order")


def test_a_heading_block_outside_the_anchors_fails_closed():
    _fails(_review(heading_block="p0010l0009"),
           contains="does not fall between the anchors")
    _fails(_review(heading_block="p0010l0099"),
           contains="is not a block of scan page")
    # y el esquema lo dice antes de llegar aquí
    payload = {"visual_source": {"sha256": SHA}, "reviews": [{
        "id": "x", "book": "Ps", "scan_page": 10,
        "outcome": BOUNDARY_AND_NUMBER, "chapter_number": 40,
        "boundary_confirmed": True, "numeral_confirmed": True,
        "insert_after_block": ANCHOR_AFTER, "insert_before_block": ANCHOR_BEFORE,
        "heading_block": "p0099l0005", "rationale": "r"}]}
    assert any("not on scan page" in p
               for p in ir.validate(payload, page_count=652))


# ======================================================================
# H/I/J. La frontera se aplica a nivel de BLOQUE
# ======================================================================
def test_HIJ_blocks_before_stay_and_blocks_after_move():
    page = _page()
    edition, _s, walker = _parse(page, [_review()])
    chapters = edition.books["Ps"].chapters
    assert 40 in chapters

    owner = {}
    for number, chapter in chapters.items():
        for verse_number, verse in chapter.verses.items():
            for block in verse.blocks:
                owner[block.provenance.block_id] = (number, verse_number)
        for block in chapter.paratext:
            owner.setdefault(block.provenance.block_id, (number, None))

    # lo de antes del rótulo se queda con el rótulo de antes -- aquí sin
    # número, en su hueco de trabajo, que es donde estaba...
    previous = owner[BLOCK["previous_verse"]][0]
    assert previous != 40
    for name in ("previous_verse", "previous_verse2"):
        assert owner[BLOCK[name]][0] == previous, name
    # ...y lo de después es del salmo recuperado
    for name in ("argument", "verse", "verse2"):
        assert owner[BLOCK[name]][0] == 40, name
    # el rótulo entra con su identificador de recuperación, no como «l»
    recovered = HEADING_BLOCK.replace("l", "r", 1)
    assert recovered in owner and owner[recovered][0] == 40
    assert HEADING_BLOCK not in owner


def test_the_recovered_heading_is_paratext_and_not_a_verse():
    page = _page()
    edition, _s, _w = _parse(page, [_review()])
    chapter = edition.books["Ps"].chapters[40]
    headings = [b for b in chapter.paratext
                if b.kind is BlockKind.CHAPTER_HEADING]
    assert len(headings) == 1
    assert headings[0].raw_text == HEADING_TEXT
    assert headings[0].recovered["review_id"] == "fx-40"
    for verse in chapter.verses.values():
        for block in verse.blocks:
            assert block.raw_text != HEADING_TEXT


# ======================================================================
# K. El crudo del reconocimiento no se toca
# ======================================================================
def test_K_raw_ocr_is_untouched():
    fixture = {"pages": [{"scan_page": 10, "width": 3402, "height": 4837,
                          "lines": [{"text": HEADING_TEXT,
                                     "bbox": [1417, 1000, 2031, 1063]}]}]}
    before = copy.deepcopy(fixture)
    list(source_ocr.pages_from_fixture(fixture))
    assert fixture == before

    page = _page()
    texts_before = [e.line.raw_text for e in _entries(page)]
    out, _records = _apply(page, _entries(page), [_review()])
    assert [e.line.raw_text for e in out] == texts_before
    marked = [e for e in out if getattr(e, "recovery", None)][0]
    # el renglón sigue diciendo lo que dijo la máquina; lo que se añade
    # es lo que dice la plana, al lado
    assert marked.line.raw_text == HEADING_TEXT
    assert marked.recovery.provenance.observed_printed_text == "SALMO XL."


# ======================================================================
# S/T/U. Idempotencia, determinismo, sin red
# ======================================================================
def test_S_applying_twice_changes_nothing_the_second_time():
    page = _page()
    entries = _entries(page)
    once, first = _apply(page, entries, [_review()])
    twice, second = _apply(page, once, [_review()])
    assert first[0].action == recovery.RESOLVED
    assert second[0].action == recovery.REDUNDANT
    assert _ids(twice) == _ids(once)
    assert sum(1 for e in twice if getattr(e, "recovery", None)) == 1

    # y el modelo resultante es el mismo capítulo con los mismos versos
    def refs(entries_in):
        edition, _s, _w = page_parser.parse_volume(
            [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
            with_walker=True, recovery_source=SOURCE,
            image_reviews={10: [_review()]})
        return {(n, v) for n, ch in edition.books["Ps"].chapters.items()
                for v in ch.verses}
    assert refs(once) == refs(twice)


def test_T_deterministic():
    page = _page()

    def run():
        _ed, _s, walker = _parse(page, [_review()])
        return json.dumps(walker.ledger.report(), sort_keys=True, default=str)
    assert run() == run()


def test_U_offline_and_no_page_hardcoded_in_production():
    import ast
    for name in ("recovery.py", "image_reviews.py", "page_parser.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for forbidden in ("urllib", "requests", "http://", "https://",
                          "socket"):
            assert forbidden not in source, f"{name}: {forbidden}"
        tree = ast.parse(source)
        docstrings = set()
        for node in ast.walk(tree):
            if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef,
                                 ast.AsyncFunctionDef)):
                doc = ast.get_docstring(node, clean=False)
                if doc is not None:
                    docstrings.add(doc)
        # Ninguna comparación puede decidir POR LA PLANA. Se mira qué se
        # compara, no el número suelto: 64 es también la longitud de un
        # sha256, y prohibir la cifra sin mirar contra qué se usa sería
        # una comprobación de mentira.
        pages = {64, 65, 133, 134, 190, 191}

        def names_a_page(node):
            if isinstance(node, ast.Name):
                return "page" in node.id or "block" in node.id
            if isinstance(node, ast.Attribute):
                return ("page" in node.attr or "block" in node.attr
                        or names_a_page(node.value))
            if isinstance(node, ast.Subscript):
                key = getattr(node.slice, "value", None)
                return isinstance(key, str) and ("page" in key or "block" in key)
            return False

        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            operands = [node.left] + list(node.comparators)
            if not any(names_a_page(o) for o in operands):
                continue
            for operand in operands:
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, \
                        f"{name}: decides by page {operand.value}"
        texts = [n.value for n in ast.walk(tree)
                 if isinstance(n, ast.Constant) and isinstance(n.value, str)
                 and n.value not in docstrings]
        for marker in ("p0064", "p0133", "p0190", "SALMO XL", "SALMO XCI",
                       "SALMO CXXXVI"):
            assert not any(marker in t for t in texts), f"{name}: {marker}"


# ======================================================================
# V/W/X + L/M/N/O/P/Q/R. El tomo real, medido sobre el audit
# ======================================================================
def _audit():
    path = os.path.join(DIR, "..", "..", "build", "torresamat1835-audit",
                        "volume3.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


#: Las tres regiones de esta tanda: (review, plana, bloque del rótulo,
#: lo que el reconocimiento dejó, capítulo impreso).
REGIONS = [("ps-40-p64", 64, "p0064l0039", "S A L M O X L.", 40),
           ("ps-91-p133", 133, "p0133l0071", "6'ALMO XCI.", 91),
           ("ps-136-p190", 190, "p0190l0059", "5ALMO CXXXVI.", 136)]


def test_the_shipped_boundary_reviews_validate():
    payload = ir.load()
    assert ir.validate(payload, page_count=652) == []
    shipped = {r["id"]: r for r in payload["reviews"]}
    for review_id, page, block, _raw, number in REGIONS:
        review = shipped[review_id]
        assert review["batch"] == "batch-117"
        assert review["book"] == "Ps"
        assert review["scan_page"] == page
        assert review["pdf_page"] == page + 1
        assert review["heading_block"] == block
        assert review["chapter_number"] == number
        assert review["observed_printed_text"] == f"SALMO {review['observed_printed_numeral']}."
        assert review["insert_after_block"] and review["insert_before_block"]
        assert review["crop_bbox"] and len(review["crop_bbox"]) == 4
        assert len(review["rationale"]) > 120
        assert review["confidence"] >= 0.9


def test_VWX_each_region_is_recovered_in_the_volume():
    report = _audit()
    if report is None:
        print("  (saltado: no hay audit en build/)")
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    applications = {a["review_id"]: a
                    for a in report["chapter_image_recovery"]["applications"]}
    ownership = {row["review_id"]: row for row in
                 report["chapter_image_recovery"]["recovered_chapter_ownership"]}
    for review_id, page, block, raw, number in REGIONS:
        record = applications[review_id]
        assert record["action"] == recovery.RESOLVED, review_id
        assert record["target_block"] == block
        assert record["chapter_number"] == number
        # el reclamo vive con el identificador de recuperación del MISMO
        # renglón: no hay dos rótulos donde el impreso tiene uno
        recovered_id = f"p{page:04d}r{block[6:]}"
        claim = claims[recovered_id]
        assert claim["accepted_number"] == number
        assert claim["disposition"] == cc.ACCEPTED
        # las dos lecturas conviven: el reclamo lleva lo que IMPRIME la
        # plana, y su procedencia guarda lo que dijo la máquina
        assert claim["raw_heading"] == f"SALMO {claim['numeral']['token']}." \
            or claim["raw_heading"].startswith("SALMO")
        assert claim["provenance"]["raw_ocr_heading"] == raw
        assert claim["heading"]["is_heading"] is True
        assert claim["provenance"]["review_id"] == review_id
        row = ownership[review_id]
        assert row["chapter"] == number
        assert page in row["scan_pages"]
        assert row["source_blocks_now_in_this_chapter"] > 0
        assert row["verse_refs_in_this_chapter"] > 0
        # antes de la revisión ese contenido no estaba publicado
        assert row["verse_refs_already_published_before"] == 0


def test_LM_the_false_inscriptions_stay_rejected():
    report = _audit()
    if report is None:
        return
    rejected = {r["block_id"]
                for r in report["heading_claim_validation"]["false_headings"]}
    for block in ("p0064l0066", "p0101l0062", "p0133l0079", "p0135l0059",
                  "p0190l0073", "p0196l0050"):
        assert block in rejected, block
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for block in rejected:
        assert claims[block]["disposition"] == cc.REJECTED_FALSE_HEADING
        assert claims[block]["accepted_number"] is None
    # y el de la plana 196 no vuelve a ser el salmo primero: Ps 1 es
    # ahora el rótulo de la plana 15, y por su palabra impresa, no por
    # esta inscripción ni por ninguna corrección de frontera.
    ones = [c for c in report["chapter_claims"]["claims"]
            if c["book"] == "Ps" and c["accepted_number"] == 1]
    assert [c["block_id"] for c in ones] == ["p0015l0003"], ones
    assert ones[0]["number_source"] == "written_ordinal"


def test_N_the_true_psalm_one_owes_nothing_to_boundary_recovery():
    report = _audit()
    if report is None:
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    real = claims["p0015l0003"]
    assert real["raw_heading"].upper().startswith("SALMO PRIMERO")
    assert real["heading"]["is_heading"] is True
    # Su frontera nunca hizo falta recuperarla --el rótulo está impreso
    # y el reconocimiento lo produjo entero--, y su numeral sigue sin
    # leerse: el número se lo dio la palabra, en la tanda 121.
    assert real["numeral"]["status"] == "no_numeral"
    assert real["number_source"] == "written_ordinal"
    assert real["accepted_number"] == 1


def test_OPQR_the_volume_keeps_its_integrity():
    report = _audit()
    if report is None:
        return
    assert report["duplicate_refs"] == []
    assert report["out_of_order_refs"] == []
    assert report["metrics"]["ocr_blocks"] == 57700
    assert report["materialized_verse_refs"] + \
        report["verse_refs_in_review_slots"] == report["verse_refs"]
    spans = dict(report["book_boundary_resolution"]["spans_after"])
    assert spans == {"Ps": 14, "Prov": 209, "Eccl": 275, "Song": 300,
                     "Wis": 318, "Sir": 362, "Isa": 496}
    # ningún capítulo recuperado choca con otro reclamante
    assert report["chapter_image_recovery"]["recovered_number_collisions"] == []
    assert report["chapter_image_recovery"]["reviews_failed_anchor_validation"] == 0
    assert report["chapter_image_recovery"]["reviews_conflicting_number"] == 0
    assert report["chapter_image_recovery"]["reviews_colliding_with_existing_chapter"] == 0


def test_the_batches_are_reported_apart():
    report = _audit()
    if report is None:
        return
    batches = report["chapter_image_recovery"]["batches"]
    assert "batch-117" in batches
    assert batches["batch-117"]["reviews"] == 3
    assert batches["batch-117"]["by_outcome"] == {
        "boundary_and_number_confirmed": 3}
    assert sorted(batches["batch-117"]["review_ids"]) == \
        sorted(r[0] for r in REGIONS)


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
    print(f"torresamat1835_missing_boundaries_failures={failures}")
    sys.exit(1 if failures else 0)
