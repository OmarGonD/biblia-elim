#!/usr/bin/env python3
"""Focused contracts for task 144's non-mutating production-scope audit."""
import ast
import json
import os
import tempfile

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
import sys
sys.path.insert(0, DIR)

import projected_form_a_production_scope_audit as diagnostic


def main():
    os.chdir(ROOT)
    first = diagnostic.build()
    second = diagnostic.build()
    canonical = open(diagnostic.ART, "rb").read()
    rendered = diagnostic.encode(first).encode()
    assert rendered == canonical == diagnostic.encode(second).encode()
    assert first["schema_version"] == 1
    assert first["provenance"]["baseline_commit"] == diagnostic.BASELINE
    assert first["production_match_derivation"]["total"] == 158
    assert first["reviewed_family_reconciliation"] == {"reviewed_matches": 47, "external_matches": 111,
                                                         "reviewed_physical_blocks": 47, "reviewed_source_events": 47}
    external = first["external_matches"]
    assert len(external) == len({r["block"] for r in external}) == 111
    assert all(r["candidate_provenance"] == "hypothetical_production_placement" for r in external)
    assert not any(r["projected_gap_path"] for r in external)
    assert all(r["source_review"]["primary_classification"] for r in external)
    assert sum(first["external_class_counts"].values()) == 111
    assert first["external_source_reviews"] == {"source_accessible": 111, "source_unavailable": 0, "all_reviewed": True}
    forbidden = ("expected", "previous+1", "next-1", "Ps.47.2", "Ps.93.2", "p0072l0083", "p0136l0092")
    source = open(diagnostic.__file__, encoding="utf-8").read()
    # The last four occur only in the historical source labels/artifact, never
    # in selection logic; source code must contain no target/conflict aliases.
    assert not any(x in source for x in forbidden[3:])
    # They are named in the documented forbidden-feature schema, but cannot
    # enter either runtime guard.
    guard_source = source[source.index("def order_state"):source.index("def build")]
    assert "previous" not in guard_source and "next" not in guard_source
    feature_source = source[source.index("def prelabel_features"):source.index("def external_review")]
    assert "projected_gap_path" in feature_source
    assert "task141" not in feature_source
    assert first["production_feature_schema"]["pre_label"] is True
    conflicts = first["late_ref_conflict_analysis"]
    assert len(conflicts) == 13
    assert {("Ps.47.7", "Ps.47.2"), ("Ps.93.19", "Ps.93.2")} <= {
        (r["physical_context"], r["proposed_native_ref"]) for r in conflicts}
    guard = first["order_guard_candidates"][0]
    assert guard["known_conflicts_accepted"] == 0 and guard["reviewed_rejected"] == 4
    selected = first["selected_scope"]
    assert selected["physical_blocks"] == selected["source_events"] == selected["occurrences"].__len__() == 43
    assert selected["other_value_markers"] == selected["non_markers"] == selected["unreviewed"] == selected["order_conflicts"] == 0
    assert first["result_status"] == "NARROWER_SCOPE_FOUND"
    assert first["task145_recommendation"]["count"] == 1
    runtime = first["runtime_invariants"]
    assert {k: runtime[k] for k in ("verse_refs", "physical_gaps", "glyph_gaps", "chapters", "unresolved_chapter_claims", "canonical_chapter_gaps", "duplicate_refs", "out_of_order_refs", "outside_canon", "ocr_blocks", "block_loss", "dual_ownership")} == {"verse_refs":3849,"physical_gaps":3254,"glyph_gaps":1309,"chapters":337,"unresolved_chapter_claims":0,"canonical_chapter_gaps":0,"duplicate_refs":0,"out_of_order_refs":0,"outside_canon":0,"ocr_blocks":57700,"block_loss":0,"dual_ownership":0}
    assert runtime["task128"] == {"markers":183,"refs":177,"moves":1355}
    assert runtime["task131"] == {"markers":276,"refs":276,"moves":1900}
    assert runtime["task139"]["ownership_moves"] == 27 and runtime["GLUED_FRAME"] == "CLOSED_UNSAFE"
    assert first["audit_summary"] == diagnostic.audit_volume.audit(str(diagnostic.XML), volume="3", witness="ia-lasagradabiblia01unkngoog", book="Ps")[1]["verse_segmentation_audit"]["projected_form_a_production_scope_audit"]
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "a.json")
        open(path, "wb").write(rendered)
        assert open(path, "rb").read() == canonical
    print("ok")


if __name__ == "__main__":
    main()
