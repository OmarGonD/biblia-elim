"""
TORRES-1835-A-GLYPH-PIXEL-SHAPE-130: la tinta tampoco decide.

    python3 test_a_glyph_pixel_shape.py

Sin red. La 129 quedó bloqueada midiendo la caja que el reconocimiento
asigna al token «a»: esa caja describe cómo segmentó el OCR, no lo que
hay impreso -- el mismo 2 impreso iba de 32 a 82 píxeles. Esta tanda fue
a la plana y midió la mancha de tinta.

    FACSIMILE LABELS THE CLASS. FACSIMILE PIXELS PROVIDE THE FEATURES.

Todo lo que hay es aritmética sobre una máscara de blanco y negro:
contar píxeles, encontrar componentes conexos, sacar el centro de masas.
Ni aprendizaje automático ni nada opaco, porque la pregunta era si existe
una propiedad FÍSICA SIMPLE, no si algo la adivina.

Lo que estos tests vigilan es que el diagnóstico siga siendo diagnóstico:
que la etiqueta venga de la plana y no del hueco ni del segundo glifo,
que la caja del OCR se use sólo para localizar, que el preprocesado no
mire la etiqueta, que «á» siga fuera, y que no se haya creado ni movido
una sola referencia.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import a_glyph_pixels as pixels
import compound_glyphs

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")
SHA = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"

#: Los casos con nombre que esta familia arrastra desde la 128.
PRINTED_1_CASES = ("p0644l0073", "p0149l0069")
NARROW_PRINTED_2 = ("p0242l0034", "p0424l0070", "p0371l0064")


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]["a_glyph_pixel_shape_validation"]


def _payload():
    with open(REVIEWS, encoding="utf-8") as handle:
        return json.load(handle)


def _rows():
    return _section()["rows"]


def _batch130():
    return [row for row in _payload()["reviews"]
            if row.get("batch") == "batch-130"]


def _grid(pattern):
    """Una imagen en gris a partir de un dibujo con «#» por tinta."""
    return [[0 if char == "#" else 255 for char in line] for line in pattern]


# --------------------------------------------------------------- A .. F
# La fuente y el recorte.

def test_A_the_source_pdf_sha_is_verified():
    section = _section()
    assert section["facsimile"]["sha256"] == SHA, section["facsimile"]
    assert section["facsimile"]["verified"] is True
    for row in _batch130():
        assert row["source_sha256"] == SHA, row["review_id"]


def test_B_scan_page_maps_to_pdf_page():
    assert _section()["facsimile"]["page_mapping"] == "pdf_page = scan_page + 1"
    for row in _batch130():
        assert row["pdf_page"] == row["scan_page"] + 1, row["review_id"]
    for row in _rows():
        assert row["pdf_page"] == row["scan_page"] + 1, row["block"]


def test_C_the_ocr_bbox_is_only_a_localization_anchor():
    # El recorte se ancla en la caja del OCR; lo que se mide después es la
    # tinta. `select_component` usa el ancla para elegir la mancha y no
    # mira ni su ancho ni su alto.
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    start = source.index("def select_component")
    body = source[start:source.index("\ndef ", start + 10)]
    for banned in ("anchor[2] - anchor[0]", "anchor_width", "width =",
                   "ocr_width"):
        assert banned not in body, banned


def test_D_the_ocr_bbox_width_is_not_a_pixel_feature():
    assert "ocr_bbox_width" not in pixels.SCALAR_FEATURES
    for row in _rows():
        assert "ocr_bbox_width" not in row["features"], row["block"]
        # se conserva para poder comparar con la 129, pero fuera de la medida
        assert set(row["features"]) & set(pixels.SCALAR_FEATURES)


def test_E_crop_coordinates_are_deterministic():
    first, second = (100, 50, 140, 90), (160, 50, 200, 90)
    once = pixels.crop_box(first, second)
    twice = pixels.crop_box(first, second)
    assert once == twice
    assert once[0] == first[0] - pixels.CROP_PAD
    # y el borde derecho se queda a medio camino del segundo token
    assert once[2] <= (first[2] + second[0]) // 2


def test_F_the_crop_preserves_physical_scale():
    # `scale_box` sólo cambia de sistema de coordenadas: no reescala de
    # forma anisótropa, que destruiría justamente ancho y proporción.
    box = (100, 50, 140, 90)
    scaled = pixels.scale_box(box, 2.0, 2.0, (10000, 10000))
    assert scaled == (200, 100, 280, 180)
    ratio_before = (box[2] - box[0]) / (box[3] - box[1])
    ratio_after = (scaled[2] - scaled[0]) / (scaled[3] - scaled[1])
    assert abs(ratio_before - ratio_after) < 1e-9
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("resize", "thumbnail", "ANTIALIAS", "LANCZOS"):
        assert banned not in source, banned


# --------------------------------------------------------------- G .. L
# Aislamiento y etiqueta.

def test_G_the_first_digit_is_isolated_from_the_second():
    first, second = (100, 50, 140, 90), (150, 50, 190, 90)
    box = pixels.crop_box(first, second)
    assert box[2] <= second[0], "la tinta del segundo glifo no puede entrar"


def test_H_the_expected_gap_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("expected_gap", "verse_gaps", "canonical", "missing_verse",
                   "verse_limit"):
        assert banned not in source, banned


def test_I_the_previous_ref_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("previous", "prev_verse", "verse + 1"):
        assert banned not in source, banned


def test_J_the_next_ref_is_not_used():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("next_verse", "verse - 1"):
        assert banned not in source, banned


def test_K_the_second_ocr_token_is_not_a_class_feature():
    # La posición del segundo token sirve para cerrar el recorte -- eso es
    # aislamiento físico -- pero su IDENTIDAD no entra en ninguna medida.
    assert "second" not in " ".join(pixels.SCALAR_FEATURES)
    for row in _rows():
        assert "second_token" not in row["features"]
        assert "form" in row, "la forma se informa, pero no se mide"


def test_L_the_label_comes_from_the_facsimile_review():
    for row in _rows():
        assert row["label"] in (1, 2), row
        assert row["label"] == int(str(row["printed_value"])[0]), row
    for row in _batch130():
        assert row["first_printed_digit"] == int(
            str(row["observed_printed_value"])[0]), row["review_id"]


def test_M_printed_1_is_represented():
    ones = [row for row in _rows() if row["label"] == 1]
    assert ones, "sin ningún 1 impreso medido"
    assert {row["block"] for row in ones} >= set(PRINTED_1_CASES)


def test_N_printed_2_is_represented():
    twos = [row for row in _rows() if row["label"] == 2]
    assert len(twos) >= 50, len(twos)


# --------------------------------------------------------------- O .. Z
# El preprocesado, medido sobre figuras conocidas.

def test_O_the_raw_crop_is_not_touched_before_preprocessing():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("ImageFilter", "GaussianBlur", "dilate", "erode",
                   "MedianFilter", "sharpen", "autocontrast", "equalize"):
        assert banned not in source, banned


def test_P_binarization_is_deterministic():
    histogram = [0] * 256
    for level in (10, 10, 10, 200, 200, 200):
        histogram[level] += 1
    assert pixels.otsu_threshold(histogram) == pixels.otsu_threshold(histogram)
    grid = _grid(["..#..", "..#.."])
    assert pixels.binarize(grid, 128) == pixels.binarize(grid, 128)
    assert pixels.binarize(grid, 128)[0] == [0, 0, 1, 0, 0]


def test_Q_the_ink_bbox_is_deterministic():
    grid = _grid(["......",
                  ".#....",
                  ".#....",
                  ".###..",
                  "......"])
    mask = pixels.binarize(grid, 128)
    once = pixels.components(mask, 0)[0]["bbox"]
    twice = pixels.components(mask, 0)[0]["bbox"]
    assert once == twice == (1, 1, 4, 4)


def test_R_ink_width_is_correct_on_a_fixture():
    grid = _grid(["......",
                  ".#....",
                  ".#....",
                  ".###..",
                  "......"])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 6))
    assert got["ink_bbox_width"] == 3, got


def test_S_ink_height_is_correct():
    grid = _grid(["......",
                  ".#....",
                  ".#....",
                  ".###..",
                  "......"])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 6))
    assert got["ink_bbox_height"] == 3, got
    assert abs(got["ink_bbox_aspect_ratio"] - 1.0) < 1e-9


def test_T_density_is_correct():
    grid = _grid(["......",
                  ".#....",
                  ".#....",
                  ".###..",
                  "......"])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 6))
    assert got["ink_pixels"] == 5, got
    assert abs(got["ink_density"] - 5 / 9) < 1e-9, got


def test_U_the_centroid_is_correct():
    # Una barra vertical en el borde izquierdo: el centro de masas cae a
    # la izquierda de su propia caja.
    grid = _grid(["....",
                  ".#..",
                  ".#..",
                  ".##.",
                  "...."])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 4))
    assert 0.0 <= got["centroid_x"] <= 1.0 and 0.0 <= got["centroid_y"] <= 1.0
    assert got["centroid_x"] < 0.5, got


def test_V_left_and_right_mass_ratios_are_correct():
    grid = _grid(["....",
                  ".#..",
                  ".#..",
                  ".##.",
                  "...."])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 4))
    assert abs(got["left_mass_ratio"] + got["right_mass_ratio"] - 1.0) < 1e-9
    assert got["left_mass_ratio"] > got["right_mass_ratio"], got


def test_W_top_and_bottom_mass_ratios_are_correct():
    grid = _grid(["....",
                  ".#..",
                  ".#..",
                  ".##.",
                  "...."])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    got = pixels.features(comps[0], comps, (5, 4))
    assert abs(got["top_mass_ratio"] + got["bottom_mass_ratio"] - 1.0) < 1e-9


def test_X_the_connected_component_count_is_correct():
    grid = _grid(["#...#",
                  "#....",
                  ".....",
                  "...##"])
    comps = pixels.components(pixels.binarize(grid, 128), 0)
    assert len(comps) == 3, [c["bbox"] for c in comps]


def test_Y_tiny_noise_filtering_is_deterministic():
    grid = _grid(["#####",
                  "#####",
                  ".....",
                  "....#"])
    mask = pixels.binarize(grid, 128)
    assert len(pixels.components(mask, 0)) == 2
    assert len(pixels.components(mask, 2)) == 1
    assert pixels.components(mask, 2) == pixels.components(mask, 2)
    assert 0 < pixels.MIN_COMPONENT_AREA_RATIO < 0.05


def test_Z_preprocessing_never_looks_at_the_label():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for name in ("binarize", "otsu_threshold", "components",
                 "select_component", "features", "crop_box"):
        start = source.index(f"def {name}")
        body = source[start:source.index("\ndef ", start + 10)]
        for banned in ('label', 'printed_value', 'printed_1', 'printed_2'):
            assert banned not in body, f"{name}: {banned}"


# ------------------------------------------------------------ AA .. AF
# Los casos con nombre y los controles.

def test_AA_p0644l0073_is_represented():
    rows = {row["block"]: row for row in _rows()}
    assert "p0644l0073" in rows
    assert rows["p0644l0073"]["printed_value"] == 12
    assert rows["p0644l0073"]["label"] == 1
    assert rows["p0644l0073"]["features"]["ink_pixels"] > 0


def test_AB_the_a_o_printed_10_case_is_represented():
    rows = {row["block"]: row for row in _rows()}
    assert "p0149l0069" in rows
    assert rows["p0149l0069"]["printed_value"] == 10
    assert rows["p0149l0069"]["form"] == "a o"


def test_AC_a_o_printed_20_cases_are_represented():
    twenties = [row for row in _rows()
                if row["form"] == "a o" and row["printed_value"] == 20]
    assert twenties, "hacen falta ejemplos de «a o» que impriman 20"


def test_AD_the_narrow_printed_2_regression_is_represented():
    rows = {row["block"]: row for row in _rows()}
    for block in NARROW_PRINTED_2:
        assert block in rows, block
        assert rows[block]["label"] == 2, block


def test_AE_the_spanish_a_control_is_represented():
    controls = _section()["controls"]
    assert controls["spanish_a"]["n"] > 0
    assert controls["spanish_a"]["measured"] > 0


def test_AF_accented_a_stays_distinct():
    controls = _section()["controls"]
    assert "spanish_á" in controls
    # y ninguna fila del estudio lleva «á»
    for row in _rows():
        assert "á" not in row["form"], row["form"]
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("unicodedata", "NFD", "NFKD", "casefold", ".lower()"):
        assert banned not in source, banned


# ------------------------------------------------------------ AG .. AK
# El análisis.

def test_AG_threshold_trials_enumerate_every_labelled_case():
    section = _section()
    rows = _rows()
    for feature, trial in section["threshold_trials"].items():
        if not trial.get("exists"):
            continue
        assert trial["total"] == len(
            [row for row in rows if row["features"].get(feature) is not None]
        ), feature


def test_AH_abstention_is_supported():
    trials = _section()["threshold_trials"]
    usable = [t for t in trials.values() if t.get("exists")]
    assert usable, "ninguna medida admite siquiera una regla sin inversiones"
    for trial in usable:
        assert trial["low"] <= trial["high"]
        assert trial["abstentions"] == trial["total"] - trial["decided"]


def test_AI_class_inversions_are_counted_explicitly():
    section = _section()
    assert "class_inversions" in section
    assert section["class_inversions"] == 0 or section["chosen_rule"] is None
    for feature, row in section["overlap_by_feature"].items():
        assert "separable" in row, feature


def test_AJ_there_is_no_machine_learning():
    source = open(os.path.join(DIR, "a_glyph_pixels.py"), encoding="utf-8").read()
    for banned in ("fit(", "predict(", "train", "model", "classifier",
                   "neural", "gradient", "weights", "epoch"):
        assert banned not in source.lower() or banned == "train", banned


def test_AK_no_ml_library_is_imported():
    banned = {"sklearn", "torch", "tensorflow", "keras", "cv2", "scipy",
              "numpy", "pandas", "xgboost"}
    for name in ("a_glyph_pixels.py",):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                for alias in node.names:
                    assert alias.name.split(".")[0] not in banned, alias.name
            elif isinstance(node, ast.ImportFrom):
                assert (node.module or "").split(".")[0] not in banned


# ------------------------------------------------------------ AL .. AT
# Lo que no se ha tocado.

def test_AL_the_runtime_glyph_map_is_unchanged():
    expected = {("I", "o"): 10, ("I", "I"): 11, ("I", "a"): 12,
                ("1", "1"): 11, ("1", "3"): 13, ("1", "4"): 14,
                ("1", "5"): 15, ("1", "8"): 18, ("1", "9"): 19}
    assert compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS == expected
    for key in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS:
        assert key[0] != "a", key


def test_AM_the_parser_recovery_is_unchanged():
    source = open(os.path.join(DIR, "page_parser.py"), encoding="utf-8").read()
    assert "a_glyph_pixels" not in source, "el parser no puede tocar píxeles"
    assert "PIL" not in source and "pdftoppm" not in source
    assert source.count("_try_compound") >= 2


def test_AN_verse_refs_are_unchanged():
    audit = _audit()
    recovery = audit["verse_segmentation_audit"]["a_glyph_pixel_recovery"]
    assert recovery["refs_before"] == 3572
    assert audit["verse_refs"] == recovery["refs_after"]
    assert audit["materialized_verse_refs"] == recovery["refs_after"]
    assert audit["verse_refs_in_review_slots"] == 0


def test_AO_ownership_is_unchanged():
    recovery = _audit()["verse_segmentation_audit"]["compound_glyph_recovery"]
    assert recovery["applied"] == 183
    assert recovery["refs_added"] == 177
    assert recovery["blocks_moved"] == 1355
    assert recovery["block_loss"] == 0
    assert recovery["dual_ownership"] == 0


def test_AP_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["structure_resolution"]["chapters_resolved"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0


def test_AQ_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AR_out_of_order_refs_stay_at_zero():
    assert _audit()["out_of_order_refs"] == []


def test_AS_nothing_lands_outside_the_canon():
    assert _audit()["verse_segmentation_audit"]["beyond_canonical_top"] == 0


def test_AT_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


# ------------------------------------------------------------ AU .. BD
# Metadatos, artefactos y determinismo.

def _batch(name, expected):
    rows = [row for row in _payload()["reviews"] if row["batch"] == name]
    assert len(rows) == expected, f"{name}: {len(rows)}"
    for row in rows:
        assert row["structural_effect"] == "none_diagnostic_only"
    return rows


def test_AU_batch_124_is_preserved():
    rows = _batch("batch-124", 12)
    outcomes = collections.Counter(row["outcome"] for row in rows)
    assert outcomes["printed_marker_corrupted"] == 5


def test_AV_batch_127_is_preserved():
    rows = _batch("batch-127", 213)
    populations = collections.Counter(row["population"] for row in rows)
    assert populations["glyph_candidate"] == 189


def test_AW_batch_128_is_preserved():
    rows = _batch("batch-128", 108)
    values = collections.defaultdict(set)
    for row in rows:
        values[row["glyph_form"]].add(row["observed_printed_value"])
    assert values["a a"] == {12, 22}


def test_AX_batch_129_is_preserved():
    rows = _batch("batch-129", 62)
    assert sum(1 for row in rows if row["first_printed_digit"] == 1) == 2


def test_AY_batch_130_is_diagnostic_only():
    rows = _batch130()
    if not rows:
        return
    required = ("review_id", "batch", "book", "scan_page", "pdf_page",
                "source_sha256", "block", "raw_ocr", "glyph_form",
                "observed_printed_value", "first_printed_digit",
                "crop_dpi", "crop_bbox", "preprocessing", "ink_bbox",
                "pixel_feature_summary", "outcome", "confidence",
                "rationale", "structural_effect")
    for row in rows:
        for field in required:
            assert field in row, f"{row['review_id']}: falta {field}"
        assert row["structural_effect"] == "none_diagnostic_only"


def test_AZ_generated_crops_are_not_tracked_by_git():
    section = _section()
    where = section["facsimile"]["crop_cache"]
    assert where.startswith("build/"), where
    data_dir = os.path.join(ROOT, "data", "torresamat1835")
    for name in os.listdir(data_dir):
        assert not name.lower().endswith((".png", ".jpg", ".tif", ".pdf")), name


def test_BA_the_analysis_is_offline():
    tree = ast.parse(open(os.path.join(DIR, "a_glyph_pixels.py"),
                          encoding="utf-8").read())
    banned = {"urllib", "requests", "socket", "http", "subprocess", "PIL"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                assert alias.name.split(".")[0] not in banned, alias.name
        elif isinstance(node, ast.ImportFrom):
            assert (node.module or "").split(".")[0] not in banned


def test_BB_feature_extraction_is_deterministic():
    grid = _grid(["......",
                  ".#....",
                  ".#....",
                  ".###..",
                  "......"])
    mask = pixels.binarize(grid, 128)
    comps = pixels.components(mask, 0)
    first = pixels.features(comps[0], comps, (5, 6))
    second = pixels.features(comps[0], comps, (5, 6))
    assert first == second


def test_BC_the_analysis_is_deterministic():
    rows = _rows()
    for feature in pixels.SCALAR_FEATURES:
        assert pixels.overlap(rows, feature) == pixels.overlap(rows, feature)
        assert pixels.zero_inversion_rule(rows, feature) == \
            pixels.zero_inversion_rule(rows, feature)
    assert pixels.verdict(rows) == pixels.verdict(rows)


def test_BD_the_parser_runtime_is_unchanged():
    section = _section()
    assert section["runtime_effect"] == "none_diagnostic_only"
    assert section["chosen_rule"] is None or \
        section["readiness_for_runtime"]["safe_for_runtime_experiment"]
    assert section["parser_runtime_delta"] == 0


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
    print(f"torresamat1835_a_glyph_pixel_shape_failures={failures}")
    sys.exit(1 if failures else 0)
