import hashlib
import json
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/projected_rejection_audit.json"


def audit():
    return json.loads(ART.read_text(encoding="utf-8"))


def test_exact_inventory_and_reasons():
    d = audit(); rows = d["occurrences"]
    assert len(rows) == d["population"] == 23
    ids = [json.dumps(r["stable_identity"], sort_keys=True) for r in rows]
    assert len(set(ids)) == 23
    assert d["accounting"]["unique_stable_identities"] == 23
    assert d["historical_reasons"] == {"no_trusted_marker_band": 18, "outside_marker_band": 5}
    assert d["current_reason_reconciliation"]["mismatches"] == []
    assert d["current_reason_reconciliation"]["all_reason_matches"]
    assert all(r["task128_candidate"]["historical_decision"] == "REJECTED_EXACT" for r in rows)
    assert not any(r["task128_candidate"]["historical_decision"] == "HANDLED_EXACT" for r in rows)


def test_stable_identity_geometry_and_no_sequence_linkage():
    d = audit()
    for r in d["occurrences"]:
        assert r["stable_identity"]["source_sha256"] == d["provenance"]["source_sha256"]
        assert r["stable_identity"]["pdf_page"] == r["stable_identity"]["scan_page"] + 1
        assert r["raw_ocr_line"] and r["token_sequence"]
        assert r["projected_token_index"] == 0
        assert r["geometry_evidence"]["source_derived"]
        ctx = r["chapter_diagnostic_context"]
        assert not ctx["expected_verse_used"]
        assert not ctx["previous_plus_one_used"]
        assert not ctx["next_minus_one_used"]


def test_one_final_status_and_task137_target():
    d = audit(); allowed = {"REJECTION_CONFIRMED", "STALE_DIAGNOSTIC_CLASSIFICATION",
                            "DISCRIMINATOR_CANDIDATE", "RECOVERY_VALIDATION_CANDIDATE",
                            "INSUFFICIENT_EVIDENCE"}
    assert all(r["final_status"] in allowed for r in d["occurrences"])
    assert sum(d["status_counts"].values()) == 23
    assert len(d["task137_recommendation"]["target"]) > 0
    assert d["rules"]["recovery_performed"] is False


def test_deterministic_and_idempotent_generation():
    args = ["python3", str(ROOT / "scripts/torresamat1835/projected_rejection_audit.py"),
            "--inventory", str(ROOT / "data/torresamat1835/remaining_glyph_inventory.json"),
            "--audit", str(ROOT / "build/torresamat1835-audit/volume3.json"),
            "--xml", str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"),
            "--pdf", str(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf"),
            "--out"]
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        a = pathlib.Path(td) / "a.json"; b = pathlib.Path(td) / "b.json"
        subprocess.run(args + [str(a)], cwd=ROOT, check=True, capture_output=True)
        subprocess.run(args + [str(b)], cwd=ROOT, check=True, capture_output=True)
        assert a.read_bytes() == b.read_bytes() == ART.read_bytes()
        assert hashlib.sha256(a.read_bytes()).hexdigest() == hashlib.sha256(b.read_bytes()).hexdigest()
