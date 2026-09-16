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

import page_parser
import source_ocr
from model import BlockKind


def audit(xml_path, *, volume, witness, book="Ps", limit=None):
    started = time.time()
    pages = source_ocr.read_pages(xml_path, limit=limit)
    edition, stats = page_parser.parse_volume(
        pages, witness=witness, volume=volume, book=book)

    chapters = edition.books.get(book).chapters if book in edition.books else {}
    refs, duplicates, out_of_order, gaps = [], [], [], []
    previous = None
    for number in sorted(chapters):
        if not number:
            continue
        if previous is not None and number < previous:
            out_of_order.append(number)
        previous = number
        verses = sorted(chapters[number].verses)
        for verse in verses:
            ref = f"{book}.{number}.{verse}"
            if ref in refs:
                duplicates.append(ref)
            refs.append(ref)
        expected = set(range(1, verses[-1] + 1)) if verses else set()
        missing = sorted(expected - set(verses))
        if missing:
            gaps.append({"chapter": number, "missing": missing[:40],
                         "missing_total": len(missing)})

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

    report = {
        "edition_id": edition.edition_id,
        "volume": volume,
        "witness": witness,
        "book": book,
        "elapsed_seconds": round(time.time() - started, 2),
        "metrics": dict(sorted(stats.items())),
        "chapters": len([c for c in chapters if c]),
        "verse_refs": len(refs),
        "duplicate_refs": sorted(set(duplicates)),
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
    for key in ("elapsed_seconds", "chapters", "verse_refs", "review_queue",
                "verse_number_gaps_total"):
        print(f"  {key:26} {report[key]}")
    for key, value in report["metrics"].items():
        print(f"  {key:26} {value}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
