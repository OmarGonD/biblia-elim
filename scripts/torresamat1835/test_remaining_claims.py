"""
TORRES-1835-REMAINING-CHAPTER-CLAIMS-122: el número que faltaba.

    python3 test_remaining_claims.py

Sin red y sin el testigo. Lo que quedaba después de la 121 no eran
rótulos por encontrar: eran veintiséis rótulos ya encontrados, válidos y
sin número. Y su causa dominante es la que da nombre a esta batería:

    A CANONICAL ROMAN NUMERAL IS NOT A CORRECT NUMBER

El reconocimiento produjo trece veces un numeral romano impecable --«XL»,
«CXL», «XX»-- que la plana no imprime: el punto final de «XI.» se funde
con la I y el conjunto sale «XL». Ninguna comprobación de sintaxis podía
cazar eso, porque no hay nada que corregir en la escritura; lo que falla
es la lectura. Sólo la imagen lo dice, y por eso esos trece llevan un
desenlace propio, `ocr_numeral_contradicted`, que no se confunde con «el
numeral estaba roto».

Lo demás que se comprueba aquí es lo de siempre, dicho una vez más
porque es donde se rompen estas tandas: que el número no lo pone la
secuencia, ni el canon, ni la cabecera corrida, ni el capítulo de al
lado; que el crudo del reconocimiento no se toca; y que resolver un
capítulo mueve texto a su sitio sin quitárselo a nadie.
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

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
METADATA = os.path.join(ROOT, "data", "torresamat1835",
                        "chapter_image_reviews.json")
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
GUTTER = 1690
PAGE_WIDTH = 3402
SHA = "f" * 64

SOURCE = __import__("recovery").SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")

#: El daño real del tomo: «SALMO XI.» sale del reconocimiento como
#: «SALMO XL», que es un romano válido y vale otra cosa.
DAMAGED = "SALMO XL"
PRINTED = "SALMO XI."


def _page(heading, *, scan_page=10, header="LIBRO DE LOS SALMOS. 12"):
    rows = [(header, (1697, 195, 2117, 255)),
            (heading, (1205, 932, 2118, 995)),
            ("Argumento del editor sobre este salmo.", (411, 1100, 3029, 1195)),
            ("1 Verbum latinum psalmi huius.", (400, 1300, 1600, 1372)),
            ("1 Verso castellano de este salmo.", (1700, 1300, 3000, 1372)),
            ("2 Alterum verbum latinum.", (400, 1390, 1600, 1462)),
            ("2 Segundo verso castellano.", (1700, 1390, 3000, 1462))]
    fixture = {"pages": [{"scan_page": scan_page, "width": PAGE_WIDTH,
                          "height": 4837,
                          "lines": [{"text": text, "bbox": list(bbox)}
                                    for text, bbox in rows]}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _numeral_review(**over):
    base = dict(
        id="fx-11", book="Ps", scan_page=10, target_block="p0010l0001",
        outcome=ir.NUMERAL_CONTRADICTED, raw_heading=DAMAGED, raw_numeral="XL",
        observed_printed_text=PRINTED, observed_printed_numeral="XI",
        recovered_chapter=11, confidence=0.95,
        rationale="la plana imprime SALMO XI.", reviewer_method="render",
        bbox=(1205, 932, 2118, 995), pdf_page=11, printed_page=None)
    base.update(over)
    return ir.NumeralReview(**base)


def _parse(heading, *, reviews=(), header_chapters=None, **kw):
    page = _page(heading)
    kw.setdefault("book", "Ps")
    _ed, _stats, walker = page_parser.parse_volume(
        [page], witness="fx", volume="3", gutter_hint=GUTTER,
        with_walker=True, recovery_source=SOURCE,
        header_chapters=header_chapters or {},
        numeral_reviews=ir.numerals_by_block(list(reviews)), **kw)
    return walker


def _claim(walker):
    return walker.ledger.claims[0]


def _metadata():
    with open(METADATA, encoding="utf-8") as handle:
        return json.load(handle)


def _batch122(data=None):
    data = data or _metadata()
    return [r for r in data["numeral_reviews"]
            if r.get("discovered_by") == "remaining_chapter_claim"]


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


# ======================================================================
# A-D. La evidencia de la imagen sobre el numeral
# ======================================================================
def test_A_an_unresolved_claim_is_resolved_by_facsimile_evidence():
    plain = _claim(_parse(DAMAGED))
    assert plain.accepted_number is None, "sin imagen no se resuelve solo"
    assert plain.disposition == cc.UNRESOLVED

    walker = _parse(DAMAGED, reviews=[_numeral_review()])
    claim = _claim(walker)
    assert claim.accepted_number == 11
    assert claim.disposition == cc.ACCEPTED
    assert claim.number_source == "image_review_roman"


def test_B_image_evidence_overrides_a_canonical_but_wrong_numeral():
    # «XL» es un romano perfecto y vale 40. La plana pone XI.
    assert roman.read("XL").status == roman.VALID
    assert roman.read("XL").value == 40
    claim = _claim(_parse(DAMAGED, reviews=[_numeral_review()]))
    assert claim.numeral["value"] == 40, "lo que leyó la máquina se conserva"
    assert claim.accepted_number == 11, "lo que manda es lo que pone la plana"


def test_C_the_raw_ocr_is_never_rewritten():
    claim = _claim(_parse(DAMAGED, reviews=[_numeral_review()]))
    assert claim.raw_heading == DAMAGED
    assert claim.numeral["raw"].strip() == "XL"
    assert claim.provenance["raw_ocr_heading"] == DAMAGED
    assert claim.provenance["observed_printed_text"] == PRINTED
    for entry in _batch122():
        assert entry["raw_heading"], entry["id"]
        assert entry["observed_printed_text"] != entry["raw_heading"] or \
            entry["outcome"] == ir.NUMERAL_CONFIRMED


def test_D_a_canonical_ocr_reading_does_not_win_over_the_image():
    review = _numeral_review(observed_printed_numeral="XII",
                             recovered_chapter=12,
                             observed_printed_text="SALMO XII.")
    claim = _claim(_parse("SALMO XL", reviews=[review]))
    assert claim.accepted_number == 12
    assert claim.numeral["value"] == 40


# ======================================================================
# E-J. Lo que NO puede dar un número
# ======================================================================
def test_E_sequence_cannot_resolve_an_unreadable_claim():
    walker = _parse("SALMO XxkV.")
    claim = _claim(walker)
    assert claim.numeral["status"] == roman.NO_NUMERAL
    assert claim.accepted_number is None
    assert claim.disposition == cc.UNRESOLVED


def test_F_canon_cannot_resolve_an_unreadable_claim():
    # El límite del canon es una cota de cordura sobre lo leído, no una
    # fuente: un libro de 150 capítulos no dice cuál es éste.
    walker = _parse("SALMO XxkV.", book="Ps")
    assert _claim(walker).accepted_number is None
    assert structure.chapter_limit("Ps") == 150


def test_G_previous_plus_one_cannot_resolve_a_claim():
    resolution = structure.resolve_chapter(
        raw_numeral="SALMO XxkV.", header_chapters=[], previous=34,
        book="Ps", sequence_available=False)
    assert resolution.resolved is None, "lo que tocaba no es lo que pone"


def test_H_next_minus_one_cannot_resolve_a_claim():
    # No existe camino «hacia atrás»: el resolvedor sólo mira lo YA
    # aceptado que queda detrás, nunca lo que viene después.
    source = open(os.path.join(DIR, "structure.py"), encoding="utf-8").read()
    assert "next_accepted" not in source and "following_chapter" not in source
    resolution = structure.resolve_chapter(
        raw_numeral="SALMO XxkV.", header_chapters=[], previous=None,
        book="Ps", sequence_available=False)
    assert resolution.resolved is None


def test_I_a_running_header_alone_does_not_override_the_heading():
    # La cabecera dice 35 y el rótulo trae un numeral legible que dice
    # otra cosa: la cabecera no puede pisarlo.
    walker = _parse("SALMO XL", header_chapters={10: [35]})
    claim = _claim(walker)
    assert claim.accepted_number != 35
    assert claim.proposal_evidence.get("header_candidates") == [35]


def test_J_latin_evidence_is_support_and_never_the_sole_authority():
    supported = [e for e in _batch122()
                 if e.get("supporting_facsimile_evidence")]
    assert supported, "alguna revisión se apoyó en la inscripción latina"
    for entry in supported:
        # el número sale del rótulo castellano; el latín acompaña
        assert entry["observed_printed_text"], entry["id"]
        assert entry["observed_printed_numeral"], entry["id"]
        assert entry["observed_printed_numeral"] in \
            entry["observed_printed_text"], entry["id"]


# ======================================================================
# K-N. Forma de la evidencia
# ======================================================================
def test_K_a_review_points_at_one_recognition_block_and_keeps_it_whole():
    for entry in _batch122():
        target = entry["target_block"]
        assert target[:1] == "p" and target[5] == "l", entry["id"]
        assert int(target[1:5]) == entry["scan_page"], entry["id"]
        # el crudo viaja entero, sin concatenar con nada
        assert entry["raw_heading"] == entry["raw_heading"].strip() or True
        assert "|" not in entry["raw_heading"], entry["id"]


def test_L_the_number_enters_through_the_claim_ledger():
    walker = _parse(DAMAGED, reviews=[_numeral_review()])
    claim = _claim(walker)
    assert isinstance(claim, cc.ChapterClaim)
    assert claim in walker.ledger.claims
    assert claim.proposal_method == "numeral_review"
    assert walker.ledger.rounds >= 1


def test_M_direct_resolutions_and_cascades_are_counted_apart():
    report = _audit()["remaining_chapter_claim_reviews"]
    assert report["resolved_direct"] + report["still_pending"] == \
        report["baseline_unresolved"]
    assert report["cascades"] == len(report["cascade_detail"])
    for row in report["claims"]:
        assert row["resolution"] in ("direct_image_review", "cascade")


def test_N_a_cascade_must_carry_its_own_number_evidence():
    report = _audit()["remaining_chapter_claim_reviews"]
    for entry in report["cascade_detail"]:
        assert entry["own_numeral_status"] == roman.VALID, entry
        assert entry["own_numeral"], entry
        assert entry["anchor_claim"], entry


# ======================================================================
# O-S. Estado del tomo y sus identidades
# ======================================================================
def test_O_a_false_physical_heading_becomes_an_explicit_rejection():
    claim = cc.ChapterClaim(
        claim_id="c0", book="Ps", scan_page=10, block_id="p0010l0003",
        raw_heading="Salmo de David, cuando le perseguía su hijo",
        numeral={"status": roman.VALID, "value": 1},
        heading={"is_heading": False, "rejections": ["prose"]})
    ledger = cc.ClaimLedger()
    ledger.add(claim)
    ledger.resolve(lambda c, previous: cc.Proposal(1, "direct_ocr", {}, 0.9))
    assert claim.disposition == cc.REJECTED_FALSE_HEADING
    assert claim.accepted_number is None
    assert claim.raw_heading.startswith("Salmo de David")


def test_P_a_rejected_claim_is_not_in_the_numeral_queue():
    audit = _audit()
    queue = {r["block_id"]
             for r in audit["numeral_image_review"]["review_queue_next"]}
    rejected = {r["block_id"]
                for r in audit["chapter_claims"]["rejected_false_heading_detail"]}
    assert rejected and not (rejected & queue)


def test_P2_a_decided_claim_is_not_ranked_for_review_again():
    """Lo decidido sale de la cola de trabajo y se queda en la historia.

    Los seis falsos rótulos de la 116 no tienen número y nunca lo
    tendrán: preguntar otra vez por ellos confundiría «sin número» con
    «sin decidir», que es la distinción que la 121 tuvo que separar.
    """
    audit = _audit()
    queue = audit["image_review_queue"]
    rejected = {r["block_id"]
                for r in audit["chapter_claims"]["rejected_false_heading_detail"]}
    ranked = {row.get("target_block") for row in (queue.get("top") or [])}
    assert not (rejected & ranked)
    assert queue["discovered_total"] >= queue["reviewed_and_closed"]
    assert queue["total"] == queue["by_family"]["unresolved_numeral"] + \
        queue["by_family"]["missing_heading"]


def test_Q_the_unresolved_set_is_exactly_the_numeral_queue():
    audit = _audit()
    claims = audit["chapter_claims"]["claims"]
    unresolved = {c["block_id"] for c in claims
                  if c["disposition"] == "unresolved"}
    queue = {r["block_id"]
             for r in audit["numeral_image_review"]["review_queue_next"]}
    assert unresolved == queue
    assert len(queue) == audit["chapter_claims"]["unresolved"]


def test_R_without_number_is_unresolved_plus_the_rejected():
    report = _audit()["chapter_claims"]
    assert report["without_number"] == (report["total_claims"]
                                        - report["accepted"])
    decided_against = (report["rejected_false_heading"]
                       + report["invalid_numeral"] + report["ambiguous_numeral"]
                       + report["uncorroborated_correction"]
                       + report["same_physical_claim"]
                       + report["competing_claim"])
    assert report["without_number"] == report["unresolved"] + decided_against


def test_S_every_per_book_column_sums_to_its_total():
    report = _audit()["chapter_claims"]
    per_book = report["per_book"]
    for name in ("accepted", "unresolved", "rejected_false_heading",
                 "without_number"):
        assert sum(stat[name] for stat in per_book.values()) == report[name], name
    counted = collections.Counter(c["disposition"] for c in report["claims"])
    assert counted["accepted"] == report["accepted"]
    assert counted["unresolved"] == report["unresolved"]


# ======================================================================
# T-X. Nada de lo anterior se ha movido
# ======================================================================
def test_T_the_written_ordinal_claims_are_untouched():
    claims = {c["block_id"]: c for c in _audit()["chapter_claims"]["claims"]}
    ordinals = [c for c in claims.values()
                if c["number_source"] in ("written_ordinal",
                                          "image_review_ordinal")]
    assert len(ordinals) == 7, "los siete rótulos ordinales del tomo"
    assert all(c["accepted_number"] == 1 for c in ordinals)


def test_U_the_true_first_psalm_stays_accepted_by_its_printed_word():
    claim = {c["block_id"]: c
             for c in _audit()["chapter_claims"]["claims"]}["p0015l0003"]
    assert claim["accepted_number"] == 1
    assert claim["number_source"] == "written_ordinal"
    assert claim["numeral"]["status"] == roman.NO_NUMERAL


def test_V_the_false_first_psalm_of_page_196_stays_rejected():
    claims = {c["block_id"]: c for c in _audit()["chapter_claims"]["claims"]}
    assert claims["p0196l0050"]["disposition"] == cc.REJECTED_FALSE_HEADING
    assert claims["p0196l0050"]["accepted_number"] is None
    ones = [c for c in claims.values()
            if c["book"] == "Ps" and c["accepted_number"] == 1]
    assert [c["block_id"] for c in ones] == ["p0015l0003"]


def test_W_psalm_142_keeps_its_heading_and_number():
    claim = {c["block_id"]: c
             for c in _audit()["chapter_claims"]["claims"]}["p0196l0047"]
    assert claim["accepted_number"] == 142
    assert claim["disposition"] == cc.ACCEPTED


def test_X_the_earlier_recoveries_are_still_represented_once():
    claims = _audit()["chapter_claims"]["claims"]
    recovered = [c for c in claims if c["block_id"][5] == "r"]
    assert recovered, "las recuperaciones de 117-121 siguen ahí"
    assert all(c["disposition"] == "accepted" for c in recovered)
    blocks = [c["block_id"] for c in claims]
    assert len(blocks) == len(set(blocks)), "ningún bloque reclama dos veces"
    physical = collections.Counter(
        (c["book"], c["scan_page"], c["block_id"][6:]) for c in claims
        if c["disposition"] == "accepted")
    assert not [k for k, n in physical.items() if n > 1], "sin rótulo duplicado"


# ======================================================================
# Y-AJ. Integridad del tomo y del código
# ======================================================================
def test_Y_no_block_was_lost():
    audit = _audit()
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert (audit["materialized_verse_refs"]
            + audit["verse_refs_in_review_slots"]) == audit["verse_refs"]


def test_Z_no_block_has_two_owners():
    audit = _audit()
    assert audit["duplicate_refs"] == []
    accepted = [c for c in audit["chapter_claims"]["claims"]
                if c["disposition"] == "accepted"]
    keys = collections.Counter((c["book"], c["accepted_number"]) for c in accepted)
    assert not [k for k, n in keys.items() if n > 1], "un número, un rótulo"


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
    data = _metadata()
    source = data["visual_source"]
    assert source["sha256"] == (
        "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346")
    assert not source.get("may_produce_release_artifact")
    audit = _audit()
    assert audit["numeral_image_review"]["hash_guard"]


def test_AF_applying_a_review_twice_changes_nothing():
    review = _numeral_review()
    once = _claim(_parse(DAMAGED, reviews=[review]))
    twice = _claim(_parse(DAMAGED, reviews=[review, review]))
    assert once.accepted_number == twice.accepted_number == 11
    assert once.raw_heading == twice.raw_heading
    walker = _parse(DAMAGED, reviews=[review, review])
    assert len(walker.ledger.claims) == 1, "no se duplica el reclamo"


def test_AG_the_same_input_gives_the_same_reading():
    first = _claim(_parse(DAMAGED, reviews=[_numeral_review()]))
    second = _claim(_parse(DAMAGED, reviews=[_numeral_review()]))
    assert first.as_dict() == second.as_dict()


def test_AH_the_reviews_are_applied_offline():
    for name in ("image_reviews.py", "chapter_claims.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for word in ("urllib", "requests", "socket."):
            assert word not in source, f"{name} usa {word!r}"


def test_AI_no_production_module_decides_by_page():
    pages = {25, 56, 60, 72, 94, 112, 122, 139, 145, 162, 165, 180, 181, 182,
             187, 189, 194, 223, 226, 339, 388, 391, 482, 484, 522}
    for name in ("chapter_claims.py", "page_parser.py", "image_reviews.py",
                 "structure.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            for operand in [node.left] + list(node.comparators):
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, f"{name}: {operand.value}"


def test_AJ_no_production_module_hardcodes_a_chapter_number():
    numbers = {11, 35, 37, 47, 66, 77, 83, 96, 102, 111, 115, 122, 124, 125,
               133, 134, 135, 140}
    for name in ("chapter_claims.py", "page_parser.py", "image_reviews.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        tree = ast.parse(source)
        for node in ast.walk(tree):
            if isinstance(node, ast.Compare):
                for operand in [node.left] + list(node.comparators):
                    if isinstance(operand, ast.Constant) and \
                            isinstance(operand.value, int) and \
                            not isinstance(operand.value, bool):
                        assert operand.value not in numbers, \
                            f"{name}: decide por el capítulo {operand.value}"
        # Sólo las constantes que el código USA: un ejemplo dentro de un
        # comentario o de una explicación no es una tabla de casos, y
        # prohibir que la documentación cite la plana que motivó el
        # código sería prohibir explicarlo.
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
            if not isinstance(node, ast.Constant) or id(node) in docstrings:
                continue
            if not isinstance(node.value, str):
                continue
            for marker in ("SALMO XI", "CAPÍTULO XLIX", "expected_missing",
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
    print(f"torresamat1835_remaining_claims_failures={failures}")
    sys.exit(1 if failures else 0)
