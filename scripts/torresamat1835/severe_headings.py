"""
Rótulos que el reconocimiento dejó irreconocibles por los dos lados.

Las tandas anteriores fueron acorralando el daño:

    118   la palabra rota, el numeral todavía legible
          («5ALMO CXXXVI.»): se busca por el esqueleto de la palabra y
          se confirma con el numeral.

    119   se revisaron los veinte candidatos de `missing_heading` y
          ninguno era un rótulo ausente: trece tenían su renglón en el
          reconocimiento con la palabra Y el numeral destrozados a la
          vez («s A X *<>», «$'AtMÓ LXirill», «XXXVI.»), que es
          justamente lo que ningún buscador de texto puede encontrar.

De ahí sale este módulo, y su regla:

    GEOMETRY / STRUCTURE DISCOVERS
    FACSIMILE CONFIRMS

Cuando el texto no sirve para nada, lo que queda es cómo está compuesta
la plana. Un rótulo de esta edición deja una huella física que no
depende de que se lea ni una letra:

    una fila ESTRECHA, centrada sobre el canal,
    separada de lo de arriba,
    y debajo el argumento del editor, que es ANCHO.

Eso es lo que se busca aquí, y no más: el resultado es una pregunta para
llevar al facsímil. Ninguna de estas señales afirma que haya un rótulo,
ninguna dice qué número tiene, y de aquí no sale ningún capítulo.

Dos decisiones que conviene explicar:

  * se trabaja por FILAS y no por renglones sueltos. El impreso compone
    una línea; el reconocimiento a veces la parte en dos bloques a un
    lado y otro del canal (la plana del salmo 34 es así). Agrupar por
    solapamiento vertical hace que esa línea vuelva a ser una sola cosa
    sin tocar el crudo: los bloques siguen separados y con su
    identificador, y lo único que se junta es la MEDIDA.

  * el numeral no es puerta. En 118 lo era, y por eso se le escaparon
    los trece de 119.
"""
import statistics
from dataclasses import dataclass, field
from typing import List, Optional

from layout import Column, Zone

#: Una fila de rótulo es estrecha: la caja del impreso ocupa en torno a
#: un cuarto de la plana, y el argumento que va debajo, tres cuartos. El
#: techo es generoso porque el reconocimiento le pega basura a los lados.
MAX_WIDTH_RATIO = 0.55
#: Y va centrada sobre el canal. Una línea suelta de una columna cae muy
#: lejos del centro, y es lo que separa un rótulo del final de un
#: párrafo.
MAX_OFF_CENTRE = 0.13
#: Separada de lo de arriba, medido en pasos de línea de su propia plana
#: y no en píxeles: la densidad cambia de una plana a otra.
MIN_GAP_BEFORE = 0.20
#: Un rótulo son dos o tres palabras; con la basura del canto, unas
#: pocas más.
MAX_TOKENS = 14
#: El argumento del editor: ancho, y bastante más que el rótulo.
ARGUMENT_WIDTH = 0.55
ARGUMENT_RATIO = 1.8
#: Cuántas filas más abajo se admite el argumento. El reconocimiento
#: mete a veces una línea de basura entre medias.
ARGUMENT_WITHIN = 3

HIGH = "high"
MEDIUM = "medium"
LOW = "low"

#: Por qué una fila NO llega a candidata. Se cuentan para poder auditar
#: el filtro y no sólo su resultado.
NOT_BODY = "not in the body band"
TOO_WIDE = "as wide as running text"
OFF_CENTRE = "not centred over the gutter"
NO_GAP = "no separation from the line above"
TOO_MANY_TOKENS = "too many words for a heading"
NO_ARGUMENT = "no wide line follows, as an editor's argument would"


@dataclass
class Row:
    """Una línea del impreso, con los bloques del OCR que la componen."""
    scan_page: int
    block_ids: List[str]
    bbox: tuple
    raw_texts: List[str]
    spanning: bool
    columns: List[str]

    @property
    def raw(self) -> str:
        return " | ".join(self.raw_texts)


@dataclass
class Candidate:
    """Lo que se sabe de una fila que podría ser un rótulo.

    Todo lo que hay aquí es medida o procedencia. No hay ninguna
    afirmación sobre el contenido: el texto crudo va entero para poder
    contrastarlo con lo que diga la imagen.
    """
    book: str
    scan_page: int
    block_ids: List[str]
    bbox: tuple
    raw_text: str
    width_ratio: float
    off_centre: float
    height: int
    gap_before: float
    gap_after: float
    next_width_ratio: Optional[float]
    argument_rows_below: Optional[int]
    spanning: bool
    columns: List[str]
    token_count: int
    upper_ratio: float
    rank: str
    evidence: List[str] = field(default_factory=list)
    represented_by: Optional[str] = None

    @property
    def pdf_page(self) -> int:
        return self.scan_page + 1

    @property
    def multi_block(self) -> bool:
        return len(self.block_ids) > 1

    def as_dict(self) -> dict:
        out = dict(self.__dict__)
        out["bbox"] = list(self.bbox)
        out["pdf_page"] = self.pdf_page
        out["multi_block"] = self.multi_block
        return out


def rows_of(page, placed) -> List[Row]:
    """Las filas del cuerpo de una plana: lo que el impreso puso en línea.

    Dos bloques pertenecen a la misma fila si sus cajas se solapan
    verticalmente más de la mitad de su altura. Es la forma barata de
    volver a juntar una línea que el reconocimiento partió por el canal
    sin tocar el crudo ni fabricar un texto que nadie escribió.
    """
    body = sorted((e for e in placed if e.zone is Zone.BODY),
                  key=lambda e: (e.line.bbox[1], e.line.bbox[0]))
    rows: List[List] = []
    current: List = []
    for entry in body:
        if not current:
            current = [entry]
            continue
        top = max(e.line.bbox[1] for e in current)
        bottom = min(e.line.bbox[3] for e in current)
        overlap = min(bottom, entry.line.bbox[3]) - max(top, entry.line.bbox[1])
        height = min(bottom - top, entry.line.bbox[3] - entry.line.bbox[1]) or 1
        if overlap > 0.5 * height:
            current.append(entry)
        else:
            rows.append(current)
            current = [entry]
    if current:
        rows.append(current)

    out = []
    for group in rows:
        x0 = min(e.line.bbox[0] for e in group)
        y0 = min(e.line.bbox[1] for e in group)
        x1 = max(e.line.bbox[2] for e in group)
        y1 = max(e.line.bbox[3] for e in group)
        out.append(Row(
            scan_page=page.scan_page,
            block_ids=[f"p{page.scan_page:04d}l{e.line.index:04d}" for e in group],
            bbox=(x0, y0, x1, y1),
            raw_texts=[e.line.raw_text for e in group],
            spanning=any(e.column is Column.SPANNING for e in group),
            columns=sorted({e.column.value for e in group})))
    return out


def _pitch(rows: List[Row]) -> float:
    tops = [row.bbox[1] for row in rows]
    steps = [tops[i + 1] - tops[i] for i in range(len(tops) - 1)]
    return statistics.median(steps) if steps else 1.0


def _upper_ratio(text: str) -> float:
    letters = [c for c in text if c.isalpha()]
    if not letters:
        return 0.0
    return round(sum(1 for c in letters if c.isupper()) / float(len(letters)), 3)


def inspect_page(page, placed, *, book=None, represented=None):
    """Las filas de una plana que merecen una visita al facsímil.

    Devuelve (candidatos, motivos de descarte). Una pasada por las filas
    de la plana: O(bloques).
    """
    represented = represented or {}
    rows = rows_of(page, placed)
    out, discarded = [], {}
    # La cabecera corrida y el pie no compiten por ser rótulos, pero se
    # cuentan: el filtro se audita entero, no sólo su resultado.
    outside = sum(1 for entry in placed if entry.zone is not Zone.BODY)
    if outside:
        discarded[NOT_BODY] = outside
    if len(rows) < 4:
        return out, discarded
    pitch = _pitch(rows) or 1.0
    width = float(page.width)

    for index, row in enumerate(rows):
        x0, y0, x1, y1 = row.bbox
        row_width = (x1 - x0) / width
        off_centre = abs((x0 + x1) / 2 - width / 2) / width
        previous = rows[index - 1] if index else None
        following = rows[index + 1] if index + 1 < len(rows) else None
        gap_before = ((y0 - previous.bbox[3]) / pitch) if previous else 99.0
        gap_after = ((following.bbox[1] - y1) / pitch) if following else 99.0
        tokens = sum(len(text.split()) for text in row.raw_texts)

        def reject(why):
            discarded[why] = discarded.get(why, 0) + 1

        if row_width > MAX_WIDTH_RATIO:
            reject(TOO_WIDE)
            continue
        if off_centre > MAX_OFF_CENTRE:
            reject(OFF_CENTRE)
            continue
        if gap_before < MIN_GAP_BEFORE:
            reject(NO_GAP)
            continue
        if tokens > MAX_TOKENS:
            reject(TOO_MANY_TOKENS)
            continue

        # Debajo tiene que venir el argumento del editor: una línea
        # ancha. Vale que cruce el canal --lo normal-- o que el propio
        # rótulo lo cruce y debajo haya texto ancho a dos columnas.
        argument_at = None
        for step in range(1, ARGUMENT_WITHIN + 1):
            if index + step >= len(rows):
                break
            other = rows[index + step]
            other_width = (other.bbox[2] - other.bbox[0]) / width
            if other.spanning and other_width >= ARGUMENT_WIDTH:
                argument_at = step
                break
        next_width = (((following.bbox[2] - following.bbox[0]) / width)
                      if following else None)
        wide_below = (next_width is not None and next_width >= ARGUMENT_WIDTH
                      and next_width >= ARGUMENT_RATIO * row_width)
        if argument_at is None and not (row.spanning and wide_below):
            reject(NO_ARGUMENT)
            continue

        raw = row.raw
        evidence = [
            f"row {row_width:.3f} of the page wide, centred to "
            f"{off_centre:.3f}",
            f"{gap_before:.2f} line-pitches of white above it",
            (f"an editor's argument {argument_at} row(s) below"
             if argument_at is not None else
             "a wide line right below, and the row crosses the gutter"),
        ]
        if len(row.block_ids) > 1:
            evidence.append(f"the printed line reached the recognition in "
                            f"{len(row.block_ids)} separate blocks")

        if row.spanning and argument_at == 1 and row_width <= 0.35 \
                and off_centre <= 0.05:
            rank = HIGH
        elif row.spanning and argument_at is not None:
            rank = MEDIUM
        else:
            rank = LOW

        represented_by = None
        for block in row.block_ids:
            if block in represented:
                represented_by = represented[block]
                break
        if represented_by:
            evidence.append(f"already represented by review {represented_by}")

        out.append(Candidate(
            book=book or "", scan_page=page.scan_page,
            block_ids=list(row.block_ids), bbox=row.bbox, raw_text=raw,
            width_ratio=round(row_width, 3), off_centre=round(off_centre, 3),
            height=y1 - y0, gap_before=round(gap_before, 2),
            gap_after=round(gap_after, 2),
            next_width_ratio=round(next_width, 3) if next_width else None,
            argument_rows_below=argument_at, spanning=row.spanning,
            columns=list(row.columns), token_count=tokens,
            upper_ratio=_upper_ratio(raw), rank=rank, evidence=evidence,
            represented_by=represented_by))
    return out, discarded


def scan(pages, *, book_at=None, represented=None):
    """Recorre el tomo y devuelve la cola de candidatos y los descartes.

    El orden es el de lectura dentro de cada rango: misma entrada, misma
    lista.
    """
    candidates, discarded = [], {}
    for page, placed in pages:
        book = book_at(page.scan_page) if book_at else None
        found, why = inspect_page(page, placed, book=book,
                                  represented=represented)
        candidates.extend(found)
        for reason, count in why.items():
            discarded[reason] = discarded.get(reason, 0) + count
    order = {HIGH: 0, MEDIUM: 1, LOW: 2}
    candidates.sort(key=lambda c: (order[c.rank], c.scan_page, c.block_ids[0]))
    return candidates, discarded


def summary(candidates) -> dict:
    out = {"total": len(candidates), "by_rank": {}, "by_book": {},
           "already_represented": 0, "multi_block": 0}
    for candidate in candidates:
        out["by_rank"][candidate.rank] = out["by_rank"].get(candidate.rank, 0) + 1
        out["by_book"][candidate.book] = out["by_book"].get(candidate.book, 0) + 1
        if candidate.represented_by:
            out["already_represented"] += 1
        if candidate.multi_block:
            out["multi_block"] += 1
    return out
