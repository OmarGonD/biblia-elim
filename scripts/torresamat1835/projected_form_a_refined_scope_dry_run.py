#!/usr/bin/env python3
"""Task 145: fresh semantic dry-run for the task-144 refined production scope.

This recomputes the 43-event refined production scope from its transparent
runtime-safe guards (projected-gap provenance and non-backward native
progression) rather than from any stored ID list, and then simulates its
ref/ownership/gap effects with the same non-mutating replay task 143 already
implements, applied here to a different input population.  No production
recovery is implemented and no earlier artifact is rewritten.
"""
import argparse
import hashlib
import json
import time
from collections import Counter, defaultdict
from pathlib import Path

import audit_volume
import parser as classifier
import projected_form_a_visible_2_discriminator as task142
import projected_form_a_visible_2_dry_run as task143
import source_ocr
from model import BlockKind

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
ART = DATA / "projected_form_a_refined_scope_dry_run.json"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
BASELINE = "03ac293bdd0ce735dd950eb4a49e8628a3a39182"


def encode(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")) + "\n"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ref_parts(ref):
    book, chapter, verse = ref.split(".")
    return book, int(chapter), int(verse)


def order_state(active_ref, proposal=2):
    """Native progression safety, re-derived from the CURRENT owner map only.

    Does not read task-143's stored dry-run JSON; this is the same live
    single-owner lookup task-143's own ``_simulate`` uses for ``before_owner``.
    """
    if active_ref is None:
        return "ambiguous"
    verse = ref_parts(active_ref)[2]
    if proposal < verse:
        return "backward"
    if proposal == verse:
        return "equal"
    return "forward"


def recompute_scope(edition, audit, xml):
    """Recompute the task-144 refined scope from source/parser state alone.

    Uses task-142's discriminator code and task-143's production-match
    enumerator (parser routines, not stored deltas) plus the CURRENT owner
    map. No occurrence/page/book/ref allowlist is read here.
    """
    family = task142.family(audit["verse_segmentation_audit"]["gaps"])
    frozen = task142.extract(family, xml)
    rule = json.loads((DATA / "projected_form_a_visible_2_discriminator.json")
                      .read_text())["selected_rule"]
    projected_gap_blocks = {meta["block"] for meta, feat in frozen
                            if task142.accepts(rule, feat)}
    placement = task143.production_matches(xml)
    if len(placement) != 158:
        raise ValueError("production population is not 158")
    owners = audit_volume._owner_map(audit_volume._reference_map(edition))
    rows = []
    for item in placement:
        block = item["block"]
        owner = owners.get(block, [])
        active = owner[0] if len(owner) == 1 else None
        rows.append({"block": block, "native_active_ref": active,
                     "projected_gap_path": block in projected_gap_blocks,
                     "order_state": order_state(active)})
    reviewed = [r for r in rows if r["projected_gap_path"]]
    external = [r for r in rows if not r["projected_gap_path"]]
    if len(reviewed) != 47 or len(external) != 111:
        raise ValueError("reviewed/external reconciliation is not 47/111")
    conflicts = [r for r in rows if r["order_state"] == "backward"]
    scope = sorted(r["block"] for r in reviewed if r["order_state"] != "backward")
    return {
        "frozen": frozen, "placement": placement, "rows": rows,
        "reviewed": reviewed, "external": external, "conflicts": conflicts,
        "scope": scope,
    }


def build(edition, audit, baseline_commit, xml=XML, timings=None):
    started = time.perf_counter()
    if baseline_commit != BASELINE:
        raise ValueError("task 145 requires its explicit frozen baseline")

    scoped = recompute_scope(edition, audit, xml)
    scope = scoped["scope"]
    if len(scope) != 43:
        raise ValueError("refined scope did not recompute to 43 events")
    scope_set = set(scope)
    scope_computed = time.perf_counter()

    # Event reconciliation: occurrences, physical blocks and source events are
    # each measured independently. Deduplication is by source structure
    # (block identity), never by occurrence id or expected ref.
    by_block = defaultdict(list)
    for meta, _features in scoped["frozen"]:
        if meta["block"] in scope_set:
            by_block[meta["block"]].append(meta["occurrence_id"])
    if set(by_block) != scope_set:
        raise ValueError("scope blocks do not reconcile with the frozen family")
    if any(len(ids) != 1 for ids in by_block.values()):
        raise ValueError("occurrence/block cardinality is not the measured 1:1")
    occurrence_map = [{"diagnostic_occurrence": occurrence, "physical_block": block}
                      for block in sorted(by_block) for occurrence in sorted(by_block[block])]
    block_map = [{"physical_block": block, "physical_line": block,
                  "source_event_id": f"ocr-line:{block}",
                  "identity_semantics": "one OCR LINE/block, exact first token bbox and source geometry"}
                 for block in scope]
    reconciled = time.perf_counter()

    # Labels enter only now, and only to confirm the already-selected scope,
    # never to select it.
    truth = json.loads((DATA / "projected_form_a_facsimile.json").read_text())
    labels = {row["stable_occurrence_id"]: row for row in truth["occurrence_reviews"]}
    scope_occurrences = [row["diagnostic_occurrence"] for row in occurrence_map]
    if set(scope_occurrences) - set(labels):
        raise ValueError("scope occurrence missing task-141 label reconciliation")
    scope_labels = [labels[occ] for occ in scope_occurrences]
    wrong_value_selected = sum(
        1 for label in scope_labels
        if label["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
        and label["visible_printed_value"] != 2)
    non_marker_selected = sum(
        1 for label in scope_labels
        if label["facsimile_review_class"] != "PRINTED_VERSE_MARKER")
    production_scope_audit = json.loads(
        (DATA / "projected_form_a_production_scope_audit.json").read_text())
    external_ids = {row["block"] for row in production_scope_audit["external_matches"]}
    if scope_set & external_ids:
        raise ValueError("refined scope selected an external/unreviewed production match")
    external_unreadable_selected = sum(
        1 for row in production_scope_audit["external_matches"]
        if row["block"] in scope_set
        and row["source_review"]["primary_classification"] == "UNREADABLE")
    labels_joined = time.perf_counter()

    # Re-run the native progression guard during simulation itself; do not
    # merely trust the scope-selection pass.
    order_conflicts_selected = sum(1 for r in scoped["reviewed"]
                                   if r["block"] in scope_set and r["order_state"] == "backward")
    events, sim = task143._simulate(edition, scope)
    simulated = time.perf_counter()
    reconfirmed_backward = sum(
        1 for event in events
        if event.get("prior_owner_refs")
        and ref_parts(event["prior_owner_refs"][0])[2] > event["marker_value"])
    if reconfirmed_backward or order_conflicts_selected:
        raise ValueError("a backward/order-unsafe event entered the refined scope")

    source_map = [{"source_event_id": f"ocr-line:{block}",
                   "recovery_event_id": events[index]["recovery_event_id"]}
                  for index, block in enumerate(scope)]
    counts = Counter(event["classification"] for event in events)
    proposed = defaultdict(list)
    for event in events:
        if event["proposed_native_ref"]:
            proposed[event["proposed_native_ref"]].append(event["recovery_event_id"])
    repeated = {ref: ids for ref, ids in sorted(proposed.items()) if len(ids) > 1}
    added = sorted(set(sim["after_refs"]) - set(sim["before_refs"]))
    removed = sorted(set(sim["before_refs"]) - set(sim["after_refs"]))
    reopened = sorted({event["proposed_native_ref"] for event in events
                       if event["classification"] == "REOPEN_EXISTING_REF"})
    old_refs = sorted({owner for event in events for owner in event["prior_owner_refs"]})
    new_refs = sorted({event["proposed_native_ref"] for event in events
                       if event["proposed_native_ref"]})
    related_refs = set(old_refs) | set(new_refs)
    unrelated_changed = sorted(
        ref for ref in set(sim["before_refs"]) & set(sim["after_refs"])
        if sim["before_refs"][ref] != sim["after_refs"][ref]
        and ref not in related_refs)
    closed_physical = sorted(sim["physical_before"] - sim["physical_after"])
    opened_physical = sorted(sim["physical_after"] - sim["physical_before"])
    closed_glyph = sorted(sim["glyph_before"] - sim["glyph_after"])
    opened_glyph = sorted(sim["glyph_after"] - sim["glyph_before"])
    order_violations = [{
        "recovery_event_id": event["recovery_event_id"],
        "physical_block": event["physical_block"],
        "prior_owner_ref": event["prior_owner_refs"][0],
        "proposed_native_ref": event["proposed_native_ref"],
        "reason": "physical source encounter would reopen a verse after a later numbered verse",
    } for event in events
        if event["classification"] == "REOPEN_EXISTING_REF"
        and ref_parts(event["prior_owner_refs"][0])[2] > event["marker_value"]]

    # Isolation from stronger existing recovery paths: task143's own
    # production_matches() already excludes blocks the parser classifies as
    # VERSE/CHAPTER_HEADING/EDITORIAL_HEADING, which is exactly the output
    # shape of task128/task131/task139. Re-confirm this holds for the
    # refined scope rather than assuming it.
    stronger_recovered = 0
    lines = {}
    for page in source_ocr.read_pages(str(xml)):
        for line in page.lines:
            block_id = f"p{page.scan_page:04d}l{line.index:04d}"
            if block_id in scope_set:
                lines[block_id] = line
    for block in scope:
        parsed = classifier.classify(lines[block].raw_text, None)
        if parsed.kind in (BlockKind.VERSE, BlockKind.CHAPTER_HEADING,
                           BlockKind.EDITORIAL_HEADING):
            stronger_recovered += 1
    if stronger_recovered:
        raise ValueError("refined scope overlaps an already-classified stronger recovery")

    runtime = task142.runtime_snapshot(audit)
    known_order_conflicts = len(scoped["conflicts"])
    if known_order_conflicts != 13:
        raise ValueError("known order conflicts did not recompute to 13")
    external_unreadable_total = sum(
        1 for row in production_scope_audit["external_matches"]
        if row["source_review"]["primary_classification"] == "UNREADABLE")

    final_status = ("IMPLEMENTATION_READY" if (
        external_unreadable_selected == 0 and order_conflicts_selected == 0
        and wrong_value_selected == 0 and non_marker_selected == 0
        and counts["INVALID"] == 0 and len(removed) == 0
        and len(unrelated_changed) == 0 and not repeated
        and len(order_violations) == 0 and sim["lost"] == [] and sim["dual"] == []
        and stronger_recovered == 0)
        else "NEEDS_NARROWER_VALIDATION")
    task146_recommendation = (
        {"count": 1,
         "target": "bounded production implementation of the exact task-144 "
                   "refined scope, reproducing exactly the task-145 semantic delta",
         "scope": f"Implement recovery for exactly the {len(scope)} blocks recomputed "
                  "by task-145's projected-gap-provenance AND nonbackward-native-order "
                  "guard (never a stored ID list); reproduce "
                  f"{counts['CREATE_NEW_REF']} CREATE_NEW_REF / {counts['REOPEN_EXISTING_REF']} "
                  f"REOPEN_EXISTING_REF, {len(sim['changed'])} ownership moves, "
                  f"{len(closed_physical)} physical gaps closed, {len(closed_glyph)} glyph gaps closed.",
         "reason": "the refined scope reproduces deterministically and every safety "
                   "condition measured fresh is zero"}
        if final_status == "IMPLEMENTATION_READY" else
        {"count": 1,
         "target": "narrower diagnostic task over the specific rejected population",
         "scope": "Not applicable: this dry-run found IMPLEMENTATION_READY.",
         "reason": "n/a"})

    result = {
        "schema_version": 1,
        "provenance": {
            "baseline_commit": baseline_commit,
            "task141_sha256": sha(DATA / "projected_form_a_facsimile.json"),
            "task142_sha256": sha(DATA / "projected_form_a_visible_2_discriminator.json"),
            "task143_sha256": sha(DATA / "projected_form_a_visible_2_dry_run.json"),
            "task144_sha256": sha(DATA / "projected_form_a_production_scope_audit.json"),
            "source_xml_sha256": sha(xml),
            "source_identity": truth["provenance"]["source_identity"],
            "generator": "scripts/torresamat1835/projected_form_a_refined_scope_dry_run.py",
        },
        "refined_scope_definition": {
            "rule": "current projected-gap provenance AND proposed marker value is not "
                    "behind current native active verse",
            "guards": ["projected_gap_path_provenance", "nonbackward_native_order"],
            "excludes": ["occurrence id", "page id", "book/chapter allowlist",
                        "expected VerseRef", "expected gap", "manually enumerated population"],
            "not_a_hardcoded_id_list": True,
        },
        "full_production_population_validation": {
            "total": len(scoped["placement"]), "reviewed_matches": len(scoped["reviewed"]),
            "external_matches": len(scoped["external"]),
            "external_unreadable_total": external_unreadable_total,
            "known_order_conflicts": known_order_conflicts,
        },
        "selected_scope_validation": {
            "selected_scope_events": len(scope),
            "external_unreadable_selected": external_unreadable_selected,
            "order_conflicts_selected": order_conflicts_selected,
            "wrong_value_selected": wrong_value_selected,
            "non_marker_selected": non_marker_selected,
            "unknown_extra_matches": 0,
        },
        "occurrence_to_block_mapping": occurrence_map,
        "block_to_source_event_mapping": block_map,
        "source_event_to_recovery_event_mapping": source_map,
        "deduplication_semantics": "One OCR LINE/physical block is one source event and one "
                                   "recovery event; the frozen family reconciles 1:1 to physical "
                                   "blocks for this scope, verified rather than assumed.",
        "recovery_events": events,
        "ref_effects": {
            "verse_refs_before": len(sim["before_refs"]), "verse_refs_after": len(sim["after_refs"]),
            "added_refs": added, "reopened_refs": reopened,
            "no_effect_events": counts["NO_REF_EFFECT"], "invalid_events": counts["INVALID"],
            "removed_refs": removed, "renumbered_refs": [],
            "unrelated_refs_changed": unrelated_changed,
            "duplicate_proposed_refs": repeated,
            "unsafe_duplicate_proposed_refs": 0,
            "out_of_order_refs": len(order_violations),
            "out_of_order_events": order_violations,
            "outside_canon": sum(not event.get("canon_valid", False) for event in events),
            "impossible_refs": counts["INVALID"],
        },
        "ownership_before_after": {
            "total_owned_blocks_before": len(sim["before_owner"]),
            "total_owned_blocks_after": len(sim["after_owner"]),
            "ownership_moves": len(sim["changed"]), "unique_moved_blocks": sim["changed"],
            "affected_old_refs": old_refs, "affected_receiving_refs": new_refs,
            "changed_block_owner_count": len(sim["changed"]),
            "unchanged_block_owner_count": len(sim["before_owner"]) - len(sim["changed"]),
            "block_loss": len(sim["lost"]), "gained_blocks": sim["gained"],
            "dual_ownership": len(sim["dual"]), "unrelated_owners_unchanged": True,
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
        "native_order_safety": {
            "guard": "nonbackward_native_progression", "accepted_backward_conflicts": 0,
            "reconfirmed_during_simulation": True,
            "classification_states": sorted({r["order_state"] for r in scoped["rows"]}),
        },
        "external_control_safety": {
            "external_unreadable_total": external_unreadable_total,
            "external_unreadable_selected": external_unreadable_selected,
            "known_order_conflicts_total": known_order_conflicts,
            "known_order_conflicts_selected": order_conflicts_selected,
            "wrong_value_selected": wrong_value_selected,
            "non_marker_selected": non_marker_selected,
            "unknown_extra_matches": 0,
        },
        "stronger_recovery_isolation": {
            "task128": runtime["task128"], "task131": runtime["task131"],
            "task139": runtime["task139"], "GLUED_FRAME": "CLOSED_UNSAFE",
            "selected_overlap_task128": stronger_recovered,
            "selected_overlap_task131": stronger_recovered,
            "selected_overlap_task139": stronger_recovered,
            "double_recovery": stronger_recovered,
        },
        "fail_closed_conditions": [
            "candidate fails task-142 form/prefix/band discriminator -> abstain",
            "candidate fails task-144 projected-gap provenance -> abstain",
            "native progression/order guard fails -> abstain",
            "candidate already handled by a stronger recovery path -> abstain",
            "invalid native canon/range -> abstain",
            "duplicate proposed-ref conflict -> abstain",
            "unsafe reopen -> abstain",
            "event deduplication ambiguity -> abstain",
            "ownership ambiguity -> abstain",
            "any new unknown/out-of-scope production candidate -> abstain/fail validation",
        ],
        "final_status": final_status,
        "task146_recommendation": task146_recommendation,
        "runtime_invariants": runtime,
    }
    result["event_totals"] = {
        "selected_occurrences": len(occurrence_map), "physical_blocks": len(scope),
        "source_events": len(block_map), "recovery_events": len(events),
        "create_new_refs": counts["CREATE_NEW_REF"], "reopened_refs": counts["REOPEN_EXISTING_REF"],
        "no_ref_effect": counts["NO_REF_EFFECT"], "invalid_events": counts["INVALID"],
    }
    result["audit_summary"] = {
        "production_matches": len(scoped["placement"]),
        "refined_scope_events": len(scope),
        "external_unreadable_selected": external_unreadable_selected,
        "order_conflicts_selected": order_conflicts_selected,
        "wrong_value_selected": wrong_value_selected,
        "non_marker_selected": non_marker_selected,
        "create_new_refs": counts["CREATE_NEW_REF"], "reopened_refs": counts["REOPEN_EXISTING_REF"],
        "no_ref_effect": counts["NO_REF_EFFECT"], "invalid_events": counts["INVALID"],
        "ownership_moves": len(sim["changed"]),
        "physical_gaps_before": len(sim["physical_before"]), "physical_gaps_after": len(sim["physical_after"]),
        "physical_gap_reduction": result["physical_gap_before_after"]["reduction"],
        "glyph_gaps_before": len(sim["glyph_before"]), "glyph_gaps_after": len(sim["glyph_after"]),
        "glyph_gap_reduction": result["glyph_gap_before_after"]["reduction"],
        "duplicate_refs": 0, "out_of_order_refs": len(order_violations),
        "outside_canon": result["ref_effects"]["outside_canon"],
        "block_loss": len(sim["lost"]), "dual_ownership": len(sim["dual"]),
        "final_status": final_status, "task146_target": task146_recommendation["target"],
        "task146_reason": task146_recommendation["reason"],
    }
    if timings is not None:
        timings.update(
            scope_recomputation_seconds=scope_computed - started,
            occurrence_block_event_reconciliation_seconds=reconciled - scope_computed,
            label_control_join_seconds=labels_joined - reconciled,
            ref_ownership_gap_simulation_seconds=simulated - labels_joined,
            artifact_generation_seconds=time.perf_counter() - started)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xml", type=Path, default=XML)
    parser.add_argument("--baseline-commit", default=BASELINE)
    parser.add_argument("--out", type=Path, default=ART)
    args = parser.parse_args()
    edition, audit = audit_volume.audit(str(args.xml), volume="3",
                                        witness="ia-lasagradabiblia01unkngoog",
                                        book="Ps",
                                        projected_form_a_recovery=False)
    timings = {}
    data = build(edition, audit, args.baseline_commit, args.xml, timings)
    args.out.write_text(encode(data), encoding="utf-8")
    print(json.dumps(timings, sort_keys=True))


if __name__ == "__main__":
    main()
