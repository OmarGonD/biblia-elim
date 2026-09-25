#!/usr/bin/env python3
"""Task 154: two-digit markers the OCR read as ONE glued token («i3»).

Diagnostic only.  A right/body line that the parser kept as continuation
and that opens with a two-character token whose characters are
unambiguous digit misreads -- i/I/l -> 1, a -> 2, o/O -> 0, or real
digits, with at least one letter -- followed by the start of a sentence.
Ambiguous glyphs (S = 5 or 8, 9 = 2 or 3) are left out.  The facsimile
review of a stratified sample is joined after classification.
"""
import argparse
import hashlib
import json
import random
import re
from collections import Counter, defaultdict
from pathlib import Path

import audit_volume
import source_ocr
from projected_form_a_recovery import validated_geometry

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "glued_two_char_marker_audit.json"
GLYPH = {"i": 1, "I": 1, "l": 1, "a": 2, "o": 0, "O": 0}
SENTENCE_START = re.compile(r"[-–—]?[A-ZÁÉÍÓÚÑ¿¡«]")
#: Two-letter Spanish words spelled only with those glyphs: never markers.
WORDS = {"la", "lo", "al", "La", "Lo", "Al", "LA", "LO", "AL", "oí", "Oí"}
SAMPLE_SEED = 154
SAMPLE_SIZE = 32
#: Facsimile reading of the stratified sample (source PDF, page = scan + 1).
SAMPLE_REVIEW = {
    "p0033l0055": 29,
    "p0102l0093": 20,
    "p0146l0034": 20,
    "p0190l0040": 20,
    "p0194l0074": 14,
    "p0242l0037": 26,
    "p0244l0041": 22,
    "p0244l0060": 28,
    "p0265l0020": 26,
    "p0266l0055": 13,
    "p0282l0036": 21,
    "p0285l0084": 16,
    "p0289l0048": 20,
    "p0289l0051": 21,
    "p0291l0077": 15,
    "p0291l0084": 16,
    "p0310l0015": 16,
    "p0324l0036": 13,
    "p0331l0083": 13,
    "p0340l0068": 16,
    "p0350l0066": 14,
    "p0358l0073": 20,
    "p0374l0040": 22,
    "p0415l0069": 26,
    "p0433l0069": 20,
    "p0468l0089": 21,
    "p0481l0051": 14,
    "p0509l0060": 29,
    "p0552l0015": 20,
    "p0597l0046": 10,
    "p0603l0020": 11,
    "p0626l0013": 21,
}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def glued_value(words):
    """Two-digit value of «xy Sentence», or None."""
    if len(words) < 2 or len(words[0].text) != 2:
        return None
    token = words[0].text
    if token in WORDS or not any(ch in GLYPH for ch in token):
        return None
    digits = []
    for ch in token:
        if ch.isdigit():
            digits.append(int(ch))
        elif ch in GLYPH:
            digits.append(GLYPH[ch])
        else:
            return None
    if digits[0] == 0 or not SENTENCE_START.match(words[1].text):
        return None
    return digits[0] * 10 + digits[1]


def candidates(edition, xml=XML):
    refs = audit_volume._reference_map(edition)
    owners = audit_volume._owner_map(refs)
    first = {info["blocks"][0] for info in refs.values() if info["blocks"]}
    rows = []
    for page in source_ocr.read_pages(str(xml)):
        where, _bands = validated_geometry(page)
        for line in page.lines:
            value = glued_value(line.words)
            if value is None:
                continue
            column, zone = where.get(line.index, (None, None))
            if getattr(column, "value", None) != "right" or \
                    getattr(zone, "value", None) != "body":
                continue
            block = f"p{page.scan_page:04d}l{line.index:04d}"
            rows.append({"block": block, "value": value,
                         "tokens": [w.text for w in line.words[:4]],
                         "owner_refs": sorted(owners.get(block, [])),
                         "opens_a_ref": block in first})
    return sorted(rows, key=lambda r: r["block"])


def stratified_sample(rows, n, seed):
    rng = random.Random(seed)
    by_book = defaultdict(list)
    for r in rows:
        by_book[r["owner_refs"][0].split(".")[0]].append(r)
    out, books = [], sorted(by_book)
    while len(out) < n and any(by_book.values()):
        for b in books:
            if by_book[b] and len(out) < n:
                out.append(by_book[b].pop(rng.randrange(len(by_book[b]))))
    return sorted(r["block"] for r in out)


def build(edition, xml=XML):
    rows = candidates(edition, xml)
    family = [r for r in rows if len(r["owner_refs"]) == 1
              and not r["opens_a_ref"]]
    sample = stratified_sample(family, SAMPLE_SIZE, SAMPLE_SEED)
    by_block = {r["block"]: r for r in family}
    reviews = [{"block": b, "source_reading": by_block[b]["value"],
                "facsimile_value": SAMPLE_REVIEW.get(b),
                "agrees": SAMPLE_REVIEW.get(b) == by_block[b]["value"]}
               for b in sample]
    return {
        "schema_version": 1,
        "provenance": {
            "generator": "scripts/torresamat1835/glued_two_char_marker_audit.py",
            "source_xml_sha256": sha(xml),
            "runtime_mode": "production (task-153 recovery enabled)"},
        "glyph_map": GLYPH,
        "corpus_candidates": len(rows),
        "family_population": len(family),
        "already_opening_a_ref": sum(1 for r in rows if r["opens_a_ref"]),
        "token_forms": dict(Counter(r["tokens"][0] for r in family)
                            .most_common()),
        "books": dict(sorted(Counter(r["owner_refs"][0].split(".")[0]
                                     for r in family).items())),
        "facsimile_sample": {"seed": SAMPLE_SEED, "size": len(reviews),
                             "agreement": sum(r["agrees"] for r in reviews),
                             "reviews": reviews},
        "runtime_invariants": {"new_recovery": False,
                               "expected_verse_used": False},
        "rows": family,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path, default=ART)
    ns = ap.parse_args()
    edition, _audit = audit_volume.audit(
        str(XML), volume="3", witness="ia-lasagradabiblia01unkngoog",
        book="Ps", glued_marker_recovery=False)  # measured before task 156
    data = build(edition)
    ns.out.write_text(json.dumps(data, ensure_ascii=False, indent=1) + "\n",
                      encoding="utf-8")
    print(json.dumps({"family": data["family_population"],
                      "forms": dict(list(data["token_forms"].items())[:12]),
                      "agreement": data["facsimile_sample"]["agreement"]},
                     ensure_ascii=False))


if __name__ == "__main__":
    main()
