#!/usr/bin/env python3
"""Focused contract for TORRES-1835 task 141."""
import ast
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / "data/torresamat1835/projected_form_a_facsimile.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
PDF = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf"
BASELINE = "ca3afefec401b3e94dad190de53144e66a9fc74b"
CLASSES = {
    "PRINTED_VERSE_MARKER", "ORDINARY_TEXT", "HEADING_OR_TITLE",
    "LATIN_PARALLEL_TEXT", "APPARATUS_OR_NOTE", "LAYOUT_OR_SCAN_ARTIFACT",
    "UNREADABLE", "OTHER",
}


def _audit(path):
    cached = ROOT / "build/torresamat1835-audit/volume3-task141.json"
    if cached.exists():
        try:
            if json.loads(cached.read_text(encoding="utf-8"))["verse_refs"] == 3849:
                shutil.copy2(cached, path)
                return
        except (OSError, KeyError, json.JSONDecodeError):
            pass
    subprocess.run([
        "python3", str(ROOT / "scripts/torresamat1835/audit_volume.py"),
        "--xml", str(XML), "--volume", "3",
        "--witness", "ia-lasagradabiblia01unkngoog", "--book", "Ps",
        "--without-projected-form-a-recovery",
        "--out", str(path)], cwd=ROOT, check=True,
        stdout=subprocess.DEVNULL)


def _artifact(audit, path):
    subprocess.run([
        "python3", str(ROOT / "scripts/torresamat1835/projected_form_a_facsimile.py"),
        "--audit", str(audit), "--out", str(path),
        "--baseline-commit", BASELINE, "--xml", str(XML), "--pdf", str(PDF),
    ], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)


def test_contract():
    with tempfile.TemporaryDirectory() as td:
        audit = Path(td) / "audit.json"
        generated_a = Path(td) / "a.json"
        generated_b = Path(td) / "b.json"
        _audit(audit)
        _artifact(audit, generated_a)
        _artifact(audit, generated_b)
        assert generated_a.read_bytes() == generated_b.read_bytes()
        assert generated_a.read_bytes() == ART.read_bytes()
        data = json.loads(generated_a.read_text(encoding="utf-8"))
        runtime = data["runtime_invariants"]

    reviews = data["occurrence_reviews"]
    assert data["schema_version"] == 1
    assert data["provenance"]["baseline_commit"] == BASELINE
    assert data["candidate_summary"]["total_candidates"] == 260
    assert data["candidate_summary"]["unique_occurrences"] == 260
    assert data["family_definition"]["diagnostic_exact_form"] == "a"
    assert data["family_definition"]["physical_single_token"] == 0
    assert data["family_definition"]["projected_from_multi_token"] == 260
    assert len(reviews) == len({r["stable_occurrence_id"] for r in reviews}) == 260
    assert data["candidate_count"] == data["accessible_count"] == data["reviewed_count"] == 260
    assert data["printed_marker_count"] == 238
    assert data["negative_count"] == 22
    assert data["unreadable_count"] == 0
    assert data["structural_subfamily_count"] == 2
    assert all(r["diagnostic_exact_form"] == "a" for r in reviews)
    assert all(r["physical_token_count"] > 1 for r in reviews)
    assert all(r["projected_token_position"] == 0 for r in reviews)
    assert all(r["evidence_state"] == "SOURCE_BACKED" for r in reviews)

    counts = data["class_counts"]
    assert set(counts) == CLASSES
    assert sum(counts.values()) == 260
    assert counts == {
        "PRINTED_VERSE_MARKER": 238, "ORDINARY_TEXT": 1,
        "HEADING_OR_TITLE": 0, "LATIN_PARALLEL_TEXT": 0,
        "APPARATUS_OR_NOTE": 21, "LAYOUT_OR_SCAN_ARTIFACT": 0,
        "UNREADABLE": 0, "OTHER": 0,
    }
    assert sum(data["visible_printed_value_counts"].values()) == 238
    assert all(r["visible_printed_value"] is not None
               for r in reviews if r["facsimile_review_class"] == "PRINTED_VERSE_MARKER")
    assert all(r["visible_printed_value"] is None
               for r in reviews if r["facsimile_review_class"] != "PRINTED_VERSE_MARKER")

    historical = data["historical_positive_reconciliation"]
    assert historical["stable_occurrence_id"].startswith("p0276l0033::")
    assert historical["historical_classification"] == "PRINTED_DIGIT"
    assert historical["current_classification"] == "PRINTED_VERSE_MARKER"
    assert historical["evidence_agrees"] is True
    assert data["selected_task142"]["outcome"] in {
        "BOUNDED_DISCRIMINATOR_VALIDATION", "NARROWER_FACSIMILE_AUDIT",
        "CLOSE_FORM_A_UNSAFE",
    }
    assert data["selected_task142"]["population"] > 0
    assert len(data["structural_subfamilies"]) >= 1
    assert all("page identity alone" not in " ".join(s.get("defining_measurable_features", []))
               for s in data["structural_subfamilies"])
    assert data["candidate_controls"]["same_form_structurally_outside_candidate_rule"]["population"] == 22
    assert runtime["expected_verse_used_as_value_authority"] is False
    assert runtime["previous_plus_one_used"] is False
    assert runtime["next_minus_one_used"] is False
    assert runtime["new_recovery"] is False
    assert runtime["runtime_pdf_or_image_reads"] is False
    assert runtime["verse_refs"] == 3849
    assert runtime["physical_gaps"] == 3254
    assert runtime["glyph_gaps"] == 1309
    assert runtime["chapters"] == 337
    assert runtime["unresolved_chapter_claims"] == 0
    assert runtime["canonical_chapter_gaps"] == 0
    assert runtime["duplicate_refs"] == runtime["out_of_order_refs"] == runtime["outside_canon"] == 0
    assert runtime["ocr_blocks"] == 57700
    assert runtime["block_loss"] == runtime["dual_ownership"] == 0
    assert runtime["task128"] == {"markers": 183, "refs": 177, "ownership_moves": 1355}
    assert runtime["task131"] == {"markers": 276, "refs": 276, "ownership_moves": 1900}
    assert runtime["GLUED_FRAME"] == "CLOSED_UNSAFE"

    source = (ROOT / "scripts/torresamat1835/projected_form_a_facsimile.py").read_text(encoding="utf-8")
    tree = ast.parse(source)
    assert not any(isinstance(n, (ast.Import, ast.ImportFrom)) and
                   any(getattr(a, "name", "") in {"torch", "tensorflow", "sklearn"}
                       for a in n.names) for n in ast.walk(tree))
    assert "raw_ocr_form" in source and "occurrence allowlist" in source


if __name__ == "__main__":
    test_contract()
    print("ok")
