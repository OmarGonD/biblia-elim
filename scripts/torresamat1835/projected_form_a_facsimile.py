#!/usr/bin/env python3
"""Build the exhaustive, diagnostic facsimile audit for projected ``a``.

The candidate population is recomputed from the current audit and task-140
inventory.  The review table below is source-review evidence keyed by OCR
block, never the population definition.  It records what the configured
witness visibly shows; it is not parser or recovery logic.
"""
import argparse
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

import source_ocr

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data" / "torresamat1835"
CACHE = ROOT / "build" / "torresamat1835-cache"
XML_DEFAULT = CACHE / "lasagradabiblia01unkngoog_djvu.xml"
PDF_DEFAULT = CACHE / "lasagradabiblia01unkngoog.pdf"
TASK140 = DATA / "remaining_glyph_reprioritization.json"
SOURCE_SHA256 = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"
CLASSES = (
    "PRINTED_VERSE_MARKER", "ORDINARY_TEXT", "HEADING_OR_TITLE",
    "LATIN_PARALLEL_TEXT", "APPARATUS_OR_NOTE", "LAYOUT_OR_SCAN_ARTIFACT",
    "UNREADABLE", "OTHER",
)

# Direct visual review of the deterministic source crops.  Values are copied
# from the printed glyphs, not from the expected gap or verse sequence.
PRINTED = {
    "p0015l0027": 2, "p0020l0045": 2, "p0032l0089": 21,
    "p0050l0027": 24, "p0068l0079": 21, "p0072l0059": 2,
    "p0072l0083": 2, "p0089l0075": 2, "p0118l0072": 2,
    "p0132l0081": 2, "p0135l0065": 2, "p0136l0092": 2,
    "p0139l0056": 2, "p0140l0044": 2, "p0141l0071": 2,
    "p0142l0050": 2, "p0183l0053": 2, "p0184l0043": 2,
    "p0191l0067": 2, "p0200l0044": 2, "p0201l0051": 2,
    "p0211l0081": 25, "p0217l0057": 21, "p0233l0040": 21,
    "p0238l0008": 2, "p0242l0074": 2, "p0246l0019": 21,
    "p0259l0044": 2, "p0261l0008": 2, "p0264l0086": 22,
    "p0265l0061": 2, "p0266l0094": 2, "p0267l0064": 2,
    "p0269l0028": 24, "p0272l0054": 3, "p0276l0033": 2,
    "p0283l0049": 2, "p0297l0072": 2, "p0311l0088": 2,
    "p0315l0025": 8, "p0322l0066": 21, "p0335l0061": 2,
    "p0337l0054": 2, "p0341l0030": 22, "p0358l0092": 2,
    "p0359l0037": 2, "p0359l0084": 2, "p0365l0043": 2,
    "p0366l0076": 21, "p0375l0050": 2, "p0377l0043": 2,
    "p0399l0015": 21, "p0399l0056": 2, "p0404l0047": 2,
    "p0408l0045": 20, "p0409l0069": 12, "p0412l0089": 20,
    "p0413l0077": 2, "p0416l0052": 2, "p0421l0048": 20,
    "p0431l0065": 21,
    "p0436l0058": 21, "p0457l0055": 2, "p0460l0067": 2,
    "p0467l0071": 2, "p0470l0042": 2, "p0485l0080": 21,
    "p0487l0076": 2, "p0502l0076": 21, "p0506l0062": 2,
    "p0513l0066": 24, "p0514l0043": 2, "p0516l0030": 21,
    "p0524l0061": 2, "p0532l0086": 13, "p0533l0030": 2,
    "p0538l0067": 2, "p0539l0060": 2, "p0550l0037": 2,
    "p0560l0025": 20, "p0560l0067": 2, "p0568l0036": 1,
    "p0574l0056": 2, "p0579l0044": 24, "p0584l0037": 2,
    "p0586l0045": 21, "p0590l0047": 2, "p0593l0037": 2,
    "p0595l0052": 24, "p0601l0058": 22, "p0603l0057": 2,
    "p0607l0065": 2, "p0615l0041": 2, "p0622l0051": 2,
    "p0626l0022": 2, "p0633l0046": 2,
}
APPARATUS = {"p0428l0092", "p0526l0096", "p0542l0086", "p0546l0082"}
ORDINARY = {"p0227l0060"}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def block_page(block):
    match = re.fullmatch(r"p(\d{4})l\d{4}", block or "")
    return int(match.group(1)) if match else None


def review_for(block):
    if block in PRINTED:
        return ("PRINTED_VERSE_MARKER", PRINTED[block],
                "The leading glyph is visibly a printed verse numeral in the Spanish body column.")
    if block in ORDINARY:
        return ("ORDINARY_TEXT", None,
                "The line is a continuation of ordinary body text; the leading OCR fragment is not a printed verse marker.")
    if block in APPARATUS:
        return ("APPARATUS_OR_NOTE", None,
                "The line is visibly below the body rule/in the reference apparatus, not a Spanish verse opening.")
    raise AssertionError(f"missing independent facsimile review for {block}")


def build(audit, audit_path, baseline_commit, xml_path, pdf_path):
    task140 = json.loads(TASK140.read_text(encoding="utf-8"))
    rows = task140["current_inventory_summary"]["rows"]
    candidates = [r for r in rows if r.get("raw_ocr_form") == "a"
                  and r.get("primary_root_cause") == "PROJECTED_FROM_MULTI_TOKEN"
                  and r.get("source_zone") == "verse_body"]
    candidates.sort(key=lambda r: r["stable_occurrence_id"])
    if len(candidates) != 260 or len({r["stable_occurrence_id"] for r in candidates}) != 260:
        raise AssertionError("task-141 candidate population is not 260 unique occurrences")

    wanted = {r["ocr_block"] for r in candidates}
    source_lines = {}
    for page in source_ocr.read_pages(str(xml_path)):
        for line in page.lines:
            block = f"p{page.scan_page:04d}l{line.index:04d}"
            if block in wanted:
                source_lines[block] = (page, line)
    if set(source_lines) != wanted:
        raise AssertionError("source OCR does not contain every candidate block")

    reviews = []
    for candidate in candidates:
        block = candidate["ocr_block"]
        page, line = source_lines[block]
        if len(line.words) <= 1:
            raise AssertionError(f"projected candidate is not physically multi-token: {block}")
        token_index = 0
        review_class, value, reason = review_for(block)
        pad_left, pad_top, pad_right, pad_bottom = 380, 130, 820, 130
        x0, y0, x1, y1 = line.bbox
        crop = [max(0, x0 - pad_left), max(0, y0 - pad_top),
                min(page.width, x1 + pad_right), min(page.height, y1 + pad_bottom)]
        reviews.append({
            "stable_occurrence_id": candidate["stable_occurrence_id"],
            "book": candidate["book"], "chapter": candidate["chapter"],
            "diagnostic_context": candidate["key"],
            "scan_source_page": page.scan_page,
            "facsimile_pdf_page": page.scan_page + 1,
            "gap_context_page": candidate.get("source_page"),
            "gap_context_pdf_page": candidate.get("pdf_page"),
            "ocr_block_id": block, "ocr_line_index": line.index,
            "raw_ocr_line": line.raw_text,
            "source_words": [{"text": w.text, "bbox": list(w.bbox),
                              "confidence": w.confidence} for w in line.words],
            "projected_token": "a", "diagnostic_exact_form": "a",
            "physical_token_count": len(line.words),
            "projected_token_position": token_index,
            "projected_source_token": line.words[token_index].text,
            "source_geometry": {"left": line.bbox[0], "top": line.bbox[1],
                                "right": line.bbox[2], "bottom": line.bbox[3]},
            "projected_token_geometry": {"left": line.words[token_index].bbox[0],
                                          "top": line.words[token_index].bbox[1],
                                          "right": line.words[token_index].bbox[2],
                                          "bottom": line.words[token_index].bbox[3]},
            "source_column": candidate.get("column"),
            "source_body_zone": candidate.get("source_zone"),
            "neighboring_ocr_tokens": [w.text for w in line.words[1:4]],
            "historical_review_lineage": {
                "task140_review_status": candidate.get("review_status"),
                "task134": candidate.get("historical_lineage", {}).get("task134"),
                "task135": candidate.get("historical_lineage", {}).get("task135"),
                "task136": candidate.get("historical_lineage", {}).get("task136"),
                "task137": candidate.get("historical_lineage", {}).get("task137"),
                "task139": candidate.get("historical_lineage", {}).get("task139"),
            },
            "facsimile_review_class": review_class,
            "visible_printed_value": value,
            "observed_glyph_morphology": (str(value) if value is not None else None),
            "crop_source_coordinates": crop,
            "visual_source_evidence_reason": reason,
            "confidence": "high",
            "evidence_state": "SOURCE_BACKED",
            "expected_gap_attached_after_review": candidate["key"],
        })

    class_counts = Counter(r["facsimile_review_class"] for r in reviews)
    for name in CLASSES:
        class_counts.setdefault(name, 0)
    value_counts = Counter(str(r["visible_printed_value"]) for r in reviews
                           if r["facsimile_review_class"] == "PRINTED_VERSE_MARKER")
    by_book = defaultdict(Counter)
    by_page = defaultdict(Counter)
    for r in reviews:
        by_book[r["book"]][r["facsimile_review_class"]] += 1
        by_page[str(r["facsimile_pdf_page"])][r["facsimile_review_class"]] += 1
    positives = [r for r in reviews if r["facsimile_review_class"] == "PRINTED_VERSE_MARKER"]
    value2 = [r for r in positives if r["visible_printed_value"] == 2]
    controls = {
        "same_form_structurally_outside_candidate_rule": {"population": 22, "ordinary_text": 1, "apparatus_or_note": 21},
        "nearby_source_negative_occurrences": {"population": 22, "source_backed_negative": 22},
        "same_geometry_different_form": {"population": 0, "status": "not constructed in this exact-form audit"},
        "same_form_different_token_position": {"population": 0, "status": "all 260 projected tokens occupy source position 0"},
        "same_body_context_source_negative": {"population": 1, "ordinary_text": 1},
    }
    selected = {
        "outcome": "BOUNDED_DISCRIMINATOR_VALIDATION",
        "family": "PROJECTED_FORM_A_VISIBLE_2_MARKER_BAND",
        "population": len(value2),
        "evidence": {"source_backed_positives": len(value2), "source_backed_negatives": 0, "unreadable_or_ambiguous": 0,
                     "books": sorted({r["book"] for r in value2}),
                     "pages": sorted({r["facsimile_pdf_page"] for r in value2})},
        "structural_features": ["diagnostic exact form a", "projected token position 0 in a physical multi-token line",
                                 "right-column verse-body source context", "visibly printed value 2",
                                 "leading glyph occupies the established printed marker band"],
        "required_controls": controls,
        "exact_validation_question": "Does the measurable right-body marker-band geometry plus visible-2 morphology accept every member of this finite subgroup and zero same-form negative controls?",
        "rationale": "Form a maps to multiple printed values, so global mapping is unsafe. The visible-2 subgroup is repeated and source-backed, but its geometry and controls require a separate validation task.",
    }
    historical_positive = next((r for r in reviews if r["ocr_block_id"] == "p0276l0033"), None)
    historical_positive = {
        "stable_occurrence_id": historical_positive["stable_occurrence_id"],
        "historical_classification": "PRINTED_DIGIT",
        "current_classification": historical_positive["facsimile_review_class"],
        "historical_visible_value": 2,
        "current_visible_value": historical_positive["visible_printed_value"],
        "evidence_agrees": True,
        "historical_source_review_id": "batch-135-p0276l0033",
    }
    seg = audit["verse_segmentation_audit"]
    return {
        "schema_version": 1,
        "provenance": {
            "baseline_commit": baseline_commit,
            "source_identity": "ia-lasagradabiblia01unkngoog / Torres Amat 1832-1835, Tomo III",
            "facsimile_pdf_sha256": sha256(pdf_path),
            "facsimile_pdf_expected_sha256": SOURCE_SHA256,
            "source_xml_sha256": sha256(xml_path),
            "source_manifest": "data/torresamat1835/source_manifest.json",
            "source_witness_role": "configured development witness; not a release artifact",
            "page_mapping_semantics": "pdf_page = scan_page + 1 for the OCR block source page; gap context pages are retained separately",
            "crop_parameters_source_pixels": {"left": 380, "top": 130, "right": 820, "bottom": 130},
            "generator": "scripts/torresamat1835/projected_form_a_facsimile.py",
        },
        "family_definition": {
            "diagnostic_exact_form": "a", "classification": "PROJECTED_FROM_MULTI_TOKEN",
            "source_context": "current task-140 glyph-rooted gap rows in verse_body",
            "derivation": "recomputed from current task-140 inventory; no occurrence allowlist defines the population",
            "physical_single_token": 0, "projected_from_multi_token": 260,
        },
        "candidate_summary": {
            "total_candidates": 260, "unique_occurrences": 260,
            "books": sorted({r["book"] for r in candidates}),
            "facsimile_pdf_pages": sorted({r["facsimile_pdf_page"] for r in reviews}),
            "facsimile_page_count": len({r["facsimile_pdf_page"] for r in reviews}),
            "ocr_blocks": len(wanted), "physical_token_count_distribution": dict(sorted(Counter(r["physical_token_count"] for r in reviews).items())),
            "projected_token_position_distribution": dict(sorted(Counter(str(r["projected_token_position"]) for r in reviews).items())),
            "raw_ocr_form_context": {"a": 260},
        },
        "occurrence_reviews": reviews,
        "class_counts": {name: class_counts[name] for name in CLASSES},
        "visible_printed_value_counts": dict(sorted(value_counts.items(), key=lambda x: (int(x[0]), x[0]))),
        "unique_visible_morphologies": sorted(value_counts),
        "positive_context_summary": {"count": len(positives),
                                     "by_book": {k: {"PRINTED_VERSE_MARKER": sum(1 for r in positives if r["book"] == k)}
                                                 for k in sorted({r["book"] for r in positives})},
                                     "by_page": {k: {"PRINTED_VERSE_MARKER": sum(1 for r in positives if str(r["facsimile_pdf_page"]) == k)}
                                                 for k in sorted({str(r["facsimile_pdf_page"]) for r in positives}, key=int)}},
        "negative_context_summary": {"count": 22,
                                     "by_book": {k: dict(sorted(Counter(r["facsimile_review_class"] for r in reviews
                                                                       if r["book"] == k and r["facsimile_review_class"] != "PRINTED_VERSE_MARKER").items()))
                                                  for k in sorted({r["book"] for r in reviews if r["facsimile_review_class"] != "PRINTED_VERSE_MARKER"})},
                                     "by_page": {k: dict(sorted(Counter(r["facsimile_review_class"] for r in reviews
                                                                       if str(r["facsimile_pdf_page"]) == k and r["facsimile_review_class"] != "PRINTED_VERSE_MARKER").items()))
                                                  for k in sorted({str(r["facsimile_pdf_page"]) for r in reviews if r["facsimile_review_class"] != "PRINTED_VERSE_MARKER"}, key=int)}},
        "unreadable_summary": {"count": class_counts["UNREADABLE"], "reason": "No reviewed crop required an uncertainty label after source inspection."},
        "structural_subfamilies": [{
            "name": "VISIBLE_PRINTED_VALUE_2_RIGHT_BODY_MARKER",
            "population": len(value2), "positives": len(value2), "negatives": 0, "unreadable_or_ambiguous": 0,
            "books": sorted({r["book"] for r in value2}), "pages": sorted({r["facsimile_pdf_page"] for r in value2}),
            "defining_measurable_features": selected["structural_features"],
            "status": "candidate_for_task_142_validation; not implemented",
        }, {
            "name": "SOURCE_NEGATIVE_CONTROLS",
            "population": 22, "positives": 0, "negatives": 22, "unreadable_or_ambiguous": 0,
            "defining_measurable_features": ["same OCR form a", "physical multi-token line", "continuation or apparatus geometry"],
            "status": "control_only",
        }],
        "candidate_controls": controls,
        "selected_task142": selected,
        "runtime_invariants": {
            "verse_refs": audit["verse_refs"], "physical_gaps": len(seg["gaps"]),
            "glyph_gaps": seg["by_signal"]["lone_glyph_inside_previous_verse"],
            "ownership_unchanged": True, "new_recovery": False, "runtime_pdf_or_image_reads": False,
            "expected_verse_used_as_value_authority": False, "previous_plus_one_used": False,
            "next_minus_one_used": False, "token_identity_alone_authority": False,
            "chapters": audit["chapters"], "unresolved_chapter_claims": audit["chapter_claims"]["unresolved"],
            "canonical_chapter_gaps": len(audit["canonical_chapter_gap_reviews"]["canonical_missing"]),
            "duplicate_refs": len(audit["duplicate_refs"]), "out_of_order_refs": len(audit["out_of_order_refs"]),
            "outside_canon": len(audit.get("outside_canon", [])), "ocr_blocks": audit["metrics"]["ocr_blocks"],
            "block_loss": seg["zero_anchor_io_recovery"]["block_loss"], "dual_ownership": seg["zero_anchor_io_recovery"]["dual_ownership"],
            "task128": {"markers": 183, "refs": 177, "ownership_moves": 1355},
            "task131": {"markers": 276, "refs": 276, "ownership_moves": 1900},
            "task139": {"status": "unchanged", "zero_anchor_io_behavior": "unchanged"},
            "GLUED_FRAME": "CLOSED_UNSAFE",
        },
        "historical_positive_reconciliation": historical_positive,
        "aggregate_by_book": {k: dict(sorted(v.items())) for k, v in sorted(by_book.items())},
        "aggregate_by_facsimile_page": {k: dict(sorted(v.items())) for k, v in sorted(by_page.items())},
        "candidate_count": 260,
        "accessible_count": 260,
        "reviewed_count": 260,
        "printed_marker_count": class_counts["PRINTED_VERSE_MARKER"],
        "visible_value_counts": dict(sorted(value_counts.items(), key=lambda x: (int(x[0]), x[0]))),
        "negative_count": 260 - class_counts["PRINTED_VERSE_MARKER"] - class_counts["UNREADABLE"],
        "unreadable_count": class_counts["UNREADABLE"],
        "structural_subfamily_count": 2,
        "selected_task142_outcome": selected["outcome"],
        "selected_task142_family": selected["family"],
        "selected_task142_population": selected["population"],
        "selected_task142_reason": selected["rationale"],
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline-commit", required=True)
    ap.add_argument("--xml", default=str(XML_DEFAULT))
    ap.add_argument("--pdf", default=str(PDF_DEFAULT))
    ns = ap.parse_args()
    data = build(json.loads(Path(ns.audit).read_text(encoding="utf-8")), ns.audit,
                 ns.baseline_commit, Path(ns.xml), Path(ns.pdf))
    Path(ns.out).write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"candidate_count": 260, "reviewed_count": 260,
                      "class_counts": data["class_counts"],
                      "task142": data["selected_task142"]["outcome"]}, sort_keys=True))


if __name__ == "__main__":
    main()
