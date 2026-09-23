#!/usr/bin/env python3
"""Focused contracts for the task-143 semantic dry run."""
import hashlib
import json
import os
import sys
import time

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
sys.path.insert(0, DIR)

import audit_volume
import projected_form_a_visible_2_dry_run as diagnostic

ART = os.path.join(ROOT, "data/torresamat1835/projected_form_a_visible_2_dry_run.json")
XML = os.path.join(ROOT, "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
HISTORICAL = ("projected_form_a_facsimile.json",
              "projected_form_a_visible_2_discriminator.json",
              "remaining_glyph_reprioritization.json",
              "zero_anchor_io_recovery_validation.json",
              "no_trusted_band_discriminator.json", "projected_rejection_audit.json",
              "standalone_glyph_facsimile.json", "remaining_glyph_inventory.json")


def digest(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def main():
    started = time.perf_counter()
    before_historical = {name: digest(os.path.join(ROOT, "data/torresamat1835", name))
                         for name in HISTORICAL}
    canonical = open(ART, "rb").read()
    artifact = json.loads(canonical)
    edition, audit = audit_volume.audit(XML, volume="3",
                                        witness="ia-lasagradabiblia01unkngoog",
                                        book="Ps")
    first = diagnostic.build(edition, audit, diagnostic.BASELINE, XML)
    second = diagnostic.build(edition, audit, diagnostic.BASELINE, XML)
    assert diagnostic.encode(first).encode() == canonical
    assert diagnostic.encode(first) == diagnostic.encode(second)

    pop = first["current_population_validation"]
    assert pop == {"full_family": 260, "positives": 77, "controls": 183,
                   "selected_occurrences": 53, "selected_controls": 0,
                   "all_selected_printed_2": True,
                   "selection_before_label_join": True,
                   "selection_uses_occurrence_allowlist": False}
    totals = first["event_totals"]
    assert totals == {"selected_occurrences": 53, "physical_blocks": 47,
                      "source_events": 47, "recovery_events": 47,
                      "create_new_refs": 45, "reopened_refs": 2,
                      "no_ref_effect": 0, "invalid_events": 0}
    assert len(first["occurrence_to_physical_block_mapping"]) == 53
    assert len(first["physical_block_to_source_event_mapping"]) == 47
    assert len(first["source_event_to_recovery_event_mapping"]) == 47
    assert len({row["physical_block"] for row in
                first["physical_block_to_source_event_mapping"]}) == 47
    assert len({row["source_event_id"] for row in
                first["physical_block_to_source_event_mapping"]}) == 47
    assert len({row["recovery_event_id"] for row in first["recovery_events"]}) == 47
    assert all(event["marker_value"] == 2 and event["canon_valid"]
               for event in first["recovery_events"])
    assert all(event["proposed_native_ref"] ==
               f'{event["native_book"]}.{event["native_chapter"]}.2'
               for event in first["recovery_events"])
    assert set(event["classification"] for event in first["recovery_events"]) <= {
        "CREATE_NEW_REF", "REOPEN_EXISTING_REF", "NO_REF_EFFECT", "INVALID"}

    placement = first["production_placement_validation"]
    assert placement["in_family_matches"] == 47
    assert placement["out_of_family_matches"] == 111
    assert placement["unreviewed_extra_matches"] == 111
    assert placement["total_actual_placement_matches"] == 158
    assert len(placement["out_of_family_blocks"]) == 111

    refs = first["ref_set_before_after"]
    assert refs["total_before"] == 3849 and refs["total_after"] == 3894
    assert len(refs["added_refs"]) == 45
    assert refs["reopened_refs"] == ["Ps.47.2", "Ps.93.2"]
    assert refs["removed_refs"] == refs["renumbered_refs"] == []
    assert refs["unrelated_refs_changed"] == []
    checks = first["duplicate_order_canon_checks"]
    assert checks["unsafe_duplicate_proposed_refs"] == 0
    assert checks["duplicate_final_refs"] == 0
    assert checks["out_of_order_refs"] == 2
    assert [(row["prior_owner_ref"], row["proposed_native_ref"])
            for row in checks["out_of_order_events"]] == [
                ("Ps.47.7", "Ps.47.2"), ("Ps.93.19", "Ps.93.2")]
    assert checks["outside_canon"] == checks["impossible_refs"] == 0

    ownership = first["ownership_before_after"]
    assert ownership["total_owned_blocks_before"] == 25434
    assert ownership["total_owned_blocks_after"] == 25434
    assert ownership["ownership_moves"] == 595
    assert len(ownership["unique_moved_blocks"]) == 595
    assert ownership["block_loss"] == ownership["dual_ownership"] == 0
    assert ownership["gained_blocks"] == []
    assert ownership["unrelated_owners_unchanged"]
    assert ownership["unchanged_block_owner_count"] == 24839

    physical = first["physical_gap_before_after"]
    glyph = first["glyph_gap_before_after"]
    assert (physical["before"], physical["after"], physical["reduction"]) == (3254, 3209, 45)
    assert len(physical["closed"]) == 45 and physical["opened"] == []
    assert (glyph["before"], glyph["after"], glyph["reduction"]) == (1309, 1255, 54)
    assert len(glyph["closed"]) == 54 and glyph["opened"] == []

    isolation = first["existing_recovery_isolation"]
    assert isolation["task128"] == {"markers": 183, "refs": 177, "moves": 1355}
    assert isolation["task131"] == {"markers": 276, "refs": 276, "moves": 1900}
    assert isolation["task139"]["new_ref_identities"] == ["Ps.17.10"]
    assert isolation["selected_overlap_task128"] == 0
    assert isolation["selected_overlap_task131"] == 0
    assert isolation["selected_overlap_task139"] == 0
    assert isolation["double_recovery"] == 0
    assert isolation["GLUED_FRAME"] == "CLOSED_UNSAFE"

    runtime = first["runtime_invariants"]
    assert runtime["verse_refs"] == 3849
    assert runtime["physical_gaps"] == 3254 and runtime["glyph_gaps"] == 1309
    assert runtime["chapters"] == 337
    assert runtime["unresolved_chapter_claims"] == 0
    assert runtime["canonical_chapter_gaps"] == 0
    assert runtime["duplicate_refs"] == runtime["out_of_order_refs"] == 0
    assert runtime["outside_canon"] == 0 and runtime["ocr_blocks"] == 57700
    assert runtime["block_loss"] == runtime["dual_ownership"] == 0
    assert runtime["new_recovery"] is False

    guards = first["authority_guards"]
    assert guards == {"expected_gap_used_for_selection": False,
                      "expected_verse_used_to_construct_refs": False,
                      "previous_plus_one": False, "next_minus_one": False,
                      "hardcoded_target_refs": False,
                      "native_owner_chapter_plus_bounded_marker_value": True,
                      "runtime_mutated": False, "pdf_or_image_reads": False}
    source = open(diagnostic.__file__, encoding="utf-8").read()
    assert "previous + 1" not in source and "next - 1" not in source
    assert "expected missing" not in source
    assert first["final_status"] == "NEEDS_NARROWER_VALIDATION"
    assert first["task144_recommendation"]["count"] == 1
    assert "111" in first["task144_recommendation"]["target"]
    assert "2 late" in first["task144_recommendation"]["target"]
    assert first["provenance"]["baseline_commit"] == diagnostic.BASELINE
    assert audit["verse_segmentation_audit"][
        "projected_form_a_visible_2_dry_run"] == first["audit_summary"]

    after_historical = {name: digest(os.path.join(ROOT, "data/torresamat1835", name))
                        for name in HISTORICAL}
    assert before_historical == after_historical
    print(json.dumps({"status": first["final_status"],
                      "selected_occurrences": 53, "physical_blocks": 47,
                      "source_events": 47, "recovery_events": 47,
                      "out_of_family_matches": 111,
                      "ownership_moves": 595,
                      "physical_gap_reduction": 45,
                      "glyph_gap_reduction": 54,
                      "total_test_seconds": time.perf_counter() - started},
                     sort_keys=True))
    print("ok")


if __name__ == "__main__":
    main()
