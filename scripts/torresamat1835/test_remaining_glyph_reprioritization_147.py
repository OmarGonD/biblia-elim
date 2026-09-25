#!/usr/bin/env python3
"""Focused regression contract for task 147 (diagnostic, no recovery)."""
import ast
import json
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/remaining_glyph_reprioritization_147.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
GEN = ROOT / "scripts/torresamat1835/remaining_glyph_reprioritization_147.py"
BASELINE = "e7dedfc80cc1f8e78afdd3e93d19a0236f9357aa"


def _make_audit(path):
    # Production mode: the task-146 recovery stays enabled.
    subprocess.run(["python3", str(ROOT / "scripts/torresamat1835/audit_volume.py"),
                    "--xml", str(XML), "--volume", "3", "--witness",
                    "ia-lasagradabiblia01unkngoog", "--book", "Ps",
                    "--without-printed-2x-recovery", "--out", str(path)],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)


def _make_artifact(audit, output):
    subprocess.run(["python3", str(GEN), "--audit", str(audit),
                    "--out", str(output), "--baseline-commit", BASELINE],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)


def test_task_147_contract():
    started = time.monotonic()
    with tempfile.TemporaryDirectory() as td:
        audit_path = Path(td) / "audit.json"
        one, two = Path(td) / "one.json", Path(td) / "two.json"
        _make_audit(audit_path)
        _make_artifact(audit_path, one)
        _make_artifact(audit_path, two)
        audit = json.loads(audit_path.read_text())
        data = json.loads(one.read_text())
        assert one.read_bytes() == two.read_bytes()
        assert one.read_bytes() == ART.read_bytes()

    # Runtime unchanged by this task: task-146 state.
    seg = audit["verse_segmentation_audit"]
    assert audit["verse_refs"] == 3892
    assert len(seg["gaps"]) == 3211
    assert seg["by_signal"]["lone_glyph_inside_previous_verse"] == 1266
    assert audit["chapters"] == 337 and audit["chapter_claims"]["unresolved"] == 0
    assert not audit["canonical_chapter_gap_reviews"]["canonical_missing"]
    assert audit["metrics"]["ocr_blocks"] == 57700
    assert audit["duplicate_refs"] == [] and audit["out_of_order_refs"] == []
    assert audit.get("outside_canon", []) == []
    integrated = seg["remaining_glyph_reprioritization_147"]
    assert integrated["selected_family"] == "PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH"
    assert integrated["selected_population"] == 26

    assert data["schema_version"] == 1
    assert data["provenance"]["frozen_baseline_commit"] == BASELINE
    base = data["runtime_baseline"]
    assert (base["verse_refs"], base["physical_gaps"], base["glyph_gaps"]) == \
        (3892, 3211, 1266)

    # 1309 -> 1266: exactly the 43 task-146 CREATE_NEW_REF identities.
    rec = data["historical_reconciliation"]
    assert rec["historical_total"] == 1309 and rec["current_total"] == 1266
    assert len(rec["removed_occurrences"]) == 43
    assert rec["removed_equal_task146_create_new_ref"] is True
    assert rec["added_occurrences"] == []
    rows = data["current_inventory_summary"]["rows"]
    assert len(rows) == 1266
    assert data["physical_projected_split"] == {
        "PHYSICAL_SINGLE_TOKEN": 6, "PROJECTED_FROM_MULTI_TOKEN": 1260}

    closed = data["closed_family_status"]
    assert closed["GLUED_FRAME"] == "CLOSED_UNSAFE"
    assert closed["outside_marker_band"] == "CLOSED_REJECTED"
    assert closed["projected_form_a_printed_2"] == "SOLVED_TASK_146_REFINED_SCOPE"
    excluded = data["excluded_populations"]
    assert excluded["task144_external_unreadable"] == 111
    assert excluded["task144_known_order_conflicts"] == 13
    assert excluded["task146_out_of_band_block"] == "p0184l0043"

    selected = data["selected_task148_family"]
    assert selected["name"] == "PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH"
    assert selected["population"] == 26 and selected["blocks"] == 26
    assert selected["population"] < 1266
    members = selected["members"]
    assert len([r for r in rows if r["selected_family_member"]]) == 26
    for m in members:
        # Facsimile value and the two digits read in the source agree.
        first, second = m["source_two_digit_reading"]
        assert first * 10 + second == m["visible_printed_value"]
        assert m["source_tokens"][0] == "a"
        assert m["ocr_block"] != excluded["task146_out_of_band_block"]
    assert selected["controls_outside_family"]["printed_2x_value_other_gap"] > 0
    assert len([f for f in data["bounded_candidate_families"]
                if f["name"] == selected["name"]]) == 1

    inv = data["runtime_invariants"]
    assert inv["new_recovery"] is False and inv["ownership_unchanged"] is True
    assert inv["expected_verse_used"] is False
    assert inv["previous_plus_one_used"] is False
    assert inv["next_minus_one_used"] is False
    assert time.monotonic() - started < 300


def test_no_runtime_recovery_logic_added():
    tree = ast.parse(GEN.read_text())
    imported = {a.name for n in ast.walk(tree)
                if isinstance(n, (ast.Import, ast.ImportFrom))
                for a in n.names}
    assert not imported & {"recovery", "projected_form_a_recovery", "parser",
                           "page_parser"}


if __name__ == "__main__":
    test_task_147_contract()
    test_no_runtime_recovery_logic_added()
    print("ok")
