"""Regressions for TORRES-1835-GLUED-COMPOUND-MARKER-AUDIT-132."""
import ast
import inspect
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import compound_glyphs
import glued_compound_markers as glued
import source_ocr

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
XML = os.path.join(ROOT, "build", "torresamat1835-cache",
                   "lasagradabiblia01unkngoog_djvu.xml")
PDF = os.path.join(ROOT, "build", "torresamat1835-cache",
                   "lasagradabiblia01unkngoog.pdf")
ARTIFACT = os.path.join(ROOT, "data", "torresamat1835",
                        "glued_marker_segments.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")


def _payload():
    return json.load(open(ARTIFACT, encoding="utf-8"))


def _audit():
    return json.load(open(AUDIT, encoding="utf-8"))


def _section():
    return _audit()["verse_segmentation_audit"][
        "glued_compound_marker_validation"]


def test_A_B_inventory_and_current_definition_are_deterministic():
    once = glued.inventory(XML)
    twice = glued.inventory(XML)
    assert once == twice and len(once) == 159
    assert all(row["candidate_family"] == "a_star_glued_frame" for row in once)
    assert sum(row["eligible_body_right"] for row in once) == 81
    assert compound_glyphs.marker_tokens(
        [source_ocr.SourceWord(".a8", (1, 1, 20, 20), 50),
         source_ocr.SourceWord("Texto", (30, 1, 80, 20), 50)]) == \
        (None, compound_glyphs.GLUED_FRAME)


def test_C_D_E_ocr_box_is_anchor_only_without_proportional_split():
    source = open(os.path.join(DIR, "glued_compound_markers.py"),
                  encoding="utf-8").read()
    assert "localization anchor only" in source
    for banned in ("word_width /", "/ character_count", "monospace",
                   "fixed percentage", "len(visible) /", "/ len(visible)"):
        assert banned not in source


def test_F_G_H_source_provenance_and_page_mapping():
    payload = _payload()
    assert glued.sha256(PDF) == payload["facsimile"]["sha256"] == \
        glued.FACSIMILE_SHA256
    assert glued.sha256(XML) == payload["source_ocr"]["sha256"] == \
        glued.OCR_SHA256
    assert all(row["pdf_page"] == row["scan_page"] + 1
               for row in payload["instances"])


def test_I_J_crop_coordinates_are_deterministic_and_cache_is_untracked():
    rows = glued.inventory(XML)
    assert [row["crop_bbox"] for row in rows] == \
        [row["crop_bbox"] for row in glued.inventory(XML)]
    assert _payload()["preprocessing"]["render_dpi"] == 600
    assert not any(name.lower().endswith((".png", ".jpg", ".jpeg"))
                   for name in os.listdir(os.path.dirname(ARTIFACT)))


def test_K_through_N_pixel_primitives_are_deterministic():
    grid = [[255, 20, 255], [255, 20, 255]]
    histogram = [0] * 256
    for row in grid:
        for value in row:
            histogram[value] += 1
    assert glued.pixels.otsu_threshold(histogram) == \
        glued.pixels.otsu_threshold(histogram)
    mask = glued.pixels.binarize(grid, 128)
    assert mask == glued.pixels.binarize(grid, 128)
    assert glued.pixels.components(mask, 0) == \
        glued.pixels.components(mask, 0)
    assert glued._projection(mask) == [0, 2, 0]


def test_O_through_S_marker_band_localises_but_answer_hints_are_absent():
    params = set(inspect.signature(glued.segment).parameters)
    assert params == {"gray", "row"}
    source = open(os.path.join(DIR, "glued_compound_markers.py"),
                  encoding="utf-8").read()
    body = source[source.index("def _one_threshold"):
                  source.index("def segment")]
    assert "marker_band" in body
    for banned in ("expected_verse", "previous_verse", "next_verse",
                   "canonical_gap", "previous + 1", "next - 1"):
        assert banned not in body


def test_T_through_AB_all_required_outcomes_and_controls_are_represented():
    rows = _payload()["instances"]
    outcomes = {row["outcome"] for row in rows}
    assert glued.SINGLE in outcomes and glued.MULTIPLE in outcomes
    assert glued.AMBIGUOUS in outcomes and glued.NO_SEPARATION in outcomes
    assert glued.APPARATUS in outcomes and glued.LATIN in outcomes
    reviews = [row for row in json.load(open(REVIEWS, encoding="utf-8"))["reviews"]
               if row.get("batch") == "batch-132"]
    assert any(row["segmentation_outcome"] == "ordinary_text_false_candidate"
               for row in reviews)
    assert any(row.get("observed_printed_marker") for row in reviews)
    assert any(row.get("observed_printed_marker") is None for row in reviews)


def test_AC_through_AF_safety_stability_and_resolution_are_explicit():
    section = _section()
    assert section["false_positive_count"] > 0
    assert section["positive_control_failures"] > 0
    assert section["stability_stats"]["max_bbox_shift"] <= 1
    control = section["resolution_control"]
    assert control["total"] >= 6 and 0 < control["equivalent"] < control["total"]


def test_AG_AH_no_runtime_image_access_or_parser_recovery_change():
    for name in ("page_parser.py", "parser.py", "a_glyph_pixel_recovery.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        assert "glued_compound_markers" not in source
    tree = ast.parse(open(os.path.join(DIR, "page_parser.py"),
                          encoding="utf-8").read())
    imports = {alias.name for node in ast.walk(tree)
               if isinstance(node, ast.Import) for alias in node.names}
    assert not imports & {"PIL", "cv2", "glued_compound_markers"}


def test_AI_through_AQ_refs_ownership_and_prior_recoveries_are_invariant():
    audit = _audit()
    section = _section()
    assert section["refs_before"] == section["refs_after"] == 3848
    assert section["ownership_changes"] == 0
    old = audit["verse_segmentation_audit"]["compound_glyph_recovery"]
    assert (old["applied"], old["refs_added"]) == (183, 177)
    pixel = audit["verse_segmentation_audit"]["a_glyph_pixel_recovery"]
    assert (pixel["markers_recovered"], pixel["refs_added"]) == (276, 276)
    assert audit["verse_segmentation_audit"]["total"] == 3255
    assert audit["verse_segmentation_audit"]["by_signal"][
        "lone_glyph_inside_previous_verse"] == 1310


def test_AR_through_AW_global_invariants():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["canonical_chapter_gap_reviews"]["canonical_missing"] == {}
    assert audit["duplicate_refs"] == [] and audit["out_of_order_refs"] == []
    assert audit["verse_segmentation_audit"]["beyond_canonical_top"] == 0
    assert audit["metrics"]["ocr_blocks"] == 57700


def test_AX_AY_prior_batches_and_batch_132_are_preserved():
    reviews = json.load(open(REVIEWS, encoding="utf-8"))["reviews"]
    batches = {row.get("batch") for row in reviews}
    assert {"batch-124", "batch-127", "batch-128", "batch-129",
            "batch-130", "batch-131", "batch-132"} <= batches
    assert all(row.get("structural_effect") == "none_diagnostic_only"
               for row in reviews if row.get("batch") == "batch-132")


def test_AZ_BA_BB_metadata_and_audit_are_deterministically_ordered():
    payload = _payload()
    blocks = [row["block"] for row in payload["instances"]]
    assert blocks == sorted(blocks)
    assert len(blocks) == len(set(blocks))
    assert _section()["candidate_total"] == len(blocks)


def test_BC_through_BF_offline_and_no_specific_production_rules():
    source = open(os.path.join(DIR, "glued_compound_markers.py"),
                  encoding="utf-8").read()
    assert "http://" not in source and "https://" not in source
    body = source[source.index("def segment"):source.index("def _taxonomy")]
    assert "p0" not in body and "verse" not in inspect.signature(
        glued.segment).parameters
    assert "expected" not in body and "chapter" not in body


if __name__ == "__main__":
    tests = sorted((name, value) for name, value in globals().items()
                   if name.startswith("test_") and callable(value))
    for name, test in tests:
        test()
    print(f"glued_compound_marker_failures=0 tests={len(tests)}")
