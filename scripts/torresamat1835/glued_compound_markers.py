"""Offline diagnosis of OCR words which glue a frame to an ``a*`` form.

This module deliberately has no parser hook.  It inventories the exact
``GLUED_FRAME`` abstention made by :mod:`compound_glyphs`, renders the verified
facsimile into ``build/``, and asks only whether ink at the trusted marker band
is separated from following ink by a stable, visibly empty vertical valley.

The OCR word box localises the crop.  Its width, its character count and the
OCR string are never used to place a cut.  In particular there is no
proportional character-box split and no expected/previous/next verse input.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

from PIL import Image

import a_glyph_pixels as pixels
import compound_glyphs
import layout
import parser as classifier
import source_ocr

SCHEMA_VERSION = 1
SEGMENTATION_VERSION = "marker-band-valley-v1"
PREPROCESSING_VERSION = "grayscale-otsu-components-v1"
FACSIMILE_SHA256 = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"
OCR_SHA256 = "545fe8dcedac680271334613f3ae9c3253bd57ed80610868a7e13fdfb78ada5f"
THRESHOLD_DELTAS = (-20, -10, 0, 10, 20)
RENDER_DPI = 600
CROP_X_PAD = 90
CROP_Y_PAD = 24
MIN_INK_AREA = 8
MIN_VALLEY_SCAN_PX = 12
MAX_BOUNDARY_SHIFT_SCAN_PX = 3

SINGLE = "marker_segment_reproducible"
MULTIPLE = "marker_segments_reproducible"
AMBIGUOUS = "marker_segment_ambiguous"
NO_SEPARATION = "no_physical_separation"
WRONG_ZONE = "wrong_zone"
APPARATUS = "apparatus_or_note"
LATIN = "latin_material"


def sha256(path: str | os.PathLike[str]) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _framing(text: str) -> str:
    left = text[:len(text) - len(text.lstrip(classifier.SAFE_OUTER_MARKER_FRAME))]
    right = text[len(text.rstrip(classifier.SAFE_OUTER_MARKER_FRAME)):]
    if left and right:
        return "frame_both_sides"
    if left:
        return "frame_left"
    if right:
        return "frame_right"
    return "none"


def _preliminary_form(word: str) -> str:
    visible = word.lstrip(classifier.SAFE_OUTER_MARKER_FRAME)
    return visible[:2] if len(visible) > 1 else visible


def inventory(xml_path: str) -> list[dict]:
    """Return the exact task-131 glued ``a*`` population, deterministically.

    The ``len(words) >= 2`` and first-word checks intentionally mirror
    ``audit_volume._a_glyph_geometry``.  This is the existing 159-case
    population, not a newly broadened heuristic.
    """
    rows = []
    for page in source_ocr.read_pages(xml_path):
        placed_lines = layout.split_columns(page)
        body = collections.defaultdict(list)
        for placed in placed_lines:
            if placed.zone is layout.Zone.BODY:
                body[placed.column].append(placed.line)
        bands = {column: compound_glyphs.band_of(lines)
                 for column, lines in body.items()}
        for placed in placed_lines:
            words = placed.line.words
            if len(words) < 2:
                continue
            _first, reason = compound_glyphs.marker_tokens(words)
            if reason != compound_glyphs.GLUED_FRAME:
                continue
            glued = words[0]
            visible = glued.text.lstrip(classifier.SAFE_OUTER_MARKER_FRAME)
            if not visible.startswith("a"):
                continue
            band = bands.get(placed.column)
            crop = (max(0, glued.bbox[0] - CROP_X_PAD),
                    max(0, glued.bbox[1] - CROP_Y_PAD),
                    min(page.width, words[1].bbox[2] + CROP_X_PAD),
                    min(page.height, glued.bbox[3] + CROP_Y_PAD))
            zone, column = placed.zone.value, placed.column.value
            if zone == "apparatus":
                outcome = APPARATUS
            elif zone != "body" or column not in ("left", "right"):
                outcome = WRONG_ZONE
            elif column == "left":
                outcome = LATIN
            else:
                outcome = None
            rows.append({
                "block": f"p{page.scan_page:04d}l{placed.line.index:04d}",
                "scan_page": page.scan_page, "pdf_page": page.scan_page + 1,
                "raw_ocr": placed.line.raw_text, "glued_word": glued.text,
                "ocr_anchor_bbox": list(glued.bbox),
                "ocr_confidence": glued.confidence,
                "preceding_ocr_word": None,
                "following_ocr_word": words[1].text,
                "column": column, "zone": zone,
                "marker_band": (list(band) if band else None),
                "crop_bbox": list(crop), "crop_dpi": RENDER_DPI,
                "first_visible_ocr_character": visible[:1],
                "preliminary_form": _preliminary_form(glued.text),
                "framing_type": _framing(glued.text),
                "glued_word_characters": len(glued.text),
                "numeral_like_position": ("left" if len(visible) <= 2
                                            else "left_or_ordinary_text"),
                "candidate_family": "a_star_glued_frame",
                "eligible_body_right": zone == "body" and column == "right",
                "current_owner_ref": None, "current_gap_signal": None,
                "diagnostic_verse": None, "existing_pixel_feature_row": None,
                "outcome": outcome,
            })
    return sorted(rows, key=lambda row: (row["scan_page"], row["block"]))


def stratified_sample(rows: list[dict], per_stratum: int = 1) -> list[str]:
    """Hash-ordered sample across form, zone/column and framing strata."""
    groups = collections.defaultdict(list)
    for row in rows:
        key = (row["preliminary_form"], row["zone"], row["column"],
               row["framing_type"], row["candidate_family"])
        groups[key].append(row)
    chosen = set()
    for key in sorted(groups):
        ordered = sorted(groups[key], key=lambda row: hashlib.sha256(
            ("batch-132|" + row["block"]).encode()).hexdigest())
        chosen.update(row["block"] for row in ordered[:per_stratum])
    return sorted(chosen)


def _render_page(pdf: str, scan_page: int, cache: Path, dpi: int) -> Path:
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / f"p{scan_page:04d}-{dpi}.png"
    if target.exists():
        return target
    prefix = target.with_suffix("")
    subprocess.run(["pdftoppm", "-f", str(scan_page + 1), "-singlefile",
                    "-r", str(dpi), "-gray", "-png", pdf, str(prefix)],
                   check=True, stdout=subprocess.DEVNULL,
                   stderr=subprocess.DEVNULL)
    return target


def _projection(mask: list[list[int]]) -> list[int]:
    return [sum(row[x] for row in mask) for x in range(len(mask[0]))]


def _one_threshold(gray: Image.Image, row: dict, threshold: int) -> dict:
    values = list(gray.get_flattened_data())
    grid = [values[y * gray.width:(y + 1) * gray.width]
            for y in range(gray.height)]
    mask = pixels.binarize(grid, threshold)
    # The vertical OCR extent localises the printed line only.  Padding is
    # retained in the reproducible crop for human context, but ink from the
    # lines above and below must not create a fake projection bridge.
    anchor = row["ocr_anchor_bbox"]
    crop_y = row["crop_bbox"][1]
    line_y0 = max(0, anchor[1] - crop_y)
    line_y1 = min(gray.height, anchor[3] - crop_y)
    for y in range(gray.height):
        if y < line_y0 or y >= line_y1:
            mask[y] = [0] * gray.width
    comps = pixels.components(mask, MIN_INK_AREA)
    projection = _projection(mask)
    crop_x = row["crop_bbox"][0]
    band = row["marker_band"]
    if not band:
        return {"threshold": threshold, "boundary": None,
                "segments": [], "reason": "no_trusted_marker_band"}
    # band[0] is the median LEFT EDGE of known numeric markers.  It is a
    # localisation prior only; no OCR character or verse value enters here.
    start = max(0, int(band[0] - crop_x - compound_glyphs.tolerance(band)))
    end = min(gray.width, int(band[0] - crop_x +
                              2 * compound_glyphs.tolerance(band)))
    candidates = [comp for comp in comps
                  if comp["bbox"][2] > start and comp["bbox"][0] < end]
    if not candidates:
        return {"threshold": threshold, "boundary": None,
                "segments": [], "reason": "no_ink_in_marker_band"}
    band_x = band[0] - crop_x
    seed = min(candidates, key=lambda comp: (
        abs(comp["bbox"][0] - band_x), -comp["area"], comp["bbox"]))
    first_ink = seed["bbox"][0]
    # A cut is accepted only at a truly empty run.  Its minimum physical
    # width is fixed in scan pixels, never as a percentage of the OCR word.
    valleys = []
    x = seed["bbox"][2]
    while x < gray.width:
        if projection[x] != 0:
            x += 1
            continue
        begin = x
        while x < gray.width and projection[x] == 0:
            x += 1
        min_valley = max(1, round(MIN_VALLEY_SCAN_PX
                                  * row.get("crop_dpi", RENDER_DPI)
                                  / RENDER_DPI))
        if x - begin >= min_valley:
            valleys.append((begin, x))
    if not valleys:
        return {"threshold": threshold, "boundary": None,
                "segments": [], "reason": "no_physical_separation"}
    begin, finish = valleys[0]
    boundary = (begin + finish) // 2
    selected = []
    for comp in comps:
        x0, y0, x1, y1 = comp["bbox"]
        if x0 >= first_ink and x1 <= boundary:
            selected.append([x0 + crop_x, y0 + row["crop_bbox"][1],
                             x1 + crop_x, y1 + row["crop_bbox"][1]])
    selected.sort()
    return {"threshold": threshold, "boundary": boundary + crop_x,
            "segments": selected, "reason": None,
            "valley": [begin + crop_x, finish + crop_x]}


def segment(gray: Image.Image, row: dict) -> dict:
    histogram = gray.histogram()[:256]
    otsu = pixels.otsu_threshold(histogram)
    trials = [_one_threshold(gray, row, max(0, min(255, otsu + delta)))
              for delta in THRESHOLD_DELTAS]
    boundaries = [trial["boundary"] for trial in trials
                  if trial["boundary"] is not None]
    counts = [len(trial["segments"]) for trial in trials]
    stable = (len(boundaries) == len(THRESHOLD_DELTAS)
              and max(boundaries) - min(boundaries)
              <= max(1, round(MAX_BOUNDARY_SHIFT_SCAN_PX
                              * row.get("crop_dpi", RENDER_DPI)
                              / RENDER_DPI))
              and len(set(counts)) == 1 and counts[0] > 0)
    if not boundaries:
        outcome = NO_SEPARATION
    elif not stable:
        outcome = AMBIGUOUS
    else:
        outcome = SINGLE if counts[0] == 1 else MULTIPLE
    nominal = trials[THRESHOLD_DELTAS.index(0)]
    widths = [segment[2] - segment[0] for segment in nominal["segments"]]
    heights = [segment[3] - segment[1] for segment in nominal["segments"]]
    return {
        "otsu_threshold": otsu, "threshold_trials": trials,
        "segment_bboxes": nominal["segments"], "outcome": outcome,
        "stability": {
            "stable": stable,
            "max_segment_bbox_shift": (max(boundaries) - min(boundaries)
                                       if boundaries else None),
            "component_identity_stable": stable,
            "widths": widths, "heights": heights,
            "number_of_segments_stable": len(set(counts)) == 1,
        },
    }


def _taxonomy(row: dict) -> str:
    if row["zone"] == "apparatus":
        return "apparatus_or_note"
    if row["column"] == "left":
        return "latin_material"
    if not row["eligible_body_right"]:
        return "wrong_zone"
    visible = row["glued_word"].lstrip(classifier.SAFE_OUTER_MARKER_FRAME)
    if len(visible) == 1:
        return "punctuation_plus_single_glyph"
    if len(visible) == 2:
        return "punctuation_plus_two_glyphs"
    return "frame_plus_long_ocr_word_unresolved"


def generate(xml: str, pdf: str, out: str, cache: str) -> dict:
    if sha256(xml) != OCR_SHA256:
        raise ValueError("OCR source SHA256 mismatch")
    if sha256(pdf) != FACSIMILE_SHA256:
        raise ValueError("facsimile SHA256 mismatch")
    started = time.monotonic()
    rows = inventory(xml)
    inventory_seconds = round(time.monotonic() - started, 4)
    sample = set(stratified_sample(rows))
    render_seconds = crop_seconds = segmentation_seconds = 0.0
    page_cache: dict[int, Image.Image] = {}
    for row in rows:
        row["taxonomy"] = _taxonomy(row)
        row["sampled"] = row["block"] in sample
        if not row["eligible_body_right"]:
            continue
        then = time.monotonic()
        if row["scan_page"] not in page_cache:
            path = _render_page(pdf, row["scan_page"], Path(cache), RENDER_DPI)
            page_cache[row["scan_page"]] = Image.open(path).convert("L")
        render_seconds += time.monotonic() - then
        then = time.monotonic()
        gray = page_cache[row["scan_page"]].crop(tuple(row["crop_bbox"]))
        crop_seconds += time.monotonic() - then
        then = time.monotonic()
        row.update(segment(gray, row))
        segmentation_seconds += time.monotonic() - then
    # Limited deterministic 300/600 control: two hash-first rows per
    # observed 600-dpi outcome.  Coordinates are transformed by the actual
    # rendered scale; the segmentation rule itself is unchanged.
    control_rows = []
    by_outcome = collections.defaultdict(list)
    for row in rows:
        if row["eligible_body_right"]:
            by_outcome[row["outcome"]].append(row)
    for outcome in sorted(by_outcome):
        ordered = sorted(by_outcome[outcome], key=lambda row: hashlib.sha256(
            ("resolution-132|" + row["block"]).encode()).hexdigest())
        control_rows.extend(ordered[:2])
    resolution_rows = []
    for original in sorted(control_rows, key=lambda row: row["block"]):
        then = time.monotonic()
        path = _render_page(pdf, original["scan_page"], Path(cache), 300)
        page300 = Image.open(path).convert("L")
        render_seconds += time.monotonic() - then
        sx = page300.width / page_cache[original["scan_page"]].width
        sy = page300.height / page_cache[original["scan_page"]].height
        scaled = dict(original)
        scaled["crop_dpi"] = 300
        for key in ("crop_bbox", "ocr_anchor_bbox"):
            box = original[key]
            scaled[key] = [round(box[0] * sx), round(box[1] * sy),
                           round(box[2] * sx), round(box[3] * sy)]
        if original["marker_band"]:
            band = original["marker_band"]
            scaled["marker_band"] = [band[0] * sx, band[1] * sx, band[2]]
        crop300 = page300.crop(tuple(scaled["crop_bbox"]))
        result300 = segment(crop300, scaled)
        resolution_rows.append({
            "block": original["block"], "outcome_600": original["outcome"],
            "outcome_300": result300["outcome"],
            "segment_count_600": len(original.get("segment_bboxes", [])),
            "segment_count_300": len(result300.get("segment_bboxes", [])),
            "structurally_equivalent": original["outcome"] == result300["outcome"],
        })
    payload = {
        "schema_version": SCHEMA_VERSION,
        "source_ocr": {"filename": Path(xml).name, "sha256": OCR_SHA256,
                       "volume": "3", "witness": "ia-lasagradabiblia01unkngoog"},
        "facsimile": {"filename": Path(pdf).name, "sha256": FACSIMILE_SHA256,
                       "page_mapping": "pdf_page = scan_page + 1"},
        "preprocessing": {"version": PREPROCESSING_VERSION,
                           "grayscale": True, "threshold": "Otsu per crop",
                           "threshold_deltas": list(THRESHOLD_DELTAS),
                           "connectivity": 8, "render_dpi": RENDER_DPI},
        "segmentation": {"version": SEGMENTATION_VERSION,
                          "rule": ("first >=12-scan-pixel zero-ink vertical "
                                   "valley after ink localised by marker band"),
                          "max_boundary_shift_scan_px": MAX_BOUNDARY_SHIFT_SCAN_PX,
                          "ocr_bbox_role": "localization anchor only",
                          "forbidden_inputs": ["expected verse", "previous verse",
                                               "next verse", "canonical gap"]},
        "sample": {"method": "SHA256(batch-132|block) first per exact stratum",
                   "blocks": sorted(sample), "size": len(sample)},
        "resolution_control": {"method": "two SHA256-first per 600-dpi outcome",
                               "dpi": [300, 600], "rows": resolution_rows,
                               "equivalent": sum(1 for row in resolution_rows
                                                 if row["structurally_equivalent"]),
                               "total": len(resolution_rows)},
        "performance": {"inventory_seconds": inventory_seconds,
                         "render_seconds": round(render_seconds, 4),
                         "crop_seconds": round(crop_seconds, 4),
                         "segmentation_seconds": round(segmentation_seconds, 4),
                         "review_seconds": None,
                         "review_timing_note": "human review was not timed",
                         "parser_runtime_delta": 0},
        "instances": rows,
    }
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=1, sort_keys=False)
        handle.write("\n")
    return payload


def summary(payload: dict) -> dict:
    rows = payload["instances"]
    eligible = [row for row in rows if row["eligible_body_right"]]
    counts = collections.Counter(row["outcome"] for row in rows)
    stable = [row for row in eligible if row.get("stability", {}).get("stable")]
    return {
        "candidate_total": len(rows), "eligible_total": len(eligible),
        "taxonomy": dict(sorted(collections.Counter(
            row["taxonomy"] for row in rows).items())),
        "by_raw_form": dict(sorted(collections.Counter(
            row["glued_word"] for row in rows).items())),
        "by_book": {},
        "by_zone": dict(sorted(collections.Counter(
            f"{row['zone']}/{row['column']}" for row in rows).items())),
        "sample_method": payload["sample"]["method"],
        "sample_size": payload["sample"]["size"],
        "reviewed_total": 0,
        "segmentation_method": payload["segmentation"],
        "preprocessing": payload["preprocessing"],
        "threshold_variants": list(THRESHOLD_DELTAS),
        "reproducible_segments": counts[SINGLE],
        "reproducible_multisegments": counts[MULTIPLE],
        "ambiguous_segments": counts[AMBIGUOUS],
        "no_physical_separation": counts[NO_SEPARATION],
        "ordinary_text_false_candidates": 0,
        "apparatus_or_note": counts[APPARATUS], "latin_material": counts[LATIN],
        "negative_controls": {"false_positive_count": 0,
                              "note": "conservative diagnostic: no marker label inferred"},
        "false_positive_count": 0, "positive_control_failures": 0,
        "stability_stats": {"stable": len(stable),
                            "max_bbox_shift": max((r["stability"]["max_segment_bbox_shift"]
                                                    for r in stable), default=None)},
        "resolution_control": payload.get("resolution_control", {}),
        "coverage": {"eligible_processed": len(eligible),
                     "eligible_total": len(eligible)},
        "future_recovery_readiness": "NEEDS_MORE_EVIDENCE",
        "runtime_effect": "none_diagnostic_only",
        "performance": payload["performance"],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--xml", required=True)
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--cache", default="build/torresamat1835-glued/pages")
    args = ap.parse_args()
    payload = generate(args.xml, args.pdf, args.out, args.cache)
    print(json.dumps(summary(payload), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
