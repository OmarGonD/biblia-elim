"""
El 2 impreso que el reconocimiento devuelve como la palabra «a».

    TASK 146: RESPALDO ACOTADO. FORMA + BANDA + PROCEDENCIA + PROGRESIÓN.

Esta edición compone el 2 con una cifra de estilo antiguo que el
reconocimiento lee como «a». Pero «a» es también la preposición, y un
renglón de prosa empieza por ella a menudo. Por eso aquí no decide la
forma sola: sólo se abre el versículo 2 cuando se cumplen, a la vez, las
condiciones que validaron las tareas 142, 144 y 145, y ninguna más.

    DISCRIMINADOR (142). La PRIMERA palabra física es exactamente «a»,
        sin marco; cada una de las TRES siguientes tiene al menos dos
        letras; hay banda de confianza en la columna
        (``compound_glyphs.band_of``) y el borde izquierdo de esa «a» cae
        dentro de ``compound_glyphs.tolerance``. Cuerpo de la columna
        castellana. Es la colocación de producción de la 143.

        La columna, la zona y la banda son las que la 142 midió y la
        143-145 validaron: las de ``layout.split_columns(page)`` SIN la
        pista de canal que el parser usa para colocar (ver
        ``validated_geometry``). Además el parser tiene que haber puesto
        el renglón como continuación de la columna castellana: se exigen
        las dos colocaciones, nunca una sola.

    PROCEDENCIA DEL HUECO PROYECTADO (144). El renglón tiene que ser el
        ``swallowed_block`` de un hueco de la numeración nativa cuyo glifo
        tragado es exactamente «a» (``verse_gaps.SWALLOWED_GLYPH``): el
        PRIMER renglón con token suelto dentro del versículo anterior a un
        número que falta. Una «a» que cae en la banda sin que falte nada
        a su alrededor no tiene esa procedencia y se deja.

    PROGRESIÓN NATIVA (144). El 2 no puede quedar detrás del versículo
        que ya posee el renglón en su capítulo nativo: si el dueño es el
        7, abrir el 2 sería retroceder en el orden físico.

Ni el hueco que falta, ni el anterior + 1, ni el siguiente - 1 deciden el
número: es la cifra de la familia revisada (``VALUE``), y el capítulo es
el nativo que ya posee el renglón. Ante cualquier duda, se abstiene.

Las guardas de la 144 sólo existen sobre la edición materializada --
antes, los capítulos todavía no tienen número--, así que el parser anota
los candidatos mientras lee y ``apply`` decide en ``finish``, justo
detrás de la guarda de canon, igual que ``verse_markers.enforce_edition``.
Nada de esto lee artefactos de diagnóstico, PDF ni imágenes.
"""
from typing import Callable, Dict, List, Optional, Tuple

import compound_glyphs
import layout
import verse_gaps

#: La cifra que interpreta la forma cuando pasa TODAS las guardas.
VALUE = 2

#: La decisión que llevan los renglones abiertos por esta ruta.
DECISION = "projected_form_a_marker"

#: Por qué un renglón con forma «a» no pasa el discriminador.
PROSE_PREFIX = "form_a_prefix_not_three_prose_words"
NOT_VALIDATED_PLACEMENT = "validated_layout_not_right_body"

#: Desenlaces en ``finish``. Todos menos RECOVERED son abstenciones.
RECOVERED = "recovered"
WOULD_RECOVER = "would_recover"
NOT_OWNED = "not_owned_as_continuation"
PROVENANCE = "provenance_guard"
AMBIGUOUS_OWNER = "ambiguous_ownership"
UNRESOLVED_CHAPTER = "native_chapter_unresolved"
ORDER = "order_guard_backward"
OUTSIDE_CANON = "outside_native_canon"
REOPEN = "existing_ref_reopen"
AMBIGUOUS_EVENT = "ambiguous_source_event"


def has_form(line) -> bool:
    """El filtro barato: primera palabra física exactamente «a», y tres más."""
    words = getattr(line, "words", None) or []
    return len(words) >= 4 and words[0].text == "a"


def validated_geometry(page) -> Tuple[dict, dict]:
    """Columna/zona por renglón y bandas por columna, como las midió la 142.

    Es ``layout.split_columns(page)`` sin pista de canal y
    ``compound_glyphs.band_of`` sobre las líneas de CUERPO de cada columna:
    exactamente la geometría de ``line_features`` (142) y de la colocación
    de producción (143). El parser coloca con la mediana de los canales ya
    medidos, que en alguna plana decide otra columna; el discriminador
    validado no se evalúa sobre esa otra geometría.
    """
    placed = layout.split_columns(page)
    body: Dict[object, list] = {}
    for row in placed:
        if row.zone is layout.Zone.BODY:
            body.setdefault(row.column, []).append(row.line)
    bands = {column: compound_glyphs.band_of(lines)
             for column, lines in body.items()}
    where = {row.line.index: (row.column, row.zone) for row in placed}
    return where, bands


def match(line, band) -> Tuple[Optional[str], Optional[str], dict]:
    """(texto, None, detalle) si el renglón pasa el discriminador de la 142.

    Con otra forma devuelve ``(None, NO_TOKENS, detalle)`` y ``detalle
    ["form"]`` a None: no es un candidato y no hay nada que contar.
    """
    words = getattr(line, "words", None) or []
    detail = {"form": None, "marker_x0": None, "band_center": None,
              "indent": None, "tolerance": None}
    if len(words) < 4 or words[0].text != "a":
        return None, compound_glyphs.NO_TOKENS, detail
    detail["form"] = "a"
    detail["marker_x0"] = words[0].bbox[0]
    if band is None:
        return None, compound_glyphs.NO_BAND, detail
    if not all(sum(ch.isalpha() for ch in word.text) >= 2
               for word in words[1:4]):
        return None, PROSE_PREFIX, detail
    limit = compound_glyphs.tolerance(band)
    detail["band_center"] = band[0]
    detail["indent"] = words[0].bbox[0] - band[0]
    detail["tolerance"] = limit
    if abs(words[0].bbox[0] - band[0]) > limit:
        return None, compound_glyphs.OUT_OF_BAND, detail
    return " ".join(word.text for word in words[1:]).strip(), None, detail


def projected_gap_blocks(edition, *, verse_limit) -> set:
    """Los renglones con procedencia de hueco proyectado «a».

    Una sola pasada del inventario de huecos, O(refs + bloques): el
    renglón tragado dentro del versículo anterior a un número que falta,
    cuando lo que se tragó es exactamente el glifo «a».
    """
    return {gap.swallowed_block
            for gap in verse_gaps.inventory(edition, verse_limit=verse_limit)
            if gap.swallowed_token == "a"
            and verse_gaps.SWALLOWED_GLYPH in gap.signals}


def apply(edition, candidates: List[dict], blocks: Dict[str, object], *,
          enabled: bool,
          verse_limit: Callable[[str, int], Optional[int]]) -> None:
    """Aplica las guardas de la 144 y, si todo pasa, abre el versículo 2.

    ``candidates`` son los registros que anotó el parser (renglones que
    pasaron el discriminador tras perder contra todas las rutas más
    fuertes) y ``blocks`` el bloque de continuación de cada uno. Cada
    registro sale con su ``outcome``; con ``enabled`` apagado es una
    simulación sobre la misma ruta de código.

    El movimiento es el de un marcador leído: desde este renglón hasta el
    final del versículo que lo tenía, todo pasa al nuevo.
    """
    if not candidates:
        return
    projected = projected_gap_blocks(edition, verse_limit=verse_limit)
    wanted = {record["block_id"] for record in candidates}
    owners: Dict[str, list] = {}
    for osis, entry in edition.books.items():
        for number, chapter in entry.chapters.items():
            for verse, slot in chapter.verses.items():
                for blk in slot.blocks:
                    block_id = blk.provenance.block_id
                    if block_id in wanted:
                        owners.setdefault(block_id, []).append(
                            (osis, number, verse))

    proposals: Dict[tuple, list] = {}
    for record in sorted(candidates, key=lambda row: row["block_id"]):
        block_id = record["block_id"]
        owner = owners.get(block_id, [])
        record["projected_gap_provenance"] = block_id in projected
        record["native_owner_refs"] = [f"{o}.{n}.{v}" for o, n, v in owner]
        record["proposed_native_ref"] = None
        if block_id not in blocks:
            record["outcome"] = NOT_OWNED
            continue
        if not record["projected_gap_provenance"]:
            record["outcome"] = PROVENANCE
            continue
        if len(owner) != 1:
            record["outcome"] = AMBIGUOUS_OWNER
            continue
        osis, number, verse = owner[0]
        if not isinstance(number, int) or number < 1:
            record["outcome"] = UNRESOLVED_CHAPTER
            continue
        if not isinstance(verse, int):
            record["outcome"] = AMBIGUOUS_OWNER
            continue
        record["native_active_verse"] = verse
        if VALUE < verse:
            record["outcome"] = ORDER
            continue
        limit = verse_limit(osis, number)
        if limit is None or not 1 <= VALUE <= limit:
            record["outcome"] = OUTSIDE_CANON
            continue
        if VALUE in edition.books[osis].chapters[number].verses:
            record["outcome"] = REOPEN
            continue
        record["proposed_native_ref"] = f"{osis}.{number}.{VALUE}"
        proposals.setdefault((osis, number), []).append(record)

    for (osis, number), records in sorted(proposals.items()):
        if len(records) != 1:
            # Dos renglones pidiendo abrir el mismo 2: no se elige.
            for record in records:
                record["outcome"] = AMBIGUOUS_EVENT
            continue
        record = records[0]
        if not enabled:
            record["outcome"] = WOULD_RECOVER
            continue
        block_id = record["block_id"]
        chapter = edition.books[osis].chapters[number]
        verse = owners[block_id][0][2]
        old = chapter.verses[verse]
        moving = [blk for blk in old.blocks
                  if (blk.provenance.block_id or "") >= block_id]
        old.blocks = [blk for blk in old.blocks
                      if (blk.provenance.block_id or "") < block_id]
        marker = blocks[block_id]
        marker.text = record["text"]
        marker.decision = DECISION
        for blk in moving:
            blk.number = VALUE
        chapter.verse(VALUE).blocks.extend(moving)
        record.update(outcome=RECOVERED, applied=True,
                      prior_owner_ref=f"{osis}.{number}.{verse}",
                      blocks_moved=sorted(blk.provenance.block_id
                                          for blk in moving))
