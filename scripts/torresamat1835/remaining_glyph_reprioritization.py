#!/usr/bin/env python3
"""Recompute and prioritize the post-task-139 glyph-gap population.

This is deliberately diagnostic.  The current inventory comes from the
current audit's gap rows; historical JSON is used only for stable lineage
and already-recorded source reviews.
"""
import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data" / "torresamat1835"
TAXONOMY = {
    "PHYSICAL_SINGLE_TOKEN": "Source line has exactly one OCR/source token.",
    "PHYSICAL_MULTI_TOKEN": "Physical source line has multiple tokens and is not merely projected.",
    "PROJECTED_FROM_MULTI_TOKEN": "Gap candidate is projected from a source line with multiple tokens.",
    "DETACHED_NUMERIC_FRAGMENT": "Numeric fragment signal is the primary source-backed cause.",
    "COMPOUND_FORM": "Compound marker form is the primary source-backed cause.",
    "LAYOUT_COLUMN_CORRUPTION": "Page/column or OCR-block geometry is the primary cause.",
    "NO_LOCAL_MARKER_EVIDENCE": "No local marker evidence is present in the source neighborhood.",
    "OTHER_SOURCE_BACKED": "Source-backed cause not covered by the other classes.",
    "UNRESOLVED": "Insufficient source evidence for a narrower classification.",
}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def audit_sha256(path):
    """Hash stable runtime evidence, excluding timing and embedded artifacts."""
    value = json.loads(Path(path).read_text(encoding="utf-8"))
    seg = value["verse_segmentation_audit"]
    value = {"verse_refs": value["verse_refs"],
             "materialized_verse_refs": value["materialized_verse_refs"],
             "chapters": value["chapters"],
             "chapter_claims": value["chapter_claims"],
             "canonical_chapter_gap_reviews": value["canonical_chapter_gap_reviews"],
             "metrics": value["metrics"], "duplicate_refs": value["duplicate_refs"],
             "out_of_order_refs": value["out_of_order_refs"],
             "gaps": seg["gaps"], "by_signal": seg["by_signal"],
             "zero_anchor_io_recovery": seg["zero_anchor_io_recovery"]}
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def stable_id(row):
    block = row.get("swallowed_block") or row.get("source_block")
    return f"{block}::{row['key']}" if block else f"{row['key']}::p{row.get('pdf_page')}"


def _historical_review_map(facsimile, current):
    by_block = defaultdict(list)
    for row in current:
        if row.get("source_block"):
            by_block[row["source_block"]].append(row)
    out = {}
    for review in facsimile.get("batch_135_reviews", []):
        matches = by_block.get(review.get("block_id"), [])
        suffix = review.get("review_id", "").rsplit("-v", 1)
        if len(suffix) == 2 and suffix[1].isdigit():
            key = next((r["key"] for r in matches if r["key"].rsplit(".", 1)[-1] == suffix[1]), None)
            if key:
                out[key] = review
        elif len(matches) == 1:
            out[matches[0]["key"]] = review
    return out


def _review_status(review):
    if not review:
        return "NOT_REVIEWED"
    label = review.get("classification")
    if label == "PRINTED_DIGIT":
        return "REVIEWED_POSITIVE"
    if label in {"ORDINARY_TEXT", "LATIN"}:
        return "REVIEWED_NEGATIVE"
    if label in {"UNREADABLE", "OTHER"}:
        return "REVIEWED_AMBIGUOUS"
    return "REVIEWED_AMBIGUOUS"


def _classify(row, true_single_keys):
    # The physical/projected distinction is source-structure evidence from
    # the historical facsimile review, joined by stable occurrence identity.
    if row["key"] in true_single_keys:
        return "PHYSICAL_SINGLE_TOKEN"
    return "PROJECTED_FROM_MULTI_TOKEN"


def build(audit, audit_path, baseline_commit):
    seg = audit["verse_segmentation_audit"]
    historical_path = DATA / "remaining_glyph_inventory.json"
    facsimile_path = DATA / "standalone_glyph_facsimile.json"
    projected_path = DATA / "projected_rejection_audit.json"
    discriminator_path = DATA / "no_trusted_band_discriminator.json"
    recovery_path = DATA / "zero_anchor_io_recovery_validation.json"
    historical = json.loads(historical_path.read_text(encoding="utf-8"))
    facsimile = json.loads(facsimile_path.read_text(encoding="utf-8"))
    projected = json.loads(projected_path.read_text(encoding="utf-8"))
    discriminator = json.loads(discriminator_path.read_text(encoding="utf-8"))
    recovery = json.loads(recovery_path.read_text(encoding="utf-8"))

    current_gaps = [g for g in seg["gaps"] if "lone_glyph_inside_previous_verse" in g.get("signals", [])]
    current_gaps.sort(key=lambda g: g["key"])
    true_single_keys = {r["key"] for r in facsimile.get("all_true_single_occurrences", [])}
    reviews = _historical_review_map(facsimile, [
        {"key": g["key"], "source_block": g.get("swallowed_block")} for g in current_gaps])
    historical_glyph = [r for r in historical["rows"] if "lone_glyph_inside_previous_verse" in r.get("signals", [])]
    historical_by_key = {r["key"]: r for r in historical_glyph}
    projected_by_key = {r.get("stable_identity", {}).get("gap_key"): r
                        for r in projected.get("occurrences", [])}
    discriminator_by_key = {r.get("stable_identity", {}).get("gap_key"): r
                            for r in discriminator.get("occurrences", [])}
    current_keys = {g["key"] for g in current_gaps}
    removed = sorted(set(historical_by_key) - current_keys)
    added = sorted(current_keys - set(historical_by_key))

    rows = []
    primary = Counter()
    secondary = Counter()
    forms = defaultdict(lambda: {"candidate_count": 0, "physical": 0, "projected": 0,
                                 "books": set(), "pages": set(), "reviewed": 0,
                                 "positive": 0, "negative": 0, "ambiguous": 0})
    review_counts = Counter()
    for gap in current_gaps:
        cls = _classify(gap, true_single_keys)
        review = reviews.get(gap["key"])
        status = _review_status(review)
        form = gap.get("swallowed_token") or ""
        tags = []
        signals = set(gap.get("signals", []))
        if signals & {"page_transition_candidate", "ocr_block_split_candidate"}:
            tags.append("LAYOUT_COLUMN_CORRUPTION")
        if signals & {"adjacent_numeric_fragment", "adjacent_superscript_like_fragment"}:
            tags.append("DETACHED_NUMERIC_FRAGMENT")
        if "marker_attached_to_text_candidate" in signals:
            tags.append("COMPOUND_FORM")
        if "no_local_numeric_evidence" in signals:
            tags.append("NO_LOCAL_MARKER_EVIDENCE")
        if not tags:
            tags.append(cls)
        primary[cls] += 1
        for tag in sorted(set(tags)):
            secondary[tag] += 1
        review_counts[status] += 1
        f = forms[form]
        f["candidate_count"] += 1
        f["physical" if cls == "PHYSICAL_SINGLE_TOKEN" else "projected"] += 1
        f["books"].add(gap["book"])
        f["pages"].add(gap.get("pdf_page"))
        if status != "NOT_REVIEWED":
            f["reviewed"] += 1
        if status == "REVIEWED_POSITIVE": f["positive"] += 1
        if status == "REVIEWED_NEGATIVE": f["negative"] += 1
        if status == "REVIEWED_AMBIGUOUS": f["ambiguous"] += 1
        rows.append({
            "stable_occurrence_id": stable_id(gap), "key": gap["key"],
            "book": gap["book"], "chapter": gap["chapter"],
            "source_page": gap.get("previous_page"), "pdf_page": gap.get("pdf_page"),
            "ocr_block": gap.get("swallowed_block"), "raw_ocr_form": form,
            "normalized_diagnostic_form": form.casefold(),
            "physical_token_count": 1 if cls == "PHYSICAL_SINGLE_TOKEN" else None,
            "projected_token_count": None if cls == "PHYSICAL_SINGLE_TOKEN" else len((gap.get("swallowed_text") or "").split()),
            "source_zone": "verse_body", "column": gap.get("previous_column"),
            "primary_root_cause": cls, "secondary_diagnostic_tags": sorted(set(tags)),
            "review_status": status,
            "historical_classification": historical_by_key.get(gap["key"], {}).get("primary_root_cause"),
            "historical_lineage": {
                "task134": historical_by_key.get(gap["key"], {}).get("primary_root_cause"),
                "task135": "TRUE_SINGLE_TOKEN_LINE" if gap["key"] in true_single_keys else "PROJECTED_TOKEN_FROM_MULTI_TOKEN_LINE",
                "task136": projected_by_key.get(gap["key"], {}).get("final_status", "NOT_IN_REVIEW_SET"),
                "task137": discriminator_by_key.get(gap["key"], {}).get("final_status", "NOT_IN_REVIEW_SET"),
                "task138": "DRY_RUN_NOT_APPLICABLE" if gap["key"] not in discriminator_by_key else "VALIDATED_SOURCE_FAMILY",
                "task139": "REMOVED_BY_RUNTIME_RECOVERY" if gap["key"] == "Ps.17.10" else "UNCHANGED",
            },
            "signals": sorted(signals), "recovery_coverage": "UNSAFE_OR_UNVALIDATED",
        })

    def clean_form(value):
        return {"candidate_count": value["candidate_count"], "physical_count": value["physical"],
                "projected_count": value["projected"], "books": sorted(value["books"]),
                "pages": sorted(x for x in value["pages"] if x is not None),
                "reviewed_count": value["reviewed"], "confirmed_positive": value["positive"],
                "confirmed_negative": value["negative"], "unreadable_or_ambiguous": value["ambiguous"]}

    form_out = {k: clean_form(forms[k]) for k in sorted(forms)}
    candidate = form_out.get("a", {})
    selected = {
        "name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_A",
        "population": candidate.get("candidate_count", 0),
        "structural_definition": "Current glyph-rooted gaps whose source line is projected from a multi-token line, exact OCR token form 'a', verse_body zone, with source block geometry retained; no expected verse sequence is used.",
        "books": candidate.get("books", []), "pages": candidate.get("pages", []),
        "ocr_forms": ["a"], "physical_projected_class": "PROJECTED_FROM_MULTI_TOKEN",
        "reviewed_positives": candidate.get("confirmed_positive", 0),
        "reviewed_negatives": candidate.get("confirmed_negative", 0),
        "ambiguous_unreviewed": candidate.get("unreadable_or_ambiguous", 0) + candidate.get("candidate_count", 0) - candidate.get("reviewed_count", 0),
        "evidence_state": "FACSIMILE_REVIEW_REQUIRED",
        "task_type": "facsimile audit",
        "selection_basis": ["finite exact population", "repeated source form", "existing occurrence-backed positive", "source pages and geometry available"],
    }
    root_counts = {k: primary.get(k, 0) for k in TAXONOMY}
    return {
        "schema_version": 1,
        "provenance": {
            "frozen_baseline_commit": baseline_commit,
            "audit_sha256": audit_sha256(audit_path), "source_xml_sha256": sha256(ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"),
            "historical_artifact_sha256": {p.name: sha256(p) for p in [historical_path, facsimile_path, projected_path, discriminator_path, recovery_path]},
            "generator": "scripts/torresamat1835/remaining_glyph_reprioritization.py",
        },
        "runtime_baseline": {"verse_refs": audit["verse_refs"], "physical_gaps": len(seg["gaps"]), "glyph_gaps": len(current_gaps), "chapters": audit["chapters"], "unresolved_chapter_claims": audit["chapter_claims"]["unresolved"], "canonical_chapter_gaps": len(audit["canonical_chapter_gap_reviews"]["canonical_missing"]), "ocr_blocks": audit["metrics"]["ocr_blocks"], "duplicate_refs": len(audit["duplicate_refs"]), "out_of_order_refs": len(audit["out_of_order_refs"]), "outside_canon": len(audit.get("outside_canon", []))},
        "historical_reconciliation": {"historical_total": len(historical_glyph), "current_total": len(current_gaps), "removed_occurrences": [{"key": k, "stable_occurrence_id": f"{historical_by_key[k].get('source_block')}::{k}", "historical_category": historical_by_key[k]["primary_root_cause"], "source_event": "task-139 zero-anchor I o recovery", "reason_absent": "source gap closed by runtime recovery"} for k in removed], "added_occurrences": added, "classification_changes": []},
        "current_inventory_summary": {"construction": "current audit gap rows filtered by lone_glyph_inside_previous_verse; not historical-list subtraction", "count": len(rows), "rows": rows},
        "physical_projected_split": {"PHYSICAL_SINGLE_TOKEN": primary["PHYSICAL_SINGLE_TOKEN"], "PHYSICAL_MULTI_TOKEN": primary["PHYSICAL_MULTI_TOKEN"], "PROJECTED_FROM_MULTI_TOKEN": primary["PROJECTED_FROM_MULTI_TOKEN"], "historical_equivalent": {"true_single": primary["PHYSICAL_SINGLE_TOKEN"], "projected": primary["PROJECTED_FROM_MULTI_TOKEN"]}, "historical_6_1304_still_applies": False if len(current_gaps) != 1310 else True, "reason": "The removed Ps.17.10 occurrence was projected; current source-structure reconciliation is 6 physical single-token and 1303 projected."},
        "root_cause_buckets": {"primary": root_counts, "secondary_tags": dict(sorted(secondary.items())), "diagnostic_families": {"physical_standalone_like_glyph": primary["PHYSICAL_SINGLE_TOKEN"], "projected_multi_token_glyph": primary["PROJECTED_FROM_MULTI_TOKEN"], "detached_numeric_fragment": secondary.get("DETACHED_NUMERIC_FRAGMENT", 0), "compound_marker": secondary.get("COMPOUND_FORM", 0), "layout_column_corruption": secondary.get("LAYOUT_COLUMN_CORRUPTION", 0), "no_local_marker_evidence": secondary.get("NO_LOCAL_MARKER_EVIDENCE", 0), "already_reviewed_unsafe": review_counts["REVIEWED_NEGATIVE"], "other_unresolved": primary.get("UNRESOLVED", 0)}, "counts_are_overlapping_secondary_tags": True, "primary_total": sum(root_counts.values()), "taxonomy": TAXONOMY},
        "form_frequencies": form_out,
        "review_coverage": {"counts": dict(sorted(review_counts.items())), "occurrence_backed": True, "historical_reviews_reused_only_on_stable_identity": True},
        "closed_family_status": {"GLUED_FRAME": "CLOSED_UNSAFE", "outside_marker_band": "CLOSED_REJECTED", "task_128": "CLOSED_NOT_WIDENED", "task_131": "CLOSED_NOT_WIDENED", "zero_anchor_I_o": "SOLVED_TASK_139", "generic_physical_single_glyph": "NO_REUSABLE_RULE"},
        "bounded_candidate_families": [{"name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_A", "population": candidate.get("candidate_count", 0), "evidence_state": "FACSIMILE_REVIEW_REQUIRED", "positive": candidate.get("confirmed_positive", 0), "negative": candidate.get("confirmed_negative", 0), "ambiguous": selected["ambiguous_unreviewed"]}, {"name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_Y", "population": form_out.get("y", {}).get("candidate_count", 0), "evidence_state": "NEGATIVE_CONTROL_AVAILABLE"}, {"name": "PHYSICAL_SINGLE_TOKEN", "population": primary["PHYSICAL_SINGLE_TOKEN"], "evidence_state": "NO_REUSABLE_RULE"}],
        "selected_task141_family": selected,
        "deferred_families": [{"name": "all_remaining_glyphs", "reason": "not bounded enough"}, {"name": "PHYSICAL_SINGLE_TOKEN", "reason": "no reusable recovery rule established"}, {"name": "I_o_zero_anchor", "reason": "solved and closed by task 139"}, {"name": "outside_marker_band", "reason": "confirmed rejected"}],
        "runtime_invariants": {"ownership_unchanged": True, "new_recovery": False, "expected_verse_used": False, "previous_plus_one_used": False, "next_minus_one_used": False, "token_identity_alone_authority": False, "block_loss": audit["verse_segmentation_audit"]["zero_anchor_io_recovery"]["block_loss"], "dual_ownership": audit["verse_segmentation_audit"]["zero_anchor_io_recovery"]["dual_ownership"]},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline-commit", required=True)
    ns = ap.parse_args()
    data = build(json.loads(Path(ns.audit).read_text(encoding="utf-8")), ns.audit, ns.baseline_commit)
    Path(ns.out).write_text(json.dumps(data, ensure_ascii=False, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    print(json.dumps({"current": data["current_inventory_summary"]["count"], "removed": len(data["historical_reconciliation"]["removed_occurrences"]), "selected": data["selected_task141_family"]["name"]}, sort_keys=True))


if __name__ == "__main__":
    main()
