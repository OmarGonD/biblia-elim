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
import a_glyph_pixel_recovery
import compound_glyphs
import divisions
import heading_validity
import image_reviews as review_outcomes
import parser as classifier
import projected_form_a_recovery
import recovery as image_recovery
import roman
import structure
import verse_markers
import written_ordinals
from layout import Column, Zone, split_columns, measure
from model import Block, BlockKind, Edition, Provenance

#: Sólo para auditoría; jamás decide geometría ni clasificación.
LATIN_HINT = ("dominus", "domine", "deus", "quoniam", "et", "in", "meus",
              "est", "non", "qui")
SPANISH_HINT = ("señor", "dios", "que", "los", "de", "su", "porque", "el",
                "la", "no")

#: Las decisiones que significan «este renglón declara un número de
#: versículo». Son varias porque el número puede haber llegado por
#: caminos distintos --limpio, enmarcado, o partido en dos glifos-- y
#: todas cuentan igual a la hora de preguntar cuántos renglones
#: numerados tiene un versículo.
_NUMBERED_DECISIONS = frozenset({"numbered line", "compound_glyph_marker",
                                 projected_form_a_recovery.DECISION})

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
                 numeral_reviews=None, compound_recovery: bool = True,
                 a_glyph_recovery: Optional[bool] = None,
                 zero_anchor_io_recovery: bool = True,
                 a_glyph_evidence=None,
                 projected_form_a_recovery: bool = True):
        self.compound_recovery = compound_recovery
        self.zero_anchor_io_recovery = zero_anchor_io_recovery
        #: Task 146. Es un respaldo: sólo actúa con las recuperaciones
        #: más fuertes encendidas, y sólo sobre lo que ellas no tomaron.
        self.projected_form_a_recovery = (bool(projected_form_a_recovery)
                                          and compound_recovery)
        self.a_glyph_recovery = (compound_recovery if a_glyph_recovery is None
                                 else a_glyph_recovery)
        if a_glyph_evidence is not None:
            self.a_glyph_evidence = a_glyph_evidence
        elif (str(volume) != a_glyph_pixel_recovery.SOURCE_VOLUME
              or witness != a_glyph_pixel_recovery.SOURCE_WITNESS):
            self.a_glyph_evidence = a_glyph_pixel_recovery.EvidenceIndex.invalid(
                "parser_source_provenance_mismatch")
        else:
            self.a_glyph_evidence = a_glyph_pixel_recovery.default_index()
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
        #: Marcadores de versículo que el canon nativo desmiente. Se
        #: llenan al materializar, que es cuando el capítulo tiene
        #: número.
        self.impossible_markers = []
        #: Marcadores aceptados sólo tras quitar la puntuación exterior.
        self.framed_markers = []
        #: Numerales de dos cifras que el reconocimiento partió en dos
        #: palabras («I o» por 10). Se anota SIEMPRE lo que el matcher
        #: ve, se aplique o no: con `compound_recovery` apagado la lista
        #: es una simulación, y con él encendido es el registro de lo que
        #: se hizo. Una sola ruta de código para las dos cosas.
        self.compound_markers = []
        self.compound_rejections = []
        self.a_glyph_candidates = []
        #: Task 146: renglones que pasan el discriminador de la 142 tras
        #: perder contra todas las rutas más fuertes. Se anotan siempre;
        #: el desenlace (recuperado o por qué se abstuvo) lo pone
        #: `finish`, que es donde las guardas de la 144 son evaluables.
        self.projected_form_a_candidates = []
        self.projected_form_a_rejections = {}
        self._form_a_blocks = {}
        self._form_a_geometry = None
        self._band = None
        self._band_anchor_counts = None
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
        self._form_a_geometry = None
        hint = self._hint()
        layout = measure(page, gutter_hint=hint)
        if layout.gutter is not None:
            self.gutters.append(layout.gutter)
        placed_lines = split_columns(page, gutter_hint=hint)
        # La banda de sangría del marcador, medida en ESTA plana sobre
        # sus propios marcadores numéricos ordinarios. Se calcula por
        # columna y sólo con el cuerpo: la cabecera, el pie y las notas
        # tienen otra caja y mezclarlas movería el centro.
        self._band = {}
        self._band_anchor_counts = {}
        by_column = {}
        for placed in placed_lines:
            if placed.zone is Zone.BODY:
                by_column.setdefault(placed.column, []).append(placed.line)
        for column, lines in by_column.items():
            self._band[column] = compound_glyphs.band_of(lines)
            self._band_anchor_counts[column] = compound_glyphs.trusted_anchor_count(lines)
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

    def _try_compound(self, placed, prov, block):
        """Mira si el renglón abre versículo con un numeral partido.

        Anota siempre lo que ve. Sólo construye el bloque si la
        recuperación está encendida: con ella apagada esto es la
        simulación que pide medir antes de tocar nada.
        """
        self._compound_block = None
        band = (self._band or {}).get(placed.column)
        value, rest, detail = compound_glyphs.match(placed.line, band)
        anchor_count = (self._band_anchor_counts or {}).get(placed.column, 0)
        record = {
            "page": self.page.scan_page, "block_id": prov.block_id,
            "column": placed.column.value, "zone": placed.zone.value,
            "raw": placed.line.raw_text[:90], "book": self.current_book,
            "chapter_slot": getattr(self.chapter, "number", None),
        }
        record.update(detail)
        record["trusted_anchor_count"] = anchor_count
        record["zero_anchor_io_recovery"] = False
        if value is None and (detail.get("form") or "").startswith("a "):
            value, rest, pixel = self._try_a_glyph(
                placed, prov, band, detail)
            record.update(pixel)
        if (value is None and rest == compound_glyphs.NO_BAND
                and self.compound_recovery
                and self.zero_anchor_io_recovery
                and anchor_count == 0
                and detail.get("form") == "I o"
                and placed.zone is Zone.BODY
                and placed.column is Column.RIGHT):
            words = placed.line.words
            first, framing = compound_glyphs.marker_tokens(words)
            if first is not None and framing is None and first + 1 < len(words):
                candidate_text = " ".join(
                    word.text for word in words[first + 2:]).strip()
                if candidate_text and not candidate_text[:1].isdigit():
                    # `detail["value"]` is the already validated compound
                    # interpretation.  It is consulted only behind this
                    # exact zero-anchor, source-derived fallback gate.
                    value, rest = detail["value"], candidate_text
                    detail["recovery"] = "task139_zero_anchor_io"
                    record["zero_anchor_io_recovery"] = True
                else:
                    record["zero_anchor_io_rejection"] = (
                        "no_verse_text_after_marker" if not candidate_text
                        else "digit_follows_marker")
        if value is None:
            # Sin forma conocida no hay nada que contar: la inmensa
            # mayoría de los renglones del tomo caen aquí y llenarían el
            # informe de ruido.
            if rest not in (compound_glyphs.NO_TOKENS,
                            compound_glyphs.NOT_IN_MAP):
                record["reason"] = rest
                self.compound_rejections.append(record)
            return
        record["value"] = value
        if detail.get("value") is None:
            detail["value"] = value
        record["text_head"] = rest[:60]
        record["applied"] = bool(self.compound_recovery)
        self.compound_markers.append(record)
        if not self.compound_recovery:
            return
        if self.chapter is None:
            record["applied"] = False
            record["reason"] = "no_chapter_open"
            return
        self._compound_block = classifier.Block(
            BlockKind.VERSE, rest, prov, number=value,
            decision="compound_glyph_marker")

    def _try_a_glyph(self, placed, prov, band, detail):
        """Interpreta ``a *`` desde metadata ya medida; jamás desde huecos."""
        words = placed.line.words
        first, framing = compound_glyphs.marker_tokens(words)
        record = {
            "block_id": prov.block_id, "raw": placed.line.raw_text[:90],
            "scan_page": self.page.scan_page,
            "pdf_page": self.page.scan_page + 1,
            "book": self.current_book,
            "chapter_slot": getattr(self.chapter, "number", None),
            "form": detail.get("form"), "zone": placed.zone.value,
            "column": placed.column.value, "pixel_status": None,
            "first_digit_decision": None, "second_token": None,
            "second_digit": None, "candidate": None, "geometry": None,
            "framing": framing or "valid", "final_outcome": None,
        }
        if first is None or first + 1 >= len(words):
            record["final_outcome"] = framing or compound_glyphs.NO_TOKENS
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        one, two = words[first].text, words[first + 1].text
        record["second_token"] = two
        if one != "a" or len(two) != 1:
            record["final_outcome"] = "not_exact_a_compound"
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        evidence, decision, problem = self.a_glyph_evidence.lookup(
            prov.block_id, scan_page=self.page.scan_page,
            form=f"{one} {two}", column=placed.column.value,
            zone=placed.zone.value, first_bbox=words[first].bbox,
            second_bbox=words[first + 1].bbox)
        record["pixel_status"] = problem or "valid"
        record["first_digit_decision"] = decision
        if evidence:
            record.update({"ink_width": evidence.ink_width,
                           "ink_aspect": evidence.ink_aspect,
                           "ink_pixels": evidence.ink_pixels})
        if decision not in (a_glyph_pixel_recovery.PRINTED_1,
                            a_glyph_pixel_recovery.PRINTED_2):
            record["final_outcome"] = decision.lower()
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        second = a_glyph_pixel_recovery.second_digit(two)
        record["second_digit"] = second
        if second is None:
            record["final_outcome"] = a_glyph_pixel_recovery.UNSAFE_SECOND_TOKEN
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        first_digit = 1 if decision == a_glyph_pixel_recovery.PRINTED_1 else 2
        value = first_digit * 10 + second
        record["candidate"] = value
        # Reutiliza, sin rebajarla, la geometría y el encuadre de la 128.
        box = compound_glyphs.marker_bbox(words, first)
        if band is None:
            record["geometry"] = compound_glyphs.NO_BAND
            record["final_outcome"] = compound_glyphs.NO_BAND
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        if abs(box[0] - band[0]) > compound_glyphs.tolerance(band):
            record["geometry"] = compound_glyphs.OUT_OF_BAND
            record["final_outcome"] = compound_glyphs.OUT_OF_BAND
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        record["geometry"] = "valid"
        rest = " ".join(word.text for word in words[first + 2:]).strip()
        if not rest:
            record["final_outcome"] = compound_glyphs.NO_TEXT
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        if rest[:1].isdigit():
            record["final_outcome"] = compound_glyphs.DIGIT_FOLLOWS
            self.a_glyph_candidates.append(record)
            return None, record["final_outcome"], record
        record["final_outcome"] = ("would_create_marker" if
                                   not self.a_glyph_recovery else "applied")
        record["applied"] = bool(self.a_glyph_recovery)
        self.a_glyph_candidates.append(record)
        if not self.a_glyph_recovery:
            return None, "a_glyph_recovery_disabled", record
        detail.update({"form": f"{one} {two}", "value": value,
                       "marker_bbox": list(box), "marker_x0": box[0],
                       "band_center": band[0], "indent": box[0] - band[0],
                       "tolerance": compound_glyphs.tolerance(band)})
        return value, rest, record

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

        # Ahora --y no antes-- cada capítulo aceptado sabe su número, así
        # que ya se puede decir qué marcadores de versículo no puede
        # tener. Un «196» en un capítulo de veintisiete versículos no es
        # una frontera: su texto pasa al versículo anterior y el número
        # se queda anotado como lo que era, una lectura imposible. Esto
        # forma parte de construir la edición, no es un arreglo posterior
        # sobre las referencias.
        self.impossible_markers = verse_markers.enforce_edition(
            self.edition, verse_limit=structure.verse_limit)
        self._bump("verse_markers_rejected_impossible",
                   len(self.impossible_markers))
        # Task 146: sus guardas (procedencia del hueco proyectado y
        # progresión nativa) sólo existen sobre la edición materializada.
        projected_form_a_recovery.apply(
            self.edition, self.projected_form_a_candidates,
            self._form_a_blocks, enabled=self.projected_form_a_recovery,
            verse_limit=structure.verse_limit)
        form_a_applied = sum(1 for row in self.projected_form_a_candidates
                             if row["applied"])
        if form_a_applied:
            self._bump("spanish_verse_starts_projected_form_a", form_a_applied)
        self._attribute_framed_markers()

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

    def _note_projected_form_a(self, placed, prov):
        """Anota un renglón que pasa el discriminador exacto de la 142.

        Sólo llega aquí lo que el clasificador, el marcador enmarcado y
        las recuperaciones 128/131/139 ya dejaron sin número: el respaldo
        no puede quitarle el sitio a ninguna de ellas. Es una comprobación
        local sobre la plana que se está leyendo; no relee nada.
        """
        if (placed.zone is not Zone.BODY or placed.column is not Column.RIGHT
                or not projected_form_a_recovery.has_form(placed.line)):
            return None
        # La geometría validada de la 142, medida una vez por plana y sólo
        # en las planas que tienen algún renglón con la forma.
        if self._form_a_geometry is None:
            self._form_a_geometry = projected_form_a_recovery.validated_geometry(
                self.page)
        where, bands = self._form_a_geometry
        column, zone = where.get(placed.line.index, (None, None))
        if column is not Column.RIGHT or zone is not Zone.BODY:
            text, reason = None, projected_form_a_recovery.NOT_VALIDATED_PLACEMENT
        else:
            text, reason, detail = projected_form_a_recovery.match(
                placed.line, bands.get(column))
        if text is None:
            self.projected_form_a_rejections[reason] = (
                self.projected_form_a_rejections.get(reason, 0) + 1)
            return None
        record = {
            "block_id": prov.block_id, "scan_page": self.page.scan_page,
            "book": self.current_book,
            "chapter_slot": getattr(self.chapter, "number", None),
            "raw": placed.line.raw_text[:90], "text": text,
            "open_verse_at_encounter": None,
            "owned_as_continuation": False,
            "outcome": None, "applied": False,
        }
        record.update(detail)
        self.projected_form_a_candidates.append(record)
        return record

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

    def _attribute_framed_markers(self):
        """Qué hizo cada marcador recuperado por puntuación exterior.

        Recuperar un marcador y ganar una referencia no son lo mismo, y
        la diferencia hay que poder verla: un marcador puede caer en un
        versículo que OTRO renglón numerado ya abría, puede repetir un
        número que ya trajo otro marcador enmarcado, o puede haber sido
        retirado después por imposible. Sin este reparto, la cuenta de
        marcadores y la de referencias no cuadran y no se sabe por qué.

        Se calcula sobre la edición ya construida, sin comparar con
        ninguna pasada anterior: lo único que hace falta es mirar, en el
        versículo donde acabó, cuántos renglones numerados lo declaran.
        """
        rejected = {block_id for record in self.impossible_markers
                    for block_id in record["block_ids"]}
        placed_at, numbered = {}, {}
        for osis, entry in self.edition.books.items():
            for number, chapter in entry.chapters.items():
                for verse, slot in chapter.verses.items():
                    ordered = sorted(
                        slot.blocks,
                        key=lambda block: block.provenance.block_id or "")
                    marks = [block.provenance.block_id for block in ordered
                             if block.decision in _NUMBERED_DECISIONS]
                    for block in ordered:
                        placed_at[block.provenance.block_id] = (
                            osis, number, verse)
                        numbered[block.provenance.block_id] = marks
        framed = {row["block_id"] for row in self.framed_markers}
        for row in self.framed_markers:
            block_id = row["block_id"]
            osis, number, verse = placed_at.get(block_id, (None, None, None))
            row["final_book"] = osis
            row["final_chapter"] = number
            row["final_verse"] = verse
            row["final_ref"] = (f"{osis}.{number}.{verse}"
                                if isinstance(number, int) and number > 0
                                else None)
            if block_id in rejected:
                row["outcome"] = "rejected_later_by_range_guard"
                continue
            if not isinstance(number, int) or number < 1:
                row["outcome"] = "chapter_left_in_review"
                continue
            # ¿Es este el ÚNICO renglón numerado de su versículo? Si lo
            # es, la referencia existe porque él la abrió. Si hay otro
            # --antes o después, da igual: los dos declaran el mismo
            # número-- la referencia tiene más de un origen y este
            # marcador no la explica solo.
            marks = [mark for mark in numbered.get(block_id, [])
                     if mark != block_id]
            row["numbered_lines_in_ref"] = len(marks) + 1
            if not marks:
                row["outcome"] = "only_numbered_line_of_its_ref"
            elif any(mark in framed for mark in marks):
                row["outcome"] = "duplicate_same_verse_marker"
            else:
                row["outcome"] = "shares_ref_with_plain_marker"

    def _handle_spanish(self, placed, prov):
        raw = placed.line.raw_text
        block = classifier.classify(raw, prov,
                                    expect_editorial=self.expect_editorial)
        self.expect_editorial = False

        # El numeral partido en dos palabras. Se prueba DESPUÉS de
        # clasificar y sólo sobre lo que el clasificador no supo leer:
        # así no le quita el sitio a ningún marcador que ya se leyera
        # bien, ni a un rótulo, ni al argumento del editor.
        if block.kind not in (BlockKind.VERSE, BlockKind.CHAPTER_HEADING,
                              BlockKind.EDITORIAL_HEADING):
            self._try_compound(placed, prov, block)
            if getattr(self, "_compound_block", None) is not None:
                block = self._compound_block
                self._compound_block = None
        # Task 146: el respaldo de la forma proyectada «a» mira sólo lo que
        # sigue sin número después de TODAS las rutas anteriores.
        form_a = None
        if block.kind not in (BlockKind.VERSE, BlockKind.CHAPTER_HEADING,
                              BlockKind.EDITORIAL_HEADING):
            form_a = self._note_projected_form_a(placed, prov)

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
            if getattr(block, "decision", None) == "compound_glyph_marker":
                # Se cuenta aparte, igual que el enmarcado, para poder
                # medir lo que aporta esta sola regla.
                self._bump("spanish_verse_starts_compound_glyph")
            if getattr(block, "decision", None) == "framed_marker":
                # La cifra estaba entera en el crudo y sólo la tapaba la
                # puntuación de al lado. Se cuenta aparte para poder
                # medir cuánto aporta esa sola regla.
                self._bump("spanish_verse_starts_framed")
                self.framed_markers.append({
                    "book": self.current_book,
                    "chapter_slot": getattr(self.chapter, "number", None),
                    "verse": block.number,
                    "block_id": prov.block_id,
                    "scan_page": prov.page,
                    "raw": raw[:90],
                    "text_head": (block.text or "")[:60],
                })
            self.chapter.verse(self.verse_number).blocks.append(
                _block(BlockKind.VERSE, placed, prov, number=block.number,
                       decision=(getattr(block, "decision", None)
                                 or "numbered line"),
                       text=block.text))
            return

        # Sin número. Sólo continúa si hay un versículo abierto en esta
        # misma columna y banda; si no, a revisión.
        if self.verse_number is not None:
            self._bump("continuations")
            continued = _block(BlockKind.VERSE, placed, prov,
                               number=self.verse_number,
                               decision="continuation of the open verse")
            self.chapter.verse(self.verse_number).blocks.append(continued)
            if form_a is not None:
                form_a["open_verse_at_encounter"] = self.verse_number
                form_a["owned_as_continuation"] = True
                self._form_a_blocks[prov.block_id] = continued
            return

        self._queue(_block(BlockKind.UNCLASSIFIED, placed, prov,
                           reason="unnumbered line with no open verse",
                           decision="no-open-verse"))


def parse_volume(pages: Iterable, *, witness: str, volume: str,
                 book: str = "Ps", edition: Optional[Edition] = None,
                 gutter_hint: Optional[int] = None, book_spans=None,
                 header_chapters=None, with_walker: bool = False,
                 image_reviews=None, recovery_source=None,
                 numeral_reviews=None, compound_recovery: bool = True,
                 a_glyph_recovery: Optional[bool] = None,
                 a_glyph_evidence=None, zero_anchor_io_recovery: bool = True,
                 projected_form_a_recovery: bool = True):
    """Recorre las páginas y devuelve (edición, métricas)."""
    edition = edition or Edition(edition_id="TorresAmat1835")
    walker = VolumeParser(edition, witness=witness, volume=volume, book=book,
                          gutter_hint=gutter_hint, book_spans=book_spans,
                          header_chapters=header_chapters,
                          image_reviews=image_reviews,
                          recovery_source=recovery_source,
                          numeral_reviews=numeral_reviews,
                          compound_recovery=compound_recovery,
                          a_glyph_recovery=a_glyph_recovery,
                          zero_anchor_io_recovery=zero_anchor_io_recovery,
                          a_glyph_evidence=a_glyph_evidence,
                          projected_form_a_recovery=projected_form_a_recovery)
    for page in pages:
        walker.feed_page(page)
    walker.finish()
    if with_walker:
        return edition, walker.stats, walker
    return edition, walker.stats
