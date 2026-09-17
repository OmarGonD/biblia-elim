"""
Quién reclama ser qué capítulo, antes de que nadie lo escriba.

    PRESERVE CLAIMS BEFORE RESOLUTION

Hasta ahora un rótulo se leía y se escribía en el acto:

    rótulo -> número -> capitulos[número] = capítulo

Un diccionario indexado por número no puede guardar dos reclamantes de la
misma clave, así que el segundo pisaba al primero y el informe decía
`duplicate_chapters = []`. Esa métrica no demostraba que no hubiera
conflictos: demostraba que la estructura elegida no sabe representarlos.
En el tomo 3 hay seis rótulos físicos reclamando Ps 100.

Aquí los reclamantes son primero una colección y sólo después una
estructura. Entre una cosa y otra caben las preguntas que antes no tenían
dónde hacerse:

    ¿el numeral está bien escrito?
    ¿son dos rótulos o es el mismo mirado dos veces?
    ¿hay dos rótulos distintos pidiendo el mismo número?

Y la regla de salida: ante un reclamo ambiguo o disputado no se acepta
nada en silencio. El capítulo queda sin número y marcado, que es
recuperable; escribir el número equivocado no lo es.

Ni la secuencia ni el canon deciden aquí. La secuencia puede corroborar
un numeral que ya se lee, y el canon sólo señala anomalías en el informe.

Y una condición que hay que decir aparte, porque es la que se coló la
primera vez:

    DETECTAR UN RECLAMO != ACEPTAR UN CAPÍTULO

El ancla de secuencia sólo la mueven los reclamos ACEPTADOS. Antes la
movía cualquier propuesta hecha durante el barrido, de modo que un rótulo
que acabaría rechazado --por numeral inválido, por ambiguo, por
disputado-- ya había servido de «señal independiente» a los rótulos que
venían detrás. Un capítulo rechazado sostenía a otros.

Por eso la resolución no ocurre durante la lectura sino aquí, y por
rondas: cada ronda propone usando ÚNICAMENTE los números ya aceptados, y
acepta lo que queda sin disputa. Lo que se acepta en una ronda sirve de
ancla en la siguiente. Nada provisional entra nunca en el estado, así
que no hay nada que deshacer: no es un rollback, es que el reclamo malo
nunca llegó a contar.
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import roman

#: Qué se ha hecho con cada reclamo.
ACCEPTED = "accepted"
#: El numeral no es un número romano, o no hay numeral.
INVALID_NUMERAL = "invalid_numeral"
#: Se leen varios numerales y ninguna señal dice cuál es.
AMBIGUOUS_NUMERAL = "ambiguous_numeral"
#: Se lee, pero sólo tras enmendar un glifo, y nada más lo sostiene.
UNCORROBORATED = "uncorroborated_correction"
#: El mismo rótulo físico visto dos veces (p.ej. OCR + revisión visual).
SAME_PHYSICAL = "same_physical_claim"
#: Dos rótulos FÍSICAMENTE distintos pidiendo el mismo número.
COMPETING = "competing_claim"
#: Se leyó un número pero ninguna otra señal lo corrobora.
UNRESOLVED = "unresolved"

#: De dónde viene el reclamo. Decide su autoridad.
FROM_OCR = "ocr"
FROM_IMAGE_REVIEW = "image_review"

#: Orden de autoridad. La secuencia y el canon NO están: no son fuentes
#: de número, son corroboración y auditoría. Un reclamo nunca se acepta
#: por tener un hueco delante y otro detrás.
AUTHORITY = {
    FROM_IMAGE_REVIEW: 3,     # numeral leído en el facsímil
    FROM_OCR: 1,              # numeral impreso leído por la máquina
}


@dataclass(frozen=True)
class Proposal:
    """Qué número saldría de un rótulo con el ancla que haya ahora.

    Se recalcula en cada ronda. No es una aceptación y no toca el estado
    de nadie: mientras el reclamo no se acepte, el número que propone no
    existe para los demás.
    """
    resolved: Optional[int]
    method: str = UNRESOLVED
    evidence: dict = field(default_factory=dict)
    confidence: float = 0.0


@dataclass
class ChapterClaim:
    """Un rótulo que dice ser un capítulo. Todavía no lo es."""
    claim_id: str
    book: str
    scan_page: int
    block_id: str
    raw_heading: str
    source: str = FROM_OCR
    bbox: Optional[tuple] = None
    column: Optional[str] = None
    zone: Optional[str] = None
    #: Lo que dio la lectura del numeral, con su estado y su motivo.
    numeral: dict = field(default_factory=dict)
    candidate_numbers: List[int] = field(default_factory=list)
    #: Lo que el resolver propone, con qué método y con qué pruebas. Una
    #: propuesta no es una aceptación.
    proposed_number: Optional[int] = None
    proposal_method: str = UNRESOLVED
    proposal_evidence: dict = field(default_factory=dict)
    confidence: float = 0.0
    provenance: Optional[dict] = None
    #: Hueco de trabajo donde se van acumulando los versículos mientras
    #: no se sabe qué capítulo es. Siempre negativo: nunca puede
    #: confundirse con un número de capítulo real.
    slot: Optional[int] = None

    #: Posición en el orden de lectura. Es lo que define «antes» para el
    #: ancla de secuencia: el capítulo aceptado más cercano por detrás.
    order: int = 0
    #: ¿Es el primer rótulo de su libro en orden de lectura? Sólo para
    #: ése tiene sentido «lo que tocaba» sin tener nada aceptado detrás.
    first_in_book: bool = False

    accepted_number: Optional[int] = None
    disposition: str = UNRESOLVED
    #: En qué ronda se aceptó. 0 quiere decir «sin necesitar ancla»:
    #: numeral leído y corroborado por la cabecera, o facsímil.
    accepted_round: Optional[int] = None
    #: El número que tenía delante cuando se aceptó, y de qué reclamo
    #: salía. None quiere decir que no se apoyó en ninguno.
    anchor_number: Optional[int] = None
    anchor_claim: Optional[str] = None
    reason: str = ""
    merged_into: Optional[str] = None
    competing_with: List[str] = field(default_factory=list)

    @property
    def authority(self) -> int:
        return AUTHORITY.get(self.source, 0)

    @property
    def review_required(self) -> bool:
        return self.accepted_number is None

    @property
    def physical_key(self):
        """Dónde está el rótulo, no qué dice.

        Dos representaciones del mismo rótulo impreso comparten plana y
        renglón aunque una venga del reconocimiento y la otra de mirar la
        imagen. El identificador de bloque lleva «l» o «r» según de dónde
        salga, así que la comparación se hace sobre plana y renglón.
        """
        line = None
        if self.block_id and len(self.block_id) >= 10:
            try:
                line = int(self.block_id[6:])
            except ValueError:
                line = None
        return (self.book, self.scan_page, line)

    def as_dict(self) -> dict:
        return {
            "claim_id": self.claim_id, "book": self.book,
            "scan_page": self.scan_page, "block_id": self.block_id,
            "bbox": list(self.bbox) if self.bbox else None,
            "column": self.column, "zone": self.zone,
            "raw_heading": self.raw_heading[:120],
            "numeral": self.numeral,
            "candidate_numbers": list(self.candidate_numbers),
            "proposed_number": self.proposed_number,
            "proposal_method": self.proposal_method,
            "proposal_evidence": self.proposal_evidence,
            "confidence": self.confidence, "source": self.source,
            "accepted_number": self.accepted_number,
            "disposition": self.disposition, "reason": self.reason,
            "review_required": self.review_required,
            "merged_into": self.merged_into,
            "competing_with": list(self.competing_with),
            "order": self.order,
            "accepted_round": self.accepted_round,
            "anchor_number": self.anchor_number,
            "anchor_claim": self.anchor_claim,
            "provenance": self.provenance,
        }


@dataclass
class ClaimGroup:
    """Todos los que piden el mismo número del mismo libro."""
    book: str
    number: int
    claims: List[ChapterClaim] = field(default_factory=list)
    disposition: str = ACCEPTED
    reason: str = ""

    @property
    def physical_places(self):
        return {c.physical_key for c in self.claims}

    @property
    def is_collision(self) -> bool:
        """Dos sitios distintos del impreso pidiendo el mismo número."""
        return len(self.physical_places) > 1

    def as_dict(self) -> dict:
        return {
            "book": self.book, "claimed_number": self.number,
            "claimants": [c.as_dict() for c in self.claims],
            "claimant_count": len(self.claims),
            "distinct_physical_places": len(self.physical_places),
            "disposition": self.disposition, "reason": self.reason,
        }


class ClaimLedger:
    """La colección de reclamos, y la política que los resuelve."""

    def __init__(self):
        self.claims: List[ChapterClaim] = []
        self.groups: List[ClaimGroup] = []
        self.rounds = 0
        self._resolved = False

    def add(self, claim: ChapterClaim) -> ChapterClaim:
        claim.order = len(self.claims)
        self.claims.append(claim)
        return claim

    def by_id(self, claim_id) -> Optional[ChapterClaim]:
        for claim in self.claims:
            if claim.claim_id == claim_id:
                return claim
        return None

    # -- 1. el mismo rótulo visto dos veces ---------------------------
    def _merge_same_physical(self) -> List[ChapterClaim]:
        """Funde las representaciones del mismo rótulo impreso.

        Un rótulo que el reconocimiento leyó mal y una persona leyó bien
        en la imagen NO son dos capítulos en disputa: son el mismo
        capítulo, y la imagen corrige a la máquina. Tratarlo como
        conflicto sería castigar justo la evidencia mejor.
        """
        by_place: Dict[tuple, List[ChapterClaim]] = {}
        for claim in self.claims:
            by_place.setdefault(claim.physical_key, []).append(claim)

        survivors = []
        for _place, group in by_place.items():
            if len(group) == 1:
                survivors.append(group[0])
                continue
            # Manda el de más autoridad; a igualdad, el primero leído.
            group = sorted(group, key=lambda c: (-c.authority, c.claim_id))
            winner, rest = group[0], group[1:]
            for loser in rest:
                loser.disposition = SAME_PHYSICAL
                loser.merged_into = winner.claim_id
                loser.reason = (
                    f"the same printed heading as {winner.claim_id}; "
                    f"{winner.source} evidence stands over {loser.source}")
            survivors.append(winner)
        return survivors

    # -- 2. ¿puede este reclamo proponer un número? -------------------
    @staticmethod
    def _screen(claim: ChapterClaim, proposal) -> bool:
        """Deja pasar sólo lo que de verdad ha leído un número.

        Un numeral mal escrito no es un número bajo. Es un numeral que no
        se ha leído, y la diferencia importa: «XXL» no vale 30, no vale
        nada, y decir que vale 30 mueve un capítulo entero de sitio.

        `proposal` es lo que la política propone para ESTE reclamo con el
        ancla que haya en este momento. Se vuelve a evaluar en cada
        ronda, porque el ancla cambia según se van aceptando otros.
        """
        status = claim.numeral.get("status")
        if claim.source == FROM_IMAGE_REVIEW:
            # Una revisión visual con numeral es la mejor evidencia que
            # hay y no pasa por más filtros. Una que confirmó la frontera
            # pero no pudo leer el numeral sigue sin número: eso no lo
            # completa ni la secuencia ni nadie.
            if proposal.resolved is not None:
                return True
            claim.disposition = UNRESOLVED
            claim.reason = ("the facsimile confirmed the boundary but not "
                            "the printed numeral")
            return False

        if status == roman.INVALID_SYNTAX:
            claim.disposition = INVALID_NUMERAL
            permissive = claim.numeral.get("permissive_value")
            claim.reason = claim.numeral.get("reason") or "numeral not Roman"
            if permissive is not None:
                claim.reason += (f"; it is NOT being read as {permissive} "
                                 f"and no other signal may supply a number")
            return False
        if proposal.resolved is None:
            # La ambigüedad se nombra antes que el genérico: un rótulo del
            # que se leen dos numerales y ninguno se impone no es lo
            # mismo que uno del que no se lee ninguno, y el informe tiene
            # que poder distinguirlos para saber qué se puede rescatar.
            if len(claim.candidate_numbers) > 1:
                claim.disposition = AMBIGUOUS_NUMERAL
                claim.reason = (
                    f"the heading yields {len(claim.candidate_numbers)} "
                    f"readable numerals {claim.candidate_numbers} and "
                    f"nothing decides between them")
                return False
            claim.disposition = UNRESOLVED
            claim.reason = (proposal.evidence.get("why")
                            or claim.numeral.get("reason")
                            or "no number could be read")
            return False
        if len(claim.candidate_numbers) > 1 and proposal.confidence < 0.85:
            claim.disposition = AMBIGUOUS_NUMERAL
            claim.reason = (
                f"the heading yields {len(claim.candidate_numbers)} readable "
                f"numerals {claim.candidate_numbers} and nothing decides "
                f"between them")
            return False
        if claim.numeral.get("origin") == roman.FROM_CONFUSABLE and \
                not proposal.evidence.get("agrees"):
            claim.disposition = UNCORROBORATED
            claim.reason = ("the numeral reads only after correcting a "
                            "confusable glyph and no independent signal "
                            "agrees with it")
            return False
        return True

    # -- 3. resolución por rondas sobre lo ya aceptado -----------------
    def _anchor(self, claim: ChapterClaim):
        """El capítulo ACEPTADO más cercano por detrás, en su libro.

        Nunca una propuesta, nunca un reclamo pendiente: si el rótulo de
        delante todavía no es un capítulo, para éste no existe.
        """
        best = None
        for other in self.claims:
            if other.book != claim.book or other.order >= claim.order:
                continue
            if other.disposition != ACCEPTED or other.accepted_number is None:
                continue
            if best is None or other.order > best.order:
                best = other
        return best

    def resolve(self, propose=None, *, max_rounds: int = 200) -> "ClaimLedger":
        """Decide, y deja escrito por qué.

        `propose(claim, previous)` es la política de numeración: qué
        número saldría de este rótulo si lo que tuviera delante fuese
        `previous`. Se le llama una vez por reclamo y por ronda, y
        `previous` es SIEMPRE un número ya aceptado o None. Sin política
        se usa lo que el propio reclamo trajera anotado, que es como lo
        construyen las pruebas.

        Agrupa por (libro, número) DESPUÉS de conservar a todos los
        reclamantes, que es la única forma de ver una colisión: en el
        diccionario final ya no existiría.
        """
        if self._resolved:
            return self
        if propose is None:
            def propose(claim, previous):
                return Proposal(claim.proposed_number, claim.proposal_method,
                                dict(claim.proposal_evidence),
                                claim.confidence)

        survivors = self._merge_same_physical()
        seen_books = set()
        for claim in sorted(self.claims, key=lambda c: c.order):
            claim.first_in_book = claim.book not in seen_books
            seen_books.add(claim.book)
        pending = list(survivors)
        taken: Dict[tuple, ChapterClaim] = {}
        contested: Dict[tuple, List[ChapterClaim]] = {}
        last_proposal: Dict[str, object] = {}
        self.rounds = 0

        while pending and self.rounds < max_rounds:
            self.rounds += 1
            offers: Dict[tuple, List[tuple]] = {}
            for claim in pending:
                anchor = self._anchor(claim)
                previous = anchor.accepted_number if anchor else None
                proposal = propose(claim, previous)
                last_proposal[claim.claim_id] = (proposal, anchor)
                # La propuesta de la ronda queda anotada aunque se
                # rechace: saber que la política HABRÍA dicho
                # «running_header_correlated» y por qué no se aceptó es
                # la mitad del valor del informe.
                claim.proposed_number = proposal.resolved
                claim.proposal_method = proposal.method
                claim.proposal_evidence = dict(proposal.evidence)
                claim.confidence = proposal.confidence
                # `_screen` deja anotada la disposición provisional del
                # reclamo; si más adelante se acepta, se sobrescribe.
                if not self._screen(claim, proposal):
                    continue
                offers.setdefault((claim.book, proposal.resolved), []).append(
                    (claim, proposal, anchor))

            newly = []
            for key, items in sorted(offers.items(), key=lambda kv: str(kv[0])):
                book, number = key
                if key in taken:
                    # Ya lo tiene un capítulo aceptado: esto es una
                    # colisión, no una alternativa.
                    contested.setdefault(key, [taken[key]])
                    contested[key].extend(c for c, _p, _a in items)
                    continue
                places = {c.physical_key for c, _p, _a in items}
                if len(places) > 1:
                    best = max(c.authority for c, _p, _a in items)
                    top = [c for c, _p, _a in items if c.authority == best]
                    if len(top) == 1 and \
                            best > min(c.authority for c, _p, _a in items):
                        winner, proposal, anchor = next(
                            i for i in items if i[0] is top[0])
                        self._accept(winner, number, proposal, anchor,
                                     f"{len(items)} headings claimed {book} "
                                     f"{number}; this one is settled by "
                                     f"{winner.source} evidence")
                        taken[key] = winner
                        newly.append(winner)
                        contested.setdefault(key, []).extend(
                            c for c, _p, _a in items)
                    else:
                        contested.setdefault(key, []).extend(
                            c for c, _p, _a in items)
                    continue
                winner, proposal, anchor = max(
                    items, key=lambda i: (i[0].authority, i[1].confidence))
                self._accept(winner, number, proposal, anchor,
                             f"{proposal.method}; sole claimant")
                taken[key] = winner
                newly.append(winner)

            if not newly:
                break
            for claim in newly:
                pending.remove(claim)

        # Lo que quedó sin aceptar se clasifica con su última propuesta.
        for claim in pending:
            proposal, anchor = last_proposal.get(
                claim.claim_id, (Proposal(None, UNRESOLVED, {}, 0.0), None))
            key = (claim.book, proposal.resolved)
            if proposal.resolved is not None and key in contested:
                # Sin duplicados: `contested` se va llenando ronda a
                # ronda y un mismo reclamante puede haber entrado varias
                # veces. Contar las entradas en vez de los reclamantes
                # daba «8 rótulos reclaman Prov 26» donde había dos.
                rivals, seen = [], {claim.claim_id}
                for other in contested[key]:
                    if other.claim_id in seen:
                        continue
                    seen.add(other.claim_id)
                    rivals.append(other)
                claim.disposition = COMPETING
                claim.competing_with = [c.claim_id for c in rivals]
                claim.reason = (
                    f"{len(rivals) + 1} distinct printed headings claim "
                    f"{claim.book} {proposal.resolved}; "
                    + ("another one is settled by stronger evidence"
                       if key in taken else
                       "no evidence settles which"))
            elif claim.disposition == ACCEPTED:
                claim.disposition = UNRESOLVED
                claim.reason = "left pending when the rounds stopped"

        self._build_groups(taken, contested)
        self._resolved = True
        return self

    def _accept(self, claim, number, proposal, anchor, reason):
        claim.accepted_number = number
        claim.disposition = ACCEPTED
        claim.accepted_round = self.rounds
        claim.proposed_number = number
        claim.proposal_method = proposal.method
        claim.proposal_evidence = dict(proposal.evidence)
        claim.confidence = proposal.confidence
        claim.anchor_number = anchor.accepted_number if anchor else None
        claim.anchor_claim = anchor.claim_id if anchor else None
        claim.reason = reason
        claim.competing_with = []

    def _build_groups(self, taken, contested):
        seen = set()
        for key, claims in contested.items():
            book, number = key
            unique = []
            for claim in claims:
                if claim.claim_id not in {c.claim_id for c in unique}:
                    unique.append(claim)
            group = ClaimGroup(book=book, number=number, claims=unique)
            winner = taken.get(key)
            if winner is not None:
                group.disposition = ACCEPTED
                group.reason = f"settled by {winner.source} evidence"
            else:
                group.disposition = COMPETING
                group.reason = ("distinct printed headings claim the same "
                                "chapter and nothing available decides")
            self.groups.append(group)
            seen.add(key)
        for key, winner in sorted(taken.items(), key=lambda kv: str(kv[0])):
            if key in seen:
                continue
            self.groups.append(ClaimGroup(book=key[0], number=key[1],
                                          claims=[winner],
                                          disposition=ACCEPTED))

    # -- 4. informe ----------------------------------------------------
    def accepted(self) -> List[ChapterClaim]:
        return [c for c in self.claims if c.disposition == ACCEPTED]

    def collisions(self) -> List[ClaimGroup]:
        return [g for g in self.groups if g.is_collision]

    def counts(self) -> dict:
        out = {"total_claims": len(self.claims)}
        for name in (ACCEPTED, INVALID_NUMERAL, AMBIGUOUS_NUMERAL,
                     UNCORROBORATED, SAME_PHYSICAL, COMPETING, UNRESOLVED):
            out[name] = sum(1 for c in self.claims if c.disposition == name)
        out["unresolved_claims"] = len(self.claims) - out[ACCEPTED]
        out["competing_claim_groups"] = len(self.collisions())
        out["recovered_by_image_review"] = sum(
            1 for c in self.accepted() if c.source == FROM_IMAGE_REVIEW)
        return out

    def per_book(self) -> dict:
        out: Dict[str, dict] = {}
        for claim in self.claims:
            stat = out.setdefault(claim.book, {
                "raw_claims": 0, "valid_numeral": 0, "invalid_numeral": 0,
                "ambiguous": 0, "uncorroborated": 0,
                "same_physical_duplicates": 0, "competing": 0,
                "accepted": 0, "unresolved": 0})
            stat["raw_claims"] += 1
            if claim.numeral.get("status") == roman.VALID:
                stat["valid_numeral"] += 1
            elif claim.numeral.get("status") == roman.INVALID_SYNTAX:
                stat["invalid_numeral"] += 1
            if claim.disposition == AMBIGUOUS_NUMERAL:
                stat["ambiguous"] += 1
            elif claim.disposition == UNCORROBORATED:
                stat["uncorroborated"] += 1
            elif claim.disposition == SAME_PHYSICAL:
                stat["same_physical_duplicates"] += 1
            elif claim.disposition == COMPETING:
                stat["competing"] += 1
            if claim.disposition == ACCEPTED:
                stat["accepted"] += 1
            else:
                stat["unresolved"] += 1
        for book, stat in out.items():
            stat["competing_claim_groups"] = sum(
                1 for g in self.collisions() if g.book == book)
        return out

    def permissive_collisions(self) -> List[dict]:
        """Qué habría chocado con el converter antiguo.

        No decide nada: mide. Se reagrupan los MISMOS reclamos por el
        número que producía la lectura permisiva -- la que sumaba y
        restaba sin comprobar si el token era un romano -- y se cuentan
        los grupos en los que dos sitios distintos del impreso caían
        encima. Ese es exactamente el daño que la métrica vieja no podía
        ver: en un diccionario indexado por número, el segundo pisaba al
        primero y el informe salía limpio.
        """
        buckets: Dict[tuple, List[ChapterClaim]] = {}
        for claim in self.claims:
            number = claim.numeral.get("permissive_value")
            if number is None and claim.numeral.get("status") == roman.VALID:
                number = claim.numeral.get("value")
            if number is None:
                continue
            buckets.setdefault((claim.book, number), []).append(claim)
        out = []
        for (book, number), claims in sorted(buckets.items()):
            places = {c.physical_key for c in claims}
            if len(places) < 2:
                continue
            out.append({
                "book": book, "claimed_number": number,
                "claimant_count": len(claims),
                "distinct_physical_places": len(places),
                "claimants": [{"scan_page": c.scan_page,
                               "block_id": c.block_id,
                               "raw_heading": c.raw_heading[:70],
                               "numeral_token": c.numeral.get("token"),
                               "numeral_status": c.numeral.get("status")}
                              for c in claims],
            })
        return out

    def report(self) -> dict:
        counts = self.counts()
        counts["per_book"] = self.per_book()
        counts["rounds"] = self.rounds
        # Dos cosas que NO son lo mismo y que conviene no leer juntas:
        #
        #   competing_chapter_claims        conflictos REALES que quedan
        #                                   hoy, medidos sobre los números
        #                                   que se han leído de verdad.
        #
        #   permissive_value_collision_*    DIAGNÓSTICO histórico: cuántos
        #                                   habría habido con el converter
        #                                   antiguo, que sacaba un número
        #                                   de cualquier secuencia de
        #                                   letras. No son reclamos
        #                                   válidos en disputa: son la
        #                                   medida del daño que el
        #                                   diccionario ocultaba.
        counts["competing_chapter_claims"] = len(self.collisions())
        counts["competing_claim_groups_detail"] = [
            g.as_dict() for g in self.collisions()]
        counts["collision_groups"] = counts["competing_claim_groups_detail"]
        permissive = self.permissive_collisions()
        counts["permissive_value_collision_groups"] = len(permissive)
        counts["permissive_value_collision_detail"] = permissive
        counts["permissive_value_collision_note"] = (
            "diagnostic only: these are NOT current competing claims. They "
            "count how many (book, number) slots two physically distinct "
            "headings would have landed on under the old permissive numeral "
            "reading, each of which the chapter map silently collapsed.")
        counts["claims"] = [c.as_dict() for c in self.claims]
        return counts


def materialize(edition, ledger: ClaimLedger) -> dict:
    """Escribe en la edición SÓLO los reclamos aceptados.

    Hasta aquí cada rótulo ha ido acumulando sus versículos en un hueco
    de trabajo negativo, que no puede chocar con nada. Ahora los
    aceptados pasan a su número y los demás se quedan donde están, en
    revisión: siguen teniendo sus versículos y su procedencia, pero no
    producen una referencia de capítulo que no se ha demostrado.
    """
    moved, kept = 0, 0
    for claim in ledger.claims:
        if claim.slot is None:
            continue
        book = edition.books.get(claim.book)
        if book is None or claim.slot not in book.chapters:
            continue
        if claim.disposition != ACCEPTED or claim.accepted_number is None:
            kept += 1
            continue
        chapter = book.chapters.pop(claim.slot)
        chapter.number = claim.accepted_number
        # El ledger garantiza un solo aceptado por (libro, número), así
        # que esto no puede pisar nada; si lo pisara sería un fallo de la
        # resolución y hay que verlo, no absorberlo.
        assert claim.accepted_number not in book.chapters, (
            f"{claim.book} {claim.accepted_number} materialised twice")
        book.chapters[claim.accepted_number] = chapter
        moved += 1
    return {"materialized": moved, "left_in_review": kept}
