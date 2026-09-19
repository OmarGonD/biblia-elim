#!/usr/bin/env python3
"""Build the diagnostic remaining verse-gap inventory (task 134)."""
import argparse, json, hashlib, subprocess
from collections import Counter, defaultdict
from pathlib import Path

STATUSES = {"EVIDENCE_READY", "NEEDS_MORE_REVIEW", "UNSAFE_WITH_CURRENT_EVIDENCE", "ALREADY_HANDLED", "CLOSED_UNSAFE", "NOT_A_GLYPH_PROBLEM"}

def classify(g):
    s = set(g.get("signals", []))
    tok = g.get("swallowed_token") or ""
    if "lone_glyph_inside_previous_verse" in s and " " not in tok:
        return "standalone_glyph_candidate"
    if "adjacent_numeric_fragment" in s or "adjacent_superscript_like_fragment" in s:
        return "detached_numeric_fragment"
    if "missing_marker_digit_inside_previous_verse" in s:
        return "exact_digit_but_context_rejected"
    if "marker_attached_to_text_candidate" in s:
        return "unsafe_compound_second_token"
    if "page_transition_candidate" in s or "ocr_block_split_candidate" in s:
        return "layout_or_column_corruption"
    if "no_local_numeric_evidence" in s:
        return "no_local_marker_evidence"
    return "unknown"

def build(audit, audit_sha=""):
    v = audit["verse_segmentation_audit"]
    rows = []
    forms = Counter(); blocks = set(); lines = 0
    for g in sorted(v["gaps"], key=lambda x: x["key"]):
        root = classify(g)
        form = g.get("swallowed_token") if g.get("swallowed_token") is not None else ""
        candidate = bool(g.get("swallowed_block") or g.get("signals"))
        if candidate: lines += 1
        if g.get("swallowed_block"): blocks.add(g["swallowed_block"])
        if form: forms[form] += 1
        rows.append({"key": g["key"], "book": g["book"], "chapter": g["chapter"], "shape": g["shape"],
                     "primary_root_cause": root, "signals": sorted(g.get("signals", [])),
                     "candidate_line": candidate, "source_block": g.get("swallowed_block"),
                     "raw_ocr_form": form, "scan_page": g.get("previous_page"), "pdf_page": g.get("pdf_page"),
                     "column": g.get("previous_column"), "zone": "verse_body"})
    counts = Counter(r["primary_root_cause"] for r in rows)
    families = {
      "standalone_glyph_candidate": {"gaps": counts.get("standalone_glyph_candidate",0), "status":"NEEDS_MORE_REVIEW"},
      "detached_numeric_fragment": {"gaps": counts.get("detached_numeric_fragment",0), "status":"NEEDS_MORE_REVIEW"},
      "exact_digit_but_context_rejected": {"gaps": counts.get("exact_digit_but_context_rejected",0), "status":"NEEDS_MORE_REVIEW"},
      "layout_or_column_corruption": {"gaps": counts.get("layout_or_column_corruption",0), "status":"NOT_A_GLYPH_PROBLEM"},
      "no_local_marker_evidence": {"gaps": counts.get("no_local_marker_evidence",0), "status":"NEEDS_MORE_REVIEW"},
      "unsafe_compound_second_token": {"gaps": counts.get("unsafe_compound_second_token",0), "status":"UNSAFE_WITH_CURRENT_EVIDENCE"},
      "unknown": {"gaps": counts.get("unknown",0), "status":"NEEDS_MORE_REVIEW"},
      "task_125_126_128_131": {"gaps":0, "status":"ALREADY_HANDLED"},
      "GLUED_FRAME": {"gaps":0, "status":"CLOSED_UNSAFE"},
    }
    recommendation = "standalone_glyph_candidate"
    try: commit = subprocess.check_output(["git","rev-parse","HEAD"], text=True).strip()
    except Exception: commit = "unknown"
    return {"schema_version":1, "source_provenance":{"audit":"build/torresamat1835-audit/volume3.json","audit_sha256":audit_sha,"edition_id":audit.get("edition_id"),"witness":audit.get("witness"),"volume":audit.get("volume"),"parser_commit":commit,"offline":True},
      "current_parser_baseline":{"verse_refs":audit["verse_refs"],"materialized_refs":audit["materialized_verse_refs"],"physical_gaps":len(rows),"glyph_gaps":v["by_signal"].get("lone_glyph_inside_previous_verse",0)},
      "physical_gap_total":len(rows), "gap_shape_counts":dict(sorted(Counter(r["shape"] for r in rows).items())),
      "glyph_gap_total":v["by_signal"].get("lone_glyph_inside_previous_verse",0), "candidate_line_count":lines,
      "unique_candidate_blocks":len(blocks), "exact_form_count":len(forms), "exact_form_frequency":dict(sorted(forms.items())),
      "root_cause_counts":dict(sorted(counts.items())), "root_cause_identity":sum(counts.values())==len(rows),
      "rows":rows, "families":families, "review_coverage":{"prior_batches":[124,127,128,129,130,131,132,133],"batch_134":0},
      "next_task_family":recommendation, "next_task_reason":"Largest glyph-rooted population; requires stratified facsimile review with explicit Spanish-text negatives before any recovery.",
      "glued_frame":{"status":"CLOSED_UNSAFE","automation_candidate":False},
      "rules":{"expected_verse_used":False,"previous_next_verse_used":False,"arbitrary_priority_score":False}}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--audit",required=True); ap.add_argument("--out",required=True)
    ns=ap.parse_args(); raw=Path(ns.audit).read_bytes(); data=build(json.loads(raw), hashlib.sha256(raw).hexdigest())
    Path(ns.out).write_text(json.dumps(data, ensure_ascii=False, indent=2)+"\n")
    print(json.dumps({"physical_gap_total":data["physical_gap_total"],"root_cause_counts":data["root_cause_counts"]},sort_keys=True))
if __name__ == "__main__": main()
