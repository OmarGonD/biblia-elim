"""
TORRES-1835-VERSE-MARKER-SANITY-125: qué número puede abrir un versículo.

    python3 test_verse_marker_sanity.py

Sin red y sin el testigo. Dos reglas, las dos de bajo riesgo, y conviene
no confundirlas nunca:

    CANON LIMIT MAY REJECT. CANON LIMIT MAY NOT CREATE.

        Isaías 26 tiene veintiún versículos, así que un «211» no es una
        frontera suya. Rechazarlo es barato y el texto se queda: pasa al
        versículo anterior. Lo que el canon NO puede hacer es lo
        contrario -- que numere un verso 7 no autoriza a inventarlo.

    EXACT DIGITS ONLY.

        «.63» se acepta porque el 63 está ENTERO en el crudo y sólo lo
        tapaba el punto que se arrastró del renglón de arriba. «a» no se
        convierte en 2, ni «S» en 8, ni «I o» en 10: para eso hay que
        mirar la plana, y es otra tanda.

Entre las dos cosas está el falso positivo que más importa evitar en
este testigo: «. 1 7 Yq sin embargo…» es el número 17 partido en dos por
el reconocimiento, y tomar el primer trozo como marcador inventaría un
versículo 1. Por eso un marcador enmarcado exige que lo que sigue NO
empiece por otra cifra.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import parser as classifier
import structure
import verse_markers
from model import BlockKind, Provenance

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")

PROV = Provenance(witness="fixture")


class _Verse:
    def __init__(self, blocks):
        self.blocks = list(blocks)


class _Block:
    def __init__(self, text, block_id):
        self.text = text
        self.provenance = type("P", (), {"block_id": block_id})()


class _Chapter:
    def __init__(self, number, verse_map, paratext=()):
        self.number = number
        self.verses = {n: _Verse([_Block(t, f"p0010l{n:04d}")])
                       for n, t in verse_map.items()}
        self.paratext = list(paratext)


def _classify(raw):
    return classifier.classify(raw, PROV)


def _limit(count):
    return lambda book, chapter: count


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]


def _refs():
    audit = _audit()
    return {f"{g['book']}.{g['chapter']}" for g in
            audit["verse_segmentation_audit"]["gaps"]}


def _materialized():
    """Las referencias que el tomo tiene hoy, como conjunto."""
    audit = _audit()
    seen = set()
    for row in audit["verse_segmentation_audit"]["gaps"]:
        seen.add((row["book"], row["chapter"]))
    return audit, seen


# ======================================================================
# A-E. Cifras exactas con puntuación exterior
# ======================================================================
def test_A_bare_digits_are_accepted():
    block = _classify("63 El fuego devoró sus jóvenes")
    assert block.kind is BlockKind.VERSE and block.number == 63


def test_B_a_leading_dot_is_trimmed():
    block = _classify(".63 El fuego devoró sus jóvenes")
    assert block.kind is BlockKind.VERSE and block.number == 63
    assert block.text.startswith("El fuego"), "el punto no entra en el texto"


def test_C_a_trailing_dot_is_trimmed():
    assert _classify("63. El fuego devoró").number == 63


def test_D_parentheses_are_trimmed():
    block = _classify("(63) El fuego devoró")
    assert block.kind is BlockKind.VERSE and block.number == 63


def test_E_brackets_are_trimmed():
    block = _classify("[63] El fuego devoró")
    assert block.kind is BlockKind.VERSE and block.number == 63


# ======================================================================
# F-L. Lo que NO se acepta
# ======================================================================
def test_F_internal_dot_is_rejected():
    assert _classify("6.3 El fuego devoró").kind is not BlockKind.VERSE
    assert classifier.safe_outer_trim("6.3") == "6.3", "por dentro no se toca"


def test_G_internal_dash_is_rejected():
    assert _classify("6-3 El fuego devoró").kind is not BlockKind.VERSE


def test_H_alphanumeric_is_rejected():
    assert _classify("63foo bar").kind is not BlockKind.VERSE


def test_I_a_letter_prefix_is_rejected():
    for raw in ("a63 El fuego", "I63 El fuego", "l63 El fuego"):
        assert _classify(raw).kind is not BlockKind.VERSE, raw


def test_J_the_glyph_S_is_not_read_as_eight():
    block = _classify("S y no se postrará ante los altares")
    assert block.kind is not BlockKind.VERSE
    assert block.number is None
    assert "S" not in classifier.SAFE_OUTER_PUNCTUATION


def test_K_the_glyph_a_is_not_read_as_two():
    block = _classify("a sino que tiene puesta toda su voluntad")
    assert block.kind is not BlockKind.VERSE
    assert block.number is None
    assert "a" not in classifier.SAFE_OUTER_PUNCTUATION


def test_L_the_pair_I_o_is_not_read_as_ten():
    for raw in ("I o Si uno va á caer", ". I o Por eso paran aquí"):
        block = _classify(raw)
        assert block.kind is not BlockKind.VERSE, raw
        assert block.number is None, raw


# ======================================================================
# M-O. La guarda de rango
# ======================================================================
def test_M_a_marker_above_the_limit_is_rejected():
    assert verse_markers.impossible(196, 27) == verse_markers.ABOVE_LIMIT
    chapter = _Chapter(11, {1: "uno", 17: "diecisiete", 196: "espurio"})
    records = verse_markers.enforce(chapter, book="Wis", verse_limit=_limit(27))
    assert [r["marker"] for r in records] == [196]
    assert sorted(chapter.verses) == [1, 17]


def test_N_a_marker_equal_to_the_limit_survives():
    assert verse_markers.impossible(27, 27) is None
    chapter = _Chapter(11, {1: "uno", 27: "el último"})
    assert verse_markers.enforce(chapter, book="Wis", verse_limit=_limit(27)) == []
    assert sorted(chapter.verses) == [1, 27]


def test_O_zero_is_not_an_ordinary_verse_marker():
    assert verse_markers.impossible(0, 27) == verse_markers.NOT_POSITIVE
    # y el título estructural no se toca: la guarda sólo mira capítulos
    # con número propio y versículos desde el 1.
    chapter = _Chapter(0, {0: "título"})
    assert verse_markers.enforce(chapter, book="Ps", verse_limit=_limit(6)) == []
    assert 0 in chapter.verses


# ======================================================================
# P-R. Nada de contexto decide el número
# ======================================================================
def test_P_a_missing_number_does_not_change_the_literal():
    # falta el 63 y el renglón trae «.64»: se lee 64, no 63
    assert _classify(".64 El fuego devoró").number == 64


def test_Q_previous_plus_one_does_not_change_the_marker():
    assert _classify(".7 Texto").number == 7
    # el lector no recibe contexto ninguno: su firma lo impide
    signature = classifier.classify.__code__.co_varnames[
        :classifier.classify.__code__.co_argcount]
    assert "previous" not in signature and "expected" not in signature


def test_R_next_minus_one_does_not_change_the_marker():
    source = open(os.path.join(DIR, "verse_markers.py"), encoding="utf-8").read()
    for word in ("previous +", "next -", "expected", "sequence"):
        assert word not in source, word


# ======================================================================
# S-V. Las regresiones reales del tomo
# ======================================================================
def test_S_psalm_77_63_is_recovered_from_its_own_literal():
    section = _section()
    framed = {(r["book"], r["verse"], r["block_id"]): r
              for r in section["exact_numeric_marker_recovery"]["markers"]}
    entry = framed[("Ps", 63, "p0116l0061")]
    assert entry["raw"].strip().startswith(".63")
    assert entry["scan_page"] == 116
    # y el hueco de Ps 77:63 ya no está en el inventario
    gaps = {(g["book"], g["chapter"], g["verse"]) for g in section["gaps"]}
    assert ("Ps", 77, 63) not in gaps


def test_T_wisdom_11_impossible_marker_is_gone():
    section = _section()
    rows = {(r["book"], r["chapter"], r["marker"]): r
            for r in section["impossible_marker_audit"]["markers"]}
    entry = rows[("Wis", 11, 196)]
    assert entry["verse_limit"] == 27
    assert entry["reason"] == verse_markers.ABOVE_LIMIT
    assert entry["text_landed"] == verse_markers.JOINED_PREVIOUS
    assert entry["new_owner_verse"] == 17
    assert entry["blocks"] > 0, "el texto no se borró"
    gaps = {(g["book"], g["chapter"], g["verse"]) for g in section["gaps"]}
    assert ("Wis", 11, 196) not in gaps


def test_U_isaiah_26_impossible_marker_is_gone():
    section = _section()
    rows = {(r["book"], r["chapter"], r["marker"]): r
            for r in section["impossible_marker_audit"]["markers"]}
    entry = rows[("Isa", 26, 211)]
    assert entry["verse_limit"] == 21
    assert entry["text_landed"] == verse_markers.JOINED_PREVIOUS
    assert entry["new_owner_verse"] == 19
    assert entry["blocks"] == 7


def test_V_rejecting_a_marker_preserves_the_text():
    chapter = _Chapter(26, {19: "diecinueve", 211: "el texto del último verso"})
    records = verse_markers.enforce(chapter, book="Isa", verse_limit=_limit(21))
    assert records and 211 not in chapter.verses
    kept = [block.text for block in chapter.verses[19].blocks]
    assert "el texto del último verso" in kept, "el texto sigue en el flujo"
    # y si no hay verso anterior, va al paratexto: tampoco se pierde
    orphan = _Chapter(26, {211: "sin nada delante"})
    verse_markers.enforce(orphan, book="Isa", verse_limit=_limit(21))
    assert not orphan.verses
    assert [b.text for b in orphan.paratext] == ["sin nada delante"]


def test_W_the_guard_is_deterministic_and_idempotent():
    def build():
        return _Chapter(11, {1: "uno", 17: "diecisiete", 99: "a", 196: "b"})
    first = verse_markers.enforce(build(), book="Wis", verse_limit=_limit(27))
    second = verse_markers.enforce(build(), book="Wis", verse_limit=_limit(27))
    assert [r["marker"] for r in first] == [r["marker"] for r in second] == [99, 196]
    # dos pasadas sobre el MISMO capítulo: la segunda no encuentra nada
    chapter = build()
    verse_markers.enforce(chapter, book="Wis", verse_limit=_limit(27))
    assert verse_markers.enforce(chapter, book="Wis",
                                 verse_limit=_limit(27)) == []


# ======================================================================
# X-AE. El tomo no se ha roto
# ======================================================================
def test_X_block_ownership_is_preserved():
    section = _section()
    audit = _audit()
    assert audit["materialized_verse_refs"] == audit["verse_refs"]
    assert section["impossible_marker_audit"]["blocks_preserved"] > 0
    landing = section["impossible_marker_audit"]["by_landing"]
    assert set(landing) <= {verse_markers.JOINED_PREVIOUS,
                            verse_markers.JOINED_PARATEXT}


def test_Y_no_block_was_lost():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_Z_no_reference_is_claimed_twice():
    audit = _audit()
    assert audit["duplicate_refs"] == []


def test_AA_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["accepted"] == 337
    assert audit["canonical_chapter_gap_reviews"]["pending_review"] == 0


def test_AB_chapter_claims_stay_resolved():
    audit = _audit()
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["numeral_image_review"]["review_queue_next"] == []


def test_AC_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AD_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_AE_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AM_every_recovered_marker_is_accounted_for():
    """Recuperar un marcador no es ganar una referencia.

    De los marcadores que la puntuación tapaba, unos abren una
    referencia que nadie más declara, otros caen en un versículo que ya
    tenía su renglón numerado, otros repiten el número que trajo otro
    marcador enmarcado, y unos pocos los retiró después la guarda de
    rango por imposibles. Las cuatro cosas suman el total: si no
    sumaran, habría marcadores sin explicar.
    """
    section = _section()
    recovery = section["exact_numeric_marker_recovery"]
    outcomes = recovery["by_outcome"]
    assert sum(outcomes.values()) == recovery["accepted"]
    assert len(recovery["markers"]) == recovery["accepted"]
    assert set(outcomes) <= {"only_numbered_line_of_its_ref",
                             "shares_ref_with_plain_marker",
                             "duplicate_same_verse_marker",
                             "rejected_later_by_range_guard",
                             "chapter_left_in_review"}
    assert recovery["only_numbered_line_of_its_ref"] == \
        outcomes.get("only_numbered_line_of_its_ref", 0)
    # cada marcador lleva su desenlace y, si abrió versículo, su
    # referencia final: nada queda sin clasificar
    for row in recovery["markers"]:
        assert row.get("outcome"), row["block_id"]
        if row["outcome"] in ("only_numbered_line_of_its_ref",
                              "shares_ref_with_plain_marker",
                              "duplicate_same_verse_marker"):
            assert row["final_ref"], row["block_id"]
            assert row["numbered_lines_in_ref"] >= 1


def test_AN_the_rejected_markers_are_counted_once():
    section = _section()
    recovery = section["exact_numeric_marker_recovery"]
    impossible = section["impossible_marker_audit"]
    rejected = [row for row in recovery["markers"]
                if row["outcome"] == "rejected_later_by_range_guard"]
    blocks = {block for record in impossible["markers"]
              for block in record["block_ids"]}
    # los marcadores enmarcados que la guarda retiró están, uno a uno,
    # entre los bloques que la guarda movió
    assert rejected and all(row["block_id"] in blocks for row in rejected)
    assert impossible["rejected"] >= len(rejected)


def test_AO_the_volume_identities_hold():
    audit = _audit()
    assert audit["verse_refs"] == audit["materialized_verse_refs"]
    assert audit["verse_refs_in_review_slots"] == 0
    assert audit["duplicate_refs"] == []
    assert audit["out_of_order_refs"] == []
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["accepted"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    section = _section()
    assert section["beyond_canonical_top"] == 0
    assert section["total"] == section["physical_domain"]


# ======================================================================
# AF-AL. Lo heredado de la 124, y lo que no se ha introducido
# ======================================================================
def test_AF_the_124_review_metadata_is_untouched():
    with open(REVIEWS, encoding="utf-8") as handle:
        data = json.load(handle)
    old = [r for r in data["reviews"] if r["batch"] == "batch-124"]
    assert len(old) == 12, len(old)
    for entry in data["reviews"]:
        assert entry["structural_effect"] == "none_diagnostic_only", entry["review_id"]
    outcomes = collections.Counter(r["outcome"] for r in old)
    assert outcomes["printed_marker_corrupted"] == 5
    assert outcomes["marker_present_parser_missed"] == 1


def test_AG_the_124_taxonomy_is_still_published():
    section = _section()
    for key in ("by_shape", "by_book", "by_signal", "strata", "sample",
                "priority_bank", "reviewed_by_outcome", "expected_count_source"):
        assert key in section, key
    assert section["reviewed"] == 12
    assert "native Vulgate versification" in section["expected_count_source"]


def test_AH_no_new_facsimile_metadata_was_added():
    with open(REVIEWS, encoding="utf-8") as handle:
        data = json.load(handle)
    # Lo que esta guarda vigila es que no aparezca un fichero de metadatos
    # nuevo por la puerta de atrás, y que las revisiones de la 124 sigan
    # ahí. Una tanda posterior SÍ puede añadir su propia tanda dentro del
    # mismo fichero: es el sitio donde tiene que estar.
    batches = {r["batch"] for r in data["reviews"]}
    assert "batch-124" in batches, batches
    # La lista es exhaustiva a propósito: cada metadato nuevo tiene que
    # declararse AQUÍ, de modo que ninguno aparezca sin que alguien lo
    # haya escrito. Lo que sigue prohibido es lo de siempre: escaneos,
    # volcados de OCR y cualquier asset de la fuente.
    data_dir = os.path.join(ROOT, "data", "torresamat1835")
    assert sorted(os.listdir(data_dir)) == [
        "a_glyph_pixel_features.json",
        "chapter_image_reviews.json", "glued_marker_discriminator.json",
        "glued_marker_segments.json", "no_trusted_band_discriminator.json",
        "projected_form_a_facsimile.json",
        "projected_rejection_audit.json",
        "remaining_glyph_inventory.json", "remaining_glyph_reprioritization.json",
        "source_manifest.json",
        "standalone_glyph_facsimile.json",
        "verse_boundary_reviews.json",
        "zero_anchor_io_recovery_validation.json"]


def test_AI_the_two_rules_work_offline():
    for name in ("verse_markers.py", "parser.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for word in ("urllib", "requests", "socket", "pdftoppm", "sword"):
            assert word not in source, f"{name} usa {word!r}"


def test_AJ_no_page_is_named_in_production_logic():
    pages = {15, 116, 340, 552, 276, 283, 534, 105}
    for name in ("verse_markers.py", "parser.py", "page_parser.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            for operand in [node.left] + list(node.comparators):
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, f"{name}: {operand.value}"


def test_AK_no_verse_number_is_hardcoded():
    numbers = {63, 196, 211, 194, 114}
    for name in ("verse_markers.py", "parser.py", "page_parser.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        tree = ast.parse(source)
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and isinstance(node.value, int) \
                    and not isinstance(node.value, bool):
                assert node.value not in numbers, f"{name}: {node.value}"


def test_AL_there_is_no_glyph_substitution_table():
    source = open(os.path.join(DIR, "verse_markers.py"), encoding="utf-8").read()
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if not isinstance(node, ast.Dict):
            continue
        # Lo que no puede haber: un glifo suelto que valga un número.
        # Las claves descriptivas de un registro («book», «marker») no
        # son eso, y prohibirlas sería prohibir escribir el informe.
        pairs = list(zip(node.keys, node.values))
        glyphs = [k.value for k, v in pairs
                  if isinstance(k, ast.Constant) and isinstance(k.value, str)
                  and len(k.value) == 1 and k.value.isalpha()
                  and isinstance(v, ast.Constant) and isinstance(v.value, int)]
        assert not glyphs, f"tabla de glifos: {glyphs}"
    # y la lista blanca no contiene ni una letra ni una cifra
    for char in classifier.SAFE_OUTER_PUNCTUATION:
        assert not char.isalnum(), char


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
    print(f"torresamat1835_verse_marker_sanity_failures={failures}")
    sys.exit(1 if failures else 0)
