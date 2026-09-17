"""
Aplicar al modelo lo que se leyó en el facsímil.

    bloques OCR crudos  +  metadata de revisión verificada
        =  vista estructural recuperada

El OCR NO SE TOCA. No se edita el DjVu XML, no se edita el ABBYY, no se
inventan bloques de reconocimiento. Lo que hace esta capa es construir,
por encima del flujo de renglones colocados, una vista en la que el
rótulo que el impreso tiene y el reconocimiento perdió existe otra vez,
marcado como lo que es: una lectura de la imagen, no una salida del OCR.

Hay dos formas de que una revisión cambie la estructura, y conviene no
confundirlas:

    insertar   el reconocimiento no dejó NADA donde el impreso tiene un
               rótulo; se añade un evento recuperado entre las anclas.

    resolver   el reconocimiento sí dejó el rótulo, pero con el numeral
               destrozado («CAPÍTULO* XX vi»), así que la frontera ya
               existía y lo que faltaba era el número. La imagen lo da.
               No se duplica nada: se completa lo que ya había.

Y una tercera que no cambia nada: si el rótulo ya está Y ya tiene ese
mismo número, la revisión es redundante y se anota como tal.

Reglas que no se negocian:

  * el numeral sale del numeral impreso leído en la imagen. Nunca de
    anterior+1, nunca del máximo de la Vulgata, nunca de la secuencia;
  * si las anclas no validan, la revisión NO se aplica: se falla cerrado
    y se dice por qué. Nunca se cae de vuelta a «por número de página»;
  * una revisión no mueve bloques de un libro a otro. El dueño del
    bloque lo decide la resolución de fronteras de libro, y una revisión
    que la contradiga se rechaza;
  * una revisión rechazada no tiene ningún efecto estructural.
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import image_reviews
import parser as classifier
import structure
from source_ocr import SourceLine, SourceWord

#: Marca de procedencia de todo lo que entra por aquí.
RECOVERED = "image_review"

#: Qué hizo cada revisión. Se audita entero.
INSERTED = "inserted"
RESOLVED = "resolved_number"
REDUNDANT = "redundant_review"
REJECTED = "rejected_no_effect"
FAILED = "failed_anchor_validation"
CONFLICT = "conflicting_number"
COLLISION = "collision_with_existing_chapter"


@dataclass(frozen=True)
class SourceIdentity:
    """El artefacto visual concreto sobre el que se hicieron las lecturas."""
    witness: str
    filename: str
    sha256: str
    page_count: Optional[int] = None
    page_mapping: Optional[str] = None


@dataclass(frozen=True)
class RecoveryProvenance:
    """De dónde sale un evento que no produjo el reconocimiento.

    Sin esto, aguas abajo no habría forma de distinguir un rótulo leído
    por el OCR de uno leído por una persona en la imagen, y esa
    distinción es justo la que hay que poder auditar.
    """
    source: str
    review_id: str
    witness: str
    source_sha256: str
    scan_page: int
    pdf_page: Optional[int]
    printed_page: Optional[int]
    bbox: Optional[tuple]
    observed_printed_text: Optional[str]
    confidence: float
    reviewer_method: str

    def as_dict(self) -> dict:
        return {
            "source": self.source, "review_id": self.review_id,
            "witness": self.witness, "source_sha256": self.source_sha256,
            "scan_page": self.scan_page, "pdf_page": self.pdf_page,
            "printed_page": self.printed_page,
            "bbox": list(self.bbox) if self.bbox else None,
            "observed_printed_text": self.observed_printed_text,
            "confidence": self.confidence,
            "reviewer_method": self.reviewer_method,
        }


@dataclass
class Recovery:
    """Lo que una revisión aporta a un punto concreto del flujo."""
    review_id: str
    action: str
    provenance: RecoveryProvenance
    #: El numeral leído en la imagen. None cuando la frontera se ve pero
    #: el numeral no: eso NO se completa por secuencia.
    chapter_number: Optional[int]
    review_required: bool


@dataclass
class StreamEntry:
    """Un renglón del flujo, con o sin recuperación encima.

    Expone `line`, `column` y `zone` igual que un PlacedLine, así que el
    parser recorre unos y otros sin saber la diferencia; lo que los
    distingue es `recovery`, que aguas abajo dice «esto no lo dijo el
    reconocimiento».
    """
    line: object
    column: object
    zone: object
    recovery: Optional[Recovery] = None

    @property
    def recovered(self) -> bool:
        return self.recovery is not None


@dataclass
class Application:
    """Qué se hizo con una revisión, para el informe."""
    review_id: str
    book: str
    scan_page: int
    action: str
    reason: str = ""
    anchor_after: Optional[str] = None
    anchor_before: Optional[str] = None
    target_block: Optional[str] = None
    resulting_block: Optional[str] = None
    chapter_number: Optional[int] = None
    provenance: dict = field(default_factory=dict)

    @property
    def changed_stream(self) -> bool:
        return self.action in (INSERTED, RESOLVED)


def _block_id(scan_page: int, line_index: int) -> str:
    return f"p{scan_page:04d}l{line_index:04d}"


def _entry_block_id(entry, scan_page) -> Optional[str]:
    line = entry.line
    index = getattr(line, "index", None)
    if index is None:
        return None
    prefix = "r" if getattr(entry, "recovery", None) else "l"
    return f"p{scan_page:04d}{prefix}{index:04d}"


def _recovered_line(review, index: int) -> SourceLine:
    """El renglón sintético del rótulo recuperado.

    Lleva el texto impreso tal y como se leyó en la imagen, y una caja
    deducida de las anclas. No pretende ser una salida del OCR: su
    identificador lleva «r» en vez de «l» y el evento va marcado.
    """
    text = review.observed_printed_text or ""
    bbox = tuple(review.crop_bbox) if review.crop_bbox else (0, 0, 0, 0)
    word = SourceWord(text=text, bbox=bbox, confidence=0)
    return SourceLine(index=index, words=[word], bbox=bbox)


def _provenance(review, source: SourceIdentity) -> RecoveryProvenance:
    return RecoveryProvenance(
        source=RECOVERED, review_id=review.id, witness=source.witness,
        source_sha256=source.sha256, scan_page=review.scan_page,
        pdf_page=review.pdf_page, printed_page=review.printed_page,
        bbox=review.crop_bbox,
        observed_printed_text=review.observed_printed_text,
        confidence=review.confidence, reviewer_method=review.reviewer_method)


def _numeral_of(raw: str, book: Optional[str]) -> Optional[int]:
    """El número que un rótulo del OCR AFIRMA por sí solo, si afirma alguno.

    Es el respaldo para cuando no se ha pasado un resolvedor: un rótulo
    afirma un número cuando su numeral se deja leer sin ambigüedad, un
    solo candidato dentro del límite del libro. Cuando el reconocimiento
    lo ha partido -- «CAPÍTULO* XX vi» da 20 y 6 porque ha roto XLVI en
    dos trozos -- no hay afirmación que contradecir. Tomar el primer
    candidato de una lista ambigua fabricaría un desacuerdo con basura.

    Nunca recupera nada: sólo sirve para saber si el rótulo que ya está
    dice algo distinto que la revisión.
    """
    limit = structure.chapter_limit(book) if book else None
    plausible = [value for value in structure.roman_candidates(
        structure._without_book_words(raw or ""))
        if limit is None or value <= limit]
    return plausible[0] if len(plausible) == 1 else None


def validate_anchors(review, entries, page, *, book) -> Optional[str]:
    """Por qué NO se puede aplicar esta revisión, o None si se puede.

    Falla cerrado: sin anclas verificables no hay inserción. No existe un
    camino alternativo «por número de página», porque ese es justamente
    el que colocaría el rótulo donde no está.
    """
    if review.scan_page != page.scan_page:
        return (f"review is for scan page {review.scan_page}, "
                f"not {page.scan_page}")
    if book is not None and review.book != book:
        return (f"review claims book {review.book} but the boundary "
                f"resolution owns this page for {book}")
    after, before = review.insert_after_block, review.insert_before_block
    if not after:
        return "no insert_after_block anchor"
    if not before:
        return "no insert_before_block anchor"

    order = {}
    for position, entry in enumerate(entries):
        block_id = _entry_block_id(entry, page.scan_page)
        if block_id is not None:
            order[block_id] = position
    if after not in order:
        return f"anchor {after} is not a block of scan page {page.scan_page}"
    if before not in order:
        return f"anchor {before} is not a block of scan page {page.scan_page}"
    if order[after] >= order[before]:
        return (f"anchors are out of reading order: {after} is not before "
                f"{before}")

    if review.crop_bbox:
        top = entries[order[after]].line.bbox[3]
        bottom = entries[order[before]].line.bbox[1]
        x0, y0, x1, y1 = review.crop_bbox
        if not (y0 <= top and bottom <= y1):
            return (f"crop_bbox {list(review.crop_bbox)} does not cover the "
                    f"space between the anchors (y {top}..{bottom})")
    return None


def _existing_heading(entries, start: int, stop: int):
    """Un rótulo de división ya presente entre las dos anclas."""
    for position in range(start + 1, stop):
        entry = entries[position]
        raw = getattr(entry.line, "raw_text", "") or ""
        if classifier.carries_division_marker(raw):
            return position, entry
    return None, None


def apply_verified_image_reviews(page, entries, reviews, *, source,
                                 book=None, resolve_numeral=None,
                                 claimed_numbers=None):
    """Aplica al flujo de una plana las revisiones verificadas de esa plana.

    `entries` es la plana ya parseada y colocada en orden de lectura;
    `reviews` las revisiones ya validadas y con la guarda de hash puesta;
    `source` la identidad del artefacto visual.

    `resolve_numeral` responde, para el texto crudo de un rótulo ya
    presente, qué número le da la resolución de capítulo -- o None si no
    se lo pudo identificar. Es la MISMA pregunta que hará el parser un
    momento después, y pasarla desde fuera es lo que impide que las dos
    respuestas se separen: un rótulo que el OCR escribió limpio pero que
    ninguna otra señal corrobora sigue estando sin identificar, y ahí la
    imagen sí tiene algo que aportar. Sin resolvedor se usa el respaldo
    de `_numeral_of`.

    `claimed_numbers` son los capítulos que ese libro ya tiene escritos.
    Si el numeral leído en la imagen es uno de ellos, la recuperación NO
    se aplica: escribirla metería dos capítulos distintos en el mismo
    hueco y los versículos del segundo pisarían a los del primero. Eso
    se falla cerrado y se dice quién reclama qué, porque significa que
    una de las dos lecturas está mal y eso lo decide una persona, no
    esta capa.

    Devuelve (flujo enriquecido, [Application]). El flujo de entrada no
    se modifica.
    """
    claimed_numbers = claimed_numbers or frozenset()
    if resolve_numeral is None:
        def resolve_numeral(raw, book=book):
            return _numeral_of(raw, book)
    stream = list(entries)
    applications: List[Application] = []

    for review in sorted(reviews, key=lambda r: r.id):
        record = Application(review_id=review.id, book=review.book,
                             scan_page=review.scan_page, action=REJECTED,
                             anchor_after=review.insert_after_block,
                             anchor_before=review.insert_before_block)

        # Una revisión que no confirma frontera no toca el flujo. Nunca.
        if not review.creates_boundary:
            record.reason = "the facsimile shows no chapter boundary here"
            applications.append(record)
            continue

        # Idempotencia: si esta misma revisión ya está puesta, no se
        # vuelve a poner.
        already = next((e for e in stream
                        if getattr(e, "recovery", None) is not None
                        and e.recovery.review_id == review.id), None)
        if already is not None:
            record.action = REDUNDANT
            record.reason = "this review is already applied to the stream"
            record.resulting_block = _entry_block_id(already, page.scan_page)
            record.chapter_number = already.recovery.chapter_number
            applications.append(record)
            continue

        problem = validate_anchors(review, stream, page, book=book)
        if problem:
            record.action = FAILED
            record.reason = problem
            applications.append(record)
            continue

        if review.chapter_number is not None and \
                review.chapter_number in claimed_numbers:
            record.action = COLLISION
            record.reason = (
                f"{review.book} {review.chapter_number} is already held by "
                f"another heading; applying this review would merge two "
                f"chapters into one slot and overwrite verses")
            record.chapter_number = review.chapter_number
            applications.append(record)
            continue

        order = {_entry_block_id(e, page.scan_page): i
                 for i, e in enumerate(stream)}
        start = order[review.insert_after_block]
        stop = order[review.insert_before_block]
        provenance = _provenance(review, source)
        recovery = Recovery(
            review_id=review.id, action=INSERTED, provenance=provenance,
            chapter_number=review.chapter_number,
            review_required=review.review_required)

        position, existing = _existing_heading(stream, start, stop)
        if existing is not None:
            seen = resolve_numeral(getattr(existing.line, "raw_text", ""))
            if seen is not None and review.chapter_number is not None and \
                    seen != review.chapter_number:
                record.action = CONFLICT
                record.reason = (
                    f"the chapter resolution identified the heading between "
                    f"the anchors as {seen}, the facsimile reads "
                    f"{review.chapter_number}")
                record.target_block = _entry_block_id(existing, page.scan_page)
                applications.append(record)
                continue
            if seen is not None and seen == review.chapter_number:
                record.action = REDUNDANT
                record.reason = ("the chapter resolution already identified "
                                 "this heading as that number")
                record.target_block = _entry_block_id(existing, page.scan_page)
                record.chapter_number = seen
                applications.append(record)
                continue
            # La frontera ya estaba; lo que faltaba era el numeral, y eso
            # es lo que da la imagen. Se completa en su sitio, sin
            # duplicar el rótulo.
            recovery.action = RESOLVED
            stream[position] = StreamEntry(
                line=existing.line, column=existing.column,
                zone=existing.zone, recovery=recovery)
            record.action = RESOLVED
            record.reason = ("the heading was there but the chapter "
                             "resolution could not identify it; the "
                             "facsimile supplies the printed numeral")
            record.target_block = _entry_block_id(existing, page.scan_page)
            record.resulting_block = record.target_block
            record.chapter_number = review.chapter_number
            record.provenance = provenance.as_dict()
            applications.append(record)
            continue

        # No había nada: se inserta, y en su sitio -- entre las anclas,
        # no al final de la plana.
        index = max((getattr(e.line, "index", -1) for e in stream),
                    default=-1) + 1
        entry = StreamEntry(line=_recovered_line(review, index),
                            column=stream[start].column,
                            zone=stream[start].zone, recovery=recovery)
        stream.insert(stop, entry)
        record.action = INSERTED
        record.reason = "no chapter heading of any kind between the anchors"
        record.resulting_block = _entry_block_id(entry, page.scan_page)
        record.chapter_number = review.chapter_number
        record.provenance = provenance.as_dict()
        applications.append(record)

    return stream, applications


def source_identity(payload: dict) -> SourceIdentity:
    source = payload["visual_source"]
    return SourceIdentity(
        witness=source["witness"], filename=source["filename"],
        sha256=source["sha256"], page_count=source.get("page_count"),
        page_mapping=source.get("page_mapping"))


def load_applicable(*, path=None, source_path=None, expected_sha256=None):
    """Revisiones listas para aplicar, con la guarda de hash puesta.

    Si el artefacto visual no es el que se miró, esto levanta
    ReviewError y NO se aplica ninguna recuperación. No hay degradación
    a aviso silencioso.
    """
    payload = image_reviews.load(path) if path else image_reviews.load()
    reviews = image_reviews.reviews_for(
        payload, source_path=source_path, expected_sha256=expected_sha256)
    return payload, source_identity(payload), reviews


def by_page(reviews) -> Dict[int, list]:
    out: Dict[int, list] = {}
    for review in reviews:
        out.setdefault(review.scan_page, []).append(review)
    return out
