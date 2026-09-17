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
import chapter_claims
import heading_validity
import image_reviews
import layout
import numeral_review
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
    numerals = []
    try:
        payload = image_reviews.load()
        pdf = os.path.join(cache, payload["visual_source"]["filename"])
        source = image_recovery.source_identity(payload)
        try:
            reviews = image_reviews.reviews_for(payload, source_path=pdf)
            numerals = image_reviews.numeral_reviews_for(payload,
                                                         source_path=pdf)
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

    # Segunda referencia: CON las recuperaciones de frontera pero SIN las
    # lecturas de numeral. Es el estado justo antes de esta tanda de
    # revisión, y es lo único contra lo que tiene sentido medir lo que la
    # tanda aporta: restarlo del total mezclaría dos trabajos distintos.
    pre_numeral_edition, _pn_stats, pre_numeral_walker = \
        page_parser.parse_volume(
            source_ocr.read_pages(xml_path, limit=limit), witness=witness,
            volume=volume, book=book, book_spans=spans,
            header_chapters=header_chapters, with_walker=True,
            image_reviews=(image_recovery.by_page(reviews)
                           if reviews else None),
            recovery_source=source if reviews else None)

    pages = source_ocr.read_pages(xml_path, limit=limit)
    edition, stats, walker = page_parser.parse_volume(
        pages, witness=witness, volume=volume, book=book,
        book_spans=spans, header_chapters=header_chapters, with_walker=True,
        image_reviews=image_recovery.by_page(reviews) if reviews else None,
        recovery_source=source if reviews else None,
        numeral_reviews=(image_reviews.numerals_by_block(numerals)
                         if numerals else None))

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

    # `duplicate_chapters` mide sobre `chapters`, que es un diccionario
    # indexado por (libro, número). Por construcción no puede tener dos
    # entradas con la misma clave, así que esta lista sale vacía SIEMPRE
    # -- también cuando seis rótulos distintos reclamaban el mismo
    # capítulo. Se conserva porque hay consumidores, pero no demuestra
    # nada: la medida buena es `competing_chapter_claims`, que se toma
    # sobre los reclamos ANTES de materializarlos (ver chapter_claims.py).
    chapter_numbers = {}
    duplicate_chapters = []
    for osis, number in chapters:
        if number <= 0:
            continue
        key = (osis, number)
        chapter_numbers[key] = chapter_numbers.get(key, 0) + 1
    duplicate_chapters = [f"{o}.{n}" for (o, n), c in chapter_numbers.items()
                          if c > 1]

    ledger = walker.ledger
    claims_report = ledger.report()
    counts_after_competing = claims_report["competing_chapter_claims"]
    claims_report["measured"] = (
        "before materialisation: every heading that claimed a chapter is "
        "still a separate record here, so two headings claiming the same "
        "number are visible. In the chapter map they are not: a dict keyed "
        "by chapter number cannot hold two claimants, which is why the old "
        "duplicate_chapters is empty even when collisions exist.")
    claims_report["old_duplicate_chapters_is_vacuous"] = True

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

    # Qué contenido rescata cada frontera recuperada, y de dónde venía.
    #
    # El «antes» no se deduce restando: sale de la pasada gemela SIN
    # revisiones, que es el mismo código leyendo lo mismo. Para cada
    # recuperación se dice qué bloques están hoy bajo ese capítulo, dónde
    # estaban sin ella y cuántas referencias pasan a estar publicadas.
    def ownership(edition_obj):
        out = {}
        for osis, entry in edition_obj.books.items():
            for number, chapter in entry.chapters.items():
                label = f"{osis}.{number}"
                for verse_number, verse in chapter.verses.items():
                    for blk in verse.blocks:
                        out[blk.provenance.block_id] = (label, verse_number)
                for blk in chapter.paratext:
                    out.setdefault(blk.provenance.block_id, (label, None))
        return out

    before_owner = ownership(before_edition)
    after_owner = ownership(edition)
    recovered_ownership = []
    for record in applications:
        if not record.changed_stream or record.chapter_number is None:
            continue
        label = f"{record.book}.{record.chapter_number}"
        blocks = sorted(b for b, (owner, _v) in after_owner.items()
                        if owner == label)
        was = {}
        for block in blocks:
            previous = before_owner.get(block)
            key = previous[0] if previous else "(not in any chapter)"
            was[key] = was.get(key, 0) + 1
        refs_now = sum(1 for b in blocks if after_owner[b][1] is not None)
        refs_before = sum(
            1 for b in blocks
            if before_owner.get(b) and before_owner[b][1] is not None
            and not before_owner[b][0].split(".")[1].startswith("-"))
        recovered_ownership.append({
            "review_id": record.review_id, "book": record.book,
            "chapter": record.chapter_number, "action": record.action,
            "heading_block": record.target_block or record.resulting_block,
            "anchor_after": record.anchor_after,
            "anchor_before": record.anchor_before,
            "source_blocks_now_in_this_chapter": len(blocks),
            "scan_pages": sorted({int(b[1:5]) for b in blocks}),
            "where_those_blocks_were_without_the_review": dict(sorted(
                was.items())),
            "verse_refs_in_this_chapter": refs_now,
            "verse_refs_already_published_before": refs_before,
        })

    review_batches = {}
    for entry in (payload.get("reviews", []) if payload else []):
        name = entry.get("batch") or "batch-110"
        stat = review_batches.setdefault(
            name, {"reviews": 0, "review_ids": [], "by_outcome": {}})
        stat["reviews"] += 1
        stat["review_ids"].append(entry["id"])
        outcome = entry.get("outcome")
        stat["by_outcome"][outcome] = stat["by_outcome"].get(outcome, 0) + 1

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
        "batches": {name: review_batches[name] for name in sorted(review_batches)},
        "recovered_chapter_ownership": recovered_ownership,
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

    # --- lo que aportó la revisión de numerales -----------------------
    pre = pre_numeral_walker.ledger
    pre_counts = pre.counts()
    reviewed_blocks = {r.target_block for r in numerals}
    direct, cascade = [], []
    for claim in ledger.accepted():
        if claim.proposal_method == "numeral_review":
            direct.append(claim)
            continue
        was = next((c for c in pre.claims
                    if c.block_id == claim.block_id), None)
        if was is not None and was.disposition != chapter_claims.ACCEPTED:
            cascade.append(claim)

    by_outcome = {}
    for review in numerals:
        by_outcome[review.outcome] = by_outcome.get(review.outcome, 0) + 1

    # Las tandas se acumulan: lo revisado antes sigue a la vista, y cada
    # tanda dice qué aportó ella. Sin esto no se puede saber si un
    # capítulo aceptado viene de mirar una plana la semana pasada o hoy.
    raw_numerals = (payload.get("numeral_reviews", []) if payload else [])
    batches = {}
    accepted_by_block = {c.block_id: c for c in ledger.accepted()}
    for entry in raw_numerals:
        name = entry.get("batch") or "batch-112"
        stat = batches.setdefault(name, {
            "reviewed": 0, "resolved": 0, "unreadable": 0,
            "still_ambiguous": 0, "rejected_false_claim": 0,
            "by_outcome": {}, "by_book": {}, "accepted_from_this_batch": 0,
            "by_queue_disposition": {}, "by_pattern": {},
            "cascade_resolutions_anchored_here": 0,
            "longest_cascade_chain": 0, "review_ids": []})
        stat["reviewed"] += 1
        stat["review_ids"].append(entry["id"])
        # De qué cola salió el reclamo y qué aspecto tenía el fallo del
        # reconocimiento. Sólo se cuenta: ninguna de las dos cosas decide
        # el número, que sale del numeral impreso.
        queued = entry.get("queue_disposition") or "unrecorded"
        stat["by_queue_disposition"][queued] = \
            stat["by_queue_disposition"].get(queued, 0) + 1
        for pattern in entry.get("patterns") or ():
            stat["by_pattern"][pattern] = stat["by_pattern"].get(pattern, 0) + 1
        outcome = entry.get("outcome")
        stat["by_outcome"][outcome] = stat["by_outcome"].get(outcome, 0) + 1
        stat["by_book"][entry["book"]] = stat["by_book"].get(entry["book"], 0) + 1
        if outcome in image_reviews.NUMERAL_RESOLVING and \
                entry.get("recovered_chapter") is not None:
            stat["resolved"] += 1
        if outcome == image_reviews.UNREADABLE:
            stat["unreadable"] += 1
        if outcome == image_reviews.STILL_AMBIGUOUS:
            stat["still_ambiguous"] += 1
        if outcome == image_reviews.FALSE_CLAIM:
            stat["rejected_false_claim"] += 1
        if entry["target_block"] in accepted_by_block:
            stat["accepted_from_this_batch"] += 1

    # Cada cascada se atribuye a la lectura facsimilar de la que cuelga,
    # siguiendo su cadena de anclas ACEPTADAS hasta la primera revisión
    # directa. Una cascada no es otra lectura de la imagen: aquí sólo se
    # dice de qué lectura depende y a cuántos saltos está.
    batch_of_review = {e["id"]: (e.get("batch") or "batch-112")
                       for e in raw_numerals}
    direct_ids = {c.claim_id for c in direct}
    cascade_ids = {c.claim_id for c in cascade}
    chains = []
    for claim in cascade:
        hops, root = 0, claim
        while root.claim_id in cascade_ids and root.anchor_claim:
            nxt = ledger.by_id(root.anchor_claim)
            if nxt is None:
                break
            root, hops = nxt, hops + 1
        root_batch = (batch_of_review.get(root.proposal_evidence.get("review_id"))
                      if root.claim_id in direct_ids else None)
        chains.append({"block_id": claim.block_id, "book": claim.book,
                       "accepted_number": claim.accepted_number,
                       "accepted_round": claim.accepted_round,
                       "anchor_claim": claim.anchor_claim,
                       "root_claim": root.claim_id,
                       "root_review_id": root.proposal_evidence.get("review_id")
                       if root.claim_id in direct_ids else None,
                       "root_batch": root_batch, "hops": hops})
        if root_batch in batches:
            stat = batches[root_batch]
            stat["cascade_resolutions_anchored_here"] += 1
            stat["longest_cascade_chain"] = max(stat["longest_cascade_chain"],
                                                hops)

    # La cola de numerales pendientes, calculada AQUÍ y publicada junto a
    # los recuentos del ledger. Son dos cosas distintas y las dos hacen
    # falta, así que van con nombre y en el mismo sitio:
    #
    #   invalid_numeral_total            estado del reclamo en el ledger
    #   invalid_numeral_reviewed         ya se miró la plana (p.ej. no era
    #                                    un rótulo); no vuelve a la cola
    #   invalid_numeral_pending_review   lo que queda por mirar
    #
    # Publicarlas por separado es lo que impide volver a informar de una
    # y llamarla la otra.
    reviewed_blocks = {r.target_block for r in numerals}
    numeral_queue = numeral_review.build(
        claims_report["claims"],
        permissive_groups=claims_report["permissive_value_collision_detail"],
        competing_groups=claims_report["competing_claim_groups_detail"],
        reviewed_blocks=reviewed_blocks)
    queue_summary = numeral_review.summary(numeral_queue)
    pending_invalid = [e for e in numeral_queue
                       if e.disposition == numeral_review.INVALID]
    invalid_pending_by_book = {}
    for entry in pending_invalid:
        invalid_pending_by_book[entry.book] = \
            invalid_pending_by_book.get(entry.book, 0) + 1
    dismissed = [c for c in ledger.claims
                 if c.block_id in reviewed_blocks
                 and c.disposition == chapter_claims.INVALID_NUMERAL]
    queue_by_disposition_book, queue_by_reason = {}, {}
    claim_by_block = {c.block_id: c for c in ledger.claims}
    for entry in numeral_queue:
        books = queue_by_disposition_book.setdefault(entry.disposition, {})
        books[entry.book] = books.get(entry.book, 0) + 1
        claim = claim_by_block.get(entry.block_id)
        why = ((claim.proposal_evidence.get("why") if claim else None)
               or entry.reason or "unrecorded")
        queue_by_reason[why] = queue_by_reason.get(why, 0) + 1

    numeral_report = {
        "about": ("headings the recognition did produce, whose numeral it "
                  "could not identify, read one by one in the facsimile. "
                  "Each entry points at an OCR block and keeps the raw OCR "
                  "beside what the page prints; the OCR is never edited."),
        "visual_source": (payload["visual_source"] if payload else None),
        "hash_guard": guard,
        "schema_problems": (image_reviews.validate_numerals(payload,
                                                            page_count=652)
                            if payload else []),
        "reviews_attempted": len(numerals),
        "reviews_resolved": sum(1 for r in numerals if r.resolves),
        "reviews_unreadable": sum(
            1 for r in numerals if r.outcome == image_reviews.UNREADABLE),
        "reviews_still_ambiguous": sum(
            1 for r in numerals if r.outcome == image_reviews.STILL_AMBIGUOUS),
        "reviews_by_outcome": dict(sorted(by_outcome.items())),
        "batches": {name: batches[name] for name in sorted(batches)},
        "reviews_applied_to_a_claim": len(
            [c for c in ledger.claims if c.block_id in reviewed_blocks
             and c.proposal_method == "numeral_review"]),
        "competing_groups_before": pre_counts["competing_claim_groups"],
        "competing_groups_after": counts_after_competing,
        "invalid_before": pre_counts["invalid_numeral"],
        "invalid_after": claims_report["invalid_numeral"],
        "invalid_numeral_total": claims_report["invalid_numeral"],
        "invalid_numeral_reviewed": len(dismissed),
        "invalid_numeral_reviewed_blocks": sorted(c.block_id for c in dismissed),
        "invalid_numeral_pending_review": len(pending_invalid),
        "invalid_numeral_pending_by_book": dict(sorted(
            invalid_pending_by_book.items())),
        "review_queue": queue_summary,
        "review_queue_next": [e.as_dict() for e in numeral_queue[:40]],
        # Qué clase de trabajo queda, medido y no supuesto: la primera
        # disposición de la cola ordenada y el desglose por libro y por
        # motivo.
        "review_queue_first_category": (numeral_queue[0].disposition
                                        if numeral_queue else None),
        "review_queue_by_disposition_and_book": queue_by_disposition_book,
        "review_queue_by_reason": queue_by_reason,
        "ambiguous_before": pre_counts["ambiguous_numeral"],
        "ambiguous_after": claims_report["ambiguous_numeral"],
        "accepted_before": pre_counts["accepted"],
        "accepted_after": claims_report["accepted"],
        "unresolved_before": pre_counts["unresolved_claims"],
        "unresolved_after": claims_report["unresolved_claims"],
        "direct_image_recoveries": len(direct),
        "cascade_resolutions": len(cascade),
        "direct": [{"review_id": c.proposal_evidence.get("review_id"),
                    "book": c.book, "scan_page": c.scan_page,
                    "block_id": c.block_id,
                    "raw_ocr_heading": c.raw_heading,
                    "raw_ocr_numeral": c.numeral.get("token"),
                    "observed_printed_numeral":
                        c.proposal_evidence.get("observed"),
                    "accepted_number": c.accepted_number,
                    "accepted_round": c.accepted_round,
                    "provenance": c.provenance} for c in direct],
        "cascade": [{"book": c.book, "scan_page": c.scan_page,
                     "block_id": c.block_id,
                     "raw_ocr_heading": c.raw_heading,
                     "accepted_number": c.accepted_number,
                     "accepted_round": c.accepted_round,
                     "sequence_anchor": c.anchor_number,
                     "anchor_claim": c.anchor_claim,
                     "source": c.source} for c in cascade],
        "cascade_chains": chains,
        "longest_cascade_chain": max((c["hops"] for c in chains), default=0),
        "per_book": {},
    }
    for osis in sorted(set(list(pre.per_book()) + list(ledger.per_book()))):
        was = pre.per_book().get(osis, {})
        now = ledger.per_book().get(osis, {})
        numeral_report["per_book"][osis] = {
            "accepted_before": was.get("accepted", 0),
            "accepted_after": now.get("accepted", 0),
            "invalid_before": was.get("invalid_numeral", 0),
            "invalid_after": now.get("invalid_numeral", 0),
            "unresolved_before": was.get("unresolved", 0),
            "unresolved_after": now.get("unresolved", 0),
        }

    # --- ¿era un rótulo? ----------------------------------------------
    #
    # Casar la palabra no es ser un rótulo. Esta sección informa de lo
    # que la validación estructural dijo de cada reclamo: qué rechazó,
    # qué dejó en duda y qué reclamos ACEPTADOS tienen la forma más
    # floja, que es por dónde habría que mirar si apareciera otro falso.
    heading_rejected = [c for c in ledger.claims
                        if c.disposition == chapter_claims.REJECTED_FALSE_HEADING]
    evaluated = [c for c in ledger.claims if c.heading]
    accepted_claims = ledger.accepted()
    reviewed_by_block = {r.target_block: r for r in numerals}

    def heading_row(claim):
        evidence = claim.heading or {}
        review = reviewed_by_block.get(claim.block_id)
        return {
            "book": claim.book, "scan_page": claim.scan_page,
            "pdf_page": claim.scan_page + 1, "block_id": claim.block_id,
            "bbox": list(claim.bbox) if claim.bbox else None,
            "column": claim.column, "zone": claim.zone,
            "raw_heading": claim.raw_heading,
            "raw_numeral": claim.numeral.get("token"),
            "numeral_status": claim.numeral.get("status"),
            # Lo que la política de numeración HABRÍA propuesto para esta
            # línea. Para un rótulo falso es la medida del daño evitado.
            "number_the_policy_would_have_given": claim.proposed_number,
            "proposal_method": claim.proposal_method,
            "disposition": claim.disposition,
            "accepted_number": claim.accepted_number,
            "heading_evidence": evidence,
            "image_review": (None if review is None else
                             {"review_id": review.id, "outcome": review.outcome,
                              "observed_printed_text":
                                  review.observed_printed_text}),
        }

    # Sospecha entre los ACEPTADOS: no decide nada, ordena la mirada. Se
    # ordena por la evidencia de forma más floja primero.
    suspicious = sorted(
        (c for c in accepted_claims
         if c.heading and (c.heading.get("rejections")
                           or c.heading.get("score", 1.0) < 0.75)),
        key=lambda c: (c.heading.get("score", 1.0), c.book, c.scan_page))
    heading_report = {
        "about": ("a line that carries the division word is not yet a "
                  "heading. The page's composition decides first "
                  "(divisions.judge), the shape of the line second, and a "
                  "facsimile reading of the same block overrides both. "
                  "Neither the canon nor the sequence takes part."),
        "signal_weights": heading_validity._WEIGHTS,
        "thresholds": {"heading_at": heading_validity.HEADING_AT,
                       "content_words": heading_validity.CONTENT_WORDS,
                       "upper_ratio": heading_validity.UPPER_RATIO,
                       "lower_ratio": heading_validity.LOWER_RATIO,
                       "prose_words": heading_validity.PROSE_WORDS},
        "total_claims_checked": len(ledger.claims),
        "claims_evaluated": len(evaluated),
        "accepted_checked": len(accepted_claims),
        "false_heading_count": len(heading_rejected),
        "uncertain_count": sum(1 for c in ledger.claims
                               if c.heading_review_required),
        "suspicious_count": len(suspicious),
        "by_source": {src: sum(1 for c in evaluated
                               if (c.heading or {}).get("source") == src)
                      for src in (heading_validity.FROM_STRUCTURE,
                                  heading_validity.FROM_IMAGE_REVIEW)},
        "false_headings": [heading_row(c) for c in heading_rejected],
        "uncertain": [heading_row(c) for c in ledger.claims
                      if c.heading_review_required],
        "suspicious_accepted": [heading_row(c) for c in suspicious[:40]],
        "note": ("a rejected false heading leaves the numeral review "
                 "queues: there is no numeral to recover where there is no "
                 "heading. It stays here, with its raw OCR and its "
                 "geometry, and it can never anchor the sequence."),
    }

    report = {
        "heading_claim_validation": heading_report,
        "numeral_image_review": numeral_report,
        "chapter_claims": claims_report,
        "competing_chapter_claims": len(ledger.collisions()),
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
        # `verse_refs` cuenta también los huecos de trabajo negativos, que
        # son texto EN REVISIÓN y no texto publicado. Separarlos es lo que
        # permite ver si un cambio movió versículos o si sólo cambió de
        # sitio lo que todavía no se ha identificado.
        "materialized_verse_refs": len(
            [r for r in refs if not r.split(".")[1].startswith("-")]),
        "verse_refs_in_review_slots": len(
            [r for r in refs if r.split(".")[1].startswith("-")]),
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
    hv = report.get("heading_claim_validation", {})
    if hv:
        print(f"  heading validation         claims={hv['total_claims_checked']}"
              f" evaluated={hv['claims_evaluated']}"
              f" accepted={hv['accepted_checked']}"
              f" false_headings={hv['false_heading_count']}"
              f" uncertain={hv['uncertain_count']}"
              f" suspicious_accepted={hv['suspicious_count']}")
        print(f"    by evidence source       {hv['by_source']}")
        for row in hv["false_headings"]:
            print(f"    FALSE HEADING {row['book']:5} p{row['scan_page']:<4}"
                  f" {row['block_id']:14} {row['raw_heading'][:38]!r}")
            print(f"      the policy would have given it"
                  f" {row['number_the_policy_would_have_given']};"
                  f" rejected: {'; '.join(row['heading_evidence'].get('rejections', []))[:90]}")
        for row in hv["suspicious_accepted"][:12]:
            print(f"    suspicious   {row['book']:5} p{row['scan_page']:<4}"
                  f" {row['block_id']:14} score="
                  f"{row['heading_evidence'].get('score')} "
                  f"{row['raw_heading'][:34]!r} -> {row['accepted_number']}")

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
        for name, stat in cr.get("batches", {}).items():
            print(f"    {name:12} reviews={stat['reviews']:3}"
                  f" {stat['by_outcome']}")
        for row in cr.get("recovered_chapter_ownership", []):
            print(f"    {row['review_id']:16} {row['book']} {row['chapter']:<4}"
                  f" blocks={row['source_blocks_now_in_this_chapter']:4}"
                  f" pages={row['scan_pages']}"
                  f" refs={row['verse_refs_in_this_chapter']:3}"
                  f" (published before: {row['verse_refs_already_published_before']})")
            print(f"      without the review those blocks sat in:"
                  f" {row['where_those_blocks_were_without_the_review']}")
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
    nr = report.get("numeral_image_review", {})
    if nr:
        print(f"  numeral image review       attempted={nr['reviews_attempted']}"
              f" resolved={nr['reviews_resolved']}"
              f" unreadable={nr['reviews_unreadable']}"
              f" still_ambiguous={nr['reviews_still_ambiguous']}")
        print(f"    by outcome               {nr['reviews_by_outcome']}")
        print(f"    schema problems          {nr['schema_problems'] or 'none'}")
        print(f"    direct recoveries        {nr['direct_image_recoveries']}"
              f"   cascade resolutions {nr['cascade_resolutions']}"
              f"   (a cascade is NOT a second image recovery)")
        print(f"    accepted   {nr['accepted_before']:4} -> {nr['accepted_after']:4}"
              f"     unresolved {nr['unresolved_before']:4} -> {nr['unresolved_after']:4}")
        print(f"    invalid    {nr['invalid_before']:4} -> {nr['invalid_after']:4}"
              f"     ambiguous  {nr['ambiguous_before']:4} -> {nr['ambiguous_after']:4}")
        print(f"    competing groups {nr['competing_groups_before']} -> "
              f"{nr['competing_groups_after']}")
        print(f"    invalid_numeral: total {nr['invalid_numeral_total']}"
              f" = reviewed {nr['invalid_numeral_reviewed']}"
              f" + pending review {nr['invalid_numeral_pending_review']}")
        print(f"    pending invalid by book  "
              f"{nr['invalid_numeral_pending_by_book']}")
        print(f"    review queue             {nr['review_queue']['total']}"
              f" {nr['review_queue']['by_disposition']}")
        print(f"    queue first category     {nr['review_queue_first_category']}"
              f"   longest cascade chain {nr['longest_cascade_chain']}")
        for name, stat in nr["batches"].items():
            print(f"    {name:10} reviewed={stat['reviewed']:3}"
                  f" resolved={stat['resolved']:3}"
                  f" accepted={stat['accepted_from_this_batch']:3}"
                  f" cascades={stat['cascade_resolutions_anchored_here']:3}"
                  f" longest={stat['longest_cascade_chain']}"
                  f" queues={stat['by_queue_disposition']}")
        print("    book   accepted      invalid       unresolved")
        for osis, stat in sorted(nr["per_book"].items()):
            print(f"      {osis:5} {stat['accepted_before']:3} -> {stat['accepted_after']:3}"
                  f"    {stat['invalid_before']:3} -> {stat['invalid_after']:3}"
                  f"    {stat['unresolved_before']:3} -> {stat['unresolved_after']:3}")

    cc = report.get("chapter_claims", {})
    if cc:
        print(f"  chapter claims             total={cc['total_claims']}"
              f" accepted={cc['accepted']}"
              f" unresolved={cc['unresolved_claims']}")
        print(f"    invalid_numeral          {cc['invalid_numeral']}")
        print(f"    ambiguous_numeral        {cc['ambiguous_numeral']}")
        print(f"    uncorroborated           {cc['uncorroborated_correction']}")
        print(f"    same_physical            {cc['same_physical_claim']}")
        print(f"    competing                {cc['competing_claim']}"
              f" in {cc['competing_claim_groups']} groups")
        print(f"    by image review          {cc['recovered_by_image_review']}")
        for group in cc["collision_groups"]:
            print(f"    COLLISION {group['book']} {group['claimed_number']}:"
                  f" {group['claimant_count']} claimants at"
                  f" {group['distinct_physical_places']} places"
                  f" -> {group['disposition']}")
            for claimant in group["claimants"]:
                print(f"        p{claimant['scan_page']:<4}"
                      f" {claimant['block_id']:14} {claimant['source']:12}"
                      f" {claimant['raw_heading'][:34]!r}")
        print("  book   raw  valid invalid ambig uncorr samephys compet accepted unres")
        for osis, stat in sorted(cc["per_book"].items()):
            print(f"    {osis:5} {stat['raw_claims']:4} {stat['raw_numeral_valid']:6}"
                  f" {stat['raw_numeral_invalid']:7} {stat['ambiguous']:5}"
                  f" {stat['uncorroborated']:6} {stat['same_physical_duplicates']:8}"
                  f" {stat['competing']:6} {stat['accepted']:8}"
                  f" {stat['unresolved']:5}")
        print(f"  resolution rounds          {cc['rounds']}"
              f"   (only accepted chapters anchor the sequence)")
        print(f"  competing_chapter_claims   {report['competing_chapter_claims']}"
              f"  <- REAL conflicts, measured before materialisation")
        print(f"  old duplicate_chapters     {report['structure_resolution']['duplicate_chapters']}"
              f"  <- vacuous: measured after the dict collapsed them")
        print(f"  permissive_value_collision_groups"
              f" {cc['permissive_value_collision_groups']:6}"
              f"  <- DIAGNOSTIC ONLY, not competing claims:")
        print(f"      how many chapter slots two distinct headings would have")
        print(f"      landed on under the old permissive numeral reading.")
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
