"""
Capa geométrica: qué zona de la página ocupa cada renglón.

La edición 1832-1835 se imprime a dos columnas, latín a la izquierda y
español a la derecha. Eso NO se da por supuesto: se mide por página (ver
`split_columns`), y el idioma sólo sirve como auditoría posterior, nunca
para decidir la geometría.

Lo que esta capa aporta y el pipeline de 1882 no tenía en absoluto: el
argumento del editor de cada salmo **cruza el gutter**. Un bloque que
ocupa las dos columnas no puede ser la continuación de un versículo de
una columna, y eso se sabe por su caja, no por lo que dice.
"""
from dataclasses import dataclass
from enum import Enum
from typing import List, Optional

from source_ocr import SourceLine, SourcePage


class Column(Enum):
    LEFT = "left"
    RIGHT = "right"
    SPANNING = "spanning"
    UNKNOWN = "unknown"


class Zone(Enum):
    HEADER = "header"
    BODY = "body"
    APPARATUS = "apparatus"   # notas al pie
    FOOTER = "footer"


@dataclass(frozen=True)
class PageLayout:
    """Lo medido en una página concreta."""
    gutter: Optional[int]
    header_below: int
    apparatus_above: Optional[int]
    footer_above: int
    median_height: float


@dataclass
class PlacedLine:
    line: SourceLine
    column: Column
    zone: Zone


def _median(values):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    middle = len(ordered) // 2
    if len(ordered) % 2:
        return float(ordered[middle])
    return (ordered[middle - 1] + ordered[middle]) / 2.0


def find_gutter(page: SourcePage, *, bucket: int = 20) -> Optional[int]:
    """El canal entre columnas, medido en esta página.

    Se cuenta, para cada franja vertical, cuántos renglones la cubren, y
    se busca el mínimo dentro del tercio central. No se exige que el
    canal esté completamente libre: los bloques a dos columnas -- el
    argumento del editor, un encabezado centrado -- lo cruzan a
    propósito, y exigir un hueco limpio hacía que no se encontrase canal
    en las páginas que más falta hace distinguir.

    Se devuelve None cuando no hay un mínimo claro: página a una sola
    columna, o geometría que no permite afirmarlo. En ese caso los
    renglones quedan en Column.UNKNOWN, que es lo honesto.
    """
    if not page.lines or page.width <= 0:
        return None
    width = page.width
    slots = width // bucket + 1
    coverage = [0] * slots
    for line in page.lines:
        start = max(0, line.bbox[0]) // bucket
        stop = min(width, line.bbox[2]) // bucket
        for index in range(start, min(stop + 1, slots)):
            coverage[index] += 1

    low, high = int(slots * 0.33), int(slots * 0.67)
    if high <= low:
        return None
    middle = coverage[low:high]
    if not middle:
        return None
    floor = min(middle)
    # La referencia es lo que cubre una columna: la mediana de las
    # franjas con texto fuera del centro.
    outside = [c for i, c in enumerate(coverage)
               if c and not (low <= i < high)]
    if not outside:
        return None
    typical = _median(outside)
    # Canal creíble: el mínimo central es muy inferior a una columna.
    if typical <= 0 or floor > typical * 0.35:
        return None
    candidates = [low + i for i, c in enumerate(middle) if c == floor]
    centre = candidates[len(candidates) // 2]
    return centre * bucket + bucket // 2


def _column_breaks(lines, *, top, bottom):
    """Alturas donde una columna deja un hueco mayor de lo normal."""
    ordered = sorted((l for l in lines if top <= l.bbox[1] < bottom),
                     key=lambda l: l.bbox[1])
    if len(ordered) < 3:
        return []
    gaps = [(ordered[i + 1].bbox[1] - ordered[i].bbox[3], ordered[i + 1].bbox[1])
            for i in range(len(ordered) - 1)]
    typical = _median([g for g, _ in gaps]) or 1
    return [y for g, y in gaps if g > typical * 1.5]


def _find_apparatus(page, gutter, header_below, footer_above):
    """Dónde empiezan las notas al pie, si las hay.

    La raya de notas del impreso cruza la página entera, así que deja un
    hueco en LAS DOS columnas casi a la misma altura. Ese acuerdo entre
    columnas es la señal: un salto en una sola columna es un salmo que
    termina, no el aparato.
    """
    if gutter is None or not page.height:
        return None
    # Las notas van al pie, no a media página: un acuerdo entre columnas
    # más arriba es un salmo que acaba en las dos a la vez, no la raya de
    # notas. Se exige que esté en la última franja de la caja.
    lower = int(page.height * 0.85)
    left = _column_breaks([l for l in page.lines if l.bbox[2] <= gutter],
                          top=header_below, bottom=footer_above)
    right = _column_breaks([l for l in page.lines if l.bbox[0] >= gutter],
                           top=header_below, bottom=footer_above)
    if not left or not right:
        return None
    tolerance = max(int(page.height * 0.02), 40)
    shared = sorted(
        min(a, b) for a in left for b in right
        if abs(a - b) <= tolerance and a >= lower and b >= lower)
    if not shared:
        return None
    return shared[0] - tolerance // 2


def measure(page: SourcePage, *, gutter_hint: Optional[int] = None) -> PageLayout:
    """Las bandas horizontales de la página, medidas.

    La banda de notas se reconoce por el hueco vertical: el cuerpo acaba,
    hay un salto mayor que el interlineado normal, y debajo empieza el
    aparato. El pie de página (la marca del digitalizador) va aún más
    abajo y separado.
    """
    heights = [line.height for line in page.lines] or [0]
    median_height = _median(heights)
    header_below = int(page.height * 0.06)
    footer_above = int(page.height * 0.95)

    # El canal de un libro impreso es estable. Cuando una página concreta
    # no da estadística suficiente -- pocas líneas, una plana casi vacía
    # -- se usa el canal medido en el resto del tomo antes que declarar
    # la página indecidible.
    gutter = find_gutter(page)
    if gutter is None:
        gutter = gutter_hint
    apparatus_above = _find_apparatus(page, gutter, header_below, footer_above)

    return PageLayout(gutter=gutter, header_below=header_below,
                      apparatus_above=apparatus_above,
                      footer_above=footer_above, median_height=median_height)


def place(line: SourceLine, page: SourcePage, layout: PageLayout) -> PlacedLine:
    """Columna y banda de un renglón. Sólo geometría."""
    x0, y0, _x1, y1 = line.bbox

    if y1 <= layout.header_below or y0 < layout.header_below:
        zone = Zone.HEADER
    elif y0 >= layout.footer_above:
        zone = Zone.FOOTER
    elif layout.apparatus_above is not None and y0 >= layout.apparatus_above:
        zone = Zone.APPARATUS
    else:
        zone = Zone.BODY

    gutter = layout.gutter
    if gutter is None:
        column = Column.UNKNOWN
    elif line.bbox[2] <= gutter:
        column = Column.LEFT
    elif line.bbox[0] >= gutter:
        column = Column.RIGHT
    else:
        # Cruza el canal. Hay dos formas legítimas de hacerlo, y las dos
        # son bloques de las dos columnas:
        #
        #   ancho     -- el argumento del editor bajo cada división, o
        #                una nota a toda la plana;
        #   centrado  -- la marca de división misma («SALMO XXIV»), que
        #                el impreso compone corta y centrada sobre el
        #                canal.
        #
        # Medido en el tomo 3: los argumentos ocupan 1900-2600 px de
        # 3402, y las marcas de división 560-910 px con su punto medio a
        # menos del 2 % del centro de la plana. Sin la segunda condición
        # 271 divisiones reales quedaban en «columna indecidible».
        #
        # Lo que no cabe en ninguna de las dos es un renglón estrecho y
        # descentrado: ahí el OCR ha pegado trozos de las dos columnas y
        # no se sabe de quién es. Ese no se adjudica a nadie.
        crosses = min(line.bbox[2], page.width) - max(line.bbox[0], 0)
        centre = (line.bbox[0] + line.bbox[2]) / 2.0
        centred = abs(centre - page.width / 2.0) <= page.width * 0.10
        if crosses > page.width * 0.45:
            column = Column.SPANNING
        elif centred and crosses >= page.width * 0.15:
            column = Column.SPANNING
        else:
            column = Column.UNKNOWN
    return PlacedLine(line=line, column=column, zone=zone)


def split_columns(page: SourcePage, *,
                  gutter_hint: Optional[int] = None) -> List[PlacedLine]:
    """Todos los renglones de la página, colocados y en orden de lectura.

    El orden es por columna y luego por altura: primero la izquierda
    entera, después la derecha. Un bloque que cruza el canal se queda en
    su sitio vertical, porque pertenece a las dos.
    """
    layout = measure(page, gutter_hint=gutter_hint)
    placed = [place(line, page, layout) for line in page.lines]

    # Un bloque a dos columnas parte la página en bandas: lo que va
    # debajo pertenece a lo que ese bloque anuncia. Ordenar todos los
    # spanning por delante rompía la continuidad entre páginas -- una
    # división a media plana se adelantaba a los versículos que venían
    # antes que ella --, así que el orden es por banda y, dentro de cada
    # banda, columna izquierda entera y luego la derecha.
    cuts = sorted(p.line.bbox[1] for p in placed
                  if p.column is Column.SPANNING and p.zone is Zone.BODY)

    def band(entry):
        # Un bloque a dos columnas ABRE su banda: cuenta como corte de sí
        # mismo, de modo que queda por delante de lo que anuncia y por
        # detrás de lo que venía antes en la plana.
        return sum(1 for c in cuts if c <= entry.line.bbox[1])

    order = {Column.SPANNING: 0, Column.LEFT: 1, Column.RIGHT: 2,
             Column.UNKNOWN: 3}
    placed.sort(key=lambda p: (band(p), order[p.column], p.line.bbox[1],
                               p.line.bbox[0]))
    return placed


def language_audit(placed: List[PlacedLine], latin_hint, spanish_hint) -> dict:
    """Auditoría secundaria: ¿cae el latín donde la geometría dice?

    No corrige nada ni decide columnas. Sirve para que el informe pueda
    avisar si una página sale al revés de lo medido en el resto.
    """
    counts = {}
    for column in (Column.LEFT, Column.RIGHT):
        text = " ".join(p.line.raw_text.lower() for p in placed
                        if p.column is column and p.zone is Zone.BODY)
        padded = f" {text} "
        counts[column.value] = {
            "latin": sum(padded.count(f" {w} ") for w in latin_hint),
            "spanish": sum(padded.count(f" {w} ") for w in spanish_hint),
        }
    return counts
