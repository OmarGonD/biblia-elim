#!/usr/bin/env python3
"""Focused contract for task 152 (dry run of split two-digit markers)."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "scripts/torresamat1835/split_digit_marker_dry_run.py"
ART = ROOT / "data/torresamat1835/split_digit_marker_dry_run.json"


def test_task_152_contract():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out.json"
        subprocess.run(["python3", str(GEN), "--out", str(out)],
                       cwd=Path(__file__).parent, check=True,
                       stdout=subprocess.DEVNULL)
        assert out.read_bytes() == ART.read_bytes()
    data = json.loads(ART.read_text())
    assert data["outcomes"]["MOVE_TO_NEW_REF"] == 270
    assert data["outcomes"]["RELABEL_FALSE_REF"] == 90
    assert "ambiguous_source_event" not in data["outcomes"]
    delta = data["predicted_delta"]
    assert delta["created"] == 360 and delta["removed"] == 90
    assert delta["verse_refs_after"] == data["runtime_baseline"]["verse_refs"] + 270
    assert len(set(delta["created_refs"])) == 360
    sample = data["facsimile_sample"]
    assert sample["size"] == 32 and sample["agreement"] == 32
    for e in data["events"]:
        if e["outcome"] in ("MOVE_TO_NEW_REF", "RELABEL_FALSE_REF"):
            assert e["proposed_native_ref"].endswith(f".{e['value']}")
            assert e["prior_verse"] < e["value"]
    assert data["runtime_invariants"]["new_recovery"] is False


def test_guards_read_no_labels():
    source = GEN.read_text()
    simulate = source.split("def simulate", 1)[1].split("\ndef build", 1)[0]
    for word in ("SAMPLE_REVIEW", "gaps", "facsimile"):
        assert word not in simulate


if __name__ == "__main__":
    test_task_152_contract()
    test_guards_read_no_labels()
    print("ok")
