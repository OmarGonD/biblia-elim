"""Regression tests for diagnostic task 133."""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glued_marker_discriminator as d

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ARTIFACT = os.path.join(ROOT, "data", "torresamat1835", "glued_marker_discriminator.json")
SEGMENTS = os.path.join(ROOT, "data", "torresamat1835", "glued_marker_segments.json")

def payload():
    return json.load(open(ARTIFACT, encoding="utf-8"))

def test_A_source_and_segmentation_provenance():
    p = payload(); s = json.load(open(SEGMENTS, encoding="utf-8"))
    assert p["source_provenance"]["facsimile_sha256"] == s["facsimile"]["sha256"]
    assert p["source_provenance"]["ocr_sha256"] == s["source_ocr"]["sha256"]
    assert p["segmentation_version"] == s["segmentation"]["version"]

def test_B_label_population_and_required_hard_negatives():
    p = payload(); c = p["label_counts"]
    assert c["TRUE_MARKER"] >= 8 and c["ORDINARY_TEXT"] >= 8
    assert c["APPARATUS"] >= 1 and c["LATIN"] >= 1
    blocks = {r["block"] for r in p["labels"]}
    assert {"p0034l0039", "p0049l0075", "p0060l0070", "p0115l0053",
            "p0222l0012", "p0257l0052", "p0335l0093", "p0593l0086"} <= blocks

def test_C_features_exclude_sequence_and_raw_form_inputs():
    p = payload()
    forbidden = ("expected", "previous", "next", "canonical", "gap", "verse")
    for row in p["labels"]:
        assert set(row["features"]) <= set(p["feature_definitions"])
        assert not any(k in row["features"] for k in forbidden)
    assert "raw_ocr" in p["labels"][0]  # provenance only, never feature input

def test_D_zero_negative_gate_and_explicit_abstention():
    p = payload(); e = p["evaluation"]
    assert p["selected_rule_or_none"] == "NONE"
    assert e["false_positive"] == 0
    assert e["abstentions"] == len(p["labels"])
    assert p["future_recovery_readiness"] == "UNSAFE_TO_AUTOMATE"

def test_E_no_ml_or_runtime_integration():
    source = open(os.path.join(os.path.dirname(__file__), "glued_marker_discriminator.py"), encoding="utf-8").read()
    assert not any(x in source for x in ("sklearn", "RandomForest", "LogisticRegression", "SVC", "KNeighbors"))
    parser = open(os.path.join(os.path.dirname(__file__), "parser.py"), encoding="utf-8").read()
    assert "glued_marker_discriminator" not in parser

def test_F_deterministic_regeneration():
    first = json.dumps(payload(), ensure_ascii=False, sort_keys=True)
    d.generate()
    second = json.dumps(payload(), ensure_ascii=False, sort_keys=True)
    assert first == second

if __name__ == "__main__":
    for name, value in sorted(globals().items()):
        if name.startswith("test_"):
            value()
    print("glued_marker_discriminator_failures=0 tests=6")
