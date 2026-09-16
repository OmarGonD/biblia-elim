"""
Identidad de libro y de capítulo a partir de evidencia redundante.

El testigo repite la misma información en varios sitios, y ahí está la
oportunidad: la cabecera corrida de cada plana lleva el nombre del libro,
el número del capítulo en curso y la página impresa; el cuerpo lleva la
marca de división; y la secuencia previa dice qué tocaba. El OCR destroza
cada una de esas señales por separado, pero rara vez las tres a la vez.

La regla que no se toca: no resolver un número NO autoriza a inventarlo.
Una división que no se puede identificar sigue saliendo con
`number=None` y `review_required`, y el verso anterior no se contamina.
Y la secuencia nunca decide sola -- una plana perdida correría el tomo
entero.
"""
import difflib
import re
import sys
import unicodedata
from dataclasses import dataclass, field
from typing import List, Optional

sys.path.insert(0, "/home/ogonzales/Projects/biblia_elim/scripts/torresamat")

#: Nombres con que el impreso titula cada libro en su cabecera corrida.
#: Es metadato de la edición, no contenido: se usa como UNA señal más,
#: siempre acompañada del reinicio de numeración y del cambio sostenido
#: de cabecera. Nunca decide por sí solo.
BOOK_HEADER_NAMES = {
    "Ps":   ("LOS SALMOS", "SALMOS", "LIBRO DE LOS SALMOS"),
    "Prov": ("LOS PROVERBIOS", "PROVERBIOS", "LIBRO DE LOS PROVERBIOS"),
    "Eccl": ("ECLESIASTES", "EL ECLESIASTES"),
    "Song": ("CANTAR DE LOS CANTARES", "EL CANTAR", "CANTARES"),
    "Wis":  ("LA SABIDURIA", "SABIDURIA", "LIBRO DE LA SABIDURIA"),
    "Sir":  ("EL ECLESIASTICO", "ECLESIASTICO"),
    "Isa":  ("ISAIAS", "LA PROFECIA DE ISAIAS", "PROFECIA DE ISAIAS"),
}

#: Orden en que el tomo 3 los imprime, leído de su portada.
VOLUME3_ORDER = ("Ps", "Prov", "Eccl", "Song", "Wis", "Sir", "Isa")

_ROMAN = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
#: El OCR de esta plana confunde estas formas con romanos.
_ROMAN_OCR = str.maketrans({"1": "I", "l": "I", "|": "I", "!": "I",
                            "J": "I", "i": "I", "T": "I",
                            "0": "C", "O": "C", "U": "V", "Y": "V"})
_ROMAN_RE = re.compile(r"[IVXLCDM]{1,9}")


def fold(text: str) -> str:
    text = unicodedata.normalize("NFD", text)
    return "".join(c for c in text if unicodedata.category(c) != "Mn")


def skeleton(text: str) -> str:
    """Sólo letras, sin tildes, en mayúsculas: lo que sobrevive al OCR."""
    return re.sub(r"[^A-Z]", "", fold(text).upper())


def roman_value(token: str) -> Optional[int]:
    token = token.strip(" .,:;·'").upper()
    if not token or any(c not in _ROMAN for c in token):
        return None
    total = previous = 0
    for char in reversed(token):
        value = _ROMAN[char]
        total += -value if value < previous else value
        previous = max(previous, value)
    return total or None


def roman_candidates(text: str) -> List[int]:
    """Números romanos plausibles en un texto, con y sin arreglo de OCR."""
    found = []
    for variant in (fold(text).upper(), fold(text).upper().translate(_ROMAN_OCR)):
        for match in _ROMAN_RE.finditer(variant):
            value = roman_value(match.group(0))
            if value is not None and 1 <= value <= 200:
                found.append(value)
    ordered = []
    for value in found:
        if value not in ordered:
            ordered.append(value)
    return ordered


#: Palabras del aparato de cabecera cuyo esqueleto se confunde con un
#: romano. No son numerales, y contarlas como tales inventaría evidencia.
_HEADER_WORDS = tuple(sorted({
    skeleton(word)
    for names in BOOK_HEADER_NAMES.values()
    for name in names
    for word in name.split()
} | {"LIBRO", "LOS", "LA", "EL", "DE", "PROFECIA", "CAPITULO", "SALMO"}))


def _without_book_words(raw: str) -> str:
    """El texto de la cabecera sin las palabras de su propio título."""
    kept = []
    for token in re.split(r"(\s+)", raw):
        bones = skeleton(token)
        if not bones:
            kept.append(token)
            continue
        similar = any(
            difflib.SequenceMatcher(None, bones, word).ratio() >= 0.72
            for word in _HEADER_WORDS if abs(len(bones) - len(word)) <= 3)
        kept.append(" " if similar else token)
    return "".join(kept)


@dataclass
class HeaderReading:
    """Lo que se ha podido leer de la cabecera corrida de una plana."""
    scan_page: int
    raw: str
    book: Optional[str] = None
    book_score: float = 0.0
    chapters: List[int] = field(default_factory=list)
    printed_page: Optional[int] = None


def read_header(scan_page: int, raw: str, *, threshold: float = 0.62
                ) -> HeaderReading:
    """Libro, capítulo y página impresa candidatos de una cabecera.

    El parecido se mide sobre el esqueleto de letras, que es lo que
    resiste al OCR: «SGCLS8IASTICO» y «XCGLBflIA9TIC<X» siguen
    pareciéndose a ECLESIASTICO aunque no coincida ni un tercio de los
    caracteres.
    """
    reading = HeaderReading(scan_page=scan_page, raw=raw)
    bones = skeleton(raw)
    if len(bones) >= 4:
        best, best_score = None, 0.0
        for osis, names in BOOK_HEADER_NAMES.items():
            for name in names:
                target = skeleton(name)
                score = difflib.SequenceMatcher(None, bones, target).ratio()
                # También cuenta que el nombre aparezca dentro de una
                # cabecera más larga («LIBRO DE LOS PROVERBIOS»).
                if len(bones) > len(target):
                    for start in range(0, len(bones) - len(target) + 1):
                        window = bones[start:start + len(target)]
                        score = max(score, difflib.SequenceMatcher(
                            None, window, target).ratio())
                if score > best_score:
                    best, best_score = osis, score
        if best_score >= threshold:
            reading.book, reading.book_score = best, round(best_score, 3)

    # Las palabras del nombre del libro no pueden aportar numerales:
    # «LOS» se lee como 50 y «LIBRO» como 51, y eso fabricaría
    # corroboración donde no la hay. Se quitan antes de buscar romanos.
    reading.chapters = roman_candidates(_without_book_words(raw))
    pages = [int(n) for n in re.findall(r"\d{2,4}", fold(raw))]
    if pages:
        reading.printed_page = pages[-1]
    return reading


@dataclass
class BookSpan:
    osis: str
    first_page: int
    last_page: Optional[int] = None
    evidence: dict = field(default_factory=dict)


def book_spans(readings: List[HeaderReading], *, run: int = 3,
               order=VOLUME3_ORDER) -> List[BookSpan]:
    """Tramos de páginas por libro.

    Un cambio de libro no se acepta por una cabecera suelta: hace falta
    que la nueva lectura se sostenga durante varias planas seguidas y que
    el libro sea el siguiente del orden del tomo. Así una cabecera mal
    leída no parte el corpus, y un libro no puede aparecer dos veces.
    """
    position = {osis: index for index, osis in enumerate(order)}
    spans: List[BookSpan] = []
    pending, pending_from, streak = None, None, 0

    for reading in readings:
        if reading.book is None:
            continue
        current = spans[-1].osis if spans else None
        if reading.book == current:
            pending, streak = None, 0
            continue
        if reading.book == pending:
            streak += 1
        else:
            pending, pending_from, streak = reading.book, reading.scan_page, 1
        if streak < run:
            continue
        # Sólo se avanza en el orden del tomo: nunca se retrocede.
        if current is not None and position.get(reading.book, -1) <= \
                position.get(current, -1):
            pending, streak = None, 0
            continue
        if spans:
            spans[-1].last_page = pending_from - 1
        spans.append(BookSpan(osis=reading.book, first_page=pending_from,
                              evidence={"header_run": streak,
                                        "first_header_page": pending_from}))
        pending, streak = None, 0
    return spans


def book_at(spans: List[BookSpan], scan_page: int) -> Optional[str]:
    found = None
    for span in spans:
        if span.first_page <= scan_page:
            found = span.osis
        else:
            break
    return found


@dataclass
class Resolution:
    raw_numeral: Optional[str]
    parsed_candidate: Optional[int]
    resolved: Optional[int]
    method: str
    evidence: dict
    confidence: float
    review_required: bool


def chapter_limit(osis: str) -> Optional[int]:
    """Cuántos capítulos tiene el libro en la Vulgata, según SWORD."""
    try:
        import canon
    except ImportError:
        return None
    entry = canon.POR_OSIS.get(osis)
    return entry["caps"] if entry else None


def resolve_chapter(*, raw_numeral, header_chapters, previous, book,
                    ) -> Resolution:
    """Resuelve una división cruzando señales independientes.

    Señales: el numeral de la propia marca, el numeral de la cabecera
    corrida de esa plana, y lo que seguía en la secuencia. Se exige que
    DOS coincidan. La secuencia sola no basta nunca: una plana perdida
    correría el tomo entero, que es justo lo que no puede pasar.
    """
    limit = chapter_limit(book) if book else None
    # La propia palabra de la división aporta romanos («CAPITULO» trae C
    # y L, «SALMO» trae L y M). Se quita antes de leer el numeral, o se
    # estaría inventando un candidato a partir del rótulo.
    direct = roman_candidates(_without_book_words(raw_numeral or ""))
    direct_value = None
    for value in direct:
        if limit is None or value <= limit:
            direct_value = value
            break

    header = [c for c in header_chapters if limit is None or c <= limit]
    sequential = (previous + 1) if previous is not None else 1
    evidence = {"heading_candidates": direct[:4],
                "header_candidates": header[:4],
                "sequential": sequential,
                "book_chapter_limit": limit}

    # 1. El numeral se lee limpio y además concuerda con otra señal.
    if direct_value is not None:
        agrees = []
        if direct_value in header:
            agrees.append("running_header")
        if direct_value == sequential:
            agrees.append("sequence")
        if agrees:
            return Resolution(raw_numeral, direct_value, direct_value,
                              "direct_ocr", dict(evidence, agrees=agrees),
                              0.99 if len(agrees) > 1 else 0.9, False)

    # 2. El numeral está roto, pero cabecera y secuencia coinciden.
    if sequential in header:
        return Resolution(raw_numeral, direct_value, sequential,
                          "running_header_correlated",
                          dict(evidence, agrees=["running_header", "sequence"]),
                          0.85, False)

    # 3. Sólo la cabecera da un número, y es plausible como siguiente.
    if len(header) == 1 and previous is not None and header[0] == previous + 1:
        return Resolution(raw_numeral, direct_value, header[0],
                          "multi_signal",
                          dict(evidence, agrees=["running_header",
                                                 "sequence_adjacent"]),
                          0.8, False)

    # 4. El numeral se lee limpio pero nada lo confirma: se registra como
    #    candidato y queda para revisión, sin escribirlo como resuelto.
    if direct_value is not None:
        return Resolution(raw_numeral, direct_value, None, "unresolved",
                          dict(evidence, why="no corroborating signal"),
                          0.4, True)

    return Resolution(raw_numeral, None, None, "unresolved",
                      dict(evidence, why="numeral unreadable and "
                                         "uncorroborated"), 0.1, True)
