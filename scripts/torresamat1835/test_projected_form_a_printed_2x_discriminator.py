#!/usr/bin/env python3
"""Focused contract for task 148 (discriminator validation, no recovery)."""
import ast
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "scripts/torresamat1835/projected_form_a_printed_2x_discriminator.py"
ART = ROOT / "data/torresamat1835/projected_form_a_printed_2x_discriminator.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"


def test_task_148_contract():
    with tempfile.TemporaryDirectory() as td:
        audit = Path(td) / "audit.json"
        out = Path(td) / "out.json"
        subprocess.run(["python3", str(ROOT / "scripts/torresamat1835/audit_volume.py"),
                        "--xml", str(XML), "--volume", "3", "--witness",
                        "ia-lasagradabiblia01unkngoog", "--book", "Ps",
                        "--without-printed-2x-recovery", "--out", str(audit)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        subprocess.run(["python3", str(GEN), "--audit", str(audit),
                        "--out", str(out)], cwd=ROOT, check=True,
                       stdout=subprocess.DEVNULL)
        assert out.read_bytes() == ART.read_bytes()
    data = json.loads(ART.read_text())
    assert data["runtime_baseline"] == {"verse_refs": 3892,
                                        "physical_gaps": 3211,
                                        "glyph_gaps": 1266}
    r2x = data["rules"]["R2X"]
    assert data["selected_rule"] == "R2X"
    assert r2x["family_true_positive"] == 23
    assert r2x["reviewed_non_marker_accepted"] == 0
    assert r2x["wrong_value_on_family"] == []
    assert r2x["other_gap_markers_accepted"] == ["p0032l0089"]
    assert len(r2x["family_false_negative"]) == 3
    # The provenance guard is what keeps owned markers out.
    assert data["rules"]["R2X_NO_PROVENANCE"]["by_label"]["UNREVIEWED"] > 200
    inv = data["runtime_invariants"]
    assert inv["new_recovery"] is False and inv["gap_key_used_by_rule"] is False


def test_rule_does_not_read_labels_or_recover():
    tree = ast.parse(GEN.read_text())
    imported = {a.name for n in ast.walk(tree)
                if isinstance(n, (ast.Import, ast.ImportFrom)) for a in n.names}
    assert not imported & {"recovery", "parser", "page_parser"}
    rules_src = GEN.read_text().split("RULES = {", 1)[1].split("\n}\n", 1)[0]
    for label in ("members", "reviews", "visible_printed_value", "key"):
        assert label not in rules_src


if __name__ == "__main__":
    test_task_148_contract()
    test_rule_does_not_read_labels_or_recover()
    print("ok")
