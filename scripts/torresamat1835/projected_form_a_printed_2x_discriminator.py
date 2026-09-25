#!/usr/bin/env python3
"""Task 148: validate a source-only discriminator for printed 2x markers.

Diagnostic only; no parser recovery is called or changed.  Features are
frozen from the source OCR and the current audit's gap provenance first;
labels (task-147 family, task-141 facsimile reviews) are joined afterwards
and never reach a predicate.

The candidate rule reads a printed two-digit marker 20-29 whose leading
``2`` the OCR returned as the word ``a`` and whose second digit is the next
physical token.  The task-142/146 rule for the printed ``2`` requires three
prose words after the ``a`` and so is disjoint from this one.
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

import source_ocr
from projected_form_a_recovery import validated_geometry

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data/torresamat1835"
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
ART = DATA / "projected_form_a_printed_2x_discriminator.json"
GLYPH_SIGNAL = "lone_glyph_inside_previous_verse"
#: Second OCR token read as the second printed digit (task-147 evidence).
SECOND_DIGIT = {"I": 1, "1": 1, "l": 1, "o": 0, "O": 0, "0": 0,
                "4": 4, "4'": 4, "a": 2, "5": 5, "3": 3}
#: Punctuation the OCR sometimes places before the marker.
LEADING_NOISE = {"•", ".", "—", "-", ","}
#: Two-digit window around the one-digit marker band, in digit widths.
BAND_WINDOW = (-2.0, 0.5)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def features(page, line, where, bands):
    """Source-only vector for a line that starts with 'a' + digit-like."""
    words = line.words
    lead = 1 if len(words) > 2 and words[0].text in LEADING_NOISE else 0
    if len(words) < lead + 3 or words[lead].text != "a" or \
            words[lead + 1].text not in SECOND_DIGIT:
        return None
    column, zone = where.get(line.index, (None, None))
    band = bands.get(column)
    delta = None
    if band and band[1]:
        delta = (words[lead].bbox[0] - band[0]) / band[1]
    return {
        "block": f"p{page.scan_page:04d}l{line.index:04d}",
        "leading_noise": bool(lead),
        "tokens": [w.text for w in words[lead:lead + 3]],
        "reading": 20 + SECOND_DIGIT[words[lead + 1].text],
        "column": getattr(column, "value", None),
        "zone": getattr(zone, "value", None),
        "band": band is not None,
        "band_delta_digits": None if delta is None else round(delta, 3),
        "in_two_digit_window": delta is not None and
        BAND_WINDOW[0] <= delta <= BAND_WINDOW[1],
    }


RULES = {
    # The candidate rule: exact unframed 'a', right (Spanish) body column,
    # projected 'a' glyph-gap provenance.
    "R2X": lambda f: not f["leading_noise"] and f["column"] == "right"
    and f["zone"] == "body" and f["projected_gap_provenance"],
    "R2X_WINDOW": lambda f: RULES["R2X"](f) and f["in_two_digit_window"],
    "R2X_ALLOW_LEADING_NOISE": lambda f: f["column"] == "right"
    and f["zone"] == "body" and f["projected_gap_provenance"],
    # Without the provenance guard, for contrast.
    "R2X_NO_PROVENANCE": lambda f: not f["leading_noise"]
    and f["column"] == "right" and f["zone"] == "body",
}


def build(audit, xml=XML):
    gaps = audit["verse_segmentation_audit"]["gaps"]
    provenance = {g["swallowed_block"] for g in gaps
                  if g.get("swallowed_token") == "a"
                  and GLYPH_SIGNAL in g.get("signals", [])}
    vectors = []
    for page in source_ocr.read_pages(str(xml)):
        where, bands = validated_geometry(page)
        for line in page.lines:
            f = features(page, line, where, bands)
            if f:
                f["projected_gap_provenance"] = f["block"] in provenance
                vectors.append(f)
    vectors.sort(key=lambda f: f["block"])
    decisions = {name: {f["block"] for f in vectors if rule(f)}
                 for name, rule in RULES.items()}

    # Labels are joined only now.
    t147 = json.loads((DATA / "remaining_glyph_reprioritization_147.json")
                      .read_text(encoding="utf-8"))
    members = {m["ocr_block"]: m
               for m in t147["selected_task148_family"]["members"]}
    reviews = {}
    for r in json.loads((DATA / "projected_form_a_facsimile.json")
                        .read_text(encoding="utf-8"))["occurrence_reviews"]:
        reviews.setdefault(r["ocr_block_id"], r)
    by_block = {f["block"]: f for f in vectors}

    def label(block):
        if block in members:
            return "FAMILY_MEMBER"
        r = reviews.get(block)
        if not r:
            return "UNREVIEWED"
        if r["facsimile_review_class"] == "PRINTED_VERSE_MARKER":
            return "REVIEWED_MARKER_OTHER_GAP"
        return "REVIEWED_NON_MARKER"

    evaluation = {}
    for name, accepted in decisions.items():
        counts = Counter(label(b) for b in accepted)
        wrong_value = sorted(
            b for b in accepted if b in members and
            by_block[b]["reading"] != members[b]["visible_printed_value"])
        evaluation[name] = {
            "accepted": len(accepted),
            "by_label": dict(sorted(counts.items())),
            "family_true_positive": counts["FAMILY_MEMBER"],
            "family_false_negative": sorted(set(members) - accepted),
            "reviewed_non_marker_accepted": counts["REVIEWED_NON_MARKER"],
            "wrong_value_on_family": wrong_value,
            "other_gap_markers_accepted": sorted(
                b for b in accepted
                if label(b) == "REVIEWED_MARKER_OTHER_GAP"),
        }
    return {
        "schema_version": 1,
        "provenance": {
            "generator":
                "scripts/torresamat1835/projected_form_a_printed_2x_discriminator.py",
            "source_xml_sha256": sha(xml),
            "artifact_sha256": {
                "remaining_glyph_reprioritization_147.json":
                    sha(DATA / "remaining_glyph_reprioritization_147.json"),
                "projected_form_a_facsimile.json":
                    sha(DATA / "projected_form_a_facsimile.json")},
            "runtime_mode": "production (projected form-a recovery enabled)",
        },
        "runtime_baseline": {
            "verse_refs": audit["verse_refs"], "physical_gaps": len(gaps),
            "glyph_gaps": audit["verse_segmentation_audit"]["by_signal"]
            .get(GLYPH_SIGNAL)},
        "feature_boundary": "source OCR + current gap provenance only; "
                            "labels joined after all decisions",
        "second_digit_forms": SECOND_DIGIT,
        "two_digit_band_window_digits": list(BAND_WINDOW),
        "corpus_candidates": len(vectors),
        "candidate_zones": dict(sorted(Counter(
            f"{f['column']}/{f['zone']}" for f in vectors).items())),
        "rules": {name: evaluation[name] for name in RULES},
        "selected_rule": "R2X",
        "findings": [
            "every corpus line starting with 'a' + digit-like token that "
            "was reviewed is a printed verse marker; the risk is placement, "
            "not identity",
            "without the projected-gap provenance guard the rule reaches "
            "hundreds of markers already owned by their verses",
            "the task-142 one-digit band tolerance does not fit two-digit "
            "markers, and a digit-width window rejects every member whose "
            "column has no trusted band: geometry is not selected",
            "three members carry a punctuation token before 'a'; admitting "
            "it is measured separately and not selected",
            "one accepted block is a printed 21 in the gap of another verse: "
            "the task-144 native-order guard must decide it",
        ],
        "recommendation": {
            "outcome": "READY_FOR_DRY_RUN",
            "next_task": "dry-run the R2X rule with the task-144 provenance "
                         "and native-order guards and measure the semantic "
                         "delta before any recovery",
        },
        "runtime_invariants": {
            "new_recovery": False, "ownership_unchanged": True,
            "expected_verse_used": False, "previous_plus_one_used": False,
            "next_minus_one_used": False, "gap_key_used_by_rule": False},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", required=True)
    ap.add_argument("--out", default=str(ART))
    ns = ap.parse_args()
    data = build(json.loads(Path(ns.audit).read_text(encoding="utf-8")))
    Path(ns.out).write_text(json.dumps(data, ensure_ascii=False, indent=2)
                            + "\n", encoding="utf-8")
    print(json.dumps({k: {"accepted": v["accepted"],
                          "tp": v["family_true_positive"],
                          "fn": len(v["family_false_negative"])}
                      for k, v in data["rules"].items()}, sort_keys=True))


if __name__ == "__main__":
    main()
