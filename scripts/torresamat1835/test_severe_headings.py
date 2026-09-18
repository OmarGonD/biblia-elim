"""
TORRES-1835-SEVERELY-CORRUPTED-HEADINGS-120: buscar por la composición.

    python3 test_severe_headings.py

Sin red y sin el testigo. Cuando el reconocimiento destroza la palabra de
división Y el numeral a la vez («s A X *<>», «ÍAlLMO€xVí.»), no queda
texto que buscar. Lo que queda es cómo está compuesta la plana: una fila
estrecha, centrada sobre el canal, con blanco encima y el argumento ancho
del editor debajo.

La regla de la tanda parte en dos lo que se comprueba aquí:

    GEOMETRY / STRUCTURE DISCOVERS   el barrido puede equivocarse y
                                     ofrecer preliminares, notas al pie o
                                     colas de columna: son preguntas.

    FACSIMILE CONFIRMS               sólo la metadata revisada crea
                                     capítulos, y cada una de sus
                                     entradas dice qué se vio.

Y una consecuencia que esta tanda hereda de la 119 y aplica desde el
principio: DESCUBRIR NO ES ESTAR PENDIENTE. Un candidato mirado se queda
en la historia --con lo que se vio-- y sale de la lista de trabajo, lo
mismo si resultó ser un rótulo que si resultó ser la portada.
"""
import ast
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import image_reviews as ir
import recovery
import severe_headings as sh
import source_ocr
from image_reviews import NO_BOUNDARY
from layout import split_columns

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
METADATA = os.path.join(ROOT, "data", "torresamat1835",
                        "chapter_image_reviews.json")
GUTTER = 1690
PAGE_WIDTH = 3402
PAGE_HEIGHT = 4837
SHA = "f" * 64

SOURCE = recovery.SourceIdentity(
    witness="fixture-witness", filename="witness.pdf", sha256=SHA,
    page_count=40, page_mapping="pdf_page = scan_page + 1")

#: Un rótulo de esta edición: estrecho y centrado sobre el canal.
HEADING_BBOX = (1417, 1000, 2031, 1063)
#: Y el argumento del editor debajo: ancho, cruzando el canal.
ARGUMENT_BBOX = (411, 1200, 3029, 1295)


def _page(rows, *, scan_page=10, width=PAGE_WIDTH, height=PAGE_HEIGHT):
    fixture = {"pages": [{"scan_page": scan_page, "width": width,
                          "height": height,
                          "lines": [{"text": text, "bbox": list(bbox)}
                                    for text, bbox in rows]}]}
    return next(source_ocr.pages_from_fixture(fixture))


def _body(y, *, left="1 Verbum latinum psalmi.",
          right="1 Verso castellano del salmo."):
    """Un renglón de cuerpo a dos columnas, a la altura pedida."""
    return [(left, (400, y, 1600, y + 72)),
            (right, (1700, y, 3000, y + 72))]


def _volume_page(*, heading=HEADING_BBOX, heading_text="s A X *<>",
                 argument=ARGUMENT_BBOX, scan_page=10, extra=()):
    """Una plana con un rótulo destrozado y su argumento debajo."""
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    rows += _body(700)
    rows += _body(790)
    rows += _body(880)
    if heading is not None:
        rows.append((heading_text, heading))
    if argument is not None:
        rows.append(("Argumento del editor sobre este salmo.", argument))
    rows += _body(1400)
    rows += _body(1490)
    rows += list(extra)
    return _page(rows, scan_page=scan_page)


def _inspect(page, **kw):
    return sh.inspect_page(page, split_columns(page, gutter_hint=GUTTER), **kw)


def _candidate(page, **kw):
    found, _why = _inspect(page, **kw)
    return found


def _metadata():
    with open(METADATA, encoding="utf-8") as handle:
        return json.load(handle)


def _batch120(data=None):
    data = data or _metadata()
    return [r for r in data["reviews"] if r.get("batch") == "batch-120"]


# ======================================================================
# A-C. La FILA, que es lo que el impreso compuso
# ======================================================================
def test_A_a_printed_line_split_by_the_gutter_is_one_row():
    # el reconocimiento partió la línea en dos bloques a un lado y otro
    page = _page([("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
                 + _body(700) + _body(790)
                 + [("SALMO", (1417, 1000, 1690, 1063)),
                    ("XXXIV.", (1720, 1004, 2031, 1067))]
                 + [("Argumento del editor.", ARGUMENT_BBOX)])
    rows = sh.rows_of(page, split_columns(page, gutter_hint=GUTTER))
    wide = [r for r in rows if len(r.block_ids) > 1]
    assert len(wide) == 3, "los pares de columna y el rótulo partido"
    heading = [r for r in rows if "SALMO" in r.raw]
    assert len(heading) == 1, "la línea impresa vuelve a ser una sola fila"
    assert heading[0].bbox == (1417, 1000, 2031, 1067)
    assert heading[0].raw_texts == ["SALMO", "XXXIV."], "el crudo no se toca"
    assert heading[0].raw == "SALMO | XXXIV.", "se junta la medida, no el texto"


def test_B_rows_far_apart_stay_separate():
    page = _volume_page()
    rows = sh.rows_of(page, split_columns(page, gutter_hint=GUTTER))
    tops = [row.bbox[1] for row in rows]
    assert tops == sorted(tops), "las filas salen en orden de lectura"
    assert len(rows) == len(set(tops)), "una fila por línea impresa"


def test_C_the_running_header_is_not_a_row_and_is_counted():
    page = _volume_page()
    rows = sh.rows_of(page, split_columns(page, gutter_hint=GUTTER))
    assert not any("LIBRO DE LOS SALMOS" in row.raw for row in rows)
    _found, why = _inspect(page)
    assert why.get(sh.NOT_BODY) == 1, "la cabecera se descarta y se cuenta"


# ======================================================================
# D-F. Lo que se ofrece es una PREGUNTA
# ======================================================================
def test_D_a_narrow_centred_row_over_an_argument_is_a_candidate():
    found = _candidate(_volume_page())
    assert len(found) == 1
    assert found[0].block_ids == ["p0010l0007"]
    assert found[0].scan_page == 10 and found[0].pdf_page == 11


def test_E_the_raw_text_travels_whole_and_untouched():
    found = _candidate(_volume_page(heading_text="$'AtMÓ LXirill"))
    assert found[0].raw_text == "$'AtMÓ LXirill", "ni se corrige ni se limpia"


def test_F_a_candidate_asserts_nothing_about_the_chapter():
    found = _candidate(_volume_page())
    fields = set(found[0].as_dict())
    assert "chapter_number" not in fields
    assert "confirmed" not in fields
    assert not hasattr(found[0], "resolves")
    assert found[0].evidence, "sólo medidas, y dichas"


# ======================================================================
# G-K. Por qué una fila NO llega a pregunta
# ======================================================================
def test_G_a_row_as_wide_as_running_text_is_rejected():
    _found, why = _inspect(_volume_page(heading=(300, 1000, 3100, 1063)))
    assert why.get(sh.TOO_WIDE), "un párrafo no es un rótulo"


def test_H_a_row_that_is_not_centred_is_rejected():
    _found, why = _inspect(_volume_page(heading=(400, 1000, 1014, 1063)))
    assert why.get(sh.OFF_CENTRE), "el final de una columna cae lejos del centro"


def test_I_a_row_glued_to_the_line_above_is_rejected():
    # pegada al renglón de cuerpo anterior: no hay blanco de rótulo
    _found, why = _inspect(_volume_page(heading=(1417, 955, 2031, 1018)))
    assert why.get(sh.NO_GAP)


def test_J_a_row_with_too_many_words_is_rejected():
    long_text = " ".join(["palabra"] * (sh.MAX_TOKENS + 1))
    _found, why = _inspect(_volume_page(heading_text=long_text))
    assert why.get(sh.TOO_MANY_TOKENS)


def test_K_a_narrow_row_with_nothing_wide_below_is_rejected():
    # ni argumento del editor ni renglón de cuerpo: sólo notas estrechas
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    rows += _body(700) + _body(790) + _body(880)
    rows += [("s A X *<>", HEADING_BBOX)]
    for y in (1200, 1290, 1380):
        rows.append(("1 nota al pie.", (400, y, 900, y + 72)))
    _found, why = _inspect(_page(rows))
    assert why.get(sh.NO_ARGUMENT), "sin nada ancho debajo no se ofrece"


# ======================================================================
# L-O. Cómo se mide
# ======================================================================
def test_L_the_gap_is_measured_in_line_pitches_not_pixels():
    # la misma plana, comprimida: cambian los píxeles, no el veredicto
    dense = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    for step, y in enumerate((700, 736, 772)):
        dense += [("1 Verbum latinum.", (400, y, 1600, y + 30)),
                  ("1 Verso castellano.", (1700, y, 3000, y + 30))]
    dense += [("s A X *<>", (1417, 830, 2031, 860)),
              ("Argumento del editor.", (411, 900, 3029, 940)),
              ("2 Verbum.", (400, 950, 1600, 980)),
              ("2 Verso.", (1700, 950, 3000, 980))]
    found = _candidate(_page(dense))
    assert len(found) == 1, "la densidad de la plana no cambia el veredicto"
    assert found[0].gap_before > sh.MIN_GAP_BEFORE


def test_M_the_argument_may_be_a_few_rows_below():
    # el reconocimiento mete basura entre el rótulo y el argumento
    junk = [("·  .", (1500, 1100, 1900, 1140))]
    assert _candidate(_volume_page())[0].argument_rows_below == 1
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    rows += _body(700) + _body(790) + _body(880)
    rows += [("s A X *<>", HEADING_BBOX)] + junk
    rows += [("Argumento del editor.", ARGUMENT_BBOX)] + _body(1400) + _body(1490)
    found = _candidate(_page(rows))
    heading = next(c for c in found if c.raw_text == "s A X *<>")
    assert heading.argument_rows_below == 2, "la basura de por medio no estorba"


def test_N_an_argument_further_than_the_window_does_not_count():
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    rows += _body(700) + _body(790) + _body(880)
    rows += [("s A X *<>", HEADING_BBOX)]
    for y in (1100, 1130, 1160, 1190):
        rows.append(("·  .", (1500, y, 1900, y + 25)))
    rows += [("Argumento del editor.", (411, 1300, 3029, 1395))]
    rows += _body(1500)
    _found, why = _inspect(_page(rows))
    assert why.get(sh.NO_ARGUMENT), \
        f"el argumento cae a más de {sh.ARGUMENT_WITHIN} filas"


def test_O_a_spanning_row_over_two_columns_of_text_also_counts():
    # Segundo camino: la edición no siempre pone argumento --el salmo 127
    # de la plana 183 no lo lleva--. Entonces lo que hay debajo del
    # rótulo son las dos columnas del texto, anchas entre las dos aunque
    # ningún bloque cruce el canal por sí solo.
    found = _candidate(_volume_page(argument=None))
    assert len(found) == 1
    assert found[0].argument_rows_below is None, "no hay argumento que ver"
    assert found[0].spanning, "pero el rótulo sí cruza el canal"
    assert found[0].next_width_ratio >= sh.ARGUMENT_WIDTH


# ======================================================================
# P-S. El orden de la cola
# ======================================================================
def test_P_a_narrow_centred_row_over_its_argument_ranks_high():
    found = _candidate(_volume_page())
    assert found[0].rank == sh.HIGH
    assert found[0].width_ratio <= 0.35 and found[0].off_centre <= 0.05


def test_Q_a_wider_or_less_centred_row_ranks_medium():
    found = _candidate(_volume_page(heading=(1100, 1000, 2400, 1063)))
    assert found[0].width_ratio > 0.35
    assert found[0].rank == sh.MEDIUM


def test_R_a_row_that_does_not_cross_the_gutter_ranks_low():
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
    rows += _body(700) + _body(790) + _body(880)
    rows += [("tísimo.", (1500, 1000, 1660, 1063)),
             ("Argumento del editor sobre este salmo.", ARGUMENT_BBOX)]
    rows += _body(1400)
    found = _candidate(_page(rows))
    assert len(found) == 1 and found[0].rank == sh.LOW, \
        "una cola de columna se ofrece, pero la última"


def test_S_a_page_with_almost_no_body_offers_nothing():
    rows = [("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255)),
            ("PROFECÍAS.", (1417, 1000, 2031, 1063)),
            ("Argumento.", ARGUMENT_BBOX)]
    found, _why = _inspect(_page(rows))
    assert found == [], "una portadilla no tiene cuerpo que medir"


# ======================================================================
# T-X. El barrido completo
# ======================================================================
def test_T_a_represented_row_is_labelled_and_not_dropped():
    known = {"p0010l0007": "ps-40-p10"}
    found = _candidate(_volume_page(), represented=known)
    assert len(found) == 1, "lo ya representado sigue saliendo del barrido"
    assert found[0].represented_by == "ps-40-p10"
    assert any("already represented" in e for e in found[0].evidence)


def test_U_the_queue_is_ordered_by_rank_then_page_then_block():
    high = _volume_page(scan_page=12)
    low = _page([("LIBRO DE LOS SALMOS. 56", (1697, 195, 2117, 255))]
                + _body(700) + _body(790) + _body(880)
                + [("tísimo.", (1500, 1000, 1660, 1063)),
                   ("Argumento del editor.", ARGUMENT_BBOX)]
                + _body(1400), scan_page=11)
    pages = [(low, split_columns(low, gutter_hint=GUTTER)),
             (high, split_columns(high, gutter_hint=GUTTER))]
    found, _why = sh.scan(pages)
    assert [c.rank for c in found] == [sh.HIGH, sh.LOW]
    assert [c.scan_page for c in found] == [12, 11], "el rango manda sobre la plana"


def test_V_the_same_input_gives_the_same_queue():
    def pages():
        page = _volume_page()
        return [(page, split_columns(page, gutter_hint=GUTTER))]
    first, why_first = sh.scan(pages())
    second, why_second = sh.scan(pages())
    assert [c.as_dict() for c in first] == [c.as_dict() for c in second]
    assert why_first == why_second


def test_W_the_summary_counts_what_the_queue_holds():
    page = _volume_page()
    found, _why = sh.scan([(page, split_columns(page, gutter_hint=GUTTER))],
                          book_at=lambda _p: "Ps",
                          represented={"p0010l0007": "ps-40-p10"})
    out = sh.summary(found)
    assert out["total"] == 1 and out["by_rank"] == {sh.HIGH: 1}
    assert out["by_book"] == {"Ps": 1} and out["already_represented"] == 1


def test_X_every_discarded_row_is_counted_by_reason():
    _found, why = _inspect(_volume_page(heading=(300, 1000, 3100, 1063)))
    assert sum(why.values()) >= 1
    assert set(why) <= {sh.NOT_BODY, sh.TOO_WIDE, sh.OFF_CENTRE, sh.NO_GAP,
                        sh.TOO_MANY_TOKENS, sh.NO_ARGUMENT}


# ======================================================================
# Y-Z. Lo que el barrido NO puede hacer
# ======================================================================
def test_Y_the_numeral_is_not_a_gate():
    # 118 exigía numeral y por eso se le escaparon los trece de 119
    found = _candidate(_volume_page(heading_text="s A X *<>"))
    assert len(found) == 1, "una fila sin un solo dígito puede ser candidata"
    assert not any(ch.isdigit() for ch in found[0].raw_text)


def test_Z_the_scan_names_no_page_and_no_chapter_number():
    with open(os.path.join(DIR, "severe_headings.py"), encoding="utf-8") as fh:
        tree = ast.parse(fh.read())
    pages = {5, 27, 41, 42, 62, 109, 124, 166, 181, 183, 185, 205, 209, 319}
    for node in ast.walk(tree):
        if isinstance(node, ast.Compare):
            operands = [node.left] + list(node.comparators)
            for operand in operands:
                if isinstance(operand, ast.Constant) and \
                        isinstance(operand.value, int) and \
                        not isinstance(operand.value, bool):
                    assert operand.value not in pages, \
                        f"decide por la plana {operand.value}"
    texts = [n.value for n in ast.walk(tree)
             if isinstance(n, ast.Constant) and isinstance(n.value, str)]
    for marker in ("p0041", "p0109", "p0166", "p0319", "SALMO", "CAPITULO"):
        assert not any(marker in t for t in texts), \
            f"{marker}: el barrido no puede leer el texto que busca"


# ======================================================================
# AA-AG. La metadata: lo que el facsímil confirmó y lo que desmintió
# ======================================================================
def test_AA_the_batch_holds_every_candidate_that_was_looked_at():
    entries = _batch120()
    outcomes = {}
    for entry in entries:
        outcomes[entry["review_outcome"]] = outcomes.get(
            entry["review_outcome"], 0) + 1
    assert outcomes == {"confirmed_severely_corrupted_heading": 10,
                        "front_matter_not_a_division": 24,
                        "body_text_not_a_division": 16}, outcomes
    assert all(e["discovered_by"] == "severely_corrupted_heading"
               for e in entries)


def test_AB_a_confirmed_heading_marks_the_block_that_already_exists():
    for entry in _batch120():
        if entry["review_outcome"] != "confirmed_severely_corrupted_heading":
            continue
        assert entry["heading_block"], entry["id"]
        # las anclas quedan como contexto comprobable: el rótulo que se
        # marca tiene que caer entre ellas, y nunca fuera.
        after, before = entry["insert_after_block"], entry["insert_before_block"]
        assert after < entry["heading_block"] < before, entry["id"]
        assert entry["boundary_confirmed"] and entry["numeral_confirmed"]
        assert isinstance(entry["chapter_number"], int)
        assert entry["observed_printed_text"], "se dice qué pone la plana"


def test_AC_every_heading_block_sits_on_its_own_page():
    for entry in _batch120():
        block = entry.get("heading_block") or entry.get("candidate_block")
        assert block, entry["id"]
        assert int(block[1:5]) == entry["scan_page"], entry["id"]
        assert block[5] == "l", "un bloque del reconocimiento, no un derivado"


def test_AD_a_rejection_names_the_candidate_it_answers():
    rejected = [e for e in _batch120() if e["outcome"] == NO_BOUNDARY]
    assert len(rejected) == 40
    for entry in rejected:
        assert entry["candidate_block"], entry["id"]
        assert not entry.get("heading_block"), "un rechazo no nombra rótulo"
        assert entry["observed_context"], "se dice qué se vio en la banda"


def test_AE_a_rejection_confirms_nothing():
    for entry in _batch120():
        if entry["outcome"] != NO_BOUNDARY:
            continue
        assert entry["boundary_confirmed"] is False
        assert entry["numeral_confirmed"] is False
        assert entry["chapter_number"] is None
        assert entry["insert_after_block"] is None
        assert entry["insert_before_block"] is None


def test_AF_no_block_is_answered_twice():
    blocks = []
    for entry in _metadata()["reviews"]:
        block = entry.get("heading_block") or entry.get("candidate_block")
        if block:
            blocks.append(block)
    assert len(blocks) == len(set(blocks)), "un bloque, una respuesta"
    ids = [e["id"] for e in _metadata()["reviews"]]
    assert len(ids) == len(set(ids))


def test_AG_the_shipped_metadata_validates():
    assert ir.validate(_metadata(), page_count=652) == []


# ======================================================================
# AH-AI. El campo nuevo, y lo que esta tanda deja sin hacer
# ======================================================================
def test_AH_a_candidate_block_is_checked_like_any_other_block():
    base = {"id": "fx", "book": "Ps", "scan_page": 10, "outcome": NO_BOUNDARY,
            "chapter_number": None, "boundary_confirmed": False,
            "numeral_confirmed": False, "insert_after_block": None,
            "insert_before_block": None, "rationale": "porque sí"}
    source = {"sha256": "a" * 64}

    def problems(**over):
        review = dict(base)
        review.update(over)
        return ir.validate({"visual_source": source, "reviews": [review]},
                           page_count=652)

    assert problems(candidate_block="p0010l0007") == []
    assert any("not on scan page" in p
               for p in problems(candidate_block="p0011l0007"))
    assert any("not an OCR block id" in p
               for p in problems(candidate_block="p0010r0007"))
    assert any("already names its block" in p
               for p in problems(candidate_block="p0010l0007",
                                 heading_block="p0010l0007"))


def test_AI_a_rejection_changes_nothing_in_the_stream():
    page = _volume_page()
    entries = split_columns(page, gutter_hint=GUTTER)
    review = ir.ChapterImageReview(
        id="fx-reject", book="Ps", scan_page=10, outcome=NO_BOUNDARY,
        chapter_number=None, boundary_confirmed=False, numeral_confirmed=False,
        insert_after_block=None, insert_before_block=None,
        observed_printed_text=None, crop_bbox=(0, 850, 3402, 1483),
        confidence=0.97, rationale="la plana no imprime ahí ningún rótulo",
        reviewer_method="render", pdf_page=11,
        candidate_block="p0010l0007")
    out, records = recovery.apply_verified_image_reviews(
        page, entries, [review], source=SOURCE, book="Ps")
    assert [r.action for r in records] == [recovery.REJECTED]
    assert len(out) == len(entries), "no añade ni quita renglones"
    assert all(getattr(e, "recovery", None) is None for e in out)


def test_AJ_the_ordinal_heading_was_closed_by_the_next_round():
    # Sab p319 imprime «CAPÍTULO PRIMERO»: rótulo de verdad, con el
    # número escrito en palabra. Esta tanda lo ofreció y lo dejó
    # pendiente a propósito --leer ordinales no era suyo--; la 121 lo
    # contestó. Lo que este barrido tiene que seguir garantizando es lo
    # de siempre: que la fila que encontró no se cierre en falso desde
    # aquí, sino con una revisión del facsímil que diga qué se vio.
    answered = {}
    for entry in _metadata()["reviews"]:
        block = entry.get("heading_block") or entry.get("candidate_block")
        if block:
            answered[block] = entry
    entry = answered.get("p0319l0002")
    assert entry is not None, "la fila sigue sin contestar"
    assert entry["discovered_by"] == "written_ordinal"
    assert entry["review_outcome"] == "resolved_written_ordinal"
    assert entry["observed_printed_ordinal"] == "PRIMERO"
    # y ninguna revisión de ESTE barrido se la atribuye
    ours = [e for e in _batch120()
            if (e.get("heading_block") or e.get("candidate_block")) == "p0319l0002"]
    assert ours == []


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
    print(f"torresamat1835_severe_headings_failures={failures}")
    sys.exit(1 if failures else 0)
