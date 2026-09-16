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

import parser as classifier
import structure
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


def _provenance(witness, volume, page, placed, printed_page=None):
    line = placed.line
    return Provenance(
        witness=witness, volume=volume, page=page.scan_page,
        line=line.index, column=placed.column.value, zone=placed.zone.value,
        bbox=line.bbox, printed_page=printed_page,
        block_id=f"p{page.scan_page:04d}l{line.index:04d}")


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
                 book_spans=None, header_chapters=None):
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
        self.last_chapter = {}
        self.resolutions = []
        self.page = None

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
        hint = self._hint()
        layout = measure(page, gutter_hint=hint)
        if layout.gutter is not None:
            self.gutters.append(layout.gutter)
        placed_lines = split_columns(page, gutter_hint=hint)
        self._bump("pages")
        self._bump("ocr_blocks", len(placed_lines))
        self._bump("gutter_found", 1 if layout.gutter is not None else 0)

        for placed in placed_lines:
            self._bump("column_" + placed.column.value)
            prov = _provenance(self.witness, self.volume, page, placed)

            # --- fuera del cuerpo: cabecera, pie, notas ---------------
            if placed.zone in _ZONE_KIND:
                kind = _ZONE_KIND[placed.zone]
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
        if classifier.looks_like_division(raw):
            number = classifier.division_number(
                classifier._DIVISION_RE.match(
                    classifier._fold(raw).upper()).group("rest"))
            self._open_chapter(placed, prov, number, raw)
            return
        block = _block(BlockKind.EDITORIAL_HEADING, placed, prov,
                       decision="spans the gutter")
        self._bump("editorial_headings")
        if self.chapter is not None:
            self.chapter.paratext.append(block)
        else:
            self.edition.book(self.current_book).chapter(0).paratext.append(block)

    def _open_chapter(self, placed, prov, number, raw):
        """Abre la división cruzando las señales del testigo.

        `number` es sólo lo que dio el numeral de la marca. Se resuelve
        con structure.resolve_chapter(), que exige que dos señales
        independientes coincidan. Si no se puede, el capítulo queda sin
        número resuelto y marcado: la frontera sobrevive igual, que es lo
        que impide que el texto se corra.
        """
        book = self.current_book
        page = self.page.scan_page if self.page is not None else None
        previous = self.last_chapter.get(book)
        resolution = structure.resolve_chapter(
            raw_numeral=raw,
            header_chapters=self.header_chapters.get(page, []),
            previous=previous, book=book)

        self._bump("chapters_resolved_" + resolution.method
                   if resolution.resolved is not None
                   else "chapters_unresolved")
        if resolution.method == "direct_ocr":
            self._bump("chapters_resolved_direct")
        elif resolution.resolved is not None:
            self._bump("chapters_resolved_correlated")

        self.resolutions.append({
            "book": book, "scan_page": page,
            "source_heading": raw[:120],
            "raw_numeral": resolution.raw_numeral[:60] if resolution.raw_numeral else None,
            "parsed_candidate": resolution.parsed_candidate,
            "resolved_number": resolution.resolved,
            "method": resolution.method,
            "evidence": resolution.evidence,
            "confidence": resolution.confidence,
            "review_required": resolution.review_required,
            "block_id": prov.block_id,
        })

        # Sin número resuelto el capítulo no recibe una cifra inventada:
        # se le da una posición correlativa NEGATIVA de trabajo, que no
        # puede confundirse con un capítulo real, y queda en revisión.
        if resolution.resolved is None:
            slot = -(len([k for k in self.edition.book(book).chapters
                          if k < 0]) + 1)
        else:
            slot = resolution.resolved
            self.last_chapter[book] = slot

        block = _block(BlockKind.CHAPTER_HEADING, placed, prov,
                       number=resolution.resolved,
                       reason=(None if not resolution.review_required else
                               f"chapter identity unresolved "
                               f"({resolution.evidence.get('why', '')})"),
                       decision=resolution.method)
        if resolution.review_required:
            self.edition.review_queue.append(block)
        self.chapter = self.edition.book(book).chapter(slot)
        self.chapter.paratext.append(block)
        self.verse_number = None
        self.expect_editorial = True

    def _handle_spanish(self, placed, prov):
        raw = placed.line.raw_text
        block = classifier.classify(raw, prov,
                                    expect_editorial=self.expect_editorial)
        self.expect_editorial = False

        if block.kind is BlockKind.CHAPTER_HEADING:
            self._open_chapter(placed, prov, block.number, raw)
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
                 header_chapters=None, with_walker: bool = False):
    """Recorre las páginas y devuelve (edición, métricas)."""
    edition = edition or Edition(edition_id="TorresAmat1835")
    walker = VolumeParser(edition, witness=witness, volume=volume, book=book,
                          gutter_hint=gutter_hint, book_spans=book_spans,
                          header_chapters=header_chapters)
    for page in pages:
        walker.feed_page(page)
    if with_walker:
        return edition, walker.stats, walker
    return edition, walker.stats
