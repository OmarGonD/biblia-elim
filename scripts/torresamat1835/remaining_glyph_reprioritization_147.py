#!/usr/bin/env python3
"""Recompute and prioritize the post-task-146 glyph-gap population.

Task 147 is diagnostic.  The current inventory comes from the current
audit's gap rows (runtime recovery enabled, as in production); historical
JSON is used only for stable lineage and already-recorded source reviews.
Nothing here changes runtime behaviour.

The next family is chosen from facsimile evidence already on record: the
task-141 review of every projected exact-form ``a`` occurrence gives the
printed value of each one.  Among the forms still open after task 146, the
printed two-digit markers 20-29 whose value equals the verse of the gap
they sit in form a finite, source-visible family: the leading ``a`` is the
printed ``2`` and the second digit is the next OCR token.
"""
import argparse
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "data" / "torresamat1835"
GLYPH_SIGNAL = "lone_glyph_inside_previous_verse"
# Second OCR token that the facsimile shows as the second printed digit.
SECOND_DIGIT_FORMS = {"I": 1, "1": 1, "l": 1, "o": 0, "O": 0, "0": 0,
                      "4": 4, "4'": 4, "a": 2, "5": 5, "3": 3}
# Glyph-like punctuation the OCR places before the marker (not a token).
LEADING_NOISE = {"•", ".", "—", "-", ","}
# Physical block kept out of scope by task 146 (geometry outside the
# validated marker band); no ID-specific rule is derived from it.
OUT_OF_BAND_BLOCK = "p0184l0043"


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def audit_sha256(path):
    """Hash stable runtime evidence, excluding timing and embedded artifacts."""
    value = json.loads(Path(path).read_text(encoding="utf-8"))
    seg = value["verse_segmentation_audit"]
    value = {"verse_refs": value["verse_refs"],
             "materialized_verse_refs": value["materialized_verse_refs"],
             "chapters": value["chapters"],
             "chapter_claims": value["chapter_claims"],
             "canonical_chapter_gap_reviews":
                 value["canonical_chapter_gap_reviews"],
             "metrics": value["metrics"],
             "duplicate_refs": value["duplicate_refs"],
             "out_of_order_refs": value["out_of_order_refs"],
             "gaps": seg["gaps"], "by_signal": seg["by_signal"]}
    payload = json.dumps(value, ensure_ascii=False, sort_keys=True,
                         separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def stable_id(gap):
    block = gap.get("swallowed_block") or gap.get("source_block")
    if block:
        return f"{block}::{gap['key']}"
    return f"{gap['key']}::p{gap.get('pdf_page')}"


def _verse(key):
    return int(key.rsplit(".", 1)[1])


def _marker_tokens(review):
    """OCR tokens from the projected 'a' on, without leading punctuation."""
    words = [w["text"] for w in review.get("source_words", [])]
    while words and words[0] in LEADING_NOISE:
        words = words[1:]
    return words


def _two_digit_reading(review):
    """(first, second) digit read from the source tokens, or None."""
    words = _marker_tokens(review)
    if len(words) < 2 or words[0] != "a":
        return None
    second = SECOND_DIGIT_FORMS.get(words[1])
    return None if second is None else (2, second)


def build(audit, audit_path, baseline_commit):
    seg = audit["verse_segmentation_audit"]
    task140_path = DATA / "remaining_glyph_reprioritization.json"
    task141_path = DATA / "projected_form_a_facsimile.json"
    task144_path = DATA / "projected_form_a_production_scope_audit.json"
    task145_path = DATA / "projected_form_a_refined_scope_dry_run.json"
    task140 = json.loads(task140_path.read_text(encoding="utf-8"))
    task141 = json.loads(task141_path.read_text(encoding="utf-8"))
    task144 = json.loads(task144_path.read_text(encoding="utf-8"))
    task145 = json.loads(task145_path.read_text(encoding="utf-8"))

    current = sorted((g for g in seg["gaps"]
                      if GLYPH_SIGNAL in g.get("signals", [])),
                     key=lambda g: g["key"])
    historical = {r["key"]: r
                  for r in task140["current_inventory_summary"]["rows"]}
    current_keys = {g["key"] for g in current}
    removed = sorted(set(historical) - current_keys)
    added = sorted(current_keys - set(historical))
    task146_events = sorted(
        e["proposed_native_ref"] for e in task145["recovery_events"]
        if e["classification"] == "CREATE_NEW_REF")

    reviews = {r["stable_occurrence_id"]: r
               for r in task141["occurrence_reviews"]}
    order_conflict_blocks = sorted(
        r["physical_block"] for r in task144["late_ref_conflict_analysis"])
    external_blocks = {m["block"] for m in task144["external_matches"]}

    rows, forms = [], defaultdict(lambda: {
        "candidate_count": 0, "physical_count": 0, "projected_count": 0,
        "books": set(), "pages": set()})
    primary, review_counts = Counter(), Counter()
    form_a_values = Counter()
    family, family_controls = [], Counter()
    for gap in current:
        old = historical.get(gap["key"], {})
        cls = old.get("primary_root_cause", "PROJECTED_FROM_MULTI_TOKEN")
        form = gap.get("swallowed_token") or ""
        review = reviews.get(stable_id(gap))
        status = old.get("review_status", "NOT_REVIEWED")
        visual = None
        if review:
            visual = review["facsimile_review_class"]
            status = ("REVIEWED_POSITIVE" if visual == "PRINTED_VERSE_MARKER"
                      else "REVIEWED_NEGATIVE")
            form_a_values[str(review.get("visible_printed_value"))] += 1
        primary[cls] += 1
        review_counts[status] += 1
        f = forms[form]
        f["candidate_count"] += 1
        f["physical_count" if cls == "PHYSICAL_SINGLE_TOKEN"
          else "projected_count"] += 1
        f["books"].add(gap["book"])
        if gap.get("pdf_page") is not None:
            f["pages"].add(gap["pdf_page"])

        block = gap.get("swallowed_block")
        in_family = False
        if review and visual == "PRINTED_VERSE_MARKER":
            value = review.get("visible_printed_value")
            if value and 20 <= value <= 29:
                reading = _two_digit_reading(review)
                if value != _verse(gap["key"]):
                    family_controls["printed_2x_value_other_gap"] += 1
                elif block in order_conflict_blocks:
                    family_controls["known_task144_order_conflict"] += 1
                elif block == OUT_OF_BAND_BLOCK or block in external_blocks:
                    family_controls["task144_task146_excluded_block"] += 1
                elif reading is None:
                    family_controls["second_digit_not_in_source"] += 1
                elif reading[0] * 10 + reading[1] != value:
                    family_controls["second_digit_disagrees"] += 1
                else:
                    in_family = True
                    family.append({
                        "stable_occurrence_id": stable_id(gap),
                        "key": gap["key"], "book": gap["book"],
                        "chapter": gap["chapter"], "ocr_block": block,
                        "scan_source_page": review.get("scan_source_page"),
                        "facsimile_pdf_page": review.get("facsimile_pdf_page"),
                        "visible_printed_value": value,
                        "source_tokens": _marker_tokens(review)[:3],
                        "source_two_digit_reading": list(reading),
                    })
            elif value == 2:
                family_controls["printed_2_not_recovered_by_task146"] += 1
        elif review:
            family_controls["reviewed_non_marker"] += 1
        rows.append({
            "stable_occurrence_id": stable_id(gap), "key": gap["key"],
            "book": gap["book"], "chapter": gap["chapter"],
            "pdf_page": gap.get("pdf_page"), "ocr_block": block,
            "raw_ocr_form": form, "primary_root_cause": cls,
            "review_status": status,
            "task141_facsimile_class": visual,
            "task141_visible_value": review.get("visible_printed_value")
            if review else None,
            "historical_lineage": {
                "task140": old.get("primary_root_cause"),
                "task146": "UNCHANGED",
            },
            "selected_family_member": in_family,
            "recovery_coverage": "UNSAFE_OR_UNVALIDATED",
        })

    form_out = {k: {"candidate_count": v["candidate_count"],
                    "physical_count": v["physical_count"],
                    "projected_count": v["projected_count"],
                    "books": sorted(v["books"]), "pages": sorted(v["pages"])}
                for k, v in sorted(forms.items())}
    family.sort(key=lambda r: r["stable_occurrence_id"])
    values = Counter(r["visible_printed_value"] for r in family)
    second = Counter(r["source_tokens"][1] for r in family)
    candidates = [
        {"name": "PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH",
         "population": len(family),
         "evidence_state": "FACSIMILE_REVIEWED_POSITIVES_ON_RECORD",
         "reviewed_positive": len(family), "reviewed_negative": 0,
         "source_signal": "leading 'a' plus a second-digit OCR token"},
        {"name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_Y",
         "population": form_out.get("y", {}).get("candidate_count", 0),
         "evidence_state": "NOT_REVIEWED", "reviewed_positive": 0,
         "reviewed_negative": 0, "source_signal": "token identity only"},
        {"name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_A_ACUTE",
         "population": form_out.get("á", {}).get("candidate_count", 0),
         "evidence_state": "NOT_REVIEWED", "reviewed_positive": 0,
         "reviewed_negative": 0, "source_signal": "token identity only"},
        {"name": "PHYSICAL_SINGLE_TOKEN",
         "population": primary["PHYSICAL_SINGLE_TOKEN"],
         "evidence_state": "NO_REUSABLE_RULE", "reviewed_positive": 1,
         "reviewed_negative": 3, "source_signal": "none reusable (task 135)"},
    ]
    selected = {
        "name": "PROJECTED_FORM_A_PRINTED_2X_VALUE_MATCH",
        "population": len(family), "blocks": len({r["ocr_block"]
                                                  for r in family}),
        "structural_definition": (
            "Current glyph-rooted gaps whose projected token is exact form "
            "'a', reviewed by task 141 as a printed verse marker with a "
            "visible two-digit value 20-29 equal to the gap's own verse, "
            "whose next OCR token reads as the second digit; blocks of "
            "known task-144 order conflicts, task-144 external matches and "
            "the task-146 out-of-band block are excluded."),
        "books": sorted({r["book"] for r in family}),
        "visible_values": {str(k): values[k] for k in sorted(values)},
        "second_token_forms": dict(sorted(second.items())),
        "members": family,
        "controls_outside_family": dict(sorted(family_controls.items())),
        "evidence_state": "FACSIMILE_REVIEWED_POSITIVES_ON_RECORD",
        "task_type": "discriminator validation (no recovery)",
        "next_task_requirements": [
            "derive the two-digit reading from source tokens and geometry "
            "only; the facsimile value is the oracle, never runtime input",
            "measure false positives on every other 'a' + digit-like token "
            "sequence in the corpus, including printed-2x occurrences in "
            "other gaps and reviewed non-markers",
            "apply the task-144 native-order and provenance guards",
            "do not use the gap key, expected verse, previous+1 or next-1",
        ],
        "selection_basis": [
            "finite population, narrower than the remaining glyph gaps",
            "every member facsimile-reviewed as a printed marker (task 141)",
            "the second digit is present in the source tokens of every "
            "member",
            "controls available: printed 2x values in other gaps, reviewed "
            "non-markers, task-144 order conflicts",
        ],
    }
    return {
        "schema_version": 1,
        "provenance": {
            "frozen_baseline_commit": baseline_commit,
            "audit_sha256": audit_sha256(audit_path),
            "historical_artifact_sha256": {
                p.name: sha256(p) for p in
                [task140_path, task141_path, task144_path, task145_path]},
            "generator":
                "scripts/torresamat1835/remaining_glyph_reprioritization_147.py",
            "runtime_mode": "production (projected form-a recovery enabled)",
        },
        "runtime_baseline": {
            "verse_refs": audit["verse_refs"],
            "physical_gaps": len(seg["gaps"]),
            "glyph_gaps": len(current), "chapters": audit["chapters"],
            "unresolved_chapter_claims":
                audit["chapter_claims"]["unresolved"],
            "canonical_chapter_gaps": len(
                audit["canonical_chapter_gap_reviews"]["canonical_missing"]),
            "ocr_blocks": audit["metrics"]["ocr_blocks"],
            "duplicate_refs": len(audit["duplicate_refs"]),
            "out_of_order_refs": len(audit["out_of_order_refs"]),
            "outside_canon": len(audit.get("outside_canon", [])),
        },
        "historical_reconciliation": {
            "historical_total": len(historical),
            "current_total": len(current),
            "removed_occurrences": removed,
            "removed_equal_task146_create_new_ref": removed == task146_events,
            "added_occurrences": added,
        },
        "current_inventory_summary": {
            "construction": "current audit gap rows filtered by "
                            f"{GLYPH_SIGNAL}; not historical-list subtraction",
            "count": len(rows), "rows": rows},
        "physical_projected_split": {
            "PHYSICAL_SINGLE_TOKEN": primary["PHYSICAL_SINGLE_TOKEN"],
            "PROJECTED_FROM_MULTI_TOKEN":
                primary["PROJECTED_FROM_MULTI_TOKEN"]},
        "form_frequencies": form_out,
        "form_a_remaining_visible_values": dict(sorted(form_a_values.items())),
        "review_coverage": dict(sorted(review_counts.items())),
        "excluded_populations": {
            "task144_external_unreadable": len(external_blocks),
            "task144_known_order_conflicts": len(order_conflict_blocks),
            "task146_out_of_band_block": OUT_OF_BAND_BLOCK,
            "glued_frame": "CLOSED_UNSAFE",
        },
        "closed_family_status": {
            "GLUED_FRAME": "CLOSED_UNSAFE",
            "outside_marker_band": "CLOSED_REJECTED",
            "task_128": "CLOSED_NOT_WIDENED", "task_131": "CLOSED_NOT_WIDENED",
            "zero_anchor_I_o": "SOLVED_TASK_139",
            "projected_form_a_printed_2": "SOLVED_TASK_146_REFINED_SCOPE",
            "generic_physical_single_glyph": "NO_REUSABLE_RULE"},
        "bounded_candidate_families": candidates,
        "selected_task148_family": selected,
        "deferred_families": [
            {"name": "all_remaining_glyphs", "reason": "not bounded"},
            {"name": "PROJECTED_MULTI_TOKEN_EXACT_FORM_Y",
             "reason": "largest form but no source review on record; token "
                       "identity alone is not evidence"},
            {"name": "PHYSICAL_SINGLE_TOKEN",
             "reason": "no reusable recovery rule established"},
            {"name": "form-a printed 2 left by task 146",
             "reason": "order conflicts / outside refined scope, closed"}],
        "runtime_invariants": {
            "ownership_unchanged": True, "new_recovery": False,
            "expected_verse_used": False, "previous_plus_one_used": False,
            "next_minus_one_used": False,
            "token_identity_alone_authority": False,
            # The family compares a value already read on the facsimile
            # with the gap it sits in: a diagnostic partition, never a
            # value inferred from the verse sequence.
            "gap_key_used_for_diagnostic_partition_only": True},
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--baseline-commit", required=True)
    ns = ap.parse_args()
    data = build(json.loads(Path(ns.audit).read_text(encoding="utf-8")),
                 ns.audit, ns.baseline_commit)
    Path(ns.out).write_text(json.dumps(data, ensure_ascii=False, indent=2)
                            + "\n", encoding="utf-8")
    print(json.dumps({"current": data["current_inventory_summary"]["count"],
                      "removed": len(data["historical_reconciliation"]
                                     ["removed_occurrences"]),
                      "selected": data["selected_task148_family"]["name"],
                      "population":
                          data["selected_task148_family"]["population"]},
                     sort_keys=True))


if __name__ == "__main__":
    main()
