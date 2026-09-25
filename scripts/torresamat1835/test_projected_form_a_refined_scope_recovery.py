#!/usr/bin/env python3
"""Focused production contract for TORRES-1835 task 146.

The projected form-a fallback is exercised through the ACTUAL parser and
audit.  Diagnostic artifacts from tasks 141-145 are opened only after the
runtime has derived its own recovery, and only as a test oracle.
"""
import ast
import builtins
import io
import json
import os
import re
import sys
import time

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
sys.path.insert(0, DIR)

import audit_volume
import page_parser
import projected_form_a_recovery as rule
import projected_form_a_visible_2_discriminator as task142
import projected_form_a_visible_2_dry_run as task143
import source_ocr
import structure
from model import Block, BlockKind, Edition, Provenance

DATA = os.path.join(ROOT, "data/torresamat1835")
XML = os.path.join(ROOT, "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml")
WITNESS = "ia-lasagradabiblia01unkngoog"
RUNTIME_SOURCES = ("page_parser.py", "projected_form_a_recovery.py",
                   "compound_glyphs.py")
HISTORICAL = ("projected_form_a_facsimile.json",
              "projected_form_a_visible_2_discriminator.json",
              "projected_form_a_visible_2_dry_run.json",
              "projected_form_a_production_scope_audit.json",
              "projected_form_a_refined_scope_dry_run.json")


def _oracle(name):
    with open(os.path.join(DATA, name), encoding="utf-8") as handle:
        return json.load(handle)


def _sections(report):
    """Task-128/131/139 audit sections without wall-clock timings."""
    seg = report["verse_segmentation_audit"]
    out = {key: dict(seg[key]) for key in ("compound_glyph_recovery",
                                           "a_glyph_pixel_recovery",
                                           "zero_anchor_io_recovery")}
    out["a_glyph_pixel_recovery"].pop("performance", None)
    return out


# --- synthetic guard contracts (no corpus) ---------------------------------

def _block(block_id, text, number):
    return Block(kind=BlockKind.VERSE, text=text, number=number,
                 provenance=Provenance(witness="w", block_id=block_id),
                 decision="continuation of the open verse")


def _chapter(verses):
    """verses: {verse: [(block_id, text), ...]} in one native chapter Ps.5."""
    edition = Edition(edition_id="t")
    chapter = edition.book("Ps").chapter(5)
    blocks = {}
    for verse, rows in verses.items():
        for block_id, text in rows:
            blk = _block(block_id, text, verse)
            chapter.verse(verse).blocks.append(blk)
            blocks[block_id] = blk
    return edition, chapter, blocks


def _candidate(block_id, text):
    return {"block_id": block_id, "text": text.split(" ", 1)[1],
            "outcome": None, "applied": False}


def _limit(_book, _chapter):
    return 20


def test_guards_fail_closed_and_value_is_not_sequence_derived():
    # AN/AP: verses 2..6 missing between verse 1 and verse 7. next-1 would be
    # 6 and the gap inventory lists five missing numbers; the bounded family
    # value is 2 and nothing else, and the other gaps stay open.
    edition, chapter, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"),
            ("p0001l0002", "a Dios clamé yo"), ("p0001l0003", "y me oyó")],
        7: [("p0001l0009", "7 Otro verso")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.apply(edition, [record], {"p0001l0002": blocks["p0001l0002"]},
               enabled=True, verse_limit=_limit)
    assert record["outcome"] == rule.RECOVERED, record
    assert record["proposed_native_ref"] == "Ps.5.2"
    assert sorted(chapter.verses) == [1, 2, 7]
    assert [b.provenance.block_id for b in chapter.verses[1].blocks] == [
        "p0001l0001"]
    assert [b.provenance.block_id for b in chapter.verses[2].blocks] == [
        "p0001l0002", "p0001l0003"]
    assert blocks["p0001l0002"].text == "Dios clamé yo"
    assert blocks["p0001l0002"].raw_text == "a Dios clamé yo"  # raw OCR kept
    assert blocks["p0001l0002"].decision == rule.DECISION

    # Provenance guard: a band-shaped "a" line with nothing missing around it.
    edition, _chapter_obj, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        2: [("p0001l0003", "2 Dos")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.apply(edition, [record], blocks, enabled=True, verse_limit=_limit)
    assert record["outcome"] == rule.PROVENANCE

    # Order guard: the owner is already verse 7.
    edition, _c, blocks = _chapter({
        1: [("p0001l0001", "1 Uno")],
        7: [("p0001l0005", "7 Siete"), ("p0001l0006", "a Dios clamé yo")],
        9: [("p0001l0008", "9 Nueve")]})
    record = _candidate("p0001l0006", "a Dios clamé yo")
    rule.apply(edition, [record], blocks, enabled=True, verse_limit=_limit)
    assert record["outcome"] == rule.ORDER
    assert 2 not in edition.books["Ps"].chapters[5].verses

    # Outside native canon (a one-verse chapter).
    edition, _c, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        3: [("p0001l0003", "3 Tres")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.apply(edition, [record], blocks, enabled=True,
               verse_limit=lambda _b, _c: 1)
    assert record["outcome"] in (rule.OUTSIDE_CANON, rule.PROVENANCE)

    # Two source events proposing the same ref abstain together. The gap
    # inventory yields one swallowed line per previous verse, so this is a
    # defensive branch: provenance is forced here to reach it.
    edition, _c, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        0: [("p0000l0008", "titulo"), ("p0000l0009", "a otro renglón más")],
        4: [("p0001l0004", "4 Cuatro")]})
    one = _candidate("p0001l0002", "a Dios clamé yo")
    two = _candidate("p0000l0009", "a otro renglón más")
    real = rule.projected_gap_blocks
    rule.projected_gap_blocks = lambda _e, verse_limit: {"p0001l0002",
                                                          "p0000l0009"}
    try:
        rule.apply(edition, [one, two], blocks, enabled=True,
                   verse_limit=_limit)
    finally:
        rule.projected_gap_blocks = real
    assert one["outcome"] == two["outcome"] == rule.AMBIGUOUS_EVENT
    assert 2 not in edition.books["Ps"].chapters[5].verses

    # Verse 2 already present: reopening is refused.
    edition, _c, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        2: [("p0001l0003", "2 Dos")], 4: [("p0001l0004", "4 Cuatro")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.projected_gap_blocks = lambda _e, verse_limit: {"p0001l0002"}
    try:
        rule.apply(edition, [record], blocks, enabled=True, verse_limit=_limit)
    finally:
        rule.projected_gap_blocks = real
    assert record["outcome"] == rule.REOPEN

    # Not owned as a continuation (a stronger path or the queue took it).
    edition, _c, _blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        4: [("p0001l0004", "4 Cuatro")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.apply(edition, [record], {}, enabled=True, verse_limit=_limit)
    assert record["outcome"] == rule.NOT_OWNED

    # Disabled: same code path, no mutation.
    edition, chapter, blocks = _chapter({
        1: [("p0001l0001", "1 Uno"), ("p0001l0002", "a Dios clamé yo")],
        4: [("p0001l0004", "4 Cuatro")]})
    record = _candidate("p0001l0002", "a Dios clamé yo")
    rule.apply(edition, [record], blocks, enabled=False, verse_limit=_limit)
    assert record["outcome"] == rule.WOULD_RECOVER
    assert sorted(chapter.verses) == [1, 4]


def test_discriminator_matches_task142_rule():
    class W:
        def __init__(self, text, x):
            self.text, self.bbox = text, (x, 0, x + 20, 30)

    class L:
        def __init__(self, words):
            self.words = words

    band = (100.0, 40.0, 5)  # tolerance = max(0.5 * 40, 30) = 30
    ok = L([W("a", 110), W("Dios", 140), W("clamé", 200), W("yo", 260)])
    assert rule.match(ok, band)[0] == "Dios clamé yo"
    assert rule.match(ok, None)[1] == "no_trusted_marker_band"
    far = L([W("a", 131), W("Dios", 160), W("clamé", 220), W("yo", 280)])
    assert rule.match(far, band)[1] == "outside_marker_band"
    short = L([W("a", 110), W("y", 140), W("clamé", 200), W("yo", 260)])
    assert rule.match(short, band)[1] == rule.PROSE_PREFIX
    framed = L([W("«a", 110), W("Dios", 140), W("clamé", 200), W("yo", 260)])
    assert rule.match(framed, band)[2]["form"] is None  # GLUED_FRAME untouched
    three = L([W("a", 110), W("Dios", 140), W("clamé", 200)])
    assert rule.match(three, band)[2]["form"] is None


# --- static source guards ---------------------------------------------------

def test_runtime_source_has_no_identity_or_artifact_authority():
    block_id = re.compile(r"p\d{4}[lr]\d{4}")
    verse_ref = re.compile(r"\b[1-4]?[A-Z][a-z]+\.\d+\.\d+\b")
    for name in RUNTIME_SOURCES:
        text = open(os.path.join(DIR, name), encoding="utf-8").read()
        assert not block_id.search(text), name
        assert not verse_ref.search(text), (name, verse_ref.findall(text))
        for artifact in HISTORICAL:
            assert artifact not in text, (name, artifact)
        for forbidden in ("previous + 1", "next - 1", "previous+1", "next-1"):
            assert forbidden not in text, (name, forbidden)
    module = open(os.path.join(DIR, "projected_form_a_recovery.py"),
                  encoding="utf-8").read()
    for forbidden in (".pdf", "pdftoppm", "pytesseract", "PIL", "subprocess",
                      "urllib", "socket"):
        assert forbidden not in module, forbidden
    tree = ast.parse(module)
    imports = {alias.name for node in ast.walk(tree)
               if isinstance(node, ast.Import) for alias in node.names}
    imports |= {node.module for node in ast.walk(tree)
                if isinstance(node, ast.ImportFrom)}
    assert imports <= {"typing", "compound_glyphs", "layout", "verse_gaps"}, imports
    calls = {node.func.attr if isinstance(node.func, ast.Attribute) else
             getattr(node.func, "id", None)
             for node in ast.walk(tree) if isinstance(node, ast.Call)}
    assert not calls & {"open", "read_text", "read_bytes", "load", "loads"}
    # No literal collections of identities: every set/list/tuple/dict literal
    # in the runtime module is empty or holds non-identity values.
    for node in ast.walk(tree):
        if isinstance(node, (ast.Set, ast.List, ast.Tuple)):
            for element in node.elts:
                if isinstance(element, ast.Constant) and isinstance(element.value, str):
                    assert not block_id.search(element.value)
                    assert not verse_ref.search(element.value)
    # The value 2 is a single named constant, not derived from neighbours.
    apply_source = ast.get_source_segment(module, next(
        node for node in ast.walk(tree)
        if isinstance(node, ast.FunctionDef) and node.name == "apply"))
    for forbidden in ("previous_verse", "next_verse", "gap.verse", "missing",
                      "+ 1", "- 1"):
        assert forbidden not in apply_source, forbidden


def test_parser_reads_no_diagnostic_or_image_input():
    opened = []
    real_open = builtins.open

    def spy(path, *args, **kwargs):
        opened.append(str(path))
        return real_open(path, *args, **kwargs)

    builtins.open = io.open = spy
    try:
        page_parser.parse_volume(source_ocr.read_pages(XML, limit=40),
                                 witness=WITNESS, volume="3", book="Ps")
    finally:
        builtins.open = io.open = real_open
    for path in opened:
        assert "projected_form_a" not in path, path
        assert not path.lower().endswith((".pdf", ".png", ".jpg", ".tif")), path


# --- the corpus contract ----------------------------------------------------

def test_runtime_recovery_reproduces_task145():
    started = time.perf_counter()
    # The task-146 contract is the production state before task 150.
    edition, report = audit_volume.audit(XML, volume="3", witness=WITNESS,
                                         book="Ps", printed_2x_recovery=False)
    production_seconds = time.perf_counter() - started
    _pre_edition, pre = audit_volume.audit(XML, volume="3", witness=WITNESS,
                                           book="Ps",
                                           projected_form_a_recovery=False)
    seg = report["verse_segmentation_audit"]
    run = seg["projected_form_a_refined_scope_recovery"]
    pre_run = pre["verse_segmentation_audit"][
        "projected_form_a_refined_scope_recovery"]
    recovered = set(run["recovered_blocks"])

    # A. the production placement population (task-143 enumerator) is 158.
    assert len(task143.production_matches(XML)) == 158
    # B/E/F/G/H. runtime derives exactly 43 new refs, no reopen.
    assert run["recovered_markers"] == 43 and len(recovered) == 43
    assert run["created_refs"] == 43
    assert run["reopened_refs"] == 0 and run["removed_refs"] == []
    assert pre["verse_refs"] == 3849 and run["verse_refs_before"] == 3849
    assert report["verse_refs"] == 3892 and run["verse_refs_after"] == 3892
    assert run["outcomes"][rule.RECOVERED] == 43
    # The disabled pass saw the same decisions and changed nothing.
    assert pre_run["outcomes"].get(rule.WOULD_RECOVER) == 43
    assert pre_run["recovered_markers"] == 0 and pre_run["created_refs"] == 0
    # J/L/M/N. ownership.
    assert run["ownership_moves"] == 581
    assert run["owned_blocks_before"] == run["owned_blocks_after"] == 25434
    assert run["block_loss"] == 0 and run["blocks_entering_text"] == 0
    assert run["dual_ownership"] == 0
    # O-V. gaps.
    assert (run["physical_gaps_before"], run["physical_gaps_after"]) == (3254, 3211)
    assert len(run["physical_gaps_closed"]) == 43
    assert run["physical_gaps_opened"] == []
    assert (run["glyph_gaps_before"], run["glyph_gaps_after"]) == (1309, 1266)
    assert len(run["glyph_gaps_closed"]) == 43
    assert run["glyph_gaps_opened"] == []
    assert len(seg["gaps"]) == 3211
    assert seg["by_signal"]["lone_glyph_inside_previous_verse"] == 1266
    # AM. stronger paths keep priority.
    assert run["stronger_path_overlap"] == 0
    assert run["external_unreadable_recovered"] == 0
    assert run["known_order_conflicts_recovered"] == 0

    # ---- oracle comparison: task-145 artifact read only now ----
    oracle = _oracle("projected_form_a_refined_scope_dry_run.json")
    scope = {event["physical_block"] for event in oracle["recovery_events"]}
    assert recovered == scope  # B/C/AA. identical selected source events
    assert run["created_ref_identities"] == oracle["ref_effects"]["added_refs"]  # I.
    by_block = {row["block_id"]: row for row in run["records"]}
    for event in oracle["recovery_events"]:
        row = by_block[event["physical_block"]]
        assert row["proposed_native_ref"] == event["proposed_native_ref"]
        assert row["prior_owner_ref"] == event["prior_owner_refs"][0]
    assert run["moved_blocks"] == oracle["ownership_before_after"]["unique_moved_blocks"]  # K.
    assert run["physical_gaps_closed"] == oracle["physical_gap_before_after"]["closed"]  # Q.
    assert run["glyph_gaps_closed"] == oracle["glyph_gap_before_after"]["closed"]  # U.
    assert oracle["physical_gap_before_after"]["opened"] == run["physical_gaps_opened"]
    assert oracle["glyph_gap_before_after"]["opened"] == run["glyph_gaps_opened"]

    # W/X. task-144 external UNREADABLE and order conflicts stay out.
    scope_audit = _oracle("projected_form_a_production_scope_audit.json")
    external = {row["block"] for row in scope_audit["external_matches"]}
    unreadable = {row["block"] for row in scope_audit["external_matches"]
                  if row["source_review"]["primary_classification"] == "UNREADABLE"}
    conflicts = {row["physical_block"]
                 for row in scope_audit["late_ref_conflict_analysis"]}
    assert len(external) == len(unreadable) == 111 and len(conflicts) == 13
    assert not recovered & external and not recovered & conflicts
    # Y/Z. task-141 labels: every recovery is a printed 2; no control taken.
    truth = _oracle("projected_form_a_facsimile.json")
    labels = {}
    for row in truth["occurrence_reviews"]:
        labels.setdefault(row["ocr_block_id"], []).append(row)
    assert recovered <= set(labels)
    for block in recovered:
        for row in labels[block]:
            assert row["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
            assert row["visible_printed_value"] == 2
    controls = {block for block, rows in labels.items()
                if any(not (row["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
                            and row["visible_printed_value"] == 2) for row in rows)}
    wrong_value = {block for block, rows in labels.items()
                   if any(row["facsimile_review_class"] == "PRINTED_VERSE_MARKER"
                          and row["visible_printed_value"] != 2 for row in rows)}
    assert not recovered & controls and not recovered & wrong_value

    # AB/AC/AD. earlier recoveries are identical to the pre-146 runtime,
    # identities included, and match the frozen task-145 evidence hash.
    assert _sections(report) == _sections(pre)
    snapshot = task142.runtime_snapshot(report)
    assert snapshot["task128"] == {"markers": 183, "refs": 177, "moves": 1355}
    assert snapshot["task131"] == {"markers": 276, "refs": 276, "moves": 1900}
    assert (snapshot["recovery_evidence_sha256"] ==
            oracle["runtime_invariants"]["recovery_evidence_sha256"])
    task139 = seg["zero_anchor_io_recovery"]
    assert task139["new_ref_identities"] == ["Ps.17.10"]
    assert task139["ownership_moves"] == 27
    assert "Ps.17.10" not in run["created_ref_identities"]
    stronger = {row["block_id"] for row in seg["compound_glyph_recovery"]["markers"]}
    assert not recovered & stronger
    # AE. the GLUED_FRAME family is not reopened: every recovered line starts
    # with an exact, unframed "a".
    assert snapshot["GLUED_FRAME"] == "CLOSED_UNSAFE"
    for record in run["records"]:
        if record["outcome"] == rule.RECOVERED:
            assert record["indent"] is not None
            assert abs(record["indent"]) <= record["tolerance"]

    # AF-AL. corpus invariants.
    assert report["duplicate_refs"] == [] and report["out_of_order_refs"] == []
    assert len(report.get("outside_canon", [])) == 0
    assert report["chapters"] == 337
    assert report["chapter_claims"]["unresolved"] == 0
    assert not report["canonical_chapter_gap_reviews"]["canonical_missing"]
    assert report["metrics"]["ocr_blocks"] == 57700
    impossible = [ref for ref in audit_volume._reference_map(edition)
                  if (structure.verse_limit(ref.split(".")[0], int(ref.split(".")[1]))
                      or 10**6) < int(ref.split(".")[2])]
    assert impossible == []

    print(json.dumps({
        "recovered_markers": run["recovered_markers"],
        "created_refs": run["created_refs"],
        "ownership_moves": run["ownership_moves"],
        "physical_gaps": run["physical_gaps_after"],
        "glyph_gaps": run["glyph_gaps_after"],
        "candidate_matches": run["candidate_matches"],
        "outcomes": run["outcomes"],
        "timing": run["timing"],
        "production_audit_seconds": round(production_seconds, 2)},
        sort_keys=True))


if __name__ == "__main__":
    for name in sorted(globals()):
        if name.startswith("test_"):
            globals()[name]()
    print("ok")
