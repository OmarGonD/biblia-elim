#!/usr/bin/env python3
"""Focused regression contract for task 140."""
import ast
import hashlib
import json
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/remaining_glyph_reprioritization.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
BASELINE = "13eb5c80d8cf03dead7940391cfc84b11e600262"


def _make_audit(path):
    subprocess.run(["python3", str(ROOT / "scripts/torresamat1835/audit_volume.py"),
                    "--xml", str(XML), "--volume", "3", "--witness",
                    "ia-lasagradabiblia01unkngoog", "--book", "Ps",
                    "--without-projected-form-a-recovery", "--out", str(path)],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)


def _make_artifact(audit, output):
    subprocess.run(["python3", str(ROOT / "scripts/torresamat1835/remaining_glyph_reprioritization.py"),
                    "--audit", str(audit), "--out", str(output),
                    "--baseline-commit", BASELINE], cwd=ROOT, check=True,
                   stdout=subprocess.DEVNULL)


def test_task_140_contract():
    started = time.monotonic()
    with tempfile.TemporaryDirectory() as td:
        audit_path = Path(td) / "audit.json"
        one = Path(td) / "one.json"
        two = Path(td) / "two.json"
        _make_audit(audit_path)
        _make_artifact(audit_path, one)
        _make_artifact(audit_path, two)
        audit = json.loads(audit_path.read_text())
        data = json.loads(one.read_text())
        assert one.read_bytes() == two.read_bytes()
        assert hashlib.sha256(one.read_bytes()).digest() == hashlib.sha256(two.read_bytes()).digest()
        assert one.read_bytes() == ART.read_bytes()

    seg = audit["verse_segmentation_audit"]
    assert audit["verse_refs"] == 3849
    assert len(seg["gaps"]) == 3254
    assert seg["by_signal"]["lone_glyph_inside_previous_verse"] == 1309
    assert audit["chapters"] == 337 and audit["chapter_claims"]["unresolved"] == 0
    assert not audit["canonical_chapter_gap_reviews"]["canonical_missing"]
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert audit["duplicate_refs"] == [] and audit["out_of_order_refs"] == []
    assert audit.get("outside_canon", []) == []
    recovery = seg["zero_anchor_io_recovery"]
    assert recovery["ownership_moves"] == 27 and recovery["block_loss"] == 0
    assert recovery["dual_ownership"] == 0
    integrated = seg["remaining_glyph_reprioritization"]
    assert integrated["current_inventory_summary"]["count"] == 1309
    assert integrated["selected_task141_family"]["name"] == "PROJECTED_MULTI_TOKEN_EXACT_FORM_A"

    assert data["provenance"]["frozen_baseline_commit"] == BASELINE
    assert data["runtime_baseline"]["verse_refs"] == 3849
    assert data["runtime_baseline"]["physical_gaps"] == 3254
    assert data["runtime_baseline"]["glyph_gaps"] == 1309
    reconciliation = data["historical_reconciliation"]
    assert reconciliation["historical_total"] == 1310
    assert reconciliation["current_total"] == 1309
    assert len(reconciliation["removed_occurrences"]) == 1
    assert reconciliation["removed_occurrences"][0]["key"] == "Ps.17.10"
    assert reconciliation["added_occurrences"] == []
    assert data["current_inventory_summary"]["construction"].startswith("current audit gap rows")
    assert len(data["current_inventory_summary"]["rows"]) == 1309

    split = data["physical_projected_split"]
    assert split["PHYSICAL_SINGLE_TOKEN"] == 6
    assert split["PHYSICAL_MULTI_TOKEN"] == 0
    assert split["PROJECTED_FROM_MULTI_TOKEN"] == 1303
    assert split["historical_6_1304_still_applies"] is False
    assert all(r["primary_root_cause"] != "PHYSICAL_SINGLE_TOKEN" or r["physical_token_count"] == 1
               for r in data["current_inventory_summary"]["rows"])
    assert all(r["primary_root_cause"] != "standalone_glyph_candidate"
               for r in data["current_inventory_summary"]["rows"])

    buckets = data["root_cause_buckets"]
    assert buckets["primary_total"] == 1309
    assert sum(buckets["primary"].values()) == 1309
    assert data["review_coverage"]["occurrence_backed"]
    assert data["review_coverage"]["counts"] == {
        "NOT_REVIEWED": 1301, "REVIEWED_AMBIGUOUS": 4,
        "REVIEWED_NEGATIVE": 3, "REVIEWED_POSITIVE": 1}
    closed = data["closed_family_status"]
    assert closed["GLUED_FRAME"] == "CLOSED_UNSAFE"
    assert closed["outside_marker_band"] == "CLOSED_REJECTED"
    assert closed["task_128"] == "CLOSED_NOT_WIDENED"
    assert closed["task_131"] == "CLOSED_NOT_WIDENED"
    assert closed["zero_anchor_I_o"] == "SOLVED_TASK_139"

    source = (ROOT / "scripts/torresamat1835/remaining_glyph_reprioritization.py").read_text()
    assert "expected_verse_sequence" not in source
    assert data["runtime_invariants"]["expected_verse_used"] is False
    assert data["runtime_invariants"]["previous_plus_one_used"] is False
    assert data["runtime_invariants"]["next_minus_one_used"] is False
    assert "token_identity_alone_authority" in source
    selected = data["selected_task141_family"]
    assert len(data["bounded_candidate_families"]) == 3
    assert selected["population"] == 260 and selected["population"] < 1309
    assert selected["name"] == "PROJECTED_MULTI_TOKEN_EXACT_FORM_A"
    assert selected["task_type"] == "facsimile audit"
    assert len([x for x in data["bounded_candidate_families"] if x["name"] == selected["name"]]) == 1
    assert selected["selection_basis"]
    assert data["runtime_invariants"]["new_recovery"] is False
    assert data["runtime_invariants"]["ownership_unchanged"] is True
    assert time.monotonic() - started < 120


def test_no_runtime_recovery_logic_added():
    source = (ROOT / "scripts/torresamat1835/remaining_glyph_reprioritization.py").read_text()
    tree = ast.parse(source)
    assert not any(isinstance(n, (ast.Assign, ast.AnnAssign)) and
                   isinstance(getattr(n, "value", None), ast.Call) and
                   getattr(getattr(n, "value", None).func, "id", None) == "recover"
                   for n in ast.walk(tree))


if __name__ == "__main__":
    test_task_140_contract()
    test_no_runtime_recovery_logic_added()
    print("ok")
