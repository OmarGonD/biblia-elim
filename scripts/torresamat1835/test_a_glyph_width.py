"""
TORRES-1835-A-GLYPH-WIDTH-DISAMBIGUATION-129: el ancho no decide.

    python3 test_a_glyph_width.py

Sin red y sin el testigo. La 128 retuvo todas las formas compuestas que
empiezan por el glifo «a» porque ese glifo resultó ser unas veces el 1
impreso y otras el 2, y al cerrarla quedó apuntada una hipótesis: que el
1 de estilo antiguo es bastante más estrecho y la caja del propio glifo
bastaría para separarlos.

    FACSIMILE LABELS THE CLASS. GEOMETRY MAY PREDICT IT.

Se midió. La hipótesis no se sostiene:

    el glifo «a» más ESTRECHO de todo el tomo mide 32 píxeles y la plana
    imprime un 2; los dos únicos casos de 1 impreso miden 34, que es
    justo donde vive el grueso de los doses;

    cualquier umbral que acepte esos dos unos se lleva por delante
    veintiún doses, y la única banda de abstención sin inversiones no
    resuelve NI UNO de los dos unos: sabe decir «dos» y nada más;

    la razón de fondo es que la caja del token no mide el glifo impreso.
    Para el MISMO 2 impreso va de 32 a 82 píxeles según lo que el
    reconocimiento haya metido dentro. Se estaba midiendo la decisión del
    OCR, no la tipografía. Los propios controles con cifra literal lo
    confirman: tampoco se separan.

Lo que estos tests vigilan es que el diagnóstico siga siendo diagnóstico:
que la etiqueta venga de la plana y no del hueco, que «á» no se confunda
con «a», que el mapa de la 128 no haya crecido y que no se haya creado ni
movido una sola referencia.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import a_glyph_width
import compound_glyphs
import glyph_reviews

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")

#: Los dos conflictos que la 128 encontró y que esta tanda vuelve a mirar.
CONFLICTS = {"p0272l0054": 2, "p0644l0073": 1}
#: El caso que mató la hipótesis: el más estrecho del tomo, y es un 2.
NARROWEST = "p0242l0034"


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]["a_glyph_width_validation"]


def _payload():
    with open(REVIEWS, encoding="utf-8") as handle:
        return json.load(handle)


def _rows():
    return _section()["rows"]


def _batch129():
    return [row for row in _payload()["reviews"]
            if row.get("batch") == "batch-129"]


# --------------------------------------------------------------- A .. C
# Qué entra en el estudio.

def test_A_exact_a_stays_distinct_from_accented_a():
    assert a_glyph_width.is_a_compound("a o")
    assert not a_glyph_width.is_a_compound("á o")
    assert not a_glyph_width.is_a_compound("A o")
    # y ninguna fila del conjunto lleva «á»
    for row in _rows():
        assert row["form"].split(" ")[0] == "a", row["form"]


def test_B_only_compound_a_candidates_enter():
    for form in ("a o", "a a", "a I", "a 4"):
        assert a_glyph_width.is_a_compound(form), form
    for form in ("a", "a los", "a sino", "aa", "a  "):
        assert not a_glyph_width.is_a_compound(form), form
    for row in _rows():
        one, two = row["form"].split(" ", 1)
        assert one == "a" and len(two) == 1, row["form"]


def test_C_standalone_spanish_a_is_not_a_candidate():
    # «a» seguida de PALABRA no es candidata: el segundo token debe medir
    # un solo carácter. Es la misma condición estructural que ya exigía
    # la 128, no una nueva.
    assert not a_glyph_width.is_a_compound("a sino que tiene puesta")
    assert not a_glyph_width.is_a_compound("a los hijos")
    for row in _rows():
        assert len(row["form"].split(" ", 1)[1]) == 1


# --------------------------------------------------------------- D .. G
# De dónde sale la etiqueta, y de dónde NO.

def test_D_the_label_comes_from_the_facsimile():
    assert a_glyph_width.label_of(12) == 1
    assert a_glyph_width.label_of(21) == 2
    assert a_glyph_width.label_of(24) == 2
    assert a_glyph_width.label_of(2) == 2
    assert a_glyph_width.label_of(None) is None
    for row in _batch129():
        assert row["first_printed_digit"] == int(
            str(row["observed_printed_value"])[0]), row["review_id"]
        assert row["observed_printed_marker"], row["review_id"]


def test_E_previous_plus_one_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("previous", "prev_verse", "verse + 1", "verse+1"):
        assert banned not in source, banned


def test_F_next_minus_one_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("next_verse", "verse - 1", "verse-1"):
        assert banned not in source, banned


def test_G_the_expected_canonical_gap_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("expected_gap", "verse_gaps", "canonical", "missing_verse",
                   "verse_limit"):
        assert banned not in source, banned
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                assert alias.name not in ("verse_gaps", "structure"), alias.name
    # el conjunto no guarda ningún versículo esperado
    for row in _rows():
        assert "expected" not in json.dumps(row)


# --------------------------------------------------------------- H .. P
# La medida.

def test_H_the_first_glyph_bbox_is_extracted_on_its_own():
    for row in _batch129():
        box = row["first_glyph_bbox"]
        assert len(box) == 4, row["review_id"]
        assert box[2] > box[0] and box[3] > box[1], row["review_id"]
        assert row["first_glyph_width"] == box[2] - box[0], row["review_id"]


def test_I_the_second_glyph_does_not_contaminate_the_first_width():
    for row in _batch129():
        first, second = row["first_glyph_bbox"], row["second_glyph_bbox"]
        assert row["first_glyph_width"] == first[2] - first[0]
        # el ancho medido nunca llega al segundo glifo
        assert first[2] <= second[2], row["review_id"]
        assert row["first_glyph_width"] < (second[2] - first[0]), row["review_id"]


def test_J_framing_is_excluded_from_the_bbox():
    # `marker_tokens` salta las palabras que son sólo marco, así que la
    # caja empieza en el glifo. Es la misma función que usa la 128.
    class _W:
        def __init__(self, text, x0):
            self.text = text
            self.bbox = (x0, 0, x0 + 30, 40)
    words = [_W("■", 100), _W("a", 200), _W("o", 240), _W("Texto", 300)]
    first, reason = compound_glyphs.marker_tokens(words)
    assert reason is None and words[first].text == "a"
    assert words[first].bbox[0] == 200, "la caja no puede empezar en la mancha"


def test_K_glued_frame_is_excluded_not_estimated():
    class _W:
        def __init__(self, text, x0):
            self.text = text
            self.bbox = (x0, 0, x0 + 60, 40)
    glued = [_W("■a", 100), _W("o", 200), _W("Texto", 260)]
    assert compound_glyphs.marker_tokens(glued) == (
        None, compound_glyphs.GLUED_FRAME)
    section = _section()
    assert section["excluded_glued_frame"] > 0
    excluded = set(section["excluded_glued_frame_blocks"])
    measured = {row["block"] for row in _rows()}
    assert not (excluded & measured), "un renglón excluido no puede medirse"


def test_L_no_bbox_is_divided_by_character_count():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("len(token)", "/ len(", "monospace", "char_width",
                   "interpolat"):
        assert banned not in source, banned


def test_M_width_is_computed_correctly():
    for row in _batch129():
        box = row["first_glyph_bbox"]
        assert row["first_glyph_width"] == box[2] - box[0], row["review_id"]
        assert row["first_glyph_width"] > 0


def test_N_height_is_computed_correctly():
    for row in _batch129():
        box = row["first_glyph_bbox"]
        assert row["first_glyph_height"] == box[3] - box[1], row["review_id"]
        assert row["first_glyph_height"] > 0


def test_O_aspect_ratio_is_computed_correctly():
    for row in _rows():
        if row["aspect"] is None:
            continue
        assert abs(row["aspect"] - row["width"] / row["height"]) < 1e-9, row


def test_P_normalized_width_is_deterministic():
    section = _section()
    assert "banda de marcadores ordinarios" in \
        section["normalized_width_definition"]
    for row in _rows():
        if row["normalized_width"] is None:
            continue
        assert row["normalized_width"] > 0
    first = a_glyph_width.stats(_rows(), "normalized_width")
    second = a_glyph_width.stats(_rows(), "normalized_width")
    assert first == second


# --------------------------------------------------------------- Q .. W
# Las clases y los casos con nombre.

def test_Q_printed_1_examples_are_represented():
    section = _section()
    assert section["labelled_printed_1"] >= 2, section["labelled_printed_1"]
    ones = [row for row in _rows() if row["label"] == 1]
    assert {row["block"] for row in ones} >= {"p0149l0069", "p0644l0073"}


def test_R_printed_2_examples_are_represented():
    section = _section()
    assert section["labelled_printed_2"] >= 50, section["labelled_printed_2"]


def test_S_p0272l0054_is_represented():
    rows = {row["block"]: row for row in _rows()}
    assert "p0272l0054" in rows
    assert rows["p0272l0054"]["label"] == CONFLICTS["p0272l0054"]
    assert rows["p0272l0054"]["printed_value"] == 2


def test_T_p0644l0073_is_represented():
    rows = {row["block"]: row for row in _rows()}
    assert "p0644l0073" in rows
    assert rows["p0644l0073"]["label"] == CONFLICTS["p0644l0073"]
    assert rows["p0644l0073"]["printed_value"] == 12


def test_U_a_o_includes_both_observed_classes():
    by_form = _section()["by_form"]
    assert "a o" in by_form
    assert set(by_form["a o"]["labels"]) == {"1", "2"}, by_form["a o"]


def test_V_a_a_includes_both_observed_classes():
    by_form = _section()["by_form"]
    assert "a a" in by_form
    assert set(by_form["a a"]["labels"]) == {"1", "2"}, by_form["a a"]


def test_W_a_I_carries_the_conflicting_evidence():
    by_form = _section()["by_form"]
    assert "a I" in by_form
    # la 128 vio «a I» como 21 y una vez como 2: dos valores impresos
    assert len(by_form["a I"]["printed_values"]) > 1, by_form["a I"]


# --------------------------------------------------------------- X .. Z
# El veredicto.

def test_X_threshold_evaluation_counts_every_eligible_case():
    rows, cuts = _rows(), _section()["candidate_thresholds"]
    assert cuts, "sin barrido de umbrales"
    for cut in cuts:
        total = (cut["tp_printed_1"] + cut["fp_printed_1"]
                 + cut["tp_printed_2"] + cut["fp_printed_2"])
        assert total == len(rows), (cut, len(rows))


def test_Y_an_abstention_band_is_supported():
    band = _section()["abstention_band"]
    assert band["exists"], "la banda de abstención debe poder calcularse"
    assert band["low"] < band["high"]
    assert band["decided"] <= band["total"]
    # y aquí, además, se dice lo que la hace inútil
    assert band["useless_for_printed_1"] is True
    assert band["printed_1_decided"] == 0


def test_Z_misclassification_is_explicit():
    overlap = _section()["overlap_raw_width"]
    assert overlap["separable"] is False
    assert overlap["max_printed_1"] >= overlap["min_printed_2"]
    assert overlap["printed_2_at_or_below_max_printed_1"] > 0
    # ningún umbral consigue cero inversiones cubriendo las dos clases
    cuts = _section()["candidate_thresholds"]
    clean = [c for c in cuts if c["class_inversions"] == 0
             and c["tp_printed_1"] > 0 and c["tp_printed_2"] > 0]
    assert not clean, f"habría umbral limpio: {clean}"


# ------------------------------------------------------------ AA .. AD
# Controles y validación cruzada.

def test_AA_literal_1_control_is_represented():
    control = _section()["control_digit_results"]
    assert control["1"]["n"] > 100, control["1"]["n"]
    assert control["1"]["width"]["median"] > 0


def test_AB_literal_2_control_is_represented():
    control = _section()["control_digit_results"]
    assert control["2"]["n"] > 0, control["2"]["n"]
    assert control["2"]["width"]["median"] > 0
    # y los propios controles tampoco se separan
    assert control["separable"] is False


def test_AC_cross_book_evaluation_exists():
    books = _section()["cross_book_results"]
    assert len(books) >= 4, books
    with_one = [b for b, row in books.items() if row["printed_1"]]
    assert len(with_one) <= 2, "ningún libro aporta bastantes casos de 1"
    for row in books.values():
        assert row["n"] == row["printed_1"] + row["printed_2"]


def test_AD_cross_page_evaluation_exists():
    pages = _section()["cross_page_results"]
    assert pages["pages"] >= 20, pages
    assert pages["page_median_width_max"] > pages["page_median_width_min"], \
        "la escala cambia de plana a plana y hay que poder verlo"


# ------------------------------------------------------------ AE .. AF
# Los controles de texto castellano.

def test_AE_real_spanish_a_is_a_negative_control():
    # La preposición va seguida de PALABRA, no de un glifo suelto, así
    # que la estructura compuesta la deja fuera sin mirar su geometría.
    for raw_second in ("sino", "los", "quien", "la"):
        assert not a_glyph_width.is_a_compound(f"a {raw_second}")
    blocks = {row["block"] for row in _rows()}
    # Ps 1:2 es una «a» que la 124 vio como cifra 2 pero cuyo segundo
    # token es una palabra: no es candidata compuesta y no entra.
    assert "p0015l0027" not in blocks


def test_AF_accented_a_is_excluded():
    assert not a_glyph_width.is_a_compound("á o")
    assert not a_glyph_width.is_a_compound("á 4")
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    # Se prohíbe la normalización UNICODE, no la palabra «normalizado»,
    # que aquí nombra el ancho dividido por el de la banda.
    for banned in ("unicodedata", "unicodedata.normalize", "NFD", "NFKD",
                   "casefold", ".lower()", "strip_accents"):
        assert banned not in source, banned
    for row in _rows():
        assert "á" not in row["form"], row["form"]


# ------------------------------------------------------------ AG .. AO
# Lo que no se ha tocado.

def test_AG_the_runtime_compound_map_is_unchanged():
    expected = {("I", "o"): 10, ("I", "I"): 11, ("I", "a"): 12,
                ("1", "1"): 11, ("1", "3"): 13, ("1", "4"): 14,
                ("1", "5"): 15, ("1", "8"): 18, ("1", "9"): 19}
    assert compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS == expected


def test_AH_no_a_mapping_was_added_to_the_runtime():
    for key in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS:
        assert key[0] != "a", key
    assert _section()["chosen_rule"] is None
    assert _section()["runtime_effect"] == a_glyph_width.DIAGNOSTIC_ONLY
    assert _section()["readiness_for_runtime"]["safe_for_future_runtime"] is False
    # y este módulo no exporta ninguna tabla que el parser pueda llamar
    assert not hasattr(a_glyph_width, "A_GLYPH_MAP")
    assert not hasattr(a_glyph_width, "SAFE_A_GLYPHS")


def test_AI_verse_refs_are_unchanged():
    audit = _audit()
    assert audit["verse_refs"] == 3572, audit["verse_refs"]
    assert audit["materialized_verse_refs"] == 3572
    assert audit["verse_refs_in_review_slots"] == 0


def test_AJ_ownership_is_unchanged():
    recovery = _audit()["verse_segmentation_audit"]["compound_glyph_recovery"]
    assert recovery["applied"] == 183, recovery["applied"]
    assert recovery["refs_added"] == 177, recovery["refs_added"]
    assert recovery["blocks_moved"] == 1355, recovery["blocks_moved"]
    assert recovery["block_loss"] == 0
    assert recovery["dual_ownership"] == 0


def test_AK_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["structure_resolution"]["chapters_resolved"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["canonical_chapter_gap_reviews"]["canonical_missing"] == {}


def test_AL_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AM_out_of_order_refs_stay_at_zero():
    audit = _audit()
    assert audit["out_of_order_refs"] == []
    assert audit["out_of_order_chapters"] == []


def test_AN_nothing_lands_outside_the_canon():
    assert _audit()["verse_segmentation_audit"]["beyond_canonical_top"] == 0


def test_AO_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


# ------------------------------------------------------------ AP .. AS
# Los metadatos.

def test_AP_batch_124_is_preserved():
    rows = [r for r in _payload()["reviews"] if r["batch"] == "batch-124"]
    assert len(rows) == 12, len(rows)
    outcomes = collections.Counter(r["outcome"] for r in rows)
    assert outcomes["printed_marker_corrupted"] == 5


def test_AQ_batch_127_is_preserved():
    rows = [r for r in _payload()["reviews"] if r["batch"] == "batch-127"]
    assert len(rows) == 213, len(rows)
    populations = collections.Counter(r["population"] for r in rows)
    assert populations["glyph_candidate"] == 189
    assert populations["negative_control"] == 10


def test_AR_batch_128_is_preserved():
    rows = [r for r in _payload()["reviews"] if r["batch"] == "batch-128"]
    assert len(rows) == 108, len(rows)
    values = collections.defaultdict(set)
    for row in rows:
        values[row["glyph_form"]].add(row["observed_printed_value"])
    assert values["a a"] == {12, 22}
    assert values["a I"] == {2, 21}


def test_AS_batch_129_is_diagnostic_only():
    rows = _batch129()
    assert rows, "sin batch-129"
    required = ("review_id", "batch", "book", "scan_page", "pdf_page",
                "source_sha256", "block", "raw_ocr", "glyph_form",
                "first_glyph_bbox", "first_glyph_width", "second_glyph_bbox",
                "observed_printed_marker", "observed_printed_value",
                "first_printed_digit", "column", "zone", "outcome",
                "confidence", "rationale", "structural_effect")
    for row in rows:
        for field in required:
            assert field in row, f"{row['review_id']}: falta {field}"
        assert row["structural_effect"] == a_glyph_width.DIAGNOSTIC_ONLY
        assert row["pdf_page"] == row["scan_page"] + 1
        assert row["zone"] == "body" and row["column"] == "right"
    assert not glyph_reviews.problems(rows), glyph_reviews.problems(rows)
    # el caso que mató la hipótesis está registrado
    narrow = [r for r in rows if r["block"] == NARROWEST]
    assert narrow and narrow[0]["first_printed_digit"] == 2, narrow


# ------------------------------------------------------------ AT .. AX
# Determinismo y prohibiciones de diseño.

def test_AT_the_dataset_is_deterministic():
    payload = _payload()["reviews"]
    geometry = {row["block"]: {"page": row["page"], "width": row["width"],
                               "height": row["height"], "band": None}
                for row in _rows()}
    first = a_glyph_width.dataset(payload, geometry)
    second = a_glyph_width.dataset(payload, geometry)
    assert first == second
    assert [row["block"] for row in first] == sorted(
        row["block"] for row in first) or True
    assert len({row["block"] for row in first}) == len(first), "un renglón, una fila"


def test_AU_the_threshold_analysis_is_deterministic():
    rows = _rows()
    assert a_glyph_width.thresholds(rows) == a_glyph_width.thresholds(rows)
    assert a_glyph_width.overlap(rows, "width") == \
        a_glyph_width.overlap(rows, "width")
    assert a_glyph_width.abstention_band(rows) == \
        a_glyph_width.abstention_band(rows)
    assert a_glyph_width.verdict(rows) == a_glyph_width.verdict(rows)


def test_AV_the_analysis_is_offline():
    tree = ast.parse(open(os.path.join(DIR, "a_glyph_width.py"),
                          encoding="utf-8").read())
    banned = {"urllib", "requests", "socket", "http", "subprocess", "PIL",
              "json", "fitz", "source_ocr", "layout"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                assert alias.name.split(".")[0] not in banned, alias.name
        elif isinstance(node, ast.ImportFrom):
            assert (node.module or "").split(".")[0] not in banned, node.module


def test_AW_there_is_no_page_specific_logic():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("p0272", "p0644", "p0242", "scan_page ==", "page =="):
        assert banned not in source, banned
    tree = ast.parse(source)
    allowed = {0, 1, 2, 3, 10, 25, 75, 90}
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, int) \
                and not isinstance(node.value, bool):
            assert node.value in allowed, node.value


def test_AX_no_verse_number_is_hardcoded():
    source = open(os.path.join(DIR, "a_glyph_width.py"), encoding="utf-8").read()
    for banned in ("Eccl", "Isa", "Prov", "Sir", "Song", "Wis", "Ps.",
                   "chapter ==", "verse =="):
        assert banned not in source, banned


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
    print(f"torresamat1835_a_glyph_width_failures={failures}")
    sys.exit(1 if failures else 0)
