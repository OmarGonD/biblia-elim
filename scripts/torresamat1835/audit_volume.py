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
import collections
import json
import os
import time

import book_boundaries
import chapter_claims
import corrupted_markers
import heading_validity
import image_reviews
import layout
import numeral_review
import parser as classifier
import recovery as image_recovery
import recovery_candidates
import page_parser
import severe_headings
import source_ocr
import structure
import verse_gaps
import written_ordinals

#: Cómo se llama cada forma de puntuación exterior en el informe. El
#: glifo suelto no dice nada a quien lee la tabla; el nombre sí.
_PUNCTUATION_FORMS = {
    ".": "leading_dot", ",": "leading_comma", ";": "leading_semicolon",
    ":": "leading_colon", "(": "parentheses", "[": "brackets",
    "'": "leading_apostrophe", '"': "leading_quote", "«": "leading_guillemet",
    "»": "trailing_guillemet", "·": "leading_middle_dot",
    "•": "leading_bullet", "*": "leading_asterisk", "-": "leading_dash",
    "—": "leading_em_dash", "_": "leading_underscore", "|": "leading_bar",
    "^": "leading_caret", "¿": "leading_question", "?": "trailing_question",
    "¡": "leading_bang", "!": "trailing_bang", "{": "braces",
    "~": "leading_tilde",
}

#: La raíz del repositorio, para la metadata versionada.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))

#: Los capítulos que la tanda 124 revisó a mano: los dos casos
#: obligatorios y los que la 123 acabó de separar. Viven aquí --en la
#: entrada del informe-- y no en la lógica: el parser no sabe nada de
#: ellos.
_VERSE_BANK = frozenset({
    ("Ps", 1), ("Wis", 1), ("Ps", 14), ("Ps", 15), ("Ps", 16), ("Ps", 25),
    ("Ps", 129), ("Sir", 1), ("Sir", 2), ("Sir", 47), ("Sir", 48),
})
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
    # Las fronteras de versículo miradas en el facsímil. Son un
    # diagnóstico: ninguna crea, mueve ni renumera nada, y por eso se
    # cargan aparte de las revisiones de capítulo.
    verse_payload = None
    verse_path = os.path.join(ROOT, "data", "torresamat1835",
                              "verse_boundary_reviews.json")
    if os.path.isfile(verse_path):
        with open(verse_path, encoding="utf-8") as handle:
            verse_payload = json.load(handle)
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
            "found_by": a.found_by,
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

    # Rótulos que el reconocimiento dejó con la PALABRA rota. Es otra
    # cola y no sustituye a `missing_heading`: aquélla busca planas donde
    # falta el renglón, y ésta renglones que están y no se leen como lo
    # que son. Genera preguntas, no capítulos.
    represented = {}
    for entry in (payload.get("reviews", []) if payload else []):
        if entry.get("heading_block"):
            represented[entry["heading_block"]] = entry["id"]
    for entry in (payload.get("numeral_reviews", []) if payload else []):
        represented[entry["target_block"]] = entry["id"]
    marker_started = time.time()
    marker_candidates, marker_rejections = corrupted_markers.scan(
        pages_with_layout(), book_at=lambda page: structure.book_at(spans, page),
        header_chapters=header_chapters, represented=represented)
    marker_seconds = round(time.time() - marker_started, 2)
    accepted_by_block = {c.block_id: c for c in ledger.accepted()}
    marker_rows = []
    for candidate in marker_candidates:
        row = candidate.as_dict()
        review_id = candidate.already_represented_by
        row["review_outcome"] = ("already_represented" if review_id
                                 else "awaiting_review")
        row["linked_recovery"] = review_id
        # Un candidato ya recuperado se ve por su reclamo: el rótulo pasó
        # a llevar «r» en el identificador, porque quien lo leyó fue una
        # persona en la imagen.
        recovered_id = f"p{candidate.scan_page:04d}r{candidate.block_id[6:]}"
        claim = accepted_by_block.get(recovered_id)
        row["recovered_chapter"] = claim.accepted_number if claim else None
        marker_rows.append(row)
    marker_report = {
        "about": ("headings the recognition DID emit and whose division word "
                  "it destroyed («S A L M O X L.», «5ALMO CXXXVI.»). The text "
                  "cannot be read as a division, so no boundary opened and "
                  "the psalm's content waited in review. This queue only says "
                  "where to look: candidate generation may be heuristic, "
                  "recovery may not -- every entry here is confirmed in the "
                  "facsimile before it becomes a chapter, and the raw OCR is "
                  "never edited."),
        "not_the_same_as_missing_heading": (
            "`image_review_queue.missing_heading` ranks pages where the "
            "heading LINE is absent. These are pages where the line is "
            "present and unreadable as a division. The two queues are kept "
            "apart on purpose."),
        "signals": {"marker_similarity_at_least": corrupted_markers.SIMILAR_ENOUGH,
                    "quite_similar": corrupted_markers.QUITE_SIMILAR,
                    "max_content_words": corrupted_markers.MAX_CONTENT_WORDS},
        "scan_seconds": marker_seconds,
        "lines_inspected": sum(marker_rejections.values()) + len(marker_rows),
        "rejected_by_reason": dict(sorted(marker_rejections.items())),
        "summary": corrupted_markers.summary(marker_candidates),
        "total": len(marker_rows),
        "already_represented": sum(1 for r in marker_rows
                                   if r["review_outcome"] == "already_represented"),
        "awaiting_review": sum(1 for r in marker_rows
                               if r["review_outcome"] == "awaiting_review"),
        "recovered": sum(1 for r in marker_rows
                         if r["recovered_chapter"] is not None),
        "candidates": marker_rows,
    }

    # Rótulos rotos por los dos lados: ni la palabra ni el numeral se
    # leen, así que lo único que queda es cómo está compuesta la plana.
    # Descubre geometría; confirma el facsímil.
    severe_started = time.time()
    severe_represented = {}
    for claim in ledger.claims:
        block = claim.block_id
        if block[5:6] == "r":
            block = "p" + block[1:5] + "l" + block[6:]
        severe_represented.setdefault(block, f"claim:{claim.disposition}")
    for entry in (payload.get("reviews", []) if payload else []):
        if entry.get("heading_block"):
            severe_represented[entry["heading_block"]] = entry["id"]
    for entry in (payload.get("numeral_reviews", []) if payload else []):
        severe_represented[entry["target_block"]] = entry["id"]
    severe_candidates, severe_discarded = severe_headings.scan(
        pages_with_layout(), book_at=lambda page: structure.book_at(spans, page),
        represented=severe_represented)
    severe_seconds = round(time.time() - severe_started, 2)
    severe_reviews = {}
    # Una fila del barrido estructural la cierra la revisión que la
    # contestó, venga del barrido mismo o de la tanda de ordinales
    # escritos: el candidato de Sabiduría salió de aquí y se resolvió
    # allí, y tiene que dejar de estar pendiente sin desaparecer.
    for entry in (payload.get("reviews", []) if payload else []):
        if entry.get("discovered_by") not in ("severely_corrupted_heading",
                                              "written_ordinal"):
            continue
        # Un rótulo confirmado nombra su renglón; un rechazo nombra el
        # candidato que contestó. Los dos cierran la misma fila.
        key = entry.get("heading_block") or entry.get("candidate_block")
        if key:
            severe_reviews[key] = entry
    severe_rows = []
    for candidate in severe_candidates:
        row = candidate.as_dict()
        review = next((severe_reviews[b] for b in candidate.block_ids
                       if b in severe_reviews), None)
        row["review_id"] = review["id"] if review else candidate.represented_by
        row["review_outcome"] = (
            review["review_outcome"] if review else
            "already_represented" if candidate.represented_by else
            "pending_review")
        row["printed_heading"] = (review.get("observed_printed_text")
                                  if review else None)
        # Mirada, con o sin texto legible que transcribir.
        row["reviewed_here"] = review is not None
        row["recovered_chapter"] = None
        for block in candidate.block_ids:
            recovered = f"p{candidate.scan_page:04d}r{block[6:]}"
            claim = accepted_by_block.get(recovered)
            if claim is not None:
                row["recovered_chapter"] = claim.accepted_number
                break
        # Descubrir no es estar pendiente: lo ya mirado se queda en la
        # historia y sale de la lista de trabajo.
        row["still_pending"] = row["review_outcome"] == "pending_review"
        severe_rows.append(row)
    severe_by_rank = {}
    for row in severe_rows:
        if not row["still_pending"]:
            continue
        severe_by_rank[row["rank"]] = severe_by_rank.get(row["rank"], 0) + 1
    severe_by_book = {}
    for row in severe_rows:
        if not row["still_pending"]:
            continue
        severe_by_book[row["book"] or "(front matter)"] = \
            severe_by_book.get(row["book"] or "(front matter)", 0) + 1
    severe_report = {
        "about": ("rows that look like a printed label by their composition "
                  "alone: narrow, centred over the gutter, white above and "
                  "the editor's argument below. Neither the division word nor "
                  "the numeral is required, because in these the recognition "
                  "destroyed both. GEOMETRY DISCOVERS, THE FACSIMILE "
                  "CONFIRMS: nothing here becomes a chapter on its own."),
        "counted_apart": ("`candidates_total` is what the sweep finds every "
                          "run, already-recovered labels included; "
                          "`pending_review` is what is left to look at."),
        "thresholds": {"max_width_ratio": severe_headings.MAX_WIDTH_RATIO,
                       "max_off_centre": severe_headings.MAX_OFF_CENTRE,
                       "min_gap_before": severe_headings.MIN_GAP_BEFORE,
                       "max_tokens": severe_headings.MAX_TOKENS,
                       "argument_width": severe_headings.ARGUMENT_WIDTH,
                       "argument_within_rows": severe_headings.ARGUMENT_WITHIN},
        "scan_seconds": severe_seconds,
        "rows_discarded_by_reason": dict(sorted(severe_discarded.items())),
        "candidates_total": len(severe_rows),
        "already_represented": sum(1 for r in severe_rows
                                   if r["review_outcome"] == "already_represented"),
        "reviewed_here": sum(1 for r in severe_rows if r["reviewed_here"]),
        "reviewed_by_outcome": dict(sorted(collections.Counter(
            r["review_outcome"] for r in severe_rows
            if r["reviewed_here"]).items())),
        "pending_review": sum(1 for r in severe_rows if r["still_pending"]),
        "pending_by_rank": dict(sorted(severe_by_rank.items())),
        "pending_by_book": dict(sorted(severe_by_book.items())),
        "multi_block_rows": sum(1 for r in severe_rows if r["multi_block"]),
        "recovered_chapters": sum(1 for r in severe_rows
                                  if r["recovered_chapter"] is not None),
        "summary": severe_headings.summary(severe_candidates),
        "candidates": severe_rows,
    }

    # Números de división escritos con palabra. Es otra evidencia que el
    # numeral romano y se cuenta aparte: la primera división de cada
    # libro de este tomo lleva «PRIMERO» donde las demás llevan «XXIV».
    ordinal_started = time.time()
    ordinal_rows = []
    seen_ordinal_blocks = set()
    for claim in ledger.claims:
        evidence = claim.ordinal or {}
        if evidence.get("status") not in (written_ordinals.RECOGNIZED,
                                          written_ordinals.UNSUPPORTED):
            continue
        seen_ordinal_blocks.add(claim.block_id)
        ordinal_rows.append({
            "found_in": "chapter_claim",
            "book": claim.book, "scan_page": claim.scan_page,
            "pdf_page": claim.scan_page + 1, "block_id": claim.block_id,
            "raw_text": claim.raw_heading[:120],
            "is_heading": bool(claim.is_structural_heading),
            "heading_source": (claim.heading or {}).get("source"),
            "ordinal_raw": evidence.get("raw_token"),
            "ordinal_normalized": evidence.get("normalized_token"),
            "ordinal_status": evidence.get("status"),
            "ordinal_value": evidence.get("value"),
            "numeral_status": claim.numeral.get("status"),
            "numeral_value": claim.numeral.get("value"),
            "claim_id": claim.claim_id,
            "claim_disposition": claim.disposition,
            "claim_number": claim.accepted_number,
            "number_source": claim.number_source,
            "proposal_method": claim.proposal_method,
            "review_id": (claim.provenance or {}).get("review_id"),
            "evidence_source": ("image_review"
                                if claim.source == chapter_claims.FROM_IMAGE_REVIEW
                                else "ocr_text"),
            "still_pending": claim.accepted_number is None,
        })
    # Y las filas que el barrido estructural ofreció sin que llegaran a
    # ser reclamo: ahí es donde estaba el rótulo de Sabiduría, con la
    # palabra de división y la del número rotas a la vez.
    for row in severe_rows:
        if not written_ordinals.looks_ordinal(row["raw_text"]):
            continue
        if any(block in seen_ordinal_blocks for block in row["block_ids"]):
            continue
        reading = written_ordinals.read(row["raw_text"], strict=False)
        ordinal_rows.append({
            "found_in": "severe_heading_candidate",
            "book": row["book"], "scan_page": row["scan_page"],
            "pdf_page": row["pdf_page"], "block_id": row["block_ids"][0],
            "raw_text": row["raw_text"][:120],
            "is_heading": None,
            "heading_source": "structural_sweep",
            "ordinal_raw": reading.raw_token,
            "ordinal_normalized": reading.normalized_token,
            "ordinal_status": reading.status,
            "ordinal_value": reading.value,
            "numeral_status": None, "numeral_value": None,
            "claim_id": None,
            "claim_disposition": None, "claim_number": None,
            "number_source": None, "proposal_method": None,
            "review_id": row["review_id"],
            "evidence_source": "structural_sweep",
            "still_pending": bool(row["still_pending"]),
        })
    ordinal_rows.sort(key=lambda r: (r["scan_page"], r["block_id"]))
    ordinal_conflicts = [c.as_dict() for c in ledger.claims
                         if c.proposal_method == chapter_claims.ORDINAL_CONFLICT]
    ordinal_report = {
        "about": ("Divisiones cuyo número está escrito con palabra y no con "
                  "numeral romano. Son evidencia distinta y se leen con un "
                  "vocabulario cerrado, medido en el tomo: lo que no está en "
                  "la tabla no se lee, y una palabra dañada espera a la "
                  "imagen en vez de arreglarse por parecido."),
        "vocabulary": written_ordinals.vocabulary(),
        "language": written_ordinals.LANGUAGE,
        "candidates_total": len(ordinal_rows),
        "reviewed": sum(1 for r in ordinal_rows if r["review_id"]),
        "recognized_clean": sum(
            1 for r in ordinal_rows
            if r["ordinal_status"] == written_ordinals.RECOGNIZED
            and r["evidence_source"] == "ocr_text"),
        "image_resolved": sum(1 for r in ordinal_rows
                              if r["number_source"] == "image_review_ordinal"),
        "unsupported": sum(1 for r in ordinal_rows
                           if r["ordinal_status"] == written_ordinals.UNSUPPORTED),
        "conflicts": len(ordinal_conflicts),
        "conflict_detail": ordinal_conflicts,
        "pending_review": sum(1 for r in ordinal_rows if r["still_pending"]),
        "by_book": dict(sorted(collections.Counter(
            r["book"] or "(front matter)" for r in ordinal_rows).items())),
        "by_ordinal_value": dict(sorted(collections.Counter(
            str(r["ordinal_value"]) for r in ordinal_rows).items())),
        "by_number_source": dict(sorted(collections.Counter(
            r["number_source"] for r in ordinal_rows
            if r["number_source"]).items())),
        "scan_seconds": round(time.time() - ordinal_started, 2),
        "candidates": ordinal_rows,
    }

    # Lo que quedaba: reclamos que ya eran rótulos válidos y seguían sin
    # número. No hay descubrimiento aquí --la lista la da el ledger--,
    # sólo lo que dijo la plana de cada uno. Se cuenta aparte de las
    # demás familias porque su causa es otra: no faltaba el rótulo,
    # faltaba poder leer su numeral.
    remaining_reviews = {}
    for entry in (payload.get("numeral_reviews", []) if payload else []):
        if entry.get("discovered_by") == "remaining_chapter_claim":
            remaining_reviews[entry["target_block"]] = entry
    remaining_rows = []
    for claim in ledger.claims:
        entry = remaining_reviews.get(claim.block_id)
        if entry is None:
            continue
        remaining_rows.append({
            "claim_id": claim.claim_id, "book": claim.book,
            "scan_page": claim.scan_page, "pdf_page": claim.scan_page + 1,
            "block_id": claim.block_id,
            "raw_heading": claim.raw_heading[:120],
            "raw_numeral": entry.get("raw_numeral"),
            "numeral_status_before": claim.numeral.get("status"),
            "review_id": entry["id"], "review_outcome": entry["outcome"],
            "observed_printed_text": entry.get("observed_printed_text"),
            "printed_numeral": entry.get("observed_printed_numeral"),
            "supporting_facsimile_evidence":
                entry.get("supporting_facsimile_evidence"),
            "resolved_chapter": entry.get("recovered_chapter"),
            "previous_anchor": entry.get("previous_anchor"),
            "next_anchor": entry.get("next_anchor"),
            "disposition_after": claim.disposition,
            "chapter_after": claim.accepted_number,
            "number_source": claim.number_source,
            "proposal_method": claim.proposal_method,
            # Directo es lo que resolvió la plana; cascada, lo que se
            # aceptó después apoyándose en un ancla ya aceptada. No es lo
            # mismo y no puede contarse junto.
            "resolution": ("direct_image_review"
                           if claim.proposal_method == "numeral_review"
                           else "cascade"),
            "still_pending": claim.accepted_number is None,
        })
    remaining_rows.sort(key=lambda r: (r["scan_page"], r["block_id"]))
    # Cascada de ESTA tanda: un reclamo que se aceptó apoyándose en un
    # ancla que acaba de resolverse aquí. Las cascadas antiguas del tomo
    # no cuentan: contarlas diría que esta revisión desbloqueó cosas que
    # ya estaban desbloqueadas.
    resolved_here = {r["claim_id"] for r in remaining_rows
                     if r["chapter_after"] is not None}
    cascades = [c for c in ledger.claims
                if c.anchor_claim in resolved_here
                and c.claim_id not in resolved_here
                and c.accepted_number is not None]
    remaining_report = {
        "about": ("Reclamos que ya eran rótulos válidos y seguían sin número. "
                  "La lista la da el ledger, no un barrido; lo que decide "
                  "cada uno es la plana. Un numeral romano canónico no es un "
                  "numeral correcto: trece de estos lo eran y decían otra "
                  "cosa que el impreso."),
        "baseline_unresolved": len(remaining_rows),
        "reviewed": sum(1 for r in remaining_rows if r["review_id"]),
        "resolved_direct": sum(1 for r in remaining_rows
                               if r["resolution"] == "direct_image_review"
                               and r["chapter_after"] is not None),
        "cascades": len(cascades),
        "cascade_detail": [{"claim_id": c.claim_id, "book": c.book,
                            "scan_page": c.scan_page, "block_id": c.block_id,
                            "chapter": c.accepted_number,
                            "own_numeral": c.numeral.get("token"),
                            "own_numeral_status": c.numeral.get("status"),
                            "number_source": c.number_source,
                            "anchor_claim": c.anchor_claim,
                            "anchor_number": c.anchor_number}
                           for c in cascades],
        "by_outcome": dict(sorted(collections.Counter(
            r["review_outcome"] for r in remaining_rows).items())),
        "by_book": dict(sorted(collections.Counter(
            r["book"] for r in remaining_rows).items())),
        "by_numeral_status_before": dict(sorted(collections.Counter(
            r["numeral_status_before"] for r in remaining_rows).items())),
        "with_supporting_evidence": sum(
            1 for r in remaining_rows if r["supporting_facsimile_evidence"]),
        "still_pending": sum(1 for r in remaining_rows if r["still_pending"]),
        "remaining_unresolved": claims_report["unresolved"],
        "claims": remaining_rows,
    }

    # Números que la versificación espera y el tomo no tiene. Un hueco
    # del canon es una PREGUNTA --dónde mirar--, nunca una respuesta:
    # esta edición puede fundir dos salmos, saltarse un número o
    # numerar distinto, y eso sólo lo dice la plana. La sección existe
    # para poder afirmar «revisado y explicado» sin que el número tenga
    # que aparecer en el mapa de capítulos.
    #
    # Todo esto vive en su propia función a propósito: calcularlo al
    # hilo del informe reutilizaba el nombre `limit`, que es el
    # parámetro con el que este audit recorta cuántas planas lee, y
    # dejaba el resto del recorrido leyendo sólo las primeras. Un
    # ámbito propio es más barato que recordarlo.
    def _canonical_gap_report():
        accepted_numbers = {}
        for claim in ledger.claims:
            if claim.disposition == chapter_claims.ACCEPTED \
                    and claim.accepted_number is not None:
                accepted_numbers.setdefault(claim.book, {})[claim.accepted_number] = claim
        gap_reviews = {}
        for entry in (payload.get("reviews", []) if payload else []):
            if entry.get("discovered_by") == "canonical_chapter_gap":
                gap_reviews[(entry["book"], entry["expected_chapter"])] = entry
        canonical_expected, canonical_missing = {}, {}
        for span in spans:
            canon_limit = structure.chapter_limit(span.osis)
            if canon_limit is None:
                continue
            canonical_expected[span.osis] = canon_limit
            have = set(accepted_numbers.get(span.osis, {}))
            canonical_missing[span.osis] = sorted(set(range(1, canon_limit + 1)) - have)
        gap_rows = []
        keys = {(book, number) for book, numbers in canonical_missing.items()
                for number in numbers} | set(gap_reviews)
        for book, number in sorted(keys, key=lambda k: (k[0], k[1])):
            entry = gap_reviews.get((book, number))
            claim = accepted_numbers.get(book, {}).get(number)
            row = {
                "book": book, "canonical_number": number,
                "present_in_chapter_map": claim is not None,
                "scan_page": claim.scan_page if claim else (
                    entry.get("scan_page") if entry else None),
                "block_id": claim.block_id if claim else None,
                "previous_accepted": entry.get("previous_accepted_heading") if entry else None,
                "next_accepted": entry.get("next_accepted_heading") if entry else None,
                "scan_range": entry.get("scan_range") if entry else None,
                "observed_printed_text": entry.get("observed_printed_text") if entry else None,
                "observed_printed_numeral": entry.get("observed_printed_numeral") if entry else None,
                "observed_spanish_structure": entry.get("observed_spanish_structure") if entry else None,
                "observed_latin_structure": entry.get("observed_latin_structure") if entry else None,
                "ocr_state": entry.get("ocr_state") if entry else None,
                "raw_ocr_heading": entry.get("raw_ocr_heading") if entry else None,
                "outcome": entry.get("gap_outcome") if entry else None,
                "review_id": entry["id"] if entry else None,
                "structural_action": entry.get("structural_action") if entry else None,
                "number_source": claim.number_source if claim else None,
                # Revisado no es resuelto: un hueco puede quedar explicado
                # porque la edición no imprime esa división, y entonces sigue
                # sin estar en el mapa y ya no espera a nadie.
                "still_pending": entry is None,
            }
            gap_rows.append(row)
        report = {
            "about": ("Números que la versificación espera y el mapa de capítulos "
                      "no tiene. El canon localiza la anomalía; lo que ocurre lo "
                      "dice el facsímil. Un hueco revisado sale de la cola aunque "
                      "el número siga ausente: puede que la edición no lo "
                      "imprima."),
            "counted_apart": ("`canonical_missing` es lo que le falta al mapa "
                              "frente al canon; `pending_review` es lo que nadie "
                              "ha ido a mirar todavía. No son lo mismo."),
            "canonical_expected": dict(sorted(canonical_expected.items())),
            "accepted_physical": {book: len(numbers) for book, numbers
                                  in sorted(accepted_numbers.items())},
            "canonical_missing": {book: numbers for book, numbers
                                  in sorted(canonical_missing.items()) if numbers},
            "candidates_total": len(gap_rows),
            "reviewed": sum(1 for r in gap_rows if r["review_id"]),
            "physically_present": sum(1 for r in gap_rows
                                      if r["outcome"] == "confirmed_missing_ocr_heading"),
            "physically_absent": sum(1 for r in gap_rows
                                     if r["outcome"] == "omitted_in_print"),
            "merged": sum(1 for r in gap_rows if r["outcome"] in
                          ("merged_with_previous", "merged_with_next")),
            "local_renumbering": sum(1 for r in gap_rows if r["outcome"] ==
                                     "local_renumbering_difference"),
            "editorial_without_number": sum(
                1 for r in gap_rows
                if r["outcome"] == "editorial_division_without_number"),
            "unreadable": sum(1 for r in gap_rows
                              if r["outcome"] == "unreadable_facsimile"),
            "by_outcome": dict(sorted(collections.Counter(
                r["outcome"] or "(sin revisar)" for r in gap_rows).items())),
            "by_book": dict(sorted(collections.Counter(
                r["book"] for r in gap_rows).items())),
            "pending_review": sum(1 for r in gap_rows if r["still_pending"]),
            "unexplained": sum(1 for r in gap_rows
                               if r["still_pending"] and not r["present_in_chapter_map"]),
            "gaps": gap_rows,
        }
        return report

    gap_report = _canonical_gap_report()

    # Cola de revisión visual, ordenada por lo que más devuelve. No crea
    # estructura: sólo dice dónde mirar.
    geometry = {}
    for page, placed in pages_with_layout():
        geometry[page.scan_page] = recovery_candidates.page_geometry(page, placed)
    # Lo decidido no está pendiente: un reclamo rechazado como falso
    # rótulo tiene respuesta, y pedir que lo revisen otra vez confunde
    # «sin número» con «sin decidir».
    decided_blocks = {c.block_id for c in ledger.claims
                      if c.accepted_number is None
                      and c.disposition != chapter_claims.UNRESOLVED}
    queue = recovery_candidates.rank(readings=readings, resolutions=resolutions,
                                     spans=spans, geometry=geometry,
                                     decided_blocks=decided_blocks)
    # Qué se hizo con cada candidato de `missing_heading`. La cola dice
    # dónde MIRAR; esto dice qué se vio. Un candidato revisado no
    # desaparece: cambia de estado y se queda con su procedencia, que es
    # lo que impide volver a rankearlo y lo que permite auditar si la
    # familia acierta o no.
    missing_reviews = {}
    for entry in (payload.get("reviews", []) if payload else []):
        if entry.get("discovered_by") != "missing_heading":
            continue
        missing_reviews[entry["scan_page"]] = entry
    accepted_by_block = {c.block_id: c for c in ledger.accepted()}
    # La lista es la UNIÓN de lo que la cola señala hoy y de todo lo que
    # se revisó como `missing_heading` alguna vez. Un candidato
    # recuperado deja de aparecer en la cola --su plana ya tiene rótulo--
    # y si sólo se mirase la cola, el trabajo hecho desaparecería del
    # informe y el contador bajaría sin decir por qué.
    still_queued = {c.scan_page: c for c in queue
                    if c.family == recovery_candidates.MISSING_HEADING}
    missing_rows = []
    for page_number in sorted(set(still_queued) | set(missing_reviews)):
        candidate = still_queued.get(page_number)
        review = missing_reviews.get(page_number)
        row = {
            "book": (candidate.book if candidate else review["book"]),
            "scan_page": page_number, "pdf_page": page_number + 1,
            "score": candidate.score if candidate else None,
            "signals": ({k: str(v) for k, v in candidate.signals.items()}
                        if candidate else {}),
            # Descubrir y estar pendiente son cosas distintas. El
            # generador sigue señalando la plana --es un diagnóstico y no
            # se le quita-- pero una vez mirada no es trabajo por hacer.
            "discovered_now": candidate is not None,
            "still_pending": candidate is not None and review is None,
            "gap_after_block": candidate.gap_after_block if candidate else None,
            "gap_before_block": candidate.gap_before_block if candidate else None,
            "review_outcome": (review.get("review_outcome") if review
                               else "awaiting_review"),
            "review_id": review["id"] if review else None,
            "printed_heading": review.get("observed_printed_text") if review else None,
            "printed_numeral": (review.get("observed_printed_numeral")
                                if review else None),
            "heading_block": review.get("heading_block") if review else None,
            "anchor_after": review.get("insert_after_block") if review else None,
            "anchor_before": review.get("insert_before_block") if review else None,
            "bbox": review.get("crop_bbox") if review else None,
            "recovered_chapter": None,
        }
        block = row["heading_block"]
        if block:
            recovered_id = f"p{page_number:04d}r{block[6:]}"
            claim = accepted_by_block.get(recovered_id)
            row["recovered_chapter"] = claim.accepted_number if claim else None
        missing_rows.append(row)
    by_outcome = {}
    for row in missing_rows:
        by_outcome[row["review_outcome"]] = \
            by_outcome.get(row["review_outcome"], 0) + 1
    by_book = {}
    for row in missing_rows:
        stat = by_book.setdefault(row["book"], {})
        stat[row["review_outcome"]] = stat.get(row["review_outcome"], 0) + 1
    pending_by_book = {}
    for row in missing_rows:
        if not row["still_pending"]:
            continue
        pending_by_book[row["book"]] = pending_by_book.get(row["book"], 0) + 1
    missing_report = {
        "about": ("what was seen at each `missing_heading` candidate. The "
                  "queue ranks blanks in the body where a label could have "
                  "been lost; only the facsimile says whether one was. A "
                  "reviewed candidate does not vanish from the queue -- it "
                  "changes state and keeps its provenance."),
        "outcomes": ("confirmed_missing_heading: the label is printed and NO "
                     "OCR block represents it, so a structural event is "
                     "inserted. existing_ocr_heading_found: the label is "
                     "printed and the recognition did emit a block for it, so "
                     "that block is marked and nothing is inserted. "
                     "false_missing_heading_candidate: no label is printed "
                     "there."),
        "counted_apart": ("`candidates_total` is what the generator has "
                          "found; `pending_review` is what is left to do. "
                          "They are not the same number and conflating them "
                          "is what made a finished family look like open "
                          "work: the seven blanks that turned out to be "
                          "margins, book endings and editorial section marks "
                          "are still discovered every run, and that is "
                          "correct -- they are simply no longer pending."),
        "candidates_total": len(missing_rows),
        "discovered_now": sum(1 for r in missing_rows if r["discovered_now"]),
        "reviewed": sum(1 for r in missing_rows
                        if r["review_outcome"] != "awaiting_review"),
        "existing_ocr_heading_found": sum(
            1 for r in missing_rows
            if r["review_outcome"] == "existing_ocr_heading_found"),
        "false_reviewed": sum(
            1 for r in missing_rows
            if r["review_outcome"] == "false_missing_heading_candidate"),
        "confirmed_missing_heading": sum(
            1 for r in missing_rows
            if r["review_outcome"] == "confirmed_missing_heading"),
        "pending_review": sum(1 for r in missing_rows if r["still_pending"]),
        "pending_by_book": pending_by_book,
        "by_outcome": dict(sorted(by_outcome.items())),
        "by_book": {book: dict(sorted(stat.items()))
                    for book, stat in sorted(by_book.items())},
        "recovered_chapters": sum(1 for r in missing_rows
                                  if r["recovered_chapter"] is not None),
        "candidates": missing_rows,
    }

    # La cola de revisión visual informa de lo que queda POR MIRAR. Una
    # plana ya mirada se sigue descubriendo --el blanco en la caja no se
    # va porque alguien lo haya explicado-- pero deja de ser trabajo
    # pendiente, y mezclar las dos cosas hacía que una familia terminada
    # pareciera abierta. Las dos cuentas se publican por separado.
    reviewed_missing_pages = {row["scan_page"] for row in missing_rows
                              if row["review_outcome"] != "awaiting_review"}
    pending_queue = [c for c in queue
                     if not (c.family == recovery_candidates.MISSING_HEADING
                             and c.scan_page in reviewed_missing_pages)]
    families = (recovery_candidates.UNRESOLVED_NUMERAL,
                recovery_candidates.MISSING_HEADING)
    candidate_report = {
        "model": "two families: boundary certain with the number lost, and "
                 "no heading at all. Ranking only -- nothing here confirms "
                 "anything or supplies a number.",
        "counted_apart": ("`total` is what is left to review. "
                          "`discovered_total` is what the generator found, "
                          "reviewed candidates included: a blank that turned "
                          "out to be a margin keeps being discovered and is "
                          "no longer pending."),
        "weights": recovery_candidates.WEIGHTS,
        "total": len(pending_queue),
        "discovered_total": len(queue),
        "reviewed_and_closed": len(queue) - len(pending_queue),
        "by_family": {family: sum(1 for c in pending_queue
                                  if c.family == family)
                      for family in families},
        "discovered_by_family": {family: sum(1 for c in queue
                                             if c.family == family)
                                 for family in families},
        "by_book": {},
        "top": [{"family": c.family, "book": c.book, "scan_page": c.scan_page,
                 "pdf_page": c.scan_page + 1, "score": c.score,
                 "target_block": c.target_block,
                 "signals": {k: str(v) for k, v in c.signals.items()}}
                for c in pending_queue[:60]],
    }
    for c in pending_queue:
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
        # «Sin número» y «sin resolver» no son lo mismo: lo segundo
        # excluye lo que se decidió rechazar. Se publican los dos.
        "without_number_before": pre_counts["without_number"],
        "without_number_after": claims_report["without_number"],
        "unresolved_before": pre_counts["unresolved"],
        "unresolved_after": claims_report["unresolved"],
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
            "without_number_before": was.get("without_number", 0),
            "without_number_after": now.get("without_number", 0),
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

    # Segmentación de versículos: cuántas fronteras faltan, de qué forma
    # y con qué indicio al lado. El inventario no crea nada -- ni un
    # versículo, ni una cola de trabajo humano: un hueco detectado es
    # una pregunta, y sólo una revisión del facsímil la contesta.
    def _verse_segmentation_report():
        found = verse_gaps.inventory(edition, verse_limit=structure.verse_limit)
        out = dict(verse_gaps.summary(found))
        out["about"] = (
            "Huecos en la numeración de versículos. El dominio va de 1 al "
            "mayor entre el último verso materializado y el último que "
            "numera la versificación nativa (Vulgata de SWORD), así que "
            "también se ven los huecos de cola. Un número materializado por "
            "encima del último canónico no es un verso perdido: es el rastro "
            "de un numeral espurio, y se cuenta aparte.")
        out["counted_apart"] = (
            "`total` son huecos detectados; `reviewed` los mirados en la "
            "imagen. Ni uno de los detectados es trabajo humano pendiente "
            "hasta que se sepa qué patrones se pueden recuperar solos.")
        out["expected_count_source"] = (
            "materialized top and native Vulgate versification (SWORD "
            "canon_vulg.h), never sequence and never the neighbours")
        out["strata"] = verse_gaps.strata(found)
        out["sample_size"] = min(30, len(found))
        out["sample"] = [g.key for g in
                         verse_gaps.sample(found, size=out["sample_size"])]
        # Lo que se miró en el facsímil, y cómo acabó cada uno.
        seen, by_outcome, bank = [], {}, {}
        for entry in (verse_payload.get("reviews", []) if verse_payload else []):
            key = f"{entry['book']}.{entry['chapter']}.{entry['verse']}"
            by_outcome[entry["outcome"]] = by_outcome.get(entry["outcome"], 0) + 1
            seen.append({
                "key": key, "review_id": entry["review_id"],
                "scan_page": entry["scan_page"], "pdf_page": entry["pdf_page"],
                "gap_shape": entry["gap_shape"],
                "marker_block": entry["marker_block"],
                "marker_case": entry["marker_case"],
                "raw_ocr": entry["raw_ocr"],
                "observed_printed_marker": entry["observed_printed_marker"],
                "latin_support": entry.get("latin_support"),
                "outcome": entry["outcome"],
                "structural_effect": entry["structural_effect"],
                "confidence": entry["confidence"],
            })
        out["reviewed"] = len(seen)
        out["reviewed_by_outcome"] = dict(sorted(by_outcome.items()))
        out["reviewed_detail"] = seen
        out["manual_review_pending"] = 0
        # El banco prioritario, con su recuento frente a la versificación.
        for osis, entry in edition.books.items():
            for number, chapter in entry.chapters.items():
                if not number or (osis, number) not in _VERSE_BANK:
                    continue
                verses = sorted(v for v in chapter.verses if v is not None)
                canonical = structure.verse_limit(osis, number)
                bank[f"{osis}.{number}"] = {
                    "canonical_verses": canonical,
                    "materialized": verses,
                    "materialized_count": len(verses),
                    "missing": [g.verse for g in found
                                if g.book == osis and g.chapter == number],
                    "paratext_blocks": len(getattr(chapter, "paratext", ())),
                }
        out["priority_bank"] = dict(sorted(bank.items()))
        # Lo que la tanda 125 rechazó y lo que recuperó, contado aparte:
        # descartar un número imposible NO es recuperar un versículo, y
        # mezclar las dos cosas haría ilegible el efecto de cada regla.
        impossible = list(getattr(walker, "impossible_markers", []))
        out["impossible_marker_audit"] = {
            "about": ("Marcadores de versículo que la versificación nativa "
                      "desmiente: un número mayor que el último verso del "
                      "capítulo no puede ser una frontera suya. El canon "
                      "RECHAZA; no crea nada. El texto que llevaba el número "
                      "imposible no se borra: pasa al versículo anterior."),
            "rejected": len(impossible),
            "chapters_affected": len({(r["book"], r["chapter"])
                                      for r in impossible}),
            "by_book": dict(sorted(collections.Counter(
                r["book"] for r in impossible).items())),
            "by_reason": dict(sorted(collections.Counter(
                r["reason"] for r in impossible).items())),
            "by_landing": dict(sorted(collections.Counter(
                r["text_landed"] for r in impossible).items())),
            "blocks_preserved": sum(r["blocks"] for r in impossible),
            "worst": sorted(impossible,
                            key=lambda r: -(r["marker"] - r["verse_limit"]))[:12],
            "markers": impossible,
        }
        framed = list(getattr(walker, "framed_markers", []))
        forms = collections.Counter()
        for row in framed:
            head = row["raw"].strip()[:1]
            forms[_PUNCTUATION_FORMS.get(head, "other_outer_punctuation")] += 1
        out["exact_numeric_marker_recovery"] = {
            "about": ("Marcadores cuyas cifras ya estaban ENTERAS en el crudo "
                      "y a los que sólo tapaba la puntuación de al lado. Se "
                      "recortan los signos de los extremos y nada más: aquí "
                      "no se convierte ninguna letra en cifra, ni se completa "
                      "ningún dígito que falte."),
            "whitelist": classifier.SAFE_OUTER_PUNCTUATION,
            "counted_apart": (
                "Recuperar un marcador no es ganar una referencia. "
                "`accepted` son los marcadores; `by_outcome` dice qué hizo "
                "cada uno. `only_numbered_line_of_its_ref` es el marcador que "
                "es el único renglón numerado de su versículo, que es lo que "
                "se puede afirmar mirando SÓLO el resultado; cuántas "
                "referencias son nuevas respecto de no aplicar la regla es "
                "otra pregunta, y se responde comparando dos pasadas."),
            "accepted": len(framed),
            "by_outcome": dict(sorted(collections.Counter(
                row.get("outcome", "unclassified") for row in framed).items())),
            "only_numbered_line_of_its_ref": sum(
                1 for row in framed
                if row.get("outcome") == "only_numbered_line_of_its_ref"),
            "by_punctuation_form": dict(sorted(forms.items())),
            "by_book": dict(sorted(collections.Counter(
                r["book"] for r in framed).items())),
            "markers": framed,
        }
        # La clase que la tanda 126 fue a buscar: el hueco cuyo número
        # ya está, entero, en un renglón del versículo anterior. Al
        # mirarla se vio que en este tomo NINGUNO está en medio del
        # texto: todos encabezan su renglón detrás de basura del canto,
        # así que se recuperan por el mismo camino de la 125 con el
        # marco ensanchado, y no hizo falta partir ningún bloque.
        embedded = [gap for gap in found
                    if verse_gaps.SWALLOWED_DIGIT in gap.signals]
        pending = []
        for gap in embedded:
            text = (gap.swallowed_text or "").strip()
            literal = str(gap.verse)
            after = text.split(literal, 1)[1] if literal in text else ""
            head = text[:text.find(literal)] if literal in text else text
            if literal in text and after[:1] and not after[:1].isspace() \
                    and (after[:1].isalpha() or after[:1].isdigit()):
                reason = "marker_glued_to_word"
            elif after.strip()[:1].isdigit():
                reason = "digit_follows_marker"
            elif any(ch.isdigit() for ch in head):
                reason = "two_candidate_digits"
            else:
                reason = "other_context"
            pending.append({
                "key": gap.key, "book": gap.book, "chapter": gap.chapter,
                "verse": gap.verse, "shape": gap.shape,
                "block_id": gap.swallowed_block,
                "scan_page": int(gap.swallowed_block[1:5])
                             if gap.swallowed_block else None,
                "raw": gap.swallowed_text, "literal": literal,
                "reason": reason,
                "current_owner_verse": gap.previous_verse,
            })
        frames = collections.Counter()
        for row in framed:
            lead = row["raw"].strip()[:1]
            frames["scan_debris" if lead in classifier.SCAN_DEBRIS
                   else "outer_punctuation"] += 1
        out["embedded_exact_marker_recovery"] = {
            "about": ("Huecos cuyo número decimal ya está ENTERO en un "
                      "renglón del versículo anterior. En este tomo ninguno "
                      "está embebido en mitad del texto: todos encabezan su "
                      "renglón tras un glifo de basura, así que se recuperan "
                      "por el marco del marcador -- ensanchado con los "
                      "cuatro glifos de canto medidos -- y ningún bloque del "
                      "reconocimiento se ha partido."),
            "scan_debris_alphabet": classifier.SCAN_DEBRIS,
            "markers_by_frame_class": dict(sorted(frames.items())),
            "still_pending": len(pending),
            "pending_by_reason": dict(sorted(collections.Counter(
                row["reason"] for row in pending).items())),
            "pending_by_book": dict(sorted(collections.Counter(
                row["book"] for row in pending).items())),
            "pending": pending,
            "blocks_logically_split": 0,
        }
        out["gaps"] = [g.as_dict() for g in found]
        return out

    report = {
        "heading_claim_validation": heading_report,
        "numeral_image_review": numeral_report,
        "chapter_claims": claims_report,
        "competing_chapter_claims": len(ledger.collisions()),
        "chapter_image_recovery": recovery_report,
        "corrupted_division_marker_candidates": marker_report,
        "severely_corrupted_heading_candidates": severe_report,
        "written_ordinal_headings": ordinal_report,
        "remaining_chapter_claim_reviews": remaining_report,
        "canonical_chapter_gap_reviews": gap_report,
        "image_review_queue": candidate_report,
        "missing_heading_reviews": missing_report,
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
        # OJO: cuenta CAPÍTULOS con al menos un hueco, no huecos. Se
        # conserva porque hay informes que la citan; la cuenta de huecos
        # está en `verse_segmentation_audit`.
        "verse_number_gaps_total": len(gaps),
        "verse_number_gap_chapters": len(gaps),
        "verse_segmentation_audit": _verse_segmentation_report(),
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
    cg = report.get("canonical_chapter_gap_reviews", {})
    if cg:
        print(f"  canonical gaps            candidates={cg['candidates_total']}"
              f" reviewed={cg['reviewed']}"
              f" present={cg['physically_present']}"
              f" absent={cg['physically_absent']}"
              f" merged={cg['merged']}"
              f" PENDING={cg['pending_review']}"
              f" unexplained={cg['unexplained']}")
        print(f"    canonical missing        "
              f"{cg['canonical_missing'] or 'none'}")
        print(f"    accepted physical        {cg['accepted_physical']}")
    rc = report.get("remaining_chapter_claim_reviews", {})
    if rc:
        print(f"  remaining claims          baseline={rc['baseline_unresolved']}"
              f" reviewed={rc['reviewed']}"
              f" direct={rc['resolved_direct']}"
              f" cascades={rc['cascades']}"
              f" PENDING={rc['still_pending']}")
        print(f"    by outcome               {rc['by_outcome']}")
        print(f"    by book                  {rc['by_book']}"
              f"   latin/vernacular support {rc['with_supporting_evidence']}")
    wo = report.get("written_ordinal_headings", {})
    if wo:
        print(f"  written ordinals          candidates={wo['candidates_total']}"
              f" clean={wo['recognized_clean']}"
              f" image={wo['image_resolved']}"
              f" unsupported={wo['unsupported']}"
              f" conflicts={wo['conflicts']}"
              f" PENDING={wo['pending_review']}  ({wo['scan_seconds']}s)")
        print(f"    vocabulary               {wo['vocabulary']}"
              f"   by book {wo['by_book']}")
        print(f"    by number source         {wo['by_number_source']}")
    sv = report.get("severely_corrupted_heading_candidates", {})
    if sv:
        print(f"  severe headings            candidates={sv['candidates_total']}"
              f" already_represented={sv['already_represented']}"
              f" reviewed_here={sv['reviewed_here']}"
              f" recovered={sv['recovered_chapters']}"
              f" PENDING={sv['pending_review']}"
              f"  ({sv['scan_seconds']}s)")
        print(f"    pending by rank          {sv['pending_by_rank'] or 'none'}"
              f"   by book {sv['pending_by_book'] or 'none'}")
        print(f"    multi-block rows         {sv['multi_block_rows']}")

    mk = report.get("corrupted_division_marker_candidates", {})
    if mk:
        print(f"  corrupted markers          candidates={mk['total']}"
              f" recovered={mk['recovered']}"
              f" already_represented={mk['already_represented']}"
              f" awaiting={mk['awaiting_review']}"
              f"  ({mk['scan_seconds']}s over {mk['lines_inspected']} lines)")
        print(f"    by rank                  {mk['summary']['by_rank']}"
              f"  by family {mk['summary']['by_marker_family']}"
              f"  by book {mk['summary']['by_book']}")
        for row in mk["candidates"]:
            print(f"    {row['rank']:6} {row['book']:5} p{row['scan_page']:<4}"
                  f" {row['block_id']:14} sim={row['marker_similarity']:.2f}"
                  f" {row['raw_text'][:34]!r} -> {row['recovered_chapter']}"
                  f" {row['review_outcome']}")

    mh = report.get("missing_heading_reviews", {})
    if mh:
        print(f"  missing heading reviews    candidates={mh['candidates_total']}"
              f" reviewed={mh['reviewed']}"
              f" existing_ocr_found={mh['existing_ocr_heading_found']}"
              f" false={mh['false_reviewed']}"
              f" confirmed_missing={mh['confirmed_missing_heading']}"
              f" PENDING={mh['pending_review']}")
        print(f"    pending by book          {mh['pending_by_book'] or 'none'}"
              f"   (still discovered: {mh['discovered_now']})")
        print(f"    by outcome               {mh['by_outcome']}")
        print(f"    by book                  {mh['by_book']}")
        for row in mh["candidates"]:
            print(f"    {row['book']:5} p{row['scan_page']:<4}"
                  f" {row['review_outcome']:32}"
                  f" {str(row['printed_heading'] or '-'):22}"
                  f" -> {row['recovered_chapter']}")

    q = report.get("image_review_queue", {})
    if q:
        print(f"  review queue               pending={q['total']}"
              f" {q['by_family']}")
        print(f"    discovered                 {q['discovered_total']}"
              f" {q['discovered_by_family']}"
              f"   reviewed_and_closed={q['reviewed_and_closed']}")
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
              f" unresolved={cc['unresolved']}"
              f" without_number={cc['without_number']}"
              f" (= unresolved + {cc['rejected_false_heading']} rejected)")
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
        print("  book   raw  valid invalid ambig uncorr samephys compet accepted unres  nonum")
        for osis, stat in sorted(cc["per_book"].items()):
            print(f"    {osis:5} {stat['raw_claims']:4} {stat['raw_numeral_valid']:6}"
                  f" {stat['raw_numeral_invalid']:7} {stat['ambiguous']:5}"
                  f" {stat['uncorroborated']:6} {stat['same_physical_duplicates']:8}"
                  f" {stat['competing']:6} {stat['accepted']:8}"
                  f" {stat['unresolved']:5} {stat['without_number']:6}")
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
