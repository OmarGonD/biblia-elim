"""
TORRES-1835-IMAGE-CHAPTER-RECOVERY-109: recuperar divisiones del facsímil.

    python3 test_image_reviews.py

Sin red. No abre el PDF salvo para la guarda de hash, y esa parte se salta
si el artefacto no está en la caché.
"""
import copy
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import image_reviews as ir
from image_reviews import (BOUNDARY_AND_NUMBER, BOUNDARY_ONLY, NO_BOUNDARY,
                           ReviewError)

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
PDF = os.path.join(ROOT, "build", "torresamat1835-cache",
                   "lasagradabiblia01unkngoog.pdf")
SHA = "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346"


def data():
    return ir.load()


def one(outcome, **over):
    base = {
        "id": "x", "book": "Sir", "scan_page": 10, "outcome": outcome,
        "chapter_number": 46 if outcome == BOUNDARY_AND_NUMBER else None,
        "boundary_confirmed": outcome != NO_BOUNDARY,
        "numeral_confirmed": outcome == BOUNDARY_AND_NUMBER,
        "insert_after_block": None if outcome == NO_BOUNDARY else "p0010l0001",
        "insert_before_block": None if outcome == NO_BOUNDARY else "p0010l0009",
        "observed_printed_text": None if outcome == NO_BOUNDARY else "CAPITULO XLVI",
        "crop_bbox": [0, 0, 3402, 2660], "confidence": 0.9,
        "rationale": "read from the facsimile", "reviewer_method": "render",
    }
    base.update(over)
    return {"schema_version": 1, "edition_id": "TorresAmat1835", "volume": 3,
            "visual_source": {"sha256": SHA, "may_produce_release_artifact": False},
            "reviews": [base]}


# ---- A/B. Guarda de hash ------------------------------------------------
def test_matching_hash_applies_the_reviews():
    reviews = ir.reviews_for(data(), expected_sha256=SHA)
    assert reviews
    assert any(r.creates_boundary for r in reviews)


def test_mismatched_hash_refuses_everything():
    for bad in ("0" * 64, "a" * 64):
        try:
            ir.reviews_for(data(), expected_sha256=bad)
        except ReviewError as exc:
            assert "not applied" in str(exc)
        else:
            raise AssertionError("un artefacto distinto no puede aplicarse")


def test_hash_guard_against_the_real_artifact():
    if not os.path.isfile(PDF):
        print("  (saltado: el PDF no está en la caché)")
        return
    reviews = ir.reviews_for(data(), source_path=PDF)
    assert any(r.id == "sir-46-p475" for r in reviews)


def test_missing_artifact_is_explicit():
    try:
        ir.reviews_for(data(), source_path=os.path.join(DIR, "no-such.pdf"))
    except ReviewError as exc:
        assert "not in the cache" in str(exc)
    else:
        raise AssertionError("sin artefacto no se puede verificar nada")


# ---- C. Mapping de páginas ---------------------------------------------
def test_page_mapping_is_declared_and_deterministic():
    source = data()["visual_source"]
    assert source["page_mapping"] == "pdf_page = scan_page + 1"
    assert len(source["page_mapping_evidence"]) >= 2
    for review in data()["reviews"]:
        if review.get("pdf_page") is not None:
            assert review["pdf_page"] == review["scan_page"] + 1, review["id"]


# ---- D/E. Los tres desenlaces ------------------------------------------
def test_boundary_with_known_number():
    review = ir.reviews_for(one(BOUNDARY_AND_NUMBER), expected_sha256=SHA)[0]
    assert review.creates_boundary
    assert review.chapter_number == 46
    assert review.review_required is False


def test_boundary_with_unknown_number_stays_under_review():
    review = ir.reviews_for(one(BOUNDARY_ONLY), expected_sha256=SHA)[0]
    assert review.creates_boundary
    assert review.chapter_number is None
    assert review.review_required is True


def test_number_cannot_be_invented_for_an_illegible_numeral():
    problems = ir.validate(one(BOUNDARY_ONLY, chapter_number=46))
    assert any("number must stay unknown" in p for p in problems)


# ---- F/G. Candidato rechazado ------------------------------------------
def test_rejected_candidate_creates_nothing_but_keeps_its_rationale():
    review = ir.reviews_for(one(NO_BOUNDARY), expected_sha256=SHA)[0]
    assert review.creates_boundary is False
    assert review.rationale
    assert ir.boundaries_by_page([review]) == {}
    # y no puede llevar ancla de inserción
    problems = ir.validate(one(NO_BOUNDARY, insert_before_block="p0010l0009"))
    assert any("no insertion anchor" in p for p in problems)


def test_the_real_rejection_is_recorded():
    """El hueco de once planas en Salmos era el Salmo 118, no capítulos
    perdidos. Queda registrado para no volver a revisarlo."""
    rejected = [r for r in ir.reviews_for(data(), expected_sha256=SHA)
                if r.outcome == NO_BOUNDARY]
    assert rejected
    assert rejected[0].book == "Ps"
    assert rejected[0].chapter_number is None
    assert "176 verses" in rejected[0].rationale


# ---- Validación del esquema --------------------------------------------
def test_schema_validation_catches_bad_metadata():
    checks = (
        (one(BOUNDARY_AND_NUMBER, chapter_number=None), "not an integer"),
        (one(BOUNDARY_AND_NUMBER, rationale=""), "empty rationale"),
        (one(BOUNDARY_AND_NUMBER, scan_page=-1), "bad scan_page"),
        (one(BOUNDARY_AND_NUMBER), "unknown outcome"),
        (one(BOUNDARY_AND_NUMBER, crop_bbox=[10, 10, 5, 5]), "malformed crop_bbox"),
    )
    for payload, expected in checks:
        if expected == "unknown outcome":
            payload["reviews"][0]["outcome"] = "whatever"
        problems = ir.validate(payload, page_count=652)
        assert any(expected in p for p in problems), expected


def test_duplicate_ids_are_caught():
    payload = one(BOUNDARY_AND_NUMBER)
    payload["reviews"].append(copy.deepcopy(payload["reviews"][0]))
    assert any("duplicated id" in p for p in ir.validate(payload))


def test_bbox_must_fall_inside_its_page():
    payload = one(BOUNDARY_AND_NUMBER, crop_bbox=[0, 0, 9999, 9999])
    problems = ir.validate(payload, page_bounds={10: (3402, 4837)})
    assert any("outside page" in p for p in problems)


def test_scan_page_beyond_the_witness_is_caught():
    assert any("beyond the witness" in p
               for p in ir.validate(one(BOUNDARY_AND_NUMBER, scan_page=700),
                                    page_count=652))


def test_shipped_metadata_validates():
    assert ir.validate(data(), page_count=652) == []


def test_visual_source_cannot_be_release_capable():
    payload = one(BOUNDARY_AND_NUMBER)
    payload["visual_source"]["may_produce_release_artifact"] = True
    assert any("release-capable" in p for p in ir.validate(payload))


# ---- Nada de canon ni de secuencia -------------------------------------
def test_neither_canon_nor_sequence_participate():
    # Se mira lo estructural -- qué importa y qué calcula --, no la
    # prosa: el docstring menciona el canon justamente para decir que no
    # participa, y prohibir esa frase sería el mismo error que evita.
    source = open(os.path.join(DIR, "image_reviews.py"), encoding="utf-8").read()
    code = "\n".join(l for l in source.splitlines()
                     if not l.lstrip().startswith("#"))
    body = code.split('"""', 2)[-1]          # fuera el docstring del módulo
    for token in ("import canon", "POR_OSIS", "chapter_limit",
                  "VOLUME3_ORDER", "import structure"):
        assert token not in body, token
    # ninguna aritmética sobre números de capítulo
    assert "chapter_number +" not in body
    assert "chapter_number -" not in body
    # la entrada real dice explícitamente que no se usó la secuencia
    entry = next(r for r in data()["reviews"] if r["id"] == "sir-46-p475")
    assert "Sequence was not used" in entry["rationale"]


# ---- Provenance, determinismo, offline ---------------------------------
def test_every_review_keeps_its_provenance():
    for review in ir.reviews_for(data(), expected_sha256=SHA):
        assert review.id and review.book and review.rationale
        assert review.reviewer_method
        assert review.scan_page >= 0
        assert review.confidence > 0
        if review.creates_boundary:
            assert review.insert_after_block and review.insert_before_block
            assert review.observed_printed_text


def test_deterministic_and_offline():
    one_pass = [(r.id, r.outcome, r.chapter_number)
                for r in ir.reviews_for(data(), expected_sha256=SHA)]
    two_pass = [(r.id, r.outcome, r.chapter_number)
                for r in ir.reviews_for(data(), expected_sha256=SHA)]
    assert one_pass == two_pass
    source = open(os.path.join(DIR, "image_reviews.py"), encoding="utf-8").read()
    assert "urllib" not in source and "requests" not in source


if __name__ == "__main__":
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"ok {name}")
            except AssertionError as exc:
                failures += 1
                print(f"FAIL {name}: {exc}")
    print(f"torresamat1835_image_reviews_failures={failures}")
    sys.exit(1 if failures else 0)
