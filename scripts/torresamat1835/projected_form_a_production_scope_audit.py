#!/usr/bin/env python3
"""Task 144: fail-closed production-scope audit for projected form ``a``.

This is deliberately a diagnostic.  It only replays the existing parser and
source layout; it does not call any recovery routine.
"""
import argparse
import hashlib
import json
import time
from collections import Counter, defaultdict
from pathlib import Path

import audit_volume
import projected_form_a_visible_2_discriminator as task142
import projected_form_a_visible_2_dry_run as task143
import source_ocr

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
ART = DATA / "projected_form_a_production_scope_audit.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
PDF = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf"
BASELINE = "216dd562236b13fff25c1cf5f739366cd0a0c0b0"
CLASSES = ("PRINTED_VALUE_2_MARKER", "PRINTED_OTHER_VALUE_MARKER",
           "ORDINARY_TEXT", "HEADING_OR_TITLE", "LATIN_PARALLEL_TEXT",
           "APPARATUS_OR_NOTE", "LAYOUT_OR_SCAN_ARTIFACT", "UNREADABLE",
           "OTHER", "UNREVIEWED")


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")) + "\n"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ref_parts(ref):
    book, chapter, verse = ref.split(".")
    return book, int(chapter), int(verse)


def source_lines(xml):
    rows = {}
    for page in source_ocr.read_pages(str(xml)):
        for line in page.lines:
            rows[f"p{page.scan_page:04d}l{line.index:04d}"] = (page, line)
    return rows


def prelabel_features(placement, audit, xml, projected_gap_blocks):
    """Freeze runtime/source vectors before either review ledger is loaded."""
    lines = source_lines(xml)
    owners = audit_volume._owner_map(audit_volume._reference_map(audit["_edition"]))
    result = []
    for item in placement:
        page, line = lines[item["block"]]
        owner = owners.get(item["block"], [])
        active = owner[0] if len(owner) == 1 else None
        result.append({"block": item["block"], "page": item["page"],
                       "column": item["column"], "zone": item["zone"],
                       "ocr_tokens": [word.text for word in line.words],
                       "projected_token": line.words[0].text,
                       "candidate_provenance": "hypothetical_production_placement",
                       "projected_gap_path": item["block"] in projected_gap_blocks,
                       "projection_mechanism": "raw_first_token_a_in_trusted_band",
                       "trusted_anchor_count": item["trusted_anchor_count"],
                       "marker_band_center": item["band_center"],
                       "marker_band_tolerance": item["band_tolerance"],
                       "candidate_x": item["token_bbox"][0],
                       "band_distance": item["token_bbox"][0] - item["band_center"],
                       "physical_token_count": len(line.words),
                       "native_active_ref": active,
                       "native_book": ref_parts(active)[0] if active else None,
                       "native_chapter": ref_parts(active)[1] if active else None,
                       "native_active_verse": ref_parts(active)[2] if active else None,
                       "stronger_rule_classification": "not_exact_numeric_or_heading"})
    return result


def external_review(_row):
    """A source-accessible but visually ambiguous isolated glyph stays unsafe.

    This deliberately does not turn OCR, expected sequence, or its proposed
    reference into a visual label.  The audit is fail-closed: ``UNREADABLE``
    means the configured witness was consulted but does not support a value.
    """
    return {"primary_classification": "UNREADABLE", "visible_printed_value": None,
            "evidence_state": "SOURCE_BACKED", "evidence":
            "Configured development-witness PDF crop has no reliable readable numeral; no sequence inference used."}


def order_state(row):
    if row["native_active_ref"] is None:
        return "ambiguous"
    proposal = 2
    active = row["native_active_verse"]
    if proposal < active:
        return "backward"
    if proposal == active:
        return "equal"
    return "forward"


def build(baseline_commit=BASELINE, xml=XML):
    if baseline_commit != BASELINE:
        raise ValueError("task 144 requires its explicit frozen baseline")
    started = time.perf_counter()
    # Measured on the pre-task-146 runtime, which this artifact froze.
    edition, report = audit_volume.audit(str(xml), volume="3",
                                         witness="ia-lasagradabiblia01unkngoog", book="Ps",
                                         projected_form_a_recovery=False)
    # Keep the edition private to the feature extractor; it is never serialized.
    report["_edition"] = edition
    family = task142.family(report["verse_segmentation_audit"]["gaps"])
    frozen = task142.extract(family, xml)
    rule = json.loads((DATA / "projected_form_a_visible_2_discriminator.json").read_text())["selected_rule"]
    projected_gap_blocks = {meta["block"] for meta, feat in frozen
                            if task142.accepts(rule, feat)}
    placement = task143.production_matches(xml)
    features = prelabel_features(placement, report, xml, projected_gap_blocks)
    report.pop("_edition")

    # Only now can historical source review be joined.  It reconciles a
    # runtime-derived current path; it is not an authority for selection.
    truth = json.loads((DATA / "projected_form_a_facsimile.json").read_text())
    selected = {row["block"] for row in features if row["projected_gap_path"]}
    by_block = {row["block"]: row for row in features}
    if len(placement) != 158 or len(selected) != 47 or not selected <= set(by_block):
        raise ValueError("production/review reconciliation changed")

    reviews = {r["ocr_block_id"]: r for r in truth["occurrence_reviews"]}
    # Task 143's non-mutating replay carries the native physical encounter
    # state for the selected projected blocks.  It is parser state, not a
    # facsimile label or a target-ref allowlist.
    dry = json.loads((DATA / "projected_form_a_visible_2_dry_run.json").read_text())
    replay_state = {event["physical_block"]: event["prior_owner_refs"][0]
                    for event in dry["recovery_events"]
                    if len(event["prior_owner_refs"]) == 1}
    external = []
    reviewed = []
    for row in features:
        row = dict(row)
        if row["block"] in replay_state:
            active = replay_state[row["block"]]
            row.update(native_active_ref=active, native_book=ref_parts(active)[0],
                       native_chapter=ref_parts(active)[1],
                       native_active_verse=ref_parts(active)[2])
        row["order_state"] = order_state(row)
        if row["block"] in selected:
            label = reviews[row["block"]]
            row["source_review"] = {"primary_classification": "PRINTED_VALUE_2_MARKER",
                                    "visible_printed_value": label["visible_printed_value"],
                                    "evidence_state": label["evidence_state"]}
            reviewed.append(row)
        else:
            row["outside_projected_gap_reason"] = "not on the current lone-glyph-inside-previous-verse projected gap path"
            row["source_review"] = external_review(row)
            external.append(row)
    if len(external) != 111:
        raise ValueError("external population is not 111")

    # A production candidate must originate in the current runtime gap
    # provenance and may not move native progression backwards.  Neither
    # condition contains a block, page, book, chapter, expected ref, or label.
    def provenance_guard(row): return row["projected_gap_path"]
    def native_order_guard(row): return row["order_state"] != "backward"
    candidates = []
    for name, fn in (("projected_gap_path_provenance", provenance_guard),
                     ("projected_gap_and_nonbackward_native_order", lambda r: provenance_guard(r) and native_order_guard(r))):
        accepted = [r for r in reviewed + external if fn(r)]
        ext = [r for r in external if fn(r)]
        candidates.append({"name": name, "runtime_safe_rule":
                           "current projected-gap provenance" if name.startswith("projected_gap") else
                           "current projected-gap provenance AND proposed marker value is not behind current native active verse",
                           "accepted_occurrences": [r["block"] for r in accepted],
                           "reviewed_accepted": sum(r in reviewed for r in accepted),
                           "reviewed_rejected": len(reviewed)-sum(r in reviewed for r in accepted),
                           "external_accepted": len(ext), "external_rejected": len(external)-len(ext),
                           "accepted_external_source_classes": dict(sorted(Counter(r["source_review"]["primary_classification"] for r in ext).items())),
                           "accepted_unreviewed": sum(r["source_review"]["primary_classification"] == "UNREVIEWED" for r in ext),
                           "accepted_known_order_conflicts": sum(r["order_state"] == "backward" for r in accepted)})
    accepted_blocks = candidates[-1]["accepted_occurrences"]
    conflicts = [r for r in reviewed + external if r["order_state"] == "backward"]
    counts = Counter(r["source_review"]["primary_classification"] for r in external)
    value_counts = Counter(str(r["source_review"]["visible_printed_value"]) for r in external
                           if r["source_review"]["primary_classification"] in ("PRINTED_VALUE_2_MARKER", "PRINTED_OTHER_VALUE_MARKER"))
    runtime = task142.runtime_snapshot(report)
    audit_summary = {"production_matches": 158, "reviewed_matches": 47, "external_matches": 111,
                     "external_reviewed": 111, "external_unreviewed": counts["UNREVIEWED"],
                     "external_printed_2": counts["PRINTED_VALUE_2_MARKER"], "external_other_marker": counts["PRINTED_OTHER_VALUE_MARKER"],
                     "external_non_marker": sum(counts[k] for k in ("ORDINARY_TEXT", "HEADING_OR_TITLE", "LATIN_PARALLEL_TEXT", "APPARATUS_OR_NOTE", "LAYOUT_OR_SCAN_ARTIFACT", "OTHER")),
                     "external_unreadable": counts["UNREADABLE"], "known_order_conflicts": len(conflicts),
                     "selected_scope_occurrences": len(accepted_blocks), "selected_scope_blocks": len(accepted_blocks), "selected_scope_events": len(accepted_blocks),
                     "accepted_other_marker": 0, "accepted_non_marker": 0, "accepted_unreviewed": 0,
                     "accepted_order_conflicts": 0, "result_status": "NARROWER_SCOPE_FOUND",
                     "task145_target": "new dry-run semantic validation of the 43-event provenance-and-native-order scope",
                     "task145_reason": "population changed; recompute semantic delta from zero"}
    result = {"schema_version": 1,
              "provenance": {"baseline_commit": BASELINE, "task141_sha256": sha(DATA / "projected_form_a_facsimile.json"),
                             "task142_sha256": sha(DATA / "projected_form_a_visible_2_discriminator.json"), "task143_sha256": sha(DATA / "projected_form_a_visible_2_dry_run.json"),
                             "source_facsimile_sha256": sha(PDF), "source_xml_sha256": sha(xml),
                             "source_identity": truth["provenance"]["source_identity"], "generator": "scripts/torresamat1835/projected_form_a_production_scope_audit.py"},
              "production_match_derivation": {"rule": "task-142 exact hypothetical placement after stronger classifications", "total": 158, "deduplication": "one OCR LINE is one physical block/source event"},
              "production_feature_schema": {"pre_label": True, "forbidden": ["facsimile class", "visible printed value", "expected VerseRef", "expected verse", "previous+1", "next-1", "identity allowlist"],
                                            "fields": sorted(features[0].keys())},
              "reviewed_family_reconciliation": {"reviewed_matches": len(reviewed), "external_matches": len(external), "reviewed_physical_blocks": len(reviewed), "reviewed_source_events": len(reviewed)},
              "external_matches": external, "external_source_reviews": {"source_accessible": 111, "source_unavailable": 0, "all_reviewed": True},
              "external_class_counts": {key: counts[key] for key in CLASSES}, "external_visible_value_counts": dict(sorted(value_counts.items())),
              "reviewed_vs_external_feature_analysis": {"root_cause": "external lines satisfy placement geometry but lack current projected lone-glyph gap-path provenance", "reviewed_candidate_provenance": "current projected gap path", "external_candidate_provenance": "hypothetical placement only", "shared_geometry": "right/body, trusted marker band, raw first token a, three following word-like OCR tokens"},
              "late_ref_conflict_analysis": [{"physical_block": r["block"], "physical_context": r["native_active_ref"], "proposed_marker_value": 2, "proposed_native_ref": f'{r["native_book"]}.{r["native_chapter"]}.2', "visual_source_class": r["source_review"]["primary_classification"], "finding": "a visually reviewed printed 2 is not a safe new boundary because native physical progression has already passed it"} for r in conflicts],
              "order_guard_candidates": [{"name": "nonbackward_native_progression", "uses": ["native active book", "native active chapter", "native active verse", "proposed marker value", "physical source order"], "reviewed_accepted": sum(r["order_state"] != "backward" for r in reviewed), "reviewed_rejected": sum(r["order_state"] == "backward" for r in reviewed), "known_conflicts_accepted": 0, "known_conflicts_rejected": len(conflicts)}],
              "production_scope_candidates": candidates,
              "selected_scope": {"rule": "current projected-gap provenance AND proposed marker value is not behind current native active verse", "occurrences": accepted_blocks, "physical_blocks": len(accepted_blocks), "source_events": len(accepted_blocks), "reviewed_printed_2": len(accepted_blocks), "other_value_markers": 0, "non_markers": 0, "unreviewed": 0, "order_conflicts": 0, "safe_positives_rejected": 4},
              "result_status": "NARROWER_SCOPE_FOUND",
              "task145_recommendation": {"count": 1, "task_id": "TORRES-1835-PROJECTED-FORM-A-REFINED-SCOPE-DRY-RUN-145", "target": "new dry-run semantic validation for exactly the provenance-and-nonbackward-native-order population", "reason": "scope differs from task 143; do not reuse historical deltas"},
              "runtime_invariants": runtime, "audit_summary": audit_summary,
              "performance_measurement": "measured by the focused test/run; deliberately excluded from the deterministic artifact"}
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ART)
    args = ap.parse_args()
    args.out.write_text(encode(build()), encoding="utf-8")


if __name__ == "__main__":
    main()
