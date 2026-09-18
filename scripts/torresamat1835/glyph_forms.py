"""
Qué formas devuelve el reconocimiento donde el impreso pone una cifra.

    FACSIMILE ESTABLISHES THE GLYPH MAPPING.

Este módulo NO traduce glifos. No hay aquí ninguna tabla que diga que
«a» vale 2 ni que «S» vale 8. Lo que hace es lo anterior a eso, y es lo
único que se puede hacer sin mirar la plana: contar. Agrupa los
renglones que el inventario de huecos señaló con

    lone_glyph_inside_previous_verse

por la FORMA EXACTA que dejó el reconocimiento, y saca de cada grupo una
muestra reproducible para llevarla al facsímil. La correspondencia
--si la hay-- la establece la imagen, se guarda en
`verse_boundary_reviews.json` y se lee desde ahí; nunca se deduce aquí.

Tres reglas gobiernan el agrupamiento, y las tres son de omisión:

    NO SE NORMALIZAN LOS DIACRÍTICOS. «a» y «á» son formas distintas
    mientras la plana no demuestre lo contrario. Quitar el acento para
    decidir un número es exactamente perder la única diferencia que
    separa la cifra de la preposición castellana.

    NO SE UNIFICA LA CAJA. «S» y «s» van por separado. Son dos ojos
    distintos de la fundición y pueden salir de dos cifras distintas.

    NO SE COLAPSAN LAS FORMAS COMPUESTAS. «I o» no es «I» con ruido al
    lado: puede ser un 10 partido en dos palabras por el reconocimiento,
    y puede ser una «I» seguida de otra cosa. Son grupos distintos hasta
    que la imagen diga.

Sobre la unidad de conteo. Un hueco NO es una instancia: `verse_gaps`
señala, para cada hueco, el primer renglón con token aislado que hay
dentro del versículo anterior, y varios huecos seguidos del mismo
capítulo comparten ese mismo versículo anterior y por tanto el MISMO
renglón. En este tomo 2093 huecos apuntan a 578 renglones. Mirar un
renglón en la plana contesta a la vez por todos los huecos que lo
señalan, así que la evidencia visual se cuenta por RENGLONES
(`instances`) y la población afectada por HUECOS (`candidates`). Darlas
por iguales inflaría la evidencia casi por cuatro.
"""
import collections
import hashlib
import re
import unicodedata
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import verse_gaps

#: La clase que esta tanda inventaría.
CANDIDATE_SIGNAL = verse_gaps.SWALLOWED_GLYPH

#: Signos que el reconocimiento deja pegados delante de la cabeza del
#: renglón sin formar parte de ella. Se registran aparte (`framing`)
#: para poder preguntar luego si el marco cambia algo, y no se borran.
_PUNCT_RUN = re.compile(r"^[^\w\s]{1,3}")

#: Formas que en castellano son además palabra o letra corriente, así
#: que para ellas el control negativo es OBLIGATORIO antes de proponer
#: ninguna regla. Esto no es un mapa de glifos: no dice qué cifra son,
#: dice que pueden no ser ninguna.
AMBIGUOUS_WITH_SPANISH = frozenset(
    ["a", "á", "e", "é", "o", "ó", "u", "ú", "y", "i", "í", "l", "I", "A",
     "E", "O", "U", "Y"])

#: Cuántos ejemplares llevar al facsímil según lo frecuente que sea la
#: forma. Los cortes son de la task: muy frecuentes 8-12, medias 5-8,
#: raras todas. Se fija aquí para que la muestra no dependa de quién la
#: pida.
#: A partir de cuántos píxeles se considera que un renglón ENTRA. Sale
#: de lo medido en el tomo --la sangría de versículo ronda los 40-50px a
#: esta resolución-- y se deja bajo para no descartar de más: aquí sirve
#: para describir la muestra, no para decidir ninguna recuperación.
INDENT_FLOOR = 25


def _median(values):
    if not values:
        return None
    ordered = sorted(values)
    return ordered[len(ordered) // 2]


#: Por debajo de este número de instancias una forma no entra en el
#: programa de revisión. No es que no importe: es que no se puede
#: GENERALIZAR desde una o dos apariciones, y llamar «forma validada» a
#: lo que se vio una vez sería el mismo error que esta tanda viene a
#: evitar. Las que quedan fuera se cuentan y se publican como cola sin
#: revisar, nunca como resueltas.
REVIEW_FLOOR = 3


def quota(size: int) -> int:
    """Cuántas instancias de una forma entran en la muestra."""
    if size < REVIEW_FLOOR:
        return 0
    if size >= 50:
        return 12
    if size >= 20:
        return 8
    if size >= 8:
        return 6
    return size


def head_of(raw: str):
    """(framing, form, second) — la cabeza del renglón, VERBATIM.

    `framing` es el signo o signos que preceden a la cabeza sin ser
    parte de ella; `form` es el primer token; `second` es el segundo
    token SÓLO cuando los dos ocupan un único carácter, que es la
    situación en la que el reconocimiento ha podido partir en dos
    palabras un numeral de dos cifras («I o» por 10). Con dos caracteres
    ya no: las palabras castellanas cortas --«el», «la», «Tú», «De»--
    miden dos, y meterlas en la forma rompería en pedazos el grupo de la
    cabeza que sí importa. Lo que sigue a la cabeza se conserva de todos
    modos en `context.following_word`, sin interpretarlo.

    Que «I o» sea un 10 partido o una «I» seguida de una «o» lo dice la
    plana y no esta función: lo único que se hace aquí es no juntarlo con
    «I» a solas.

    Ni se quitan acentos ni se cambia la caja: los caracteres salen tal
    y como el reconocimiento los dejó.
    """
    words = (raw or "").split()
    if not words:
        return "", "", None
    first = words[0]
    match = _PUNCT_RUN.match(first)
    framing = match.group(0) if match else ""
    token = first[len(framing):]
    if not token:
        # El signo era la palabra entera: «. I o». El marco se lleva esa
        # palabra completa y la cabeza es la siguiente.
        framing, words = first, words[1:]
        if not words:
            return framing, "", None
        first = words[0]
        match = _PUNCT_RUN.match(first)
        if match and len(first) > len(match.group(0)):
            framing += match.group(0)
            first = first[len(match.group(0)):]
        token = first
    second = None
    if len(token) == 1 and len(words) > 1 and len(words[1]) == 1:
        second = words[1]
    return framing, token, second


def glyph_form(raw: str) -> str:
    """La forma por la que se agrupa: la cabeza, con su compuesto."""
    _, token, second = head_of(raw)
    return token if second is None else f"{token} {second}"


def _is_accented(token: str) -> bool:
    return any(unicodedata.category(ch) == "Mn"
               for ch in unicodedata.normalize("NFD", token))


def _capitalization(token: str) -> str:
    letters = [ch for ch in token if ch.isalpha()]
    if not letters:
        return "non_alphabetic"
    if all(ch.isupper() for ch in letters):
        return "upper"
    if all(ch.islower() for ch in letters):
        return "lower"
    return "mixed"


@dataclass
class Instance:
    """Un renglón del reconocimiento señalado como cabeza de hueco.

    Es la unidad de evidencia visual: se mira UNA vez en la plana y
    contesta por todos los huecos que lo señalan.
    """
    block_id: str
    scan_page: Optional[int]
    pdf_page: Optional[int]
    column: Optional[str]
    zone: Optional[str]
    bbox: Optional[tuple]
    raw: str
    framing: str
    form: str
    second: Optional[str]
    #: Cuánto entra el renglón respecto de la mediana de su columna en su
    #: plana. En esta edición el renglón que ABRE versículo entra un poco
    #: y el que continúa la frase no, así que es una medida geométrica
    #: --no una lectura-- que separa las dos cosas sin mirar qué glifo
    #: hay. Se registra en bruto; lo que signifique lo dice el facsímil.
    indent_px: Optional[int] = None
    column_left_median: Optional[int] = None
    #: Los huecos que apuntan a este renglón. `expected_values` es
    #: DIAGNÓSTICO: sirve para estratificar la muestra y jamás para
    #: decidir qué cifra imprime la plana.
    gap_keys: List[str] = field(default_factory=list)
    expected_values: List[int] = field(default_factory=list)
    books: List[str] = field(default_factory=list)

    @property
    def glyph_form(self) -> str:
        return self.form if self.second is None else f"{self.form} {self.second}"

    @property
    def book(self) -> str:
        return self.books[0] if self.books else ""

    def as_dict(self) -> dict:
        return {
            "block_id": self.block_id, "scan_page": self.scan_page,
            "pdf_page": self.pdf_page, "column": self.column,
            "zone": self.zone, "bbox": list(self.bbox) if self.bbox else None,
            "raw": self.raw, "framing": self.framing,
            "glyph_form": self.glyph_form, "form": self.form,
            "second": self.second,
            "indent_px": self.indent_px,
            "column_left_median": self.column_left_median,
            "capitalization": _capitalization(self.form),
            "accented": _is_accented(self.form),
            "context": dict(self.context()),
            "candidate_gaps": len(self.gap_keys),
            "gap_keys": list(self.gap_keys),
            "expected_values_diagnostic_only": list(self.expected_values),
            "books": list(self.books),
        }

    def context(self) -> dict:
        """Lo que rodea a la cabeza, sin interpretarlo.

        Son las diferencias que tendría que mirar un matcher futuro para
        poder excluir castellano de verdad: si hay espacio detrás, si lo
        que sigue va en mayúscula, si el renglón está en el cuerpo o en
        las notas. Ninguna de ellas dice aquí qué cifra hay.
        """
        words = (self.raw or "").split()
        tail = words[2:] if self.second is not None else words[1:]
        if self.framing and self.framing == (words[0] if words else ""):
            tail = tail[1:] if tail else tail
        following = tail[0] if tail else ""
        glued = bool(re.match(r"^\s*[^\s\w]{0,3}\w{1,2}[^\W\d_]{2,}",
                              self.raw or "")) and not self.framing
        return {
            "framed_by_punctuation": bool(self.framing),
            "followed_by_space": bool(self.raw and " " in self.raw.strip()),
            "following_word": following,
            "following_is_capitalized": bool(following[:1].isupper()),
            "following_is_lowercase": bool(following[:1].islower()),
            "composite_head": self.second is not None,
            "in_apparatus": self.zone == "apparatus",
            "in_body": self.zone == "body",
            "glued_candidate": glued,
        }


def _left_medians(edition) -> Dict[tuple, int]:
    """La sangría de referencia de cada columna de cada plana.

    La mediana del borde izquierdo, y no la mínima: los renglones que
    CONTINÚAN una frase son la gran mayoría y todos empiezan en el mismo
    sitio, así que la mediana cae sobre ellos. El renglón que abre
    versículo entra a la derecha de esa mediana. Una plana con las dos
    columnas fundidas dará una mediana sin sentido, y por eso la medida
    se guarda en bruto y no se convierte aquí en ninguna decisión.
    """
    edges: Dict[tuple, List[int]] = collections.defaultdict(list)
    for entry in edition.books.values():
        for chapter in entry.chapters.values():
            groups = [getattr(chapter, "paratext", ())]
            groups.extend(getattr(slot, "blocks", ())
                          for slot in chapter.verses.values())
            for group in groups:
                for block in group:
                    prov = getattr(block, "provenance", None)
                    if prov is None or not prov.bbox:
                        continue
                    if prov.zone != "body":
                        continue
                    edges[(prov.page, prov.column)].append(prov.bbox[0])
    out = {}
    for key, values in edges.items():
        values.sort()
        out[key] = values[len(values) // 2]
    return out


def _blocks_by_id(edition) -> Dict[str, object]:
    index = {}
    for entry in edition.books.values():
        for chapter in entry.chapters.values():
            slots = list(chapter.verses.values())
            for slot in slots:
                for block in getattr(slot, "blocks", ()):
                    prov = getattr(block, "provenance", None)
                    if prov is not None and prov.block_id:
                        index[prov.block_id] = block
            for block in getattr(chapter, "paratext", ()):
                prov = getattr(block, "provenance", None)
                if prov is not None and prov.block_id:
                    index[prov.block_id] = block
    return index


def inventory(edition, gaps) -> List[Instance]:
    """Las instancias de la clase, agrupadas por renglón.

    Una pasada por los huecos y otra por los bloques: O(huecos +
    bloques). No se abre el facsímil, no se lee el XML otra vez y no se
    toca nada: el crudo sale del bloque, que lo guarda intacto.
    """
    index = _blocks_by_id(edition)
    medians = _left_medians(edition)
    found: Dict[str, Instance] = {}
    for gap in gaps:
        signals = getattr(gap, "signals", None)
        if signals is None:
            signals = gap.get("signals", [])
        if CANDIDATE_SIGNAL not in signals:
            continue
        block_id = (getattr(gap, "swallowed_block", None)
                    if not isinstance(gap, dict) else gap.get("swallowed_block"))
        if not block_id:
            continue
        book = gap["book"] if isinstance(gap, dict) else gap.book
        chapter = gap["chapter"] if isinstance(gap, dict) else gap.chapter
        verse = gap["verse"] if isinstance(gap, dict) else gap.verse
        key = f"{book}.{chapter}.{verse}"
        instance = found.get(block_id)
        if instance is None:
            block = index.get(block_id)
            # `raw_text` es el crudo que el bloque conserva intacto; si
            # por lo que sea no está, se usa lo que trajo el hueco, y
            # nunca se reconstruye nada.
            raw = (getattr(block, "raw_text", None) or "") if block else ""
            if not raw:
                raw = (gap.get("swallowed_text", "") if isinstance(gap, dict)
                       else (gap.swallowed_text or ""))
            prov = getattr(block, "provenance", None) if block else None
            framing, form, second = head_of(raw)
            page = getattr(prov, "page", None)
            column = getattr(prov, "column", None)
            bbox = getattr(prov, "bbox", None)
            median = medians.get((page, column))
            instance = Instance(
                block_id=block_id, scan_page=page,
                pdf_page=(page + 1) if isinstance(page, int) else None,
                column=column, zone=getattr(prov, "zone", None), bbox=bbox,
                raw=raw, framing=framing, form=form, second=second,
                column_left_median=median,
                indent_px=(bbox[0] - median)
                          if bbox and median is not None else None)
            found[block_id] = instance
        instance.gap_keys.append(key)
        instance.expected_values.append(verse)
        if book not in instance.books:
            instance.books.append(book)
    for instance in found.values():
        instance.gap_keys.sort()
        instance.expected_values.sort()
    return sorted(found.values(), key=lambda i: i.block_id)


def forms(instances: List[Instance]) -> dict:
    """La tabla de frecuencias por forma exacta.

    De cada forma se dice lo que hace falta para decidir si merece
    muestra y de qué tamaño, y lo que hará falta luego para leer la
    revisión: en qué libros aparece, con qué marcos, con qué caja, y
    qué valores de hueco la acompañan --esto último, y hay que repetirlo,
    sólo como estrato--.
    """
    buckets: Dict[str, List[Instance]] = collections.defaultdict(list)
    for instance in instances:
        buckets[instance.glyph_form].append(instance)
    out = {}
    for form, rows in buckets.items():
        books = collections.Counter(b for row in rows for b in row.books)
        values = collections.Counter(v for row in rows
                                     for v in row.expected_values)
        out[form] = {
            "glyph_form": form,
            "instances": len(rows),
            "candidate_gaps": sum(len(row.gap_keys) for row in rows),
            "books": dict(sorted(books.items())),
            "chapters": len({(b, k.split(".")[1])
                             for row in rows for b in row.books
                             for k in row.gap_keys}),
            "scan_pages": len({row.scan_page for row in rows}),
            "capitalization": dict(sorted(collections.Counter(
                _capitalization(row.form) for row in rows).items())),
            "accented": sum(1 for row in rows if _is_accented(row.form)),
            "framing": dict(sorted(collections.Counter(
                row.framing for row in rows).items())),
            "composite": rows[0].second is not None,
            "zones": dict(sorted(collections.Counter(
                str(row.zone) for row in rows).items())),
            "ambiguous_with_spanish": form in AMBIGUOUS_WITH_SPANISH,
            "indent_px_median": _median([row.indent_px for row in rows
                                         if row.indent_px is not None]),
            "indented_like_a_marker": sum(
                1 for row in rows
                if row.indent_px is not None and row.indent_px >= INDENT_FLOOR),
            "flush_with_continuations": sum(
                1 for row in rows
                if row.indent_px is not None and row.indent_px < INDENT_FLOOR),
            "expected_values_diagnostic_only": dict(sorted(
                (str(v), n) for v, n in values.items())),
            "sample_blocks": [row.block_id for row in rows[:6]],
            "quota": quota(len(rows)),
        }
    return dict(sorted(out.items(), key=lambda kv: (-kv[1]["instances"], kv[0])))


def _fingerprint(seed: str, instance: Instance) -> str:
    return hashlib.sha256(
        f"{seed}|{instance.block_id}".encode("utf-8")).hexdigest()


def sample(instances: List[Instance], *, seed: str = "127") -> List[Instance]:
    """Una muestra estratificada por FORMA, reproducible.

    Dentro de cada forma se reparte además por libro --ronda a ronda,
    empezando por el libro que más aporta-- y el orden dentro de cada
    libro es el de una huella estable del identificador de bloque, no el
    de nadie. La misma entrada da siempre la misma muestra: no se puede
    escoger la plana que conviene.
    """
    buckets: Dict[str, List[Instance]] = collections.defaultdict(list)
    for instance in instances:
        buckets[instance.glyph_form].append(instance)
    picked: List[Instance] = []
    for form in sorted(buckets):
        rows = buckets[form]
        by_book: Dict[str, List[Instance]] = collections.defaultdict(list)
        for row in rows:
            by_book[row.book].append(row)
        for book_rows in by_book.values():
            book_rows.sort(key=lambda row: _fingerprint(seed, row))
        order = sorted(by_book, key=lambda b: (-len(by_book[b]), b))
        target, taken, round_index = quota(len(rows)), [], 0
        while len(taken) < target:
            progressed = False
            for book in order:
                if round_index < len(by_book[book]):
                    taken.append(by_book[book][round_index])
                    progressed = True
                    if len(taken) == target:
                        break
            if not progressed:
                break
            round_index += 1
        picked.extend(taken)
    picked.sort(key=lambda row: row.block_id)
    return picked
