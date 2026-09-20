"""Focused runtime contract for TORRES-1835 task 139."""
import ast
import json
import pathlib
import subprocess
import tempfile
from functools import lru_cache

ROOT = pathlib.Path(__file__).resolve().parents[2]
XML = ROOT / "build/torresamat1835-cache/lasagradabiblia01unkngoog_djvu.xml"
PARSER = ROOT / "scripts/torresamat1835/page_parser.py"
COMPOUND = ROOT / "scripts/torresamat1835/compound_glyphs.py"


@lru_cache(maxsize=1)
def _audit():
    with tempfile.TemporaryDirectory() as directory:
        output = pathlib.Path(directory) / "volume3.json"
        subprocess.run([
            "python3", str(ROOT / "scripts/torresamat1835/audit_volume.py"),
            "--xml", str(XML), "--volume", "3", "--witness",
            "ia-lasagradabiblia01unkngoog", "--book", "Ps", "--out",
            str(output)], cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        return json.loads(output.read_text(encoding="utf-8"))


def test_runtime_delta_and_accounting():
    audit = _audit()
    seg = audit["verse_segmentation_audit"]
    recovery = seg["zero_anchor_io_recovery"]
    assert audit["verse_refs"] == 3849
    assert recovery["candidate_occurrences"] == 18
    assert recovery["candidate_source_events"] == 1
    assert recovery["parser_markers_interpreted"] == 1
    assert recovery["new_refs"] == 1
    assert recovery["new_ref_identities"] == ["Ps.17.10"]
    assert recovery["reopened_refs"] == 0
    assert recovery["removed_refs"] == []
    assert recovery["renumbered_refs"] == []
    assert recovery["ownership_moves"] == 27
    assert recovery["physical_gaps_before"] == 3255
    assert recovery["physical_gaps_after"] == 3254
    assert recovery["physical_gaps_closed"] == ["Ps.17.10"]
    assert recovery["glyph_gaps_before"] == 1310
    assert recovery["glyph_gaps_after"] == 1309
    assert recovery["glyph_gaps_closed"] == ["Ps.17.10"]
    assert recovery["block_loss"] == 0
    assert recovery["dual_ownership"] == 0
    assert audit["duplicate_refs"] == []
    assert audit["out_of_order_refs"] == []
    assert audit["chapters"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert not audit["canonical_chapter_gap_reviews"]["canonical_missing"]
    assert audit["metrics"]["ocr_blocks"] == 57700


def test_task_regressions_are_separately_accounted():
    seg = _audit()["verse_segmentation_audit"]
    old = seg["compound_glyph_recovery"]
    pixel = seg["a_glyph_pixel_recovery"]
    assert (old["dry_run_matches"], old["applied"], old["refs_added"],
            old["blocks_moved"]) == (183, 183, 177, 1355)
    assert (pixel["applied"], pixel["refs_added"], pixel["blocks_moved"]) == (
        276, 276, 1900)
    assert seg["zero_anchor_io_recovery"]["task128_historical_counters_excluded"]
    assert seg["zero_anchor_io_recovery"]["task131_overlap"] == 0


def test_source_has_bounded_fail_closed_gate_without_occurrence_allowlist():
    parser = PARSER.read_text(encoding="utf-8")
    compound = COMPOUND.read_text(encoding="utf-8")
    forbidden = ("scan page 32", "PDF page 33", "Ps.17.10",
                 "p0032", "previous + 1", "next - 1")
    assert not any(token in parser for token in forbidden)
    tree = ast.parse(parser)
    source = ast.get_source_segment(parser, next(
        node for node in ast.walk(tree)
        if isinstance(node, ast.FunctionDef) and node.name == "_try_compound"))
    assert source is not None
    assert 'detail.get("form") == "I o"' in source
    assert "anchor_count == 0" in source
    assert "placed.zone is Zone.BODY" in source
    assert "placed.column is Column.RIGHT" in source
    assert source.index('compound_glyphs.match') < source.index(
        'detail.get("form") == "I o"')
    assert "previous + 1" not in parser
    assert "next - 1" not in parser
    assert "Ps.17.10" not in compound


def test_anchor_policy_and_compound_value_remain_narrow():
    compound = COMPOUND.read_text(encoding="utf-8")
    tree = ast.parse(compound)
    fn = next(node for node in ast.walk(tree)
              if isinstance(node, ast.FunctionDef)
              and node.name == "trusted_anchor_count")
    source = ast.get_source_segment(compound, fn)
    assert "MIN_BAND_MARKERS" in source
    assert "count if count >= MIN_BAND_MARKERS else 0" in source
    assert "I o" not in source


if __name__ == "__main__":
    for name in sorted(globals()):
        if name.startswith("test_"):
            globals()[name]()
    print("ok")
