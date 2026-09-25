#!/usr/bin/env python3
"""Task 153: production split two-digit recovery reproduces task 152."""
import json
from pathlib import Path

import audit_volume
import split_digit_recovery as rule

ROOT = Path(__file__).resolve().parents[2]
XML = str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
DRY = ROOT / "data/torresamat1835/split_digit_marker_dry_run.json"


class _W:
    def __init__(self, text):
        self.text = text


class _L:
    def __init__(self, *words):
        self.words = [_W(w) for w in words]


def test_match_is_source_only():
    assert rule.match(_L("1", "8", "-", "Libóme")) == ("- Libóme", 18)
    assert rule.match(_L("7", "1", "Bien", "me")) == ("Bien me", 71)
    assert rule.match(_L("1", "8", "de", "ellos")) == (None, None)
    assert rule.match(_L("0", "8", "Porque")) == (None, None)
    assert rule.match(_L("18", "Porque", "x")) == (None, None)


def test_production_reproduces_task152():
    # The task-153 contract is the production state before task 156.
    _edition, report = audit_volume.audit(
        XML, volume="3", witness="ia-lasagradabiblia01unkngoog", book="Ps",
        glued_marker_recovery=False)
    seg = report["verse_segmentation_audit"]
    run = seg["split_two_digit_marker_recovery"]
    dry = json.loads(DRY.read_text())
    assert run["applied"] == 360
    assert run["outcomes"]["moved_to_new_ref"] == dry["outcomes"]["MOVE_TO_NEW_REF"]
    assert run["outcomes"]["relabelled_false_ref"] == dry["outcomes"]["RELABEL_FALSE_REF"]
    created = set(dry["predicted_delta"]["created_refs"])
    removed = set(dry["predicted_delta"]["relabelled_false_refs"])
    assert set(run["created_ref_identities"]) == created - removed
    assert set(run["removed_ref_identities"]) == removed - created
    assert run["block_loss"] == 0 and run["dual_ownership"] == 0
    assert run["blocks_entering_text"] == 0
    assert (run["verse_refs_before"], run["verse_refs_after"]) == (3916, 4186)
    assert (run["physical_gaps_before"], run["physical_gaps_after"]) == (3187, 2917)
    # The only gaps opened are the false verses that disappear.
    assert set(run["physical_gaps_opened"]) == removed - created
    assert report["chapters"] == 337 and report["metrics"]["ocr_blocks"] == 57700
    assert report["duplicate_refs"] == [] and report["out_of_order_refs"] == []
    assert seg["projected_form_a_printed_2x_recovery"]["recovered_markers"] == 24
    source = Path(rule.__file__).read_text()
    for token in ("p0032l0079", "Isa.7", "SAMPLE"):
        assert token not in source


if __name__ == "__main__":
    test_match_is_source_only()
    test_production_reproduces_task152()
    print("ok")
