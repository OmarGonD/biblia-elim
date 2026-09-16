"""
Dónde empieza de verdad cada libro, sin aflojar cómo se confirma cuál es.

Son dos preguntas distintas y hasta ahora se contestaban con una sola:

    confirmación -- ¿tenemos evidencia bastante de que esto es Isaías?
    frontera     -- ¿desde qué bloque exactamente empieza Isaías?

La confirmación sigue exigiendo tres cabeceras corridas coherentes
(structure.book_spans), que es lo que impide que un OCR roto cambie de
libro por una plana suelta. Pero la cabecera nueva tarda una o dos planas
en aparecer, así que confirmar en la plana N no significa que el libro
empiece en N.

Lo que el impreso pone en cada una de las seis transiciones, medido:

    ADVERTENCIA / SOBRE EL LIBRO DE …      paratexto de presentación
    LIBRO DE LOS PROVERBIOS                título del libro
    DE SALOMÓN.
    CAPÍTULO PRIMERO.                      primera división del nuevo

El título es un bloque que cruza el canal, centrado y ancho, y no lleva
rótulo de división. Ésa es la frontera efectiva: el bloque donde empieza
el libro nuevo. Se busca sólo dentro de una ventana acotada hacia atrás
desde la confirmación, nunca por todo el tomo.
"""
from dataclasses import dataclass, field
from typing import List, Optional

import parser as classifier
import structure
from layout import Column, Zone

#: Ventana de retroinspección, en planas. Medida sobre las seis
#: transiciones reales del tomo 3: la distancia entre el título del libro
#: y su confirmación por cabeceras va de 0 a 8 planas -- las advertencias
#: que preceden a Sabiduría y a Eclesiástico ocupan varias planas y la
#: cabecera nueva no aparece hasta que empieza el texto. Se toma 10, algo
#: por encima del máximo observado, y se corta ahí: la ventana nunca
#: retrocede más allá del comienzo del libro anterior.
LOOKBACK_PAGES = 10

#: Un título de libro ocupa buena parte del ancho de la plana y va
#: centrado sobre el canal. Medido: 2326-2556 px de 3402.
_TITLE_MIN_WIDTH = 0.45
_TITLE_MAX_OFFSET = 0.12
#: Un título o un encabezado de advertencia son pocas palabras. La prosa
#: de la propia advertencia menciona el libro constantemente y ocupa el
#: mismo ancho, así que sin este corte una frase cualquiera de la página
#: pasaría por título.
_TITLE_MAX_WORDS = 9


@dataclass
class BookBoundaryDecision:
    from_book: Optional[str]
    to_book: str
    confirmation_page: int
    confirmation_evidence: dict = field(default_factory=dict)
    first_evidence_page: Optional[int] = None
    first_evidence_block: Optional[str] = None
    effective_page: Optional[int] = None
    effective_block: Optional[str] = None
    evidence: List[str] = field(default_factory=list)
    confidence: float = 0.0
    review_required: bool = True

    @property
    def lookback_distance(self) -> Optional[int]:
        if self.effective_page is None:
            return None
        return self.confirmation_page - self.effective_page


def is_book_title_block(placed, page_width: int, *, book: str,
                        threshold: float = 0.72) -> float:
    """Parecido de un bloque al título del libro que se busca.

    La geometría manda: tiene que cruzar el canal, ir centrado y ser
    ancho. Sólo entonces se mira si el texto se parece al nombre del
    libro, y para eso se reutiliza el mismo comparador de esqueletos que
    ya identifica las cabeceras (structure), no uno nuevo. Devuelve 0.0
    cuando la geometría no acompaña, por mucho que el texto coincida.
    """
    if placed.column is not Column.SPANNING or placed.zone is not Zone.BODY:
        return 0.0
    box = placed.line.bbox
    width = box[2] - box[0]
    if width < page_width * _TITLE_MIN_WIDTH:
        return 0.0
    centre = (box[0] + box[2]) / 2.0
    if abs(centre - page_width / 2.0) > page_width * _TITLE_MAX_OFFSET:
        return 0.0
    if classifier.carries_division_marker(placed.line.raw_text):
        return 0.0          # es una división, no el título del libro
    if len(placed.line.raw_text.split()) > _TITLE_MAX_WORDS:
        return 0.0          # es prosa, no un rótulo
    reading = structure.read_header(placed.line.index, placed.line.raw_text,
                                    threshold=threshold)
    if reading.book != book:
        return 0.0
    return reading.book_score


def resolve_boundary(*, from_book, to_book, confirmation_page, window,
                     confirmation_evidence=None) -> BookBoundaryDecision:
    """Frontera efectiva dentro de una ventana acotada.

    `window` es [(scan_page, page_width, [PlacedLine, ...]), ...] ya
    recortada por el llamante a LOOKBACK_PAGES planas. Aquí no se recorre
    nada más: el coste es el de la ventana, no el del tomo.
    """
    decision = BookBoundaryDecision(
        from_book=from_book, to_book=to_book,
        confirmation_page=confirmation_page,
        confirmation_evidence=confirmation_evidence or {})

    best = None
    for scan_page, page_width, placed_lines in window:
        for placed in placed_lines:
            score = is_book_title_block(placed, page_width, book=to_book)
            if score and (best is None or scan_page >= best[0]):
                best = (scan_page, placed, score)

    if best is None:
        # Se sabe que el libro cambió, pero no desde dónde. No se elige
        # la plana más probable en silencio: la frontera se queda en la
        # confirmación y queda marcada para revisión.
        decision.effective_page = confirmation_page
        decision.effective_block = None
        decision.evidence = ["no book-title block inside the lookback window"]
        decision.confidence = 0.3
        decision.review_required = True
        return decision

    scan_page, placed, score = best
    block_id = f"p{scan_page:04d}l{placed.line.index:04d}"
    decision.first_evidence_page = scan_page
    decision.first_evidence_block = block_id
    decision.effective_page = scan_page
    decision.effective_block = block_id
    decision.evidence = [
        "spanning centred block across the gutter",
        f"width >= {_TITLE_MIN_WIDTH:.0%} of the page",
        f"book name matches {to_book} (score {score:.2f})",
        "carries no division marker",
    ]
    decision.confidence = round(min(0.6 + score * 0.4, 0.98), 3)
    decision.review_required = False
    return decision


def refine_spans(spans, page_window_fn, *, lookback=LOOKBACK_PAGES):
    """Ajusta el `first_page` de cada tramo a su frontera efectiva.

    La identidad de cada libro NO se toca: sigue siendo la que confirmaron
    las tres cabeceras. Lo único que se mueve es desde dónde empieza.
    """
    decisions = []
    for index, span in enumerate(spans):
        if index == 0:
            continue
        previous = spans[index - 1]
        confirmation = span.first_page
        lowest = max(previous.first_page + 1, confirmation - lookback)
        window = page_window_fn(lowest, confirmation)
        decision = resolve_boundary(
            from_book=previous.osis, to_book=span.osis,
            confirmation_page=confirmation, window=window,
            confirmation_evidence=dict(span.evidence))
        decisions.append(decision)
        if not decision.review_required and decision.effective_page is not None:
            span.first_page = decision.effective_page
            previous.last_page = decision.effective_page - 1
    return decisions
