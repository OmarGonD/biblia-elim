"""
Numerales de dos cifras que el reconocimiento partió en dos palabras.

    FORM + GEOMETRY + ZONE + FACSIMILE PROVENANCE.

Esta edición compone los números de versículo con cifras de estilo
antiguo. El reconocimiento no las lee como cifras: devuelve letras. Y
cuando el número tiene dos cifras, además las separa en dos palabras, de
modo que el impreso

    10 Si uno va á caer…

llega aquí como «I o Si uno va á caer». La tanda 127 fue a mirar qué
imprime la plana en cada una de esas formas, y la 128 agotó la población
de las candidatas. Lo que quedó demostrado es lo que gobierna este
módulo, y conviene decirlo entero porque la mitad interesante es la que
prohíbe:

    LA FORMA SOLA NO DECIDE. «a» es unas veces la cifra 2 y otras la
    preposición; y, lo que remató el asunto, el 1 de estilo antiguo
    TAMBIÉN se reconoce como «a». Por eso «a a» vale 22 doce veces y 12
    una, y «a I» vale 21 diez veces y 2 una. Ninguna forma que empiece
    por «a» entra aquí, por muy redonda que parezca su estadística:
    equivocar la primera cifra no deja un versículo sin abrir, abre el
    versículo EQUIVOCADO, y eso es peor que no hacer nada.

    LA GEOMETRÍA SOLA TAMPOCO. Una «y» de conjunción cae dentro de la
    banda del marcador de vez en cuando. Lo que no aparece nunca --en
    los doscientos y pico renglones mirados, ni una vez-- es una pareja
    de dos glifos de un solo carácter que sea castellano de verdad al
    principio de un renglón. Forma compuesta Y banda: las dos.

    EL HUECO NO PINTA NADA. El valor sale de la tabla, que sale del
    facsímil. No se mira qué versículo falta, ni cuál es el anterior,
    ni cuál el siguiente. Si la tabla dice 22 donde el inventario
    esperaba 23, se abre el 22 y el desacuerdo se ve en el informe, que
    es donde tiene que verse.

El canon sólo puede rechazar, nunca proponer: el número que sale de aquí
pasa después por la guarda de `verse_markers`, igual que cualquier otro.
"""
import re
import statistics
from typing import Dict, List, Optional, Tuple

import parser as classifier

#: La tabla. Clave: los dos tokens EXACTOS que dejó el reconocimiento,
#: carácter por carácter -- sin bajar la caja, sin quitar acentos y sin
#: parecidos. Valor: la cifra que imprime la plana, leída en el facsímil
#: y registrada en `verse_boundary_reviews.json` (batch-127, batch-128).
#:
#: Sólo entran formas cuya primera pareja empieza por un «1» o una «I»,
#: que son las que la revisión agotó sin encontrar una sola discrepancia.
#: Las que empiezan por «a» están fuera a propósito; el motivo está
#: arriba y tiene nombre y apellidos en las reviews.
SAFE_COMPOUND_VERSE_GLYPHS: Dict[Tuple[str, str], int] = {
    ("I", "o"): 10,
    ("I", "I"): 11,
    ("I", "a"): 12,
    ("1", "1"): 11,
    ("1", "3"): 13,
    ("1", "4"): 14,
    ("1", "5"): 15,
    ("1", "8"): 18,
    ("1", "9"): 19,
}

#: Un marcador ordinario para calibrar: una palabra de una a tres cifras
#: decimales seguida de algo más. Es lo que se usa para saber DÓNDE
#: empieza un marcador en esta plana y en esta columna, y por eso tiene
#: que ser un marcador que el reconocimiento ya leyó bien: si hiciera
#: falta interpretarlo, la calibración heredaría el problema que viene a
#: resolver.
_PLAIN_NUMERAL = re.compile(r"^\d{1,3}$")

#: Mínimo de marcadores ordinarios para fiarse de la banda de una
#: columna. Con menos, la mediana es una anécdota y el matcher se
#: abstiene.
MIN_BAND_MARKERS = 3

#: Cuánto puede apartarse el marcador del centro de la banda. Se deriva
#: de la tipografía de la propia plana --media cifra de ancho-- y no de
#: una constante elegida a ojo; el suelo existe porque en algunas planas
#: el reconocimiento devuelve cifras estrechísimas y la medida del ancho
#: se vuelve inservible. Medido sobre las reviews: acepta 99 de 99
#: marcadores compuestos y no acepta ni uno de los 46 negativos.
BAND_WIDTH_FRACTION = 0.5
BAND_FLOOR_PX = 30

#: Por qué no se aplicó, cuando no se aplica. Se cuenta todo en el
#: informe: una recuperación que no se sabe por qué no ocurrió no se
#: puede auditar.
NO_TOKENS = "no_compound_token_pair"
GLUED_FRAME = "frame_glued_to_first_token"
NOT_IN_MAP = "form_not_in_safe_map"
NO_BAND = "no_trusted_marker_band"
OUT_OF_BAND = "outside_marker_band"
NO_TEXT = "no_verse_text_after_marker"
DIGIT_FOLLOWS = "digit_follows_marker"


def _is_frame(text: str) -> bool:
    return bool(text) and all(ch in classifier.SAFE_OUTER_MARKER_FRAME
                              for ch in text)


def marker_tokens(words) -> Tuple[Optional[int], Optional[str]]:
    """Índice de la primera palabra del MARCADOR, saltando el marco.

    Devuelve `(indice, None)` o `(None, motivo)`. La basura del canto y
    la puntuación arrastrada son palabras propias del reconocimiento y
    se saltan; lo que NO se hace es adivinar dónde empieza la cifra
    dentro de una palabra que trae el marco pegado. En ese caso se dice
    que no se sabe y no se toca nada: la caja de esa palabra empieza en
    la mancha, y medir la sangría sobre ella es medir la mancha. Era el
    defecto que la 127 dejó anotado.
    """
    for index, word in enumerate(words):
        text = word.text
        if _is_frame(text):
            continue
        if text.lstrip(classifier.SAFE_OUTER_MARKER_FRAME) != text:
            return None, GLUED_FRAME
        return index, None
    return None, NO_TOKENS


def marker_bbox(words, first: int) -> Tuple[int, int, int, int]:
    """La caja del numeral: la unión de las cajas de sus DOS glifos.

    Ni el marco ni la basura entran: empieza en el primer glifo del
    número y acaba en el segundo.
    """
    boxes = [words[first].bbox, words[first + 1].bbox]
    return (min(b[0] for b in boxes), min(b[1] for b in boxes),
            max(b[2] for b in boxes), max(b[3] for b in boxes))


def band_of(lines) -> Optional[Tuple[float, float, int]]:
    """(centro, ancho de cifra, nº de marcadores) de una columna.

    La banda se saca de los marcadores numéricos ORDINARIOS que hay en
    esa misma plana, esa misma columna y esa misma zona: dónde los pone
    el impreso es la única definición de «sangría de marcador» que no
    depende de que alguien elija un número. Si no hay bastantes, no hay
    banda y no se recupera nada en esa columna.
    """
    edges, widths = [], []
    for line in lines:
        words = getattr(line, "words", None)
        if not words or len(words) < 2:
            continue
        first, _ = marker_tokens(words)
        if first is None or first + 1 >= len(words):
            continue
        token = words[first].text
        if not _PLAIN_NUMERAL.match(token):
            continue
        edges.append(words[first].bbox[0])
        widths.append((words[first].bbox[2] - words[first].bbox[0])
                      / len(token))
    if len(edges) < MIN_BAND_MARKERS:
        return None
    return statistics.median(edges), statistics.median(widths), len(edges)


def trusted_anchor_count(lines) -> int:
    """Count the ordinary marker anchors used by :func:`band_of`.

    This deliberately exposes the count without changing the three-anchor
    policy.  A caller may distinguish the separately validated zero-anchor
    fallback from the other cases where no trusted band is available.
    """
    count = 0
    for line in lines:
        words = getattr(line, "words", None)
        if not words or len(words) < 2:
            continue
        first, _ = marker_tokens(words)
        if first is None or first + 1 >= len(words):
            continue
        if _PLAIN_NUMERAL.match(words[first].text):
            count += 1
    # Match the task-128 meaning of *trusted* anchors: a band with fewer
    # than the unchanged minimum is unavailable, so it contributes zero
    # trusted anchors rather than a partial band.
    return count if count >= MIN_BAND_MARKERS else 0


def tolerance(band) -> float:
    """Cuánto puede apartarse un marcador del centro de su banda."""
    return max(BAND_WIDTH_FRACTION * band[1], BAND_FLOOR_PX)


def match(line, band) -> Tuple[Optional[int], Optional[str], dict]:
    """(valor, texto, detalle) si el renglón abre versículo; si no, por qué.

    El orden de las comprobaciones es el orden en que se quiere leer el
    informe: primero si hay pareja, luego si la pareja está en la tabla,
    luego dónde cae, y por último si detrás hay versículo. Cada negativa
    tiene nombre.
    """
    words = getattr(line, "words", None) or []
    detail = {"form": None, "value": None, "marker_bbox": None,
              "marker_x0": None, "band_center": None, "indent": None,
              "tolerance": None}
    first, reason = marker_tokens(words)
    if first is None:
        return None, reason, detail
    if first + 1 >= len(words):
        return None, NO_TOKENS, detail
    one, two = words[first].text, words[first + 1].text
    # Dos glifos de UN carácter cada uno. No se normaliza el contenido:
    # con bajar la caja o quitar un acento, «Í o» pasaría por «I o» y la
    # tabla estaría diciendo algo que la plana no dijo.
    if len(one) != 1 or len(two) != 1:
        return None, NO_TOKENS, detail
    detail["form"] = f"{one} {two}"
    value = SAFE_COMPOUND_VERSE_GLYPHS.get((one, two))
    if value is None:
        return None, NOT_IN_MAP, detail
    detail["value"] = value
    box = marker_bbox(words, first)
    detail["marker_bbox"] = list(box)
    detail["marker_x0"] = box[0]
    if band is None:
        return None, NO_BAND, detail
    limit = tolerance(band)
    detail["band_center"] = band[0]
    detail["indent"] = box[0] - band[0]
    detail["tolerance"] = limit
    if abs(box[0] - band[0]) > limit:
        return None, OUT_OF_BAND, detail
    # Detrás del numeral tiene que venir versículo, igual que exige el
    # marcador limpio. Y si lo que sigue empieza por cifra, el impreso
    # tenía un número más largo que el reconocimiento partió: no se lee.
    rest = " ".join(word.text for word in words[first + 2:]).strip()
    if not rest:
        return None, NO_TEXT, detail
    if rest[:1].isdigit():
        return None, DIGIT_FOLLOWS, detail
    return value, rest, detail
