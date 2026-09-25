#!/usr/bin/env python3
"""Focused contract for task 151 (split two-digit markers, diagnostic)."""
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = ROOT / "scripts/torresamat1835/split_digit_marker_audit.py"
ART = ROOT / "data/torresamat1835/split_digit_marker_audit.json"


class _W:
    def __init__(self, text):
        self.text = text


def test_split_marker_shape():
    import split_digit_marker_audit as audit
    words = lambda *t: [_W(x) for x in t]
    assert audit.split_marker(words("1", "8", "-", "Libóme")) == (1, 8)
    assert audit.split_marker(words("1", "3", "Porque", "cuando")) == (1, 3)
    # A digit inside prose, a lone digit, or lower case after: not a marker.
    assert audit.split_marker(words("1", "8", "de", "ellos")) is None
    assert audit.split_marker(words("18", "Porque", "x")) is None
    assert audit.split_marker(words("0", "8", "Porque")) is None


def test_task_151_contract():
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out.json"
        subprocess.run(["python3", str(GEN), "--out", str(out)],
                       cwd=Path(__file__).parent, check=True,
                       stdout=subprocess.DEVNULL)
        assert out.read_bytes() == ART.read_bytes()
    data = json.loads(ART.read_text())
    assert data["family"]["population"] == 379
    assert data["classes"]["SPLIT_MARKER_READ_AS_FIRST_DIGIT"] == 379
    assert data["facsimile_sample"]["agreement"] == 8
    assert "p0032l0079" in data["family"]["blocks"]
    assert data["runtime_invariants"]["new_recovery"] is False


if __name__ == "__main__":
    test_split_marker_shape()
    test_task_151_contract()
    print("ok")
