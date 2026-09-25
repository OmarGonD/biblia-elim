#!/usr/bin/env python3
"""Task 152: dry-run the split two-digit marker rule («1 8» -> 18).

Nothing is applied.  On the current production edition (task 150 on) every
right/body line opening «d d Sentence» (task 151 shape) is simulated IN
PHYSICAL ORDER within its chapter, so each event sees the verse labels the
previous events left.  The value is the two source digits; the guards are
the task-144/150 ones plus a two-sided progression check:

* single native owner, resolved chapter;
* the value is inside the native canon and no block of the chapter carries
  that verse yet (no reopen, no duplicate);
* native progression: every block before the line carries a lower verse and
  the first differently-labelled block after the moved run a higher one;
* one event per proposed ref.

The moved run is the line and the rest of its contiguous run of the same
verse.  When the line opens that run and the owner verse has no other
block, the event RELABELS a verse that only existed because of the misread
first digit; otherwise it MOVES blocks and the owner keeps the rest.
No gap key, expected verse, previous+1 or next-1 is read.
"""
import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path

import audit_volume
import source_ocr
import structure
from projected_form_a_recovery import validated_geometry
from split_digit_marker_audit import split_marker

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "split_digit_marker_dry_run.json"

#: Facsimile reading (source PDF, page = scan page + 1) of a sample of 32
#: accepted events stratified by book and outcome (20 RELABEL, 12 MOVE).
SAMPLE_REVIEW = {
    "p0077l0063": 14,
    "p0118l0013": 11,
    "p0119l0053": 18,
    "p0167l0066": 18,
    "p0172l0074": 71,
    "p0216l0073": 13,
    "p0221l0070": 31,
    "p0232l0068": 13,
    "p0255l0065": 14,
    "p0264l0061": 15,
    "p0281l0056": 11,
    "p0284l0022": 15,
    "p0286l0022": 18,
    "p0288l0056": 11,
    "p0291l0070": 14,
    "p0308l0023": 11,
    "p0310l0012": 15,
    "p0312l0022": 13,
    "p0320l0039": 13,
    "p0340l0060": 15,
    "p0348l0082": 31,
    "p0353l0047": 18,
    "p0383l0060": 11,
    "p0390l0056": 17,
    "p0407l0066": 13,
    "p0433l0060": 15,
    "p0464l0054": 17,
    "p0512l0069": 14,
    "p0513l0039": 18,
    "p0532l0081": 11,
    "p0634l0074": 11,
    "p0636l0014": 11,
}

MOVE = "MOVE_TO_NEW_REF"
RELABEL = "RELABEL_FALSE_REF"
NOT_OWNED = "not_owned"
AMBIGUOUS_OWNER = "ambiguous_owner"
OUTSIDE_CANON = "outside_native_canon"
EXISTS = "verse_already_present"
BEHIND = "progression_behind_previous"
AHEAD = "progression_past_next"
AMBIGUOUS_EVENT = "ambiguous_source_event"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def candidates(xml):
    out = []
    for page in source_ocr.read_pages(str(xml)):
        where, _bands = validated_geometry(page)
        for line in page.lines:
            digits = split_marker(line.words)
            if not digits:
                continue
            column, zone = where.get(line.index, (None, None))
            if getattr(column, "value", None) != "right" or \
                    getattr(zone, "value", None) != "body":
                continue
            out.append({"block": f"p{page.scan_page:04d}l{line.index:04d}",
                        "tokens": [w.text for w in line.words[:4]],
                        "value": digits[0] * 10 + digits[1]})
    return sorted(out, key=lambda c: c["block"])


def simulate(edition, cands):
    refs = audit_volume._reference_map(edition)
    owners = audit_volume._owner_map(refs)
    # chapter -> physical order of (block, verse)
    chapters = defaultdict(dict)
    for ref, info in refs.items():
        osis, chapter, verse = ref.split(".")
        for block in info["blocks"]:
            chapters[(osis, int(chapter))][block] = int(verse)
    events = []
    for c in cands:
        event = dict(c, native_owner_refs=sorted(owners.get(c["block"], [])),
                     proposed_native_ref=None)
        events.append(event)
        owner = owners.get(c["block"], [])
        if not owner:
            event["outcome"] = NOT_OWNED
            continue
        if len(owner) != 1:
            event["outcome"] = AMBIGUOUS_OWNER
            continue
        osis, chapter, _v = owner[0].split(".")
        event["_chapter"] = (osis, int(chapter))
    # Physical order within each chapter; state is updated as events apply.
    proposed = Counter()
    by_chapter = defaultdict(list)
    for e in events:
        if "_chapter" in e:
            by_chapter[e["_chapter"]].append(e)
    for key, group in sorted(by_chapter.items()):
        labels = chapters[key]
        order = sorted(labels)
        limit = structure.verse_limit(*key)
        for e in sorted(group, key=lambda x: x["block"]):
            value, block = e["value"], e["block"]
            if limit is None or not 1 <= value <= limit:
                e["outcome"] = OUTSIDE_CANON
                continue
            if value in labels.values():
                e["outcome"] = EXISTS
                continue
            at = order.index(block)
            current = labels[block]
            end = at
            while end + 1 < len(order) and labels[order[end + 1]] == current:
                end += 1
            before = [labels[b] for b in order[:at]]
            after = next((labels[b] for b in order[end + 1:]), None)
            if before and max(before) >= value:
                e["outcome"] = BEHIND
                continue
            if after is not None and after <= value:
                e["outcome"] = AHEAD
                continue
            run = order[at:end + 1]
            relabel = (at == 0 or labels[order[at - 1]] != current) and \
                sum(1 for v in labels.values() if v == current) == len(run)
            e.update(outcome=RELABEL if relabel else MOVE,
                     proposed_native_ref=f"{key[0]}.{key[1]}.{value}",
                     prior_verse=current, blocks_moved=run)
            proposed[e["proposed_native_ref"]] += 1
            for b in run:
                labels[b] = value
    for e in events:
        e.pop("_chapter", None)
        if e.get("proposed_native_ref") and proposed[e["proposed_native_ref"]] > 1:
            e["outcome"] = AMBIGUOUS_EVENT
    return events, refs


def build(edition, audit, xml=XML):
    seg = audit["verse_segmentation_audit"]
    cands = candidates(xml)
    events, refs = simulate(edition, cands)
    applied = [e for e in events if e["outcome"] in (MOVE, RELABEL)]
    created = sorted(e["proposed_native_ref"] for e in applied)
    removed = sorted(f"{e['proposed_native_ref'].rsplit('.', 1)[0]}."
                     f"{e['prior_verse']}" for e in applied
                     if e["outcome"] == RELABEL)
    gap_keys = {g["key"] for g in seg["gaps"]}
    # Labels joined only now: the facsimile sample.
    by_block = {e["block"]: e for e in events}
    reviews = [{"block": b, "outcome": by_block[b]["outcome"],
                "source_reading": by_block[b]["value"], "facsimile_value": v,
                "agrees": by_block[b]["value"] == v}
               for b, v in sorted(SAMPLE_REVIEW.items())]
    return {
        "schema_version": 1,
        "provenance": {
            "generator": "scripts/torresamat1835/split_digit_marker_dry_run.py",
            "source_xml_sha256": sha(xml),
            "runtime_mode": "production (task-150 recovery enabled), "
                            "dry run: nothing applied"},
        "runtime_baseline": {
            "verse_refs": audit["verse_refs"],
            "physical_gaps": len(seg["gaps"]),
            "chapters": audit["chapters"],
            "ocr_blocks": audit["metrics"]["ocr_blocks"]},
        "candidates": len(cands),
        "outcomes": dict(sorted(Counter(e["outcome"] for e in events).items())),
        "predicted_delta": {
            "created_refs": created, "created": len(created),
            "relabelled_false_refs": removed, "removed": len(removed),
            "verse_refs_after": audit["verse_refs"] + len(created) -
            len(removed),
            "ownership_moves": sum(len(e["blocks_moved"]) for e in applied),
            "physical_gaps_closed": sorted(set(created) & gap_keys),
            "removed_refs_become_gaps": removed},
        "facsimile_sample": {
            "size": len(reviews),
            "by_outcome": dict(sorted(Counter(r["outcome"]
                                              for r in reviews).items())),
            "agreement": sum(r["agrees"] for r in reviews),
            "reviews": reviews},
        "events": events,
        "runtime_invariants": {
            "new_recovery": False, "ownership_unchanged": True,
            "expected_verse_used": False, "previous_plus_one_used": False,
            "next_minus_one_used": False, "gap_key_used_by_guards": False},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ART)
    ns = ap.parse_args()
    edition, audit = audit_volume.audit(
        str(XML), volume="3", witness="ia-lasagradabiblia01unkngoog",
        book="Ps", split_digit_recovery=False)  # measured before task 153
    data = build(edition, audit)
    ns.out.write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n",
                      encoding="utf-8")
    print(json.dumps({"outcomes": data["outcomes"],
                      "created": data["predicted_delta"]["created"],
                      "removed": data["predicted_delta"]["removed"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
