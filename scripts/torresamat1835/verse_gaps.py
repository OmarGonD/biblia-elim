"""
Huecos en la numeración de versículos: qué falta, y qué se ve al lado.

    VERSE GAP IS A QUESTION, NOT AN ANSWER

Que la versificación nativa numere un verso no demuestra que el impreso
marque ahí una frontera, y que falte un número entre dos que sí están no
demuestra que el texto de por medio sea ese verso. Este módulo localiza
la anomalía y describe lo que hay alrededor; no crea versículos, no
mueve texto y no toca el reconocimiento.

Lo primero que hay que decir es cómo se contaba antes, porque de ahí sale
la mayor parte del número:

    expected = 1 .. max(verso materializado)

Es una cuenta que se mira a sí misma. Si el reconocimiento leyó como
verso un número de página --«211» al pie de una plana de Isaías 26, que
tiene 21 versos--, esa sola lectura fabrica ciento noventa y ocho huecos
que no existen en ninguna plana. Por eso aquí cada hueco se compara
además con la versificación NATIVA de esta edición (la Vulgata, leída de
la cabecera de SWORD), y los que caen más allá del último verso canónico
se nombran por lo que son: el rastro de un numeral espurio, no un verso
perdido.

De cada hueco se guardan tres cosas distintas, y conviene no
confundirlas:

    FORMA      dónde cae el hueco --al principio, dentro, al final, o
               fuera del canon--. Es aritmética sobre lo materializado.

    SEÑALES    qué hay en los bloques vecinos: un número suelto, un
               numeral pegado a una palabra, dos versos compartiendo
               bloque, un salto de columna o de plana. Son indicios
               locales y baratos; ninguno afirma nada.

    DESENLACE  qué muestra el facsímil. No se calcula aquí: lo trae una
               revisión, y sin ella el hueco sigue siendo una pregunta.
"""
import collections
import hashlib
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional

#: Dónde cae el hueco. Aritmética, no diagnóstico.
LEADING = "leading_gap"
INTERIOR = "interior_gap"
TRAILING = "trailing_gap"
#: Más allá del último verso que la versificación nativa numera: el
#: capítulo materializó un número que no puede ser un verso suyo.
OVER_CANON = "over_canonical_top"

#: Señales locales. Varias pueden darse a la vez, y ninguna es un
#: resultado: dicen dónde mirar y con qué expectativa.
NUMERIC_FRAGMENT = "adjacent_numeric_fragment"
SUPERSCRIPT_LIKE = "adjacent_superscript_like_fragment"
ATTACHED_MARKER = "marker_attached_to_text_candidate"
BLOCK_MERGE = "ocr_block_merge_candidate"
BLOCK_SPLIT = "ocr_block_split_candidate"
COLUMN_TRANSITION = "column_transition_candidate"
PAGE_TRANSITION = "page_transition_candidate"
#: Dentro del verso anterior hay un renglón que empieza por el dígito
#: que falta: el marcador sobrevivió al reconocimiento y lo que se perdió
#: fue la frontera.
SWALLOWED_DIGIT = "missing_marker_digit_inside_previous_verse"
#: Y lo mismo con un glifo suelto que no es dígito. Esta edición numera
#: con cifras de estilo antiguo --el 2 tiene la forma de una «a»-- y el
#: reconocimiento las devuelve como letras. Es un indicio, no una
#: lectura: sólo la plana dice qué cifra hay ahí, y «y» o «á» son además
#: palabras castellanas que pueden empezar un renglón de verdad.
SWALLOWED_GLYPH = "lone_glyph_inside_previous_verse"
RECOVERED_BOUNDARY = "recovered_chapter_boundary_candidate"
SPURIOUS_TOP = "spurious_materialized_top"
NO_NUMERIC_EVIDENCE = "no_local_numeric_evidence"

#: Un número de verso del impreso es corto: esta edición numera hasta
#: 176 (Salmo 118). Un token de cuatro cifras es una página, no un verso.
_MAX_PLAUSIBLE = 200

_TOKEN = re.compile(r"[0-9]+")
#: Un renglón que empieza por un token corto y aislado, seguido de
#: palabra: la forma que tiene un marcador de verso en este impreso.
_LEADING_TOKEN = re.compile(
    r"^\s*(?:[^\s\w]{1,2}\s*)?([0-9]{1,3}|[^\W\d_])\s+\S")
#: Y el mismo sitio con el número pegado a la palabra, sin el espacio
#: que el impreso sí deja: «2sino que tiene puesta».
_GLUED_TOKEN = re.compile(r"^\s*([0-9]{1,3})(?=[^\W\d_])")
#: Glifos con los que el reconocimiento sustituye una cifra volada.
_SUPERSCRIPT_LIKE = frozenset("aoeszïÏ'`^*·°•¡!|")


@dataclass
class Gap:
    """Un número de verso que el capítulo no tiene, y su vecindad."""
    book: str
    chapter: int
    verse: int
    shape: str
    #: Lo que el capítulo sí materializó alrededor.
    previous_verse: Optional[int] = None
    next_verse: Optional[int] = None
    #: La versificación nativa de esta edición para ese capítulo.
    canonical_verses: Optional[int] = None
    materialized_top: Optional[int] = None
    materialized_count: int = 0
    #: Procedencia: por dónde pasa la frontera que falta.
    previous_block: Optional[str] = None
    next_block: Optional[str] = None
    previous_page: Optional[int] = None
    next_page: Optional[int] = None
    previous_column: Optional[str] = None
    next_column: Optional[str] = None
    previous_bbox: Optional[tuple] = None
    next_bbox: Optional[tuple] = None
    previous_tail: str = ""
    next_head: str = ""
    #: El renglón, dentro del verso anterior, que empieza por un token
    #: aislado -- el sitio donde el marcador perdido podría estar.
    swallowed_block: Optional[str] = None
    swallowed_token: Optional[str] = None
    swallowed_text: str = ""
    #: El rótulo del capítulo se recuperó de la imagen.
    recovered_chapter: bool = False
    signals: List[str] = field(default_factory=list)

    @property
    def pdf_page(self) -> Optional[int]:
        page = self.next_page if self.next_page is not None else self.previous_page
        return page + 1 if page is not None else None

    @property
    def key(self) -> str:
        return f"{self.book}.{self.chapter}.{self.verse}"

    def as_dict(self) -> dict:
        out = dict(self.__dict__)
        out["previous_bbox"] = list(self.previous_bbox) if self.previous_bbox else None
        out["next_bbox"] = list(self.next_bbox) if self.next_bbox else None
        out["signals"] = list(self.signals)
        out["pdf_page"] = self.pdf_page
        out["key"] = self.key
        return out


def _blocks_of(verse) -> list:
    """Los bloques de un versículo, en orden de lectura.

    Se ordenan por identificador --que lleva plana y renglón con ceros
    delante, así que el orden alfabético ES el de lectura-- y no por
    cómo vinieran: aguas arriba hay al menos un sitio que recorre un
    conjunto, de modo que el orden de llegada puede cambiar de un
    proceso a otro. Sin este orden, el mismo tomo daba dos inventarios
    distintos y el diagnóstico no era reproducible.
    """
    blocks = list(getattr(verse, "blocks", ()))
    return sorted(blocks, key=lambda block: (
        getattr(_provenance(block), "block_id", "") or ""))


def _provenance(block):
    return getattr(block, "provenance", None)


def _numeric_tokens(text: str) -> List[int]:
    return [int(token) for token in _TOKEN.findall(text or "")
            if len(token) <= 3]


def _looks_superscript(text: str) -> bool:
    head = (text or "").strip()[:2]
    return bool(head) and head[0] in _SUPERSCRIPT_LIKE


def inventory(edition, *, verse_limit=None) -> List[Gap]:
    """Todos los huecos de numeración del tomo, con su vecindad.

    `verse_limit(osis, chapter)` da el número de versículos que la
    versificación nativa asigna a ese capítulo; sin él no se puede
    distinguir un hueco de un numeral espurio, y se dice en la forma.

    Una pasada por capítulos y versículos: O(refs + bloques).
    """
    gaps: List[Gap] = []
    for osis, entry in sorted(edition.books.items()):
        for chapter_number in sorted(entry.chapters):
            if not chapter_number:
                continue
            chapter = entry.chapters[chapter_number]
            verses = sorted(v for v in chapter.verses if v is not None)
            if not verses:
                continue
            canonical = (verse_limit(osis, chapter_number)
                         if verse_limit else None)
            top = verses[-1]
            present = set(verses)
            # El dominio: de 1 al último materializado, y también hasta
            # el último que numera el canon -- lo segundo es lo que hace
            # visible un hueco de cola, que la cuenta antigua no veía.
            ceiling = max(top, canonical or 0)
            recovered = any(
                (_provenance(block) or object()).__dict__.get("block_id", "")[5:6] == "r"
                for block in getattr(chapter, "paratext", ())
                if _provenance(block) is not None)
            for verse in range(1, ceiling + 1):
                if verse in present:
                    continue
                if canonical is not None and verse > canonical:
                    shape = OVER_CANON
                elif verse < verses[0]:
                    shape = LEADING
                elif verse > top:
                    shape = TRAILING
                else:
                    shape = INTERIOR
                gap = Gap(book=osis, chapter=chapter_number, verse=verse,
                          shape=shape, canonical_verses=canonical,
                          materialized_top=top,
                          materialized_count=len(verses),
                          recovered_chapter=recovered)
                lower = [v for v in verses if v < verse]
                upper = [v for v in verses if v > verse]
                gap.previous_verse = lower[-1] if lower else None
                gap.next_verse = upper[0] if upper else None
                _describe(gap, chapter)
                gaps.append(gap)
    gaps.sort(key=lambda g: (g.book, g.chapter, g.verse))
    return gaps


def _swallowed(gap: Gap, before) -> None:
    """¿Hay dentro del verso anterior un renglón que empiece por un token
    aislado? Ahí es donde se queda el marcador que el parser no vio.

    Se recorren los bloques del verso anterior SALTANDO el primero, que
    es el que lleva su propio marcador. El primero que aparece es el
    candidato: el impreso pone un marcador por verso y en orden.
    """
    for block in _blocks_of(before)[1:]:
        text = getattr(block, "text", "") or ""
        glued = _GLUED_TOKEN.match(text)
        match = _LEADING_TOKEN.match(text)
        if not match and not glued:
            continue
        token = (glued or match).group(1)
        gap.swallowed_block = getattr(_provenance(block), "block_id", None)
        gap.swallowed_token = token
        gap.swallowed_text = (getattr(block, "text", "") or "")[:90]
        if glued and token == str(gap.verse):
            # El número está, y pegado: el impreso deja un espacio que
            # el reconocimiento se comió.
            gap.signals.append(ATTACHED_MARKER)
            gap.signals.append(SWALLOWED_DIGIT)
        elif token == str(gap.verse):
            gap.signals.append(SWALLOWED_DIGIT)
        else:
            gap.signals.append(SWALLOWED_GLYPH)
        return


def _describe(gap: Gap, chapter) -> None:
    """Lo que hay en los bloques que bordean el hueco."""
    before = chapter.verses.get(gap.previous_verse) if gap.previous_verse else None
    after = chapter.verses.get(gap.next_verse) if gap.next_verse else None
    before_blocks = _blocks_of(before) if before is not None else []
    after_blocks = _blocks_of(after) if after is not None else []

    if before_blocks:
        prov = _provenance(before_blocks[-1])
        gap.previous_block = getattr(prov, "block_id", None)
        gap.previous_page = getattr(prov, "page", None)
        gap.previous_column = getattr(prov, "column", None)
        gap.previous_bbox = getattr(prov, "bbox", None)
        gap.previous_tail = (getattr(before_blocks[-1], "text", "") or "")[-90:]
    if after_blocks:
        prov = _provenance(after_blocks[0])
        gap.next_block = getattr(prov, "block_id", None)
        gap.next_page = getattr(prov, "page", None)
        gap.next_column = getattr(prov, "column", None)
        gap.next_bbox = getattr(prov, "bbox", None)
        gap.next_head = (getattr(after_blocks[0], "text", "") or "")[:90]

    signals = []
    if gap.shape == OVER_CANON or (
            gap.canonical_verses is not None
            and gap.materialized_top is not None
            and gap.materialized_top > gap.canonical_verses):
        signals.append(SPURIOUS_TOP)

    # ¿Aparece el número que falta suelto en el texto vecino?
    around = _numeric_tokens(gap.previous_tail) + _numeric_tokens(gap.next_head)
    if gap.verse in around:
        signals.append(NUMERIC_FRAGMENT)
    if _looks_superscript(gap.next_head):
        signals.append(SUPERSCRIPT_LIKE)
    # ¿El número podría estar pegado a la primera palabra?
    head = (gap.next_head or "").lstrip()
    if head[:len(str(gap.verse))] == str(gap.verse) and \
            len(head) > len(str(gap.verse)) and head[len(str(gap.verse))].isalpha():
        signals.append(ATTACHED_MARKER)
    # ¿Los dos versos que bordean el hueco comparten bloque? Entonces el
    # impreso pudo marcar ahí una frontera que quedó dentro de un bloque.
    if gap.previous_block and gap.previous_block == gap.next_block:
        signals.append(BLOCK_MERGE)
    if before is not None and len(before_blocks) > 1:
        signals.append(BLOCK_SPLIT)
    if gap.previous_column and gap.next_column and \
            gap.previous_column != gap.next_column:
        signals.append(COLUMN_TRANSITION)
    if gap.previous_page is not None and gap.next_page is not None and \
            gap.previous_page != gap.next_page:
        signals.append(PAGE_TRANSITION)
    if gap.recovered_chapter:
        signals.append(RECOVERED_BOUNDARY)
    gap.signals = signals
    if before is not None:
        _swallowed(gap, before)
    if not any(s in gap.signals for s in (NUMERIC_FRAGMENT, SUPERSCRIPT_LIKE,
                                          ATTACHED_MARKER, SWALLOWED_DIGIT,
                                          SWALLOWED_GLYPH)):
        gap.signals.append(NO_NUMERIC_EVIDENCE)


def summary(gaps: List[Gap]) -> dict:
    """Las cuentas del inventario, por donde importan."""
    by_shape = collections.Counter(g.shape for g in gaps)
    by_book = collections.Counter(g.book for g in gaps)
    by_signal = collections.Counter(s for g in gaps for s in g.signals)
    per_chapter = collections.Counter((g.book, g.chapter) for g in gaps)
    physical = [g for g in gaps if g.shape != OVER_CANON]
    return {
        "total": len(gaps),
        "physical_domain": len(physical),
        "beyond_canonical_top": by_shape.get(OVER_CANON, 0),
        "by_shape": dict(sorted(by_shape.items())),
        "by_book": dict(sorted(by_book.items())),
        "by_signal": dict(sorted(by_signal.items())),
        "chapters_with_gaps": len(per_chapter),
        "chapters_by_gap_count": dict(sorted(collections.Counter(
            per_chapter.values()).items())),
        "worst_chapters": [{"book": book, "chapter": chapter, "gaps": count}
                           for (book, chapter), count
                           in per_chapter.most_common(10)],
    }


def _bucket(gap: Gap) -> tuple:
    """El estrato al que pertenece un hueco, para muestrear."""
    evidence = (SWALLOWED_DIGIT if SWALLOWED_DIGIT in gap.signals else
                SWALLOWED_GLYPH if SWALLOWED_GLYPH in gap.signals else
                NUMERIC_FRAGMENT if NUMERIC_FRAGMENT in gap.signals else
                ATTACHED_MARKER if ATTACHED_MARKER in gap.signals else
                SUPERSCRIPT_LIKE if SUPERSCRIPT_LIKE in gap.signals else
                NO_NUMERIC_EVIDENCE)
    return (gap.book, gap.shape, evidence)


def sample(gaps: List[Gap], *, size: int, seed: str = "124") -> List[Gap]:
    """Una muestra estratificada y reproducible.

    Reparte por estrato --libro, forma del hueco, clase de evidencia-- y
    dentro de cada estrato ordena por una huella estable del hueco, no
    por azar: la misma entrada da siempre la misma muestra, y nadie
    puede elegir la plana que le conviene.
    """
    buckets: Dict[tuple, List[Gap]] = collections.defaultdict(list)
    for gap in gaps:
        buckets[_bucket(gap)].append(gap)

    def fingerprint(gap: Gap) -> str:
        raw = f"{seed}|{gap.book}.{gap.chapter}.{gap.verse}"
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()

    for rows in buckets.values():
        rows.sort(key=fingerprint)
    order = sorted(buckets, key=lambda k: (-len(buckets[k]), k))
    picked: List[Gap] = []
    round_index = 0
    while len(picked) < size and order:
        progressed = False
        for key in order:
            rows = buckets[key]
            if round_index < len(rows):
                picked.append(rows[round_index])
                progressed = True
                if len(picked) == size:
                    break
        if not progressed:
            break
        round_index += 1
    picked.sort(key=lambda g: (g.book, g.chapter, g.verse))
    return picked


def strata(gaps: List[Gap]) -> dict:
    """Cuántos huecos hay en cada estrato, para poder leer la muestra."""
    counts = collections.Counter(_bucket(gap) for gap in gaps)
    return {f"{book}|{shape}|{evidence}": count
            for (book, shape, evidence), count in sorted(counts.items())}
