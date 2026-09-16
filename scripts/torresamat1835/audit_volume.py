"""
Pasada de auditoría sobre un tomo entero.

Sólo mide y describe: no corrige nada. Las métricas señalan dónde mirar
-- numerales imposibles, duplicados, saltos de numeración, bloques sin
clasificar --, y cada hallazgo sale con su procedencia para poder ir al
facsímil. Ninguna de estas señales se usa jamás para arreglar el texto.

    python3 audit_volume.py --xml <djvu.xml> --volume 3 \
        --out build/torresamat1835-audit/volume3.json
"""
import argparse
import json
import os
import time

import book_boundaries
import layout
import page_parser
import source_ocr
import structure
from model import BlockKind


def read_structure(xml_path, *, limit=None, gutter_hint=1700):
    """Primera pasada: sólo las cabeceras corridas.

    Barata y lineal. De aquí salen los tramos de libro y el numeral de
    capítulo de cada plana, que son las señales con que después se
    identifica cada división.
    """
    readings = []
    for page in source_ocr.read_pages(xml_path, limit=limit):
        header = " ".join(
            entry.line.raw_text
            for entry in layout.split_columns(page, gutter_hint=gutter_hint)
            if entry.zone is layout.Zone.HEADER)
        if header.strip():
            readings.append(structure.read_header(page.scan_page, header))
    return readings, structure.book_spans(readings)


def audit(xml_path, *, volume, witness, book="Ps", limit=None):
    started = time.time()
    readings, spans = read_structure(xml_path, limit=limit)
    header_chapters = {r.scan_page: r.chapters for r in readings}
    spans_before = [(s.osis, s.first_page) for s in spans]

    # La identidad de cada libro ya está confirmada por tres cabeceras.
    # Ahora, y sólo dentro de una ventana acotada, se busca desde qué
    # bloque empieza de verdad. Las planas de la ventana se releen una
    # sola vez: el coste sigue siendo lineal.
    needed = set()
    for index in range(1, len(spans)):
        low = max(spans[index - 1].first_page + 1,
                  spans[index].first_page - book_boundaries.LOOKBACK_PAGES)
        needed.update(range(low, spans[index].first_page + 1))
    window_cache = {}
    if needed:
        for page in source_ocr.read_pages(xml_path, limit=limit):
            if page.scan_page in needed:
                window_cache[page.scan_page] = (
                    page.width, layout.split_columns(page, gutter_hint=1700))

    def window_for(low, high):
        return [(p, window_cache[p][0], window_cache[p][1])
                for p in range(low, high + 1) if p in window_cache]

    boundary_decisions = book_boundaries.refine_spans(spans, window_for)

    pages = source_ocr.read_pages(xml_path, limit=limit)
    edition, stats, walker = page_parser.parse_volume(
        pages, witness=witness, volume=volume, book=book,
        book_spans=spans, header_chapters=header_chapters, with_walker=True)

    chapters = {}
    for osis, entry in edition.books.items():
        for number in entry.chapters:
            if number:
                chapters[(osis, number)] = entry.chapters[number]
    refs, duplicates, out_of_order, gaps = [], [], [], []
    seen_refs = set()
    per_book = {}
    previous_by_book = {}
    for osis, number in sorted(chapters, key=lambda k: (k[0], k[1])):
        chapter = chapters[(osis, number)]
        stat = per_book.setdefault(osis, {"chapters": 0, "resolved": 0,
                                          "unresolved": 0, "verses": 0,
                                          "limit": structure.chapter_limit(osis)})
        stat["chapters"] += 1
        if number > 0:
            stat["resolved"] += 1
        else:
            stat["unresolved"] += 1
        previous = previous_by_book.get(osis)
        if previous is not None and 0 < number < previous:
            out_of_order.append(f"{osis}.{number}")
        if number > 0:
            previous_by_book[osis] = number

        verses = sorted(chapter.verses)
        for verse in verses:
            ref = f"{osis}.{number}.{verse}"
            if ref in seen_refs:
                duplicates.append(ref)
            seen_refs.add(ref)
            refs.append(ref)
        stat["verses"] += len(verses)
        expected = set(range(1, verses[-1] + 1)) if verses else set()
        missing = sorted(expected - set(verses))
        if missing:
            gaps.append({"book": osis, "chapter": number,
                         "missing": missing[:40],
                         "missing_total": len(missing)})

    chapter_numbers = {}
    duplicate_chapters = []
    for osis, number in chapters:
        if number <= 0:
            continue
        key = (osis, number)
        chapter_numbers[key] = chapter_numbers.get(key, 0) + 1
    duplicate_chapters = [f"{o}.{n}" for (o, n), c in chapter_numbers.items()
                          if c > 1]

    issues = []
    for block in edition.review_queue:
        issues.append({
            "kind": block.kind.value,
            "reason": block.review_reason,
            "decision": block.decision,
            "page": block.provenance.page,
            "column": block.provenance.column,
            "zone": block.provenance.zone,
            "bbox": list(block.provenance.bbox or ()),
            "block_id": block.provenance.block_id,
            "raw": (block.raw_text or "")[:120],
        })

    resolutions = walker.resolutions
    by_method = {}
    for item in resolutions:
        by_method[item["method"]] = by_method.get(item["method"], 0) + 1

    boundaries_report = [{
        "from_book": d.from_book, "to_book": d.to_book,
        "confirmation_page": d.confirmation_page,
        "confirmation_evidence": d.confirmation_evidence,
        "first_evidence_page": d.first_evidence_page,
        "first_evidence_block": d.first_evidence_block,
        "effective_boundary_page": d.effective_page,
        "effective_boundary_block": d.effective_block,
        "lookback_distance": d.lookback_distance,
        "evidence": d.evidence, "confidence": d.confidence,
        "review_required": d.review_required,
    } for d in boundary_decisions]

    cand = walker.division_candidates
    by_class = {}
    for item in cand:
        by_class[item["classification"]] = by_class.get(item["classification"], 0) + 1
    rejected_reasons = {}
    for item in cand:
        for reason in item["rejections"]:
            rejected_reasons[reason] = rejected_reasons.get(reason, 0) + 1

    report = {
        "book_boundary_resolution": {
            "lookback_pages": book_boundaries.LOOKBACK_PAGES,
            "spans_before": spans_before,
            "spans_after": [(s.osis, s.first_page) for s in spans],
            "boundaries": boundaries_report,
            "ambiguous": [b for b in boundaries_report if b["review_required"]],
        },
        "division_detection": {
            "candidates": len(cand),
            "by_classification": dict(sorted(by_class.items())),
            "rejection_reasons": dict(sorted(rejected_reasons.items())),
            "rejected_sample": [c for c in cand
                                if c["classification"] == "not_boundary"][:40],
            "review_sample": [c for c in cand if c["review_required"]][:40],
            "all": cand,
        },
        "structure_resolution": {
            "books_detected": [s.osis for s in spans],
            "book_boundaries": len(spans) - 1 if spans else 0,
            "book_spans": [{"book": s.osis, "first_page": s.first_page,
                            "last_page": s.last_page, "evidence": s.evidence}
                           for s in spans],
            "chapters_detected": len(resolutions),
            "by_method": dict(sorted(by_method.items())),
            "chapters_resolved": sum(1 for r in resolutions
                                     if r["resolved_number"] is not None),
            "chapters_unresolved": sum(1 for r in resolutions
                                       if r["resolved_number"] is None),
            "duplicate_chapters": sorted(duplicate_chapters),
            "per_book": per_book,
            "chapters": resolutions,
        },
        "edition_id": edition.edition_id,
        "volume": volume,
        "witness": witness,
        "book": book,
        "elapsed_seconds": round(time.time() - started, 2),
        "metrics": dict(sorted(stats.items())),
        "chapters": len([c for c in chapters if c]),
        "verse_refs": len(refs),
        "duplicate_refs": sorted(set(duplicates)),
        "out_of_order_refs": out_of_order,
        "out_of_order_chapters": out_of_order,
        "verse_number_gaps": gaps[:60],
        "verse_number_gaps_total": len(gaps),
        "review_queue": len(edition.review_queue),
        "review_issues_sample": issues[:200],
        "note": "Detection only. Nothing here is used to correct the text.",
    }
    return edition, report


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--xml", required=True)
    ap.add_argument("--volume", default="3")
    ap.add_argument("--witness", default="ia-lasagradabiblia01unkngoog")
    ap.add_argument("--book", default="Ps")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--out")
    args = ap.parse_args()

    _edition, report = audit(args.xml, volume=args.volume,
                             witness=args.witness, book=args.book,
                             limit=args.limit)
    if args.out:
        os.makedirs(os.path.dirname(args.out), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as handle:
            json.dump(report, handle, ensure_ascii=False, indent=1)
            handle.write("\n")
        print(f"informe en {args.out}")
    bb_r = report["book_boundary_resolution"]
    print(f"  lookback K                 {bb_r['lookback_pages']}")
    for b in bb_r["boundaries"]:
        print(f"    {b['from_book']:5}->{b['to_book']:5} confirm={b['confirmation_page']:3}"
              f" effective={b['effective_boundary_page']:3} K={b['lookback_distance']}"
              f" conf={b['confidence']} review={b['review_required']}")
    print(f"  spans before               {bb_r['spans_before']}")
    print(f"  spans after                {bb_r['spans_after']}")
    dd = report["division_detection"]
    print(f"  division candidates        {dd['candidates']}")
    print(f"  by_classification          {dd['by_classification']}")
    for reason, count in dd["rejection_reasons"].items():
        print(f"    rejected: {reason[:56]:58} {count}")
    sr = report["structure_resolution"]
    print(f"  books_detected             {sr['books_detected']}")
    print(f"  book_boundaries            {sr['book_boundaries']}")
    print(f"  chapters_detected          {sr['chapters_detected']}")
    print(f"  chapters_resolved          {sr['chapters_resolved']}")
    print(f"  chapters_unresolved        {sr['chapters_unresolved']}")
    print(f"  by_method                  {sr['by_method']}")
    print(f"  duplicate_chapters         {len(sr['duplicate_chapters'])}")
    for osis, stat in sr["per_book"].items():
        print(f"    {osis:5} chapters={stat['chapters']:4} resolved={stat['resolved']:4}"
              f" unresolved={stat['unresolved']:4} verses={stat['verses']:5}"
              f" vulg_limit={stat['limit']}")
    for key in ("elapsed_seconds", "verse_refs", "review_queue",
                "verse_number_gaps_total"):
        print(f"  {key:26} {report[key]}")
    print(f"  duplicate_refs             {len(report['duplicate_refs'])}")
    print(f"  out_of_order_refs          {len(report['out_of_order_refs'])}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
