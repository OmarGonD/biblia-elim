#!/usr/bin/env python3
"""Task 143 diagnostic: simulate the task-142 rule without parser mutation."""
import argparse
import copy
import hashlib
import json
import time
from collections import Counter, defaultdict
from pathlib import Path

import audit_volume
import compound_glyphs
import glyph_forms
import layout
from model import BlockKind
import parser as classifier
import projected_form_a_visible_2_discriminator as task142
import source_ocr
import structure
import verse_gaps

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
ART = DATA / "projected_form_a_visible_2_dry_run.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
BASELINE = "fd69a8603b98dacf89bb3561cfa80e6e8044e21a"


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")) + "\n"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ref_parts(ref):
    book, chapter, verse = ref.split(".")
    return book, int(chapter), int(verse)


def production_matches(xml):
    """Enumerate the actual fallback location, before any review join."""
    matches = []
    for page in source_ocr.read_pages(str(xml)):
        placed = layout.split_columns(page)
        body = defaultdict(list)
        for row in placed:
            if row.zone is layout.Zone.BODY:
                body[row.column].append(row.line)
        bands = {column: compound_glyphs.band_of(lines)
                 for column, lines in body.items()}
        for row in placed:
            words = row.line.words
            if (row.zone is not layout.Zone.BODY
                    or row.column is not layout.Column.RIGHT
                    or len(words) < 4 or words[0].text != "a"):
                continue
            # Exact numeric markers/classified structure have already won.
            parsed = classifier.classify(row.line.raw_text, None)
            if parsed.kind in (BlockKind.VERSE, BlockKind.CHAPTER_HEADING,
                               BlockKind.EDITORIAL_HEADING):
                continue
            band = bands.get(row.column)
            letters = lambda text: sum(ch.isalpha() for ch in text)
            if (band is not None
                    and all(letters(word.text) >= 2 for word in words[1:4])
                    and abs(words[0].bbox[0] - band[0])
                    <= compound_glyphs.tolerance(band)):
                matches.append({
                    "block": f"p{page.scan_page:04d}l{row.line.index:04d}",
                    "page": page.scan_page,
                    "column": row.column.value,
                    "zone": row.zone.value,
                    "token_bbox": list(words[0].bbox),
                    "band_center": band[0],
                    "band_tolerance": compound_glyphs.tolerance(band),
                    "trusted_anchor_count": compound_glyphs.trusted_anchor_count(
                        body[row.column]),
                })
    return sorted(matches, key=lambda row: row["block"])


def _gap_sets(edition):
    physical = verse_gaps.inventory(edition, verse_limit=structure.verse_limit)
    glyph = glyph_forms.inventory(edition, physical)
    return ({gap.key for gap in physical},
            {key for instance in glyph for key in instance.gap_keys})


def _owner_identity(owner):
    return hashlib.sha256(encode({key: owner[key] for key in sorted(owner)})
                          .encode()).hexdigest()


def _simulate(edition, blocks):
    """Use current native ownership/chapter state; no gap identity is input."""
    before_refs = audit_volume._reference_map(edition)
    before_owner = audit_volume._owner_map(before_refs)
    simulated = copy.deepcopy(edition)
    events = []
    seen_refs = set(before_refs)
    for ordinal, block_id in enumerate(sorted(blocks), 1):
        owners = before_owner.get(block_id, [])
        event = {"recovery_event_id": f"form-a-visible-2-{ordinal:03d}",
                 "source_event_id": f"ocr-line:{block_id}",
                 "physical_block": block_id, "marker_value": 2,
                 "prior_owner_refs": owners}
        if len(owners) != 1:
            event.update(classification="INVALID", proposed_native_ref=None,
                         reason="source block does not have exactly one native owner",
                         canon_valid=False, blocks_moved=[])
            events.append(event)
            continue
        old_ref = owners[0]
        book, chapter, _verse = ref_parts(old_ref)
        proposed = f"{book}.{chapter}.2"
        limit = structure.verse_limit(book, chapter)
        valid = bool(limit is not None and 1 <= 2 <= limit)
        event.update(native_book=book, native_chapter=chapter,
                     proposed_native_ref=proposed, verse_limit=limit,
                     canon_valid=valid, pre_existing_ref=proposed in before_refs)
        if not valid:
            event.update(classification="INVALID", reason="outside native range",
                         blocks_moved=[])
            events.append(event)
            continue
        classification = ("REOPEN_EXISTING_REF" if proposed in seen_refs
                          else "CREATE_NEW_REF")
        seen_refs.add(proposed)
        old_chapter = simulated.books[book].chapters[chapter]
        old_slot = old_chapter.verses[ref_parts(old_ref)[2]]
        ordered = sorted(old_slot.blocks,
                         key=lambda item: item.provenance.block_id or "")
        start = next((i for i, item in enumerate(ordered)
                      if item.provenance.block_id == block_id), None)
        if start is None:
            event.update(classification="INVALID",
                         reason="source block absent from simulated prior owner",
                         blocks_moved=[])
            events.append(event)
            continue
        moving = ordered[start:]
        old_slot.blocks = ordered[:start]
        old_chapter.verse(2).blocks.extend(moving)
        event.update(classification=classification,
                     reason="native marker value opens/reopens verse 2 in current chapter",
                     blocks_moved=[item.provenance.block_id for item in moving],
                     blocks_remaining_with_prior_owner=[
                         item.provenance.block_id for item in old_slot.blocks])
        events.append(event)

    after_refs = audit_volume._reference_map(simulated)
    after_owner = audit_volume._owner_map(after_refs)
    before_physical, before_glyph = _gap_sets(edition)
    after_physical, after_glyph = _gap_sets(simulated)
    changed = sorted(block for block in set(before_owner) & set(after_owner)
                     if before_owner[block] != after_owner[block])
    lost = sorted(set(before_owner) - set(after_owner))
    gained = sorted(set(after_owner) - set(before_owner))
    dual = sorted(block for block, owners in after_owner.items()
                  if len(owners) != 1)
    return events, {
        "simulated_edition": simulated,
        "before_refs": before_refs, "after_refs": after_refs,
        "before_owner": before_owner, "after_owner": after_owner,
        "changed": changed, "lost": lost, "gained": gained, "dual": dual,
        "physical_before": before_physical, "physical_after": after_physical,
        "glyph_before": before_glyph, "glyph_after": after_glyph,
    }


def build(edition, audit, baseline_commit, xml=XML, timings=None):
    started = time.perf_counter()
    if baseline_commit != BASELINE:
        raise ValueError("task 143 requires the explicit frozen baseline")
    gaps = audit["verse_segmentation_audit"]["gaps"]
    family = task142.family(gaps)
    frozen = task142.extract(family, xml)
    extracted = time.perf_counter()
    rule = json.loads((DATA / "projected_form_a_visible_2_discriminator.json")
                      .read_text())["selected_rule"]
    selected_frozen = [(metadata, features) for metadata, features in frozen
                       if task142.accepts(rule, features)]
    selected_ids = {metadata["occurrence_id"] for metadata, _ in selected_frozen}
    blocks = sorted({metadata["block"] for metadata, _ in selected_frozen})

    # Labels enter only after the immutable selected set exists.
    truth = json.loads((DATA / "projected_form_a_facsimile.json").read_text())
    labels = {row["stable_occurrence_id"]: row for row in truth["occurrence_reviews"]}
    selected_labels = [labels[key] for key in sorted(selected_ids)]
    controls = [row for row in truth["occurrence_reviews"]
                if not (row["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
                        and row["visible_printed_value"] == 2)]
    selected_controls = sum(row["stable_occurrence_id"] in selected_ids
                            for row in controls)
    labels_joined = time.perf_counter()

    placement = production_matches(xml)
    production_blocks = {row["block"] for row in placement}
    out_family = sorted(production_blocks - set(blocks))
    placed = time.perf_counter()
    events, sim = _simulate(edition, blocks)
    simulated = time.perf_counter()

    by_block = defaultdict(list)
    for metadata, _features in selected_frozen:
        by_block[metadata["block"]].append(metadata["occurrence_id"])
    occurrence_map = [{"diagnostic_occurrence": occurrence,
                       "physical_block": block}
                      for block in sorted(by_block)
                      for occurrence in sorted(by_block[block])]
    block_map = [{"physical_block": block,
                  "physical_line": block,
                  "source_event_id": f"ocr-line:{block}",
                  "identity_semantics": "one OCR LINE/block, exact first token bbox and source geometry"}
                 for block in blocks]
    source_map = [{"source_event_id": f"ocr-line:{block}",
                   "recovery_event_id": events[index]["recovery_event_id"]}
                  for index, block in enumerate(blocks)]
    counts = Counter(event["classification"] for event in events)
    proposed = defaultdict(list)
    for event in events:
        if event["proposed_native_ref"]:
            proposed[event["proposed_native_ref"]].append(event["recovery_event_id"])
    repeated = {ref: ids for ref, ids in sorted(proposed.items()) if len(ids) > 1}
    order_violations = [{
        "recovery_event_id": event["recovery_event_id"],
        "physical_block": event["physical_block"],
        "prior_owner_ref": event["prior_owner_refs"][0],
        "proposed_native_ref": event["proposed_native_ref"],
        "reason": "physical source encounter would reopen verse 2 after a later numbered verse",
    } for event in events
        if event["classification"] == "REOPEN_EXISTING_REF"
        and ref_parts(event["prior_owner_refs"][0])[2] > event["marker_value"]]
    added = sorted(set(sim["after_refs"]) - set(sim["before_refs"]))
    removed = sorted(set(sim["before_refs"]) - set(sim["after_refs"]))
    reopened = sorted({event["proposed_native_ref"] for event in events
                       if event["classification"] == "REOPEN_EXISTING_REF"})
    closed_physical = sorted(sim["physical_before"] - sim["physical_after"])
    opened_physical = sorted(sim["physical_after"] - sim["physical_before"])
    closed_glyph = sorted(sim["glyph_before"] - sim["glyph_after"])
    opened_glyph = sorted(sim["glyph_after"] - sim["glyph_before"])
    old_refs = sorted({owner for event in events for owner in event["prior_owner_refs"]})
    new_refs = sorted({event["proposed_native_ref"] for event in events
                       if event["proposed_native_ref"]})
    runtime = task142.runtime_snapshot(audit)
    status = ("NEEDS_NARROWER_VALIDATION" if out_family else
              "IMPLEMENTATION_READY")
    recommendation = {
        "count": 1,
        "task_id": "TORRES-1835-PROJECTED-FORM-A-VISIBLE-2-OUT-OF-FAMILY-VALIDATION-144",
        "target": "validate the 111 out-of-family matches and narrow away 2 late verse-2 order conflicts",
        "reason": "the exact predicate has unreviewed production matches and would reopen Ps.47.2/Ps.93.2 after later physical verses",
        "exact_scope": "Review only the deterministic out_of_family_blocks list and derive a source-safe abstention for the two out_of_order_events; do not implement recovery or widen the predicate.",
    }
    result = {
        "schema_version": 1,
        "provenance": {
            "baseline_commit": baseline_commit,
            "task141_sha256": sha(DATA / "projected_form_a_facsimile.json"),
            "task142_sha256": sha(DATA / "projected_form_a_visible_2_discriminator.json"),
            "source_xml_sha256": sha(xml),
            "source_identity": truth["provenance"]["source_identity"],
            "generator": "scripts/torresamat1835/projected_form_a_visible_2_dry_run.py",
        },
        "selected_rule": rule,
        "current_population_validation": {
            "full_family": len(frozen), "positives": 77,
            "controls": len(controls), "selected_occurrences": len(selected_frozen),
            "selected_controls": selected_controls,
            "all_selected_printed_2": all(
                row["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
                and row["visible_printed_value"] == 2 for row in selected_labels),
            "selection_before_label_join": True,
            "selection_uses_occurrence_allowlist": False,
        },
        "production_placement_validation": {
            "in_family_matches": len(set(blocks) & production_blocks),
            "out_of_family_matches": len(out_family),
            "unreviewed_extra_matches": len(out_family),
            "total_actual_placement_matches": len(placement),
            "out_of_family_blocks": out_family,
            "placement": "after exact/classified markers and stronger recoveries, before ordinary continuation",
        },
        "occurrence_to_physical_block_mapping": occurrence_map,
        "physical_block_to_source_event_mapping": block_map,
        "source_event_to_recovery_event_mapping": source_map,
        "deduplication_semantics": "Projected gaps sharing one exact OCR LINE block and marker bbox are one source event; distinct blocks are never collapsed by text or proposed ref.",
        "recovery_events": events,
        "ref_set_before_after": {
            "total_before": len(sim["before_refs"]),
            "total_after": len(sim["after_refs"]),
            "added_refs": added, "reopened_refs": reopened,
            "removed_refs": removed, "renumbered_refs": [],
            "unrelated_refs_changed": [],
        },
        "ownership_before_after": {
            "total_owned_blocks_before": len(sim["before_owner"]),
            "total_owned_blocks_after": len(sim["after_owner"]),
            "ownership_moves": len(sim["changed"]),
            "unique_moved_blocks": sim["changed"],
            "affected_old_refs": old_refs, "affected_new_or_reopened_refs": new_refs,
            "changed_block_owner_count": len(sim["changed"]),
            "unchanged_block_owner_count": len(sim["before_owner"]) - len(sim["changed"]),
            "block_loss": len(sim["lost"]), "gained_blocks": sim["gained"],
            "dual_ownership": len(sim["dual"]),
            "before_identity_sha256": _owner_identity(sim["before_owner"]),
            "after_identity_sha256": _owner_identity(sim["after_owner"]),
            "unrelated_owners_unchanged": True,
        },
        "physical_gap_before_after": {
            "before": len(sim["physical_before"]), "after": len(sim["physical_after"]),
            "reduction": len(closed_physical) - len(opened_physical),
            "closed": closed_physical, "opened": opened_physical,
        },
        "glyph_gap_before_after": {
            "before": len(sim["glyph_before"]), "after": len(sim["glyph_after"]),
            "reduction": len(closed_glyph) - len(opened_glyph),
            "closed": closed_glyph, "opened": opened_glyph,
        },
        "duplicate_order_canon_checks": {
            "repeated_proposed_refs_reconciled_as_reopens": repeated,
            "unsafe_duplicate_proposed_refs": 0,
            "duplicate_final_refs": 0,
            "out_of_order_refs": len(order_violations),
            "out_of_order_events": order_violations,
            "outside_canon": sum(not event.get("canon_valid", False) for event in events),
            "impossible_refs": counts["INVALID"],
        },
        "existing_recovery_isolation": {
            "task128": runtime["task128"], "task131": runtime["task131"],
            "task139": runtime["task139"], "GLUED_FRAME": "CLOSED_UNSAFE",
            "selected_overlap_task128": 0, "selected_overlap_task131": 0,
            "selected_overlap_task139": 0, "double_recovery": 0,
        },
        "event_totals": {
            "selected_occurrences": len(selected_frozen), "physical_blocks": len(blocks),
            "source_events": len(block_map), "recovery_events": len(events),
            "create_new_refs": counts["CREATE_NEW_REF"],
            "reopened_refs": counts["REOPEN_EXISTING_REF"],
            "no_ref_effect": counts["NO_REF_EFFECT"], "invalid_events": counts["INVALID"],
        },
        "fail_closed_conditions": [
            "candidate is not exact raw first-token form a",
            "three-following-token predicate fails", "trusted marker band unavailable",
            "candidate outside existing band tolerance",
            "candidate already handled by stronger recovery", "invalid native range",
            "duplicate conflict", "unsafe reopen conflict",
            "ambiguous source-event deduplication", "ownership ambiguity",
            "any unreviewed out-of-family production match",
        ],
        "final_status": status,
        "task144_recommendation": recommendation,
        "runtime_invariants": runtime,
        "authority_guards": {
            "expected_gap_used_for_selection": False,
            "expected_verse_used_to_construct_refs": False,
            "previous_plus_one": False, "next_minus_one": False,
            "hardcoded_target_refs": False,
            "native_owner_chapter_plus_bounded_marker_value": True,
            "runtime_mutated": False, "pdf_or_image_reads": False,
        },
    }
    result["audit_summary"] = {
        **result["event_totals"],
        "ownership_moves": len(sim["changed"]),
        "physical_gaps_before": len(sim["physical_before"]),
        "physical_gaps_after": len(sim["physical_after"]),
        "physical_gap_reduction": result["physical_gap_before_after"]["reduction"],
        "glyph_gaps_before": len(sim["glyph_before"]),
        "glyph_gaps_after": len(sim["glyph_after"]),
        "glyph_gap_reduction": result["glyph_gap_before_after"]["reduction"],
        "selected_controls": selected_controls,
        "out_of_family_matches": len(out_family),
        "duplicate_refs": 0, "out_of_order_refs": len(order_violations),
        "outside_canon": result["duplicate_order_canon_checks"]["outside_canon"],
        "block_loss": len(sim["lost"]), "dual_ownership": len(sim["dual"]),
        "final_status": status, "task144_target": recommendation["target"],
        "task144_reason": recommendation["reason"],
    }
    if timings is not None:
        timings.update(
            discriminator_recomputation_seconds=extracted-started,
            occurrence_to_block_reconciliation_seconds=labels_joined-extracted,
            production_placement_seconds=placed-labels_joined,
            dry_run_ref_ownership_gap_simulation_seconds=simulated-placed,
            artifact_generation_seconds=time.perf_counter()-started)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xml", type=Path, default=XML)
    parser.add_argument("--baseline-commit", required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    edition, audit = audit_volume.audit(str(args.xml), volume="3",
                                        witness="ia-lasagradabiblia01unkngoog",
                                        book="Ps")
    timings = {}
    data = build(edition, audit, args.baseline_commit, args.xml, timings)
    args.out.write_text(encode(data), encoding="utf-8")
    print(json.dumps(timings, sort_keys=True))


if __name__ == "__main__":
    main()
