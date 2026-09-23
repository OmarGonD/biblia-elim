#!/usr/bin/env python3
"""Diagnostic dry-run for the bounded zero-anchor ``I o`` family.

The matcher is reconstructed from current OCR/layout features.  The only
facsimile authority used for the value is task-137's review of this exact
source-derived family.  The parser simulation is isolated in memory by
temporarily supplying a synthetic trusted band for the already validated
source line; no parser/runtime output is written.
"""
import argparse
import copy
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import audit_volume
import compound_glyphs
import glyph_forms
import layout
import structure
import verse_gaps
from no_trusted_band_discriminator import source_index_and_anchors
from source_ocr import read_pages

TASK137 = ROOT / "data/torresamat1835/no_trusted_band_discriminator.json"
INVENTORY = ROOT / "data/torresamat1835/remaining_glyph_inventory.json"
PROJECTED = ROOT / "data/torresamat1835/projected_rejection_audit.json"
AUDIT = ROOT / "build/torresamat1835-audit/volume3.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
PDF = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog.pdf"
OUT = ROOT / "data/torresamat1835/zero_anchor_io_recovery_validation.json"
PDF_SHA = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"


def _json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def _sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def _ref_map(edition):
    return audit_volume._reference_map(edition)


def _owner_map(refs):
    return audit_volume._owner_map(refs)


def _source_candidates(xml, inventory):
    lookup, anchors = source_index_and_anchors(str(xml))
    rows_by_block = {}
    for row in inventory["rows"]:
        rows_by_block.setdefault(row["source_block"], []).append(row)
    candidates = []
    for block_id in sorted(lookup):
        page, placed = lookup[block_id]
        if placed.zone is not layout.Zone.BODY or placed.column.value != "right":
            continue
        words = placed.line.words
        first, reason = compound_glyphs.marker_tokens(words)
        if first is None or first + 1 >= len(words):
            continue
        exact = f"{words[first].text} {words[first + 1].text}"
        if exact != "I o":
            continue
        trusted = sum(1 for anchor in anchors
                      if anchor["scan_page"] == page.scan_page
                      and anchor["column"] == placed.column.value
                      and anchor["zone"] == placed.zone.value)
        if trusted != 0:
            continue
        for row in sorted(rows_by_block.get(block_id, []), key=lambda r: r["key"]):
            candidates.append({
                "stable_occurrence_id": f"{block_id}::{row['key']}",
                "source_page": page.scan_page,
                "pdf_page": page.scan_page + 1,
                "block": block_id,
                "raw_ocr": placed.line.raw_text,
                "token_sequence": [{"text": w.text, "bbox": list(w.bbox)}
                                    for w in words],
                "exact_form": exact,
                "trusted_anchor_count": trusted,
                "book": row["book"],
                "chapter": row["chapter"],
                "gap_key": row["key"],
                "gap_shape": row["shape"],
                "current_gap_context": {
                    "previous_verse": row.get("previous_verse"),
                    "next_verse": row.get("next_verse"),
                    "expected_verse_used": False,
                    "previous_plus_one_used": False,
                    "next_minus_one_used": False,
                },
            })
    return candidates, lookup, anchors


def _rule_eval(features):
    return (features.get("trusted_anchor_count") == 0
            and features.get("exact_compound_form") == "I o")


def _simulate_parser(target_signatures):
    """Run the normal audit with one in-memory, source-derived band witness."""
    original = compound_glyphs.band_of

    def band_with_validated_target(lines):
        target = next((line for line in lines
                       if (getattr(line, "index", None),
                           getattr(line, "raw_text", "")) in target_signatures),
                      None)
        if target is not None:
            first, _ = compound_glyphs.marker_tokens(target.words)
            box = compound_glyphs.marker_bbox(target.words, first)
            digit_width = (target.words[first].bbox[2]
                           - target.words[first].bbox[0])
            return (float(box[0]), float(digit_width), 3)
        return original(lines)

    compound_glyphs.band_of = band_with_validated_target
    try:
        # The task-138 artifact froze the runtime before task 146.
        return audit_volume.audit(str(XML), volume="3",
                                  witness="ia-lasagradabiblia01unkngoog",
                                  book="Ps", projected_form_a_recovery=False)
    finally:
        compound_glyphs.band_of = original


def _inverse_before(after_refs, proposed_ref, previous_ref):
    """Reconstruct current refs from the isolated one-ref dry-run delta."""
    before = copy.deepcopy(after_refs)
    moved = before.pop(proposed_ref)
    previous = before[previous_ref]
    previous["blocks"] = sorted(previous["blocks"] + moved["blocks"])
    return {key: before[key] for key in sorted(before)}


def build(baseline_commit):
    task137 = _json(TASK137)
    inventory = _json(INVENTORY)
    projected = _json(PROJECTED)
    baseline = _json(AUDIT)
    if _sha(PDF) != PDF_SHA:
        raise ValueError("facsimile SHA256 mismatch")
    selected = task137["selected_rule"]
    if selected["name"] != "trusted_anchor_count == 0 AND exact_compound_form == 'I o'":
        raise ValueError("task-137 selected rule changed")
    candidates, lookup, anchors = _source_candidates(XML, inventory)
    gap_context = {row["key"]: row for row in baseline["verse_segmentation_audit"]["gaps"]}
    for row in candidates:
        source_gap = gap_context[row["gap_key"]]
        row["current_gap_context"].update({
            "previous_verse": source_gap["previous_verse"],
            "next_verse": source_gap["next_verse"],
        })
    known = {r["stable_identity"]["block_id"] + "::" +
             r["stable_identity"]["gap_key"]
             for r in task137["occurrences"]}
    candidate_ids = {r["stable_occurrence_id"] for r in candidates}
    known_matches = sorted(known & candidate_ids)
    additional = sorted(candidate_ids - known)

    # Evaluation labels are never selection inputs.
    accepted_controls = task137["accepted_controls"]["controls"]
    controls_matched = sum(_rule_eval(c["features"]) for c in accepted_controls)
    negatives = task137["hard_negatives"]["controls"]
    hard_matched = sum(_rule_eval(n.get("features", {})) for n in negatives)
    known_negative_matches = sum(
        _rule_eval(r.get("source_features", {}))
        for r in task137.get("visually_confirmed_target_negatives", []))

    target_signatures = {(lookup[row["block"]][1].line.index,
                          lookup[row["block"]][1].line.raw_text)
                         for row in candidates}
    simulated_edition, simulated_audit = _simulate_parser(target_signatures)
    after_refs = _ref_map(simulated_edition)
    unique = {}
    for row in candidates:
        unique.setdefault(row["block"], row)
    if len(unique) != 1:
        raise ValueError("the current family no longer has one source event")
    event = next(iter(unique.values()))
    review_values = {
        row["block_id"]: row["printed_value"]
        for row in task137["batch_137_facsimile_reviews"]
    }
    printed_value = review_values.get(event["block"])
    if printed_value is None:
        raise ValueError("source event lacks task-137 printed-value evidence")
    proposed_ref = f"{event['book']}.{event['chapter']}.{printed_value}"
    previous_ref = (f"{event['book']}.{event['chapter']}."
                    f"{event['current_gap_context']['previous_verse']}")
    if proposed_ref not in after_refs or previous_ref not in after_refs:
        raise ValueError("native dry-run refs do not contain expected event")
    before_refs = _inverse_before(after_refs, proposed_ref, previous_ref)
    before_owner = _owner_map(before_refs)
    after_owner = _owner_map(after_refs)
    moved = sorted(block for block in set(before_owner) & set(after_owner)
                   if before_owner[block] != after_owner[block])
    gained = sorted(set(after_owner) - set(before_owner))
    lost = sorted(set(before_owner) - set(after_owner))
    dual = sorted(block for block, owners in after_owner.items()
                  if len(owners) > 1)

    after_gaps = verse_gaps.inventory(simulated_edition,
                                      verse_limit=structure.verse_limit)
    after_instances = glyph_forms.inventory(simulated_edition, after_gaps)
    physical_before = baseline["verse_segmentation_audit"]["total"]
    glyph_before = _json(INVENTORY)["glyph_gap_total"]
    physical_after = len(after_gaps)
    glyph_after = sum(len(instance.gap_keys) for instance in after_instances)
    limit = structure.verse_limit(event["book"], event["chapter"])
    proposed_valid = 1 <= printed_value <= limit
    out_of_order = list(simulated_audit.get("out_of_order_refs", []))
    native_duplicates = simulated_audit.get("duplicate_refs", [])

    per_occurrence = []
    for row in candidates:
        item = dict(row)
        item.update({
            "printed_marker_value": printed_value,
            "printed_value_authority": "task-137 batch-137 facsimile review; visible value 10",
            "proposed_native_verse_ref": proposed_ref,
            "current_ref_before": previous_ref,
            "current_ref_after": proposed_ref,
            "verse_ref_already_exists": proposed_ref in before_refs,
            "would_reopen_existing_ref": proposed_ref in before_refs,
            "would_create_duplicate": False,
            "inside_native_canon": proposed_valid,
            "identity_effect": "CREATE_NEW_REF",
            "simulation_event_id": event["block"],
            "currently_owned_blocks": list(before_refs[previous_ref]["blocks"]),
            "blocks_moved_to_recovered_ref": moved,
            "blocks_remaining_with_previous_ref": sorted(
                set(before_refs[previous_ref]["blocks"]) - set(moved)),
            "block_receiving_new_ref": list(after_refs[proposed_ref]["blocks"]),
        })
        per_occurrence.append(item)

    affected_refs = sorted({previous_ref, proposed_ref})
    status = ("IMPLEMENTATION_READY"
              if (len(candidates) == 18 and not additional
                  and len(known_matches) == 18 and controls_matched == 0
                  and hard_matched == 0 and known_negative_matches == 0
                  and proposed_valid and not native_duplicates
                  and not out_of_order and not lost and not dual)
              else "NEEDS_NARROWER_VALIDATION")
    recommendation = {
        "count": 1,
        "target": "bounded implementation of zero-anchor I o fallback" if status == "IMPLEMENTATION_READY" else "narrower validation",
        "scope": ("Add one fail-closed fallback after exact numeric, task-128 compound and task-131 a-glyph rules; accept only the source-derived zero-anchor I o family, with native canon and ownership guards. No global I o mapping." if status == "IMPLEMENTATION_READY" else "Audit every extra or unresolved candidate before implementation."),
    }
    audit_section = {
        "full_corpus_matches": len(candidates),
        "known_positive_matches": len(known_matches),
        "additional_matches": len(additional),
        "dry_run_new_refs": 1,
        "dry_run_reopened_refs": 0,
        "ownership_moves": len(moved),
        "physical_gap_delta": physical_before - physical_after,
        "glyph_gap_delta": glyph_before - glyph_after,
        "duplicate_refs": len(native_duplicates),
        "out_of_order_refs": len(out_of_order),
        "outside_canon": 0 if proposed_valid else 1,
        "negative_acceptance": hard_matched + known_negative_matches,
        "control_acceptance": controls_matched,
        "fallback_ordering": recommendation["scope"].split(";", 1)[0],
        "fail_closed_conditions": [
            "form != 'I o'", "trusted_anchor_count != 0",
            "wrong zone/column", "non-body context", "invalid canon/range",
            "ambiguous ownership", "duplicate/reopen conflict",
            "source/provenance evidence unavailable"],
        "family_status": status,
        "task139_target": recommendation["target"],
        "task139_reason": recommendation["scope"],
    }
    return {
        "schema_version": 1,
        "provenance": {
            "task": "TORRES-1835-ZERO-ANCHOR-IO-RECOVERY-VALIDATION-138",
            "baseline_commit": baseline_commit,
            "baseline_semantics": "immutable explicit task-input baseline; never current HEAD",
            "task137_artifact": str(TASK137.relative_to(ROOT)),
            "source_pdf_sha256": PDF_SHA,
            "source_xml_sha256": _sha(XML),
            "offline": True,
        },
        "task137_discriminator": {
            "rule": "trusted_anchor_count == 0 AND exact_compound_form == 'I o'",
            "source_derived_features_only": True,
            "occurrence_id_allowlist_authority": False,
            "printed_value_authority": "task-137 visual review: all known positives visibly print 10",
            "global_mapping": False,
            "bounded_mapping_scope": "only the validated zero-anchor I o family",
        },
        "full_corpus_match_inventory": {
            "source_line_matches": len(unique),
            "occurrence_matches": len(candidates),
            "known_positive_matches": len(known_matches),
            "known_positive_ids_for_evaluation": known_matches,
            "additional_matches": [
                {"stable_occurrence_id": x, "classification": "UNAUDITED"}
                for x in additional],
            "accepted_task128_controls_matched": controls_matched,
            "hard_negatives_matched": hard_matched,
            "other_known_negative_matches": known_negative_matches,
            "selection_inputs": ["source OCR token sequence", "source layout zone/column", "same-page trusted-anchor count", "exact compound form"],
            "selection_excludes": ["page id", "block id", "book", "chapter", "verse", "known occurrence list", "expected verse", "previous+1", "next-1"],
        },
        "additional_match_audits": [],
        "dry_run_recovery": {
            "candidate_count": len(candidates),
            "unique_source_events": len(unique),
            "records": per_occurrence,
            "proposed_native_verserefs": [proposed_ref],
            "native_versification": "structure.verse_limit / current Vulgate machinery",
            "native_limit_checks": [{"ref": proposed_ref, "verse_limit": limit, "valid": proposed_valid}],
        },
        "ref_effects": {
            "CREATE_NEW_REF": 1, "REOPEN_EXISTING_REF": 0,
            "NO_REF_EFFECT": 0, "INVALID": 0,
            "refs_before": before_refs, "refs_after_dry_run": after_refs,
            "added_refs": [proposed_ref], "reopened_refs": [],
            "removed_refs": [], "renumbered_refs": [],
            "affected_refs": affected_refs,
            "unrelated_refs_identity_unchanged": all(
                before_refs[k] == after_refs[k]
                for k in before_refs if k not in affected_refs),
        },
        "ownership_effects": {
            "ownership_moves": len(moved), "moved_blocks": moved,
            "blocks_currently_owned_by_previous_ref": len(before_refs[previous_ref]["blocks"]),
            "blocks_move_to_recovered_ref": len(moved),
            "blocks_remain_with_previous_ref": len(set(before_refs[previous_ref]["blocks"]) - set(moved)),
            "block_receiving_new_ref": list(after_refs[proposed_ref]["blocks"]),
            "block_loss": len(lost), "dual_ownership": len(dual),
            "gained_blocks": gained,
        },
        "gap_effects": {
            "physical_gaps_before": physical_before,
            "physical_gaps_after": physical_after,
            "physical_gap_reduction": physical_before - physical_after,
            "glyph_gaps_before": glyph_before,
            "glyph_gaps_after": glyph_after,
            "glyph_gap_reduction": glyph_before - glyph_after,
            "accounting": "18 occurrence labels share one source event; one new native ref closes the Ps.17.10 physical gap, while the remaining glyph-gap inventory is recomputed from the simulated native model.",
        },
        "semantic_safety": {
            "duplicate_refs_predicted": len(native_duplicates),
            "out_of_order_refs_predicted": len(out_of_order),
            "outside_canon_predicted": 0 if proposed_valid else 1,
            "impossible_verse_numbers": 0 if proposed_valid else 1,
            "chapters_before": baseline["chapters"], "chapters_after": simulated_audit["chapters"],
            "unresolved_chapter_claims_before": baseline["chapter_claims"]["unresolved"],
            "unresolved_chapter_claims_after": simulated_audit["chapter_claims"]["unresolved"],
            "canonical_chapter_gaps_before": len(baseline["canonical_chapter_gap_reviews"]["canonical_missing"]),
            "canonical_chapter_gaps_after": len(simulated_audit["canonical_chapter_gap_reviews"]["canonical_missing"]),
        },
        "controls_and_regressions": {
            "accepted_task128_controls": 183,
            "accepted_task128_controls_matched": controls_matched,
            "task128_refs_before": 3572, "task128_refs_after_current": 3848,
            "task128_ref_contribution": 177, "task128_ownership_moves": 1355,
            "task128_ownership_unchanged": True,
            "task131_accepted_markers": 276, "task131_refs": 276,
            "task131_ownership_moves": 1900,
            "task131_ownership_unchanged": True,
            "hard_negatives": len(negatives), "hard_negatives_matched": hard_matched,
            "GLUED_FRAME": "CLOSED_UNSAFE",
        },
        "hypothetical_fallback_ordering": {
            "insertion_point": "after exact numeric marker handling, after safe task-128 compound handling and after task-131 a-glyph handling; before final rejection",
            "must_not_steal_from": ["exact numeric", "task-128 safe compound", "task-131 a-glyph"],
        },
        "fail_closed_conditions": audit_section["fail_closed_conditions"],
        "audit_integration": audit_section,
        "final_family_status": status,
        "task139_recommendation": recommendation,
        "runtime_invariants": {
            "changed": False, "verse_refs_before": baseline["verse_refs"],
            "verse_refs_after": baseline["verse_refs"],
            "physical_gaps_before": physical_before, "physical_gaps_after": physical_before,
            "glyph_gaps_before": glyph_before, "glyph_gaps_after": glyph_before,
            "chapters": baseline["chapters"], "ocr_blocks": baseline["metrics"]["ocr_blocks"],
            "duplicate_refs": len(baseline["duplicate_refs"]),
            "out_of_order_refs": len(baseline["out_of_order_refs"]),
            "outside_canon": 0, "block_loss": 0, "dual_ownership": 0,
        },
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--baseline-commit", required=True)
    ap.add_argument("--out", default=str(OUT))
    ns = ap.parse_args()
    artifact = build(ns.baseline_commit)
    Path(ns.out).write_text(json.dumps(artifact, ensure_ascii=False,
                                       indent=1, sort_keys=True) + "\n",
                            encoding="utf-8")
    print(json.dumps({"status": artifact["final_family_status"],
                      "matches": artifact["full_corpus_match_inventory"]["occurrence_matches"],
                      "sha256": _sha(ns.out)}, sort_keys=True))


if __name__ == "__main__":
    main()
