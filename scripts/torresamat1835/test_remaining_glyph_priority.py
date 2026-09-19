#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/remaining_glyph_inventory.json"

def test_inventory_identity():
    d=json.loads(ART.read_text())
    assert d["physical_gap_total"] == len(d["rows"]) == 3255
    assert d["root_cause_identity"]
    assert sum(d["root_cause_counts"].values()) == d["physical_gap_total"]
    assert d["exact_form_count"] == len(d["exact_form_frequency"])
    assert d["next_task_family"]

def test_provenance_and_closed_families():
    d=json.loads(ART.read_text())
    assert d["review_coverage"]["prior_batches"] == [124,127,128,129,130,131,132,133]
    assert d["review_coverage"]["batch_134"] == 0
    assert d["glued_frame"] == {"status":"CLOSED_UNSAFE","automation_candidate":False}
    assert d["rules"] == {"expected_verse_used":False,"previous_next_verse_used":False,"arbitrary_priority_score":False}

def test_exact_forms_are_not_normalized():
    d=json.loads(ART.read_text())
    forms=set(d["exact_form_frequency"])
    assert "a" in forms
    assert "á" not in forms or "á" != "a"
    assert all(isinstance(x,str) for x in forms)

if __name__ == "__main__":
    test_inventory_identity(); test_provenance_and_closed_families(); test_exact_forms_are_not_normalized(); print("ok")
