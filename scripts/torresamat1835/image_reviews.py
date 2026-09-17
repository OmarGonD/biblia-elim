"""
Divisiones de capítulo recuperadas del facsímil.

El reconocimiento del testigo no produjo el rótulo impreso de algunas
planas, y se comprobó que ninguna representación OCR aguas arriba lo
conserva. La única evidencia que queda es la imagen, y una lectura sobre
la imagen es una transcripción del testigo: por eso vive como DATO
versionado y no como código.

Este módulo sólo carga, valida y aplica esa metadata. No decide nada: ni
la secuencia, ni el canon, ni la cabecera corrida pueden crear una
entrada aquí.

Y una revisión no se aplica si el artefacto visual no es el que se miró:
el sha256 del PDF es la llave. Sin esa guarda, «plana 475, recuadro X»
podría caer sobre otra edición.
"""
import hashlib
import json
import os
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "chapter_image_reviews.json")

#: Los tres desenlaces posibles de mirar una plana.
BOUNDARY_AND_NUMBER = "boundary_and_number_confirmed"
BOUNDARY_ONLY = "boundary_confirmed_number_unknown"
NO_BOUNDARY = "no_boundary"
OUTCOMES = (BOUNDARY_AND_NUMBER, BOUNDARY_ONLY, NO_BOUNDARY)


class ReviewError(Exception):
    """La metadata de revisión no cuadra con el testigo."""


@dataclass
class ChapterImageReview:
    id: str
    book: str
    scan_page: int
    outcome: str
    chapter_number: Optional[int]
    boundary_confirmed: bool
    numeral_confirmed: bool
    insert_after_block: Optional[str]
    insert_before_block: Optional[str]
    observed_printed_text: Optional[str]
    crop_bbox: Optional[tuple]
    confidence: float
    rationale: str
    reviewer_method: str
    printed_page: Optional[int] = None
    pdf_page: Optional[int] = None
    evidence: List[str] = field(default_factory=list)

    @property
    def creates_boundary(self) -> bool:
        return self.outcome in (BOUNDARY_AND_NUMBER, BOUNDARY_ONLY)

    @property
    def review_required(self) -> bool:
        """Una frontera sin numeral legible sigue necesitando revisión."""
        return self.outcome == BOUNDARY_ONLY


def sha256_of(path, chunk=1 << 20):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(chunk), b""):
            digest.update(block)
    return digest.hexdigest()


def load(path=REVIEWS):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def validate(data, *, page_bounds=None, page_count=None) -> List[str]:
    """Comprueba la metadata contra sí misma y contra el testigo.

    `page_bounds` es {scan_page: (width, height)} cuando se quiere
    comprobar que cada recuadro cae dentro de su plana.
    """
    problems = []
    seen = set()
    source = data.get("visual_source", {})
    if len(source.get("sha256", "")) != 64:
        problems.append("visual_source.sha256 is not a sha256")
    if source.get("may_produce_release_artifact"):
        problems.append("the visual source must not be release-capable")

    for review in data.get("reviews", []):
        rid = review.get("id") or "(sin id)"
        if rid in seen:
            problems.append(f"{rid}: duplicated id")
        seen.add(rid)
        if review.get("outcome") not in OUTCOMES:
            problems.append(f"{rid}: unknown outcome {review.get('outcome')!r}")
        if not review.get("rationale"):
            problems.append(f"{rid}: empty rationale")
        page = review.get("scan_page")
        if not isinstance(page, int) or page < 0:
            problems.append(f"{rid}: bad scan_page")
        elif page_count is not None and page >= page_count:
            problems.append(f"{rid}: scan_page {page} beyond the witness")
        if review.get("outcome") == BOUNDARY_AND_NUMBER:
            if not isinstance(review.get("chapter_number"), int):
                problems.append(f"{rid}: confirmed number is not an integer")
            if not review.get("numeral_confirmed"):
                problems.append(f"{rid}: number confirmed but numeral_confirmed false")
        if review.get("outcome") == BOUNDARY_ONLY:
            if review.get("chapter_number") is not None:
                problems.append(f"{rid}: number must stay unknown here")
        if review.get("outcome") == NO_BOUNDARY:
            if review.get("boundary_confirmed"):
                problems.append(f"{rid}: rejected candidate cannot confirm a boundary")
            if review.get("insert_before_block") or review.get("insert_after_block"):
                problems.append(f"{rid}: a rejection has no insertion anchor")
        box = review.get("crop_bbox")
        if box is not None:
            if len(box) != 4 or box[0] >= box[2] or box[1] >= box[3]:
                problems.append(f"{rid}: malformed crop_bbox")
            elif page_bounds and page in page_bounds:
                width, height = page_bounds[page]
                if box[0] < 0 or box[1] < 0 or box[2] > width or box[3] > height:
                    problems.append(f"{rid}: crop_bbox outside page {page}")
    return problems


def reviews_for(data, *, source_path=None, expected_sha256=None
                ) -> List[ChapterImageReview]:
    """Las revisiones aplicables, con la guarda de hash puesta.

    Si el artefacto visual no es el que se revisó, no se aplica ninguna:
    se falla y se dice por qué. Nunca en silencio.
    """
    declared = data.get("visual_source", {}).get("sha256")
    actual = expected_sha256
    if actual is None and source_path:
        if not os.path.isfile(source_path):
            raise ReviewError(
                f"{os.path.basename(source_path)} is not in the cache; the "
                f"reviews cannot be checked against the witness")
        actual = sha256_of(source_path)
    if actual is None:
        raise ReviewError("no visual source to check the reviews against")
    if actual != declared:
        raise ReviewError(
            f"visual source sha256 {actual} != {declared} recorded with the "
            f"reviews; they describe another artefact and are not applied")

    out = []
    for review in data.get("reviews", []):
        out.append(ChapterImageReview(
            id=review["id"], book=review["book"],
            scan_page=review["scan_page"], outcome=review["outcome"],
            chapter_number=review.get("chapter_number"),
            boundary_confirmed=bool(review.get("boundary_confirmed")),
            numeral_confirmed=bool(review.get("numeral_confirmed")),
            insert_after_block=review.get("insert_after_block"),
            insert_before_block=review.get("insert_before_block"),
            observed_printed_text=review.get("observed_printed_text"),
            crop_bbox=tuple(review["crop_bbox"]) if review.get("crop_bbox") else None,
            confidence=float(review.get("confidence", 0.0)),
            rationale=review["rationale"],
            reviewer_method=review.get("reviewer_method", ""),
            printed_page=review.get("printed_page"),
            pdf_page=review.get("pdf_page"),
            evidence=[review.get("observed_context")] if review.get("observed_context") else [],
        ))
    return out


_LEAF_RE = re.compile(rb'<PARAM name="PAGE" value="[^"]*?_(\d+)\.djvu"')
_PDF_COUNT_RE = re.compile(rb"/Count\s+(\d+)")


def verify_page_mapping(xml_path, pdf_path, *, chunk=1 << 22) -> dict:
    """Comprueba pdf_page = scan_page + 1 como invariante, no como desfase.

    No hace falta creerse una relación medida en unas cuantas planas: el
    propio derivado la declara. Cada OBJECT del DjVu XML lleva su índice
    de hoja en PARAM name="PAGE" («..._0475.djvu»), contando desde cero,
    y ese índice es el `scan_page` con que se lee el OCR. El PDF contiene
    esas mismas hojas numeradas desde uno.

    Así que la comprobación es doble y es estructural: que el índice
    declarado coincida con el orden de lectura en TODAS las hojas, y que
    los dos derivados tengan el mismo número de hojas. Si las dos cosas
    se cumplen, ninguna hoja se añadió ni se perdió entre uno y otro, y
    la relación vale en todo el tomo y no sólo donde se miró.
    """
    leaves, mismatches = 0, []
    with open(xml_path, "rb") as handle:
        tail = b""
        for block in iter(lambda: handle.read(chunk), b""):
            data = tail + block
            for match in _LEAF_RE.finditer(data):
                declared = int(match.group(1))
                if declared != leaves:
                    mismatches.append((leaves, declared))
                leaves += 1
            tail = data[-200:]

    with open(pdf_path, "rb") as handle:
        counts = [int(m.group(1)) for m in _PDF_COUNT_RE.finditer(handle.read())]
    pdf_pages = max(counts) if counts else 0

    problems = []
    if mismatches:
        problems.append(f"{len(mismatches)} leaves declare an index that is "
                        f"not their reading order, first {mismatches[0]}")
    if leaves != pdf_pages:
        problems.append(f"the DjVu XML declares {leaves} leaves and the PDF "
                        f"holds {pdf_pages}: the two are not the same "
                        f"sequence and the offset cannot be an invariant")
    return {
        "relation": "pdf_page = scan_page + 1",
        "declared_leaves": leaves,
        "pdf_pages": pdf_pages,
        "declared_index_matches_reading_order": not mismatches,
        "same_leaf_sequence": leaves == pdf_pages and leaves > 0,
        "invariant": not problems and leaves > 0,
        "problems": problems,
    }


def boundaries_by_page(reviews) -> Dict[int, List[ChapterImageReview]]:
    """Sólo las que crean frontera, indexadas por plana."""
    out: Dict[int, List[ChapterImageReview]] = {}
    for review in reviews:
        if review.creates_boundary:
            out.setdefault(review.scan_page, []).append(review)
    return out
