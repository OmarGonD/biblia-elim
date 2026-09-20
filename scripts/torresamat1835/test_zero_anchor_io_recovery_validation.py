"""Focused regression checks for task 138's diagnostic artifact."""
import hashlib
import json
import pathlib
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/zero_anchor_io_recovery_validation.json"


def _audit():
    return json.loads(ART.read_text(encoding="utf-8"))


def test_rule_and_positive_accounting():
    d = _audit()
    inv = d["full_corpus_match_inventory"]
    assert d["task137_discriminator"]["rule"] == (
        "trusted_anchor_count == 0 AND exact_compound_form == 'I o'")
    assert d["task137_discriminator"]["occurrence_id_allowlist_authority"] is False
    assert inv["source_line_matches"] == 1
    assert inv["occurrence_matches"] == 18
    assert inv["known_positive_matches"] == 18
    assert inv["additional_matches"] == []
    assert inv["accepted_task128_controls_matched"] == 0
    assert inv["hard_negatives_matched"] == 0
    assert inv["other_known_negative_matches"] == 0


def test_value_and_selection_are_source_bounded():
    d = _audit()
    assert d["task137_discriminator"]["global_mapping"] is False
    assert "only the validated zero-anchor I o family" in d[
        "task137_discriminator"]["bounded_mapping_scope"]
    assert set(d["full_corpus_match_inventory"]["selection_excludes"]) >= {
        "page id", "block id", "book", "chapter", "verse",
        "known occurrence list", "expected verse", "previous+1", "next-1"}
    for row in d["dry_run_recovery"]["records"]:
        assert row["printed_marker_value"] == 10
        assert not row["current_gap_context"]["expected_verse_used"]
        assert not row["current_gap_context"]["previous_plus_one_used"]
        assert not row["current_gap_context"]["next_minus_one_used"]


def test_ref_identity_and_semantic_safety():
    d = _audit()
    effects = d["ref_effects"]
    assert effects["CREATE_NEW_REF"] == 1
    assert effects["REOPEN_EXISTING_REF"] == 0
    assert effects["NO_REF_EFFECT"] == 0
    assert effects["INVALID"] == 0
    assert d["dry_run_recovery"]["proposed_native_verserefs"] == ["Ps.17.10"]
    assert effects["removed_refs"] == []
    assert effects["renumbered_refs"] == []
    assert effects["unrelated_refs_identity_unchanged"]
    safe = d["semantic_safety"]
    assert safe["duplicate_refs_predicted"] == 0
    assert safe["out_of_order_refs_predicted"] == 0
    assert safe["outside_canon_predicted"] == 0
    assert safe["impossible_verse_numbers"] == 0


def test_ownership_and_gap_delta():
    d = _audit()
    own = d["ownership_effects"]
    assert own["ownership_moves"] == 27
    assert own["block_loss"] == 0
    assert own["dual_ownership"] == 0
    gaps = d["gap_effects"]
    assert (gaps["physical_gaps_before"], gaps["physical_gaps_after"],
            gaps["physical_gap_reduction"]) == (3255, 3254, 1)
    assert (gaps["glyph_gaps_before"], gaps["glyph_gaps_after"],
            gaps["glyph_gap_reduction"]) == (1310, 1309, 1)


def test_task_regressions_and_runtime_immutability():
    d = _audit()
    r = d["controls_and_regressions"]
    assert (r["accepted_task128_controls"],
            r["accepted_task128_controls_matched"]) == (183, 0)
    assert (r["task128_refs_before"], r["task128_refs_after_current"]) == (3572, 3848)
    assert (r["task128_ref_contribution"], r["task128_ownership_moves"]) == (177, 1355)
    assert (r["task131_accepted_markers"], r["task131_refs"]) == (276, 276)
    assert r["task131_ownership_moves"] == 1900
    assert r["task128_ownership_unchanged"] and r["task131_ownership_unchanged"]
    assert r["GLUED_FRAME"] == "CLOSED_UNSAFE"
    runtime = d["runtime_invariants"]
    assert not runtime["changed"]
    assert runtime["verse_refs_before"] == runtime["verse_refs_after"] == 3848
    assert runtime["physical_gaps_before"] == runtime["physical_gaps_after"] == 3255
    assert runtime["glyph_gaps_before"] == runtime["glyph_gaps_after"] == 1310
    assert runtime["chapters"] == 337 and runtime["ocr_blocks"] == 57700


def test_provenance_status_and_one_recommendation():
    d = _audit()
    assert len(d["provenance"]["baseline_commit"]) == 40
    assert "never current HEAD" in d["provenance"]["baseline_semantics"]
    assert d["final_family_status"] == "IMPLEMENTATION_READY"
    assert d["task139_recommendation"]["count"] == 1
    assert d["task139_recommendation"]["target"] == (
        "bounded implementation of zero-anchor I o fallback")
    assert len(d["fail_closed_conditions"]) >= 8


def test_artifact_deterministic_and_idempotent():
    d = _audit()
    baseline = d["provenance"]["baseline_commit"]
    cmd = ["python3", str(ROOT / "scripts/torresamat1835/zero_anchor_io_recovery_validation.py"),
           "--baseline-commit", baseline, "--out"]
    with tempfile.TemporaryDirectory() as td:
        a, b = pathlib.Path(td) / "a.json", pathlib.Path(td) / "b.json"
        subprocess.run(cmd + [str(a)], cwd=ROOT, check=True,
                       capture_output=True)
        subprocess.run(cmd + [str(b)], cwd=ROOT, check=True,
                       capture_output=True)
        assert a.read_bytes() == b.read_bytes() == ART.read_bytes()
        assert hashlib.sha256(a.read_bytes()).digest() == hashlib.sha256(b.read_bytes()).digest()


if __name__ == "__main__":
    for name in sorted(globals()):
        if name.startswith("test_"):
            globals()[name]()
    print("ok")
