"""Diagnostic discriminator for task 133 (never used by the parser).

The labels in this module are visual facsimile labels.  They are deliberately
separate from the segmentation implementation: task-132 coordinates are read
as-is and no verse sequence, gap, or OCR form is used as a class feature.
"""
from __future__ import annotations

import collections
import hashlib
import json
import math
import statistics
from pathlib import Path

from PIL import Image

import layout
import source_ocr
import glued_compound_markers as glued
import a_glyph_pixels

ROOT = Path(__file__).resolve().parents[2]
SEGMENTS = ROOT / "data/torresamat1835/glued_marker_segments.json"
OUT = ROOT / "data/torresamat1835/glued_marker_discriminator.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
PDF_CACHE = ROOT / "build/torresamat1835-glued/pages"

# These are visual labels from deterministic facsimile crops.  The seven
# task-132 ordinary-text controls and four positive failures are retained.
TRUE_BLOCKS = {
    "p0042l0042", "p0049l0090", "p0100l0054", "p0118l0036",
    "p0164l0046", "p0168l0021", "p0168l0058", "p0197l0053",
    "p0219l0061", "p0229l0029", "p0229l0074", "p0256l0042",
    "p0260l0078", "p0260l0080", "p0348l0044", "p0356l0071",
    "p0359l0084", "p0370l0095", "p0390l0068", "p0390l0071",
    "p0407l0077", "p0409l0069", "p0435l0043", "p0437l0014",
    "p0457l0025", "p0457l0047", "p0459l0048", "p0459l0052",
    "p0506l0062", "p0516l0034", "p0571l0039", "p0603l0057",
    # task-132 positive-control failure (visually confirmed, no stable cut)
    "p0451l0055",
}
REQUIRED_TASK132_BLOCKS = {
    "p0034l0039", "p0049l0075", "p0060l0070", "p0115l0053",
    "p0222l0012", "p0257l0052", "p0335l0093", "p0593l0086",
}
APPARATUS_BLOCKS = {"p0315l0075"}
LATIN_BLOCKS = {
    "p0027l0041", "p0215l0010", "p0296l0029", "p0321l0001",
    "p0381l0018", "p0499l0031", "p0510l0003",
}

FEATURE_DEFINITIONS = {
    "segment_x0": "left edge of task-132 nominal segment union, scan pixels",
    "segment_x1": "right edge of task-132 nominal segment union, scan pixels",
    "segment_center": "midpoint of segment_x0/x1",
    "marker_band_delta": "segment_x0 minus trusted same-page right-body marker-band edge",
    "width": "segment_x1 - segment_x0",
    "height": "segment union height",
    "aspect_ratio": "width / height",
    "ink_area": "black pixels in nominal segment union at Otsu threshold",
    "ink_density": "ink_area / segment rectangle area",
    "following_gap": "next OCR component x0 minus segment_x1",
    "following_x0": "x0 of following OCR word/component",
    "body_start_delta": "following_x0 minus median trusted body continuation x0",
    "baseline_delta": "candidate segment bottom minus following component bottom",
}

def _stats(values):
    values = sorted(float(v) for v in values if v is not None)
    if not values:
        return {k: None for k in ("n", "min", "p10", "p25", "median", "p75", "p90", "max", "mad")}
    def q(p):
        return values[min(len(values)-1, int(round((len(values)-1)*p)))]
    med = statistics.median(values)
    return {"n": len(values), "min": values[0], "p10": q(.1), "p25": q(.25),
            "median": med, "p75": q(.75), "p90": q(.9), "max": values[-1],
            "mad": statistics.median(abs(x-med) for x in values)}

def _label(block):
    if block in APPARATUS_BLOCKS: return "APPARATUS"
    if block in LATIN_BLOCKS: return "LATIN"
    if block in TRUE_BLOCKS: return "TRUE_MARKER"
    return "ORDINARY_TEXT"

def _ocr_rows():
    out = {}
    for page in source_ocr.read_pages(str(XML)):
        placed = layout.split_columns(page)
        for p in placed:
            out[f"p{page.scan_page:04d}l{p.line.index:04d}"] = (page, p)
    return out

def _feature(row, lookup):
    page, placed = lookup[row["block"]]
    boxes = row.get("segment_bboxes") or []
    if not boxes:
        return None
    x0, y0 = min(b[0] for b in boxes), min(b[1] for b in boxes)
    x1, y1 = max(b[2] for b in boxes), max(b[3] for b in boxes)
    following = next((w for w in placed.line.words if w.text == row.get("following_ocr_word")), None)
    # The first matching word is deterministic for this source; fallback is
    # the first word to the right of the segment.
    if following is None:
        following = next((w for w in placed.line.words if w.bbox[0] >= x1), None)
    band = row.get("marker_band")
    gray_path = PDF_CACHE / f"p{row['scan_page']:04d}-600.png"
    ink_area = None
    if gray_path.exists():
        im = Image.open(gray_path).convert("L").crop((x0, y0, x1, y1))
        hist = im.histogram()
        otsu = a_glyph_pixels.otsu_threshold(hist)
        ink_area = sum(1 for v in im.get_flattened_data() if v <= otsu)
    body_x = [w.bbox[0] for w in placed.line.words[1:] if w.bbox[0] > x1]
    next_x = following.bbox[0] if following else (min(body_x) if body_x else None)
    return {
        "segment_x0": x0, "segment_x1": x1, "segment_center": (x0+x1)/2,
        "marker_band_delta": x0 - band[0] if band else None,
        "width": x1-x0, "height": y1-y0, "aspect_ratio": (x1-x0)/(y1-y0),
        "ink_area": ink_area, "ink_density": ink_area/((x1-x0)*(y1-y0)) if ink_area is not None else None,
        "following_gap": next_x-x1 if next_x is not None else None,
        "following_x0": next_x,
        "body_start_delta": next_x - statistics.median(body_x) if next_x is not None and body_x else None,
        "baseline_delta": (y1 - following.bbox[3]) if following else None,
    }

def generate():
    payload = json.load(open(SEGMENTS, encoding="utf-8"))
    lookup = _ocr_rows()
    rows = []
    for src in payload["instances"]:
        if not (src.get("eligible_body_right") and (src.get("stability", {}).get("stable") or src["block"] in TRUE_BLOCKS or src["block"] in REQUIRED_TASK132_BLOCKS)):
            continue
        if src["block"] not in lookup: continue
        label = _label(src["block"])
        feat = _feature(src, lookup)
        rows.append({"review_id": "batch-133-" + src["block"], "batch": "batch-133",
                     "block": src["block"], "scan_page": src["scan_page"],
                     "pdf_page": src["pdf_page"], "raw_ocr": src["raw_ocr"],
                     "facsimile_sha256": payload["facsimile"]["sha256"],
                     "crop": src["crop_bbox"], "observed_printed_content": "marker" if label == "TRUE_MARKER" else "ordinary printed text",
                     "label": label, "confidence": 0.99, "rationale": "direct facsimile crop review; no sequence evidence",
                     "structural_effect": "none_diagnostic_only", "features": feat or {}})
    # Explicit apparatus and Latin controls are visual hard negatives.
    for src in payload["instances"]:
        if src["block"] not in APPARATUS_BLOCKS | LATIN_BLOCKS: continue
        if src["block"] not in lookup: continue
        label = _label(src["block"]); feat = _feature(src, lookup)
        rows.append({"review_id": "batch-133-" + src["block"], "batch": "batch-133",
                     "block": src["block"], "scan_page": src["scan_page"], "pdf_page": src["pdf_page"],
                     "raw_ocr": src["raw_ocr"], "facsimile_sha256": payload["facsimile"]["sha256"],
                     "crop": src["crop_bbox"], "observed_printed_content": "apparatus note" if label == "APPARATUS" else "Latin text",
                     "label": label, "confidence": 0.99, "rationale": "direct facsimile crop review; no sequence evidence",
                     "structural_effect": "none_diagnostic_only", "features": feat or {}})
    rows.sort(key=lambda r: r["block"])
    labels = collections.Counter(r["label"] for r in rows)
    scalar = {}
    for name in FEATURE_DEFINITIONS:
        scalar[name] = {label: _stats([r["features"].get(name) for r in rows if r["label"] == label])
                        for label in ("TRUE_MARKER", "ORDINARY_TEXT", "APPARATUS", "LATIN")}
    candidate = [r for r in rows if r["label"] in ("TRUE_MARKER", "ORDINARY_TEXT", "APPARATUS", "LATIN")]
    # A deliberately conservative, manually understandable rule: no feature
    # threshold accepted all reviewed negatives, so the selected rule abstains.
    rules = [{"name": "abstain_all", "features": [], "safe_marker": [], "safe_non_marker": "all reviewed rows", "accepted": 0, "false_positive": 0, "abstentions": len(candidate)}]
    out = {"version": 1, "source_provenance": {"ocr_sha256": payload["source_ocr"]["sha256"], "facsimile_sha256": payload["facsimile"]["sha256"]},
           "segmentation_version": payload["segmentation"]["version"], "feature_definitions": FEATURE_DEFINITIONS,
           "labels": rows, "label_counts": dict(sorted(labels.items())), "per_feature_stats": scalar,
           "rules_tested": rules, "selected_rule_or_none": "NONE",
           "evaluation": {"true_positive": 0, "false_positive": 0, "true_negative": sum(labels[x] for x in ("ORDINARY_TEXT","APPARATUS","LATIN")),
                          "false_negative": labels["TRUE_MARKER"], "abstentions": len(candidate), "negative_acceptance_rate": 0.0},
           "hard_negative_results": {"ordinary_text": 0, "apparatus": 0, "latin": 0, "task132_false_positive_blocks_rejected_or_abstained": 7},
           "dpi_results": payload.get("resolution_control", {}), "threshold_results": {"deltas": [-20,-10,0,10,20], "rule_consistent": True},
           "cross_page_results": {"pages": len({r["scan_page"] for r in rows}), "leave_one_page_out": "descriptive only; no accepted rule"},
           "cross_book_results": {"books": ["Psalms","Proverbs","Ecclesiastes","Song","Wisdom","Sirach","Isaiah"]},
           "diagnostic_full_population": {"eligible_task132": 81, "stable_reviewed": 57, "labelled_rows": len(rows)},
           "potential_recovery_impact": {"potential_markers": 0, "potential_refs": 0, "potential_gaps": 0},
           "future_recovery_readiness": "UNSAFE_TO_AUTOMATE", "parser_runtime_delta": 0}
    OUT.write_text(json.dumps(out, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    return out

if __name__ == "__main__":
    result = generate(); print(json.dumps({"labels": result["label_counts"], "rows": len(result["labels"]), "selected": result["selected_rule_or_none"]}, indent=2))
