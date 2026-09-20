#!/usr/bin/env python3
import hashlib
import json
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/no_trusted_band_discriminator.json"
BASELINE_COMMIT = json.loads(ART.read_text())['provenance']['baseline_commit']
ARGS = ["python3", str(ROOT / "scripts/torresamat1835/no_trusted_band_discriminator.py"),
        "--inventory", str(ROOT / "data/torresamat1835/remaining_glyph_inventory.json"),
        "--projected", str(ROOT / "data/torresamat1835/projected_rejection_audit.json"),
        "--audit", str(ROOT / "build/torresamat1835-audit/volume3.json"),
        "--xml", str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"),
        "--pdf", str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf"),
        "--baseline-commit", BASELINE_COMMIT]


def test_artifact_contract():
    d = json.loads(ART.read_text())
    audit = json.loads((ROOT / "build/torresamat1835-audit/volume3.json").read_text())
    inventory = json.loads((ROOT / "data/torresamat1835/remaining_glyph_inventory.json").read_text())
    assert d["schema_version"] == 1
    assert len(d["occurrences"]) == 18
    ids = {(r["stable_identity"]["block_id"], r["stable_identity"]["gap_key"]) for r in d["occurrences"]}
    assert len(ids) == 18
    assert all(r["task128_candidate"]["historical_reason"] == "no_trusted_marker_band" for r in d["occurrences"])
    assert len(d["batch_137_facsimile_reviews"]) == 18
    assert {r["classification"] for r in d["batch_137_facsimile_reviews"]} == {"PRINTED_VERSE_MARKER"}
    assert {r["printed_value"] for r in d["batch_137_facsimile_reviews"]} == {10}
    assert all(r["structural_effect"] == "none_diagnostic_only" for r in d["batch_137_facsimile_reviews"])
    assert all(not r["chapter_diagnostic_context"]["expected_verse_used"] and
               not r["chapter_diagnostic_context"]["previous_plus_one_used"] and
               not r["chapter_diagnostic_context"]["next_minus_one_used"]
               for r in d["occurrences"])
    assert d["trusted_anchor_failure"]["count_distribution"] == {"0": 18}
    assert d["accepted_controls"]["population"] == 183
    assert d["hard_negatives"]["population"] >= 3
    assert all("page" not in r.get("features", {}) and "block_id" not in r.get("features", {}) for r in d["hard_negatives"]["controls"])
    assert all("page" not in r["features"] and "block_id" not in r["features"] for r in d["feature_groups"]["accepted_task128_geometry_controls"])
    assert len(d["candidate_rules"]) == 3
    assert all(len(r["features"]) <= 2 for r in d["candidate_rules"])
    assert all("page" not in r["features"] and "block_id" not in r["features"] and "chapter" not in r["features"] and "verse" not in r["features"] for r in d["candidate_rules"])
    assert d["selected_rule"]["safe_gate"]
    assert d["selected_rule"]["hard_negatives_accepted"] == 0
    assert d["audit_integration"]["safe_discriminator_found"] is True
    assert d["invariants"]["runtime_recovery"] is False
    assert d["invariants"]["expected_verse_used"] is False
    assert d["invariants"]["previous_plus_one_used"] is False
    assert d["invariants"]["next_minus_one_used"] is False
    assert d["invariants"]["task128_minimum_trusted_anchors"] == 3
    assert audit["verse_refs"] == 3848
    assert inventory["physical_gap_total"] == 3255
    assert inventory["glyph_gap_total"] == 1310
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert audit["duplicate_refs"] == []
    assert audit["out_of_order_refs"] == []
    section = audit["verse_segmentation_audit"]["no_trusted_band_discriminator_validation"]
    assert section["audit_integration"]["population"] == 18
    assert section["overall_family_status"] == d["overall_family_status"]
    assert d["task138_recommendation"]["target"]


def test_recomputed_anchor_policy_and_idempotence():
    with tempfile.TemporaryDirectory() as td:
        a = pathlib.Path(td) / "a.json"; b = pathlib.Path(td) / "b.json"
        subprocess.run(ARGS + ["--out", str(a)], cwd=ROOT, check=True, capture_output=True)
        subprocess.run(ARGS + ["--out", str(b)], cwd=ROOT, check=True, capture_output=True)
        assert a.read_bytes() == b.read_bytes() == ART.read_bytes()
        assert hashlib.sha256(a.read_bytes()).digest() == hashlib.sha256(b.read_bytes()).digest()


if __name__ == "__main__":
    test_artifact_contract(); test_recomputed_anchor_policy_and_idempotence(); print("ok")
