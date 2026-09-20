#!/usr/bin/env python3
"""Build the task-137 no-trusted-marker-band diagnostic.

This is an evidence report only.  It never changes the parser, creates a
VerseRef, or proposes a runtime recovery.  The visual labels below are the
batch-137 review of the single source block represented by the 18 stable gap
occurrences; they are deliberately kept as facsimile provenance.
"""
import argparse
import hashlib
import json
import statistics
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import compound_glyphs
import layout
from source_ocr import read_pages

ROOT = Path(__file__).resolve().parents[2]
PDF_SHA = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"
OUT = ROOT / "data/torresamat1835/no_trusted_band_discriminator.json"
REVIEW_BATCH = "batch-137"

FEATURE_DEFINITIONS = {
    "trusted_anchor_count": "count of same-page, same-column, same-zone ordinary numeric marker anchors used by task-128 band_of",
    "candidate_x": "left edge of the union of the two OCR token boxes",
    "nearest_anchor_before_x_delta": "candidate_x minus x of nearest trusted marker before the candidate in source reading order, same column and zone",
    "nearest_anchor_after_x_delta": "x of nearest trusted marker after the candidate in source reading order, same column and zone minus candidate_x",
    "local_marker_x_delta": "candidate_x minus median trusted marker x in the local same-page band; null when the band is unavailable",
    "body_start_delta": "following text word x0 minus median x0 of following words on the line",
    "following_text_gap": "following text word x0 minus candidate bbox x1",
    "indentation": "candidate_x minus local marker median x; null when unavailable",
    "bbox_width": "candidate bbox width in OCR scan pixels",
    "bbox_height": "candidate bbox height in OCR scan pixels",
    "bbox_aspect": "bbox_width / bbox_height",
    "baseline_delta": "candidate bbox bottom minus following text word bbox bottom",
    "column": "layout column from source geometry",
    "zone": "layout zone from source geometry",
    "exact_compound_form": "exact two-token OCR form, retained as a descriptive source feature and never used alone",
}


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def stats(values):
    values = sorted(float(v) for v in values if v is not None)
    if not values:
        return {"n": 0, "min": None, "max": None, "median": None, "mad": None}
    med = statistics.median(values)
    return {"n": len(values), "min": values[0], "max": values[-1],
            "median": med,
            "mad": statistics.median(abs(v - med) for v in values)}


def source_index_and_anchors(xml):
    out, anchors = {}, []
    for page in read_pages(str(xml)):
        placed_lines = layout.split_columns(page)
        by_col = {}
        for p in placed_lines:
            if p.zone is layout.Zone.BODY:
                by_col.setdefault(p.column.value, []).append(p.line)
        bands = {c: compound_glyphs.band_of(lines) for c, lines in by_col.items()}
        for placed in placed_lines:
            block = f"p{page.scan_page:04d}l{placed.line.index:04d}"
            out[block] = (page, placed)
            words = placed.line.words
            first, reason = compound_glyphs.marker_tokens(words)
            if (first is None or first + 1 >= len(words)
                    or placed.zone is not layout.Zone.BODY
                    or placed.column.value != "right"
                    or not compound_glyphs._PLAIN_NUMERAL.match(words[first].text)):
                continue
            band = bands.get(placed.column.value)
            if band is None:
                continue
            anchors.append({
                "scan_page": page.scan_page, "line_index": placed.line.index,
                "column": placed.column.value, "zone": placed.zone.value,
                "marker_x0": words[first].bbox[0], "band_center": band[0],
                "indent": words[first].bbox[0] - band[0],
                "form": f"{words[first].text} {words[first + 1].text}",
                "bbox": list(compound_glyphs.marker_bbox(words, first)),
            })
    return out, anchors


def candidate_features(page, placed, trusted, all_anchors):
    words = placed.line.words
    first, reason = compound_glyphs.marker_tokens(words)
    if first is None or first + 1 >= len(words):
        raise ValueError(f"candidate marker extraction failed: {reason}")
    box = compound_glyphs.marker_bbox(words, first)
    following = words[first + 2] if first + 2 < len(words) else None
    same_band = [a for a in all_anchors
                 if a["scan_page"] == page.scan_page
                 and a["column"] == placed.column.value
                 and a["zone"] == placed.zone.value]
    trusted_count = len(same_band)
    band = trusted.get(placed.column.value)
    following_x = following.bbox[0] if following else None
    following_xs = [w.bbox[0] for w in words[first + 2:]]
    median_following = statistics.median(following_xs) if following_xs else None
    before = [a for a in all_anchors
              if a["column"] == placed.column.value and a["zone"] == placed.zone.value
              and (a["scan_page"], a["line_index"]) < (page.scan_page, placed.line.index)]
    after = [a for a in all_anchors
             if a["column"] == placed.column.value and a["zone"] == placed.zone.value
             and (a["scan_page"], a["line_index"]) > (page.scan_page, placed.line.index)]
    before = max(before, key=lambda a: (a["scan_page"], a["line_index"]), default=None)
    after = min(after, key=lambda a: (a["scan_page"], a["line_index"]), default=None)
    return {
        "trusted_anchor_count": trusted_count,
        "candidate_x": box[0],
        "nearest_anchor_before_x_delta": None if before is None else box[0] - before["marker_x0"],
        "nearest_anchor_after_x_delta": None if after is None else after["marker_x0"] - box[0],
        "local_marker_x_delta": None if band is None else box[0] - band[0],
        "body_start_delta": None if following_x is None or median_following is None else following_x - median_following,
        "following_text_gap": None if following_x is None else following_x - box[2],
        "indentation": None if band is None else box[0] - band[0],
        "bbox_width": box[2] - box[0],
        "bbox_height": box[3] - box[1],
        "bbox_aspect": (box[2] - box[0]) / (box[3] - box[1]),
        "baseline_delta": None if following is None else box[3] - following.bbox[3],
        "column": placed.column.value,
        "zone": placed.zone.value,
        "exact_compound_form": f"{words[first].text} {words[first + 1].text}",
    }


def _review(record):
    sid = record["stable_identity"]
    return {
        "review_id": f"{REVIEW_BATCH}-{sid['block_id']}-{sid['gap_key'].replace('.', '-')}",
        "batch": REVIEW_BATCH,
        "stable_occurrence_id": f"{sid['block_id']}::{sid['gap_key']}",
        "source_sha256": sid["source_sha256"],
        "scan_page": sid["scan_page"], "pdf_page": sid["pdf_page"],
        "block_id": sid["block_id"], "raw_ocr": record["raw_ocr_line"],
        "token_sequence": record["token_sequence"],
        "bbox": record["projected_bbox"],
        "visible_printed_content": "printed verse marker 10",
        "classification": "PRINTED_VERSE_MARKER",
        "printed_value": 10,
        "confidence": 1.0,
        "rationale": "Direct review of PDF page 33 (scan page 32), right Spanish body column; the visible printed marker is 10. No expected verse or sequence evidence used.",
        "structural_effect": "none_diagnostic_only",
    }


def _hard_negatives(projected, audit):
    rows = []
    geometry = audit["verse_segmentation_audit"]["compound_glyph_recovery"]["geometry_rejected_detail"]
    for r in geometry:
        if r["reason"] == "no_trusted_marker_band" and r["block_id"] != "p0032l0052":
            rows.append({
                "negative_id": "task128-no-band-" + r["block_id"],
                "class": "no_band_other_form",
                "source_evidence": {"block_id": r["block_id"], "page": r["page"], "raw": r["raw"],
                                    "bbox": r["marker_bbox"], "reason": r["reason"]},
                "form": r["form"], "trusted_anchor_count": 0,
                "features": {"trusted_anchor_count": 0, "exact_compound_form": r["form"]},
            })
    for r in projected["occurrences"]:
        if r["task128_candidate"]["historical_reason"] in ("outside_marker_band", "no_trusted_marker_band"):
            if r["stable_identity"]["block_id"] == "p0032l0052":
                continue
            rows.append({
                "negative_id": "task136-" + r["stable_identity"]["block_id"],
                "class": "geometry_near_miss" if r["task128_candidate"]["historical_reason"] == "outside_marker_band" else "no_band_other_form",
                "source_evidence": r["stable_identity"],
                "form": r["exact_projected_form"],
                "trusted_anchor_count": None,
                "features": {"trusted_anchor_count": None, "exact_compound_form": r["exact_projected_form"]},
            })
    fac = json.loads((ROOT / "data/torresamat1835/standalone_glyph_facsimile.json").read_text())
    for r in fac["batch_135_reviews"]:
        if r["classification"] == "ORDINARY_TEXT":
            rows.append({
                "negative_id": r["review_id"], "class": "facsimile_ordinary_text",
                "source_evidence": {"source": "task-135-facsimile", "review_id": r["review_id"],
                                    "source_sha256": fac["provenance"]["source_sha256"]},
                "form": r["exact_ocr_form"], "trusted_anchor_count": None,
                "features": {"trusted_anchor_count": None, "exact_compound_form": r["exact_ocr_form"]},
            })
    return sorted(rows, key=lambda r: r["negative_id"])


def build(inventory, projected_path, audit_path, xml, pdf, baseline_commit):
    projected = json.loads(Path(projected_path).read_text())
    audit = json.loads(Path(audit_path).read_text())
    targets = [r for r in projected["occurrences"] if r["task128_candidate"]["historical_reason"] == "no_trusted_marker_band"]
    targets.sort(key=lambda r: (r["stable_identity"]["scan_page"], r["stable_identity"]["block_id"], r["stable_identity"]["gap_key"]))
    if len(targets) != 18 or len({(r["stable_identity"]["block_id"], r["stable_identity"]["gap_key"]) for r in targets}) != 18:
        raise ValueError("task-137 target population is not 18 unique stable occurrences")
    if sha256(pdf) != PDF_SHA:
        raise ValueError("facsimile SHA256 mismatch")
    lookup, anchors = source_index_and_anchors(xml)
    target_block = targets[0]["stable_identity"]["block_id"]
    page, placed = lookup[target_block]
    placed_lines = layout.split_columns(page)
    bands = {p.column.value: compound_glyphs.band_of([q.line for q in placed_lines
             if q.zone is layout.Zone.BODY and q.column == p.column]) for p in placed_lines}
    target_records = []
    for r in targets:
        f = candidate_features(page, placed, bands, anchors)
        r = json.loads(json.dumps(r))
        r["recomputed_task128"] = {
            "trusted_anchor_count": f["trusted_anchor_count"],
            "minimum_trusted_anchor_count": compound_glyphs.MIN_BAND_MARKERS,
            "band_available": f["trusted_anchor_count"] >= compound_glyphs.MIN_BAND_MARKERS,
            "reason": "no_trusted_marker_band" if f["trusted_anchor_count"] < compound_glyphs.MIN_BAND_MARKERS else "unexpected_band_available",
            "geometry_checks_before_band_decision": ["exact two one-character tokens", "safe compound form", "body/right-column source placement"],
            "geometry_checks_after_band_decision": ["outside-band test and following-text checks are not reached when band is unavailable"],
        }
        r["source_features"] = f
        r["facsimile_review"] = _review(r)
        target_records.append(r)

    controls = audit["verse_segmentation_audit"]["compound_glyph_recovery"]["markers"]
    accepted_controls = []
    for c in sorted(controls, key=lambda x: (x["page"], x["block_id"])):
        control_page, control_placed = lookup[c["block_id"]]
        control_lines = layout.split_columns(control_page)
        control_bands = {p.column.value: compound_glyphs.band_of([q.line for q in control_lines
                        if q.zone is layout.Zone.BODY and q.column == p.column]) for p in control_lines}
        cf = candidate_features(control_page, control_placed, control_bands, anchors)
        accepted_controls.append({**c, "control_id": "accepted-task128-" + c["block_id"],
                                  "trusted_anchor_count": cf["trusted_anchor_count"],
                                  "features": cf})
    if len(accepted_controls) != 183:
        raise ValueError(f"expected 183 accepted controls, got {len(accepted_controls)}")
    negatives = _hard_negatives(projected, audit)
    for n in negatives:
        block = n["source_evidence"].get("block_id")
        if not block or block not in lookup:
            continue
        negative_page, negative_placed = lookup[block]
        negative_lines = layout.split_columns(negative_page)
        negative_bands = {p.column.value: compound_glyphs.band_of([q.line for q in negative_lines
                         if q.zone is layout.Zone.BODY and q.column == p.column]) for p in negative_lines}
        n["features"] = candidate_features(negative_page, negative_placed, negative_bands, anchors)
        n["trusted_anchor_count"] = n["features"]["trusted_anchor_count"]

    def rule_eval(name, predicate):
        def decision(features):
            try:
                return predicate(features)
            except (KeyError, TypeError):
                return False
        tp = sum(decision(r["source_features"]) for r in target_records)
        fp_target = 0
        controls_yes = sum(decision(r["features"]) for r in accepted_controls)
        neg_yes = 0
        for n in negatives:
            if n["features"]["trusted_anchor_count"] is None:
                # Missing geometry is an abstention, never an acceptance.
                continue
            neg_yes += decision(n["features"])
        neg_abstentions = sum(1 for n in negatives
                              if n["features"].get("trusted_anchor_count") is None)
        return {"name": name, "features": [], "target_visual_positives_accepted": tp,
                "target_visual_positives_abstained": len(target_records) - tp,
                "target_visual_negatives_accepted": fp_target,
                "target_visual_negatives_rejected_or_abstained": 0,
                "accepted_task128_controls_accepted": controls_yes,
                "accepted_task128_controls_rejected_or_abstained": len(accepted_controls) - controls_yes,
                "hard_negatives_accepted": neg_yes,
                "hard_negatives_rejected_or_abstained": len(negatives) - neg_yes,
                "total_abstentions": len(target_records) - tp + neg_abstentions,
                "safe_gate": tp >= 1 and fp_target == 0 and neg_yes == 0}

    rules = []
    rules.append(rule_eval("trusted_anchor_count == 0", lambda f: f.get("trusted_anchor_count") == 0))
    rules.append(rule_eval("exact_compound_form == 'I o'", lambda f: f.get("exact_compound_form") == "I o"))
    rules.append(rule_eval("trusted_anchor_count == 0 AND exact_compound_form == 'I o'",
                           lambda f: f.get("trusted_anchor_count") == 0 and f.get("exact_compound_form") == "I o"))
    rules[-1]["features"] = ["trusted_anchor_count", "exact_compound_form"]
    selected = rules[-1] if rules[-1]["safe_gate"] else None
    status = "SAFE_DISCRIMINATOR_FOUND" if selected else "UNSAFE_TO_AUTOMATE"
    recommendation = {
        "target": "bounded recovery-validation task for the 18-case I o / zero-anchor family" if selected else "additional hard-negative/facsimile evidence study for the no_trusted family",
        "reason": "The selected two-feature rule has zero accepted known negatives and zero accepted hard negatives; it remains diagnostic-only." if selected else "No evaluated rule has zero known-negative and hard-negative acceptance with useful coverage.",
        "exact_scope": "Validate only source-derived candidates with zero same-page same-column same-zone trusted anchors and exact OCR form I o; no runtime change, no marker recovery in task 137." if selected else "Review same-witness ordinary-text and near-miss candidates around no-band lines; no runtime change.",
    }
    groups = {"visually_confirmed_target_markers": target_records,
              "visually_confirmed_target_negatives": [],
              "accepted_task128_geometry_controls": accepted_controls,
              "hard_negative_controls": negatives}
    scalar_features = [k for k in FEATURE_DEFINITIONS if k not in ("column", "zone", "exact_compound_form")]
    descriptive = {}
    for feature in scalar_features:
        descriptive[feature] = {name: stats([(r.get("source_features") or r.get("features") or {}).get(feature) for r in rows])
                                for name, rows in groups.items()}
    reviews = [r["facsimile_review"] for r in target_records]
    labels = Counter(r["facsimile_review"]["classification"] for r in target_records)
    artifact = {
        "schema_version": 1,
        "provenance": {"task": "TORRES-1835-NO-TRUSTED-BAND-DISCRIMINATOR-137",
                       "baseline_commit": baseline_commit,
                       "source_witness": "lasagradabiblia01unkngoog",
                       "source_sha256": PDF_SHA,
                       "source_pdf": "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf",
                       "source_xml": "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml",
                       "pdf_pages": 652, "mapping": "pdf_page = scan_page + 1", "offline": True},
        "historical_task128_linkage": {"artifact": "data/torresamat1835/projected_rejection_audit.json",
                                        "historical_reason": "no_trusted_marker_band", "population": 18,
                                        "outside_marker_band_excluded": 5,
                                        "three_anchor_policy_unchanged": True},
        "occurrences": target_records,
        "batch_137_facsimile_reviews": reviews,
        "visual_label_accounting": {"total_reviewed": 18, **dict(sorted(labels.items())),
                                     "ordinary_text": 0, "heading": 0, "latin": 0,
                                     "apparatus": 0, "unreadable": 0, "other": 0,
                                     "printed_marker_values": {"10": 18}},
        "trusted_anchor_failure": {"count_distribution": dict(sorted(Counter(r["recomputed_task128"]["trusted_anchor_count"] for r in target_records).items())),
                                   "minimum_required": compound_glyphs.MIN_BAND_MARKERS,
                                   "definition": "same-page, same-column, same-zone ordinary numeric markers; band median left edge and median per-digit width",
                                   "semantics": "fewer than three trusted anchors yields no band and abstention before outside-band or following-text checks"},
        "accepted_controls": {"population": len(accepted_controls), "controls": accepted_controls,
                              "books": dict(sorted(Counter(c.get("book") for c in accepted_controls).items())),
                              "pages": sorted({c["page"] for c in accepted_controls}),
                              "forms": dict(sorted(Counter(c["form"] for c in accepted_controls).items())),
                              "geometry_coverage": ["marker_x0", "band_center", "indent", "tolerance", "column", "zone", "bbox"]},
        "hard_negatives": {"population": len(negatives), "classes": dict(sorted(Counter(n["class"] for n in negatives).items())), "controls": negatives},
        "feature_definitions": FEATURE_DEFINITIONS,
        "feature_groups": {k: [{"id": r.get("control_id") or r.get("negative_id") or r["stable_identity"]["gap_key"], "features": r.get("features") or r.get("source_features")} for r in v] for k, v in groups.items()},
        "descriptive_statistics": descriptive,
        "candidate_rules": rules,
        "selected_rule": selected,
        "overall_family_status": status,
        "subfamilies": {"Ps.scan32.form_I_o.zero_anchors": {"status": "EVIDENCE_READY" if selected else "UNSAFE_OR_AMBIGUOUS", "population": 18}},
        "task138_recommendation": recommendation,
        "audit_integration": {"section": "verse_segmentation_audit.no_trusted_band_discriminator_validation", "population": 18,
                               "visual_labels": dict(sorted(labels.items())), "trusted_anchor_count_distribution": dict(sorted(Counter(r["recomputed_task128"]["trusted_anchor_count"] for r in target_records).items())),
                               "accepted_controls": 183, "hard_negatives": len(negatives), "features": sorted(FEATURE_DEFINITIONS),
                               "candidate_rules": [r["name"] for r in rules], "rule_outcomes": rules,
                               "safe_discriminator_found": bool(selected), "positive_coverage": selected["target_visual_positives_accepted"] if selected else 0,
                               "known_negative_acceptance": selected["target_visual_negatives_accepted"] if selected else None,
                               "abstentions": selected["total_abstentions"] if selected else None, "overall_status": status,
                               "task138_target": recommendation["target"], "task138_reason": recommendation["reason"]},
        "invariants": {"runtime_recovery": False, "expected_verse_used": False, "previous_plus_one_used": False, "next_minus_one_used": False,
                        "page_block_chapter_verse_identity_rule_feature": False,
                        "task128_minimum_trusted_anchors": compound_glyphs.MIN_BAND_MARKERS,
                        "task128_marker_band_width_unchanged": True, "task128_geometry_tolerance_unchanged": True},
    }
    return artifact


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--inventory", required=True); ap.add_argument("--projected", required=True)
    ap.add_argument("--audit", required=True); ap.add_argument("--xml", required=True)
    ap.add_argument("--pdf", required=True); ap.add_argument("--out", required=True)
    ap.add_argument("--baseline-commit", help="immutable task-input baseline commit")
    ns = ap.parse_args()

    # The artifact's baseline identifies the repository state whose inputs
    # were audited.  It must not change merely because a later commit invokes
    # this generator again.  An explicit value is preferred for new artifacts;
    # the existing canonical artifact supplies the frozen value for the
    # task-137 regeneration path.
    baseline_commit = ns.baseline_commit
    if baseline_commit is None:
        try:
            canonical = json.loads(OUT.read_text(encoding="utf-8"))
            baseline_commit = canonical["provenance"]["baseline_commit"]
        except (FileNotFoundError, KeyError, TypeError, json.JSONDecodeError) as exc:
            raise SystemExit("--baseline-commit is required when no canonical artifact provenance is available") from exc
    out = build(ns.inventory, ns.projected, ns.audit, ns.xml, ns.pdf, baseline_commit)
    Path(ns.out).write_text(json.dumps(out, ensure_ascii=False, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"population": len(out["occurrences"]), "status": out["overall_family_status"], "sha256": sha256(ns.out)}, sort_keys=True))


if __name__ == "__main__":
    main()
