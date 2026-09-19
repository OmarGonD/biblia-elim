"""Regresiones de TORRES-1835-A-GLYPH-PIXEL-RECOVERY-131."""
import ast
import copy
import json
import math
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import a_glyph_pixel_recovery as recovery
import compound_glyphs

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
ARTIFACT = os.path.join(ROOT, "data", "torresamat1835",
                        "a_glyph_pixel_features.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")


def _evidence(width=24, aspect=2 / 3, pixels=640):
    return recovery.PixelEvidence("p0001l0001", 1, "a 4", "right", "body",
                                  (1, 2, 3, 4), 600, 100,
                                  width, aspect, pixels)


def _payload():
    with open(ARTIFACT, encoding="utf-8") as handle:
        return json.load(handle)


def _load_changed(change):
    payload = _payload()
    change(payload)
    with tempfile.TemporaryDirectory() as directory:
        path = os.path.join(directory, "features.json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(payload, handle)
        return recovery.EvidenceIndex.load(path)


def _audit():
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]["a_glyph_pixel_recovery"]


def test_A_artifact_provenance_is_validated():
    index = recovery.EvidenceIndex.load(ARTIFACT)
    assert index.valid and len(index.rows) == 409, index.error
    assert _payload()["source_ocr"]["sha256"] == recovery.SOURCE_OCR_SHA256
    assert _payload()["facsimile"]["sha256"] == recovery.FACSIMILE_SHA256


def test_B_wrong_source_sha_fails_closed():
    index = _load_changed(lambda p: p["source_ocr"].update(sha256="0" * 64))
    assert not index.valid and index.error == "source_provenance_mismatch"


def test_C_unknown_schema_fails_closed():
    index = _load_changed(lambda p: p.update(schema_version=999))
    assert not index.valid and index.error == "unsupported_artifact_schema"


def test_D_missing_block_evidence_abstains():
    index = recovery.EvidenceIndex.load(ARTIFACT)
    evidence, outcome, reason = index.lookup(
        "p9999l9999", scan_page=9999, form="a 4", column="right",
        zone="body", first_bbox=(0, 0, 10, 10),
        second_bbox=(20, 0, 30, 10))
    assert evidence is None and outcome == recovery.MISSING_EVIDENCE
    assert reason == "missing_block_evidence"


def test_E_ocr_bbox_width_is_not_a_classifier_input():
    params = set(__import__("inspect").signature(
        recovery.classify_a_first_digit).parameters)
    assert params == {"evidence"}
    source = open(os.path.join(DIR, "a_glyph_pixel_recovery.py"),
                  encoding="utf-8").read()
    assert "ocr_bbox_width" not in source


def test_F_G_validated_width_and_aspect_classify_both_classes():
    assert recovery.classify_a_first_digit(_evidence()) == recovery.PRINTED_1
    assert recovery.classify_a_first_digit(
        _evidence(31, .825, 744)) == recovery.PRINTED_2


def test_H_width_abstention_band_abstains():
    for width in range(25, 31):
        assert recovery.classify_a_first_digit(
            _evidence(width, .7, 700)) == recovery.ABSTAIN


def test_I_J_K_both_aspect_conditions_are_required_and_disagreement_abstains():
    assert recovery.classify_a_first_digit(
        _evidence(24, .825, 700)) == recovery.ABSTAIN
    assert recovery.classify_a_first_digit(
        _evidence(31, 2 / 3, 700)) == recovery.ABSTAIN
    assert recovery.classify_a_first_digit(
        _evidence(24, .7, 640)) == recovery.ABSTAIN
    assert recovery.classify_a_first_digit(
        _evidence(30, .825, 744)) == recovery.ABSTAIN


def test_L_ink_pixel_contradiction_never_overrides_safety():
    assert recovery.classify_a_first_digit(
        _evidence(24, 2 / 3, 744)) == recovery.ABSTAIN
    assert recovery.classify_a_first_digit(
        _evidence(31, .825, 640)) == recovery.ABSTAIN


def test_M_N_O_sequence_and_gap_are_not_classifier_inputs():
    source = open(os.path.join(DIR, "a_glyph_pixel_recovery.py"),
                  encoding="utf-8").read()
    body = source[source.index("def classify_a_first_digit"):
                  source.index("\ndef second_digit")]
    for banned in ("gap", "previous", "next", "book", "chapter",
                   "verse_limit", "+ 1", "- 1"):
        assert banned not in body, banned


def test_P_Q_R_exact_a_and_compound_context_only():
    source = open(os.path.join(DIR, "page_parser.py"), encoding="utf-8").read()
    assert 'one != "a"' in source
    assert 'first + 1 >= len(words)' in source
    assert 'startswith("a ")' in source
    assert recovery.second_digit("á") is None


def test_S_T_U_V_W_structural_guards_are_retained():
    source = open(os.path.join(DIR, "page_parser.py"), encoding="utf-8").read()
    for required in ("marker_tokens", "marker_bbox", "tolerance(band)",
                     "NO_BAND", "OUT_OF_BAND", "NO_TEXT", "DIGIT_FOLLOWS"):
        assert required in source
    # La ruta se alcanza desde el manejador español; layout entrega sólo
    # cuerpo/derecha a ese manejador, igual que en la 128.
    assert "self._handle_spanish(placed, prov)" in source


def test_X_Y_Z_runtime_never_processes_images():
    for name in ("page_parser.py", "a_glyph_pixel_recovery.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for banned in ("pdftoppm", "PIL", "Image.open",
                       "connected component", "a_glyph_pixels"):
            assert banned not in source, (name, banned)
        assert "otsu_threshold(" not in source
        tree = ast.parse(source)
        imports = {(node.module or "").split(".")[0]
                   for node in ast.walk(tree)
                   if isinstance(node, ast.ImportFrom)}
        imports |= {alias.name.split(".")[0] for node in ast.walk(tree)
                    if isinstance(node, ast.Import) for alias in node.names}
        assert not imports & {"PIL", "cv2", "numpy", "a_glyph_pixels"}


def test_AA_AB_literal_second_digit_is_literal_and_independent():
    assert [recovery.second_digit(str(n)) for n in range(10)] == list(range(10))
    assert recovery.second_digit("4") == 4
    source = open(os.path.join(DIR, "a_glyph_pixel_recovery.py"),
                  encoding="utf-8").read()
    body = source[source.index("def second_digit"):source.index("\ndef crop_box")]
    assert "classify" not in body and "first" not in body


def test_AC_AD_a_o_is_locally_and_exhaustively_validated():
    section = _section()["visual_validation"]
    assert section["a_o_would_apply"] == section["a_o_reviewed"] == 32
    assert section["a_o_complete"] and section["conflicts"] == 0
    assert recovery.second_digit("o") == 0
    assert recovery.second_digit("O") is None


def test_AE_AF_a_a_is_locally_and_exhaustively_validated():
    section = _section()["visual_validation"]
    assert section["a_a_would_apply"] == section["a_a_reviewed"] == 31
    assert section["a_a_complete"] and section["conflicts"] == 0
    assert recovery.second_digit("a") == 2
    assert recovery.second_digit("á") is None


def test_AG_AH_a_I_and_unknown_second_tokens_are_withheld():
    assert recovery.second_digit("I") is None
    for token in ("A", "Y", "y", "O", "t", "*", "¡", "¿", "É", "Ó"):
        assert recovery.second_digit(token) is None, token
    assert _section()["safety_gate_by_form"]["a I"]["status"] == "WITHHELD"


def test_AI_AJ_canon_only_rejects_the_composed_candidate():
    section = _section()
    assert section["canon_rejections"] == 2
    for row in section["canon_rejected_detail"]:
        assert row["marker"] > row["verse_limit"]
    source = open(os.path.join(DIR, "page_parser.py"), encoding="utf-8").read()
    assert "first_digit * 10 + second" in source


def test_AK_AL_AM_no_sequence_or_gap_authority():
    for name in ("a_glyph_pixel_recovery.py", "page_parser.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        assert "expected_gap" not in source and "next - 1" not in source
        assert "previous + 1" not in source


def test_AN_through_AS_accounting_and_ownership_are_closed():
    section = _section()
    assert section["refs_removed"] == 0
    assert section["block_loss"] == 0
    assert section["dual_ownership"] == 0
    assert section["markers_recovered"] == 276
    assert section["refs_added"] == 276
    assert section["blocks_moved"] == 1900
    assert section["unexplained_block_moves"] == 0
    assert sum(row["blocks_moved"]
               for row in section["by_form"].values()) == 1900
    assert section["duplicate_markers"] == 0
    assert section["markers_recovered"] == (section["refs_added"]
                                             + section["existing_ref_matches"])


def test_AT_AU_AV_single_a_and_other_simple_glyphs_stay_outside():
    glyph = _audit()["verse_segmentation_audit"]["glyph_facsimile_validation"]
    historic = {(row.get("book"), row.get("chapter"), row.get("verse"))
                for row in glyph["reviews"]
                if (row.get("glyph_form") or "").startswith(("a", "S"))}
    assert ("Ps", 1, 2) in historic
    assert ("Eccl", 1, 2) in historic
    assert ("Isa", 17, 8) in historic
    assert all(not key[0].startswith("a")
               for key in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS)


def test_AW_task_128_runtime_map_is_unchanged():
    expected = {("I", "o"): 10, ("I", "I"): 11, ("I", "a"): 12,
                ("1", "1"): 11, ("1", "3"): 13, ("1", "4"): 14,
                ("1", "5"): 15, ("1", "8"): 18, ("1", "9"): 19}
    assert compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS == expected
    old = _audit()["verse_segmentation_audit"]["compound_glyph_recovery"]
    assert (old["applied"], old["refs_added"], old["blocks_moved"]) == \
        (183, 177, 1355)


def test_AX_through_BB_named_real_regressions():
    reviews = json.load(open(REVIEWS, encoding="utf-8"))["reviews"]
    rows = {row.get("block"): row for row in reviews
            if row.get("batch") == "batch-131"}
    assert rows["p0644l0073"]["observed_printed_value"] == 12
    assert any(r["glyph_form"] == "a o" and r["observed_printed_value"] == 10
               for r in rows.values())
    assert any(r["glyph_form"] == "a o" and r["observed_printed_value"] == 20
               for r in rows.values())
    assert any(r["glyph_form"] == "a a" and r["observed_printed_value"] == 12
               for r in rows.values())
    assert any(r["glyph_form"] == "a a" and r["observed_printed_value"] == 22
               for r in rows.values())


def test_BC_BD_visual_and_dry_run_identities_close():
    section = _section()
    visual = section["visual_validation"]
    assert visual["predicted_first_1_total"] == 2
    assert visual["predicted_first_1_reviewed"] == 2
    assert visual["predicted_first_1_complete"]
    dry = section["dry_run"]
    assert dry["eligible_candidates"] == (
        dry["first_digit_1"] + dry["first_digit_2"]
        + dry["pixel_abstain"] + dry["invalid_evidence"]
        + dry["missing_evidence"])
    assert dry["observed_a_compounds"] == (
        dry["glued_frame"] + dry["zone_rejected"]
        + dry["eligible_candidates"])


def test_BE_through_BJ_global_invariants():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["duplicate_refs"] == []
    assert audit["out_of_order_refs"] == []
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert audit["verse_refs"] == audit["materialized_verse_refs"] == 3848


def test_BK_through_BQ_review_batches_and_schema_are_preserved():
    payload = json.load(open(REVIEWS, encoding="utf-8"))
    batches = {row.get("batch") for row in payload["reviews"]}
    for batch in ("batch-124", "batch-127", "batch-128", "batch-129",
                  "batch-130", "batch-131"):
        assert batch in batches
    rows = [row for row in payload["reviews"] if row.get("batch") == "batch-131"]
    assert len(rows) == 79
    required = {"review_id", "batch", "book", "chapter", "scan_page",
                "pdf_page", "source_sha256", "block", "raw_ocr",
                "glyph_form", "first_pixel_decision", "ink_width",
                "ink_aspect", "second_token", "observed_printed_marker",
                "observed_printed_value", "full_candidate_value", "outcome",
                "confidence", "rationale", "structural_effect"}
    assert all(required <= set(row) for row in rows)


def test_BR_BS_BT_deterministic_lookup_parse_and_idempotency():
    first = recovery.EvidenceIndex.load(ARTIFACT)
    second = recovery.EvidenceIndex.load(ARTIFACT)
    assert first.rows == second.rows
    section = _section()
    assert len({row["block_id"] for row in section["rows"]}) == 409
    assert _audit()["duplicate_refs"] == []


def test_BU_BV_BW_offline_and_no_case_hardcoding():
    for name in ("a_glyph_pixel_recovery.py", "page_parser.py"):
        source = open(os.path.join(DIR, name), encoding="utf-8").read()
        for banned in ("urllib", "requests", "socket", "if block_id ==",
                       "if page ==", "if chapter ==", "if verse =="):
            assert banned not in source, (name, banned)


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
    print(f"torresamat1835_a_glyph_pixel_recovery_failures={failures}")
    sys.exit(1 if failures else 0)
