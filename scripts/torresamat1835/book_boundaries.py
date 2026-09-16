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
from typing import Dict, List, Optional

import parser as classifier
import structure
from layout import Column, Zone

#: El orden en que el tomo 3 imprime sus libros, ya establecido en
#: structure.py. No se redefine aquí.
VOLUME3_ORDER = structure.VOLUME3_ORDER

#: Distancia máxima, en planas, entre dos bloques del mismo frontmatter
#: para considerarlos un solo bloque de apertura de libro. NO es una
#: ventana hacia atrás desde la confirmación: es adyacencia local entre
#: candidatos. Medido: la advertencia y el título formal de cada libro
#: van en planas contiguas o casi (Isa 496-497, Wis 318-319, Sir
#: 362-363-365), mientras que los candidatos falsos quedan a 5, 47 o 533
#: planas del grupo bueno.
CLUSTER_GAP_PAGES = 3

#: Un título de libro ocupa buena parte del ancho de la plana y va
#: centrado sobre el canal. Medido: 2326-2556 px de 3402.
_TITLE_MIN_WIDTH = 0.45
_TITLE_MAX_OFFSET = 0.12
#: Un título o un encabezado de advertencia son pocas palabras. La prosa
#: de la propia advertencia menciona el libro constantemente y ocupa el
#: mismo ancho, así que sin este corte una frase cualquiera de la página
#: pasaría por título.
_TITLE_MAX_WORDS = 9
#: Proporción mínima de mayúsculas. El impreso compone los títulos de
#: libro en versales («LIBRO DEL ECCLESIASTICO.», «LA profecía de
#: ISAÍAS.»: 0,44-1,00) mientras que el argumento del editor va en caja
#: de frase («Profetiza Isaías contra una nación que no nombra»: 0,05) y
#: la prosa corrida más abajo todavía. Sin esta señal, un argumento que
#: nombre el libro pasaba por título: es lo que anclaba Isaías en la
#: plana 534 en vez de en la 496.
_TITLE_MIN_UPPER_RATIO = 0.30


@dataclass
class PendingBookBoundaryCandidate:
    """Un comienzo de libro visto, todavía sin confirmar.

    Se detecta durante el barrido y se guarda. NO cambia el libro en
    curso: la identidad sigue exigiendo tres cabeceras coherentes. El
    candidato espera hacia adelante, sin caducar por número de planas,
    hasta que una confirmación lo ratifique o lo descarte.

    Ésa es la diferencia con la ventana fija que había antes: entre el
    comienzo físico de Isaías (p. 497) y su confirmación (p. 538) hay 41
    planas de cabeceras ilegibles, y cualquier `lookback` que las cubriera
    sería un número inventado.
    """
    to_book: str
    from_book: Optional[str]
    scan_page: int
    block_index: int
    bbox: tuple
    score: float
    raw_text: str
    evidence: List[str] = field(default_factory=list)
    disposition: str = "pending"
    rejection_reason: Optional[str] = None

    @property
    def block_id(self) -> str:
        return f"p{self.scan_page:04d}l{self.block_index:04d}"


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
    raw = placed.line.raw_text
    if len(raw.split()) > _TITLE_MAX_WORDS:
        return 0.0          # es prosa, no un rótulo
    letters = [c for c in raw if c.isalpha()]
    if not letters:
        return 0.0
    upper = sum(1 for c in letters if c.isupper()) / len(letters)
    if upper < _TITLE_MIN_UPPER_RATIO:
        return 0.0          # caja de frase: argumento del editor, no título
    reading = structure.read_header(placed.line.index, placed.line.raw_text,
                                    threshold=threshold)
    if reading.book != book:
        return 0.0
    return reading.book_score


def resolve_boundary(*, from_book, to_book, confirmation_page, window,
                     confirmation_evidence=None) -> BookBoundaryDecision:
    """Frontera efectiva dentro de una ventana acotada.

    `window` es [(scan_page, page_width, [PlacedLine, ...]), ...] ya
    recortada por el llamante. Esta función no descarta nada por la edad
    en planas de un candidato: eso era el defecto que el modelo de
    candidato pendiente vino a quitar.
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


@dataclass
class PendingBookBoundaryCandidate:
    """Un comienzo de libro visto, todavía sin confirmar.

    Se detecta durante el barrido y se guarda. NO cambia el libro en
    curso: la identidad sigue exigiendo tres cabeceras coherentes. El
    candidato espera hacia adelante, sin caducar por número de planas,
    hasta que una confirmación lo ratifique o lo descarte.

    Ésa es la diferencia con la ventana fija que había antes: entre el
    comienzo físico de Isaías (p. 497) y su confirmación (p. 538) hay 41
    planas de cabeceras ilegibles, y cualquier `lookback` que las cubriera
    sería un número inventado.
    """
    to_book: str
    from_book: Optional[str]
    scan_page: int
    block_index: int
    bbox: tuple
    score: float
    raw_text: str
    evidence: List[str] = field(default_factory=list)
    disposition: str = "pending"
    rejection_reason: Optional[str] = None

    @property
    def block_id(self) -> str:
        return f"p{self.scan_page:04d}l{self.block_index:04d}"


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
    raw = placed.line.raw_text
    if len(raw.split()) > _TITLE_MAX_WORDS:
        return 0.0          # es prosa, no un rótulo
    letters = [c for c in raw if c.isalpha()]
    if not letters:
        return 0.0
    upper = sum(1 for c in letters if c.isupper()) / len(letters)
    if upper < _TITLE_MIN_UPPER_RATIO:
        return 0.0          # caja de frase: argumento del editor, no título
    reading = structure.read_header(placed.line.index, placed.line.raw_text,
                                    threshold=threshold)
    if reading.book != book:
        return 0.0
    return reading.book_score


def resolve_boundary(*, from_book, to_book, confirmation_page, window,
                     confirmation_evidence=None) -> BookBoundaryDecision:
    """Frontera efectiva dentro de una ventana acotada.

    `window` es [(scan_page, page_width, [PlacedLine, ...]), ...] ya
    recortada por el llamante. Esta función no descarta nada por la edad
    en planas de un candidato: eso era el defecto que el modelo de
    candidato pendiente vino a quitar.
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


def refine_spans(spans, page_window_fn, *, lookback=4):
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


class BookCandidateTracker:
    """Lleva los candidatos vistos y los resuelve al confirmarse el libro.

    Tres estados, deliberadamente separados:

        1. candidato detectado      -- se ha visto un comienzo de libro
        2. identidad confirmada     -- tres cabeceras coherentes
        3. frontera efectiva fijada -- el candidato queda ratificado

    Lo que NO cancela un candidato: una lectura de cabecera suelta o
    incoherente. Entre las planas 498 y 533 del tomo las cabeceras se leen
    alternativamente como Song, Eccl e Isa, y ninguna de esas lecturas
    puede tumbar el comienzo de Isaías que ya se vio en la 497. Sólo puede
    cancelarlo evidencia de la misma fuerza: otro candidato de apertura de
    libro, estructural, incompatible con éste.
    """

    def __init__(self, order=VOLUME3_ORDER):
        self.order = order
        self.position = {osis: i for i, osis in enumerate(order)}
        self.candidates: List[PendingBookBoundaryCandidate] = []

    # -- 1. detección ------------------------------------------------
    def observe_page(self, page, placed_lines, *, current_book: Optional[str],
                     current_book_started: int):
        """Registra los candidatos fuertes de una plana.

        Sólo se consideran libros posteriores al actual en el orden del
        tomo -- nunca se retrocede -- y sólo bloques posteriores al
        comienzo del libro en curso, lo que descarta por estructura la
        portada del tomo, que nombra los siete libros de una vez.
        """
        if page.scan_page < current_book_started:
            return []
        start = self.position.get(current_book, -1) if current_book else -1
        found = []
        for placed in placed_lines:
            for target in self.order[start + 1:]:
                score = is_book_title_block(placed, page.width, book=target)
                if not score:
                    continue
                candidate = PendingBookBoundaryCandidate(
                    to_book=target, from_book=current_book,
                    scan_page=page.scan_page, block_index=placed.line.index,
                    bbox=tuple(placed.line.bbox), score=round(score, 3),
                    raw_text=placed.line.raw_text[:120],
                    evidence=["spanning centred block across the gutter",
                              "short heading, no division marker",
                              f"book name matches {target} ({score:.2f})",
                              "later than the current book in the volume order"])
                self.candidates.append(candidate)
                found.append(candidate)
        return found

    # -- 2/3. confirmación -------------------------------------------
    def settle(self, *, to_book: str, confirmation_page: int,
               from_book: Optional[str], current_book_started: int,
               confirmation_evidence=None) -> BookBoundaryDecision:
        """Resuelve la frontera al confirmarse `to_book`.

        Se toma el grupo de candidatos contiguos que termina antes de la
        confirmación: la advertencia del libro y su título formal van
        juntos, y la frontera se sitúa al principio de ese grupo, de modo
        que el frontmatter pertenece al libro que presenta -- la misma
        semántica que dejó BOOK-BOUNDARIES-106.
        """
        decision = BookBoundaryDecision(
            from_book=from_book, to_book=to_book,
            confirmation_page=confirmation_page,
            confirmation_evidence=confirmation_evidence or {})

        mine = [c for c in self.candidates
                if c.to_book == to_book and c.disposition == "pending"
                and current_book_started <= c.scan_page <= confirmation_page]

        # Un candidato fuerte a OTRO libro, visto después de éste, es la
        # única evidencia que puede invalidarlo.
        for candidate in mine:
            blockers = [c for c in self.candidates
                        if c.to_book != to_book
                        and c.scan_page > candidate.scan_page
                        and c.scan_page <= confirmation_page
                        and self.position.get(c.to_book, -1) >
                        self.position.get(to_book, -1)]
            if blockers:
                candidate.disposition = "rejected"
                candidate.rejection_reason = (
                    f"a stronger candidate for {blockers[0].to_book} follows "
                    f"on page {blockers[0].scan_page}")
        mine = [c for c in mine if c.disposition == "pending"]

        if not mine:
            decision.effective_page = confirmation_page
            decision.evidence = ["no surviving book-opening candidate before "
                                 "the confirmation"]
            decision.confidence = 0.3
            decision.review_required = True
            return decision

        mine.sort(key=lambda c: (c.scan_page, c.block_index))
        chosen = [mine[-1]]
        for candidate in reversed(mine[:-1]):
            if chosen[0].scan_page - candidate.scan_page <= CLUSTER_GAP_PAGES:
                chosen.insert(0, candidate)
            else:
                candidate.disposition = "rejected"
                candidate.rejection_reason = (
                    f"{chosen[0].scan_page - candidate.scan_page} pages before "
                    f"the book-opening group; not part of it")
        first, last = chosen[0], chosen[-1]

        for candidate in chosen:
            candidate.disposition = "confirmed"

        decision.first_evidence_page = first.scan_page
        decision.first_evidence_block = first.block_id
        decision.effective_page = first.scan_page
        decision.effective_block = first.block_id
        decision.evidence = list(first.evidence) + [
            f"{len(chosen)} adjacent opening block(s), pages "
            f"{first.scan_page}-{last.scan_page}",
            f"held pending for {confirmation_page - first.scan_page} pages "
            f"until three coherent running headers confirmed {to_book}",
        ]
        decision.confidence = round(min(0.6 + first.score * 0.4, 0.98), 3)
        decision.review_required = False
        return decision


def resolve_with_candidates(spans, pages_fn, *, order=VOLUME3_ORDER):
    """Ajusta los tramos con el modelo de candidato pendiente.

    Un solo barrido: se detectan candidatos mientras se leen las planas y
    se resuelven al llegar a cada confirmación. Sin releer ventanas.
    """
    tracker = BookCandidateTracker(order=order)
    pending_confirmations = {s.first_page: index
                             for index, s in enumerate(spans) if index}
    decisions: List[BookBoundaryDecision] = []
    current_index = 0

    for page, placed_lines in pages_fn():
        index = pending_confirmations.get(page.scan_page)
        if index is not None:
            span, previous = spans[index], spans[index - 1]
            decision = tracker.settle(
                to_book=span.osis, confirmation_page=page.scan_page,
                from_book=previous.osis,
                current_book_started=previous.first_page,
                confirmation_evidence=dict(span.evidence))
            decisions.append(decision)
            if not decision.review_required:
                span.first_page = decision.effective_page
                previous.last_page = decision.effective_page - 1
            current_index = index
        tracker.observe_page(
            page, placed_lines, current_book=spans[current_index].osis,
            current_book_started=spans[current_index].first_page)
    return decisions, tracker
