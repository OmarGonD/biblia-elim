"""
¿Es este bloque un RÓTULO del impreso, o un texto que dice «Salmo»?

    TEXT MATCH IS NOT STRUCTURE

El clasificador de bloques reconoce la palabra de división en cualquier
caja, y hace bien: el impreso compone «SALMO LIII.» pero el
reconocimiento devuelve «SALMO LVfir;» o «'; :/ SALMO c XX VII I.», y
exigir mayúsculas limpias perdería rótulos de verdad.

El precio de esa tolerancia es que la inscripción del salmo -- «Salmo de
David, cuando le perseguía su hijo Absalón», que es TEXTO del salmo y no
su rótulo -- también empieza por la palabra. En el tomo 3 hay seis
líneas así, y una de ellas se aceptó como Ps 1: ocupaba el número del
salmo primero, dejaba al rótulo verdadero sin él y se llevaba los
versículos del salmo 142.

Aquí se separan las dos preguntas:

    ¿casó la palabra?              -> parser.py
    ¿es esto FÍSICAMENTE un rótulo? -> este módulo

La primera autoridad es la composición de la plana, y no se mide otra
vez: la geometría la juzga `divisions.judge`, que es la que ya sabe que
en esta edición una división se compone A DOS COLUMNAS, cruzando el
canal, en la caja de texto. Una línea encerrada en una columna es texto
de esa columna.

Sobre eso se añade la FORMA del texto, que no decide sola:

    la palabra abre el renglón
    el numeral va pegado a la palabra
    el renglón es corto
    predomina la mayúscula
    no sigue prosa detrás

Ninguna de esas señales es autoridad por su cuenta -- el OCR degrada la
caja y descentra las cajas --, y por eso se combinan y hay una banda
intermedia que no resuelve: sale `review_required` y se informa.

Lo que este módulo NO mira, a propósito:

    el canon           (no hay «tiene que haber un Ps 1»)
    la secuencia       (que detrás vaya el 141 no hace rótulo a nada)
    el número de plana (ninguna regla puede nombrar una página)

Y una excepción explícita: si el facsímil se miró y dijo que ESE MISMO
bloque es un rótulo, esa lectura manda sobre la clasificación
automática. Al revés también: si dijo que no lo es, no lo es.
"""
import re
from dataclasses import dataclass, field
from typing import List, Optional

import divisions
import parser as classifier
from layout import Column, Zone

#: Peso de cada señal de FORMA. La geometría no está aquí: es requisito
#: previo, no un sumando que se pueda compensar con buena tipografía.
_WEIGHTS = {
    "marker_leads": 0.25,       # el renglón empieza por la palabra
    "numeral_adjacent": 0.30,   # el numeral va pegado a la palabra
    "compact": 0.20,            # un rótulo es corto; la prosa no
    "upper_case": 0.15,         # predomina la caja alta
    "no_prose_tail": 0.10,      # detrás del numeral no sigue hablando
}
#: Por encima: rótulo. Por debajo de `REVIEW_AT`: no es un rótulo. Entre
#: las dos: hay señales en las dos direcciones y no se decide aquí.
HEADING_AT = 0.55
REVIEW_AT = 0.35

#: Un rótulo del impreso es la palabra y su numeral («SALMO CXLII.»,
#: «CAPÍTULO PRIMERO.»). Se cuentan sólo las palabras DE CONTENIDO: los
#: trozos del numeral que el OCR separó («c XX VII I») no son palabras,
#: y la basura del canto («'; :/») tampoco.
CONTENT_WORDS = 3
UPPER_RATIO = 0.60
#: Por debajo de esto la caja ya no es evidencia de nada: el OCR de este
#: testigo degrada la caja alta con facilidad, así que sólo cuenta como
#: señal EN CONTRA cuando el renglón es abiertamente minúsculo.
LOWER_RATIO = 0.50
#: Palabras de prosa que hacen frase. Una sola puede ser un numeral que
#: el reconocimiento no supo leer («LVfir»); dos ya son texto.
PROSE_WORDS = 2

_LETTERS_RE = re.compile(r"[^\W\d_]", re.UNICODE)

#: De dónde sale el veredicto.
FROM_STRUCTURE = "structural_classification"
FROM_IMAGE_REVIEW = "image_review"


@dataclass
class HeadingEvidence:
    """Qué se sabe de este bloque COMO rótulo, y con qué apoyo."""
    is_heading: bool
    score: float = 0.0
    signals: List[str] = field(default_factory=list)
    rejections: List[str] = field(default_factory=list)
    review_required: bool = False
    source: str = FROM_STRUCTURE
    geometry: Optional[str] = None
    shape: dict = field(default_factory=dict)

    @property
    def rejection_reason(self) -> Optional[str]:
        return "; ".join(self.rejections) or None

    def as_dict(self) -> dict:
        return {
            "is_heading": self.is_heading, "score": self.score,
            "signals": list(self.signals), "rejections": list(self.rejections),
            "review_required": self.review_required, "source": self.source,
            "geometry": self.geometry, "shape": dict(self.shape),
        }


#: Letras con que se escribe un romano. Un token hecho sólo de éstas no
#: es una palabra de la frase: es el numeral, entero o en trozos.
_ROMAN_LETTERS = set("IVXLCDM")


def _letters(token: str) -> str:
    return "".join(_LETTERS_RE.findall(token))


def _marker_index(tokens) -> Optional[int]:
    for index, token in enumerate(tokens):
        if classifier.looks_like_division(token):
            return index
    return None


def _is_numeral_like(token: str) -> bool:
    """¿Este token es numeral y no palabra?

    No dice cuánto vale -- eso es de roman.py y no se toca aquí --, sólo
    si está hecho de las letras de un romano, de cifras o es un ordinal
    escrito. Sirve para no contar como prosa los pedazos en que el
    reconocimiento parte un numeral.
    """
    body = _letters(token)
    bare = token.strip(" .,;:·'\"()[]-—*")
    if bare.isdigit():
        return True
    if not body:
        return False
    if classifier.division_number(body) is not None:
        return True
    return all(c.upper() in _ROMAN_LETTERS for c in body)


def shape_of(raw_text: str) -> dict:
    """Las señales de FORMA del renglón. Sólo mide; no concluye.

    No hay reglas de lengua aquí: no se pregunta qué dicen las palabras
    de detrás, sólo cuántas son, de qué caja y si lo primero que sigue a
    la palabra de división es un numeral.
    """
    raw = (raw_text or "").strip()
    tokens = raw.split()
    content = [t for t in tokens
               if len(_letters(t)) >= 2 and not _is_numeral_like(t)]
    marker = _marker_index(tokens)
    out = {
        "tokens": len(tokens), "content_words": len(content),
        "marker_token": marker,
        "content_words_before_marker": 0,
        "numeral_after_marker": None,
        "prose_words_after_marker": 0,
        "lower_case_prose_words": 0,
        "upper_ratio": 0.0,
    }
    letters = _letters(raw)
    if letters:
        out["upper_ratio"] = round(
            sum(1 for c in letters if c.isupper()) / float(len(letters)), 3)
    if marker is None:
        return out

    out["content_words_before_marker"] = len(
        [t for t in tokens[:marker]
         if len(_letters(t)) >= 2 and not _is_numeral_like(t)])
    tail = tokens[marker + 1:]
    # El numeral inmediatamente posterior, leído por el mismo camino que
    # usa el clasificador: romano bien escrito, cifra u ordinal escrito.
    out["numeral_after_marker"] = classifier.division_number(" ".join(tail))
    # Prosa detrás: palabras de tres letras o más que no son numeral.
    # «CXLVII.» no cuenta; «de David» sí. La caja se anota aparte, que es
    # evidencia más débil: el OCR la degrada.
    prose, lower = 0, 0
    for token in tail:
        body = _letters(token)
        if len(body) < 3 or _is_numeral_like(token):
            continue
        prose += 1
        if sum(1 for c in body if c.isupper()) * 2 <= len(body):
            lower += 1
    out["prose_words_after_marker"] = prose
    out["lower_case_prose_words"] = lower
    return out


def _composition_facts(column: Column, zone: Zone) -> List[str]:
    """Dónde está compuesto el bloque, dicho en palabras, PARA EL INFORME.

    Quien DECIDE sobre la composición es `divisions.judge` y aquí no se
    vuelve a medir nada: esto sólo nombra el valor de columna y de banda
    que ya vienen dados, para que un rechazo no se explique con la
    primera comprobación que falló cuando además el bloque está donde
    esta edición no compone divisiones.
    """
    out = []
    if column in (Column.LEFT, Column.RIGHT):
        out.append("set inside a single column, where this edition composes "
                   "no divisions")
    elif column is Column.UNKNOWN:
        out.append("column undecidable")
    if zone is Zone.APPARATUS:
        out.append("in the footnote band")
    elif zone in (Zone.HEADER, Zone.FOOTER):
        out.append("in the running header or footer")
    return out


def _shape_objections(shape: dict) -> List[str]:
    """Lo que la FORMA del renglón dice en contra de que sea un rótulo.

    Cada objeción se nombra por separado y se devuelven todas: el que
    lea el informe tiene que poder ver si la decisión se apoya en una
    señal o en cuatro.
    """
    out = []
    if shape["marker_token"] is None or shape["content_words_before_marker"]:
        out.append("the division word does not open the line")
    if shape["content_words"] > CONTENT_WORDS:
        out.append(f"{shape['content_words']} words of running text: longer "
                   f"than any heading in this edition")
    if shape["prose_words_after_marker"] >= PROSE_WORDS:
        out.append(f"{shape['prose_words_after_marker']} words follow the "
                   f"marker: the line reads as prose")
    if shape["upper_ratio"] < LOWER_RATIO:
        out.append(f"lower case predominates "
                   f"(upper-case ratio {shape['upper_ratio']})")
    return out


def judge(*, raw_text: str, column: Column, zone: Zone, bbox,
          page_width: int, header_chapters=None, verse_restart: bool = False,
          image_review_outcome: Optional[bool] = None) -> HeadingEvidence:
    """¿Es este bloque un rótulo de división del impreso?

    `image_review_outcome` es lo que dijo el facsímil de ESTE MISMO
    bloque: True si se leyó un rótulo, False si se comprobó que ahí no
    hay ninguno, None si nadie lo ha mirado. Una revisión de numeral de
    otro bloque no dice nada de éste.

    Coste: una pasada por los tokens del renglón. Por reclamo, no por
    pares de reclamos.
    """
    shape = shape_of(raw_text)
    against = _shape_objections(shape)

    if image_review_outcome is not None:
        # El facsímil mirado gana a la clasificación automática, en los
        # dos sentidos. Es la mejor evidencia que hay y apunta a este
        # bloque, no a su vecindad.
        return HeadingEvidence(
            is_heading=bool(image_review_outcome),
            score=1.0 if image_review_outcome else 0.0,
            signals=["facsimile_read_as_heading"] if image_review_outcome
            else [],
            rejections=[] if image_review_outcome else
            ["the facsimile shows no chapter heading in this block"],
            source=FROM_IMAGE_REVIEW, shape=shape)

    # --- 1. la composición de la plana --------------------------------
    verdict = divisions.judge(
        raw_text=raw_text, column=column, zone=zone, bbox=bbox,
        page_width=page_width, header_chapters=header_chapters,
        verse_restart=verse_restart)
    geometry = verdict.classification.value
    if not verdict.is_boundary:
        # Se informa de TODO lo que sostiene el rechazo, no sólo de la
        # primera comprobación que falló: así se ve si la decisión se
        # apoya en una señal o en varias.
        reasons = list(verdict.rejections)
        for fact in _composition_facts(column, zone) + against:
            if fact not in reasons:
                reasons.append(fact)
        return HeadingEvidence(
            is_heading=False, score=0.0, signals=list(verdict.signals),
            rejections=reasons, geometry=geometry, shape=shape)

    # --- 2. la forma del renglón --------------------------------------
    #
    # Las señales a favor suman un marcador de calidad. Las señales de
    # PROSA no restan: vetan. Un numeral pegado a la palabra no convierte
    # en rótulo una frase que sigue hablando, y sumar y comparar con un
    # umbral era exactamente lo que dejaba pasar «CAPÍTULO XXV de Isaías,
    # donde el Señor prepara un convite…».
    signals, score = [], 0.0
    leads = (shape["marker_token"] is not None
             and shape["content_words_before_marker"] == 0)
    compact = shape["content_words"] <= CONTENT_WORDS

    if leads:
        signals.append("marker_leads")
        score += _WEIGHTS["marker_leads"]
    if shape["numeral_after_marker"] is not None:
        signals.append("numeral_adjacent")
        score += _WEIGHTS["numeral_adjacent"]
    if compact:
        signals.append("compact")
        score += _WEIGHTS["compact"]
    if shape["upper_ratio"] >= UPPER_RATIO:
        signals.append("upper_case")
        score += _WEIGHTS["upper_case"]
    if not shape["prose_words_after_marker"]:
        signals.append("no_prose_tail")
        score += _WEIGHTS["no_prose_tail"]
    score = round(min(score, 1.0), 3)

    # Una sola señal en contra puede ser el OCR: un numeral que no se
    # deja leer parece una palabra, y la caja alta se degrada. Dos ya no
    # se explican por el reconocimiento.
    if len(against) >= 2:
        return HeadingEvidence(False, score, signals, against,
                               geometry=geometry, shape=shape)
    if against:
        # Duda: no se rechaza un rótulo por dudar de él. Se conserva
        # como rótulo y se informa para que alguien lo mire.
        if score >= HEADING_AT:
            return HeadingEvidence(True, score, signals, against, True,
                                   geometry=geometry, shape=shape)
        return HeadingEvidence(False, score, signals,
                               against + ["and nothing else looks like a "
                                          "heading either"],
                               geometry=geometry, shape=shape)
    return HeadingEvidence(True, score, signals, [], False,
                           geometry=geometry, shape=shape)
