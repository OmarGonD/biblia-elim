"""
El número de una división escrito con palabra.

    ROMAN NUMERALS AND WRITTEN ORDINALS ARE DIFFERENT EVIDENCE TYPES

Esta edición no numera igual la primera división de un libro que las
demás. Las demás llevan numeral romano --«SALMO XXIV.», «CAPÍTULO
XLIII.»-- y de eso sabe `roman.py`. La primera lleva la palabra:

    SALMO PRIMERO.
    CAPÍTULO PRIMERO.

Para `roman.py` eso no es un numeral roto: es que no hay numeral, y hace
bien en decirlo. El número está, pero escrito en otro sistema, y mezclar
los dos sistemas en el mismo lector sería empezar a aceptar «PRIMERO»
como si fuera una variante mal escrita de «I». No lo es. Por eso vive
aquí, aparte, con su propio estado y su propia procedencia.

Tres reglas gobiernan este módulo:

  * VOCABULARIO CERRADO. Sólo las formas que el tomo imprime de verdad,
    medidas una por una en el facsímil. No hay un analizador de
    ordinales españoles esperando por si acaso: lo que no está en la
    tabla no se lee, y decirlo es la respuesta correcta.

  * NORMALIZAR NO ES ENMENDAR. Se admite lo que no cambia qué palabra
    hay escrita --caja, acentos, la puntuación de alrededor, las formas
    Unicode--. No se admite cambiar una letra por otra que se le parece,
    ni reconstruir una palabra dañada, ni parecidos aproximados: para
    eso está la imagen.

  * EL GÉNERO NO ES EVIDENCIA. Que el rótulo diga «SALMO» no autoriza a
    leer «PRIMERO» donde el reconocimiento dejó otra cosa. La
    concordancia gramatical explicaría la lectura después de tenerla;
    no puede producirla.

Lo que sale de aquí es evidencia, no un capítulo: el número entra en la
estructura por `ChapterClaim` y `ClaimLedger`, como cualquier otro.
"""
import difflib
import re
import unicodedata
from dataclasses import dataclass
from typing import Optional

#: Se leyó una forma soportada, exacta tras normalizar.
RECOGNIZED = "recognized"
#: Hay algo en el sitio del número que se parece a una palabra ordinal,
#: pero no es ninguna de las soportadas. No se adivina: se dice.
UNSUPPORTED = "unsupported"
#: No hay palabra ordinal ninguna. Puede haber un romano, o nada.
ABSENT = "absent"

#: De dónde sale la lectura.
FROM_OCR = "ocr"
FROM_IMAGE = "image_review"

LANGUAGE = "es"

#: El vocabulario, medido en el tomo 3 y no supuesto: los siete libros
#: imprimen «PRIMERO» --las siete planas revisadas contra el facsímil--
#: y ninguna otra palabra ordinal aparece en un rótulo. Por eso la tabla
#: tiene una entrada y no veinte: añadir «SEGUNDO» sin haberlo visto
#: sería soportar una forma que el testigo no usa, y el día que aparezca
#: lo correcto es volver a mirar la plana, no haberlo adivinado antes.
VOCABULARY = {
    "primero": 1,
}

#: Umbral con el que una palabra se considera PARECIDA a una soportada.
#: Sólo sirve para OFRECER el renglón a revisión --descubrir puede ser
#: aproximado--; nunca para leerlo. Un candidato parecido se queda en
#: `unsupported` hasta que la imagen diga qué pone.
SIMILAR_AT = 0.70

#: Las letras con las que se escribe un numeral romano. Aquí no sirven
#: para leer ningún número --de eso sabe `roman.py` y este módulo no lo
#: toca--, sino para distinguir un vecino numérico de una palabra de
#: prosa: en «CAPÍTULO XXIV PRIMERO» lo que acompaña al ordinal es otro
#: número, y eso es un conflicto que hay que ver, no una frase.
_ROMAN_LETTERS = set("ivxlcdm")

#: Una «palabra» incluye los dígitos que el reconocimiento mete dentro
#: («PR1MERO»): así la palabra dañada llega entera al juicio en vez de
#: partirse en trozos que ya no se parecen a nada.
_WORD = re.compile(r"[^\W_]+", re.UNICODE)


@dataclass
class OrdinalReading:
    """Lo que se sabe del número escrito con palabra, y de dónde sale."""
    raw_token: Optional[str] = None
    normalized_token: Optional[str] = None
    value: Optional[int] = None
    status: str = ABSENT
    language: str = LANGUAGE
    source: str = FROM_OCR
    confidence: float = 0.0
    reason: str = ""

    @property
    def is_recognized(self) -> bool:
        return self.status == RECOGNIZED and self.value is not None

    def as_dict(self) -> dict:
        return {
            "raw_token": self.raw_token,
            "normalized_token": self.normalized_token,
            "value": self.value,
            "status": self.status,
            "language": self.language,
            "source": self.source,
            "confidence": self.confidence,
            "reason": self.reason,
        }


def normalize(token: str) -> str:
    """Caja, acentos, puntuación de alrededor y forma Unicode.

    Todo lo que se hace aquí deja la MISMA palabra escrita: «PRIMERO.»,
    «primero» y «PRIMÉRO» son la misma palabra en tres vestidos. Lo que
    no se hace aquí es igual de importante: no se cambia ninguna letra
    por otra, así que «PR1MERO» no sale «primero» ni «PRIMERA» sale
    «primero».
    """
    if not token:
        return ""
    folded = unicodedata.normalize("NFD", token)
    folded = "".join(c for c in folded if not unicodedata.combining(c))
    return folded.strip().strip(".,;:·•-–—_'\"«»()[]").casefold()


def _similar(word: str) -> Optional[str]:
    """La forma soportada a la que se parece, si se parece a alguna."""
    best, score = None, 0.0
    for known in VOCABULARY:
        ratio = difflib.SequenceMatcher(None, word, known).ratio()
        if ratio > score:
            best, score = known, ratio
    return best if score >= SIMILAR_AT else None


def read(text: str, *, source: str = FROM_OCR,
         strict: bool = True) -> OrdinalReading:
    """Lee el número escrito con palabra que haya en `text`.

    `text` es el SITIO DEL NÚMERO: lo que queda del rótulo después de su
    palabra de división. Devuelve siempre una lectura -- también cuando
    no hay nada que leer, porque «aquí no hay ordinal» es una respuesta
    y no un fallo.

    `strict` exige que la palabra ordinal sea lo único que hay en ese
    sitio. Es lo que separa un rótulo de una frase: en «SALMO PRIMERO.»
    el sitio del número lo ocupa la palabra entera, mientras que en
    «...conviene primero que todo...» la palabra va acompañada de la
    frase que la contiene. Sin esta condición, cualquier prosa con un
    ordinal dentro podría proponer un capítulo. Sólo el descubrimiento
    --que ofrece renglones, no los lee-- baja la exigencia.
    """
    reading = OrdinalReading(source=source)
    if not text or not text.strip():
        reading.reason = "nothing in the number slot"
        return reading

    words = _WORD.findall(text)
    if not words:
        reading.reason = "no word in the number slot"
        return reading

    content = [w for w in words if len(normalize(w)) >= 3]

    def _prose_neighbours(token):
        """Palabras del sitio del número que no son números.

        Un ordinal acompañado de un numeral romano sigue siendo el
        número de un rótulo --con dos lecturas que habrá que
        contrastar--; acompañado de una frase, no.
        """
        others = []
        for other in content:
            if other == token:
                continue
            folded = normalize(other)
            if folded and set(folded) <= _ROMAN_LETTERS:
                continue
            others.append(folded)
        return others

    # Se mira palabra por palabra: el sitio del número puede traer
    # basura del canto pegada («PRIMERO. ^»).
    similar = None
    for word in words:
        folded = normalize(word)
        if folded in VOCABULARY:
            reading.raw_token = word
            reading.normalized_token = folded
            prose = _prose_neighbours(word) if strict else []
            if prose:
                # La palabra está, pero acompañada de una frase: eso es
                # prosa que contiene un ordinal, no el número de un
                # rótulo.
                reading.status = ABSENT
                reading.reason = (
                    f"{folded!r} appears next to {prose[:4]}: that is prose "
                    f"containing an ordinal, not the number of a heading")
                return reading
            reading.value = VOCABULARY[folded]
            reading.status = RECOGNIZED
            reading.confidence = 0.95 if source == FROM_OCR else 0.99
            reading.reason = (f"{folded!r} is a supported written ordinal "
                              f"and reads exactly, with no letter changed")
            return reading
        if similar is None and len(folded) >= 4:
            match = _similar(folded)
            if match is not None:
                similar = (word, folded, match)

    if similar is not None:
        word, folded, match = similar
        reading.raw_token = word
        reading.normalized_token = folded
        reading.status = UNSUPPORTED
        reading.reason = (
            f"{folded!r} is not a supported written ordinal; it resembles "
            f"{match!r}, and resembling is not reading: only the facsimile "
            f"can say what the page prints")
        return reading

    reading.reason = "no written ordinal in the number slot"
    return reading


def looks_ordinal(text: str) -> bool:
    """¿Merece este renglón que alguien mire la plana?

    Para DESCUBRIR, no para leer: devuelve verdad tanto con la forma
    soportada como con una parecida. Lo que se haga después con cada una
    lo decide `read`, que sólo lee la exacta.
    """
    reading = read(text, strict=False)
    return reading.status in (RECOGNIZED, UNSUPPORTED)


def vocabulary() -> dict:
    """La tabla, para que el informe pueda decir qué se soporta."""
    return dict(VOCABULARY)
