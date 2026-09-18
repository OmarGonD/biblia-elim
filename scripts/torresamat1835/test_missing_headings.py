"""
TORRES-1835-MISSING-HEADING-RECOVERY-119: ¿falta de verdad el rótulo?

    python3 test_missing_headings.py

Sin red y sin el testigo. La cola `missing_heading` señala planas donde
el cuerpo tiene un blanco lo bastante ancho como para haberse comido un
rótulo. Sólo la imagen dice si se lo comió, y en este tomo la respuesta
resultó ser que no: de los veinte candidatos, trece tenían el rótulo
impreso Y un bloque del reconocimiento representándolo -- con la palabra
y el numeral destrozados a la vez, que es lo que los hacía invisibles --
y los otros siete no tenían rótulo ninguno.

Eso parte la tanda en dos comprobaciones distintas:

    lo que se hizo         marcar el bloque que ya estaba, nunca añadir
                           uno al lado (`existing_ocr_heading_found`);

    lo que hay que poder   insertar un evento estructural derivado
    hacer                  cuando el rótulo está impreso y NO hay bloque
                           que lo represente. En el tomo no hizo falta,
                           así que ese camino se ejercita aquí con
                           fixtures: un día hará falta.

Y una que no se negocia: un evento derivado no es un bloque del
reconocimiento. No cuenta como `ocr_blocks` y no lleva su identificador.
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
from image_reviews import BOUNDARY_AND_NUMBER, BOUNDARY_ONLY, NO_BOUNDARY
from layout import split_columns
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690
PAGE_WIDTH = 3402
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")

#: La plana del fixture. `heading` es lo que el reconocimiento dejó donde
#: el impreso tiene el rótulo: None cuando no dejó nada -- el caso de un
#: rótulo de verdad ausente -- y un texto cuando dejó el renglón roto.
_PREVIOUS_HEADING = "SALMO XXXIX."


def _page(*, heading=None, scan_page=10):
    rows = [("header", "LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255)),
            ("previous_heading", _PREVIOUS_HEADING, (1417, 400, 2031, 463)),
            ("previous_latin", "17 Verbum latinum psalmi prioris.", (400, 700, 1600, 772)),
            ("previous_verse", "17 Verso del salmo anterior.", (1700, 700, 3000, 772)),
            ("previous_latin2", "continuatio latina eiusdem versus.", (400, 790, 1600, 862)),
            ("previous_verse2", "18 Ultimo verso del salmo anterior.", (1700, 790, 3000, 862))]
    if heading is not None:
        rows.append(("heading", heading, (1417, 1000, 2031, 1063)))
    rows += [("argument", "Argumento del editor sobre este salmo.", (411, 1200, 3029, 1295)),
             ("latin", "1 In finem, Psalmus ipsi David. XL.", (400, 1400, 1600, 1472)),
             ("verse", "2 Primer verso del salmo nuevo.", (1700, 1400, 3000, 1472)),
             ("latin2", "continuatio latina sequentis.", (400, 1490, 1600, 1562)),
             ("verse2", "3 Segundo verso del salmo nuevo.", (1700, 1490, 3000, 1562))]
    names = {name: f"p{scan_page:04d}l{index:04d}"
             for index, (name, _t, _b) in enumerate(rows)}
    fixture = {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                          "height": 4837,
                          "lines": [{"text": text, "bbox": list(bbox)}
                                    for _n, text, bbox in rows]}]}
    return next(source_ocr.pages_from_fixture(fixture)), names


def _entries(page):
    return split_columns(page, gutter_hint=GUTTER)


def _review(names, **over):
    """Una revisión de frontera sobre esa plana, con sus anclas reales."""
    base = dict(
        id="fx-40", book="Ps", scan_page=10, outcome=BOUNDARY_AND_NUMBER,
        chapter_number=40, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block=names["previous_verse2"],
        insert_before_block=names["argument"],
        observed_printed_text="SALMO XL.", crop_bbox=(0, 800, 3402, 1300),
        confidence=0.95, rationale="the facsimile prints SALMO XL.",
        reviewer_method="render", pdf_page=11)
    base.update(over)
    return ir.ChapterImageReview(**base)


def _apply(page, entries, reviews, **kw):
    kw.setdefault("book", "Ps")
    return recovery.apply_verified_image_reviews(
        page, entries, reviews, source=SOURCE, **kw)


def _parse(page, reviews):
    return page_parser.parse_volume(
        [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        image_reviews={page.scan_page: list(reviews)})


def _recovered_claim(walker):
    return next(c for c in walker.ledger.claims
                if c.source == cc.FROM_IMAGE_REVIEW)


def _ids(entries, scan_page=10):
    return [recovery._entry_block_id(e, scan_page) for e in entries]


# ======================================================================
# A/B/C. El rótulo impreso que el reconocimiento NO dejó
# ======================================================================
def test_A_a_truly_missing_heading_is_inserted_as_a_derived_event():
    page, names = _page(heading=None)
    entries = _entries(page)
    # sin nada donde el impreso tiene el rótulo, no hay frontera ninguna
    _ed, _s, plain = _parse(page, [])
    assert [c.raw_heading for c in plain.ledger.claims] == [_PREVIOUS_HEADING]

    out, records = _apply(page, entries, [_review(names)])
    assert [r.action for r in records] == [recovery.INSERTED]
    assert len(out) == len(entries) + 1, "se añade un renglón, y sólo uno"
    added = [e for e in out if getattr(e, "recovery", None)]
    assert len(added) == 1
    assert added[0].recovery.chapter_number == 40
    assert added[0].line.raw_text == "SALMO XL."
    assert added[0].recovery.provenance.source == recovery.RECOVERED
    assert added[0].recovery.provenance.source_sha256 == SHA
    assert added[0].recovery.provenance.review_id == "fx-40"


def test_B_the_inserted_event_is_not_counted_as_an_ocr_block():
    page, names = _page(heading=None)
    _ed, without, _w = _parse(page, [])
    _ed, with_review, _w = _parse(page, [_review(names)])
    assert with_review["ocr_blocks"] == without["ocr_blocks"], \
        "un evento derivado no puede disfrazarse de bloque del OCR"
    assert with_review["ocr_blocks"] == len(_entries(page))


def test_C_the_derived_event_carries_an_id_of_its_own():
    page, names = _page(heading=None)
    out, _records = _apply(page, _entries(page), [_review(names)])
    added = [e for e in out if getattr(e, "recovery", None)][0]
    block_id = recovery._entry_block_id(added, 10)
    assert block_id.startswith("p0010r"), block_id
    assert block_id not in _ids(_entries(page)), "no puede pisar un id del OCR"
    # y aguas abajo el reclamo lo lleva igual
    _ed, _s, walker = _parse(page, [_review(names)])
    claim = _recovered_claim(walker)
    assert claim.block_id.startswith("p0010r")
    assert claim.provenance["source"] == recovery.RECOVERED


# ======================================================================
# D/E/F/G. Sin anclas verificables no se inserta nada
# ======================================================================
def _fails(review, *, contains=None):
    page, _names = _page(heading=None)
    entries = _entries(page)
    out, records = _apply(page, entries, [review])
    assert records[0].action == recovery.FAILED, records[0].action
    assert len(out) == len(entries)
    assert not [e for e in out if getattr(e, "recovery", None)]
    if contains:
        assert contains in records[0].reason, records[0].reason


def test_DE_both_anchors_are_required():
    _page_obj, names = _page(heading=None)
    _fails(_review(names, insert_after_block=None),
           contains="no insert_after_block")
    _fails(_review(names, insert_before_block=None),
           contains="no insert_before_block")
    _fails(_review(names, insert_after_block="p0010l0099"),
           contains="is not a block")


def test_F_reversed_anchors_fail_closed():
    _page_obj, names = _page(heading=None)
    _fails(_review(names, insert_after_block=names["argument"],
                   insert_before_block=names["previous_verse2"]),
           contains="out of reading order")


def test_G_a_bbox_that_does_not_cover_the_anchors_fails_closed():
    _page_obj, names = _page(heading=None)
    _fails(_review(names, crop_bbox=(0, 0, 3402, 500)),
           contains="does not cover the space between the anchors")


# ======================================================================
# H/I. Precisión de bloque
# ======================================================================
def test_HI_blocks_before_stay_and_blocks_after_move():
    page, names = _page(heading=None)
    edition, _s, _w = _parse(page, [_review(names)])
    owner = {}
    for number, chapter in edition.books["Ps"].chapters.items():
        for verse_number, verse in chapter.verses.items():
            for block in verse.blocks:
                owner[block.provenance.block_id] = (number, verse_number)
        for block in chapter.paratext:
            owner.setdefault(block.provenance.block_id, (number, None))
    previous = owner[names["previous_verse"]][0]
    assert previous != 40
    assert owner[names["previous_verse2"]][0] == previous
    assert owner[names["argument"]][0] == 40
    assert owner[names["verse"]][0] == 40
    assert owner[names["verse2"]][0] == 40


# ======================================================================
# J/K/L/M. El número sale de la imagen y de nada más
# ======================================================================
def test_JKL_the_number_comes_from_the_image_not_from_sequence_or_canon():
    page, names = _page(heading=None)
    # los vecinos son 39 y lo que toque; el número es el que diga la imagen
    _ed, _s, walker = _parse(page, [_review(names, chapter_number=91,
                                            observed_printed_text="SALMO XCI.")])
    assert _recovered_claim(walker).accepted_number == 91

    # y sin revisión, ni la secuencia ni el canon abren nada
    _ed, _s, walker = _parse(page, [])
    assert not [c for c in walker.ledger.claims
                if c.source == cc.FROM_IMAGE_REVIEW]
    assert 40 not in {c.accepted_number for c in walker.ledger.accepted()}

    import ast
    tree = ast.parse(open(os.path.join(DIR, "recovery.py"),
                          encoding="utf-8").read())
    imported = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imported |= {a.name for a in node.names}
        elif isinstance(node, ast.ImportFrom):
            imported.add(node.module or "")
    assert "canon" not in imported


def test_M_a_boundary_without_a_readable_numeral_keeps_no_number():
    page, names = _page(heading=None)
    out, records = _apply(page, _entries(page), [_review(
        names, outcome=BOUNDARY_ONLY, chapter_number=None,
        numeral_confirmed=False)])
    assert records[0].action == recovery.INSERTED
    added = [e for e in out if getattr(e, "recovery", None)][0]
    assert added.recovery.chapter_number is None
    assert added.recovery.review_required

    _ed, _s, walker = _parse(page, [_review(
        names, outcome=BOUNDARY_ONLY, chapter_number=None,
        numeral_confirmed=False)])
    claim = _recovered_claim(walker)
    assert claim.accepted_number is None
    assert claim.review_required


# ======================================================================
# N/O/V. Nada se duplica
# ======================================================================
def test_N_an_existing_ocr_heading_is_marked_and_never_duplicated():
    """El caso real de esta tanda: el rótulo estaba, roto por dentro."""
    page, names = _page(heading="$A,LMO CVL")
    entries = _entries(page)
    out, records = _apply(page, entries,
                          [_review(names, heading_block=names["heading"],
                                   chapter_number=106,
                                   observed_printed_text="SALMO CVI.")],
                          resolve_numeral=lambda raw: None)
    assert records[0].action == recovery.RESOLVED
    assert records[0].found_by == recovery.BY_NAME
    assert len(out) == len(entries), "no se añade ningún renglón"
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert len(marked) == 1
    assert marked[0].line.raw_text == "$A,LMO CVL"


def test_the_noise_of_a_broken_line_does_not_block_the_facsimile():
    """La máquina saca un número de la línea rota; el facsímil lee otro.

    Cuando el rótulo hubo que NOMBRARLO es que ninguna lectura del texto
    veía una división ahí: lo que la resolución saque de ese renglón sale
    de la palabra rota o de una cabecera corrida ilegible, y no es una
    lectura rival. Se anota y se sigue. Pero si la máquina SÍ lee una
    división en la línea y da otro número, eso es un desacuerdo de
    verdad y se para.
    """
    page, names = _page(heading="$A,LMO CVL")
    out, records = _apply(page, _entries(page),
                          [_review(names, heading_block=names["heading"],
                                   chapter_number=106,
                                   observed_printed_text="SALMO CVI.")],
                          resolve_numeral=lambda raw: 1)
    assert records[0].action == recovery.RESOLVED
    assert "running header" in records[0].reason
    assert [e for e in out if getattr(e, "recovery", None)]

    # el rótulo legible que dice otra cosa sigue parando la revisión
    page, names = _page(heading="SALMO XL.")
    out, records = _apply(page, _entries(page),
                          [_review(names, chapter_number=46)],
                          resolve_numeral=lambda raw: 40)
    assert records[0].action == recovery.CONFLICT
    assert not [e for e in out if getattr(e, "recovery", None)]


def test_OV_applying_the_same_review_twice_changes_nothing():
    page, names = _page(heading=None)
    entries = _entries(page)
    once, first = _apply(page, entries, [_review(names)])
    twice, second = _apply(page, once, [_review(names)])
    assert first[0].action == recovery.INSERTED
    assert second[0].action == recovery.REDUNDANT
    assert len(twice) == len(once)
    assert sum(1 for e in twice if getattr(e, "recovery", None)) == 1
    assert _ids(twice) == _ids(once)


def test_P_a_false_candidate_has_no_structural_effect():
    page, names = _page(heading=None)
    entries = _entries(page)
    out, records = _apply(page, entries, [_review(
        names, outcome=NO_BOUNDARY, chapter_number=None,
        boundary_confirmed=False, numeral_confirmed=False,
        insert_after_block=None, insert_before_block=None,
        observed_printed_text=None)])
    assert records[0].action == recovery.REJECTED
    assert len(out) == len(entries)
    assert not [e for e in out if getattr(e, "recovery", None)]
    _ed, _s, walker = _parse(page, [_review(
        names, outcome=NO_BOUNDARY, chapter_number=None,
        boundary_confirmed=False, numeral_confirmed=False,
        insert_after_block=None, insert_before_block=None,
        observed_printed_text=None)])
    assert not [c for c in walker.ledger.claims
                if c.source == cc.FROM_IMAGE_REVIEW]


# ======================================================================
# T/U. El crudo no se toca; el ledger decide
# ======================================================================
def test_TU_raw_ocr_is_untouched_and_the_ledger_decides():
    page, names = _page(heading="$A,LMO CVL")
    texts = [e.line.raw_text for e in _entries(page)]
    review = _review(names, heading_block=names["heading"], chapter_number=106,
                     observed_printed_text="SALMO CVI.")
    out, _records = _apply(page, _entries(page), [review],
                           resolve_numeral=lambda raw: None)
    assert [e.line.raw_text for e in out] == texts

    edition, _s, walker = _parse(page, [review])
    claim = _recovered_claim(walker)
    assert claim.provenance["raw_ocr_heading"] == "$A,LMO CVL"
    assert claim.raw_heading == "SALMO CVI."
    assert claim.disposition == cc.ACCEPTED
    assert claim.heading["is_heading"] is True
    assert claim.heading["source"] == "image_review"
    assert 106 in edition.books["Ps"].chapters


# ======================================================================
# El tomo real, medido sobre el audit
# ======================================================================
def _audit():
    path = os.path.join(DIR, "..", "..", "build", "torresamat1835-audit",
                        "volume3.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


#: Las trece planas de esta tanda, con el bloque que el reconocimiento SÍ
#: había producido y el capítulo que el impreso les da.
RECOVERED = [("ps-28-p46", 46, "p0046l0054", 28), ("ps-34-p54", 54, "p0054l0026", 34),
             ("ps-55-p82", 82, "p0082l0037", 55), ("ps-68-p98", 98, "p0098l0022", 68),
             ("ps-76-p110", 110, "p0110l0059", 76), ("ps-78-p117", 117, "p0117l0012", 78),
             ("ps-106-p154", 154, "p0154l0044", 106), ("ps-117-p166", 166, "p0166l0045", 117),
             ("ps-126-p183", 183, "p0183l0001", 126), ("prov-23-p254", 254, "p0254l0047", 23),
             ("prov-24-p256", 256, "p0256l0069", 24), ("sir-36-p449", 449, "p0449l0040", 36),
             ("isa-22-p541", 541, "p0541l0012", 22)]
FALSE_PAGES = (53, 123, 299, 304, 306, 309, 316)


def test_the_shipped_batch_validates():
    payload = ir.load()
    assert ir.validate(payload, page_count=652) == []
    batch = {r["id"]: r for r in payload["reviews"]
             if r.get("batch") == "batch-119"}
    assert len(batch) == len(RECOVERED) + len(FALSE_PAGES)
    for review_id, page, block, number in RECOVERED:
        review = batch[review_id]
        assert review["discovered_by"] == "missing_heading"
        assert review["review_outcome"] == "existing_ocr_heading_found"
        assert review["heading_block"] == block
        assert review["chapter_number"] == number
        assert review["scan_page"] == page and review["pdf_page"] == page + 1
        assert review["insert_after_block"] and review["insert_before_block"]
        assert len(review["rationale"]) > 120
    rejections = [r for r in batch.values() if r["outcome"] == NO_BOUNDARY]
    assert {r["scan_page"] for r in rejections} == set(FALSE_PAGES)
    for review in rejections:
        assert review["review_outcome"] == "false_missing_heading_candidate"
        assert review["chapter_number"] is None
        assert review.get("heading_block") is None
        assert review["insert_after_block"] is None
        assert review["insert_before_block"] is None


def test_every_candidate_of_the_queue_is_accounted_for():
    report = _audit()
    if report is None:
        print("  (saltado: no hay audit en build/)")
        return
    section = report["missing_heading_reviews"]
    assert section["candidates_total"] == len(RECOVERED) + len(FALSE_PAGES)
    assert section["reviewed"] == section["candidates_total"]
    assert section["by_outcome"] == {
        "existing_ocr_heading_found": len(RECOVERED),
        "false_missing_heading_candidate": len(FALSE_PAGES)}
    assert section["existing_ocr_heading_found"] == len(RECOVERED)
    assert section["false_reviewed"] == len(FALSE_PAGES)
    assert section["confirmed_missing_heading"] == 0
    # un candidato recuperado NO desaparece del informe aunque salga de
    # la cola: se queda con su desenlace y su procedencia
    rows = {row["scan_page"]: row for row in section["candidates"]}
    for _review_id, page, block, number in RECOVERED:
        row = rows[page]
        assert row["review_outcome"] == "existing_ocr_heading_found"
        assert row["heading_block"] == block
        assert row["recovered_chapter"] == number
        assert row["still_pending"] is False
    for page in FALSE_PAGES:
        assert rows[page]["review_outcome"] == "false_missing_heading_candidate"
        assert rows[page]["recovered_chapter"] is None
        assert rows[page]["still_pending"] is False


def test_a_reviewed_candidate_is_history_and_not_pending_work():
    """Descubrir no es estar pendiente.

    Los siete blancos que resultaron ser márgenes, finales de libro y
    marcas de sección se siguen descubriendo en cada pasada -- el blanco
    sigue ahí -- y eso está bien: es un diagnóstico. Lo que no pueden es
    volver a contar como trabajo por hacer.
    """
    report = _audit()
    if report is None:
        return
    section = report["missing_heading_reviews"]
    queue = report["image_review_queue"]

    # 1. nadie queda pendiente en esta familia
    assert section["pending_review"] == 0
    assert section["pending_by_book"] == {}
    assert queue["by_family"]["missing_heading"] == 0
    assert not [c for c in queue["top"] if c["family"] == "missing_heading"]

    # 2. el generador los sigue encontrando, y el informe lo dice
    assert section["discovered_now"] == len(FALSE_PAGES)
    assert queue["discovered_by_family"]["missing_heading"] == len(FALSE_PAGES)
    assert queue["reviewed_and_closed"] == len(FALSE_PAGES)
    assert queue["discovered_total"] == queue["total"] + \
        queue["reviewed_and_closed"]

    # 3. y ninguno de los veinte se ha borrado de la historia
    rows = {row["scan_page"]: row for row in section["candidates"]}
    assert set(rows) == {page for _r, page, _b, _n in RECOVERED} | \
        set(FALSE_PAGES)
    for page in FALSE_PAGES:
        row = rows[page]
        assert row["discovered_now"] is True, page
        assert row["still_pending"] is False, page
        assert row["review_id"], page

    # 4. `pending_review` no es `candidates_total`, y se dicen aparte
    assert section["candidates_total"] != section["pending_review"]
    assert section["candidates_total"] == section["reviewed"] + \
        section["pending_review"]


def test_nothing_was_inserted_in_the_volume_and_no_ocr_block_appeared():
    report = _audit()
    if report is None:
        return
    assert report["metrics"]["ocr_blocks"] == 57700
    cr = report["chapter_image_recovery"]
    actions = {a["review_id"]: a for a in cr["applications"]}
    for review_id, _page, block, number in RECOVERED:
        record = actions[review_id]
        # se marcó el renglón que ya estaba: ninguna inserción
        assert record["action"] in (recovery.RESOLVED, recovery.CONFIRMED)
        assert record["target_block"] == block
        assert record["chapter_number"] == number
    assert cr["recovered_boundaries"] == 0, \
        "en este tomo no hubo ningún rótulo realmente ausente"
    assert cr["reviews_failed_anchor_validation"] == 0
    assert cr["reviews_colliding_with_existing_chapter"] == 0
    assert cr["recovered_number_collisions"] == []


def test_the_false_candidates_create_nothing():
    report = _audit()
    if report is None:
        return
    actions = {a["review_id"]: a
               for a in report["chapter_image_recovery"]["applications"]}
    for review in ir.load()["reviews"]:
        if review.get("batch") != "batch-119" or \
                review["outcome"] != NO_BOUNDARY:
            continue
        assert actions[review["id"]]["action"] == recovery.REJECTED
        assert actions[review["id"]]["chapter_number"] is None


def test_QR_the_earlier_work_is_not_touched():
    report = _audit()
    if report is None:
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    # los marcadores corruptos de 118 siguen recuperados y no se repiten
    markers = report["corrupted_division_marker_candidates"]
    assert markers["awaiting_review"] == 0
    assert markers["recovered"] == markers["total"]
    seen_blocks = [r.get("heading_block") for r in ir.load()["reviews"]]
    assert len(seen_blocks) == len(set(b for b in seen_blocks if b)) + \
        seen_blocks.count(None)
    # las seis inscripciones falsas de 116 siguen rechazadas
    for block in ("p0064l0066", "p0101l0062", "p0133l0079", "p0135l0059",
                  "p0190l0073", "p0196l0050"):
        assert claims[block]["disposition"] == cc.REJECTED_FALSE_HEADING
        assert claims[block]["accepted_number"] is None


def test_S_the_first_psalm_was_never_a_missing_heading():
    report = _audit()
    if report is None:
        return
    claim = {c["block_id"]: c
             for c in report["chapter_claims"]["claims"]}["p0015l0003"]
    assert claim["raw_heading"].upper().startswith("SALMO PRIMERO")
    # Su numeral sigue sin leerse, y tiene que seguir así: «PRIMERO» no
    # es un romano roto. Lo que le dio número fue la palabra, leída por
    # `written_ordinals` en la tanda 121 -- nunca este barrido.
    assert claim["numeral"]["status"] == "no_numeral"
    assert claim["number_source"] == "written_ordinal"
    assert claim["accepted_number"] == 1
    # y nunca apareció como candidato de rótulo ausente: el rótulo
    # estaba impreso y el reconocimiento lo produjo entero
    pages = {row["scan_page"]
             for row in report["missing_heading_reviews"]["candidates"]}
    assert 15 not in pages


def test_WXYZ_the_volume_keeps_its_integrity():
    report = _audit()
    if report is None:
        return
    assert report["duplicate_refs"] == []
    assert report["out_of_order_refs"] == []
    assert report["materialized_verse_refs"] + \
        report["verse_refs_in_review_slots"] == report["verse_refs"]
    spans = dict(report["book_boundary_resolution"]["spans_after"])
    assert spans == {"Ps": 14, "Prov": 209, "Eccl": 275, "Song": 300,
                     "Wis": 318, "Sir": 362, "Isa": 496}
    # cada capítulo recuperado tiene su reclamo aceptado, uno solo
    claims = report["chapter_claims"]["claims"]
    for _review_id, page, block, number in RECOVERED:
        recovered_id = f"p{page:04d}r{block[6:]}"
        holder = [c for c in claims if c["block_id"] == recovered_id]
        assert len(holder) == 1, recovered_id
        assert holder[0]["accepted_number"] == number
        # y nadie más se queda con ese número en ese libro
        same = [c for c in claims if c["book"] == holder[0]["book"]
                and c["accepted_number"] == number]
        assert len(same) == 1, (holder[0]["book"], number)


# ======================================================================
# AA/AB/AC. Determinista, sin red, sin planas en el código
# ======================================================================
def test_AA_deterministic():
    page, names = _page(heading=None)

    def run():
        _ed, _s, walker = _parse(page, [_review(names)])
        return json.dumps(walker.ledger.report(), sort_keys=True, default=str)
    assert run() == run()


def test_ABC_offline_and_no_page_specific_logic():
    import ast
    pages = {46, 54, 82, 98, 110, 117, 154, 166, 183, 254, 256, 449, 541}
    for name in ("recovery.py", "page_parser.py", "image_reviews.py",
                 "recovery_candidates.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for forbidden in ("urllib", "requests", "http://", "https://",
                          "socket"):
            assert forbidden not in source, f"{name}: {forbidden}"
        tree = ast.parse(source)

        def names_a_page(node):
            if isinstance(node, ast.Name):
                return "page" in node.id or "block" in node.id
            if isinstance(node, ast.Attribute):
                return ("page" in node.attr or "block" in node.attr
                        or names_a_page(node.value))
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
                 if isinstance(n, ast.Constant) and isinstance(n.value, str)]
        for marker in ("p0046", "p0110", "p0183", "p0541"):
            assert not any(marker in t for t in texts), f"{name}: {marker}"


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
    print(f"torresamat1835_missing_headings_failures={failures}")
    sys.exit(1 if failures else 0)
