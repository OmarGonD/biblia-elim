#!/usr/bin/env python3
"""Tasks 154-156: glued two-character markers («i3» -> 13)."""
import json
import subprocess
import tempfile
from pathlib import Path

import audit_volume
import glued_two_char_marker_audit as diag
import split_digit_recovery as rule

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).parent
XML = str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
AUDIT = ROOT / "data/torresamat1835/glued_two_char_marker_audit.json"
DRY = ROOT / "data/torresamat1835/glued_two_char_marker_dry_run.json"


class _W:
    def __init__(self, text):
        self.text = text


class _L:
    def __init__(self, *words):
        self.words = [_W(w) for w in words]


def test_matcher():
    assert rule.match_glued(_L("i3", "Porque")) == ("Porque", 13)
    assert rule.match_glued(_L("ao", "Bendecid", "al")) == ("Bendecid al", 20)
    assert rule.match_glued(_L("3a", "Será")) == ("Será", 32)
    assert rule.match_glued(_L("II", "Yo")) == ("Yo", 11)
    # Spanish words, ambiguous glyphs, pure digits, prose: never.
    for words in (("la", "Casa"), ("al", "Señor"), ("iS", "Pero"),
                  ("99", "Porque"), ("i3", "de"), ("oi", "El")):
        assert rule.match_glued(_L(*words)) == (None, None), words
    # Diagnostic and runtime matchers agree.
    for words in (("i3", "Porque"), ("la", "Casa"), ("a6", "El")):
        got = rule.match_glued(_L(*words))[1]
        assert got == diag.glued_value(_L(*words).words)


def _regen(script, art):
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out.json"
        subprocess.run(["python3", script, "--out", str(out)], cwd=HERE,
                       check=True, stdout=subprocess.DEVNULL)
        assert out.read_bytes() == art.read_bytes()


def test_audit_and_dry_run_artifacts():
    _regen("glued_two_char_marker_audit.py", AUDIT)
    _regen("glued_two_char_marker_dry_run.py", DRY)
    audit = json.loads(AUDIT.read_text())
    assert audit["family_population"] == 670
    assert audit["facsimile_sample"]["size"] == 32
    assert audit["facsimile_sample"]["agreement"] == 32
    dry = json.loads(DRY.read_text())
    assert dry["predicted_delta"]["created"] == 524
    assert dry["predicted_delta"]["removed"] == 0


def test_production_reproduces_task155():
    _edition, report = audit_volume.audit(
        XML, volume="3", witness="ia-lasagradabiblia01unkngoog", book="Ps")
    seg = report["verse_segmentation_audit"]
    run = seg["glued_two_char_marker_recovery"]
    dry = json.loads(DRY.read_text())["predicted_delta"]
    assert run["applied"] == 524
    assert set(run["created_ref_identities"]) == set(dry["created_refs"])
    assert run["removed_ref_identities"] == []
    assert run["block_loss"] == 0 and run["dual_ownership"] == 0
    assert run["blocks_entering_text"] == 0
    assert (run["verse_refs_before"], run["verse_refs_after"]) == (4186, 4710)
    assert (run["physical_gaps_before"], run["physical_gaps_after"]) == (2917, 2393)
    assert run["physical_gaps_opened"] == []
    assert seg["split_two_digit_marker_recovery"]["applied"] == 360
    assert report["chapters"] == 337 and report["metrics"]["ocr_blocks"] == 57700
    assert report["duplicate_refs"] == [] and report["out_of_order_refs"] == []


if __name__ == "__main__":
    test_matcher()
    test_audit_and_dry_run_artifacts()
    test_production_reproduces_task155()
    print("ok")
