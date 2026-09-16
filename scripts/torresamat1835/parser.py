"""
Clasificador de bloques de la edición 1832-1835.

La regla que lo gobierna todo, y que es la lección de la edición de 1882:

    reconocer QUE un bloque es una frontera es independiente de saber
    QUÉ número tiene.

El importador anterior las juntaba: pedía «SALMO» seguido de un romano
válido <= 150, y si el OCR había roto el numeral («SALMO LIM», «CAPITULO
XXVHulI», «CAPITULO:») la línea entera dejaba de ser frontera y caía al
cuerpo del versículo en curso. Aquí, si se reconoce la forma pero no el
número, sale un ChapterHeading con number=None y review_reason: sigue
siendo paratexto y no toca el texto bíblico.

Y el fail-safe: lo que no se sabe clasificar sale como UnclassifiedBlock
a la cola de revisión. Nunca se concatena al versículo anterior.
"""
import re
import unicodedata
from typing import Optional

from model import Block, BlockKind, Edition, Provenance

#: Palabras con que el impreso abre una división. La tolerancia al ruido
#: del OCR está aquí, en la PALABRA, que es larga y sobrevive bien; nunca
#: en el numeral, que es corto y se rompe.
_DIVISION_WORDS = (
    r"CAP[IÍ1T]{1,2}[UO0]?L[O0]",
    r"P?SALM[O0]",
)
_DIVISION_RE = re.compile(
    r"^\W*(?:" + "|".join(_DIVISION_WORDS) + r")\b\W*(?P<rest>.*)$",
    re.IGNORECASE,
)

#: El mismo marcador, pero en cualquier punto del renglón. El OCR de
#: estas ediciones pega la división al final del versículo anterior, a
#: veces con la basura del canto delante ("… que 3 er Ea, a, P al k
#: SALMO XX. 14 hare yo …"). Un renglón así no es un versículo limpio.
_DIVISION_ANYWHERE_RE = re.compile(
    r"\b(?:" + "|".join(_DIVISION_WORDS) + r")\b")

_ROMAN_VALUES = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
_ORDINAL_WORDS = {
    "PRIMERO": 1, "PRIMERA": 1, "SEGUNDO": 2, "TERCERO": 3, "CUARTO": 4,
    "QUINTO": 5, "SEXTO": 6, "SEPTIMO": 7, "OCTAVO": 8, "NOVENO": 9,
    "DECIMO": 10, "UNICO": 1,
}
_VERSE_RE = re.compile(r"^\s*(?P<num>\d{1,3})\s*[.,]?\s+(?P<text>\S.*)$")


def _fold(text: str) -> str:
    text = unicodedata.normalize("NFD", text)
    return "".join(c for c in text if unicodedata.category(c) != "Mn")


def roman_value(token: str) -> Optional[int]:
    """El valor del romano, o None si no es un romano limpio.

    Deliberadamente estricto y deliberadamente sin consecuencias: que
    devuelva None no degrada el bloque, sólo deja su número sin saber.
    """
    token = token.strip(" .,:;")
    if not token or any(c not in _ROMAN_VALUES for c in token.upper()):
        return None
    total = previous = 0
    for char in reversed(token.upper()):
        value = _ROMAN_VALUES[char]
        total += -value if value < previous else value
        previous = max(previous, value)
    return total or None


def division_number(rest: str) -> Optional[int]:
    """Número de la división si se lee con claridad, si no None."""
    head = rest.split()[0] if rest.split() else ""
    folded = _fold(head).upper().strip(" .,:;")
    if folded in _ORDINAL_WORDS:
        return _ORDINAL_WORDS[folded]
    if folded.isdigit():
        return int(folded)
    return roman_value(folded)


def carries_division_marker(line: str) -> bool:
    """¿Aparece una marca de división en cualquier punto del renglón?

    Exige que vaya en mayúsculas, que es como el impreso compone la
    división («SALMO LIII», «CAPITULO XXIX»). Dentro del texto corrido la
    misma palabra aparece en caja normal -- «Salmo de David», que es una
    inscripción canónica y no una frontera --, así que la caja distingue
    las dos cosas sin tener que interpretar lo que dicen.
    """
    return _DIVISION_ANYWHERE_RE.search(_fold(line)) is not None


def looks_like_division(line: str) -> bool:
    """¿Abre este renglón una división del impreso?

    Sólo mira la palabra. Un numeral ilegible no cambia la respuesta.
    """
    return _DIVISION_RE.match(_fold(line).upper()) is not None


def classify(line: str, prov: Provenance, *, expect_editorial: bool = False) -> Block:
    """Clasifica un renglón del cuerpo. Nunca devuelve texto por descarte."""
    stripped = line.strip()
    if not stripped:
        return Block(BlockKind.PARAGRAPH, "", prov)

    match = _DIVISION_RE.match(_fold(stripped).upper())
    if match:
        number = division_number(match.group("rest"))
        return Block(
            BlockKind.CHAPTER_HEADING,
            stripped,
            prov,
            number=number,
            review_reason=(None if number is not None else
                           "division recognised but its numeral is unreadable"),
        )

    verse = _VERSE_RE.match(stripped)
    if verse:
        # Un renglón numerado es versículo aunque venga justo detrás de
        # la división: el argumento del editor no lleva número, y no
        # todos los salmos traen argumento.
        #
        # Pero si además arrastra una marca de división más adelante, no
        # es un versículo limpio: es el renglón donde el impreso pegó la
        # frontera al texto. Ahí está el caso que partió Sal 18:15 en la
        # edición de 1882. No se acepta como texto bíblico: a revisión.
        if carries_division_marker(verse.group("text")):
            return Block(BlockKind.UNCLASSIFIED, stripped, prov,
                         number=int(verse.group("num")),
                         review_reason="verse line also carries a division "
                                       "marker; boundary not resolved")
        return Block(BlockKind.VERSE, verse.group("text").strip(), prov,
                     number=int(verse.group("num")))

    # El argumento del editor: debajo de la marca de división y sin
    # número de versículo. Lo dice su posición, no sus palabras.
    if expect_editorial:
        return Block(BlockKind.EDITORIAL_HEADING, stripped, prov)

    # Aquí es donde 1882 se rompió: no se sabe qué es esto. No se pega al
    # versículo anterior; se manda a revisión.
    return Block(BlockKind.UNCLASSIFIED, stripped, prov,
                 review_reason="unrecognised block; not attached to any verse")


def parse_lines(lines, *, witness: str, book: str, edition_id="TorresAmat1835",
                edition: Optional[Edition] = None) -> Edition:
    """Recorre los renglones de un libro y construye el modelo."""
    edition = edition or Edition(edition_id=edition_id)
    target = edition.book(book)
    chapter = None
    verse_number = None
    expect_editorial = False

    for index, raw in enumerate(lines, start=1):
        prov = Provenance(witness=witness, line=index)
        block = classify(raw, prov, expect_editorial=expect_editorial)

        if block.kind is BlockKind.PARAGRAPH and not block.text:
            continue

        if block.kind is BlockKind.CHAPTER_HEADING:
            # Una división cierra el versículo en curso pase lo que pase.
            # Si su número no se lee, o no es el que toca, se abre un
            # capítulo correlativo y el bloque queda marcado para
            # revisión: lo que no se hace nunca es seguir escribiendo en
            # el capítulo anterior.
            #
            # Los capítulos de un libro van seguidos, así que «el que
            # toca» es una comprobación estructural y no una lista por
            # libro. Es lo que ataja el caso «SALMO LIM»: L, I y M son
            # romanos válidos y dan 949, un número limpio y absurdo.
            expected = (chapter.number + 1) if chapter else 1
            number = block.number
            if number is None:
                block.review_reason = ("division recognised but its numeral "
                                       "is unreadable")
            elif number != expected:
                block.review_reason = (f"division numbered {number} where "
                                       f"{expected} was due")
            if block.review_reason:
                number = expected
                edition.review_queue.append(block)
            chapter = target.chapter(number)
            chapter.paratext.append(block)
            verse_number = None
            expect_editorial = True
            continue

        expect_editorial = False

        if chapter is None:
            edition.review_queue.append(block)
            continue

        if block.kind is BlockKind.EDITORIAL_HEADING:
            chapter.paratext.append(block)
            continue

        if block.kind is BlockKind.VERSE:
            verse_number = block.number
            chapter.verse(verse_number).blocks.append(block)
            continue

        # Unclassified y todo lo demás: a revisión, con su procedencia.
        # Jamás al versículo en curso.
        edition.review_queue.append(block)

    return edition


def mark_canonical_title(edition: Edition, book: str, chapter: int, verse: int):
    """Marca como inscripción el versículo que el impreso numera.

    Que un versículo sea la inscripción del salmo no se deduce del texto:
    lo dice el testigo. Esta función es el punto por el que esa decisión,
    tomada contra el facsímil, entra en el modelo -- y conserva el número
    nativo, sin mover nada al versículo 0.
    """
    slot = edition.book(book).chapter(chapter).verse(verse)
    if not slot.blocks:
        raise ValueError(f"{book} {chapter}:{verse} no tiene contenido")
    head = slot.blocks[0]
    if head.kind is not BlockKind.VERSE:
        raise ValueError(f"{book} {chapter}:{verse} no empieza por texto bíblico")
    slot.blocks[0] = Block(BlockKind.CANONICAL_TITLE, head.text, head.provenance,
                           number=head.number)
    return slot
