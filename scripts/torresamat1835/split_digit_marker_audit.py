#!/usr/bin/env python3
"""Task 151: printed two-digit verse markers split by the OCR («1 8»).

Diagnostic only.  A line whose first two physical tokens are single digits
followed by the start of a sentence is a printed two-digit marker the OCR
split in two; the parser reads the first digit as the verse number, so
the verse of the first digit owns text that belongs to the two-digit
verse (task 149: p0032l0079 «1 8 - Libóme…» = Ps 17:18 owned by Ps.17.1).

Source features and parser ownership are measured first; the facsimile
review of a random sample (task-151 reading of the source PDF) is joined
afterwards and is never an input to the classification.
"""
import argparse
import hashlib
import json
import random
import re
from collections import Counter
from pathlib import Path

import audit_volume
import source_ocr
import structure
from projected_form_a_recovery import validated_geometry

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "split_digit_marker_audit.json"
SENTENCE_START = re.compile(r"[-–—]?[A-ZÁÉÍÓÚÑ¿¡«]")
SAMPLE_SEED = 151
SAMPLE_SIZE = 8
#: Facsimile reading of the seeded sample (source PDF page = scan page + 1).
SAMPLE_REVIEW = {
    "p0173l0044": 78, "p0251l0055": 13, "p0305l0013": 15,
    "p0350l0062": 13, "p0567l0068": 17, "p0614l0047": 18,
    "p0620l0040": 15, "p0632l0062": 17,
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def split_marker(words):
    """(first, second) digits if the line opens with «d d Sentence»."""
    if len(words) < 3:
        return None
    a, b = words[0].text.strip("."), words[1].text.strip(".")
    third = words[2].text
    if third in "-–—" and len(words) > 3:
        third = third + words[3].text
    if re.fullmatch(r"[1-9]", a) and re.fullmatch(r"[0-9]", b) and \
            SENTENCE_START.match(third):
        return int(a), int(b)
    return None


def classify(row):
    if row["column"] != "right" or row["zone"] != "body":
        return "OUTSIDE_SPANISH_BODY"
    if len(row["owner_refs"]) != 1:
        return "NOT_SINGLY_OWNED"
    if row["owner_verse"] != row["first_digit"]:
        return "OWNER_IS_NOT_FIRST_DIGIT"
    if row["verse_limit"] is None or row["two_digit"] > row["verse_limit"]:
        return "OUTSIDE_NATIVE_CANON"
    if row["two_digit_ref_exists"]:
        return "TWO_DIGIT_REF_EXISTS"
    return "SPLIT_MARKER_READ_AS_FIRST_DIGIT"


def build(edition, xml=XML):
    refs = audit_volume._reference_map(edition)
    owners = audit_volume._owner_map(refs)
    rows = []
    for page in source_ocr.read_pages(str(xml)):
        where, _bands = validated_geometry(page)
        for line in page.lines:
            digits = split_marker(line.words)
            if not digits:
                continue
            column, zone = where.get(line.index, (None, None))
            block = f"p{page.scan_page:04d}l{line.index:04d}"
            owner = owners.get(block, [])
            row = {"block": block, "tokens": [w.text for w in line.words[:4]],
                   "first_digit": digits[0],
                   "two_digit": digits[0] * 10 + digits[1],
                   "column": getattr(column, "value", None),
                   "zone": getattr(zone, "value", None),
                   "owner_refs": sorted(owner), "owner_verse": None,
                   "verse_limit": None, "two_digit_ref_exists": None}
            if len(owner) == 1:
                osis, chapter, verse = owner[0].split(".")
                row["owner_verse"] = int(verse)
                row["verse_limit"] = structure.verse_limit(osis, int(chapter))
                row["two_digit_ref_exists"] = (
                    f"{osis}.{chapter}.{row['two_digit']}" in refs)
            row["class"] = classify(row)
            rows.append(row)
    rows.sort(key=lambda r: r["block"])
    family = [r for r in rows if r["class"] == "SPLIT_MARKER_READ_AS_FIRST_DIGIT"]

    # The facsimile sample is drawn from the family and joined afterwards.
    rng = random.Random(SAMPLE_SEED)
    sample = sorted(r["block"] for r in rng.sample(family, SAMPLE_SIZE))
    if sample != sorted(SAMPLE_REVIEW):
        raise ValueError("facsimile sample no longer matches the family")
    by_block = {r["block"]: r for r in family}
    reviewed = [{"block": b, "source_reading": by_block[b]["two_digit"],
                 "facsimile_value": SAMPLE_REVIEW[b],
                 "agrees": by_block[b]["two_digit"] == SAMPLE_REVIEW[b]}
                for b in sample]
    return {
        "schema_version": 1,
        "provenance": {
            "generator": "scripts/torresamat1835/split_digit_marker_audit.py",
            "source_xml_sha256": sha(xml),
            "runtime_mode": "production (task-150 recovery enabled)"},
        "corpus_candidates": len(rows),
        "classes": dict(sorted(Counter(r["class"] for r in rows).items())),
        "family": {
            "name": "SPLIT_MARKER_READ_AS_FIRST_DIGIT",
            "definition": ("right/body line opening «d d Sentence», singly "
                           "owned by the verse of the first digit, two-digit "
                           "value inside the native canon and not yet a ref"),
            "population": len(family),
            "books": dict(sorted(Counter(r["owner_refs"][0].split(".")[0]
                                         for r in family).items())),
            "blocks": [r["block"] for r in family]},
        "facsimile_sample": {"seed": SAMPLE_SEED, "size": SAMPLE_SIZE,
                             "reviews": reviewed,
                             "agreement": sum(r["agrees"] for r in reviewed)},
        "controls": {
            "latin_or_other_zone": sum(1 for r in rows
                                       if r["class"] == "OUTSIDE_SPANISH_BODY"),
            "owner_is_not_first_digit": sum(
                1 for r in rows if r["class"] == "OWNER_IS_NOT_FIRST_DIGIT"),
            "two_digit_ref_exists": sum(
                1 for r in rows if r["class"] == "TWO_DIGIT_REF_EXISTS")},
        "recommendation": {
            "outcome": "READY_FOR_DISCRIMINATOR_AND_DRY_RUN",
            "next_task": ("measure a runtime rule for «d d Sentence» in the "
                          "Spanish body (value = two digits from the source) "
                          "with the task-144 guards in a dry run; the owner "
                          "and no-existing-ref conditions are guards, not "
                          "selection by expected verse")},
        "runtime_invariants": {"new_recovery": False,
                               "ownership_unchanged": True,
                               "expected_verse_used": False},
        "rows": rows,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ART)
    ns = ap.parse_args()
    edition, _audit = audit_volume.audit(
        str(XML), volume="3", witness="ia-lasagradabiblia01unkngoog",
        book="Ps", split_digit_recovery=False)  # measured before task 153
    data = build(edition)
    ns.out.write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n",
                      encoding="utf-8")
    print(json.dumps({"candidates": data["corpus_candidates"],
                      "classes": data["classes"],
                      "sample_agreement": data["facsimile_sample"]["agreement"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
