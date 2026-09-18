"""
TORRES-1835-IMAGE-RECOVERY-INTEGRATION-110: aplicar lo leído en el facsímil.

    python3 test_recovery.py

Sin red y sin el testigo real. La geometría del fixture reproduce la de
las planas reales del tomo 3; contra el tomo entero se comprueba con la
pasada de auditoría, no aquí.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import image_reviews as ir
import page_parser
import recovery
import source_ocr
from image_reviews import BOUNDARY_AND_NUMBER, BOUNDARY_ONLY, NO_BOUNDARY
from layout import split_columns
from model import BlockKind

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
GUTTER = 1690

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256="f" * 64,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


# --- una plana a dos columnas, con o sin su rótulo ----------------------
#
# Las dos columnas tienen que estar pobladas de verdad: el canal se MIDE
# por cobertura, así que una plana de juguete con sólo media columna no
# da canal y todo cae en «columna indecidible». La geometría es la de las
# planas reales del tomo 3.
#
#   sin rótulo   0 cab  1 L  2 R  3 L  4 R  5 L  6 R  7 L  8 R
#   con rótulo   0 cab  1 L  2 R  3 L  4 R  5 RÓTULO  6 L  7 R  8 L  9 R
_ANCHOR_AFTER = "p0010l0004"
_ANCHOR_BEFORE_PLAIN = "p0010l0006"
_ANCHOR_BEFORE_WITH_HEADING = "p0010l0007"


def _page(*, with_heading=None, scan_page=10):
    lines = [
        {"text": "LOS SALMOS. 100", "bbox": [1697, 195, 2117, 255]},
        {"text": "1 Verbum latinum primae lineae.",
         "bbox": [400, 700, 1600, 772]},
        {"text": "1 Primer versiculo espanol.", "bbox": [1700, 700, 3000, 772]},
        {"text": "continuatio latina eiusdem versus.",
         "bbox": [400, 790, 1600, 862]},
        {"text": "2 Segundo versiculo espanol.", "bbox": [1700, 790, 3000, 862]},
    ]
    if with_heading:
        lines.append({"text": with_heading, "bbox": [1200, 1000, 2150, 1080]})
    lines += [
        {"text": "1 Verbum latinum sequentis capituli.",
         "bbox": [400, 1300, 1600, 1372]},
        {"text": "1 Primer versiculo del capitulo siguiente.",
         "bbox": [1700, 1300, 3000, 1372]},
        {"text": "continuatio latina sequentis.",
         "bbox": [400, 1390, 1600, 1462]},
        {"text": "2 Segundo versiculo del capitulo siguiente.",
         "bbox": [1700, 1390, 3000, 1462]},
    ]
    fixture = {"pages": [{"scan_page": scan_page, "width": 3402,
                          "height": 4837, "lines": lines}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _entries(page):
    return split_columns(page, gutter_hint=GUTTER)


def _review(**over):
    """Una revisión verificada sobre esa plana, con sus anclas reales."""
    base = dict(
        id="fx-1", book="Ps", scan_page=10, outcome=BOUNDARY_AND_NUMBER,
        chapter_number=46, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block=_ANCHOR_AFTER,
        insert_before_block=_ANCHOR_BEFORE_PLAIN,
        observed_printed_text="CAPÍTULO XLVI",
        crop_bbox=(0, 0, 3402, 2000), confidence=0.95,
        rationale="read from the facsimile", reviewer_method="render")
    base.update(over)
    return ir.ChapterImageReview(**base)


def _apply(page, entries, reviews, **kw):
    kw.setdefault("book", "Ps")
    return recovery.apply_verified_image_reviews(
        page, entries, reviews, source=SOURCE, **kw)


def _ids(entries, scan_page=10):
    return [recovery._entry_block_id(e, scan_page) for e in entries]


# ---- A. El rótulo recuperado entra en el flujo -------------------------
def test_valid_recovered_heading_is_inserted():
    page = _page()
    entries = _entries(page)
    out, records = _apply(page, entries, [_review()])
    assert len(out) == len(entries) + 1
    assert [r.action for r in records] == [recovery.INSERTED]
    added = [e for e in out if getattr(e, "recovery", None)]
    assert len(added) == 1
    assert added[0].recovery.chapter_number == 46
    assert added[0].line.raw_text == "CAPÍTULO XLVI"


# ---- B. El número sale de la imagen, no de anterior+1 ------------------
def test_recovered_number_comes_from_the_review_not_from_the_neighbours():
    """Quitarle el numeral a la metadata deja el capítulo sin número.

    Los vecinos siguen siendo 45 y 47 y el rótulo sigue entrando: lo que
    no ocurre es que nadie deduzca el 46 de ellos.
    """
    fixture = {"pages": [
        {"scan_page": 10, "width": 3402, "height": 4837, "lines": [
            {"text": "CAPÍTULO XLV.", "bbox": [1200, 300, 2100, 380]},
            {"text": "1 Verso del capitulo cuarenta y cinco.",
             "bbox": [1700, 500, 3000, 572]}]},
        {"scan_page": 11, "width": 3402, "height": 4837, "lines": [
            {"text": "1 Latina.", "bbox": [400, 300, 1640, 372]},
            {"text": "1 Verso de un capitulo sin rotulo legible.",
             "bbox": [1700, 300, 3000, 372]},
            {"text": "2 Otro verso.", "bbox": [1700, 400, 3000, 472]}]},
        {"scan_page": 12, "width": 3402, "height": 4837, "lines": [
            {"text": "CAPÍTULO XLVII.", "bbox": [1200, 300, 2100, 380]},
            {"text": "1 Verso del capitulo cuarenta y siete.",
             "bbox": [1700, 500, 3000, 572]}]}]}

    def run(number):
        review = ir.ChapterImageReview(
            id="fx-mid", book="Sir", scan_page=11, chapter_number=number,
            outcome=(BOUNDARY_AND_NUMBER if number is not None
                     else BOUNDARY_ONLY),
            boundary_confirmed=True, numeral_confirmed=number is not None,
            insert_after_block="p0011l0001",
            insert_before_block="p0011l0002",
            observed_printed_text="CAPÍTULO XLVI" if number else "CAPÍTULO ...",
            crop_bbox=(0, 0, 3402, 600), confidence=0.9,
            rationale="facsimile", reviewer_method="render")
        edition, _stats, walker = page_parser.parse_volume(
            source_ocr.pages_from_fixture(copy.deepcopy(fixture)),
            witness="fx", volume="3", book="Sir", gutter_hint=GUTTER,
            with_walker=True, image_reviews={11: [review]},
            recovery_source=SOURCE)
        return [r for r in walker.resolutions if r["method"] == "image_review"]

    with_number = run(46)
    assert [r["resolved_number"] for r in with_number] == [46]

    without_number = run(None)
    assert [r["resolved_number"] for r in without_number] == [None]
    assert without_number[0]["review_required"] is True


# ---- C. Frontera sin numeral: sigue sin número ------------------------
def test_unknown_numeral_stays_none_and_asks_for_review():
    page = _page()
    review = _review(outcome=BOUNDARY_ONLY, chapter_number=None,
                     numeral_confirmed=False,
                     observed_printed_text="CAPÍTULO [ilegible]")
    out, records = _apply(page, _entries(page), [review])
    assert records[0].action == recovery.INSERTED
    assert records[0].chapter_number is None
    added = [e for e in out if getattr(e, "recovery", None)][0]
    assert added.recovery.chapter_number is None
    assert added.recovery.review_required is True


# ---- D. Una revisión rechazada no toca nada ---------------------------
def test_rejected_review_has_no_structural_effect():
    page = _page()
    entries = _entries(page)
    review = _review(outcome=NO_BOUNDARY, chapter_number=None,
                     boundary_confirmed=False, numeral_confirmed=False,
                     insert_after_block=None, insert_before_block=None,
                     observed_printed_text=None)
    out, records = _apply(page, entries, [review])
    assert records[0].action == recovery.REJECTED
    assert len(out) == len(entries)
    assert all(a is b for a, b in zip(out, entries))
    assert not any(getattr(e, "recovery", None) for e in out)


# ---- E. Hash que no cuadra: cero recuperaciones ------------------------
def test_wrong_source_hash_applies_nothing():
    payload = ir.load()
    for bad in ("0" * 64, "9" * 64):
        try:
            ir.reviews_for(payload, expected_sha256=bad)
        except ir.ReviewError as exc:
            assert bad in str(exc)
        else:
            raise AssertionError("a mismatched sha256 must refuse everything")
    try:
        recovery.load_applicable(expected_sha256="0" * 64)
    except ir.ReviewError:
        pass
    else:
        raise AssertionError("load_applicable must refuse a foreign artefact")


def test_absent_visual_source_applies_nothing():
    try:
        ir.reviews_for(ir.load(), source_path=os.path.join(DIR, "nope.pdf"))
    except ir.ReviewError as exc:
        assert "cache" in str(exc)
    else:
        raise AssertionError("a missing artefact must refuse everything")


# ---- F/G/H/I/J. Las anclas fallan cerradas ----------------------------
def _fails(review, *, book="Ps", contains=None):
    page = _page()
    entries = _entries(page)
    out, records = _apply(page, entries, [review], book=book)
    assert records[0].action == recovery.FAILED, records[0].action
    if contains:
        assert contains in records[0].reason, records[0].reason
    assert len(out) == len(entries)
    assert not any(getattr(e, "recovery", None) for e in out)


def test_missing_previous_anchor_fails_closed():
    _fails(_review(insert_after_block=None), contains="insert_after_block")


def test_missing_next_anchor_fails_closed():
    _fails(_review(insert_before_block=None), contains="insert_before_block")


def test_reversed_anchors_fail_closed():
    _fails(_review(insert_after_block=_ANCHOR_BEFORE_PLAIN,
                   insert_before_block=_ANCHOR_AFTER),
           contains="reading order")


def test_anchor_from_another_page_fails_closed():
    _fails(_review(insert_after_block="p0099l0004"), contains="not a block")


def test_review_for_another_page_fails_closed():
    _fails(_review(scan_page=11), contains="scan page")


def test_anchor_from_another_book_fails_closed():
    """El dueño de la plana lo decide la resolución de fronteras de libro.

    Una revisión no mueve bloques de un libro a otro: si dice Sir y la
    plana es de Ps, no se aplica.
    """
    _fails(_review(book="Sir"), book="Ps", contains="boundary resolution")


def test_crop_that_does_not_cover_the_anchors_fails_closed():
    _fails(_review(crop_bbox=(0, 0, 3402, 500)), contains="crop_bbox")


# ---- K. No se duplica un rótulo que ya está ---------------------------
def test_existing_compatible_heading_prevents_a_duplicate():
    """El rótulo no se duplica, y la lectura del facsímil no se tira.

    Cuando la resolución ya daba ese mismo número, la revisión no tiene
    nada que corregir, pero sigue siendo mejor evidencia que la de la
    máquina: se queda marcando el MISMO renglón, con su procedencia. Lo
    que no puede pasar -- y es lo que este test vigila -- es que aparezca
    un segundo rótulo.
    """
    page = _page(with_heading="CAPÍTULO XLVI.")
    entries = _entries(page)
    out, records = _apply(
        page, entries,
        [_review(insert_before_block=_ANCHOR_BEFORE_WITH_HEADING)],
        resolve_numeral=lambda raw: 46)
    assert records[0].action == recovery.CONFIRMED
    assert len(out) == len(entries)
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert len(marked) == 1
    assert marked[0].recovery.chapter_number == 46


def test_existing_unidentified_heading_gets_its_number_without_duplicating():
    page = _page(with_heading="CAPÍTULO* XX vi")
    entries = _entries(page)
    out, records = _apply(
        page, entries,
        [_review(insert_before_block=_ANCHOR_BEFORE_WITH_HEADING)],
        resolve_numeral=lambda raw: None)
    assert records[0].action == recovery.RESOLVED
    assert len(out) == len(entries)
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert len(marked) == 1
    assert marked[0].recovery.chapter_number == 46
    assert marked[0].line.raw_text == "CAPÍTULO* XX vi"


def test_heading_already_identified_as_another_number_fails_closed():
    page = _page(with_heading="CAPÍTULO XL.")
    entries = _entries(page)
    out, records = _apply(
        page, entries,
        [_review(insert_before_block=_ANCHOR_BEFORE_WITH_HEADING)],
        resolve_numeral=lambda raw: 40)
    assert records[0].action == recovery.CONFLICT
    assert len(out) == len(entries)


def test_number_already_held_by_another_chapter_fails_closed():
    """Dos capítulos no caben en el mismo hueco.

    Se encontró de verdad en el tomo: el rótulo de la plana 411 salió
    como «CAPÍTULO XXL» y se resolvió 30, que es el número que el
    facsímil da al rótulo de la plana 435. Aplicar la recuperación
    fundiría los dos y los versículos del segundo pisarían a los del
    primero, así que no se aplica y se dice quién reclama qué.
    """
    page = _page()
    entries = _entries(page)
    out, records = _apply(page, entries, [_review()],
                          claimed_numbers=frozenset({46}))
    assert records[0].action == recovery.COLLISION
    assert "already held" in records[0].reason
    assert len(out) == len(entries)


# ---- L. Idempotencia ---------------------------------------------------
def test_applying_twice_inserts_only_one_heading():
    page = _page()
    once, _r1 = _apply(page, _entries(page), [_review()])
    twice, records = _apply(page, once, [_review()])
    assert len(twice) == len(once)
    assert records[0].action == recovery.REDUNDANT
    assert _ids(twice) == _ids(once)
    assert sum(1 for e in twice if getattr(e, "recovery", None)) == 1


# ---- M. Orden estable --------------------------------------------------
def test_recovered_heading_lands_between_its_anchors():
    page = _page()
    entries = _entries(page)
    out, _records = _apply(page, entries, [_review()])
    ids = _ids(out)
    after, before = ids.index(_ANCHOR_AFTER), ids.index(_ANCHOR_BEFORE_PLAIN)
    recovered = [i for i, e in enumerate(out) if getattr(e, "recovery", None)]
    assert after < recovered[0] < before
    assert recovered[0] != len(out) - 1
    # el resto del flujo conserva su orden
    assert [i for i in ids if i and i.startswith("p0010l")] == _ids(entries)


# ---- N. El OCR crudo no se toca ---------------------------------------
def test_raw_ocr_is_not_modified():
    page = _page()
    entries = _entries(page)
    before_lines = [(l.index, l.bbox, l.raw_text) for l in page.lines]
    before_entries = list(entries)
    before_texts = [e.line.raw_text for e in entries]
    out, _records = _apply(page, entries, [_review()])
    assert [(l.index, l.bbox, l.raw_text) for l in page.lines] == before_lines
    assert entries == before_entries
    assert [e.line.raw_text for e in entries] == before_texts
    assert len(out) != len(entries)          # el cambio vive en la vista
    # y el bloque recuperado no se disfraza de renglón del reconocimiento
    added = [e for e in out if getattr(e, "recovery", None)][0]
    assert recovery._entry_block_id(added, 10).startswith("p0010r")


def test_recovery_layer_never_writes_anything_to_disk():
    """La capa de aplicación no abre ni escribe ningún fichero.

    Se comprueba sobre el árbol sintáctico, no sobre el texto: el propio
    comentario de cabecera de recovery.py dice que no se edita el DjVu ni
    el ABBYY, y buscar esas palabras en el fuente daría un falso positivo
    contra la frase que promete justo lo contrario.
    """
    import ast
    tree = ast.parse(open(os.path.join(DIR, "recovery.py"),
                          encoding="utf-8").read())
    for node in ast.walk(tree):
        if isinstance(node, ast.Call):
            func = node.func
            if isinstance(func, ast.Name):
                assert func.id != "open", "the recovery layer opened a file"
            if isinstance(func, ast.Attribute):
                assert func.attr not in ("write", "writelines", "truncate",
                                         "dump", "unlink", "remove"), func.attr


# ---- O. El dueño del libro no cambia ----------------------------------
def test_book_ownership_is_unchanged_by_a_recovery():
    fixture = {"pages": [
        {"scan_page": 10, "width": 3402, "height": 4837, "lines": [
            {"text": "1 Latina.", "bbox": [400, 300, 1640, 372]},
            {"text": "1 Verso.", "bbox": [1700, 300, 3000, 372]},
            {"text": "2 Otro verso.", "bbox": [1700, 400, 3000, 472]}]}]}
    review = ir.ChapterImageReview(
        id="fx", book="Ps", scan_page=10, outcome=BOUNDARY_AND_NUMBER,
        chapter_number=5, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block="p0010l0001", insert_before_block="p0010l0002",
        observed_printed_text="SALMO V", crop_bbox=(0, 0, 3402, 600),
        confidence=0.9, rationale="facsimile", reviewer_method="render")

    def run(with_review):
        edition, _s, _w = page_parser.parse_volume(
            source_ocr.pages_from_fixture(copy.deepcopy(fixture)),
            witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
            with_walker=True,
            image_reviews={10: [review]} if with_review else None,
            recovery_source=SOURCE if with_review else None)
        return sorted(edition.books)

    # El recuperado abre su capítulo en Ps, que es el libro que la
    # resolución de fronteras da a esa plana, y no aparece ningún otro
    # libro ni con revisión ni sin ella.
    assert run(True) == ["Ps"]
    assert set(run(False)) <= {"Ps"}


# ---- P. El salmo 118 sigue siendo un rechazo --------------------------
def test_psalm_118_stays_a_rejection_with_no_effect():
    """El control negativo sigue siendo un rechazo.

    Salmos ya no tiene una sola revisión de frontera: desde la tanda 117
    tiene además las tres que recuperan rótulos que el reconocimiento
    dejó ilegibles. Lo que este test fija es que MIRAR una plana no
    obliga a que salga una frontera de ahí: el hueco de once planas del
    salmo 118 se miró y no había ninguna.
    """
    payload = ir.load()
    psalms = [r for r in payload["reviews"] if r["book"] == "Ps"]
    rejections = [r for r in psalms if r["outcome"] == NO_BOUNDARY]
    assert rejections, "the Ps 118 control must stay in the metadata"
    for review in rejections:
        assert review["chapter_number"] is None
        assert review["insert_after_block"] is None
        assert review["insert_before_block"] is None
        assert review.get("heading_block") is None
        assert review["boundary_confirmed"] is False
        assert review["rationale"]
    parsed = {r.id: r for r in ir.reviews_for(payload, expected_sha256=_sha())
              if r.book == "Ps"}
    assert parsed
    for review in rejections:
        assert not parsed[review["id"]].creates_boundary

    # y las que sí recuperan una frontera la llevan entera
    for review in psalms:
        if review["outcome"] == NO_BOUNDARY:
            continue
        assert review["boundary_confirmed"] is True
        assert isinstance(review["chapter_number"], int)
        # Con las dos anclas, o nombrando el renglón que ya está: lo
        # segundo es lo que permite marcar un rótulo impreso en cabeza
        # de plana, donde no hay ningún bloque delante que anclar.
        assert (review["insert_after_block"] and review["insert_before_block"]) \
            or review.get("heading_block"), review["id"]
        assert parsed[review["id"]].creates_boundary


# ---- Q/R. Los libros sin revisión no se mueven ------------------------
def test_books_without_a_review_cannot_be_touched():
    payload = ir.load()
    covered = {r["book"] for r in payload["reviews"]
               if r["outcome"] != NO_BOUNDARY}
    for book in ("Eccl", "Song"):
        assert book not in covered
    # y una revisión sólo llega a la plana que nombra
    by_page = recovery.by_page(ir.reviews_for(payload, expected_sha256=_sha()))
    for page, reviews in by_page.items():
        assert all(r.scan_page == page for r in reviews)


# ---- S. El canon no participa -----------------------------------------
def test_canon_is_not_consulted_to_recover_anything():
    page = _page()
    # un número por encima del máximo del libro en la Vulgata se aplica
    # igual: el canon no es una compuerta, la imagen manda.
    out, records = _apply(page, _entries(page), [_review(chapter_number=199)])
    assert records[0].action == recovery.INSERTED
    assert records[0].chapter_number == 199
    added = [e for e in out if getattr(e, "recovery", None)][0]
    assert added.recovery.chapter_number == 199
    source = open(os.path.join(DIR, "recovery.py"), encoding="utf-8").read()
    assert "import canon" not in source


# ---- T. La aritmética de secuencia no crea capítulos ------------------
def test_sequence_alone_recovers_nothing():
    page = _page()
    out, records = _apply(page, _entries(page), [])
    assert records == []
    assert out == _entries(page)
    source = open(os.path.join(DIR, "recovery.py"), encoding="utf-8").read()
    for forbidden in ("previous + 1", "previous+1", "+ 1)", "last_chapter"):
        assert forbidden not in source, forbidden


# ---- U. La procedencia llega entera a la auditoría --------------------
def test_provenance_survives_into_the_model_and_the_audit():
    fixture = {"pages": [
        {"scan_page": 10, "width": 3402, "height": 4837, "lines": [
            {"text": "1 Latina.", "bbox": [400, 300, 1640, 372]},
            {"text": "1 Verso anterior.", "bbox": [1700, 300, 3000, 372]},
            {"text": "1 Verso del capitulo recuperado.",
             "bbox": [1700, 900, 3000, 972]}]}]}
    review = ir.ChapterImageReview(
        id="fx-prov", book="Ps", scan_page=10, outcome=BOUNDARY_AND_NUMBER,
        chapter_number=46, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block="p0010l0001", insert_before_block="p0010l0002",
        observed_printed_text="SALMO XLVI", crop_bbox=(0, 0, 3402, 1000),
        confidence=0.95, rationale="facsimile", reviewer_method="render",
        pdf_page=11, printed_page=38)
    edition, _stats, walker = page_parser.parse_volume(
        source_ocr.pages_from_fixture(fixture), witness="fx", volume="3",
        book="Ps", gutter_hint=GUTTER, with_walker=True,
        image_reviews={10: [review]}, recovery_source=SOURCE)

    headings = [b for chapter in edition.books["Ps"].chapters.values()
                for b in chapter.paratext
                if b.kind is BlockKind.CHAPTER_HEADING]
    recovered = [b for b in headings if b.is_recovered]
    assert len(recovered) == 1
    prov = recovered[0].recovered
    for field in ("source", "review_id", "witness", "source_sha256",
                  "scan_page", "pdf_page", "printed_page", "bbox",
                  "observed_printed_text", "confidence", "reviewer_method"):
        assert field in prov, field
    assert prov["source"] == recovery.RECOVERED
    assert prov["source_sha256"] == SOURCE.sha256
    assert recovered[0].number == 46
    assert recovered[0].decision.startswith("image_review:")

    # y un rótulo del reconocimiento sigue sin llevar esa marca
    assert all(not b.is_recovered for b in headings if b is not recovered[0])

    entry = [r for r in walker.resolutions if r.get("recovered")]
    assert len(entry) == 1
    assert entry[0]["method"] == "image_review"
    assert entry[0]["evidence"]["review_id"] == "fx-prov"
    assert entry[0]["evidence"]["source_sha256"] == SOURCE.sha256
    assert json.dumps(entry[0])          # la auditoría lo serializa entero


# ---- V. Determinismo ---------------------------------------------------
def test_application_is_deterministic():
    def run():
        page = _page()
        out, records = _apply(page, _entries(page), [_review()])
        return (_ids(out), [(r.review_id, r.action, r.chapter_number)
                            for r in records])
    assert run() == run()


# ---- W. Sin red --------------------------------------------------------
def test_offline():
    for name in ("recovery.py", "recovery_candidates.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for forbidden in ("urllib", "requests", "http://", "https://",
                          "socket"):
            assert forbidden not in source, f"{name}: {forbidden}"


# ---- El mapping de planas es estructural, no un desfase medido --------
def test_page_mapping_is_declared_by_the_derivatives():
    payload = ir.load()
    source = payload["visual_source"]
    assert source["page_mapping"] == "pdf_page = scan_page + 1"
    assert source.get("page_mapping_basis")
    assert len(source.get("page_mapping_evidence", [])) >= 7
    for review in payload["reviews"]:
        if review.get("pdf_page") is not None:
            assert review["pdf_page"] == review["scan_page"] + 1


def _sha():
    return ir.load()["visual_source"]["sha256"]


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
    print(f"torresamat1835_recovery_failures={failures}")
    sys.exit(1 if failures else 0)
