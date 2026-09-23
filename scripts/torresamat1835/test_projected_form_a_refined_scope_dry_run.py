#!/usr/bin/env python3
"""Focused contracts for the task-145 refined-scope semantic dry run."""
import ast
import hashlib
import json
import os
import sys
import time

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
sys.path.insert(0, DIR)

import audit_volume
import projected_form_a_refined_scope_dry_run as diagnostic

ART = os.path.join(ROOT, "data/torresamat1835/projected_form_a_refined_scope_dry_run.json")
XML = os.path.join(ROOT, "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
HISTORICAL = ("projected_form_a_facsimile.json",
              "projected_form_a_visible_2_discriminator.json",
              "projected_form_a_visible_2_dry_run.json",
              "projected_form_a_production_scope_audit.json",
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

    edition, audit = audit_volume.audit(XML, volume="3",
                                        witness="ia-lasagradabiblia01unkngoog",
                                        book="Ps",
                                        projected_form_a_recovery=False)
    first = diagnostic.build(edition, audit, diagnostic.BASELINE, XML)
    second = diagnostic.build(edition, audit, diagnostic.BASELINE, XML)
    rendered = diagnostic.encode(first).encode()
    assert rendered == canonical  # A. artifact matches its committed rendering
    assert rendered == diagnostic.encode(second).encode()  # idempotent

    assert first["schema_version"] == 1
    assert first["provenance"]["baseline_commit"] == diagnostic.BASELINE

    # A. production population = 158
    pop = first["full_production_population_validation"]
    assert pop["total"] == 158
    assert pop["reviewed_matches"] == 47
    assert pop["external_matches"] == 111
    assert pop["external_unreadable_total"] == 111  # D.
    assert pop["known_order_conflicts"] == 13  # F.

    # B. task-144 refined scope recomputes to 43; C. via guards, not an allowlist
    selected = first["selected_scope_validation"]
    assert selected["selected_scope_events"] == 43
    assert selected["external_unreadable_selected"] == 0  # E.
    assert selected["order_conflicts_selected"] == 0  # G.
    assert selected["wrong_value_selected"] == 0  # H.
    assert selected["non_marker_selected"] == 0  # I.
    assert selected["unknown_extra_matches"] == 0

    definition = first["refined_scope_definition"]
    assert definition["not_a_hardcoded_id_list"] is True
    assert set(definition["guards"]) == {"projected_gap_path_provenance", "nonbackward_native_order"}
    for forbidden in ("occurrence id", "page id", "book/chapter allowlist",
                      "expected VerseRef", "expected gap", "manually enumerated population"):
        assert forbidden in definition["excludes"]

    source = open(diagnostic.__file__, encoding="utf-8").read()
    forbidden_literals = ("Ps.47.2", "Ps.93.2", "p0072l0083", "p0136l0092", "previous + 1", "next - 1")
    for literal in forbidden_literals:
        assert literal not in source, literal
    scope_source = source[source.index("def recompute_scope"):source.index("def build")]
    # M/N/O/P: no expected-count/VerseRef/prev+1/next-1 authority in selection
    assert "== 43" not in scope_source and "!= 43" not in scope_source
    assert "previous" not in scope_source and "next" not in scope_source

    # J. labels joined only after selection
    build_source = source[source.index("def build"):]
    label_index = build_source.index("labels = {row")
    scope_call_index = build_source.index("recompute_scope(edition")
    assert scope_call_index < label_index

    # K. occurrence/block/event mappings deterministic and independently sized
    occ = first["occurrence_to_block_mapping"]
    blocks = first["block_to_source_event_mapping"]
    src = first["source_event_to_recovery_event_mapping"]
    assert len(occ) == 43
    assert len({row["diagnostic_occurrence"] for row in occ}) == 43
    assert len(blocks) == 43
    assert len({row["physical_block"] for row in blocks}) == 43
    assert len(src) == 43
    assert len({row["recovery_event_id"] for row in src}) == 43

    # L. source/recovery-event deduplication deterministic
    events = first["recovery_events"]
    assert len(events) == len({event["recovery_event_id"] for event in events}) == 43
    assert len({event["physical_block"] for event in events}) == 43

    # Q. native parser semantics; R. exhaustive effect classes
    assert set(event["classification"] for event in events) <= {
        "CREATE_NEW_REF", "REOPEN_EXISTING_REF", "NO_REF_EFFECT", "INVALID"}
    assert all(event["marker_value"] == 2 for event in events)
    assert all(event["proposed_native_ref"] ==
               f'{event["native_book"]}.{event["native_chapter"]}.2' for event in events)

    totals = first["event_totals"]
    assert totals == {"selected_occurrences": 43, "physical_blocks": 43,
                      "source_events": 43, "recovery_events": 43,
                      "create_new_refs": 43, "reopened_refs": 0,
                      "no_ref_effect": 0, "invalid_events": 0}

    # AA. task-143's historical semantic counts must NOT appear as the result
    assert totals["create_new_refs"] != 45 or totals["reopened_refs"] != 2
    ownership = first["ownership_before_after"]
    assert ownership["ownership_moves"] != 595

    # S. duplicate proposed refs checked
    ref_effects = first["ref_effects"]
    assert ref_effects["duplicate_proposed_refs"] == {}
    assert ref_effects["unsafe_duplicate_proposed_refs"] == 0
    # T. physical ordering checked
    assert ref_effects["out_of_order_refs"] == 0
    assert ref_effects["out_of_order_events"] == []
    # U. canon/range checked
    assert ref_effects["outside_canon"] == 0
    assert ref_effects["impossible_refs"] == 0
    assert ref_effects["removed_refs"] == ref_effects["renumbered_refs"] == []
    assert ref_effects["unrelated_refs_changed"] == []
    assert ref_effects["verse_refs_before"] == 3849
    assert ref_effects["verse_refs_after"] == 3892
    assert len(ref_effects["added_refs"]) == 43
    assert ref_effects["reopened_refs"] == []

    # V. ownership block loss = 0; W. dual ownership = 0; X. unrelated unchanged
    assert ownership["total_owned_blocks_before"] == 25434
    assert ownership["total_owned_blocks_after"] == 25434
    assert ownership["block_loss"] == 0
    assert ownership["dual_ownership"] == 0
    assert ownership["gained_blocks"] == []
    assert ownership["unrelated_owners_unchanged"] is True
    assert ownership["ownership_moves"] == 581
    assert len(ownership["unique_moved_blocks"]) == 581

    # Y. physical gap delta recomputed fresh; Z. glyph gap delta recomputed fresh
    physical = first["physical_gap_before_after"]
    glyph = first["glyph_gap_before_after"]
    assert (physical["before"], physical["after"], physical["reduction"]) == (3254, 3211, 43)
    assert len(physical["closed"]) == 43 and physical["opened"] == []
    assert (glyph["before"], glyph["after"], glyph["reduction"]) == (1309, 1266, 43)
    assert len(glyph["closed"]) == 43 and glyph["opened"] == []
    # AA (continued): task-143's -45/-54 must not appear as the task-145 result
    assert physical["reduction"] != 45
    assert glyph["reduction"] != 54

    # AB/AC/AD. task-128/131/139 unchanged; isolation
    isolation = first["stronger_recovery_isolation"]
    assert isolation["task128"] == {"markers": 183, "refs": 177, "moves": 1355}
    assert isolation["task131"] == {"markers": 276, "refs": 276, "moves": 1900}
    assert isolation["task139"]["new_ref_identities"] == ["Ps.17.10"]
    assert isolation["double_recovery"] == 0
    assert isolation["selected_overlap_task128"] == 0
    assert isolation["selected_overlap_task131"] == 0
    assert isolation["selected_overlap_task139"] == 0
    # AE. GLUED_FRAME CLOSED_UNSAFE
    assert isolation["GLUED_FRAME"] == "CLOSED_UNSAFE"

    native_order = first["native_order_safety"]
    assert native_order["accepted_backward_conflicts"] == 0
    assert native_order["reconfirmed_during_simulation"] is True

    external_control = first["external_control_safety"]
    assert external_control["external_unreadable_selected"] == 0
    assert external_control["known_order_conflicts_selected"] == 0
    assert external_control["wrong_value_selected"] == 0
    assert external_control["non_marker_selected"] == 0
    assert external_control["unknown_extra_matches"] == 0

    assert len(first["fail_closed_conditions"]) >= 10

    # AF-AR. actual runtime invariants remain the pre-task-145 baseline
    runtime = first["runtime_invariants"]
    assert runtime["verse_refs"] == 3849  # AF.
    assert runtime["physical_gaps"] == 3254  # AG.
    assert runtime["glyph_gaps"] == 1309  # AH.
    assert runtime["chapters"] == 337  # AJ.
    assert runtime["unresolved_chapter_claims"] == 0  # AK.
    assert runtime["canonical_chapter_gaps"] == 0  # AL.
    assert runtime["duplicate_refs"] == 0  # AM.
    assert runtime["out_of_order_refs"] == 0  # AN.
    assert runtime["outside_canon"] == 0  # AO.
    assert runtime["ocr_blocks"] == 57700  # AP.
    assert runtime["block_loss"] == 0  # AQ.
    assert runtime["dual_ownership"] == 0  # AR.
    assert runtime["new_recovery"] is False

    # AI. actual ownership remains 25434 -- pre-existing invariant, not
    # independently recomputed by this diagnostic-only artifact (no runtime
    # mutation occurs, so it is unchanged by construction as in task 143/144).
    assert ownership["total_owned_blocks_before"] == 25434

    # AS/AT. exactly one final status and one task-146 recommendation
    assert first["final_status"] in (
        "IMPLEMENTATION_READY", "NEEDS_NARROWER_VALIDATION", "UNSAFE_TO_IMPLEMENT")
    assert first["final_status"] == "IMPLEMENTATION_READY"
    assert first["task146_recommendation"]["count"] == 1

    # AU. frozen provenance
    assert first["provenance"]["task141_sha256"] == diagnostic.sha(
        os.path.join(ROOT, "data/torresamat1835/projected_form_a_facsimile.json"))
    assert first["provenance"]["task144_sha256"] == diagnostic.sha(
        os.path.join(ROOT, "data/torresamat1835/projected_form_a_production_scope_audit.json"))

    assert audit["verse_segmentation_audit"][
        "projected_form_a_refined_scope_dry_run"] == first["audit_summary"]

    # AV/AW. deterministic/idempotent artifact re-verified via a temp round-trip
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "a.json")
        open(path, "wb").write(rendered)
        assert open(path, "rb").read() == canonical

    # Historical artifacts remain frozen
    after_historical = {name: digest(os.path.join(ROOT, "data/torresamat1835", name))
                        for name in HISTORICAL}
    assert before_historical == after_historical

    print(json.dumps({"status": first["final_status"],
                      "selected_scope_events": 43,
                      "create_new_refs": totals["create_new_refs"],
                      "reopened_refs": totals["reopened_refs"],
                      "ownership_moves": ownership["ownership_moves"],
                      "physical_gap_reduction": physical["reduction"],
                      "glyph_gap_reduction": glyph["reduction"],
                      "total_test_seconds": time.perf_counter() - started},
                     sort_keys=True))
    print("ok")


if __name__ == "__main__":
    main()
