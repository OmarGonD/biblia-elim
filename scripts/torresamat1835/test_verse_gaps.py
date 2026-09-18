"""
TORRES-1835-VERSE-SEGMENTATION-AUDIT-124: huecos de versículo, medidos.

    python3 test_verse_gaps.py

Sin red y sin el testigo. Cerrada la estructura de capítulos, lo que
queda mal es la numeración de versículos. Esta batería fija el
diagnóstico, y lo primero que fija es cómo se cuenta, porque de ahí
salía la mitad del problema:

    expected = 1 .. max(verso materializado)

Esa cuenta se mira a sí misma. Un número de página leído como verso
--«196» en Sabiduría 11, que tiene 19 versículos-- fabrica ciento
noventa ausencias que no existen en ninguna plana. Aquí el dominio se
compara además con la versificación NATIVA de esta edición, y lo que cae
más allá del último verso canónico se llama por su nombre.

    VERSE GAP IS A QUESTION, NOT AN ANSWER

Lo demás que se comprueba es que nada de esto crea versículos: ni el
canon, ni la secuencia, ni el verso de al lado, ni un indicio local. Los
indicios --un glifo suelto al empezar un renglón-- sirven para ofrecer,
y el tomo demuestra por qué no pueden servir para leer: «a» es la cifra
2 en la fundición de este impreso, pero «y» y «á» son palabras
castellanas que empiezan renglones de verdad.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import source_ocr
import structure
import verse_gaps

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")


class _Verse:
    def __init__(self, blocks):
        self.blocks = blocks


class _Block:
    def __init__(self, text, block_id, page=10, column="right", bbox=None):
        self.text = text
        self.provenance = type("P", (), {
            "block_id": block_id, "page": page, "column": column,
            "bbox": bbox or (1700, 100, 3000, 180)})()


class _Chapter:
    def __init__(self, verses, paratext=()):
        self.verses = verses
        self.paratext = list(paratext)


class _Book:
    def __init__(self, chapters):
        self.chapters = chapters


class _Edition:
    def __init__(self, books):
        self.books = books


def _chapter(verse_map, paratext=()):
    verses = {}
    for number, lines in verse_map.items():
        verses[number] = _Verse([
            _Block(text, f"p0010l{index:04d}")
            for index, text in enumerate(lines, start=number * 10)])
    return _Chapter(verses, paratext)


def _edition(book, number, verse_map, paratext=()):
    return _Edition({book: _Book({number: _chapter(verse_map, paratext)})})


def _limit(count):
    return lambda osis, chapter: count


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _reviews():
    with open(REVIEWS, encoding="utf-8") as handle:
        return json.load(handle)


# ======================================================================
# A-E. La forma del hueco y de dónde sale el dominio
# ======================================================================
def test_A_an_interior_gap_is_detected():
    edition = _edition("Ps", 1, {1: ["uno"], 3: ["tres"]})
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(3))
    assert [(g.verse, g.shape) for g in gaps] == [(2, verse_gaps.INTERIOR)]
    assert gaps[0].previous_verse == 1 and gaps[0].next_verse == 3


def test_B_a_leading_gap_is_detected():
    edition = _edition("Ps", 14, {3: ["tres"], 4: ["cuatro"]})
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(4))
    assert [(g.verse, g.shape) for g in gaps] == [
        (1, verse_gaps.LEADING), (2, verse_gaps.LEADING)]


def test_C_a_trailing_gap_is_detected():
    # Lo que la cuenta antigua no podía ver: el capítulo se queda corto.
    edition = _edition("Wis", 1, {1: ["uno"], 2: ["dos"]})
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(5))
    assert [(g.verse, g.shape) for g in gaps] == [
        (3, verse_gaps.TRAILING), (4, verse_gaps.TRAILING),
        (5, verse_gaps.TRAILING)]


def test_D_a_number_beyond_the_canon_is_not_a_missing_verse():
    # «196» en un capítulo de 19: el hueco no está en las planas, está
    # en el numeral espurio, y se cuenta aparte.
    edition = _edition("Wis", 11, {1: ["uno"], 17: ["diecisiete"], 196: ["espurio"]})
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(19))
    shapes = collections.Counter(g.shape for g in gaps)
    # de 20 a 196 menos el propio 196, que sí está materializado
    assert shapes[verse_gaps.OVER_CANON] == (196 - 19) - 1
    assert all(g.verse > 19 for g in gaps if g.shape == verse_gaps.OVER_CANON)
    assert any(verse_gaps.SPURIOUS_TOP in g.signals for g in gaps)
    out = verse_gaps.summary(gaps)
    assert out["beyond_canonical_top"] == 176
    assert out["physical_domain"] == out["total"] - 176


def test_E_the_expected_domain_comes_from_the_native_versification():
    # La Vulgata de SWORD, que es la versificación de esta edición.
    assert structure.verse_limit("Ps", 1) == 6
    assert structure.verse_limit("Wis", 1) == 16
    assert structure.verse_limit("Isa", 26) == 21
    audit = _audit()
    assert "native Vulgate versification" in \
        audit["verse_segmentation_audit"]["expected_count_source"]


# ======================================================================
# F-H. Nada de esto crea un versículo
# ======================================================================
def test_F_the_expected_count_creates_no_verse():
    edition = _edition("Ps", 1, {1: ["uno"], 3: ["tres"]})
    before = dict(edition.books["Ps"].chapters[1].verses)
    verse_gaps.inventory(edition, verse_limit=_limit(6))
    assert edition.books["Ps"].chapters[1].verses == before
    assert 2 not in edition.books["Ps"].chapters[1].verses


def test_G_sequence_creates_no_verse():
    edition = _edition("Ps", 1, {1: ["uno"], 3: ["tres"]})
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(6))
    gap = next(g for g in gaps if g.verse == 2)
    # el módulo describe, no resuelve: no hay campo de número recuperado
    assert not hasattr(gap, "resolved_verse")
    assert "verse" in gap.as_dict() and "recovered" not in gap.as_dict()


def test_H_canon_creates_no_verse():
    source = open(os.path.join(DIR, "verse_gaps.py"), encoding="utf-8").read()
    tree = ast.parse(source)
    imported = {node.names[0].name for node in ast.walk(tree)
                if isinstance(node, ast.Import)}
    assert imported <= {"collections", "hashlib", "re"}, imported
    assert "canon" not in imported


# ======================================================================
# I-N. Las clases que el facsímil distinguió
# ======================================================================
def test_I_a_printed_marker_missing_from_ocr_is_classifiable():
    outcomes = {r["outcome"] for r in _reviews()["reviews"]}
    assert "printed_marker_missing_from_ocr" in outcomes
    entry = next(r for r in _reviews()["reviews"]
                 if r["outcome"] == "printed_marker_missing_from_ocr")
    assert entry["marker_case"] == "text_present_without_marker"
    assert entry["observed_printed_marker"] is None
    assert entry["structural_effect"] == "none_diagnostic_only"


def test_J_a_corrupted_printed_marker_is_classifiable():
    entries = [r for r in _reviews()["reviews"]
               if r["outcome"] == "printed_marker_corrupted"]
    assert len(entries) >= 4, "el patrón se vio en varios libros"
    read = {(r["book"], r["chapter"], r["verse"]): r["observed_printed_marker"]
            for r in entries}
    # la cifra 2 de estilo antiguo leída como «a», en dos libros
    assert read[("Ps", 1, 2)] == "2"
    assert read[("Eccl", 1, 2)] == "2"
    for entry in entries:
        assert entry["observed_printed_marker"] is not None
        assert entry["raw_ocr"]
        assert entry["observed_printed_marker"] not in entry["raw_ocr"][:3]


def test_K_a_marker_attached_to_text_is_classifiable():
    # El número pegado a la palabra, sin el espacio del impreso.
    edition = _edition("Ps", 1, {1: ["uno", "2sino que tiene puesta"], 3: ["tres"]})
    gap = next(g for g in verse_gaps.inventory(
        edition, verse_limit=_limit(6)) if g.verse == 2)
    assert verse_gaps.ATTACHED_MARKER in gap.signals
    assert verse_gaps.SWALLOWED_DIGIT in gap.signals
    assert gap.swallowed_token == "2"
    assert gap.swallowed_text.startswith("2sino")


def test_L_a_marker_in_a_separate_block_is_classifiable():
    entry = next(r for r in _reviews()["reviews"]
                 if r["outcome"] == "marker_present_parser_missed")
    assert entry["observed_printed_marker"] == "63"
    assert entry["observed_printed_marker"] in entry["raw_ocr"]
    assert entry["marker_case"] == "inside_existing_ocr_block"


def test_M_two_merged_verses_are_representable():
    # Los dos versos del impreso comparten unidad: el hueco lo dice con
    # el bloque en el que se quedó el marcador.
    edition = _edition("Ps", 1, {1: ["uno", "a sino que tiene puesta"], 3: ["tres"]})
    gap = next(g for g in verse_gaps.inventory(
        edition, verse_limit=_limit(6)) if g.verse == 2)
    assert verse_gaps.SWALLOWED_GLYPH in gap.signals
    assert gap.swallowed_block == "p0010l0011"
    assert gap.swallowed_text.startswith("a sino")
    assert gap.previous_verse == 1 and gap.next_verse == 3


def test_N_a_split_verse_is_representable():
    edition = _edition("Ps", 1, {1: ["uno", "sigue", "y sigue"], 3: ["tres"]})
    gap = next(g for g in verse_gaps.inventory(
        edition, verse_limit=_limit(6)) if g.verse == 2)
    assert verse_gaps.BLOCK_SPLIT in gap.signals
    assert gap.previous_block == "p0010l0012", "el último bloque del verso"


# ======================================================================
# O-R. Lo que el diagnóstico no puede hacer
# ======================================================================
def test_O_a_numbering_difference_corrects_no_reference():
    audit = _audit()
    section = audit["verse_segmentation_audit"]
    # el tomo tiene capítulos cuyo último materializado excede el canon
    spurious = [g for g in section["gaps"]
                if verse_gaps.SPURIOUS_TOP in g["signals"]]
    assert spurious, "el caso existe en el tomo"
    # y aun así no se ha tocado ninguna referencia
    assert audit["duplicate_refs"] == []
    assert audit["verse_refs"] == audit["materialized_verse_refs"]


def test_P_an_absent_marker_creates_nothing():
    for entry in _reviews()["reviews"]:
        assert entry["structural_effect"] == "none_diagnostic_only", entry["review_id"]
    assert _audit()["verse_segmentation_audit"]["manual_review_pending"] == 0


def test_Q_latin_evidence_is_support_only():
    for entry in _reviews()["reviews"]:
        if not entry.get("latin_support"):
            continue
        # el latín acompaña; el número sale del marcador castellano
        assert entry["column"] == "right"
        assert entry["outcome"] != "Latin_only_marker" or \
            entry["structural_effect"] == "none_diagnostic_only"


def test_R_the_raw_ocr_is_never_rewritten():
    audit = _audit()
    assert audit["metrics"]["ocr_blocks"] == 57700
    for entry in _reviews()["reviews"]:
        assert "raw_ocr" in entry
        if entry["observed_printed_marker"] and entry["raw_ocr"]:
            # las dos lecturas se conservan, y son distintas
            assert entry["raw_ocr"] != entry["observed_printed_text_context"]


def test_S_a_multi_block_verse_keeps_its_context():
    edition = _edition("Ps", 1, {1: ["primero", "segundo", "tercero"], 3: ["tres"]})
    gap = next(g for g in verse_gaps.inventory(
        edition, verse_limit=_limit(6)) if g.verse == 2)
    assert gap.previous_block and gap.next_block
    assert gap.previous_block != gap.next_block
    assert gap.previous_tail and gap.next_head


# ======================================================================
# T-V. Determinismo y semántica de las cuentas
# ======================================================================
def test_T_the_inventory_is_deterministic():
    def build():
        return _edition("Ps", 1, {1: ["uno", "a sino"], 3: ["tres"], 6: ["seis"]})
    first = [g.as_dict() for g in verse_gaps.inventory(
        build(), verse_limit=_limit(6))]
    second = [g.as_dict() for g in verse_gaps.inventory(
        build(), verse_limit=_limit(6))]
    assert first == second
    keys = [g["key"] for g in first]
    assert keys == sorted(keys, key=lambda k: (k.split(".")[0],
                                               int(k.split(".")[1]),
                                               int(k.split(".")[2])))


def test_U_the_sample_is_deterministic_and_stratified():
    edition = _Edition({
        "Ps": _Book({n: _chapter({1: ["uno"], 4: ["cuatro"]}) for n in range(1, 9)}),
        "Sir": _Book({n: _chapter({2: ["dos"], 5: ["cinco"]}) for n in range(1, 9)}),
    })
    gaps = verse_gaps.inventory(edition, verse_limit=_limit(5))
    first = [g.key for g in verse_gaps.sample(gaps, size=8)]
    second = [g.key for g in verse_gaps.sample(gaps, size=8)]
    assert first == second, "la misma entrada da la misma muestra"
    assert len(set(first)) == 8
    books = {key.split(".")[0] for key in first}
    assert books == {"Ps", "Sir"}, "reparte entre estratos"
    other = [g.key for g in verse_gaps.sample(gaps, size=8, seed="otra")]
    assert other != first, "la semilla cambia la muestra, no el orden"
    assert len(verse_gaps.strata(gaps)) >= 2


def test_V_detected_is_not_reviewed_and_not_pending():
    section = _audit()["verse_segmentation_audit"]
    assert section["total"] > section["reviewed"] > 0
    assert section["manual_review_pending"] == 0
    assert "counted_apart" in section
    assert len(section["reviewed_detail"]) == section["reviewed"]
    assert sum(section["reviewed_by_outcome"].values()) == section["reviewed"]
    assert section["sample_size"] == len(section["sample"])


# ======================================================================
# W-AC. El tomo no se ha movido
# ======================================================================
def test_W_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["accepted"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["canonical_chapter_gap_reviews"]["pending_review"] == 0
    assert audit["numeral_image_review"]["review_queue_next"] == []
    assert audit["image_review_queue"]["total"] == 0


def test_X_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_Y_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_Z_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AA_the_inventory_is_offline():
    source = open(os.path.join(DIR, "verse_gaps.py"), encoding="utf-8").read()
    for word in ("urllib", "requests", "socket", "open(", "pdftoppm"):
        assert word not in source, f"verse_gaps.py usa {word!r}"
    assert _reviews()["visual_source"]["sha256"] == (
        "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346")


def test_AB_no_page_is_named_in_the_inventory():
    tree = ast.parse(open(os.path.join(DIR, "verse_gaps.py"),
                          encoding="utf-8").read())
    pages = {15, 105, 116, 276, 279, 281, 283, 319, 320, 340, 428, 534, 552}
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, int) \
                and not isinstance(node.value, bool):
            assert node.value not in pages, f"decide por la plana {node.value}"


def test_AC_no_verse_number_is_hardcoded_in_the_inventory():
    tree = ast.parse(open(os.path.join(DIR, "verse_gaps.py"),
                          encoding="utf-8").read())
    for node in ast.walk(tree):
        if not isinstance(node, ast.Compare):
            continue
        for operand in [node.left] + list(node.comparators):
            if isinstance(operand, ast.Constant) and \
                    isinstance(operand.value, int) and \
                    not isinstance(operand.value, bool):
                assert operand.value in (0, 1, 2, 3), \
                    f"comparación contra {operand.value}"


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
    print(f"torresamat1835_verse_gaps_failures={failures}")
    sys.exit(1 if failures else 0)
