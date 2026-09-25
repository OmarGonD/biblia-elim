"""
El número de dos cifras que el reconocimiento parte en dos: «1 8 Libóme».

    TASK 153. FORMA + COLOCACIÓN + PROGRESIÓN, EN ORDEN FÍSICO.

El OCR lee el 18 impreso como dos palabras, «1» y «8», y el parser toma
la primera: el versículo 1 se queda con el texto del 18 en adelante. La
151 midió la familia (379 renglones) y la 152 simuló esta regla sobre la
edición: 360 eventos, 32/32 de una muestra estratificada confirmados en
el facsímil.

    FORMA. El renglón empieza por dos palabras de una sola cifra (la
        primera 1-9) y la tercera abre frase (mayúscula, «¿», «¡», ««»,
        o un guion delante). El valor son esas dos cifras.
    COLOCACIÓN. Columna castellana, cuerpo, con la geometría validada de
        la 142 (``projected_form_a_recovery.validated_geometry``).
    PROGRESIÓN. Dentro del capítulo, en orden físico de bloques y viendo
        lo que dejaron los eventos anteriores: todo lo anterior lleva un
        versículo menor, el primer bloque de otro versículo detrás del
        tramo movido uno mayor, el valor cabe en el canon nativo y nadie
        lleva ya ese número. Un solo evento por referencia.

Se mueve el tramo contiguo del versículo que tenía el renglón, desde el
renglón. Si ese tramo era todo el versículo, el versículo sólo existía por
la primera cifra mal leída y desaparece (se RENOMBRA). Nada de esto lee
huecos, el versículo esperado, el anterior + 1 ni el siguiente - 1, ni
artefactos de diagnóstico.
"""
import re
from collections import Counter, defaultdict
from typing import Callable, Dict, List, Optional

#: La decisión que llevan los renglones leídos por esta ruta.
DECISION = "split_two_digit_marker"

SENTENCE_START = re.compile(r"[-–—]?[A-ZÁÉÍÓÚÑ¿¡«]")

MOVE = "moved_to_new_ref"
RELABEL = "relabelled_false_ref"
NOT_OWNED = "not_owned"
AMBIGUOUS_OWNER = "ambiguous_owner"
OUTSIDE_CANON = "outside_native_canon"
EXISTS = "verse_already_present"
BEHIND = "progression_behind_previous"
AHEAD = "progression_past_next"
AMBIGUOUS_EVENT = "ambiguous_source_event"
WOULD_APPLY = "would_apply"


def match(line):
    """(texto, valor) si el renglón abre con «d d Frase», si no (None, None)."""
    words = getattr(line, "words", None) or []
    if len(words) < 3:
        return None, None
    a, b = words[0].text.strip("."), words[1].text.strip(".")
    third = words[2].text
    if third in "-–—" and len(words) > 3:
        third = third + words[3].text
    if not (re.fullmatch(r"[1-9]", a) and re.fullmatch(r"[0-9]", b)
            and SENTENCE_START.match(third)):
        return None, None
    text = " ".join(w.text for w in words[2:]).strip()
    return text, int(a) * 10 + int(b)


#: TASK 156. El mismo número de dos cifras leído como UNA palabra con las
#: cifras cambiadas por letras («i3» = 13, «a6» = 26, «ao» = 20). Sólo las
#: confusiones sin ambigüedad (la 154 lo midió; «S» y «9» quedan fuera).
GLYPH = {"i": 1, "I": 1, "l": 1, "a": 2, "o": 0, "O": 0}
#: Palabras castellanas de dos letras escritas con esos glifos: nunca marca.
WORDS = {"la", "lo", "al", "La", "Lo", "Al", "LA", "LO", "AL", "oí", "Oí"}
DECISION_GLUED = "glued_two_char_marker"
OPENS_VERSE = "already_opens_its_verse"


def match_glued(line):
    """(texto, valor) si el renglón abre con «xy Frase» (task 156)."""
    words = getattr(line, "words", None) or []
    if len(words) < 2 or len(words[0].text) != 2:
        return None, None
    token = words[0].text
    if token in WORDS or not any(ch in GLYPH for ch in token):
        return None, None
    digits = []
    for ch in token:
        if ch.isdigit():
            digits.append(int(ch))
        elif ch in GLYPH:
            digits.append(GLYPH[ch])
        else:
            return None, None
    if digits[0] == 0 or not SENTENCE_START.match(words[1].text):
        return None, None
    text = " ".join(w.text for w in words[1:]).strip()
    return text, digits[0] * 10 + digits[1]


def apply(edition, candidates: List[dict], *, enabled: bool,
          verse_limit: Callable[[str, int], Optional[int]],
          skip_openers: bool = False,
          decision: str = DECISION) -> None:
    """Simula (o aplica, con ``enabled``) la regla, capítulo a capítulo.

    ``skip_openers`` (156): un renglón que ya abre su versículo lo leyó
    otra ruta y no se toca.
    """
    if not candidates:
        return
    wanted = {c["block_id"]: c for c in candidates}
    owners: Dict[str, list] = defaultdict(list)
    for osis, entry in edition.books.items():
        for number, chapter in entry.chapters.items():
            if not isinstance(number, int) or number < 1:
                continue
            for verse, slot in chapter.verses.items():
                if verse is None:
                    continue
                for blk in slot.blocks:
                    if blk.provenance.block_id in wanted:
                        owners[blk.provenance.block_id].append(
                            (osis, number, verse))
    by_chapter = defaultdict(list)
    for record in sorted(candidates, key=lambda r: r["block_id"]):
        owner = owners.get(record["block_id"], [])
        record["native_owner_refs"] = [f"{o}.{n}.{v}" for o, n, v in owner]
        record["proposed_native_ref"] = None
        if not owner:
            record["outcome"] = NOT_OWNED
        elif len(owner) != 1:
            record["outcome"] = AMBIGUOUS_OWNER
        else:
            by_chapter[owner[0][:2]].append(record)

    proposed = Counter()
    plans = []
    for (osis, number), records in sorted(by_chapter.items()):
        chapter = edition.books[osis].chapters[number]
        blocks = {}
        for verse, slot in chapter.verses.items():
            if verse is None:
                continue
            for blk in slot.blocks:
                if blk.provenance.block_id:
                    blocks[blk.provenance.block_id] = (blk, verse)
        labels = {bid: verse for bid, (_b, verse) in blocks.items()}
        order = sorted(labels)
        limit = verse_limit(osis, number)
        for record in records:
            value, block_id = record["value"], record["block_id"]
            if skip_openers:
                verse = labels[block_id]
                if min(b for b, v in labels.items() if v == verse) == block_id:
                    record["outcome"] = OPENS_VERSE
                    continue
            if limit is None or not 1 <= value <= limit:
                record["outcome"] = OUTSIDE_CANON
                continue
            if value in labels.values():
                record["outcome"] = EXISTS
                continue
            at = order.index(block_id)
            current = labels[block_id]
            end = at
            while end + 1 < len(order) and labels[order[end + 1]] == current:
                end += 1
            before = [labels[b] for b in order[:at]]
            after = next((labels[b] for b in order[end + 1:]), None)
            if before and max(before) >= value:
                record["outcome"] = BEHIND
                continue
            if after is not None and after <= value:
                record["outcome"] = AHEAD
                continue
            run = order[at:end + 1]
            relabel = (at == 0 or labels[order[at - 1]] != current) and \
                sum(1 for v in labels.values() if v == current) == len(run)
            record.update(outcome=RELABEL if relabel else MOVE,
                          prior_verse=current, blocks_moved=list(run),
                          proposed_native_ref=f"{osis}.{number}.{value}")
            proposed[record["proposed_native_ref"]] += 1
            for b in run:
                labels[b] = value
            plans.append((chapter, blocks, record))

    for chapter, blocks, record in plans:
        if proposed[record["proposed_native_ref"]] > 1:
            record["outcome"] = AMBIGUOUS_EVENT
            continue
        if not enabled:
            record["planned_outcome"] = record["outcome"]
            record["outcome"] = WOULD_APPLY
            continue
        value = record["value"]
        for bid in record["blocks_moved"]:
            blk, _old = blocks[bid]
            # El dueño actual puede ser un evento anterior del capítulo.
            for verse, slot in list(chapter.verses.items()):
                if blk in slot.blocks:
                    slot.blocks.remove(blk)
                    if not slot.blocks:
                        del chapter.verses[verse]
                    break
            blk.number = value
            chapter.verse(value).blocks.append(blk)
        marker = blocks[record["block_id"]][0]
        marker.text = record["text"]
        marker.decision = decision
        target = chapter.verses[value]
        target.blocks.sort(key=lambda b: b.provenance.block_id or "")
        record["applied"] = True
