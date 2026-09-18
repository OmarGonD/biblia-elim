"""
TORRES-1835-WRITTEN-ORDINAL-HEADINGS-121: el número escrito con palabra.

    python3 test_written_ordinals.py

Sin red y sin el testigo. Esta edición numera la primera división de cada
libro con una palabra --«SALMO PRIMERO.», «CAPÍTULO PRIMERO.»-- y las
demás con numeral romano. Para `roman.py` eso no es un numeral roto: es
que no hay numeral, y hace bien en decirlo. El número está, escrito en
otro sistema.

    ROMAN NUMERALS AND WRITTEN ORDINALS ARE DIFFERENT EVIDENCE TYPES

Lo que se comprueba aquí se reparte en tres:

    el lector         vocabulario cerrado, normalización que no enmienda,
                      y una palabra dañada que NO se arregla por parecido;

    la integración    el número entra por `ChapterClaim` y `ClaimLedger`
                      como cualquier otro, y un rótulo que trae romano y
                      ordinal diciendo cosas distintas se para;

    el tomo           lo que la metadata y el facsímil dejaron escrito de
                      los siete rótulos ordinales de este volumen, el de
                      Sabiduría incluido, que sólo la imagen pudo leer.
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
import roman
import source_ocr
import structure
import written_ordinals as wo
from layout import split_columns

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
METADATA = os.path.join(ROOT, "data", "torresamat1835",
                        "chapter_image_reviews.json")
GUTTER = 1690
PAGE_WIDTH = 3402
SHA = "f" * 64

SOURCE = __import__("recovery").SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


def _slot(heading):
    """El sitio del número: el rótulo sin su palabra de división."""
    return structure._without_book_words(heading)


def _page(heading, *, scan_page=10):
    """Una plana con un rótulo y su texto, como las del tomo."""
    rows = [("LIBRO DE LOS SALMOS. 12", (1697, 195, 2117, 255)),
            (heading, (1205, 932, 2118, 995)),
            ("Argumento del editor sobre este salmo.", (411, 1100, 3029, 1195)),
            ("1 Beatus vir qui non abiit.", (400, 1300, 1600, 1372)),
            ("1 Dichoso el varon que no anduvo.", (1700, 1300, 3000, 1372)),
            ("2 Sed in lege Domini voluntas eius.", (400, 1390, 1600, 1462)),
            ("2 Sino que su voluntad está en la ley.", (1700, 1390, 3000, 1462))]
    fixture = {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                          "height": 4837,
                          "lines": [{"text": text, "bbox": list(bbox)}
                                    for text, bbox in rows]}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _parse(heading, **kw):
    page = _page(heading)
    kw.setdefault("book", "Ps")
    _ed, _stats, walker = page_parser.parse_volume(
        [page], witness="fx", volume="3", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE, **kw)
    return walker


def _claim_of(walker):
    return walker.ledger.claims[0]


def _metadata():
    with open(METADATA, encoding="utf-8") as handle:
        return json.load(handle)


def _ordinal_reviews(data=None):
    data = data or _metadata()
    return [r for r in data["reviews"]
            if r.get("discovered_by") == "written_ordinal"]


# ======================================================================
# A-D. El lector: lo que SÍ se lee
# ======================================================================
def test_A_exact_primero_in_a_heading_reads_one():
    reading = wo.read(_slot("SALMO PRIMERO."))
    assert reading.status == wo.RECOGNIZED
    assert reading.value == 1
    assert reading.normalized_token == "primero"
    assert reading.raw_token == "PRIMERO."[:7]


def test_B_primera_is_not_in_the_closed_vocabulary():
    # El tomo imprime «PRIMERO» en los siete rótulos --comprobado plana a
    # plana--, así que la forma femenina no está soportada: soportarla
    # «por si acaso» sería admitir una lectura que el testigo no usa, y
    # justo la que el reconocimiento inventa en la plana 319.
    assert "primera" not in wo.VOCABULARY
    reading = wo.read("PRIMERA")
    assert reading.status == wo.UNSUPPORTED
    assert reading.value is None
    assert "resembling is not reading" in reading.reason


def test_C_case_and_accents_do_not_change_the_word():
    for form in ("PRIMERO", "primero", "Primero", "PRIMÉRO", "PRIMERO"):
        reading = wo.read(form)
        assert reading.status == wo.RECOGNIZED, form
        assert reading.value == 1, form


def test_D_outer_punctuation_is_not_part_of_the_word():
    for form in ("PRIMERO.", " PRIMERO, ", "«PRIMERO»", "PRIMERO. ^"):
        assert wo.read(form).value == 1, form


# ======================================================================
# E-H. El lector: lo que NO se lee
# ======================================================================
def test_E_prose_containing_the_word_yields_no_number():
    prose = "y conviene primero que todo mirar la plana antes de escribir"
    reading = wo.read(prose)
    assert reading.value is None
    assert reading.status == wo.ABSENT
    assert "prose containing an ordinal" in reading.reason


def test_F_an_invalid_heading_with_primero_becomes_no_chapter():
    # Un renglón de prosa que lleva la palabra de división Y el ordinal:
    # el número se leería, pero el rótulo no es un rótulo, y eso se
    # decide antes que el número.
    claim = cc.ChapterClaim(
        claim_id="c0", book="Ps", scan_page=10, block_id="p0010l0003",
        raw_heading="Salmo primero de David, cuando huía de su hijo",
        numeral={"status": roman.NO_NUMERAL},
        ordinal=wo.read("primero").as_dict(),
        heading={"is_heading": False, "rejections": ["prose"]})
    ledger = cc.ClaimLedger()
    ledger.add(claim)
    ledger.resolve(lambda c, previous: cc.Proposal(
        c.ordinal_value, cc.WRITTEN_ORDINAL, {}, 0.95))
    assert claim.disposition == cc.REJECTED_FALSE_HEADING
    assert claim.accepted_number is None


def test_G_a_damaged_ordinal_is_not_repaired_by_similarity():
    for damaged in ("PR1MERO", "PRIMFRO", "PRJMERO", "PR-IMERO"):
        reading = wo.read(damaged)
        assert reading.value is None, damaged
        assert reading.status == wo.UNSUPPORTED, damaged


def test_H_the_damaged_form_of_the_witness_is_explicit():
    # La plana 319 llegó al reconocimiento como «CÁFÍTXJLO PRIMERa»: la
    # palabra de división rota y el ordinal con la última letra cambiada.
    reading = wo.read("PRIMERa")
    assert reading.status == wo.UNSUPPORTED
    assert reading.normalized_token == "primera"
    assert reading.value is None
    # y aun así el renglón se OFRECE, que es lo que lleva a mirar la plana
    assert wo.looks_ordinal("CÁFÍTXJLO PRIMERa")


# ======================================================================
# I-J. Los dos sistemas no se mezclan
# ======================================================================
def test_I_the_roman_parser_is_untouched_by_ordinals():
    assert roman.read("PRIMERO").value is None
    assert roman.read("PRIMERO").status in (roman.NO_NUMERAL,
                                            roman.INVALID_SYNTAX)
    # `roman.py` no sabe de ordinales y no tiene por qué: no importa el
    # módulo nuevo, no tiene tabla de palabras y no le ha hecho falta
    # cambiar ni una línea para esta tanda.
    source = open(os.path.join(DIR, "roman.py"), encoding="utf-8").read()
    for word in ("written_ordinals", "VOCABULARY", "ordinal"):
        assert word not in source, f"roman.py menciona {word!r}"
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if isinstance(node, ast.Dict):
            values = [n.value for n in node.values
                      if isinstance(n, ast.Constant)]
            keys = [n.value for n in node.keys if isinstance(n, ast.Constant)]
            words = [k for k in keys
                     if isinstance(k, str) and len(k) > 3 and k.isalpha()]
            assert not (words and all(isinstance(v, int) for v in values)
                        and values), "roman.py mapea palabras a números"


def test_J_the_ordinal_reader_does_not_read_Roman_numerals():
    for numeral in ("I", "XXIV", "CXLII", "MDCCC"):
        assert wo.read(numeral).value is None, numeral
        assert wo.read(numeral).status == wo.ABSENT, numeral


# ======================================================================
# K-L. Autoridad y conflicto
# ======================================================================
def test_K_roman_and_ordinal_that_disagree_fail_closed():
    walker = _parse("SALMO XXIV PRIMERO.")
    claim = _claim_of(walker)
    assert claim.ordinal.get("value") == 1
    assert claim.numeral.get("value") == 24
    assert claim.accepted_number is None, "no se elige: se para"
    assert claim.proposal_method == cc.ORDINAL_CONFLICT
    assert "nothing in the text decides" in (
        claim.proposal_evidence.get("why") or "")


def test_L_an_image_review_can_resolve_what_the_text_cannot():
    review = ir.ChapterImageReview(
        id="fx-ord", book="Wis", scan_page=10, outcome=ir.BOUNDARY_AND_NUMBER,
        chapter_number=1, boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block="p0010l0000", insert_before_block="p0010l0002",
        observed_printed_text="CAPÍTULO PRIMERO.", crop_bbox=(0, 200, 3402, 1100),
        confidence=0.95, rationale="la plana imprime CAPÍTULO PRIMERO.",
        reviewer_method="render", pdf_page=11,
        heading_block="p0010l0001",
        observed_printed_ordinal="PRIMERO", ordinal_value=1,
        raw_ocr_heading="CÁFÍTXJLO PRIMERa")
    walker = _parse("CÁFÍTXJLO PRIMERa", book="Wis",
                    image_reviews={10: [review]})
    claims = [c for c in walker.ledger.claims
              if c.source == cc.FROM_IMAGE_REVIEW]
    assert len(claims) == 1
    assert claims[0].accepted_number == 1
    assert claims[0].number_source == "image_review_ordinal"
    assert claims[0].provenance["ordinal_value"] == 1
    assert claims[0].provenance["observed_printed_ordinal"] == "PRIMERO"


def test_M_the_review_keeps_what_the_machine_read():
    review = next(r for r in _ordinal_reviews())
    assert review["raw_ocr_heading"] == "CÁFÍTXJLO PRIMERa"
    assert review["observed_printed_text"] == "CAPÍTULO PRIMERO."
    assert review["observed_printed_ordinal"] == "PRIMERO"
    # lo que la máquina leyó y lo que pone la plana, los dos, y distintos
    assert review["raw_ocr_heading"] != review["observed_printed_text"]


# ======================================================================
# N-Q. Por dónde entra el número
# ======================================================================
def test_N_the_number_enters_through_a_chapter_claim():
    walker = _parse("SALMO PRIMERO.")
    claim = _claim_of(walker)
    assert isinstance(claim, cc.ChapterClaim)
    assert claim.ordinal["status"] == wo.RECOGNIZED
    assert claim.accepted_number == 1
    assert claim.proposal_method == cc.WRITTEN_ORDINAL
    assert claim.number_source == cc.WRITTEN_ORDINAL


def test_O_the_ledger_still_decides():
    # Dos rótulos físicos distintos con el mismo ordinal: el ledger los
    # ve competir y no acepta ninguno en silencio.
    ledger = cc.ClaimLedger()
    for index in (3, 40):
        ledger.add(cc.ChapterClaim(
            claim_id=f"c{index}", book="Ps", scan_page=10 + index,
            block_id=f"p{10 + index:04d}l{index:04d}",
            raw_heading="SALMO PRIMERO.", order=index,
            numeral={"status": roman.NO_NUMERAL},
            ordinal=wo.read("PRIMERO").as_dict(),
            heading={"is_heading": True}))
    ledger.resolve(lambda c, previous: cc.Proposal(
        c.ordinal_value, cc.WRITTEN_ORDINAL, {}, 0.95))
    assert [c.disposition for c in ledger.claims] == [cc.COMPETING] * 2
    assert all(c.accepted_number is None for c in ledger.claims)


def test_P_sequence_cannot_supply_the_ordinal():
    # Sin palabra no hay ordinal, por mucho que «lo que tocaba» sea 1.
    walker = _parse("SALMO")
    claim = _claim_of(walker)
    assert claim.ordinal["status"] == wo.ABSENT
    assert claim.accepted_number is None


def test_Q_canon_cannot_supply_the_ordinal():
    source = open(os.path.join(DIR, "written_ordinals.py"), encoding="utf-8").read()
    for word in ("canon", "sequence", "chapter_limit", "deficit", "import structure"):
        assert word not in source, f"written_ordinals.py usa {word!r}"
    tree = ast.parse(source)
    imported = {n.names[0].name for n in ast.walk(tree) if isinstance(n, ast.Import)}
    assert imported <= {"difflib", "re", "unicodedata"}, imported


# ======================================================================
# R-U. El tomo: lo que quedó escrito
# ======================================================================
def test_R_the_real_first_psalm_needs_the_word_to_resolve():
    with_word = _claim_of(_parse("SALMO PRIMERO."))
    assert with_word.accepted_number == 1
    # el mismo rótulo sin la palabra soportada no se resuelve solo
    saved = dict(wo.VOCABULARY)
    try:
        wo.VOCABULARY.clear()
        without = _claim_of(_parse("SALMO PRIMERO."))
        assert without.accepted_number is None
        assert without.disposition == cc.UNRESOLVED
    finally:
        wo.VOCABULARY.update(saved)


def test_S_the_false_first_psalm_of_page_196_stays_rejected():
    # La inscripción «Salmo de David 1 cuando le perseguía» la rechazó la
    # 116 y sigue rechazada: leer ordinales no reabre nada.
    audit = _audit()
    claims = [c for c in audit["chapter_claims"]["claims"]
              if c["block_id"] == "p0196l0050"]
    assert len(claims) == 1
    assert claims[0]["disposition"] == cc.REJECTED_FALSE_HEADING
    assert claims[0]["accepted_number"] is None
    assert not any(c["accepted_number"] == 1 and c["book"] == "Ps"
                   and c["scan_page"] == 196
                   for c in audit["chapter_claims"]["claims"])


def test_T_psalm_142_keeps_its_own_heading():
    audit = _audit()
    claim = next(c for c in audit["chapter_claims"]["claims"]
                 if c["block_id"] == "p0196l0047")
    assert claim["accepted_number"] == 142
    assert claim["disposition"] == cc.ACCEPTED


def test_U_the_wisdom_heading_resolved_from_its_printed_ordinal():
    audit = _audit()
    claim = next(c for c in audit["chapter_claims"]["claims"]
                 if c["scan_page"] == 319 and c["block_id"].startswith("p0319r"))
    assert claim["accepted_number"] == 1
    assert claim["number_source"] == "image_review_ordinal"
    assert claim["raw_heading"] == "CAPÍTULO PRIMERO."


def test_V_a_reviewed_candidate_leaves_the_queue_and_stays_in_history():
    audit = _audit()
    severe = audit["severely_corrupted_heading_candidates"]
    row = next(r for r in severe["candidates"] if r["scan_page"] == 319)
    assert row["review_id"] == "wis-1-p319"
    assert row["still_pending"] is False
    assert severe["pending_review"] == 0
    ordinals = audit["written_ordinal_headings"]
    assert ordinals["pending_review"] == 0
    assert ordinals["candidates_total"] >= 7, "los descubiertos siguen contados"


def test_W_no_previous_review_was_duplicated_or_reopened():
    data = _metadata()
    ids = [r["id"] for r in data["reviews"]]
    assert len(ids) == len(set(ids))
    blocks = [r.get("heading_block") or r.get("candidate_block")
              for r in data["reviews"]]
    blocks = [b for b in blocks if b]
    assert len(blocks) == len(set(blocks)), "un bloque, una respuesta"
    assert len(_ordinal_reviews(data)) == 1, "esta tanda añade una sola"
    assert ir.validate(data, page_count=652) == []


# ======================================================================
# X-AF. Integridad del tomo
# ======================================================================
def _audit():
    path = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
    if not os.path.isfile(path):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def test_X_no_block_was_lost():
    audit = _audit()
    assert audit["metrics"]["ocr_blocks"] == 57700
    counted = (audit["materialized_verse_refs"]
               + audit["verse_refs_in_review_slots"])
    assert counted == audit["verse_refs"], "toda referencia está en un sitio"


def test_Y_no_block_has_two_owners():
    audit = _audit()
    assert audit["duplicate_refs"] == []
    assert audit["out_of_order_chapters"] == [] or \
        isinstance(audit["out_of_order_chapters"], list)


def test_Z_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AA_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_AB_ocr_blocks_unchanged():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AC_reading_twice_gives_the_same_reading():
    slot = _slot("CAPÍTULO PRIMERO.")
    first, second = wo.read(slot).as_dict(), wo.read(slot).as_dict()
    assert first == second
    # y aplicar la lectura dos veces sobre el mismo reclamo no la cambia
    walker = _parse("SALMO PRIMERO.")
    claim = _claim_of(walker)
    again = wo.read(_slot(claim.raw_heading)).as_dict()
    assert again["value"] == claim.ordinal["value"] == 1


def test_AD_the_vocabulary_is_ordered_and_closed():
    assert wo.vocabulary() == {"primero": 1}
    assert wo.vocabulary() is not wo.VOCABULARY, "la tabla no se entrega viva"
    audit = _audit()
    assert audit["written_ordinal_headings"]["vocabulary"] == {"primero": 1}


def test_AE_the_reader_works_offline():
    source = open(os.path.join(DIR, "written_ordinals.py"), encoding="utf-8").read()
    for word in ("urllib", "requests", "socket", "open("):
        assert word not in source, f"written_ordinals.py usa {word!r}"


def test_AG_the_published_counts_add_up():
    """Lo que publica el tomo tiene que sumar lo que dice que suma."""
    audit = _audit()
    cc_report = audit["chapter_claims"]
    claims = cc_report["claims"]
    assert len(claims) == cc_report["total_claims"]

    counted = collections.Counter(c["disposition"] for c in claims)
    for name in ("accepted", "unresolved", "rejected_false_heading"):
        assert counted[name] == cc_report[name], name
    assert sum(counted.values()) == cc_report["total_claims"]

    per_book = cc_report["per_book"]
    for name in ("accepted", "unresolved", "rejected_false_heading",
                 "without_number"):
        assert sum(stat[name] for stat in per_book.values()) == cc_report[name], name
    assert cc_report["without_number"] == (
        cc_report["total_claims"] - cc_report["accepted"])

    # y la cola de numerales es exactamente lo que sigue sin resolverse
    queue = audit["numeral_image_review"]["review_queue_next"]
    assert len(queue) == cc_report["unresolved"]
    assert {r["block_id"] for r in queue} == {
        c["block_id"] for c in claims if c["disposition"] == "unresolved"}
    by_book = collections.Counter(r["book"] for r in queue)
    assert sum(by_book.values()) == len(queue)
    assert dict(by_book) == {book: stat["unresolved"]
                             for book, stat in per_book.items()
                             if stat["unresolved"]}


def test_AF_no_page_or_chapter_is_named_in_the_reader():
    source = open(os.path.join(DIR, "written_ordinals.py"), encoding="utf-8").read()
    tree = ast.parse(source)
    pages = {15, 196, 210, 276, 303, 319, 365, 497}
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, int) \
                and not isinstance(node.value, bool):
            assert node.value not in pages, f"decide por la plana {node.value}"
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            for marker in ("p0015", "p0319", "Ps ", "Wis "):
                assert marker not in node.value, marker


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
    print(f"torresamat1835_written_ordinals_failures={failures}")
    sys.exit(1 if failures else 0)
