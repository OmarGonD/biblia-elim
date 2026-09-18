"""
Qué número puede ser un marcador de versículo, y qué número no puede.

    CANON LIMIT MAY REJECT. CANON LIMIT MAY NOT CREATE.

Es la única forma en que la versificación nativa entra aquí. Sirve para
decir que un número es IMPOSIBLE --Isaías 26 tiene veintiún versículos,
así que un «211» no es un marcador de ese capítulo-- y no sirve para lo
contrario: que el canon numere un verso 7 no autoriza a crear el verso
7. Rechazar es barato y reversible; inventar no.

De dónde salen esos números imposibles: el reconocimiento pega al
marcador lo que tiene al lado. En la plana 340 el impreso dice «19 ó
fieras de una nueva especie», con «ó» de conjunción, y la máquina lo
devolvió como «196». Un solo número así hace dos daños:

    se lleva su texto a un versículo que no existe;

    y, como la métrica de huecos iba de 1 al mayor materializado,
    fabricaba cientos de ausencias que no están en ninguna plana.

Lo que este módulo hace con uno de esos números es lo mínimo: no lo
acepta como frontera. El TEXTO no se toca -- pasa al versículo anterior,
que es donde el impreso lo tenía antes de que apareciera la cifra falsa,
o al paratexto del capítulo si no hay ninguno antes. Ni un bloque se
borra, ni el crudo se edita, ni se intenta adivinar qué cifra quiso ser:
para eso hay que mirar la plana, y eso es otra tanda.
"""
from typing import Callable, List, Optional

#: Por qué se rechazó.
ABOVE_LIMIT = "above_native_verse_limit"
NOT_POSITIVE = "not_a_positive_verse_number"

#: Dónde acabó el texto que llevaba el número imposible.
JOINED_PREVIOUS = "joined_to_previous_verse"
JOINED_PARATEXT = "joined_to_chapter_paratext"


def impossible(verse: Optional[int], limit: Optional[int]) -> Optional[str]:
    """Por qué ese número no puede ser un marcador, o None si puede.

    Sin límite conocido no se rechaza nada: la ignorancia no es una
    razón para tirar una frontera.
    """
    if verse is None:
        return None
    if verse < 1:
        return NOT_POSITIVE
    if limit is not None and verse > limit:
        return ABOVE_LIMIT
    return None


def enforce(chapter, *, book: str,
            verse_limit: Callable[[str, int], Optional[int]]) -> List[dict]:
    """Retira de un capítulo los marcadores que no puede tener.

    Devuelve un registro por cada número retirado, con lo que hacía
    falta para poder auditarlo: el número, el límite, los bloques que
    llevaba y dónde han quedado. El capítulo se modifica en el sitio.

    Una pasada por los versículos del capítulo: O(versículos + bloques).
    """
    number = getattr(chapter, "number", None)
    if not isinstance(number, int) or number < 1:
        return []
    limit = verse_limit(book, number)
    if limit is None:
        return []

    records: List[dict] = []
    # De menor a mayor: así el texto de un número imposible siempre cae
    # en el último versículo VÁLIDO que quedó por detrás, aunque haya
    # varios imposibles seguidos.
    for verse in sorted(v for v in chapter.verses if v is not None):
        reason = impossible(verse, limit)
        if reason is None:
            continue
        removed = chapter.verses.pop(verse)
        blocks = list(getattr(removed, "blocks", ()))
        previous = max((v for v in chapter.verses
                        if v is not None and v < verse), default=None)
        if previous is not None:
            # El texto sigue en el flujo, pegado a lo que venía antes.
            chapter.verses[previous].blocks.extend(blocks)
            landed, owner = JOINED_PREVIOUS, previous
        else:
            chapter.paratext.extend(blocks)
            landed, owner = JOINED_PARATEXT, None
        records.append({
            "book": book, "chapter": number, "marker": verse,
            "verse_limit": limit, "reason": reason,
            "blocks": len(blocks),
            "block_ids": [getattr(getattr(block, "provenance", None),
                                  "block_id", None) for block in blocks],
            "raw_head": (getattr(blocks[0], "text", "") or "")[:80]
                        if blocks else "",
            "text_landed": landed,
            "new_owner_verse": owner,
        })
    return records


def enforce_edition(edition, *,
                    verse_limit: Callable[[str, int], Optional[int]]
                    ) -> List[dict]:
    """La misma guarda sobre toda la edición, en orden estable."""
    records: List[dict] = []
    for osis in sorted(edition.books):
        entry = edition.books[osis]
        for chapter_number in sorted(entry.chapters):
            records.extend(enforce(entry.chapters[chapter_number],
                                   book=osis, verse_limit=verse_limit))
    return records
