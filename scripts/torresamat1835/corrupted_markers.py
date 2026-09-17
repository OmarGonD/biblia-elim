"""
Rótulos que están en el reconocimiento con la PALABRA rota.

La task 117 encontró tres planas en las que el impreso tiene su rótulo,
el reconocimiento produjo el renglón, el numeral sobrevivió entero... y
aun así no había frontera, porque lo que el OCR destrozó fue la palabra:

    S A L M O X L.        compuesta letra a letra
    6'ALMO XCI.           la S leída como «6'»
    5ALMO CXXXVI.         la S leída como «5»

Ninguna lectura del texto puede ver ahí una división, y la cola de
`missing_heading` tampoco los señala: esa cola busca planas donde FALTA
el renglón, y aquí el renglón sobra de presente. Son dos problemas
distintos y hacen falta dos colas.

Lo que este módulo hace es SEÑALAR dónde mirar:

    CANDIDATE GENERATION MAY BE HEURISTIC
    RECOVERY MAY NOT

Aquí se combinan señales débiles -- la forma de la línea, la geometría
que ya juzga `divisions.judge`, un numeral que sobrevive al final, un
esqueleto parecido al de la palabra impresa -- para decidir a qué
renglones merece la pena ir con el facsímil delante. Ninguna de ellas
recupera nada: un candidato es una pregunta, y la respuesta la da la
imagen.

Y una cosa que este módulo NO hace, a propósito:

    no corrige el texto.

La equivalencia entre "5" y "S" vive dentro del cálculo de parecido y no
sale de él: no se escribe en ningún sitio un «SALMO» que el
reconocimiento no dijo, no se sustituye ningún glifo en el crudo y no se
deriva de aquí ninguna regla de corrección automática. El crudo sigue
diciendo «5ALMO CXXXVI.» para siempre.
"""
import difflib
import re
import unicodedata
from dataclasses import dataclass, field
from typing import List, Optional

import divisions
import parser as classifier
import roman
import structure
from layout import Column, Zone

#: Familias de marcador, derivadas del vocabulario del importador. No hay
#: una segunda lista de palabras en este módulo.
PSALM_LIKE = "psalm_like"
CHAPTER_LIKE = "chapter_like"
_FAMILY = {"SALMO": PSALM_LIKE, "CAPITULO": CHAPTER_LIKE}

#: Glifos que el reconocimiento confunde con letras en este testigo. Se
#: usan SÓLO para medir parecido: nunca para reescribir el texto. Que «5»
#: se parezca a «S» es una observación sobre la tipografía, no un
#: permiso para cambiar lo que dijo la máquina.
_CONFUSABLE = {"0": "O", "1": "I", "3": "E", "4": "A", "5": "S", "6": "S",
               "7": "T", "8": "B", "9": "P", "$": "S", "|": "I", "!": "I"}

#: Cuánto tiene que parecerse el esqueleto de la cabeza del renglón a la
#: palabra impresa para que valga la pena ir a mirar la plana.
SIMILAR_ENOUGH = 0.66
QUITE_SIMILAR = 0.85
#: Un rótulo es la palabra y su numeral. Se cuentan palabras de
#: contenido: los trozos en que el OCR parte el numeral no lo son.
MAX_CONTENT_WORDS = 3

HIGH = "high"
MEDIUM = "medium"
LOW = "low"

#: Por qué un renglón NO es candidato. Se conservan para poder auditar
#: el filtro, no sólo su resultado.
ALREADY_A_DIVISION = "already read as a division"
OUTSIDE_THE_BODY = "outside the body band"
NO_MARKER_SHAPE = "nothing shaped like the printed division word"
NO_NUMERAL = "no numeral survives at the end of the line"
READS_AS_PROSE = "the line reads as prose"
ALREADY_REPRESENTED = "already represented by a facsimile review"


@dataclass
class Candidate:
    """Un renglón que podría ser un rótulo con la palabra rota."""
    book: str
    scan_page: int
    block_id: str
    raw_text: str
    bbox: Optional[tuple]
    column: str
    zone: str
    marker_family: str
    marker_shape: str
    marker_similarity: float
    #: Lo que queda del numeral, en uno o varios trozos. Su valor sólo
    #: se anota cuando sobrevive entero; partido, lo dice la imagen.
    numeral_tokens: List[str]
    numeral_value: Optional[int]
    content_words: int
    prose_words: int
    upper_ratio: float
    centred: bool
    spans_gutter: bool
    geometry: str
    rank: str
    evidence: List[str] = field(default_factory=list)
    already_represented_by: Optional[str] = None

    @property
    def pdf_page(self) -> int:
        return self.scan_page + 1

    def as_dict(self) -> dict:
        out = dict(self.__dict__)
        out["bbox"] = list(self.bbox) if self.bbox else None
        out["pdf_page"] = self.pdf_page
        return out


def _fold(text: str) -> str:
    text = unicodedata.normalize("NFD", text)
    return "".join(c for c in text if unicodedata.category(c) != "Mn")


def shape_of(token: str) -> str:
    """El esqueleto de un trozo de texto, para COMPARAR y nada más.

    Quita tildes y puntuación, sube a caja alta y trata como letra los
    glifos que el reconocimiento confunde con ella. El resultado no se
    escribe en ninguna parte: sólo entra en un cálculo de parecido.
    """
    out = []
    for char in _fold(token).upper():
        char = _CONFUSABLE.get(char, char)
        if char.isalpha():
            out.append(char)
    return "".join(out)


def _numeral_suffix(tokens):
    """Lo que queda de numeral al final del renglón, si queda algo.

    Se lee con `roman.py` y sin permisividad: «XL.» vale 40 y «XXL» no
    vale nada. Puede quedar en un trozo o en varios, porque el
    reconocimiento parte los numerales igual que parte las palabras
    («X I X»), y entonces AQUÍ NO SE DECIDE CUÁNTO VALE: juntar los
    trozos sería inventar el número, que es justo lo que decide la
    imagen. Se devuelve lo que hay y, si es un solo trozo, su valor.

    Un numeral al final no demuestra que el renglón sea un rótulo: sólo
    dice que ahí queda algo que un rótulo tendría.
    """
    tail = []
    for token in reversed(tokens):
        bare = token.strip(".,;:·\'\"()[]-—*«»")
        if not bare:
            continue
        if roman.to_int(_fold(bare).upper()) is None:
            break
        tail.append(bare)
        if len(tail) >= 6:
            break
    if not tail:
        return [], None
    tail.reverse()
    value = roman.to_int(_fold(tail[0]).upper()) if len(tail) == 1 else None
    return tail, value


def _marker_match(head_tokens):
    """A qué palabra impresa se parece la cabeza del renglón, y cuánto.

    La cabeza puede venir en un solo trozo («5ALMO») o repartida en
    muchos («S A L M O»), así que se compara el esqueleto de todo lo que
    va delante del numeral, junto.
    """
    shape = "".join(shape_of(token) for token in head_tokens)
    best_word, best_ratio = None, 0.0
    for word in classifier.DIVISION_WORDS:
        ratio = difflib.SequenceMatcher(None, shape, word).ratio()
        if ratio > best_ratio:
            best_word, best_ratio = word, ratio
    return best_word, round(best_ratio, 3), shape


def _content_words(tokens):
    out = 0
    for token in tokens:
        letters = [c for c in _fold(token) if c.isalpha()]
        if len(letters) < 2:
            continue
        if roman.to_int("".join(letters).upper()) is not None:
            continue
        out += 1
    return out


def inspect(*, raw_text, column, zone, bbox, page_width, book=None,
            header_chapters=None, represented_by=None):
    """Mira un renglón y dice si merece una visita al facsímil.

    Devuelve (Candidate, None) o (None, motivo del descarte). Cuesta una
    pasada por los tokens del renglón: O(renglones), sin comparar unos
    con otros.
    """
    raw = (raw_text or "").strip()
    tokens = raw.split()
    if not tokens:
        return None, NO_MARKER_SHAPE

    # Lo que YA se lee como división no es asunto de esta cola.
    if classifier.carries_division_marker(raw):
        return None, ALREADY_A_DIVISION
    # Fuera del cuerpo no se componen divisiones: cabecera, pie y notas
    # quedan fuera salvo que alguien las traiga a mano.
    if zone is not Zone.BODY:
        return None, OUTSIDE_THE_BODY

    numeral_tokens, numeral_value = _numeral_suffix(tokens)
    if not numeral_tokens:
        return None, NO_NUMERAL
    limit = structure.chapter_limit(book) if book else None
    if numeral_value is not None and limit is not None and \
            numeral_value > limit:
        return None, NO_NUMERAL

    # La cabeza es lo que va delante de ese resto de numeral.
    head = tokens[:len(tokens) - len(numeral_tokens)]
    if not head:
        # Un numeral solo no es un rótulo: le falta la palabra.
        return None, NO_MARKER_SHAPE

    word, similarity, shape = _marker_match(head)
    if similarity < SIMILAR_ENOUGH:
        return None, NO_MARKER_SHAPE

    content = _content_words(head)
    prose = max(0, content - 1)
    if content > MAX_CONTENT_WORDS or prose >= 2:
        # «Salmo de David, cuando le perseguía…» tiene la palabra y
        # tiene un número, y no es un rótulo: es la inscripción del
        # salmo. La longitud la delata sin tener que interpretarla.
        return None, READS_AS_PROSE

    # La geometría la juzga quien ya la juzga. Se le pregunta por una
    # línea que sí lleva la palabra, porque lo que se quiere saber es
    # dónde está compuesto el renglón, no si el texto se lee.
    verdict = divisions.judge(
        raw_text=f"{word} {numeral_tokens[0]}", column=column, zone=zone,
        bbox=bbox, page_width=page_width, header_chapters=header_chapters)

    letters = [c for c in _fold(raw) if c.isalpha()]
    upper_ratio = (round(sum(1 for c in letters if c.isupper())
                         / float(len(letters)), 3) if letters else 0.0)
    centred = "centred" in verdict.signals
    spans = "spans_gutter" in verdict.signals

    numeral_text = " ".join(numeral_tokens)
    evidence = [f"head shape {shape!r} ~ {word} ({similarity})",
                (f"the numeral survives whole as {numeral_text!r} "
                 f"({numeral_value})" if numeral_value is not None else
                 f"a numeral survives in pieces, {numeral_tokens}: what it "
                 f"reads is for the facsimile to say, not for this queue"),
                f"geometry {verdict.classification.value}"]
    if centred:
        evidence.append("centred over the gutter")
    if spans:
        evidence.append("set across the gutter")
    if upper_ratio >= 0.6:
        evidence.append(f"upper case predominates ({upper_ratio})")

    if not verdict.is_boundary:
        # El sitio no es de una división. Se conserva como candidato de
        # última fila: el reconocimiento también estropea la geometría.
        rank = LOW
        evidence.append("but the composition is not that of a division here")
    elif spans and similarity >= QUITE_SIMILAR and content <= 1:
        rank = HIGH
    elif spans:
        rank = MEDIUM
    else:
        rank = LOW

    candidate = Candidate(
        book=book or "", scan_page=-1, block_id="", raw_text=raw,
        bbox=tuple(bbox) if bbox else None,
        column=column.value if hasattr(column, "value") else str(column),
        zone=zone.value if hasattr(zone, "value") else str(zone),
        marker_family=_FAMILY.get(word, word), marker_shape=shape,
        marker_similarity=similarity, numeral_tokens=list(numeral_tokens),
        numeral_value=numeral_value, content_words=content, prose_words=prose,
        upper_ratio=upper_ratio, centred=centred, spans_gutter=spans,
        geometry=verdict.classification.value, rank=rank, evidence=evidence,
        already_represented_by=represented_by)
    if represented_by:
        candidate.evidence.append(
            f"already represented by review {represented_by}")
    return candidate, None


def scan(pages, *, book_at=None, header_chapters=None, represented=None):
    """Recorre las planas ya colocadas y devuelve la cola de candidatos.

    `pages` son pares (página, renglones colocados); `book_at` dice de
    qué libro es cada plana; `represented` son los bloques que ya tienen
    una lectura del facsímil, que siguen saliendo en la cola marcados
    como tales para que nadie los revise dos veces.

    El orden es el de lectura y no depende de ningún diccionario: misma
    entrada, misma lista.
    """
    represented = represented or {}
    header_chapters = header_chapters or {}
    out, rejected = [], {}
    for page, placed_lines in pages:
        book = book_at(page.scan_page) if book_at else None
        for placed in sorted(placed_lines, key=lambda p: p.line.index):
            block_id = f"p{page.scan_page:04d}l{placed.line.index:04d}"
            candidate, why = inspect(
                raw_text=placed.line.raw_text, column=placed.column,
                zone=placed.zone, bbox=placed.line.bbox,
                page_width=page.width, book=book,
                header_chapters=header_chapters.get(page.scan_page),
                represented_by=represented.get(block_id))
            if candidate is None:
                rejected[why] = rejected.get(why, 0) + 1
                continue
            candidate.scan_page = page.scan_page
            candidate.block_id = block_id
            out.append(candidate)
    order = {HIGH: 0, MEDIUM: 1, LOW: 2}
    out.sort(key=lambda c: (order[c.rank], -c.marker_similarity,
                            c.scan_page, c.block_id))
    return out, rejected


def summary(candidates) -> dict:
    out = {"total": len(candidates), "by_rank": {}, "by_book": {},
           "by_marker_family": {}, "already_represented": 0}
    for candidate in candidates:
        out["by_rank"][candidate.rank] = out["by_rank"].get(candidate.rank, 0) + 1
        out["by_book"][candidate.book] = out["by_book"].get(candidate.book, 0) + 1
        family = candidate.marker_family
        out["by_marker_family"][family] = \
            out["by_marker_family"].get(family, 0) + 1
        if candidate.already_represented_by:
            out["already_represented"] += 1
    return out
