#!/usr/bin/env python3
"""Task 150: production R2X reproduces the task-149 dry run exactly."""
import json
from pathlib import Path

import audit_volume
import projected_form_a_recovery as rule

ROOT = Path(__file__).resolve().parents[2]
XML = str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
DRY = ROOT / "data/torresamat1835/projected_form_a_printed_2x_dry_run.json"


def test_match_2x_is_source_only():
    class W:
        def __init__(self, text):
            self.text = text

    class L:
        def __init__(self, *words):
            self.words = [W(w) for w in words]

    assert rule.match_2x(L("a", "I", "Y", "se")) == ("Y se", 21)
    assert rule.match_2x(L("a", "o", "porque")) == ("porque", 20)
    assert rule.match_2x(L("a", "4'", "No")) == ("No", 24)
    # Prose «a» and a framed «a» are not read as numbers.
    assert rule.match_2x(L("a", "los", "hijos")) == (None, None)
    assert rule.match_2x(L("•", "a", "4", "Por")) == (None, None)
    assert rule.match_2x(L("a", "I")) == (None, None)


def test_production_reproduces_task149():
    _edition, report = audit_volume.audit(XML, volume="3",
                                          witness="ia-lasagradabiblia01unkngoog",
                                          book="Ps",
                                          split_digit_recovery=False)
    seg = report["verse_segmentation_audit"]
    run = seg["projected_form_a_printed_2x_recovery"]
    dry = json.loads(DRY.read_text())["predicted_delta"]
    assert run["recovered_markers"] == 24
    assert run["created_ref_identities"] == dry["new_refs"]
    assert run["ownership_moves"] == dry["ownership_moves"] == 409
    assert run["reopened_refs"] == 0 and run["removed_refs"] == []
    assert run["block_loss"] == 0 and run["dual_ownership"] == 0
    assert run["physical_gaps_closed"] == dry["physical_gaps_closed"]
    assert run["physical_gaps_opened"] == [] and run["glyph_gaps_opened"] == []
    # 24 glyph gaps closed; 44 more stay physical gaps but lose the glyph
    # signal: their only evidence was the marker now consumed.
    assert set(dry["glyph_gaps_closed"]) <= set(run["glyph_gaps_closed"])
    assert len(run["glyph_gaps_closed"]) == 68
    physical = {g["key"] for g in seg["gaps"]}
    extra = set(run["glyph_gaps_closed"]) - set(dry["glyph_gaps_closed"])
    assert extra <= physical
    assert (run["verse_refs_before"], run["verse_refs_after"]) == (3892, 3916)
    assert report["verse_refs"] == 3916 and len(seg["gaps"]) == 3187
    assert seg["by_signal"]["lone_glyph_inside_previous_verse"] == 1198
    assert report["chapters"] == 337 and report["metrics"]["ocr_blocks"] == 57700
    assert report["duplicate_refs"] == [] and report["out_of_order_refs"] == []
    # Task 146 is untouched.
    t146 = seg["projected_form_a_refined_scope_recovery"]
    assert t146["created_refs"] == 43 and t146["ownership_moves"] == 581
    # No allowlist: the runtime module never names a block or a ref.
    source = Path(rule.__file__).read_text()
    for block in ("p0032l0089", "p0217l0057", "Ps.17.21"):
        assert block not in source


if __name__ == "__main__":
    test_match_2x_is_source_only()
    test_production_reproduces_task149()
    print("ok")
