#!/usr/bin/env python3
"""Task 149: dry-run the task-148 R2X rule under the task-144 guards.

Nothing is applied: the current production edition (task-146 recovery on)
is read, R2X is re-derived from source OCR and live gap provenance, and each
accepted block goes through the same guards as
``projected_form_a_recovery.apply`` (single native owner, resolved chapter,
native progression, canon limit, no reopening of an existing verse, one
event per proposed ref).  The value comes from the source tokens (``a`` +
second digit), never from the gap, the expected verse, previous+1 or
next-1.  Labels are joined only after every outcome is fixed.
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

import audit_volume
import source_ocr
import structure
from projected_form_a_printed_2x_discriminator import RULES, features
from projected_form_a_recovery import validated_geometry

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "projected_form_a_printed_2x_dry_run.json"
GLYPH_SIGNAL = "lone_glyph_inside_previous_verse"

WOULD_CREATE = "CREATE_NEW_REF"
NOT_OWNED = "not_owned"
AMBIGUOUS_OWNER = "ambiguous_owner"
UNRESOLVED_CHAPTER = "native_chapter_unresolved"
ORDER = "order_guard_backward_or_equal"
OUTSIDE_CANON = "outside_native_canon"
REOPEN = "existing_ref_reopen"
EMPTIES_OWNER = "would_empty_prior_owner"
AMBIGUOUS_EVENT = "ambiguous_source_event"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _parts(ref):
    osis, chapter, verse = ref.split(".")
    return osis, int(chapter), int(verse)


def accepted_blocks(audit, xml):
    """R2X re-derived from source and live provenance (task 148)."""
    provenance = {g["swallowed_block"]
                  for g in audit["verse_segmentation_audit"]["gaps"]
                  if g.get("swallowed_token") == "a"
                  and GLYPH_SIGNAL in g.get("signals", [])}
    out = []
    for page in source_ocr.read_pages(str(xml)):
        where, bands = validated_geometry(page)
        for line in page.lines:
            f = features(page, line, where, bands)
            if not f:
                continue
            f["projected_gap_provenance"] = f["block"] in provenance
            if RULES["R2X"](f):
                out.append(f)
    return sorted(out, key=lambda f: f["block"])


def simulate(edition, audit, candidates):
    refs = audit_volume._reference_map(edition)
    owners = audit_volume._owner_map(refs)
    events, proposals = [], {}
    for f in candidates:
        block, value = f["block"], f["reading"]
        owner = owners.get(block, [])
        event = {"physical_block": block, "source_tokens": f["tokens"],
                 "source_reading": value, "native_owner_refs": sorted(owner),
                 "proposed_native_ref": None}
        events.append(event)
        if not owner:
            event["outcome"] = NOT_OWNED
            continue
        if len(owner) != 1:
            event["outcome"] = AMBIGUOUS_OWNER
            continue
        osis, chapter, verse = _parts(owner[0])
        if chapter < 1:
            event["outcome"] = UNRESOLVED_CHAPTER
            continue
        if value <= verse:
            event["outcome"] = ORDER
            continue
        limit = structure.verse_limit(osis, chapter)
        if limit is None or not 1 <= value <= limit:
            event["outcome"] = OUTSIDE_CANON
            continue
        proposed = f"{osis}.{chapter}.{value}"
        if proposed in refs:
            event["outcome"] = REOPEN
            continue
        blocks = refs[owner[0]]["blocks"]
        moving = [b for b in blocks if b >= block]
        if len(moving) == len(blocks):
            event["outcome"] = EMPTIES_OWNER
            continue
        event.update(proposed_native_ref=proposed, prior_owner_ref=owner[0],
                     blocks_moved=moving,
                     blocks_remaining_with_prior_owner=len(blocks) -
                     len(moving))
        proposals.setdefault(proposed, []).append(event)
    for proposed, group in proposals.items():
        for event in group:
            event["outcome"] = (WOULD_CREATE if len(group) == 1
                                else AMBIGUOUS_EVENT)
    return events


def build(edition, audit, xml=XML):
    seg = audit["verse_segmentation_audit"]
    candidates = accepted_blocks(audit, xml)
    events = simulate(edition, audit, candidates)
    created = sorted(e["proposed_native_ref"] for e in events
                     if e["outcome"] == WOULD_CREATE)
    gap_keys = {g["key"] for g in seg["gaps"]}
    glyph_keys = {g["key"] for g in seg["gaps"]
                  if GLYPH_SIGNAL in g.get("signals", [])}
    moves = sum(len(e["blocks_moved"]) for e in events
                if e["outcome"] == WOULD_CREATE)

    # Labels are joined only now.
    t147 = json.loads((DATA / "remaining_glyph_reprioritization_147.json")
                      .read_text(encoding="utf-8"))
    members = {m["ocr_block"]: m
               for m in t147["selected_task148_family"]["members"]}
    for e in events:
        m = members.get(e["physical_block"])
        e["label"] = "FAMILY_MEMBER" if m else "OUTSIDE_FAMILY"
        if m:
            e["facsimile_value"] = m["visible_printed_value"]
            e["task147_gap_key"] = m["key"]
    created_events = [e for e in events if e["outcome"] == WOULD_CREATE]
    return {
        "schema_version": 1,
        "provenance": {
            "generator":
                "scripts/torresamat1835/projected_form_a_printed_2x_dry_run.py",
            "source_xml_sha256": sha(xml),
            "artifact_sha256": {
                p: sha(DATA / p) for p in (
                    "projected_form_a_printed_2x_discriminator.json",
                    "remaining_glyph_reprioritization_147.json")},
            "runtime_mode": "production (task-146 recovery enabled), "
                            "dry run: nothing applied",
        },
        "runtime_baseline": {
            "verse_refs": audit["verse_refs"],
            "physical_gaps": len(seg["gaps"]),
            "glyph_gaps": len(glyph_keys),
            "chapters": audit["chapters"],
            "ocr_blocks": audit["metrics"]["ocr_blocks"],
            "duplicate_refs": len(audit["duplicate_refs"]),
            "out_of_order_refs": len(audit["out_of_order_refs"])},
        "rule": "R2X (task 148)",
        "accepted_blocks": len(candidates),
        "outcomes": dict(sorted(Counter(e["outcome"]
                                        for e in events).items())),
        "events": events,
        "predicted_delta": {
            "CREATE_NEW_REF": len(created), "REOPEN_EXISTING_REF": 0,
            "new_refs": created,
            "verse_refs_after": audit["verse_refs"] + len(created),
            "ownership_moves": moves,
            "physical_gaps_closed": sorted(set(created) & gap_keys),
            "glyph_gaps_closed": sorted(set(created) & glyph_keys),
            "block_loss": 0, "dual_ownership": 0,
            "prior_owner_emptied": 0},
        "label_reconciliation": {
            "created_family_members": sum(1 for e in created_events
                                          if e["label"] == "FAMILY_MEMBER"),
            "created_outside_family": sorted(
                e["physical_block"] for e in created_events
                if e["label"] != "FAMILY_MEMBER"),
            "created_value_equals_facsimile": all(
                e["source_reading"] == e["facsimile_value"]
                for e in created_events if e["label"] == "FAMILY_MEMBER"),
            "created_ref_equals_task147_gap": all(
                e["proposed_native_ref"] == e["task147_gap_key"]
                for e in created_events if e["label"] == "FAMILY_MEMBER"),
        },
        "runtime_invariants": {
            "new_recovery": False, "ownership_unchanged": True,
            "expected_verse_used": False, "previous_plus_one_used": False,
            "next_minus_one_used": False, "gap_key_used_by_guards": False},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--xml", type=Path, default=XML)
    ap.add_argument("--out", type=Path, default=ART)
    ns = ap.parse_args()
    # Measured before the task-150 fallback existed (it is what this
    # dry run predicted); the frozen artifact reproduces that state.
    edition, audit = audit_volume.audit(
        str(ns.xml), volume="3", witness="ia-lasagradabiblia01unkngoog",
        book="Ps", printed_2x_recovery=False)
    data = build(edition, audit, ns.xml)
    ns.out.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    print(json.dumps({"outcomes": data["outcomes"],
                      "create": data["predicted_delta"]["CREATE_NEW_REF"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
