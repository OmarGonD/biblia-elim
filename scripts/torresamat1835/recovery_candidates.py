"""
Qué planas merece la pena mirar en el facsímil.

La primera versión de esto ordenaba por el hueco entre divisiones
aceptadas, y el propio facsímil la desmintió: el mayor hueco de Salmos
--once planas-- no era un capítulo perdido, era el salmo 118, que tiene
176 versículos y ocupa esas planas con todo derecho. Un hueco grande
significa «aquí no hay rótulo», que es justo lo que hace un texto largo.

    longitud del hueco  !=  evidencia de capítulo perdido

La revisión de Sir XLVI enseñó dónde estaba de verdad el material
recuperable, y no era donde se buscaba. El reconocimiento SÍ había
escrito el rótulo de aquella plana; lo que destrozó fue su numeral
(«CAPÍTULO* XX vi» por «CAPÍTULO XLVI»), de modo que la frontera existía
y el capítulo se quedaba sin identificar. Eso parte la cola en dos
familias que no valen ni cuestan lo mismo:

    unresolved_numeral   la frontera es segura y sólo falta el número.
                         Barata de revisar y casi siempre concluyente.

    missing_heading      no hay rótulo de ninguna clase y la geometría
                         insinúa que podría faltar uno. Cara, y es la
                         familia que produjo el falso positivo del salmo
                         118: aquí se mira con desconfianza, y un hueco
                         de planas no basta por sí solo para mirar.

NINGUNA de estas señales confirma nada, y ninguna da un número: sólo
dicen dónde mirar. Este módulo no crea fronteras, no escribe estructura y
no entra en el modelo. Lo que decide es el facsímil, y esa decisión se
registra como dato en chapter_image_reviews.json.
"""
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import structure

#: Pesos de ordenación. No son probabilidades ni umbrales de aceptación:
#: sólo sirven para poner primero lo que más vale la pena mirar.
WEIGHTS = {
    # familia 1: la frontera ya es segura, sólo falta el número
    "unresolved_numeral": 1.00,
    "numeral_ambiguous": 0.20,
    # familia 2: podría faltar el rótulo entero
    "missing_ocr_zone": 0.40,
    "header_unreadable": 0.20,
    "verse_restart": 0.10,
    "page_gap": 0.10,
}

#: A partir de aquí un hueco vertical en la columna del cuerpo es mayor
#: que el interlineado y puede haberse comido un rótulo. Medido como
#: múltiplo del paso típico de la propia plana, no en píxeles absolutos.
_ZONE_FACTOR = 3.0

#: Un hueco de plana por debajo de esto es ordinario en un libro de
#: capítulos largos y no dice nada por sí mismo.
_GAP_PAGES = 3

_VERSE_ONE = re.compile(r"^\s*1\s*[.·]?\s+\S")


@dataclass
class PageGeometry:
    """Lo que hace falta de cada plana para ordenar la cola."""
    scan_page: int
    max_body_gap: float = 0.0
    median_pitch: float = 0.0
    gap_after_block: Optional[str] = None
    gap_before_block: Optional[str] = None
    verse_restart: bool = False

    @property
    def zone_ratio(self) -> float:
        if self.median_pitch <= 0:
            return 0.0
        return self.max_body_gap / self.median_pitch


@dataclass
class RecoveryCandidate:
    family: str
    book: str
    scan_page: int
    score: float
    signals: Dict[str, object] = field(default_factory=dict)
    #: El rótulo ya presente cuyo numeral hay que ir a leer, cuando la
    #: familia es unresolved_numeral.
    target_block: Optional[str] = None
    gap_after_block: Optional[str] = None
    gap_before_block: Optional[str] = None

    @property
    def why(self) -> str:
        return ", ".join(sorted(self.signals))


def _median(values):
    ordered = sorted(values)
    if not ordered:
        return 0.0
    middle = len(ordered) // 2
    if len(ordered) % 2:
        return float(ordered[middle])
    return (ordered[middle - 1] + ordered[middle]) / 2.0


def page_geometry(page, placed) -> PageGeometry:
    """Mide el mayor hueco vertical del cuerpo de la columna española.

    Un rótulo que el reconocimiento no produjo deja exactamente eso: un
    blanco donde el impreso tiene tinta. La medida es relativa al paso
    de la propia plana, que cambia con la densidad de la composición.
    """
    from layout import Column, Zone

    body = [p for p in placed
            if p.zone is Zone.BODY and p.column is Column.RIGHT]
    body.sort(key=lambda p: p.line.bbox[1])
    geometry = PageGeometry(scan_page=page.scan_page)
    if len(body) < 3:
        return geometry
    pitch = _median([body[i + 1].line.bbox[1] - body[i].line.bbox[1]
                     for i in range(len(body) - 1)])
    geometry.median_pitch = pitch
    for index in range(len(body) - 1):
        gap = body[index + 1].line.bbox[1] - body[index].line.bbox[3]
        if gap > geometry.max_body_gap:
            geometry.max_body_gap = gap
            geometry.gap_after_block = (
                f"p{page.scan_page:04d}l{body[index].line.index:04d}")
            geometry.gap_before_block = (
                f"p{page.scan_page:04d}l{body[index + 1].line.index:04d}")
    geometry.verse_restart = any(
        _VERSE_ONE.match(p.line.raw_text) for p in body)
    return geometry


#: Las dos familias de candidato, que no valen lo mismo y no cuestan lo
#: mismo de revisar.
UNRESOLVED_NUMERAL = "unresolved_numeral"
MISSING_HEADING = "missing_heading"


def rank(*, readings, resolutions, spans, geometry,
         decided_blocks=()) -> List[RecoveryCandidate]:
    """Ordena la cola de revisión visual.

    `readings` son las cabeceras corridas leídas, `resolutions` los
    rótulos que el reconocimiento sí produjo, `spans` los tramos de
    libro y `geometry` lo medido plana a plana.

    `decided_blocks` son los bloques cuyo reclamo YA se decidió aunque no
    tenga número: una inscripción que se comprobó que no es un rótulo no
    espera a nadie. Sin esta lista, la cola pedía revisar seis renglones
    que la tanda 116 ya había rechazado, y «pendiente» acababa
    significando dos cosas a la vez.

    Salen dos familias, y la primera va delante porque es la que más
    devuelve por revisión:

      unresolved_numeral  el rótulo ESTÁ -- la frontera es segura -- y lo
                          único perdido es el número. Es el caso que se
                          verificó en Sir XLVI: el reconocimiento escribió
                          «CAPÍTULO* XX vi» y ahí se quedó. Mirar la
                          imagen resuelve un capítulo entero.

      missing_heading     no hay rótulo de ninguna clase y la geometría
                          dice que podría faltar uno. Es la familia cara
                          y la que produjo el falso positivo del salmo
                          118: aquí se mira con desconfianza.
    """
    by_page = {r.scan_page: r for r in readings}
    pages_with_heading = set()
    candidates: List[RecoveryCandidate] = []

    # --- familia 1: la frontera está, el número no --------------------
    decided = set(decided_blocks or ())
    for item in resolutions:
        page = item["scan_page"]
        pages_with_heading.add(page)
        if item["resolved_number"] is not None or item.get("recovered"):
            continue
        if item.get("block_id") in decided:
            continue
        raw = item.get("source_heading") or ""
        book = item["book"]
        limit = structure.chapter_limit(book)
        seen = [v for v in structure.roman_candidates(
            structure._without_book_words(raw))
            if limit is None or v <= limit]
        signals = {
            "heading_detected": item.get("block_id"),
            ("numeral_ambiguous" if len(seen) > 1 else "numeral_unreadable"):
                seen or "no roman numeral survived",
        }
        geo = geometry.get(page)
        score = WEIGHTS["unresolved_numeral"] + (
            WEIGHTS["numeral_ambiguous"] if len(seen) > 1 else 0.0)
        candidates.append(RecoveryCandidate(
            family=UNRESOLVED_NUMERAL, book=book, scan_page=page,
            score=round(score, 3), signals=signals,
            target_block=item.get("block_id"),
            gap_after_block=geo.gap_after_block if geo else None,
            gap_before_block=geo.gap_before_block if geo else None))

    # --- familia 2: no hay rótulo de ninguna clase ---------------------
    for span in spans:
        last = span.last_page
        if last is None:
            last = max(geometry) if geometry else span.first_page
        pages = [p for p in sorted(geometry) if span.first_page <= p <= last]
        previous_heading = None
        for page in pages:
            if page in pages_with_heading:
                previous_heading = page
                continue
            signals: Dict[str, object] = {}
            geo = geometry.get(page)
            if geo is not None and geo.zone_ratio >= _ZONE_FACTOR:
                signals["missing_ocr_zone"] = round(geo.zone_ratio, 2)
                if geo.verse_restart:
                    signals["verse_restart"] = True
            # Una plana que no dejó leer su cabecera ha perdido su parte
            # alta; ahí cabe un rótulo que tampoco se leyó.
            if page not in by_page or not by_page[page].chapters:
                signals["header_unreadable"] = (
                    "no header line" if page not in by_page
                    else by_page[page].raw[:60])
            if previous_heading is not None and \
                    page - previous_heading >= _GAP_PAGES:
                signals["page_gap"] = page - previous_heading

            # El hueco de plana NO manda a nadie a mirar una imagen por
            # sí solo: el salmo 118 ocupaba once planas sin que faltase
            # nada. Hace falta que además haya un blanco en la caja.
            if "missing_ocr_zone" not in signals:
                continue
            score = sum(WEIGHTS[name] for name in signals if name in WEIGHTS)
            candidates.append(RecoveryCandidate(
                family=MISSING_HEADING, book=span.osis, scan_page=page,
                score=round(score, 3), signals=signals,
                gap_after_block=geo.gap_after_block if geo else None,
                gap_before_block=geo.gap_before_block if geo else None))

    family_order = {UNRESOLVED_NUMERAL: 0, MISSING_HEADING: 1}
    candidates.sort(key=lambda c: (family_order[c.family], -c.score,
                                   c.book, c.scan_page))
    return candidates
