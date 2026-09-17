"""
Qué numerales hay que ir a leer al facsímil, y en qué orden.

No construye nada: lee los reclamos que el ClaimLedger ya conserva y los
ordena. Un rótulo que no se pudo identificar no es un misterio abstracto
-- es una plana concreta, un recuadro concreto y una tinta que sigue ahí.
Esto sólo dice a cuál merece la pena ir primero.

El orden no lo decide el canon ni el hueco en la numeración, que son las
dos formas fáciles de acabar mirando donde uno quiere encontrar algo:

    1  reclamos en disputa -- dos rótulos del impreso pidiendo el mismo
       capítulo. Mientras no se distingan, los dos están perdidos.

    2  numerales inválidos cuyo valor permisivo caía encima de otro
       capítulo. Son los que el converter antiguo resolvía mal Y además
       le robaban el número a un rótulo legítimo.

    3  numerales inválidos que bloquean una cadena: detrás de ellos hay
       rótulos que SÍ se leen y que no se aceptan por falta de ancla.

    4  el resto.

Ninguna prioridad afirma nada sobre el contenido. Lo que diga el impreso
lo dice el impreso.
"""
from dataclasses import dataclass, field
from typing import List, Optional

#: Motivo por el que un reclamo entra en la cola. Entra todo lo que no
#: se aceptó: un rótulo cuyo numeral no se leyó entero («no_numeral») o
#: que se leyó bien pero no cabe en el libro («out_of_range») necesita
#: exactamente la misma revisión que uno mal escrito, y dejarlo fuera de
#: la cola por cómo se llame su problema lo escondería.
COMPETING = "competing_claim"
INVALID = "invalid_numeral"
AMBIGUOUS = "ambiguous_numeral"
UNRESOLVED = "unresolved"
QUEUED = (COMPETING, INVALID, AMBIGUOUS, UNRESOLVED)

PRIORITY_COMPETING = 1
PRIORITY_STOLE_A_CHAPTER = 2
PRIORITY_BLOCKS_A_CHAIN = 3
PRIORITY_REST = 4


@dataclass
class QueueEntry:
    """Un reclamo pendiente, con todo lo que hace falta para ir a mirarlo."""
    priority: int
    book: str
    scan_page: int
    pdf_page: int
    block_id: str
    disposition: str
    numeral_status: Optional[str]
    raw_heading: str
    raw_numeral: Optional[str]
    bbox: Optional[list] = None
    permissive_value: Optional[int] = None
    candidate_numbers: List[int] = field(default_factory=list)
    previous_accepted: Optional[int] = None
    previous_accepted_page: Optional[int] = None
    next_accepted: Optional[int] = None
    next_accepted_page: Optional[int] = None
    competing_with: List[str] = field(default_factory=list)
    competing_for: Optional[int] = None
    reason: str = ""
    priority_reason: str = ""

    def as_dict(self) -> dict:
        return dict(self.__dict__)


def build(claims: List[dict], *, permissive_groups=None,
          competing_groups=None) -> List[QueueEntry]:
    """La cola, a partir de los reclamos que el ledger ya tiene.

    `claims` son los diccionarios que el ledger publica; no se vuelve a
    parsear nada ni se recorre el OCR otra vez. Un paso por los reclamos
    para los vecinos aceptados y otro para clasificar: O(reclamos).
    """
    permissive_groups = permissive_groups or []
    competing_groups = competing_groups or []

    ordered = sorted(claims, key=lambda c: c.get("order", 0))

    # Vecinos aceptados, en una pasada hacia delante y otra hacia atrás.
    previous, nxt = {}, {}
    seen = {}
    for claim in ordered:
        key = claim["claim_id"]
        previous[key] = seen.get(claim["book"])
        if claim["disposition"] == "accepted":
            seen[claim["book"]] = claim
    seen = {}
    for claim in reversed(ordered):
        key = claim["claim_id"]
        nxt[key] = seen.get(claim["book"])
        if claim["disposition"] == "accepted":
            seen[claim["book"]] = claim

    # Los que le quitaban el sitio a otro capítulo bajo la lectura
    # permisiva: prioridad 2.
    stole = set()
    for group in permissive_groups:
        for claimant in group.get("claimants", []):
            stole.add(claimant["block_id"])

    # Los que tienen detrás un rótulo legible esperando ancla: prioridad 3.
    blocks_chain = set()
    for index, claim in enumerate(ordered):
        if claim["disposition"] == "accepted":
            continue
        for later in ordered[index + 1:]:
            if later["book"] != claim["book"]:
                break
            if later["disposition"] == "accepted":
                break
            if later["numeral"].get("status") == "valid":
                blocks_chain.add(claim["claim_id"])
            break

    competing_number = {}
    for group in competing_groups:
        for claimant in group.get("claimants", []):
            competing_number[claimant["block_id"]] = group["claimed_number"]

    out = []
    for claim in ordered:
        disposition = claim["disposition"]
        if disposition not in QUEUED:
            continue
        if disposition == COMPETING:
            priority = PRIORITY_COMPETING
            why = "two printed headings claim the same chapter"
        elif claim["block_id"] in stole:
            priority = PRIORITY_STOLE_A_CHAPTER
            why = ("its permissive value landed on a chapter another "
                   "heading also claimed")
        elif claim["claim_id"] in blocks_chain:
            priority = PRIORITY_BLOCKS_A_CHAIN
            why = "a readable heading right after it has no anchor"
        else:
            priority = PRIORITY_REST
            why = "unidentified numeral"

        before, after = previous.get(claim["claim_id"]), nxt.get(claim["claim_id"])
        out.append(QueueEntry(
            priority=priority, book=claim["book"],
            scan_page=claim["scan_page"], pdf_page=claim["scan_page"] + 1,
            block_id=claim["block_id"], disposition=disposition,
            numeral_status=claim["numeral"].get("status"),
            raw_heading=claim["raw_heading"],
            raw_numeral=claim["numeral"].get("token"),
            bbox=claim.get("bbox"),
            permissive_value=claim["numeral"].get("permissive_value"),
            candidate_numbers=list(claim.get("candidate_numbers") or []),
            previous_accepted=before["accepted_number"] if before else None,
            previous_accepted_page=before["scan_page"] if before else None,
            next_accepted=after["accepted_number"] if after else None,
            next_accepted_page=after["scan_page"] if after else None,
            competing_with=list(claim.get("competing_with") or []),
            competing_for=competing_number.get(claim["block_id"]),
            reason=claim.get("reason", ""), priority_reason=why))

    out.sort(key=lambda e: (e.priority, e.book, e.scan_page))
    return out


def summary(entries: List[QueueEntry]) -> dict:
    out = {"total": len(entries), "by_priority": {}, "by_disposition": {},
           "by_numeral_status": {}, "by_book": {}}
    for entry in entries:
        out["by_priority"][entry.priority] = \
            out["by_priority"].get(entry.priority, 0) + 1
        out["by_disposition"][entry.disposition] = \
            out["by_disposition"].get(entry.disposition, 0) + 1
        status = entry.numeral_status or "unknown"
        out["by_numeral_status"][status] = \
            out["by_numeral_status"].get(status, 0) + 1
        out["by_book"][entry.book] = out["by_book"].get(entry.book, 0) + 1
    return out
