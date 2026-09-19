"""Recuperación conservadora de numerales compuestos ``a *``.

Este módulo sólo lee medidas PRECOMPUTADAS. No abre el facsímil, no importa
el código que procesa imágenes y no conoce huecos ni vecinos canónicos.
"""
from dataclasses import dataclass
import json
import math
import os
import time
from typing import Dict, Optional, Sequence, Tuple


SCHEMA_VERSION = 2
PREPROCESSING_SCHEMA = "binary-ink-component-v1"
SOURCE_VOLUME = "3"
SOURCE_WITNESS = "ia-lasagradabiblia01unkngoog"
SOURCE_OCR_SHA256 = (
    "545fe8dcedac680271334613f3ae9c3253bd57ed80610868a7e13fdfb78ada5f")
FACSIMILE_SHA256 = (
    "cb9cf759ff77d0a7822efeba5bf62734544cee9681736bd00085a1a0b2384346")
CROP_DPI = 600
CROP_PAD = 14

# Extremos observados, sin inversión, en la población etiquetada de la 130.
W1_MAX = 24
W2_MIN = 31
A1_MAX = 2 / 3
A2_MIN = 0.825
P1_MAX = 640
P2_MIN = 744

PRINTED_1 = "PRINTED_1"
PRINTED_2 = "PRINTED_2"
ABSTAIN = "ABSTAIN"
INVALID_EVIDENCE = "INVALID_EVIDENCE"
MISSING_EVIDENCE = "MISSING_EVIDENCE"

UNSAFE_SECOND_TOKEN = "unsafe_second_token"


@dataclass(frozen=True)
class PixelEvidence:
    block: str
    scan_page: int
    form: str
    column: str
    zone: str
    crop_bbox_scan: Tuple[int, int, int, int]
    crop_dpi: int
    threshold: int
    ink_width: int
    ink_aspect: float
    ink_pixels: int


def classify_a_first_digit(evidence: PixelEvidence) -> str:
    """Clasifica sólo la tinta; no recibe libro, capítulo ni secuencia."""
    if not isinstance(evidence, PixelEvidence):
        return INVALID_EVIDENCE
    width, aspect, pixels = (evidence.ink_width, evidence.ink_aspect,
                             evidence.ink_pixels)
    if (isinstance(width, bool) or not isinstance(width, int) or width <= 0
            or isinstance(pixels, bool) or not isinstance(pixels, int)
            or pixels <= 0 or not isinstance(aspect, (int, float))
            or isinstance(aspect, bool) or not math.isfinite(aspect)
            or aspect <= 0):
        return INVALID_EVIDENCE
    if width <= W1_MAX and aspect <= A1_MAX:
        # ``ink_pixels`` no añade una tercera condición de aceptación, pero
        # una contradicción física fuerte obliga a abstenerse.
        return ABSTAIN if pixels >= P2_MIN else PRINTED_1
    if width >= W2_MIN and aspect >= A2_MIN:
        return ABSTAIN if pixels <= P1_MAX else PRINTED_2
    return ABSTAIN


def second_digit(token: str) -> Optional[int]:
    """Semántica local de la SEGUNDA posición, independiente de la primera."""
    if len(token) == 1 and "0" <= token <= "9":
        return ord(token) - ord("0")
    if token == "o":
        return 0
    if token == "a":
        return 2
    return None


def crop_box(first_bbox: Sequence[int], second_bbox: Sequence[int]) \
        -> Tuple[int, int, int, int]:
    """Reproduce la caja declarada, sin importar el módulo de píxeles."""
    x0 = first_bbox[0] - CROP_PAD
    y0 = first_bbox[1] - CROP_PAD
    y1 = first_bbox[3] + CROP_PAD
    halfway = (first_bbox[2] + second_bbox[0]) // 2
    x1 = min(first_bbox[2] + CROP_PAD, halfway)
    if x1 <= x0:
        x1 = first_bbox[2]
    return x0, y0, x1, y1


class EvidenceIndex:
    """Índice inmutable block_id -> evidencia, validado al cargar."""

    def __init__(self, rows=None, *, valid=True, error=None, load_seconds=0.0,
                 path=None):
        self.rows: Dict[str, PixelEvidence] = rows or {}
        self.valid = valid
        self.error = error
        self.load_seconds = load_seconds
        self.path = path

    @classmethod
    def invalid(cls, reason: str):
        return cls(valid=False, error=reason)

    @classmethod
    def load(cls, path: str, *, volume=SOURCE_VOLUME,
             witness=SOURCE_WITNESS, ocr_sha256=SOURCE_OCR_SHA256,
             facsimile_sha256=FACSIMILE_SHA256):
        started = time.perf_counter()
        try:
            with open(path, encoding="utf-8") as handle:
                payload = json.load(handle)
        except (OSError, ValueError) as exc:
            return cls.invalid(f"artifact_unreadable:{exc}")
        if payload.get("schema_version") != SCHEMA_VERSION:
            return cls.invalid("unsupported_artifact_schema")
        source = payload.get("source_ocr") or {}
        expected_source = {"volume": str(volume), "witness": witness,
                           "sha256": ocr_sha256}
        if any(source.get(key) != value for key, value in expected_source.items()):
            return cls.invalid("source_provenance_mismatch")
        facsimile = payload.get("facsimile") or {}
        if facsimile.get("sha256") != facsimile_sha256:
            return cls.invalid("facsimile_provenance_mismatch")
        preprocessing = payload.get("preprocessing") or {}
        if preprocessing.get("schema") != PREPROCESSING_SCHEMA:
            return cls.invalid("unsupported_preprocessing_schema")
        rows = {}
        try:
            for raw in payload.get("instances", []):
                features = raw["features"]
                evidence = PixelEvidence(
                    block=raw["block"], scan_page=raw["scan_page"],
                    form=raw["form"], column=raw["column"], zone=raw["zone"],
                    crop_bbox_scan=tuple(raw["crop_bbox_scan"]),
                    crop_dpi=raw["crop_dpi"], threshold=raw["otsu_threshold"],
                    ink_width=features["ink_bbox_width"],
                    ink_aspect=features["ink_bbox_aspect_ratio"],
                    ink_pixels=features["ink_pixels"])
                if evidence.block in rows:
                    return cls.invalid("duplicate_block_evidence")
                if (len(evidence.crop_bbox_scan) != 4
                        or evidence.crop_dpi != CROP_DPI
                        or not isinstance(evidence.threshold, int)):
                    return cls.invalid("invalid_feature_row")
                if classify_a_first_digit(evidence) == INVALID_EVIDENCE:
                    return cls.invalid("invalid_feature_row")
                rows[evidence.block] = evidence
        except (KeyError, TypeError, ValueError):
            return cls.invalid("incomplete_feature_row")
        if not rows:
            return cls.invalid("empty_feature_artifact")
        return cls(rows, load_seconds=time.perf_counter() - started, path=path)

    def lookup(self, block_id: str, *, scan_page: int, form: str,
               column: str, zone: str, first_bbox, second_bbox):
        if not self.valid:
            return None, INVALID_EVIDENCE, self.error
        evidence = self.rows.get(block_id)
        if evidence is None:
            return None, MISSING_EVIDENCE, "missing_block_evidence"
        expected_crop = crop_box(first_bbox, second_bbox)
        actual = (evidence.scan_page, evidence.form, evidence.column,
                  evidence.zone, evidence.crop_bbox_scan)
        expected = (scan_page, form, column, zone, expected_crop)
        if actual != expected:
            return None, INVALID_EVIDENCE, "row_source_identity_mismatch"
        return evidence, classify_a_first_digit(evidence), None


_DEFAULT_INDEX = None


def default_index() -> EvidenceIndex:
    global _DEFAULT_INDEX
    if _DEFAULT_INDEX is None:
        root = os.path.dirname(os.path.dirname(os.path.dirname(
            os.path.abspath(__file__))))
        path = os.path.join(root, "data", "torresamat1835",
                            "a_glyph_pixel_features.json")
        _DEFAULT_INDEX = EvidenceIndex.load(path)
    return _DEFAULT_INDEX
