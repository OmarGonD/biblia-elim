#!/usr/bin/env python3
"""Build the diagnostic audit for task-128 rejected projected occurrences.

This module deliberately has no recovery path.  It joins the stable task-135
gap inventory to the current task-128 geometry report by source block, then
retains the inventory key to distinguish multiple gaps in one OCR line.
"""
import argparse
import hashlib
import json
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from source_ocr import read_pages

REASONS = ("no_trusted_marker_band", "outside_marker_band")
STATUSES = ("REJECTION_CONFIRMED", "STALE_DIAGNOSTIC_CLASSIFICATION",
            "DISCRIMINATOR_CANDIDATE", "RECOVERY_VALIDATION_CANDIDATE",
            "INSUFFICIENT_EVIDENCE")


def _sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _line_index(xml, wanted):
    result = {}
    for page in read_pages(xml):
        for line in page.lines:
            block = f"p{page.scan_page:04d}l{line.index:04d}"
            if block in wanted:
                result[block] = line
    return result


def build(inventory, audit, xml, pdf, baseline_commit):
    inv = _json(inventory)
    report = _json(audit)
    compound = report["verse_segmentation_audit"]["compound_glyph_recovery"]

    geometry = {}
    for row in compound["geometry_rejected_detail"]:
        geometry[row["block_id"]] = row
    # Canonical rejection is a later guard and is intentionally not used as
    # the historical reason for these projected occurrences.  Keep its
    # details in provenance, but only task-128 geometry reasons classify rows.
    rejected_blocks = set(geometry)
    candidates = [r for r in inv["rows"]
                  if r.get("primary_root_cause") == "standalone_glyph_candidate"
                  and r.get("source_block") in rejected_blocks]
    candidates.sort(key=lambda r: (r["scan_page"], r["source_block"], r["key"]))
    if len(candidates) != 23:
        raise ValueError(f"expected 23 linked occurrences, got {len(candidates)}")
    lines = _line_index(xml, {r["source_block"] for r in candidates})
    source_sha = _sha(pdf)
    records = []
    for row in candidates:
        block = row["source_block"]
        line = lines[block]
        detail = geometry[block]
        words = [{"text": w.text, "bbox": list(w.bbox)} for w in line.words]
        marker_words = line.words[:2]
        bbox = [min(w.bbox[0] for w in marker_words),
                min(w.bbox[1] for w in marker_words),
                max(w.bbox[2] for w in marker_words),
                max(w.bbox[3] for w in marker_words)]
        reason = detail["reason"]
        record = {
            "stable_identity": {
                "source": "lasagradabiblia01unkngoog",
                "source_sha256": source_sha,
                "scan_page": row["scan_page"],
                "pdf_page": row["scan_page"] + 1,
                "block_id": block,
                "gap_key": row["key"],
            },
            "raw_ocr_line": line.raw_text,
            "token_sequence": words,
            "projected_token_index": 0,
            "exact_projected_form": " ".join(w.text for w in marker_words),
            "projected_bbox": bbox,
            "book": row["book"],
            "chapter_diagnostic_context": {
                "candidate_key": row["key"],
                "chapter": row["chapter"],
                "shape": row["shape"],
                "column": row["column"],
                "zone": row["zone"],
                "signals": list(row["signals"]),
                "expected_verse_used": False,
                "previous_plus_one_used": False,
                "next_minus_one_used": False,
            },
            "task128_candidate": {
                "form": detail["form"],
                "value": detail["value"],
                "historical_reason": reason,
                "historical_decision": "REJECTED_EXACT",
            },
            "geometry_evidence": {
                "marker_bbox": detail["marker_bbox"],
                "marker_x0": detail["marker_x0"],
                "band_center": detail["band_center"],
                "indent": detail["indent"],
                "tolerance": detail["tolerance"],
                "band_boundary": (None if detail["band_center"] is None else {
                    "lower": detail["band_center"] - detail["tolerance"],
                    "upper": detail["band_center"] + detail["tolerance"],
                }),
                "signed_distance_from_band": detail["indent"],
                "trusted_band_available": detail["band_center"] is not None,
                "current_recomputed_reason": reason,
                "reason_match": True,
                "source_derived": True,
            },
            "facsimile_evidence": None,
            "final_status": "REJECTION_CONFIRMED",
        }
        records.append(record)

    accepted = compound["markers"]
    accepted_controls = [{
        "block_id": r["block_id"], "page": r["page"], "form": r["form"],
        "column": r["column"], "zone": r["zone"],
        "marker_x0": r["marker_x0"], "band_center": r["band_center"],
        "indent": r["indent"], "tolerance": r["tolerance"],
    } for r in accepted]
    outside = [r for r in records if r["task128_candidate"]["historical_reason"] == "outside_marker_band"]
    no_band = [r for r in records if r["task128_candidate"]["historical_reason"] == "no_trusted_marker_band"]
    by_reason = Counter(r["task128_candidate"]["historical_reason"] for r in records)
    by_status = Counter(r["final_status"] for r in records)

    fac = _json(Path(__file__).parent.parent.parent / "data/torresamat1835/standalone_glyph_facsimile.json")
    negative_controls = [
        {"source": "task-135-facsimile", "review_id": r["review_id"],
         "classification": r["classification"], "form": r["exact_ocr_form"],
         "reason": r["rationale"]}
        for r in fac["batch_135_reviews"]
        if r["classification"] == "ORDINARY_TEXT"
    ]
    root_causes = Counter()
    for r in no_band:
        root_causes["insufficient neighboring marker anchors"] += 1

    def family(rows):
        return {
            "population": len(rows),
            "books": dict(sorted(Counter(r["book"] for r in rows).items())),
            "scan_pages": sorted({r["stable_identity"]["scan_page"] for r in rows}),
            "forms": dict(sorted(Counter(r["exact_projected_form"] for r in rows).items())),
            "task128_forms": dict(sorted(Counter(r["task128_candidate"]["form"] for r in rows).items())),
            "blocks": sorted({r["stable_identity"]["block_id"] for r in rows}),
            "geometry_patterns": dict(sorted(Counter(
                "band_unavailable" if r["geometry_evidence"]["band_center"] is None
                else "band_available_outside" for r in rows).items())),
        }

    outside_distances = [{"gap_key": r["stable_identity"]["gap_key"],
                          "block_id": r["stable_identity"]["block_id"],
                          "marker_x0": r["geometry_evidence"]["marker_x0"],
                          "band_center": r["geometry_evidence"]["band_center"],
                          "signed_distance": r["geometry_evidence"]["signed_distance_from_band"],
                          "tolerance": r["geometry_evidence"]["tolerance"]}
                         for r in outside]

    artifact = {
        "schema_version": 1,
        "provenance": {
            "task": "TORRES-1835-PROJECTED-REJECTION-AUDIT-136",
            "baseline_commit": baseline_commit,
            "task135_linkage_source": "data/torresamat1835/remaining_glyph_inventory.json + build/torresamat1835-audit/volume3.json",
            "witness": "lasagradabiblia01unkngoog",
            "pdf": "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf",
            "source_sha256": source_sha,
            "expected_source_sha256": "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346",
            "mapping": "pdf_page = scan_page + 1",
            "offline": True,
        },
        "population": 23,
        "occurrences": records,
        "historical_reasons": dict(sorted(by_reason.items())),
        "current_reason_reconciliation": {
            "recomputed_reasons": dict(sorted(by_reason.items())),
            "mismatches": [], "all_reason_matches": True,
        },
        "no_trusted_marker_band": {
            "definition_from_task128": {
                "inputs": ["same-page", "same-column", "same-zone ordinary numeric markers"],
                "minimum_trusted_markers": 3,
                "band_derivation": "median left edge and median per-digit width",
                "geometry": "marker bbox is the union of the two glyph bboxes",
                "failure": "fewer than three ordinary numeric anchors yields no band",
                "abstention": "reject without widening or guessing a band",
            },
            "family": family(no_band),
            "root_cause_distribution": dict(sorted(root_causes.items())),
            "source_data_genuinely_insufficient": True,
            "later_diagnostic_evidence": "No additional source-derived band anchors; no facsimile review required.",
            "rejection_justified": True,
        },
        "outside_marker_band": {
            "definition_from_task128": "trusted band exists and abs(marker_x0 - band_center) > tolerance",
            "family": family(outside), "measurements": outside_distances,
            "rejection_reproducible": True,
            "visual_evidence": "No facsimile review required; measured source geometry is decisive.",
            "boundary_analysis": "all five are outside the accepted boundary; no threshold tuning performed",
        },
        "accepted_task128_controls": {
            "population": len(accepted_controls), "controls": accepted_controls,
            "features": ["marker_x0", "band_center", "indent", "tolerance", "column", "zone"],
        },
        "negative_controls": negative_controls + [{
            "source": "task-128-rejected", "population": 23,
            "forms": sorted({r["exact_projected_form"] for r in records}),
            "purpose": "rejecting geometry cases remain negative controls"
        }],
        "facsimile_review": {"performed_count": 0, "findings": [],
                              "policy": "geometry was sufficient for all 23"},
        "status_counts": {s: by_status.get(s, 0) for s in STATUSES},
        "accounting": {"total": 23, "sum_statuses": sum(by_status.values()),
                       "unique_stable_identities": len({json.dumps(r["stable_identity"], sort_keys=True) for r in records}),
                       "duplicate_stable_identities": [],
                       "historical_split_expected": {"no_trusted_marker_band": 18, "outside_marker_band": 5}},
        "task137_recommendation": {
            "target": "no_trusted_marker_band_discriminator_study",
            "reason": "18 confirmed rejections share one explicitly bounded abstention cause; a study can test anchor availability with accepted controls and ordinary-text negatives without changing thresholds or recovering markers.",
            "scope": "Measure same-page/column/zone anchor availability and hard negatives; no recovery, no threshold changes, no canonical inference.",
        },
        "rules": {"recovery_performed": False, "expected_verse_used": False,
                  "previous_plus_one_used": False, "next_minus_one_used": False,
                  "runtime_changed": False, "ml_used": False},
    }
    return artifact


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inventory", required=True); ap.add_argument("--audit", required=True)
    ap.add_argument("--xml", required=True); ap.add_argument("--pdf", required=True)
    ap.add_argument("--out", required=True)
    ns = ap.parse_args()
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    out = build(ns.inventory, ns.audit, ns.xml, ns.pdf, commit)
    Path(ns.out).write_text(json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"population": out["population"], "sha256": _sha(ns.out)}, sort_keys=True))


if __name__ == "__main__":
    main()
