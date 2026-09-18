"""
Del OCR con coordenadas al modelo neutral.

El reparto lo manda la geometría, no el texto:

    columna derecha, cuerpo    -> español canónico
    columna izquierda, cuerpo  -> latín paralelo (procedencia, no cuerpo)
    cruza el canal             -> paratexto: división o argumento del editor
    banda superior/inferior    -> cabecera / pie
    banda de notas             -> nota al pie
    no se sabe                 -> UnclassifiedBlock, a revisión

Y la regla de aceptación, la que se saltó el pipeline de 1882:

    un renglón sólo continúa el versículo abierto si está en la MISMA
    columna, en la banda de CUERPO y no abre una frontera.

Cualquier otra cosa sale del versículo. Ante la duda, a la cola de
revisión; nunca al `body` del verso anterior.
"""
from typing import Iterable, Optional

import chapter_claims
import divisions
import heading_validity
import image_reviews as review_outcomes
import parser as classifier
import recovery as image_recovery
import roman
import structure
import written_ordinals
from layout import Column, Zone, split_columns, measure
from model import Block, BlockKind, Edition, Provenance

#: Sólo para auditoría; jamás decide geometría ni clasificación.
LATIN_HINT = ("dominus", "domine", "deus", "quoniam", "et", "in", "meus",
              "est", "non", "qui")
SPANISH_HINT = ("señor", "dios", "que", "los", "de", "su", "porque", "el",
                "la", "no")

_ZONE_KIND = {
    Zone.HEADER: BlockKind.PAGE_HEADER,
    Zone.FOOTER: BlockKind.PAGE_FOOTER,
    Zone.APPARATUS: BlockKind.FOOTNOTE,
}


def _heading_says(review):
    """Qué dijo el facsímil de ESTE bloque como RÓTULO, si alguien lo miró.

    Una revisión que leyó el numeral impreso vio el rótulo; una que
    comprobó que ahí no hay rótulo lo niega. Las demás -- numeral
    ilegible, numeral todavía ambiguo -- hablan del NÚMERO y no de la
    existencia del rótulo, así que no responden a esta pregunta y dejan
    decidir a la estructura.
    """
    if review is None:
        return None
    if review.outcome in review_outcomes.NUMERAL_RESOLVING:
        return True
    if review.outcome == review_outcomes.FALSE_CLAIM:
        return False
    return None


def _provenance(witness, volume, page, placed, printed_page=None):
    line = placed.line
    # Un evento recuperado del facsímil lleva «r» en su identificador
    # donde el reconocimiento lleva «l»: aguas abajo no puede confundirse
    # con un renglón que dijo el OCR.
    prefix = "r" if getattr(placed, "recovery", None) is not None else "l"
    return Provenance(
        witness=witness, volume=volume, page=page.scan_page,
        line=line.index, column=placed.column.value, zone=placed.zone.value,
        bbox=line.bbox, printed_page=printed_page,
        block_id=f"p{page.scan_page:04d}{prefix}{line.index:04d}")


def _block(kind, placed, prov, *, number=None, reason=None, decision=None,
           text=None):
    """`text` es el normalizado (sin la marca de versículo); el crudo del
    OCR se conserva aparte y no se sobrescribe nunca."""
    raw = placed.line.raw_text
    return Block(kind=kind, text=(raw if text is None else text),
                 provenance=prov, number=number,
                 review_reason=reason, raw_text=raw,
                 confidence=placed.line.confidence, decision=decision)


class VolumeParser:
    """Estado de lectura a lo largo de las páginas del tomo."""

    def __init__(self, edition: Edition, *, witness: str, volume: str,
                 book: str = "Ps", gutter_hint: Optional[int] = None,
                 book_spans=None, header_chapters=None,
                 image_reviews=None, recovery_source=None,
                 numeral_reviews=None):
        self.edition = edition
        self.witness = witness
        self.volume = volume
        self.book = book
        self.chapter = None
        self.verse_number = None
        self.expect_editorial = False
        self.stats = {}
        #: Canales medidos por página; su mediana sirve de respaldo para
        #: las páginas donde la medición no alcanza.
        self.gutters = []
        self.gutter_hint = gutter_hint
        #: Tramos de libro y numerales de la cabecera corrida, leídos en
        #: una pasada previa. Son las señales redundantes con que se
        #: identifica cada división.
        self.book_spans = book_spans or []
        self.header_chapters = header_chapters or {}
        self.current_book = book
        #: NO hay ancla de secuencia durante la lectura. La había, y era
        #: el fallo: una propuesta que después se rechazaba ya había
        #: servido de segunda señal a los rótulos siguientes. La
        #: secuencia se aplica ahora en chapter_claims.ClaimLedger, por
        #: rondas y sólo sobre capítulos ACEPTADOS.
        self.resolutions = []
        self.page = None
        self.division_candidates = []
        self._seen_on_page = set()
        #: Revisiones del facsímil, indexadas por plana, y la identidad
        #: del artefacto visual sobre el que se hicieron. Sin las dos
        #: cosas no se aplica ninguna: ver recovery.py.
        self.image_reviews = image_reviews or {}
        #: Lecturas del facsímil sobre rótulos que YA existen, indexadas
        #: por el bloque del reconocimiento al que apuntan. No insertan
        #: nada: le dan a un reclamo el numeral que la máquina no supo
        #: leer, y con él la autoridad de la imagen.
        self.numeral_reviews = numeral_reviews or {}
        self.recovery_source = recovery_source
        self.recoveries = []
        #: Todos los rótulos que dicen ser un capítulo, conservados antes
        #: de que ninguno se escriba. Ver chapter_claims.py: un
        #: diccionario indexado por número no puede representar dos
        #: reclamantes de la misma clave, así que la comprobación tiene
        #: que ocurrir aquí y no después.
        self.ledger = chapter_claims.ClaimLedger()
        self._slots = {}
        self._blocks_by_claim = {}

    def _bump(self, key, amount=1):
        self.stats[key] = self.stats.get(key, 0) + amount

    def _close_verse(self, why):
        """Cierra el versículo abierto. Todo lo que no sea continuación
        canónica pasa por aquí antes de que se escriba nada más."""
        if self.verse_number is not None:
            self._bump("verse_closed_" + why)
        self.verse_number = None

    def _queue(self, block):
        self.edition.review_queue.append(block)
        self._bump("unclassified")

    def _hint(self):
        if self.gutters:
            ordered = sorted(self.gutters)
            return ordered[len(ordered) // 2]
        return self.gutter_hint

    def _book_for(self, page):
        if not self.book_spans:
            return self.book
        found = structure.book_at(self.book_spans, page.scan_page)
        return found or self.book

    def feed_page(self, page):
        self.page = page
        book = self._book_for(page)
        if book != self.current_book:
            # Cambio de libro: se cierra lo abierto. Ni un versículo ni un
            # capítulo puede cruzar la frontera.
            self._close_verse("book_change")
            self.chapter = None
            self.current_book = book
            self._bump("book_boundaries")
        self._seen_on_page = set()
        hint = self._hint()
        layout = measure(page, gutter_hint=hint)
        if layout.gutter is not None:
            self.gutters.append(layout.gutter)
        placed_lines = split_columns(page, gutter_hint=hint)
        self._bump("pages")
        self._bump("ocr_blocks", len(placed_lines))
        self._bump("gutter_found", 1 if layout.gutter is not None else 0)

        # Aquí, y sólo aquí, entra lo que se leyó en el facsímil: la
        # plana ya está parseada y colocada en orden de lectura, y
        # todavía no se ha resuelto ninguna división. El recuento de
        # `ocr_blocks` se ha tomado ANTES, de modo que un evento
        # recuperado no puede disfrazarse de bloque de reconocimiento.
        reviews = self.image_reviews.get(page.scan_page)
        if reviews and self.recovery_source is not None:
            def resolve_numeral(raw, book=book, page=page):
                # ¿Afirma este rótulo un número por sí mismo? Sólo con lo
                # que hay en la plana: su numeral y la cabecera corrida.
                # Sin secuencia: durante la lectura todavía no hay ningún
                # capítulo aceptado en que apoyarse, y usar una propuesta
                # sería justo lo que esta corrección viene a quitar.
                return structure.resolve_chapter(
                    raw_numeral=raw,
                    header_chapters=self.header_chapters.get(page.scan_page, []),
                    previous=None, book=book).resolved

            placed_lines, applications = \
                image_recovery.apply_verified_image_reviews(
                    page, placed_lines, reviews,
                    source=self.recovery_source, book=book,
                    resolve_numeral=resolve_numeral)
            # Sin `claimed_numbers`: la guarda temprana de colisión que
            # esta capa traía dejó de hacer falta y estorbaba. Cada
            # reclamo abre ahora su propio hueco de trabajo, así que dos
            # capítulos no pueden fundirse por escribir antes de tiempo;
            # y quién se queda con un número disputado lo decide el
            # ledger, que ve a TODOS los reclamantes -- también los que
            # vienen en planas posteriores, que una guarda en streaming
            # no puede ver.  La lectura del facsímil ya no pierde el
            # número por llegar la segunda.
            for record in applications:
                self._bump("image_review_" + record.action)
                self.recoveries.append(record)

        for placed in placed_lines:
            self._bump("column_" + placed.column.value)
            prov = _provenance(self.witness, self.volume, page, placed)

            # --- rótulo leído en la imagen ----------------------------
            # No pasa por la resolución de capítulo: su numeral no es una
            # inferencia que haya que corroborar, es una lectura directa
            # del impreso. Y si la imagen no dejó leer el numeral, sigue
            # sin número: la secuencia no lo completa.
            if getattr(placed, "recovery", None) is not None:
                self._open_recovered_chapter(placed, prov)
                continue

            # --- fuera del cuerpo: cabecera, pie, notas ---------------
            if placed.zone in _ZONE_KIND:
                kind = _ZONE_KIND[placed.zone]
                if classifier.carries_division_marker(placed.line.raw_text):
                    # Un rótulo de división en la cabecera corrida o en
                    # el aparato NO es una frontera: es la propia
                    # cabecera de la plana repitiendo el capítulo.
                    verdict = divisions.judge(
                        raw_text=placed.line.raw_text, column=placed.column,
                        zone=placed.zone, bbox=placed.line.bbox,
                        page_width=page.width)
                    self.division_candidates.append({
                        "page": page.scan_page, "block_id": prov.block_id,
                        "bbox": list(placed.line.bbox),
                        "raw": placed.line.raw_text[:100],
                        "column": placed.column.value, "zone": placed.zone.value,
                        "classification": verdict.classification.value,
                        "signals": verdict.signals,
                        "rejections": verdict.rejections,
                        "score": verdict.score, "review_required": False})
                    self._bump("division_" + verdict.classification.value)
                # La cabecera corrida, el pie y las notas son mobiliario
                # de la plana: no entran en el versículo, pero tampoco
                # son una frontera. Cerrar aquí rompía la continuidad
                # legítima de un versículo entre dos páginas. Lo que sí
                # cierra un versículo es una frontera estructural:
                # división, argumento del editor o columna indecidible.
                block = _block(kind, placed, prov, decision="zone")
                self._bump(kind.value.lower())
                if self.chapter is not None:
                    self.chapter.paratext.append(block)
                else:
                    self.edition.book(self.current_book).chapter(0).paratext.append(block)
                continue

            # --- bloque que cruza el canal ----------------------------
            if placed.column is Column.SPANNING:
                self._handle_spanning(placed, prov)
                continue

            if placed.column is Column.UNKNOWN:
                # Ni columna ni frontera demostrable: a revisión.
                self._close_verse("unknown_column")
                self._queue(_block(BlockKind.UNCLASSIFIED, placed, prov,
                                   reason="line crosses the gutter without "
                                          "spanning it; column undecidable",
                                   decision="geometry-undecidable"))
                continue

            # --- latín paralelo ---------------------------------------
            if placed.column is Column.LEFT:
                block = _block(BlockKind.PARAGRAPH, placed, prov,
                               decision="parallel-latin")
                self._bump("latin_lines")
                if classifier.looks_like_division(placed.line.raw_text):
                    self._bump("latin_division_marker")
                if self.chapter is not None:
                    self.chapter.parallel_latin.append(block)
                continue

            # --- español canónico -------------------------------------
            self._handle_spanish(placed, prov)

    def _handle_spanning(self, placed, prov):
        """Un bloque a dos columnas nunca continúa un versículo.

        Por su caja ya se sabe que no pertenece a una sola columna, así
        que aquí no cabe la decisión que arruinó la edición de 1882.
        """
        raw = placed.line.raw_text
        self._close_verse("spanning")
        verdict = divisions.judge(
            raw_text=raw, column=placed.column, zone=placed.zone,
            bbox=placed.line.bbox, page_width=self.page.width,
            header_chapters=self.header_chapters.get(self.page.scan_page),
            seen_on_page=self._seen_on_page)
        if verdict.signals and "division_word" in verdict.signals:
            self.division_candidates.append({
                "page": self.page.scan_page, "block_id": prov.block_id,
                "bbox": list(placed.line.bbox), "raw": raw[:100],
                "column": placed.column.value, "zone": placed.zone.value,
                "classification": verdict.classification.value,
                "signals": verdict.signals, "rejections": verdict.rejections,
                "score": verdict.score,
                "review_required": verdict.review_required,
            })
            self._bump("division_" + verdict.classification.value)
        if verdict.is_boundary:
            self._seen_on_page.add(raw.strip())
        if verdict.is_boundary:
            self._open_chapter(placed, prov, raw)
            return
        block = _block(BlockKind.EDITORIAL_HEADING, placed, prov,
                       decision="spans the gutter")
        self._bump("editorial_headings")
        if self.chapter is not None:
            self.chapter.paratext.append(block)
        else:
            self.edition.book(self.current_book).chapter(0).paratext.append(block)

    def _open_chapter(self, placed, prov, raw):
        """Anota lo que este rótulo RECLAMA ser. Todavía no lo escribe.

        Llevaba un parámetro `number` con el numeral que había sacado el
        clasificador y que aquí no se miraba nunca: se resolvía otra vez
        desde el texto. Se ha quitado, porque un número que viaja sin que
        nadie lo use es un número esperando a que alguien lo use.

        El numeral se lee y se VALIDA (roman.py), y el resultado -- con
        su estado, su procedencia y sus candidatos -- se guarda como un
        reclamo. Quién se queda con cada número lo decide el ledger
        cuando ya están todos, no aquí.
        """
        book = self.current_book
        page = self.page.scan_page if self.page is not None else None
        numeral = structure.roman_reading(
            structure._without_book_words(raw or ""),
            limit=structure.chapter_limit(book))
        self._bump("chapters_claimed")

        # ¿Es esto un rótulo? El clasificador ha visto la PALABRA; que el
        # impreso componga aquí una división lo dicen la plana y la forma
        # del renglón. Un renglón que la lleva dentro de una frase --la
        # inscripción del salmo, por ejemplo-- se conserva como reclamo
        # con su evidencia, pero no abre capítulo ninguno.
        review = self.numeral_reviews.get(prov.block_id)
        heading = heading_validity.judge(
            raw_text=raw, column=placed.column, zone=placed.zone,
            bbox=placed.line.bbox,
            page_width=self.page.width if self.page is not None else None,
            header_chapters=self.header_chapters.get(page),
            image_review_outcome=_heading_says(review))
        if not heading.is_heading:
            # No se cuenta aquí: `finish` ya cuenta una vez por
            # disposición, y dos contadores con el mismo nombre daban
            # doce rechazos donde hay seis.
            claim = self._claim(
                book=book, page=page, prov=prov, placed=placed, raw=raw,
                source=chapter_claims.FROM_OCR, numeral=numeral,
                candidates=list(numeral.candidates),
                proposed=None, method=chapter_claims.UNRESOLVED,
                evidence={}, confidence=0.0, heading=heading.as_dict())
            # El renglón sale a revisión. Y queda por decidir qué pasa
            # con lo que venga DETRÁS, que es donde está el peligro.
            #
            # Estas líneas son inscripciones de salmo («Salmo de David,
            # cuando le perseguía su hijo Absalón»), y el impreso las
            # pone en dos sitios distintos:
            #
            #   tras un rótulo que sí se leyó -- el capítulo está
            #   abierto y todavía no ha tomado ningún versículo: la
            #   inscripción es el principio de SU texto y sigue siendo
            #   suyo;
            #
            #   en medio del cuerpo -- el capítulo abierto ya tiene
            #   versículos: entonces aquí empieza un salmo cuyo rótulo
            #   el reconocimiento NO produjo, y seguir echando lo que
            #   venga en el capítulo anterior lo mezclaría en silencio.
            #   Eso es el fallo de 1882 con otro disfraz, así que se
            #   cierra y el texto espera en la cola con su procedencia
            #   hasta que una recuperación de frontera diga qué división
            #   falta.
            if self.chapter is not None and self.chapter.verses:
                self._close_verse("not_a_heading")
                self.chapter = None
            # El bloque se encola AQUÍ y no se registra en
            # `_blocks_by_claim`: si se registrara, `finish` lo volvería
            # a encolar al repasar los reclamos sin número y la misma
            # línea saldría dos veces en la cola de revisión.
            self._queue(_block(
                BlockKind.UNCLASSIFIED, placed, prov,
                reason=(f"{chapter_claims.REJECTED_FALSE_HEADING}: carries a "
                        f"division word but is not a chapter heading: "
                        + (heading.rejection_reason or "")),
                decision="not-a-heading"))
            return
        if heading.review_required:
            self._bump("chapters_heading_review_required")

        # ¿Ha mirado alguien esta plana? Si el facsímil dice qué numeral
        # lleva este rótulo, el reclamo nace con la autoridad de la
        # imagen y con su procedencia. El texto crudo del reconocimiento
        # sigue siendo el suyo: lo que cambia es de dónde sale el número.
        if review is not None and review.resolves:
            self._bump("chapters_numeral_reviewed")
            provenance = {
                "source": image_recovery.RECOVERED,
                "review_id": review.id, "kind": "numeral_review",
                "outcome": review.outcome,
                "witness": (self.recovery_source.witness
                            if self.recovery_source else None),
                "source_sha256": (self.recovery_source.sha256
                                  if self.recovery_source else None),
                "scan_page": review.scan_page, "pdf_page": review.pdf_page,
                "printed_page": review.printed_page,
                "bbox": list(review.bbox) if review.bbox else None,
                "raw_ocr_heading": review.raw_heading,
                "raw_ocr_numeral": review.raw_numeral,
                "observed_printed_text": review.observed_printed_text,
                "observed_printed_numeral": review.observed_printed_numeral,
                "confidence": review.confidence,
                "reviewer_method": review.reviewer_method,
            }
            claim = self._claim(
                book=book, page=page, prov=prov, placed=placed, raw=raw,
                source=chapter_claims.FROM_IMAGE_REVIEW, numeral=numeral,
                candidates=list(numeral.candidates),
                proposed=review.recovered_chapter, method="numeral_review",
                evidence={"review_id": review.id, "outcome": review.outcome,
                          "observed": review.observed_printed_numeral,
                          "raw_ocr_numeral": review.raw_numeral},
                confidence=review.confidence, provenance=provenance,
                recovered=True, heading=heading.as_dict())
            block = _block(BlockKind.CHAPTER_HEADING, placed, prov,
                           number=None,
                           decision="numeral_review:" + review.id)
            block.recovered = provenance
            self._open_claim_chapter(claim, block)
            return

        claim = self._claim(
            book=book, page=page, prov=prov, placed=placed, raw=raw,
            source=chapter_claims.FROM_OCR, numeral=numeral,
            candidates=list(numeral.candidates),
            proposed=None, method=chapter_claims.UNRESOLVED,
            evidence={}, confidence=0.0, heading=heading.as_dict())

        block = _block(BlockKind.CHAPTER_HEADING, placed, prov,
                       number=None, decision="claimed")
        self._open_claim_chapter(claim, block)

    def _open_recovered_chapter(self, placed, prov):
        """Abre una división que el reconocimiento no dejó legible.

        El número viene del numeral impreso leído en la imagen y de
        ningún otro sitio. Si la revisión confirmó la frontera pero no el
        numeral, el capítulo queda sin número y marcado para revisión:
        exactamente igual que una división del OCR que no se pudo
        identificar. Una frontera sin número sigue siendo una frontera, y
        eso es lo que impide que el texto se corra.
        """
        recovery = placed.recovery
        book = self.current_book
        page = self.page.scan_page if self.page is not None else None
        number = recovery.chapter_number
        raw = getattr(placed.line, "raw_text", "") or ""

        self._bump("chapters_recovered_" + recovery.action)
        provenance = recovery.provenance.as_dict()
        # Qué dijo la máquina en ESTE renglón, al lado de lo que dice la
        # plana. Cuando la recuperación marca un renglón que el
        # reconocimiento sí produjo -- con la palabra de división rota --
        # las dos lecturas existen, y el informe tiene que poder
        # enseñarlas juntas. Vacío cuando el renglón es sintético porque
        # el reconocimiento no dejó nada.
        provenance["raw_ocr_heading"] = raw
        observed = recovery.provenance.observed_printed_text or raw
        numeral = structure.roman_reading(
            structure._without_book_words(observed or ""),
            limit=structure.chapter_limit(book))
        claim = self._claim(
            book=book, page=page, prov=prov, placed=placed,
            raw=observed, source=chapter_claims.FROM_IMAGE_REVIEW,
            numeral=numeral.as_dict(), candidates=list(numeral.candidates),
            proposed=number, method="image_review",
            evidence={"review_id": recovery.review_id,
                      "action": recovery.action,
                      "witness": recovery.provenance.witness,
                      "source_sha256": recovery.provenance.source_sha256,
                      "pdf_page": recovery.provenance.pdf_page,
                      "printed_page": recovery.provenance.printed_page,
                      "observed": recovery.provenance.observed_printed_text},
            confidence=recovery.provenance.confidence,
            provenance=provenance, recovered=True,
            # La frontera la vio una persona en la imagen: eso ES la
            # evidencia estructural, y no se vuelve a juzgar por la
            # geometría de un renglón que el reconocimiento no produjo.
            heading=heading_validity.judge(
                raw_text=observed or raw, column=placed.column,
                zone=placed.zone, bbox=placed.line.bbox,
                page_width=self.page.width if self.page is not None else None,
                image_review_outcome=True).as_dict())

        block = _block(BlockKind.CHAPTER_HEADING, placed, prov, number=None,
                       decision="image_review:" + recovery.review_id)
        block.recovered = provenance
        self._open_claim_chapter(claim, block)

    def finish(self):
        """Cierra la pasada: resolver los reclamos y sólo entonces escribir.

        Este es el único punto donde un número de capítulo llega a la
        estructura final. Todo lo anterior son reclamos con su evidencia.
        """
        # La política de numeración vive en structure.resolve_chapter y
        # se le entrega al ledger, que decide CUÁNDO llamarla y con qué
        # ancla. `previous` es siempre un capítulo ya aceptado o None:
        # esa es la garantía que esta función existe para dar.
        def propose(claim, previous):
            resolution = structure.resolve_chapter(
                raw_numeral=claim.raw_heading,
                header_chapters=self.header_chapters.get(claim.scan_page, []),
                previous=previous, book=claim.book,
                sequence_available=claim.first_in_book)
            ordinal = claim.ordinal_value
            if ordinal is None:
                return chapter_claims.Proposal(
                    resolution.resolved, resolution.method,
                    dict(resolution.evidence), resolution.confidence)

            # El rótulo trae el número escrito con palabra, leído entero
            # y sin enmendar ninguna letra.
            evidence = dict(resolution.evidence,
                            ordinal_token=claim.ordinal.get("normalized_token"),
                            ordinal_raw=claim.ordinal.get("raw_token"),
                            ordinal_value=ordinal,
                            why=claim.ordinal.get("reason"))
            # «Evidencia romana» es un numeral VALIDADO, no un candidato
            # que se le parezca: lo que el propio reclamo trae leído.
            roman_value = (claim.numeral.get("value")
                           if claim.numeral.get("status") == roman.VALID
                           else None)
            if roman_value is not None and roman_value != ordinal:
                # Dos sistemas de numeración en el mismo rótulo diciendo
                # cosas distintas. No se elige el que más guste: se para
                # y se deja dicho, que es recuperable. Sólo una revisión
                # del facsímil de ESTE rótulo puede desempatar.
                return chapter_claims.Proposal(
                    None, chapter_claims.ORDINAL_CONFLICT,
                    dict(evidence, roman_value=roman_value,
                         why=(f"the heading carries a Roman numeral reading "
                              f"{roman_value} and the written ordinal "
                              f"{ordinal}; nothing in the text decides "
                              f"between them")),
                    0.0)
            # La palabra es la evidencia, y se sostiene sola: no hace
            # falta que la secuencia ni la cabecera la corroboren, igual
            # que no hacen falta para un romano que se lee limpio y
            # concuerda. Lo que sí hace falta es que el renglón sea un
            # rótulo, y de eso responde `is_structural_heading`.
            return chapter_claims.Proposal(
                ordinal, chapter_claims.WRITTEN_ORDINAL, evidence,
                claim.ordinal.get("confidence") or 0.95)

        def propose_or_review(claim, previous):
            # Una revisión del facsímil trae su propio número leído en la
            # imagen; no se vuelve a deducir de nada.
            if claim.source == chapter_claims.FROM_IMAGE_REVIEW:
                return chapter_claims.Proposal(
                    claim.proposed_number, claim.proposal_method,
                    dict(claim.proposal_evidence), claim.confidence)
            return propose(claim, previous)

        self.ledger.resolve(propose_or_review)
        self._bump("claim_resolution_rounds", self.ledger.rounds)
        outcome = chapter_claims.materialize(self.edition, self.ledger)
        self._bump("chapters_materialized", outcome["materialized"])
        self._bump("chapters_left_in_review", outcome["left_in_review"])

        by_claim = {r["claim_id"]: r for r in self.resolutions
                    if r.get("claim_id")}
        for claim in self.ledger.claims:
            block = self._blocks_by_claim.get(claim.claim_id)
            if block is not None:
                block.number = claim.accepted_number
                block.review_reason = (
                    None if not claim.review_required
                    else f"{claim.disposition}: {claim.reason}")
                if claim.review_required:
                    self.edition.review_queue.append(block)
            record = by_claim.get(claim.claim_id)
            if record is not None:
                record["resolved_number"] = claim.accepted_number
                record["review_required"] = claim.review_required
                record["disposition"] = claim.disposition
                record["disposition_reason"] = claim.reason
                record["accepted_from"] = claim.source
                record["method"] = claim.proposal_method
                record["evidence"] = dict(claim.proposal_evidence)
                record["confidence"] = claim.confidence
                record["parsed_candidate"] = claim.proposed_number
                record["accepted_round"] = claim.accepted_round
                record["sequence_anchor"] = claim.anchor_number
                record["sequence_anchor_claim"] = claim.anchor_claim
                # `method` dice CÓMO se propuso el número y `disposition`
                # QUÉ se decidió con él. Machacar el primero con el
                # segundo borraba de dónde venía el reclamo, que es
                # justamente lo que hay que poder auditar.
            key = ("chapters_accepted_" + claim.source
                   if claim.disposition == chapter_claims.ACCEPTED
                   else "chapters_" + claim.disposition)
            self._bump(key)
            if claim.disposition == chapter_claims.ACCEPTED:
                self._bump(f"chapters_accepted_round_{claim.accepted_round}")
        return self.ledger

    def _claim(self, *, book, page, prov, placed, raw, source, numeral,
               candidates, proposed, method, evidence, confidence,
               provenance=None, recovered=False, heading=None, ordinal=None):
        """Anota un reclamo. No escribe todavía ningún capítulo."""
        reading = numeral if isinstance(numeral, dict) else numeral.as_dict()
        # El número puede estar escrito con palabra en vez de con
        # numeral. Se lee SIEMPRE, también cuando hay romano: es la
        # única forma de poder decir después que los dos no coinciden.
        if ordinal is None:
            ordinal = written_ordinals.read(
                structure._without_book_words(raw or "")).as_dict()
        claim = chapter_claims.ChapterClaim(
            claim_id=f"c{len(self.ledger.claims):04d}:{prov.block_id}",
            book=book, scan_page=page, block_id=prov.block_id,
            raw_heading=raw or "", source=source,
            bbox=tuple(placed.line.bbox) if placed is not None else None,
            column=prov.column, zone=prov.zone,
            numeral=reading, ordinal=ordinal, heading=heading,
            candidate_numbers=candidates,
            proposed_number=proposed, proposal_method=method,
            proposal_evidence=dict(evidence or {}), confidence=confidence,
            provenance=provenance)
        self.ledger.add(claim)
        self.resolutions.append({
            "book": book, "scan_page": page,
            "claim_id": claim.claim_id,
            "source_heading": (raw or "")[:120],
            # `raw_numeral` conserva el sentido que tenía: el texto del
            # rótulo tal y como entró al resolver. El token concreto que
            # se intentó leer va aparte, para no cambiarle el significado
            # a un campo que ya tiene consumidores.
            "raw_numeral": (raw or "")[:60],
            "numeral_token": reading.get("token"),
            "numeral_status": reading.get("status"),
            "numeral_permissive_value": reading.get("permissive_value"),
            "parsed_candidate": proposed,
            "resolved_number": None,          # lo fija la materialización
            "method": method,
            "evidence": dict(evidence or {}),
            "confidence": confidence,
            "review_required": True,          # idem
            "recovered": recovered,
            "block_id": prov.block_id,
        })
        return claim

    def _open_claim_chapter(self, claim, block):
        """Abre el capítulo de un reclamo en un hueco de trabajo.

        El hueco es SIEMPRE negativo y único, aunque el numeral se haya
        leído sin dudas: mientras el ledger no haya visto a todos los
        reclamantes, escribir en `capitulos[número]` es justo lo que
        impide detectar que dos rótulos piden el mismo.
        """
        book = claim.book
        self._slots[book] = self._slots.get(book, 0) + 1
        claim.slot = -self._slots[book]
        self.chapter = self.edition.book(book).chapter(claim.slot)
        self.chapter.paratext.append(block)
        self._blocks_by_claim[claim.claim_id] = block
        self.verse_number = None
        self.expect_editorial = True

    def _handle_spanish(self, placed, prov):
        raw = placed.line.raw_text
        block = classifier.classify(raw, prov,
                                    expect_editorial=self.expect_editorial)
        self.expect_editorial = False

        if block.kind is BlockKind.CHAPTER_HEADING:
            self._open_chapter(placed, prov, raw)
            return

        if self.chapter is None:
            self._queue(_block(BlockKind.UNCLASSIFIED, placed, prov,
                               reason="text before any chapter division",
                               decision="no-chapter-open"))
            return

        if block.kind is BlockKind.EDITORIAL_HEADING:
            self._close_verse("editorial")
            self._bump("editorial_headings")
            self.chapter.paratext.append(
                _block(BlockKind.EDITORIAL_HEADING, placed, prov,
                       decision="under a division"))
            return

        if block.kind is BlockKind.VERSE:
            self.verse_number = block.number
            self._bump("spanish_verse_starts")
            self.chapter.verse(self.verse_number).blocks.append(
                _block(BlockKind.VERSE, placed, prov, number=block.number,
                       decision="numbered line", text=block.text))
            return

        # Sin número. Sólo continúa si hay un versículo abierto en esta
        # misma columna y banda; si no, a revisión.
        if self.verse_number is not None:
            self._bump("continuations")
            self.chapter.verse(self.verse_number).blocks.append(
                _block(BlockKind.VERSE, placed, prov,
                       number=self.verse_number,
                       decision="continuation of the open verse"))
            return

        self._queue(_block(BlockKind.UNCLASSIFIED, placed, prov,
                           reason="unnumbered line with no open verse",
                           decision="no-open-verse"))


def parse_volume(pages: Iterable, *, witness: str, volume: str,
                 book: str = "Ps", edition: Optional[Edition] = None,
                 gutter_hint: Optional[int] = None, book_spans=None,
                 header_chapters=None, with_walker: bool = False,
                 image_reviews=None, recovery_source=None,
                 numeral_reviews=None):
    """Recorre las páginas y devuelve (edición, métricas)."""
    edition = edition or Edition(edition_id="TorresAmat1835")
    walker = VolumeParser(edition, witness=witness, volume=volume, book=book,
                          gutter_hint=gutter_hint, book_spans=book_spans,
                          header_chapters=header_chapters,
                          image_reviews=image_reviews,
                          recovery_source=recovery_source,
                          numeral_reviews=numeral_reviews)
    for page in pages:
        walker.feed_page(page)
    walker.finish()
    if with_walker:
        return edition, walker.stats, walker
    return edition, walker.stats
