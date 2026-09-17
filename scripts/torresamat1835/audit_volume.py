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
import image_reviews
import layout
import recovery as image_recovery
import recovery_candidates
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

    # Candidatos pendientes: se detectan durante el barrido y esperan
    # hacia adelante hasta que tres cabeceras coherentes confirmen el
    # libro. Sin ventana fija: entre el comienzo de Isaías y su
    # confirmación hay 42 planas de cabeceras ilegibles.
    def pages_with_layout():
        for page in source_ocr.read_pages(xml_path, limit=limit):
            yield page, layout.split_columns(page, gutter_hint=1700)

    boundary_decisions, tracker = book_boundaries.resolve_with_candidates(
        spans, pages_with_layout)

    # Revisiones del facsímil: se cargan con la guarda de hash puesta. Si
    # el artefacto visual no es el que se miró, `reviews` queda vacío y no
    # se aplica ninguna -- y el informe dice por qué, no lo silencia.
    cache = os.path.dirname(xml_path)
    payload, source, reviews, guard = None, None, [], "ok"
    try:
        payload = image_reviews.load()
        pdf = os.path.join(cache, payload["visual_source"]["filename"])
        source = image_recovery.source_identity(payload)
        try:
            reviews = image_reviews.reviews_for(payload, source_path=pdf)
        except image_reviews.ReviewError as exc:
            guard = str(exc)
    except FileNotFoundError:
        guard = "no review metadata"

    # Se parsea dos veces con exactamente el mismo código y sólo las
    # revisiones cambiadas. Es la única forma honesta de poder decir qué
    # cambió la imagen y qué ya estaba: deducir el «antes» restando
    # esconde las resoluciones que un capítulo recuperado vuelve a hacer
    # posibles al devolverle a la secuencia su punto de apoyo.
    before_edition, _before_stats, before_walker = page_parser.parse_volume(
        source_ocr.read_pages(xml_path, limit=limit), witness=witness,
        volume=volume, book=book, book_spans=spans,
        header_chapters=header_chapters, with_walker=True)

    pages = source_ocr.read_pages(xml_path, limit=limit)
    edition, stats, walker = page_parser.parse_volume(
        pages, witness=witness, volume=volume, book=book,
        book_spans=spans, header_chapters=header_chapters, with_walker=True,
        image_reviews=image_recovery.by_page(reviews) if reviews else None,
        recovery_source=source if reviews else None)

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

    # Revisiones facsimilares: qué demostró la imagen y qué hizo con el
    # flujo estructural. El «antes» sale de la pasada gemela sin
    # revisiones, no de una resta.
    def chapter_view(ed):
        return {osis: sorted(n for n in bk.chapters if n > 0)
                for osis, bk in ed.books.items()}

    before_view, after_view = chapter_view(before_edition), chapter_view(edition)
    per_book_recovery = {}
    for osis in sorted(set(before_view) | set(after_view)):
        was, now = set(before_view.get(osis, [])), set(after_view.get(osis, []))
        per_book_recovery[osis] = {
            "before": len(was), "after": len(now),
            "gained": sorted(now - was), "lost": sorted(was - now),
            "vulg_audit_limit": structure.chapter_limit(osis),
        }

    applications = walker.recoveries
    def count(action):
        return sum(1 for a in applications if a.action == action)

    applied = [a for a in applications if a.changed_stream]
    problems = image_reviews.validate(payload, page_count=652) if payload else []
    pdf_path = (os.path.join(cache, payload["visual_source"]["filename"])
                if payload else None)
    mapping = None
    if pdf_path and os.path.isfile(pdf_path):
        mapping = image_reviews.verify_page_mapping(xml_path, pdf_path)

    # Un número recuperado que otro rótulo también reclama sería dos
    # capítulos en el mismo hueco. La capa de aplicación lo rechaza al
    # aplicar, pero sólo ve lo ya escrito; esta comprobación mira el tomo
    # entero, incluidos los rótulos posteriores.
    claims = {}
    for item in resolutions:
        if item["resolved_number"] is not None:
            claims.setdefault((item["book"], item["resolved_number"]),
                              []).append({"scan_page": item["scan_page"],
                                          "method": item["method"],
                                          "block_id": item["block_id"]})
    recovered_keys = {(a.book, a.chapter_number) for a in applied
                      if a.chapter_number is not None}
    collisions = [{"book": b, "chapter": n, "claimants": claims[(b, n)]}
                  for (b, n) in sorted(recovered_keys)
                  if len(claims.get((b, n), [])) > 1]

    recovery_report = {
        "available": payload is not None,
        "visual_source": payload["visual_source"] if payload else None,
        "hash_guard": guard,
        "page_mapping": mapping,
        "schema_problems": problems,
        "reviews_loaded": len(payload.get("reviews", [])) if payload else 0,
        "reviews_valid": len(reviews),
        "reviews_applied": len(applied),
        "reviews_rejected": count(image_recovery.REJECTED),
        "reviews_redundant": count(image_recovery.REDUNDANT),
        "reviews_failed_anchor_validation": count(image_recovery.FAILED),
        "reviews_conflicting_number": count(image_recovery.CONFLICT),
        "reviews_colliding_with_existing_chapter": count(
            image_recovery.COLLISION),
        "recovered_boundaries": sum(1 for a in applied
                                    if a.action == image_recovery.INSERTED),
        "recovered_numbers": sum(1 for a in applied
                                 if a.chapter_number is not None),
        "recovered_unknown_numbers": sum(1 for a in applied
                                         if a.chapter_number is None),
        "recovered_number_collisions": collisions,
        "raw_ocr_blocks": stats.get("ocr_blocks"),
        "per_book": per_book_recovery,
        "applications": [{
            "review_id": a.review_id, "book": a.book, "scan_page": a.scan_page,
            "action": a.action, "reason": a.reason,
            "anchor_after": a.anchor_after, "anchor_before": a.anchor_before,
            "target_block": a.target_block,
            "resulting_block": a.resulting_block,
            "chapter_number": a.chapter_number,
            "provenance": a.provenance,
        } for a in applications],
        "recovered_events": [{
            "book": item["book"], "scan_page": item["scan_page"],
            "block_id": item["block_id"],
            "chapter_number": item["resolved_number"],
            "method": item["method"],
            "review_required": item["review_required"],
            "observed": item["source_heading"],
            "provenance": item["evidence"],
        } for item in resolutions if item.get("recovered")],
    }

    # Cola de revisión visual, ordenada por lo que más devuelve. No crea
    # estructura: sólo dice dónde mirar.
    geometry = {}
    for page, placed in pages_with_layout():
        geometry[page.scan_page] = recovery_candidates.page_geometry(page, placed)
    queue = recovery_candidates.rank(readings=readings, resolutions=resolutions,
                                     spans=spans, geometry=geometry)
    candidate_report = {
        "model": "two families: boundary certain with the number lost, and "
                 "no heading at all. Ranking only -- nothing here confirms "
                 "anything or supplies a number.",
        "weights": recovery_candidates.WEIGHTS,
        "total": len(queue),
        "by_family": {family: sum(1 for c in queue if c.family == family)
                      for family in (recovery_candidates.UNRESOLVED_NUMERAL,
                                     recovery_candidates.MISSING_HEADING)},
        "by_book": {},
        "top": [{"family": c.family, "book": c.book, "scan_page": c.scan_page,
                 "pdf_page": c.scan_page + 1, "score": c.score,
                 "target_block": c.target_block,
                 "signals": {k: str(v) for k, v in c.signals.items()}}
                for c in queue[:60]],
    }
    for c in queue:
        key = f"{c.book}/{c.family}"
        candidate_report["by_book"][key] = \
            candidate_report["by_book"].get(key, 0) + 1

    report = {
        "chapter_image_recovery": recovery_report,
        "image_review_queue": candidate_report,
        "book_boundary_resolution": {
            "model": "pending candidate held forward until confirmation",
            "cluster_gap_pages": book_boundaries.CLUSTER_GAP_PAGES,
            "candidates": [{
                "to_book": c.to_book, "from_book": c.from_book,
                "page": c.scan_page, "block_id": c.block_id,
                "bbox": list(c.bbox), "score": c.score,
                "raw": c.raw_text, "evidence": c.evidence,
                "disposition": c.disposition,
                "rejection_reason": c.rejection_reason,
            } for c in tracker.candidates],
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
    cr = report.get("chapter_image_recovery", {})
    if cr.get("available"):
        print(f"  image reviews loaded       {cr['reviews_loaded']}"
              f" valid={cr['reviews_valid']} applied={cr['reviews_applied']}"
              f" rejected={cr['reviews_rejected']}"
              f" redundant={cr['reviews_redundant']}"
              f" failed_anchors={cr['reviews_failed_anchor_validation']}"
              f" conflicting={cr['reviews_conflicting_number']}"
              f" colliding={cr['reviews_colliding_with_existing_chapter']}")
        print(f"  recovered                  boundaries={cr['recovered_boundaries']}"
              f" numbers={cr['recovered_numbers']}"
              f" unknown_numbers={cr['recovered_unknown_numbers']}")
        print(f"  hash guard                 {cr['hash_guard'][:60]}")
        print(f"  schema problems            {cr['schema_problems'] or 'none'}")
        mapping = cr.get("page_mapping") or {}
        print(f"  page mapping               {mapping.get('relation')}"
              f" invariant={mapping.get('invariant')}"
              f" leaves={mapping.get('declared_leaves')}"
              f" pdf_pages={mapping.get('pdf_pages')}"
              f" problems={mapping.get('problems')}")
        print(f"  recovered collisions       {cr['recovered_number_collisions'] or 'none'}")
        for a in cr["applications"]:
            print(f"    {a['review_id']:20} {a['action']:34} n={a['chapter_number']}")
            print(f"      {a['reason'][:100]}")
        print("  book   before  after  gained")
        for osis, stat in cr["per_book"].items():
            print(f"    {osis:5} {stat['before']:6} {stat['after']:6}"
                  f"  {stat['gained']} lost={stat['lost']}"
                  f" vulg={stat['vulg_audit_limit']}")
    q = report.get("image_review_queue", {})
    if q:
        print(f"  review queue               {q['total']} {q['by_family']}")
    bb_r = report["book_boundary_resolution"]
    print(f"  model                      {bb_r['model']}")
    print(f"  candidates                 {len(bb_r['candidates'])}"
          f" confirmed={sum(1 for c in bb_r['candidates'] if c['disposition']=='confirmed')}"
          f" rejected={sum(1 for c in bb_r['candidates'] if c['disposition']=='rejected')}"
          f" pending={sum(1 for c in bb_r['candidates'] if c['disposition']=='pending')}")
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
