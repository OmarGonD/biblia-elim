#!/usr/bin/env python3
"""Task 155: dry-run the glued two-character marker rule («i3» -> 13).

The production parser runs with the task-156 switch OFF: the same code path
(``split_digit_recovery.apply`` with ``enabled=False``) decides every
candidate and leaves ``would_apply`` with its planned outcome, exactly as
task 146 simulated on its own route.  Nothing is applied.  The facsimile
sample of task 154 is joined afterwards.
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

import audit_volume
from glued_two_char_marker_audit import SAMPLE_REVIEW

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "glued_two_char_marker_dry_run.json"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(report, xml=XML):
    seg = report["verse_segmentation_audit"]
    run = seg["glued_two_char_marker_recovery"]
    records = run["records"]
    planned = [r for r in records if r["outcome"] == "would_apply"]
    created = sorted(r["proposed_native_ref"] for r in planned)
    removed = sorted(f"{r['proposed_native_ref'].rsplit('.', 1)[0]}."
                     f"{r['prior_verse']}" for r in planned
                     if r["planned_outcome"] == "relabelled_false_ref")
    gaps = {g["key"] for g in seg["gaps"]}
    by_block = {r["block_id"]: r for r in records}
    reviews = [{"block": b, "facsimile_value": v,
                "planned": by_block.get(b, {}).get("outcome"),
                "source_reading": by_block.get(b, {}).get("value"),
                "agrees": by_block.get(b, {}).get("value") == v}
               for b, v in sorted(SAMPLE_REVIEW.items())]
    return {
        "schema_version": 1,
        "provenance": {
            "generator": "scripts/torresamat1835/glued_two_char_marker_dry_run.py",
            "source_xml_sha256": sha(xml),
            "runtime_mode": "production with task-156 switch off (dry run)"},
        "runtime_baseline": {"verse_refs": report["verse_refs"],
                             "physical_gaps": len(seg["gaps"]),
                             "chapters": report["chapters"],
                             "ocr_blocks": report["metrics"]["ocr_blocks"]},
        "candidates": len(records),
        "outcomes": dict(sorted(Counter(
            r["planned_outcome"] or r["outcome"] for r in records).items())),
        "predicted_delta": {
            "created_refs": created, "created": len(created),
            "relabelled_false_refs": removed, "removed": len(removed),
            "verse_refs_after": report["verse_refs"] + len(created) -
            len(removed),
            "blocks_moved": sum(len(r["blocks_moved"] or []) for r in planned),
            "physical_gaps_closed": sorted(set(created) & gaps)},
        "facsimile_sample": {"size": len(reviews),
                             "agreement": sum(r["agrees"] for r in reviews),
                             "sample_planned": dict(Counter(
                                 r["planned"] for r in reviews)),
                             "reviews": reviews},
        "records": records,
        "runtime_invariants": {"new_recovery": False,
                               "expected_verse_used": False},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ART)
    ns = ap.parse_args()
    _edition, report = audit_volume.audit(
        str(XML), volume="3", witness="ia-lasagradabiblia01unkngoog",
        book="Ps", glued_marker_recovery=False)
    data = build(report)
    ns.out.write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n",
                      encoding="utf-8")
    print(json.dumps({"outcomes": data["outcomes"],
                      "created": data["predicted_delta"]["created"],
                      "removed": data["predicted_delta"]["removed"],
                      "sample": [data["facsimile_sample"]["agreement"],
                                 data["facsimile_sample"]["sample_planned"]]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
