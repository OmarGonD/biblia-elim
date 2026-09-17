"""
Numerales romanos del impreso: leer, validar, convertir.

Hasta ahora esto era una sola función que recorría la cadena de derecha a
izquierda sumando y restando. Eso convierte CUALQUIER secuencia de letras
romanas en un número, y lo hace en silencio:

    XXL  ->  L(50) - X(10) - X(10)  =  30

«XXL» no es un número romano. Lo que el impreso tiene en esa plana es
XXI, y el reconocimiento leyó la I como L. Pero el converter devolvió 30
sin objetar, la cabecera corrida de la misma plana traía el mismo error,
y las dos lecturas «concordaron»: el capítulo 21 del Eclesiástico acabó
escrito como el 30.

De ahí que aquí vayan separadas tres cosas que no son la misma:

    NORMALIZACIÓN   qué glifo quiso decir el reconocimiento
    VALIDACIÓN      ¿es esto un número romano?
    CONVERSIÓN      ¿cuánto vale?

Sólo se convierte lo que valida. Y la validación es canónica: un token es
un romano si, y sólo si, escribir su valor en romano devuelve el mismo
token. Eso cubre de una vez las repeticiones ilegales (IIII, VV, XXXX),
las sustracciones ilegales (IL, IC, VX, LC) y el orden roto (IXX), sin
una lista de casos que siempre se queda corta.

Un numeral que no valida NO se arregla aquí. Sale con su motivo y queda
para que lo resuelva evidencia de fuera: una lectura del facsímil. Ni la
secuencia ni el canon pueden tocarlo.
"""
import re
import unicodedata
from dataclasses import dataclass, field
from typing import List, Optional

_VALUES = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
_CANON = ((1000, "M"), (900, "CM"), (500, "D"), (400, "CD"), (100, "C"),
          (90, "XC"), (50, "L"), (40, "XL"), (10, "X"), (9, "IX"),
          (5, "V"), (4, "IV"), (1, "I"))

TOKEN_RE = re.compile(r"[IVXLCDM]{1,9}")

#: Un numeral pegado a otro carácter que podría formar parte de él no
#: está leído entero. «XX1» da XX=20 si se mira sin más, y la plana pone
#: XXI=21: el uno es la I que el reconocimiento no supo componer. Un
#: token sólo cuenta cuando está delimitado por algo que no puede ser
#: parte de un numeral.
_ADJACENT = re.compile(r"[0-9A-Za-z|!]")

#: NIVEL A -- normalización tipográfica determinista.
#: El mismo trazo compuesto con otro carácter: un palote vertical es una
#: I, la venga como la venga. No cambia la identidad del glifo, así que
#: se aplica sin pedir permiso a nadie.
#:
#: OJO con la caja: esto se aplica ANTES de pasar a mayúsculas, porque la
#: ele minúscula y la ele mayúscula son cosas distintas. Normalizar
#: después convertiría «XXL» en «XXI» -- que da la casualidad de ser el
#: numeral correcto de la plana 411, y precisamente por eso es la trampa:
#: sería adivinar, no leer, y la siguiente vez adivinaría mal.
TYPOGRAPHIC = {"1": "I", "l": "I", "|": "I", "!": "I"}

#: NIVEL B -- confusión de reconocimiento plausible, PERO cambia el
#: glifo. J por I, Y por V, O por C: pasan de verdad en este testigo, y
#: por eso se conservan; pero una lectura que dependa de una de ellas no
#: es un numeral leído, es un numeral enmendado, y necesita que otra
#: señal independiente diga lo mismo.
CONFUSABLE = {"J": "I", "T": "I", "U": "V", "Y": "V", "O": "C", "0": "C"}

#: NIVEL C -- enmienda especulativa: cambiar letras hasta que el numeral
#: valide. No se hace. Nunca. Es el camino por el que «XXL» se
#: convertiría en XXI o en XXX según convenga, y eso no es leer el
#: impreso, es adivinar qué debería poner.

#: Estados de una lectura.
VALID = "valid"
INVALID_SYNTAX = "invalid_roman_syntax"
NO_NUMERAL = "no_numeral"
OUT_OF_RANGE = "out_of_range"

#: De dónde salió el valor, que es lo que decide cuánta autoridad tiene.
FROM_TYPOGRAPHIC = "typographic"
FROM_CONFUSABLE = "ocr_confusion"
FROM_LITERAL = "literal"


def fold(text: str) -> str:
    text = unicodedata.normalize("NFD", text)
    return "".join(c for c in text if unicodedata.category(c) != "Mn")


def to_roman(value: int) -> Optional[str]:
    """La única grafía canónica de un número."""
    if not isinstance(value, int) or not 1 <= value <= 3999:
        return None
    out = []
    for amount, glyph in _CANON:
        while value >= amount:
            out.append(glyph)
            value -= amount
    return "".join(out)


def accumulate(token: str) -> Optional[int]:
    """Lo que valdría el token si se le sumara y restara sin preguntar.

    Es el comportamiento antiguo, y se conserva SÓLO para poder decir
    qué número se estaba produciendo antes y contrastarlo. No lo usa
    ninguna decisión: para eso está `read`.
    """
    token = token.strip(" .,:;·'").upper()
    if not token or any(c not in _VALUES for c in token):
        return None
    total = previous = 0
    for char in reversed(token):
        value = _VALUES[char]
        total += -value if value < previous else value
        previous = max(previous, value)
    return total or None


def is_valid(token: str) -> bool:
    """¿Es `token` un número romano escrito como se escribe?"""
    token = (token or "").strip(" .,:;·'").upper()
    if not token or any(c not in _VALUES for c in token):
        return False
    value = accumulate(token)
    return value is not None and to_roman(value) == token


def to_int(token: str) -> Optional[int]:
    """El valor del token, o None si el token no es un romano."""
    if not is_valid(token):
        return None
    return accumulate((token or "").strip(" .,:;·'").upper())


@dataclass
class NumeralReading:
    """Lo que se ha podido leer de un numeral, y a qué precio."""
    raw: str
    token: Optional[str] = None
    normalized: Optional[str] = None
    value: Optional[int] = None
    status: str = NO_NUMERAL
    origin: Optional[str] = None
    reason: str = ""
    #: Todo lo que valida en el texto, en orden de aparición. Más de uno
    #: significa que el numeral no está leído, está disputado.
    candidates: List[int] = field(default_factory=list)
    #: Lo que el converter antiguo habría producido, para poder auditar
    #: la diferencia sin tener que reconstruirla.
    permissive_value: Optional[int] = None

    @property
    def is_valid(self) -> bool:
        return self.status == VALID

    @property
    def needs_corroboration(self) -> bool:
        """Una lectura que depende de enmendar un glifo no se sostiene sola."""
        return self.origin == FROM_CONFUSABLE

    @property
    def ambiguous(self) -> bool:
        return len(self.candidates) > 1

    def as_dict(self) -> dict:
        return {"raw": self.raw, "token": self.token,
                "normalized": self.normalized, "value": self.value,
                "status": self.status, "origin": self.origin,
                "reason": self.reason, "candidates": list(self.candidates),
                "permissive_value": self.permissive_value}


def _apply(text: str, table: dict) -> str:
    return "".join(table.get(c, c) for c in text)


def delimited_tokens(text: str) -> List[str]:
    """Los numerales del texto que están enteros.

    Se descarta el que lleve pegado un carácter que podría ser parte
    suya: si no se sabe dónde acaba el numeral, no se sabe qué numeral
    es, y quedarse con el trozo que casa produciría un número más bajo
    sin avisar.
    """
    out = []
    for match in TOKEN_RE.finditer(text):
        start, stop = match.span()
        before = text[start - 1] if start else ""
        after = text[stop] if stop < len(text) else ""
        if _ADJACENT.match(before) or _ADJACENT.match(after):
            continue
        out.append(match.group(0))
    return out


def variants(text: str):
    """El texto tal cual, y luego con cada nivel de normalización.

    En orden de menos a más intervención, para que quien busque pueda
    quedarse con el primero que valide y sepa cuánto se ha tocado.

    El nivel A se aplica sobre el texto SIN pasar a mayúsculas: es la
    única forma de que «l» minúscula se lea I sin que «L» mayúscula se
    lea I también.
    """
    folded = fold(text or "")
    literal = folded.upper()
    typographic = _apply(folded, TYPOGRAPHIC).upper()
    return ((literal, FROM_LITERAL),
            (typographic, FROM_TYPOGRAPHIC),
            (_apply(typographic, CONFUSABLE), FROM_CONFUSABLE))


def read(text: str, *, limit: Optional[int] = None) -> NumeralReading:
    """Lee el numeral de un texto, diciendo de dónde sale cada cosa.

    Se intenta primero literal, luego con la normalización tipográfica
    (nivel A) y sólo al final con las confusiones de reconocimiento
    (nivel B), que quedan marcadas como tales. En ningún momento se toca
    una letra para que el numeral valide: si no valida, no valida.

    `limit` descarta valores imposibles para el libro. Es una cota de
    cordura sobre lo LEÍDO, no una forma de elegir entre lecturas: no
    convierte un numeral inválido en válido ni al revés.
    """
    reading = NumeralReading(raw=text or "")
    folded = fold(text or "").upper()
    if not folded.strip():
        reading.reason = "the heading carries no numeral at all"
        return reading

    attempts = variants(text)

    tried = []
    #: Un romano bien escrito cuyo valor no cabe en el libro. No es lo
    #: mismo que un numeral roto: «XL» es un numeral perfecto que vale
    #: 40, y si el libro tiene 19 capítulos lo que falla es la
    #: correspondencia, no la escritura. Llamarlo «sintaxis inválida»
    #: mandaba a revisar con el diagnóstico equivocado.
    over_limit = None
    for variant, origin in attempts:
        found = []
        for token in delimited_tokens(variant):
            value = to_int(token)
            if value is None or not 1 <= value <= 200:
                continue
            if limit is not None and value > limit:
                if over_limit is None:
                    over_limit = (token, value)
                continue
            found.append((token, value))
        if not found:
            tried.append(variant)
            continue
        reading.normalized = variant
        reading.origin = origin
        reading.candidates = [value for _t, value in found]
        reading.token, reading.value = found[0]
        reading.status = VALID
        if len(found) > 1:
            reading.reason = (
                f"the text carries {len(found)} readable numerals "
                f"({', '.join(t for t, _v in found)}); which one is the "
                f"chapter is not decided here")
        elif origin is FROM_CONFUSABLE:
            reading.reason = ("read only after correcting a confusable glyph; "
                              "needs an independent signal to stand")
        break

    if reading.status != VALID:
        # Nada validó. Se dice QUÉ había y de qué clase es el problema,
        # porque no todos se revisan igual.
        if over_limit is not None:
            token, value = over_limit
            reading.status = OUT_OF_RANGE
            reading.token = token
            reading.value = None
            reading.permissive_value = value
            reading.reason = (
                f"{token!r} is a well-formed Roman numeral worth {value}, "
                f"which is beyond the {limit} chapters this book has; what "
                f"the page prints has to be read, not guessed")
        else:
            # Sólo cuentan los numerales que estén enteros. Sacar letras
            # de dentro de una palabra daba «IM» por «PRIMERO» y «VI» por
            # «VIH.», y presentaba esa basura como el numeral del rótulo.
            broken = [t for variant in (tried or [folded])
                      for t in delimited_tokens(variant)]
            if broken:
                reading.status = INVALID_SYNTAX
                reading.token = broken[0]
                reading.permissive_value = accumulate(broken[0])
                reading.reason = (
                    f"{broken[0]!r} is not a Roman numeral"
                    + (f"; a permissive reading would have made it "
                       f"{reading.permissive_value}"
                       if reading.permissive_value is not None else ""))
            else:
                reading.status = NO_NUMERAL
                reading.reason = (
                    "no whole Roman numeral in the heading: the letters that "
                    "look like one are stuck to other characters")
    return reading
