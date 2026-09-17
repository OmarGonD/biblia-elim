"""
TORRES-1835-CORRUPTED-DIVISION-MARKERS-118: buscar la palabra rota.

    python3 test_corrupted_markers.py

Sin red y sin el testigo. La 117 encontró tres rótulos que el
reconocimiento sí escribió y cuya PALABRA destrozó; esta tanda pregunta
si eran casos aislados, y para eso hace falta un buscador:

    CANDIDATE GENERATION MAY BE HEURISTIC
    RECOVERY MAY NOT

Aquí se comprueban las dos mitades. Que el buscador encuentre lo que
tiene que encontrar -- incluidos los tres de 117, por señales genéricas y
sin nombrar ninguna plana -- y que no confunda con un rótulo la
inscripción del salmo, que lleva la misma palabra y a veces un número
detrás. Y que nada de lo que encuentra se convierta en capítulo sin que
la imagen lo diga.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import chapter_claims as cc
import corrupted_markers as cm
import image_reviews as ir
import page_parser
import parser as classifier
import recovery
import source_ocr
from layout import Column, Zone

DIR = os.path.dirname(os.path.abspath(__file__))
GUTTER = 1690
PAGE_WIDTH = 3402
SHA = "f" * 64

#: Una caja de rótulo: cruza el canal y va centrada sobre él.
HEADING_BBOX = (1417, 1907, 2031, 1970)
#: Una caja de la columna española.
COLUMN_BBOX = (1799, 2648, 3041, 2723)


def _look(raw, *, column=Column.SPANNING, zone=Zone.BODY, bbox=None,
          book="Ps", represented_by=None):
    return cm.inspect(raw_text=raw, column=column, zone=zone,
                      bbox=bbox or HEADING_BBOX, page_width=PAGE_WIDTH,
                      book=book, represented_by=represented_by)


# ======================================================================
# A/B/C. Los tres de 117 salen por señales genéricas
# ======================================================================
def test_ABC_the_three_known_cases_are_found_by_generic_signals():
    for raw in ("S A L M O X L.", "5ALMO CXXXVI.", "6'ALMO XCI."):
        candidate, why = _look(raw)
        assert candidate is not None, (raw, why)
        assert candidate.rank == cm.HIGH, (raw, candidate.rank)
        assert candidate.marker_family == cm.PSALM_LIKE
        assert candidate.numeral_tokens, raw
        assert candidate.evidence


def test_the_numeral_is_not_reassembled_by_the_queue():
    """Un numeral partido se señala; cuánto vale lo dice la imagen."""
    candidate, _why = _look("S A L M O X L.")
    assert candidate.numeral_tokens == ["X", "L"]
    assert candidate.numeral_value is None, "juntar los trozos sería inventar"
    whole, _why = _look("5ALMO CXXXVI.")
    assert whole.numeral_tokens == ["CXXXVI"]
    assert whole.numeral_value == 136


# ======================================================================
# D/E/G/H. Lo que NO es candidato
# ======================================================================
def test_D_a_clean_heading_is_not_a_corrupted_candidate():
    for raw in ("SALMO XL.", "SALMO CXXXVI.", "CAPÍTULO XXV.",
                "SALMO PRIMERO."):
        candidate, why = _look(raw)
        assert candidate is None, raw
        assert why == cm.ALREADY_A_DIVISION, (raw, why)
        # y es que el clasificador ya los lee: no se pierde ninguno
        assert classifier.carries_division_marker(raw), raw


def test_E_the_psalm_inscription_is_never_a_candidate():
    """«Salmo de David…» lleva la palabra y a veces un número detrás."""
    for raw in ("Salmo de Bavíd 1 enando Je persegnia",
                "Salmo de David, cuando le perseguia su hijo Absalon CXLII.",
                "Salmo del mismo David para el cuarto dia de la semana IV.",
                "5almo y Cantico para el dia del sabado XCI."):
        for column in (Column.RIGHT, Column.SPANNING):
            candidate, why = _look(raw, column=column, bbox=COLUMN_BBOX)
            assert candidate is None, (raw, column)
            assert why in (cm.NO_NUMERAL, cm.READS_AS_PROSE,
                           cm.NO_MARKER_SHAPE), why


def test_G_a_numeral_on_its_own_is_not_a_candidate():
    for raw in ("XL.", "CXXXVI.", "1 2 3", "XL"):
        candidate, why = _look(raw)
        assert candidate is None, raw
        assert why in (cm.NO_MARKER_SHAPE, cm.NO_NUMERAL), (raw, why)


def test_H_centred_prose_without_the_word_is_not_a_candidate():
    for raw in ("Fervorosa oración que hace David á Dios XL.",
                "Argumento del editor sobre este salmo XL.",
                "1 In finem, Psalmus ipsi David. XL."):
        candidate, why = _look(raw)
        assert candidate is None, raw
        assert why in (cm.NO_MARKER_SHAPE, cm.READS_AS_PROSE), (raw, why)


def test_I_the_header_and_the_apparatus_are_left_out():
    for zone in (Zone.HEADER, Zone.FOOTER, Zone.APPARATUS):
        candidate, why = _look("5ALMO CXXXVI.", zone=zone)
        assert candidate is None, zone
        assert why == cm.OUTSIDE_THE_BODY


# ======================================================================
# F. La otra palabra del vocabulario, sin un buscador propio
# ======================================================================
def test_F_the_chapter_word_is_found_the_same_way():
    for raw in ("C A P I T U L O XII.", "C4PITULO XII.", "CAPÍtULO VIII.",
                "GAPÍTÜLO XXIV-", "CAFÍTULO XIV."):
        candidate, why = _look(raw, book="Isa")
        assert candidate is not None, (raw, why)
        assert candidate.marker_family == cm.CHAPTER_LIKE, raw

    # «CAPITUL0» con cero YA lo lee el clasificador: no se ha perdido, así
    # que no es asunto de esta cola. Es el resultado correcto, no un fallo
    # del buscador.
    assert classifier.carries_division_marker("CAPITUL0 XII.")
    candidate, why = _look("CAPITUL0 XII.", book="Isa")
    assert candidate is None and why == cm.ALREADY_A_DIVISION


def test_the_vocabulary_comes_from_the_importer_and_not_from_this_module():
    import ast
    source = open(os.path.join(DIR, "corrupted_markers.py"),
                  encoding="utf-8").read()
    tree = ast.parse(source)
    docstrings = set()
    for node in ast.walk(tree):
        if isinstance(node, (ast.Module, ast.FunctionDef, ast.ClassDef)):
            doc = ast.get_docstring(node, clean=False)
            if doc:
                docstrings.add(doc)
    literals = [n.value for n in ast.walk(tree)
                if isinstance(n, ast.Constant) and isinstance(n.value, str)
                and n.value not in docstrings]
    # el módulo puede NOMBRAR las palabras para clasificarlas en familias,
    # pero la lista de lo que es una división vive en el importador
    assert "SALMO" in classifier.DIVISION_WORDS
    assert "CAPITULO" in classifier.DIVISION_WORDS
    assert cm._FAMILY.keys() <= set(classifier.DIVISION_WORDS)
    for book in ("Ps", "Prov", "Eccl", "Song", "Wis", "Sir", "Isa"):
        assert book not in literals, book


# ======================================================================
# X. Ninguna plana escrita en el código de producción
# ======================================================================
def test_X_no_page_is_hardcoded_in_production():
    import ast
    pages = {64, 133, 190, 18, 25, 36, 121, 143, 187}
    for name in ("corrupted_markers.py", "recovery.py", "page_parser.py",
                 "image_reviews.py", "parser.py"):
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
        for marker in ("p0064", "p0133", "p0190", "p0018", "p0187"):
            assert not any(marker in t for t in texts), f"{name}: {marker}"


# ======================================================================
# V/W. Determinista y sin red
# ======================================================================
def test_V_the_sweep_is_deterministic():
    lines = [("5ALMO CXXXVI.", Column.SPANNING), ("S A L M O X L.", Column.SPANNING),
             ("6'ALMO XCI.", Column.SPANNING), ("una linea cualquiera", Column.RIGHT)]

    def run():
        out = []
        for raw, column in lines:
            candidate, _why = _look(raw, column=column)
            if candidate:
                out.append(candidate.as_dict())
        return json.dumps(out, sort_keys=True, default=str)
    assert run() == run()

    # y el orden de la cola no depende de ningún diccionario
    page = _page([("5ALMO CXXXVI.", HEADING_BBOX)])

    def queued():
        found, _rejected = cm.scan([(page, _entries(page))],
                                   book_at=lambda p: "Ps")
        return [c.block_id for c in found]
    assert queued() == queued()


# ======================================================================
# K/L/M/N/O/P/Q/R. La recuperación, sobre el renglón que ya existe
# ======================================================================
def _page(lines, *, scan_page=10):
    """Una plana con sus dos columnas pobladas y un rótulo entre medias."""
    rows = [{"text": "LIBRO DE LOS SALMOS. 56", "bbox": [1697, 195, 2117, 255]},
            # un rótulo anterior que la máquina SÍ escribió bien: así hay
            # un capítulo abierto y se puede comprobar que lo de antes de
            # la frontera recuperada se queda donde estaba
            {"text": "SALMO XXXIX.", "bbox": [1417, 400, 2031, 463]},
            {"text": "17 Verbum latinum psalmi prioris.", "bbox": [400, 700, 1600, 772]},
            {"text": "17 Verso del salmo anterior.", "bbox": [1700, 700, 3000, 772]},
            {"text": "continuatio latina eiusdem versus.", "bbox": [400, 790, 1600, 862]},
            {"text": "18 Ultimo verso del salmo anterior.", "bbox": [1700, 790, 3000, 862]}]
    for text, bbox in lines:
        rows.append({"text": text, "bbox": list(bbox)})
    rows += [{"text": "Argumento del editor sobre este salmo.",
              "bbox": [411, 2100, 3029, 2195]},
             {"text": "1 In finem, Psalmus ipsi David. XL.", "bbox": [400, 2300, 1600, 2372]},
             {"text": "2 Primer verso del salmo nuevo.", "bbox": [1700, 2300, 3000, 2372]},
             {"text": "continuatio latina sequentis.", "bbox": [400, 2390, 1600, 2462]},
             {"text": "3 Segundo verso del salmo nuevo.", "bbox": [1700, 2390, 3000, 2462]}]
    fixture = {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                          "height": 4837, "lines": rows}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _entries(page):
    from layout import split_columns
    return split_columns(page, gutter_hint=GUTTER)


def _review(**over):
    base = dict(
        id="fx-40", book="Ps", scan_page=10,
        outcome=ir.BOUNDARY_AND_NUMBER, chapter_number=40,
        boundary_confirmed=True, numeral_confirmed=True,
        insert_after_block="p0010l0005", insert_before_block="p0010l0007",
        heading_block="p0010l0006", observed_printed_text="SALMO XL.",
        crop_bbox=(0, 800, 3402, 2200), confidence=0.95,
        rationale="the facsimile prints SALMO XL. across the gutter",
        reviewer_method="render", pdf_page=11)
    base.update(over)
    return ir.ChapterImageReview(**base)


SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")


def _parse(page, reviews):
    return page_parser.parse_volume(
        [page], witness="fx", volume="3", book="Ps", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        image_reviews={page.scan_page: list(reviews)})


def test_K_a_candidate_with_a_review_is_marked_and_not_queued_again():
    candidate, _why = _look("S A L M O X L.", represented_by="ps-40-p64")
    assert candidate.already_represented_by == "ps-40-p64"
    assert any("already represented" in line for line in candidate.evidence)


def test_L_the_recovery_marks_the_existing_line_and_adds_none():
    page = _page([("S A L M O X L.", HEADING_BBOX)])
    entries = _entries(page)
    out, records = recovery.apply_verified_image_reviews(
        page, entries, [_review()], source=SOURCE, book="Ps")
    assert [r.action for r in records] == [recovery.RESOLVED]
    assert len(out) == len(entries), "no se inserta ningún renglón"
    marked = [e for e in out if getattr(e, "recovery", None)]
    assert len(marked) == 1
    assert marked[0].line.raw_text == "S A L M O X L."


def test_MNO_raw_ocr_survives_and_the_claim_goes_through_the_ledger():
    page = _page([("S A L M O X L.", HEADING_BBOX)])
    edition, _s, walker = _parse(page, [_review()])
    claim = next(c for c in walker.ledger.claims
                 if c.source == cc.FROM_IMAGE_REVIEW)
    assert claim.disposition == cc.ACCEPTED and claim.accepted_number == 40
    # N: la evidencia de rótulo es la de la imagen, sobre el mismo bloque
    assert claim.heading["is_heading"] is True
    assert claim.heading["source"] == "image_review"
    # M: el crudo sigue diciendo lo que dijo la máquina
    assert claim.provenance["raw_ocr_heading"] == "S A L M O X L."
    assert claim.raw_heading == "SALMO XL."
    heading_blocks = [b for ch in edition.books["Ps"].chapters.values()
                      for b in ch.paratext
                      if b.raw_text == "S A L M O X L."]
    assert len(heading_blocks) == 1
    # O: el capítulo existe porque el ledger lo aceptó, no por escritura
    assert 40 in edition.books["Ps"].chapters


def test_PQR_block_ownership_is_exact():
    page = _page([("S A L M O X L.", HEADING_BBOX)])
    edition, _s, _w = _parse(page, [_review()])
    owner = {}
    for number, chapter in edition.books["Ps"].chapters.items():
        for verse_number, verse in chapter.verses.items():
            for block in verse.blocks:
                assert block.provenance.block_id not in owner, "doble dueño"
                owner[block.provenance.block_id] = (number, verse_number)
        for block in chapter.paratext:
            owner.setdefault(block.provenance.block_id, (number, None))
    # el latín paralelo es procedencia, no cuerpo, pero tiene dueño: sin
    # contarlo, «no se pierde ningún bloque» sería una comprobación falsa
    latin = {b.provenance.block_id
             for chapter in edition.books["Ps"].chapters.values()
             for b in chapter.parallel_latin}
    queued = {b.provenance.block_id for b in edition.review_queue}

    # P: lo de antes del rótulo se queda con el salmo de antes...
    previous = owner["p0010l0003"][0]
    assert previous != 40
    assert owner["p0010l0005"][0] == previous
    # ...y lo de después es del salmo recuperado
    assert owner["p0010l0007"][0] == 40      # el argumento del editor
    assert owner["p0010l0009"][0] == 40      # el primer verso español
    assert owner["p0010l0011"][0] == 40
    # Q: ningún bloque desaparece: o tiene dueño o está en la cola
    page_blocks = {f"p0010l{i:04d}" for i in range(12)}
    recovered = {"p0010r0006"}
    seen = set(owner) | queued | recovered | latin
    lost = {b for b in page_blocks if b not in seen and b != "p0010l0006"}
    assert not lost, lost
    # R: el rótulo vive una sola vez, con su identificador de recuperación
    assert "p0010r0006" in owner
    assert "p0010l0006" not in owner


def test_a_line_the_facsimile_does_not_confirm_recovers_nothing():
    """Un candidato no es un capítulo: sin revisión no pasa nada."""
    page = _page([("S A L M O X L.", HEADING_BBOX)])
    _ed, _s, walker = _parse(page, [])
    assert not [c for c in walker.ledger.claims
                if c.source == cc.FROM_IMAGE_REVIEW]
    assert not [c for c in walker.ledger.accepted() if c.accepted_number == 40]


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


def test_the_shipped_batch_validates_and_marks_existing_lines():
    payload = ir.load()
    assert ir.validate(payload, page_count=652) == []
    batch = [r for r in payload["reviews"] if r.get("batch") == "batch-118"]
    assert batch, "la tanda 118 está en los datos"
    for review in batch:
        assert review["heading_block"], review["id"]
        assert review["outcome"] == ir.BOUNDARY_AND_NUMBER
        assert isinstance(review["chapter_number"], int)
        assert review["insert_after_block"] and review["insert_before_block"]
        assert review["pdf_page"] == review["scan_page"] + 1
        assert review["discovered_by"] == "corrupted_division_marker_candidates"
        assert len(review["rationale"]) > 120
        assert review["ocr_state"] == "heading present, division word corrupted"


def test_the_volume_sweep_finds_the_known_cases_and_nothing_false():
    report = _audit()
    if report is None:
        print("  (saltado: no hay audit en build/)")
        return
    section = report["corrupted_division_marker_candidates"]
    rows = {row["block_id"]: row for row in section["candidates"]}
    # los tres de 117, encontrados otra vez y ya representados
    for block in ("p0064l0039", "p0133l0071", "p0190l0059"):
        assert block in rows, block
        assert rows[block]["review_outcome"] == "already_represented"
        assert rows[block]["recovered_chapter"] is not None
    # ninguna de las seis inscripciones falsas de 116 está aquí
    false_blocks = {r["block_id"]
                    for r in report["heading_claim_validation"]["false_headings"]}
    assert not (false_blocks & set(rows)), false_blocks & set(rows)
    # y el salmo primero tampoco: su palabra está bien
    assert "p0015l0003" not in rows
    assert section["awaiting_review"] == section["total"] - \
        section["already_represented"]


def test_J_the_false_headings_stay_rejected():
    report = _audit()
    if report is None:
        return
    claims = {c["block_id"]: c for c in report["chapter_claims"]["claims"]}
    for block in ("p0064l0066", "p0101l0062", "p0133l0079", "p0135l0059",
                  "p0190l0073", "p0196l0050"):
        assert claims[block]["disposition"] == cc.REJECTED_FALSE_HEADING
        assert claims[block]["accepted_number"] is None


def test_U_the_first_psalm_is_untouched():
    report = _audit()
    if report is None:
        return
    claim = {c["block_id"]: c
             for c in report["chapter_claims"]["claims"]}["p0015l0003"]
    assert claim["raw_heading"].upper().startswith("SALMO PRIMERO")
    assert claim["disposition"] == cc.UNRESOLVED
    assert claim["accepted_number"] is None
    assert claim["numeral"]["status"] == "no_numeral"


def test_ST_the_volume_keeps_its_invariants():
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
    cr = report["chapter_image_recovery"]
    assert cr["reviews_failed_anchor_validation"] == 0
    assert cr["reviews_conflicting_number"] == 0
    assert cr["reviews_colliding_with_existing_chapter"] == 0
    assert cr["recovered_number_collisions"] == []
    # cada recuperación de esta tanda marcó un renglón existente
    actions = {a["review_id"]: a for a in cr["applications"]}
    for review in ir.load()["reviews"]:
        if review.get("batch") != "batch-118":
            continue
        record = actions[review["id"]]
        assert record["action"] == recovery.RESOLVED, review["id"]
        assert record["target_block"] == review["heading_block"]
        assert record["chapter_number"] == review["chapter_number"]


def test_the_two_queues_stay_apart():
    report = _audit()
    if report is None:
        return
    markers = report["corrupted_division_marker_candidates"]
    missing = report["image_review_queue"]
    assert "not_the_same_as_missing_heading" in markers
    # la cola de rótulos ausentes sigue existiendo y midiendo lo suyo:
    # se mira lo DESCUBIERTO, que es lo que esa familia detecta, y no lo
    # pendiente, que puede estar a cero por haberse revisado entero
    assert "missing_heading" in missing["discovered_by_family"]
    assert missing["discovered_by_family"]["missing_heading"] > 0
    marker_pages = {row["scan_page"] for row in markers["candidates"]}
    missing_pages = {row["scan_page"]
                     for row in report["missing_heading_reviews"]["candidates"]
                     if row["discovered_now"]}
    assert not (marker_pages & missing_pages), \
        "una plana ya recuperada no puede seguir contando como rótulo ausente"


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
    print(f"torresamat1835_corrupted_markers_failures={failures}")
    sys.exit(1 if failures else 0)
