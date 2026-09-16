"""
Decidir si un bloque es frontera de capítulo -- y sólo eso.

Detectar la frontera y ponerle número son dos cosas distintas, y aquí se
hace únicamente la primera. Una frontera confirmada puede salir con
`number=None`: eso es correcto y es lo que impide repetir el fallo
histórico, donde no saber leer el numeral acababa metiendo el rótulo
dentro del versículo anterior.

Medido sobre el tomo 3 real (652 planas): de las 286 marcas de división
que el OCR conserva, 282 están en un bloque que cruza el canal y en la
banda de cuerpo; 4 caen en la banda de cabecera y son la cabecera
corrida, no una división. No hay ninguna marca en columna simple, así
que no se pierde ninguna frontera por geometría: las que faltan frente
al canon (Ps 107 de 150) son planas donde el OCR ha perdido el propio
rótulo, y ésas no se inventan.
"""
from dataclasses import dataclass, field
from enum import Enum
from typing import List, Optional

import parser as classifier
from layout import Column, Zone


class Boundary(Enum):
    CONFIRMED = "confirmed_boundary"
    PROBABLE = "probable_boundary_review"
    REJECTED = "not_boundary"


@dataclass
class Verdict:
    classification: Boundary
    signals: List[str] = field(default_factory=list)
    rejections: List[str] = field(default_factory=list)
    score: float = 0.0

    @property
    def is_boundary(self) -> bool:
        return self.classification in (Boundary.CONFIRMED, Boundary.PROBABLE)

    @property
    def review_required(self) -> bool:
        return self.classification is Boundary.PROBABLE


#: Peso de cada señal estructural. Ninguna basta por sí sola salvo la
#: combinación de rótulo y geometría, que es la que el impreso usa.
_WEIGHTS = {
    "division_word": 0.55,     # el rótulo, aunque el numeral esté roto
    "spans_gutter": 0.30,      # cruza el canal: no es de una columna
    "centred": 0.10,           # compuesto centrado sobre el canal
    "body_zone": 0.05,         # en la caja de texto, no en el aparato
    "running_header": 0.10,    # la cabecera de la plana lo corrobora
    "verse_restart": 0.10,     # la numeración vuelve a empezar
}
CONFIRM_AT = 0.85
PROBABLE_AT = 0.60


def judge(*, raw_text: str, column: Column, zone: Zone, bbox, page_width: int,
          header_chapters=None, verse_restart: bool = False,
          seen_on_page: Optional[set] = None) -> Verdict:
    """Clasifica un bloque como frontera, frontera dudosa o no frontera.

    `verse_restart` y `header_chapters` son evidencia SECUNDARIA: suman,
    pero ninguna de las dos crea una frontera por su cuenta. El OCR
    pierde el «1» de los versículos, las inscripciones vulgatas cuentan
    como versículo y los números de página se parecen a numerales, así
    que fiarse de ellas solas fabricaría capítulos.
    """
    signals, rejections, score = [], [], 0.0

    if not classifier.carries_division_marker(raw_text or ""):
        return Verdict(Boundary.REJECTED, rejections=["no division marker"])
    signals.append("division_word")
    score += _WEIGHTS["division_word"]

    # --- rechazos duros: el bloque no puede ser una división ----------
    if zone is Zone.APPARATUS:
        rejections.append("inside the footnote band")
    if zone in (Zone.HEADER, Zone.FOOTER):
        rejections.append("in the running header or footer, not the text")
    if column is Column.LEFT or column is Column.RIGHT:
        rejections.append("confined to a single column; a division is set "
                          "across the gutter in this edition")
    if column is Column.UNKNOWN:
        rejections.append("column undecidable")
    if seen_on_page and raw_text.strip() in seen_on_page:
        rejections.append("duplicate of another candidate on the same page")
    if rejections:
        return Verdict(Boundary.REJECTED, signals=signals,
                       rejections=rejections, score=score)

    # --- señales que suman --------------------------------------------
    if column is Column.SPANNING:
        signals.append("spans_gutter")
        score += _WEIGHTS["spans_gutter"]
    if bbox and page_width:
        centre = (bbox[0] + bbox[2]) / 2.0
        if abs(centre - page_width / 2.0) <= page_width * 0.10:
            signals.append("centred")
            score += _WEIGHTS["centred"]
    if zone is Zone.BODY:
        signals.append("body_zone")
        score += _WEIGHTS["body_zone"]
    if header_chapters:
        signals.append("running_header")
        score += _WEIGHTS["running_header"]
    if verse_restart:
        signals.append("verse_restart")
        score += _WEIGHTS["verse_restart"]

    score = round(min(score, 1.0), 3)
    if score >= CONFIRM_AT:
        return Verdict(Boundary.CONFIRMED, signals, rejections, score)
    if score >= PROBABLE_AT:
        return Verdict(Boundary.PROBABLE, signals, rejections, score)
    return Verdict(Boundary.REJECTED, signals,
                   rejections + ["not enough structural evidence"], score)


def exceeds_book(resolved: Optional[int], limit: Optional[int]) -> bool:
    """Si el número resuelto pasa del canon, algo va mal.

    Señal de auditoría, no motivo de borrado: el bloque sigue siendo una
    frontera; lo que queda en duda es su número.
    """
    return (resolved is not None and limit is not None and resolved > limit)
