"""
TORRES-1835-EMBEDDED-EXACT-VERSE-MARKERS-126: la cifra ya estaba ahí.

    python3 test_embedded_markers.py

Sin red y sin el testigo. La tanda fue a buscar los huecos cuyo número
decimal ya existe, entero, en un renglón que hoy pertenece al versículo
anterior, y encontró doce. Al mirarlos uno a uno apareció el dato que
cambia el diseño:

    ninguno está embebido en mitad del texto.

Los doce encabezan su renglón, y lo que los tapaba era un glifo de basura
del canto --«>», «■», «=», «<»-- que la lista de la tanda 125 no
contemplaba. De modo que no hizo falta partir lógicamente ningún bloque
del reconocimiento: se ensanchó el marco del marcador, con cuatro glifos
medidos y mirados uno a uno, y ocho de los doce quedaron resueltos por el
camino que ya existía.

    THE DIGIT ALREADY EXISTS

Lo que sigue prohibido es todo lo demás: ninguna letra se convierte en
cifra, ningún número se completa, y los cuatro casos que quedan --el
número pegado a la palabra, el que lleva otra cifra detrás, el que tiene
dos cifras candidatas-- se quedan pendientes con su motivo escrito, que
es más barato que acertar por casualidad.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import parser as classifier
import verse_gaps
import verse_markers
from model import BlockKind, Provenance

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")

PROV = Provenance(witness="fixture")


def _classify(raw):
    return classifier.classify(raw, PROV)


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]


def _embedded():
    return _section()["embedded_exact_marker_recovery"]


# ======================================================================
# A-F. La cifra que ya está abre frontera
# ======================================================================
def test_A_a_digit_behind_scan_debris_opens_a_boundary():
    for raw in (">6 Eslbraad los aullidos", "■ 5 Y si en esta vida",
                "= 10 Pió te rias con él", "<5 No temas la sentencia"):
        block = _classify(raw)
        assert block.kind is BlockKind.VERSE, raw
        assert block.number in (5, 6, 10), raw


def test_B_the_prefix_junk_never_reaches_the_text():
    block = _classify("■ 5 Y si en esta vida se codician")
    assert block.text.startswith("Y si en esta vida")
    assert "■" not in block.text


def test_C_the_rest_of_the_line_becomes_the_new_verse():
    block = _classify(">7 Aiordo de tus beneficios")
    assert block.number == 7
    assert block.text == "Aiordo de tus beneficios"


def test_D_the_raw_block_is_never_rewritten():
    raw = ">6 Eslbraad los aullidos» porque cof*"
    before = raw
    _classify(raw)
    assert raw == before, "el crudo entra y sale igual"
    # y el tomo conserva sus bloques
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_E_prefix_marker_and_text_reconstruct_the_line():
    raw = "= 10 Pió te rias con él» no sea que al"
    block = _classify(raw)
    literal = str(block.number)
    head, _, tail = raw.partition(literal)
    # nada se duplica y nada se pierde: el renglón es cabecera + cifra +
    # texto, y el texto del bloque es ese resto sin espacios de sobra
    assert head + literal + tail == raw
    assert block.text == tail.strip()


def test_F_the_marker_behaves_like_any_other_marker():
    plain = _classify("10 Pió te rias con él")
    framed = _classify("= 10 Pió te rias con él")
    assert plain.kind is framed.kind is BlockKind.VERSE
    assert plain.number == framed.number == 10
    assert plain.text == framed.text, "mismo texto, mismo trato"


# ======================================================================
# G-N. Lo que no puede pasar por esta ruta
# ======================================================================
def test_G_the_literal_must_equal_the_verse_number():
    # el lector no sabe qué verso falta: lee lo que hay
    assert _classify("> 4 Tuvieron sed").number == 4
    assert _classify("> 5 Tuvieron sed").number == 5


def test_H_the_glyph_S_cannot_become_eight():
    block = _classify("S y no se postrará ante los altares")
    assert block.kind is not BlockKind.VERSE and block.number is None
    assert "S" not in classifier.SAFE_OUTER_MARKER_FRAME


def test_I_the_glyph_a_cannot_become_two():
    block = _classify("a sino que tiene puesta toda su voluntad")
    assert block.kind is not BlockKind.VERSE and block.number is None
    assert "a" not in classifier.SAFE_OUTER_MARKER_FRAME


def test_J_the_pair_I_o_cannot_become_ten():
    for raw in ("I o Si uno va á caer", "> I o Si uno va á caer"):
        block = _classify(raw)
        assert block.kind is not BlockKind.VERSE, raw


def test_K_a_year_or_quantity_inside_a_phrase_creates_no_marker():
    for raw in ("en el año 1834 se imprimió en Madrid",
                "y tenía 300 carros de guerra",
                "Véase Matth. 11: 12, 13."):
        assert _classify(raw).kind is not BlockKind.VERSE, raw


def test_L_a_digit_glued_to_a_word_is_rejected():
    assert _classify("2as^; porque nuestra viña").kind is not BlockKind.VERSE
    assert _classify("18Nuevo texto").kind is not BlockKind.VERSE


def test_M_a_missing_delimiter_before_is_rejected():
    # letra pegada delante: no hay marco, hay palabra
    for raw in ("a63 El fuego", "foo8bar texto", "I63 El fuego"):
        assert _classify(raw).kind is not BlockKind.VERSE, raw


def test_N_a_missing_delimiter_after_is_rejected():
    assert _classify(">63foo bar").kind is not BlockKind.VERSE
    assert _classify("■6.3 texto").kind is not BlockKind.VERSE


# ======================================================================
# O-R. La guarda de rango y los casos repetidos
# ======================================================================
def test_O_a_marker_above_the_limit_is_still_rejected():
    assert verse_markers.impossible(98, 19) == verse_markers.ABOVE_LIMIT
    section = _section()
    assert section["impossible_marker_audit"]["rejected"] > 0
    assert section["beyond_canonical_top"] == 0


def test_P_a_marker_equal_to_the_limit_is_accepted():
    assert verse_markers.impossible(19, 19) is None


def test_Q_a_repeated_verse_does_not_duplicate_a_reference():
    audit = _audit()
    assert audit["duplicate_refs"] == []
    # El reparto tiene que ser exhaustivo: cada marcador aceptado cae en
    # una categoría y en una sola. QUÉ categorías aparecen depende de lo
    # que haya en el tomo -- una recuperación posterior puede dejar la de
    # los duplicados vacía porque ya no hay verso compartido -- y eso no
    # es lo que este test cuida.
    recovery = _section()["exact_numeric_marker_recovery"]
    outcomes = recovery["by_outcome"]
    assert sum(outcomes.values()) == recovery["accepted"]
    assert set(outcomes) <= {"only_numbered_line_of_its_ref",
                             "duplicate_same_verse_marker",
                             "shares_ref_with_plain_marker",
                             "rejected_later_by_range_guard",
                             "chapter_left_in_review"}, outcomes


def test_R_ambiguous_candidates_are_left_pending_with_a_reason():
    embedded = _embedded()
    assert embedded["still_pending"] == len(embedded["pending"])
    reasons = {row["reason"] for row in embedded["pending"]}
    assert reasons <= {"digit_follows_marker", "marker_glued_to_word",
                       "two_candidate_digits", "other_context"}
    for row in embedded["pending"]:
        assert row["raw"] and row["literal"] and row["block_id"]
        assert row["current_owner_verse"] is None or \
            isinstance(row["current_owner_verse"], int)


# ======================================================================
# S-X. Lo que esta tanda NO tocó
# ======================================================================
def test_S_detached_numeric_fragments_are_out_of_scope():
    gaps = _section()["gaps"]
    detached = [g for g in gaps
                if verse_gaps.NUMERIC_FRAGMENT in g["signals"]]
    assert detached, "la clase sigue existiendo, sin tocar"


def test_T_gaps_without_local_evidence_are_out_of_scope():
    gaps = _section()["gaps"]
    blind = [g for g in gaps
             if verse_gaps.NO_NUMERIC_EVIDENCE in g["signals"]]
    assert len(blind) > 1000, "la clase grande sigue intacta"


def test_U_no_block_serves_two_verses():
    # No se ha partido ningún bloque: la sección lo dice y el tomo lo
    # confirma con sus referencias.
    assert _embedded()["blocks_logically_split"] == 0
    audit = _audit()
    assert audit["duplicate_refs"] == []
    assert audit["verse_refs"] == audit["materialized_verse_refs"]


def test_V_no_text_was_duplicated():
    raw = "> 18 El alma de un varón piadoso"
    block = _classify(raw)
    assert raw.count(block.text) == 1
    assert block.text not in ">" and block.text != raw


def test_W_no_text_was_lost():
    raw = "> 19 porque de la tristeza viene"
    block = _classify(raw)
    # todo lo que no es marco ni cifra sigue en el texto
    assert block.text == "porque de la tristeza viene"
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_X_the_glyph_cases_are_untouched():
    """Esta tanda no convierte ninguna letra en cifra.

    Los cinco casos eran, en la 126, huecos que sólo se podían cerrar
    mirando la plana. Tres lo siguen siendo, y por la misma razón de
    entonces: su marcador es un glifo SUELTO, y un glifo suelto es unas
    veces cifra y otras castellano. Los otros dos los cerró la 128, pero
    no por parecido: su numeral son DOS glifos, la 128 agotó esa
    población en el facsímil y exige además que el número caiga en la
    banda de sangría de la plana. Lo que aquí se vigila es que la regla
    de la 126 --cifras decimales enteras y nada más-- no se haya
    ensanchado a hurtadillas.
    """
    gaps = {(g["book"], g["chapter"], g["verse"]) for g in _section()["gaps"]}
    for key in (("Ps", 1, 2), ("Eccl", 1, 2), ("Isa", 17, 8)):
        assert key in gaps, f"{key} debe seguir sin recuperar"
    for raw in ("a sino que tiene puesta toda su voluntad",
                "S y no se postrará ante los altares",
                "I o Si uno va á caer, el otro le sostiene"):
        assert classifier.framed_verse_marker(raw) == (None, None), raw


# ======================================================================
# Y-AJ. El tomo y el código
# ======================================================================
def test_Y_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["accepted"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["canonical_chapter_gap_reviews"]["pending_review"] == 0


def test_Z_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AA_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_AB_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AC_the_verse_boundary_reviews_are_untouched():
    with open(REVIEWS, encoding="utf-8") as handle:
        data = json.load(handle)
    # La 124 sigue con sus doce y con su vocabulario. Otras tandas pueden
    # añadir las suyas -- la 127 añade las del facsímil de glifos -- y lo
    # que no puede cambiar es lo que la 124 dejó escrito ni el hecho de
    # que NINGUNA revisión, de la tanda que sea, toque el texto.
    old = [r for r in data["reviews"] if r["batch"] == "batch-124"]
    assert len(old) == 12, len(old)
    for entry in data["reviews"]:
        assert entry["structural_effect"] == "none_diagnostic_only"


def test_AD_the_125_impossible_guard_is_still_active():
    section = _section()
    assert section["impossible_marker_audit"]["rejected"] > 0
    assert section["impossible_marker_audit"]["blocks_preserved"] > 0
    assert section["beyond_canonical_top"] == 0


def test_AE_the_125_framed_recovery_is_still_active():
    recovery = _section()["exact_numeric_marker_recovery"]
    assert recovery["accepted"] > 300
    assert recovery["whitelist"] == classifier.SAFE_OUTER_PUNCTUATION
    classes = _embedded()["markers_by_frame_class"]
    assert classes.get("outer_punctuation", 0) > 0, "la ruta de 125 sigue viva"
    assert classes.get("scan_debris", 0) > 0, "y la nueva aporta lo suyo"
    assert sum(classes.values()) == recovery["accepted"]


def test_AF_the_reader_is_deterministic():
    raw = "■ 139 Mi celo me ha hecho consumir"
    first, second = _classify(raw), _classify(raw)
    assert (first.kind, first.number, first.text) == \
        (second.kind, second.number, second.text)


def test_AG_reading_twice_changes_nothing():
    raw = "= 10 Pió te rias con él"
    once = _classify(raw)
    twice = _classify(once.text)
    assert once.number == 10
    assert twice.kind is not BlockKind.VERSE, "el texto ya no lleva marcador"


def test_AH_the_reader_works_offline():
    source = open(os.path.join(DIR, "parser.py"), encoding="utf-8").read()
    for word in ("urllib", "requests", "socket", "pdftoppm"):
        assert word not in source, word


def test_AI_no_page_is_named_in_production_logic():
    pages = {49, 131, 306, 333, 339, 435, 453, 455, 525, 543}
    for name in ("parser.py", "verse_markers.py", "verse_gaps.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            for operand in [node.left] + list(node.comparators):
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, f"{name}: {operand.value}"


def test_AJ_no_verse_number_is_hardcoded():
    numbers = {16, 139, 18, 19, 10}
    for name in ("parser.py", "verse_markers.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if not isinstance(node, ast.Compare):
                continue
            for operand in [node.left] + list(node.comparators):
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in numbers, f"{name}: {operand.value}"
    # y el alfabeto nuevo no lleva ni letras ni cifras
    for char in classifier.SCAN_DEBRIS:
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
    print(f"torresamat1835_embedded_markers_failures={failures}")
    sys.exit(1 if failures else 0)
