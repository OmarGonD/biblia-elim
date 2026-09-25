#!/usr/bin/env python3
"""Focused contract for task 149 (dry run of R2X, nothing applied)."""
import ast
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "scripts/torresamat1835/projected_form_a_printed_2x_dry_run.py"
ART = ROOT / "data/torresamat1835/projected_form_a_printed_2x_dry_run.json"


def test_task_149_contract():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out.json"
        subprocess.run(["python3", str(GEN), "--out", str(out)],
                       cwd=Path(__file__).parent, check=True,
                       stdout=subprocess.DEVNULL)
        assert out.read_bytes() == ART.read_bytes()
    data = json.loads(ART.read_text())
    base = data["runtime_baseline"]
    assert (base["verse_refs"], base["physical_gaps"], base["glyph_gaps"],
            base["chapters"], base["ocr_blocks"]) == (3892, 3211, 1266, 337,
                                                      57700)
    assert data["outcomes"] == {"CREATE_NEW_REF": 24}
    delta = data["predicted_delta"]
    assert delta["CREATE_NEW_REF"] == 24 and delta["REOPEN_EXISTING_REF"] == 0
    assert delta["verse_refs_after"] == 3916
    assert len(delta["glyph_gaps_closed"]) == 24
    assert delta["prior_owner_emptied"] == 0
    rec = data["label_reconciliation"]
    assert rec["created_family_members"] == 23
    assert rec["created_outside_family"] == ["p0032l0089"]
    assert rec["created_value_equals_facsimile"] is True
    assert rec["created_ref_equals_task147_gap"] is True
    for e in data["events"]:
        assert e["blocks_remaining_with_prior_owner"] >= 1
        assert e["proposed_native_ref"].endswith(f".{e['source_reading']}")
    assert data["runtime_invariants"]["new_recovery"] is False


def test_guards_do_not_read_labels():
    source = GEN.read_text()
    simulate = source.split("def simulate", 1)[1].split("\ndef build", 1)[0]
    for label in ("members", "facsimile", "task147", "gaps"):
        assert label not in simulate
    tree = ast.parse(source)
    imported = {a.name for n in ast.walk(tree)
                if isinstance(n, (ast.Import, ast.ImportFrom)) for a in n.names}
    assert "apply" not in imported


if __name__ == "__main__":
    test_task_149_contract()
    test_guards_do_not_read_labels()
    print("ok")
