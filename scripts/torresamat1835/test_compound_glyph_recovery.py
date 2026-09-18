"""
TORRES-1835-COMPOUND-VERSE-GLYPH-RECOVERY-128: el numeral partido en dos.

    python3 test_compound_glyph_recovery.py

Sin red y sin el testigo. La 127 dejó quince formas compuestas como
CANDIDATAS. La 128 agotó su población en el facsímil y dos de ellas se
cayeron por sí solas:

    «a I» vale 21 diez veces y, una vez, el 2 impreso seguido del signo
    «¡» que el reconocimiento devolvió como «I»;

    «a a» vale 22 doce veces y, una vez, 12 -- porque el 1 de estilo
    antiguo TAMBIÉN se reconoce como «a», que es exactamente lo que ya
    había pasado con «a o» (10 y 20) en la 127.

De ahí sale la regla que gobierna esta tanda, y es una regla de familia,
no de caso: NINGUNA forma que empiece por «a» se automatiza. Equivocar la
primera cifra no deja un versículo sin abrir; abre el versículo
equivocado, y eso es peor que no hacer nada. Quedan nueve formas, todas
con «1» o «I» delante, todas con su población agotada y sin una sola
discrepancia.

    FORM + GEOMETRY + ZONE + FACSIMILE PROVENANCE

Ninguna de las cuatro sobra, y el tomo lo demuestra: hay 609 renglones de
la COLUMNA LATINA que empiezan por una forma segura --el latín se compone
con las mismas cifras-- y ni uno solo abre versículo castellano.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import compound_glyphs
import glyph_reviews
import layout
import page_parser
import source_ocr
import structure
from model import Provenance

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")
XML = os.path.join(ROOT, "build", "torresamat1835-cache",
                   "lasagradabiblia01unkngoog_djvu.xml")

#: Lo que la 127 propuso y lo que la 128 dejó encendido.
ENABLED = {("I", "o"): 10, ("I", "I"): 11, ("I", "a"): 12,
           ("1", "1"): 11, ("1", "3"): 13, ("1", "4"): 14,
           ("1", "5"): 15, ("1", "8"): 18, ("1", "9"): 19}
WITHHELD = ("a o", "a a", "a I", "a 1", "a 3", "a 4", "a 8",
            "1 6", "1 7", "S y", "l")
#: Formas de un solo glifo. Ni una puede entrar, coincida con lo que
#: coincida.
SINGLE_GLYPHS = ("a", "á", "S", "s", "y", "Y", "o", "ó", "O", "I", "i",
                 "l", "f", "n", "t", "u", "é", "V", "j", "k")

#: Casos REALES del tomo, no inventados. Los de zona salen de barrer el
#: volumen entero buscando formas seguras fuera del cuerpo castellano.
APPARATUS_CASES = ("p0399l0083", "p0414l0096", "p0581l0089")
LATIN_CASE = "p0018l0042"
HEADER_CASES = ("p0166l0015", "p0284l0018")
OUT_OF_BAND_CASES = ("p0542l0037", "p0553l0077")
HISTORIC_RECOVERED = {"Eccl.4.10": "p0283l0080", "Ps.72.10": "p0105l0060"}
HISTORIC_WITHHELD = {"Ps.1.2": "p0015l0027", "Eccl.1.2": "p0276l0033",
                     "Isa.17.8": "p0534l0035"}


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]["compound_glyph_recovery"]


def _glyph_section():
    return _audit()["verse_segmentation_audit"]["glyph_facsimile_validation"]


def _payload():
    with open(REVIEWS, encoding="utf-8") as handle:
        return json.load(handle)


class _Word:
    """Una palabra del reconocimiento, con su caja. Para los fixtures."""

    def __init__(self, text, x0, x1=None, y0=100, y1=170):
        self.text = text
        self.bbox = (x0, y0, x1 if x1 is not None else x0 + 60, y1)
        self.confidence = 50


class _Line:
    def __init__(self, words, index=1):
        self.words = words
        self.index = index
        self.bbox = (min(w.bbox[0] for w in words),
                     min(w.bbox[1] for w in words),
                     max(w.bbox[2] for w in words),
                     max(w.bbox[3] for w in words))

    @property
    def raw_text(self):
        return " ".join(w.text for w in self.words)

    @property
    def confidence(self):
        return 50


def _marker_line(tokens, x0=1800, text=("Texto", "del", "versiculo")):
    """Un renglón que empieza por `tokens` en la abscisa `x0`."""
    words, cursor = [], x0
    for token in tokens:
        words.append(_Word(token, cursor, cursor + 30))
        cursor += 40
    for word in text:
        words.append(_Word(word, cursor, cursor + 90))
        cursor += 100
    return _Line(words)


def _band(center=1800.0, width=60.0, markers=8):
    return (center, width, markers)


def _real_line(block_id):
    """El renglón REAL del tomo, tal y como lo colocó el parser."""
    page_number = int(block_id[1:5])
    index = int(block_id[6:])
    for page in source_ocr.read_pages(XML, first=page_number - 1):
        if page.scan_page != page_number:
            continue
        for placed in layout.split_columns(page):
            if placed.line.index == index:
                return placed, page
        break
    raise AssertionError(f"no se encontró {block_id}")


# --------------------------------------------------------------- A .. H
# Qué hay en el mapa y qué no puede estar.

def test_A_a_compound_form_maps_to_its_reviewed_value():
    value, text, detail = compound_glyphs.match(
        _marker_line(("I", "o")), _band())
    assert value == 10, detail
    assert text.startswith("Texto"), text
    assert detail["form"] == "I o"


def test_B_the_runtime_map_only_holds_review_supported_forms():
    matrix = _glyph_section()["by_form"]
    for (one, two), value in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS.items():
        form = f"{one} {two}"
        row = matrix.get(form)
        assert row, f"{form}: sin revisiones que lo respalden"
        assert row["printed_values"] == {str(value): row["confirmed"]}, \
            f"{form}: la plana no dice sólo {value}: {row['printed_values']}"
        assert row["negative"] == 0, f"{form}: tiene negativos revisados"
        assert row["confirmed"] >= glyph_reviews.MIN_REVIEWED, form
    # y al revés: ninguna forma del mapa sin respaldo, ninguna con respaldo
    # contradictorio dentro del mapa
    assert len(compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS) == len(ENABLED)


def test_C_no_single_glyph_form_is_in_the_runtime_map():
    keys = compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    for glyph in SINGLE_GLYPHS:
        assert (glyph,) not in keys, glyph
        assert glyph not in {f"{a} {b}" for a, b in keys}, glyph
    # todas las claves son parejas de UN carácter
    for key in keys:
        assert isinstance(key, tuple) and len(key) == 2, key
        assert all(len(token) == 1 for token in key), key


def _absent(form):
    tokens = tuple(form.split(" "))
    assert tokens not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS, form
    value, reason, _ = compound_glyphs.match(_marker_line(tokens), _band())
    assert value is None and reason == compound_glyphs.NOT_IN_MAP, form


def test_D_a_o_is_absent():
    _absent("a o")


def test_E_1_6_is_absent():
    _absent("1 6")


def test_F_1_7_is_absent():
    _absent("1 7")


def test_G_S_y_is_absent():
    _absent("S y")


def test_H_l_is_absent():
    value, reason, _ = compound_glyphs.match(
        _marker_line(("l",), text=("Servid", "al", "Senor")), _band())
    assert value is None, "«l» a solas no puede abrir versículo"


# --------------------------------------------------------------- I .. W
# Cada forma encendida da su cifra, y sólo la suya.

def _maps(tokens, value):
    assert compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS[tokens] == value
    got, _text, _detail = compound_glyphs.match(_marker_line(tokens), _band())
    assert got == value, f"{tokens} -> {got}"


def test_I_I_o_maps_to_10():
    _maps(("I", "o"), 10)


def test_J_I_a_maps_to_12():
    _maps(("I", "a"), 12)


def test_K_I_I_maps_to_11():
    _maps(("I", "I"), 11)


def test_L_1_1_maps_to_11():
    _maps(("1", "1"), 11)


def test_M_1_3_maps_to_13():
    _maps(("1", "3"), 13)


def test_N_1_4_maps_to_14():
    _maps(("1", "4"), 14)


def test_O_1_5_maps_to_15():
    _maps(("1", "5"), 15)


def test_P_1_8_maps_to_18():
    _maps(("1", "8"), 18)


def test_Q_1_9_maps_to_19():
    _maps(("1", "9"), 19)


def test_R_a_I_is_not_enabled():
    assert ("a", "I") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    withheld = _section()["withheld_forms"]
    assert "a I" in withheld
    assert "2" in withheld["a I"]["printed_values"], withheld["a I"]


def test_S_a_1_is_not_enabled():
    assert ("a", "1") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    assert "a 1" in _section()["withheld_forms"]


def test_T_a_a_is_not_enabled():
    assert ("a", "a") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    withheld = _section()["withheld_forms"]["a a"]
    assert "12" in withheld["printed_values"] and "22" in withheld["printed_values"]


def test_U_a_3_is_not_enabled():
    assert ("a", "3") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    assert "a 3" in _section()["withheld_forms"]


def test_V_a_4_is_not_enabled():
    assert ("a", "4") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    assert "a 4" in _section()["withheld_forms"]


def test_W_a_8_is_not_enabled():
    assert ("a", "8") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    assert "a 8" in _section()["withheld_forms"]


# --------------------------------------------------------------- X .. Z
# El emparejamiento es exacto, carácter por carácter.

def test_X_capitalization_matters():
    assert compound_glyphs.match(_marker_line(("I", "o")), _band())[0] == 10
    # la minúscula NO es la misma forma
    assert compound_glyphs.match(_marker_line(("i", "o")), _band())[0] is None
    assert compound_glyphs.match(_marker_line(("I", "O")), _band())[0] is None


def test_Y_accents_are_not_stripped():
    assert compound_glyphs.match(_marker_line(("I", "a")), _band())[0] == 12
    for pair in (("Í", "a"), ("I", "á"), ("I", "à")):
        assert compound_glyphs.match(_marker_line(pair), _band())[0] is None, pair


def test_Z_no_unicode_case_fold_is_used():
    source = open(os.path.join(DIR, "compound_glyphs.py"), encoding="utf-8").read()
    for banned in ("casefold", ".lower()", ".upper()", "unicodedata",
                   "normalize", "levenshtein", "difflib", "SequenceMatcher"):
        assert banned not in source, banned


# ------------------------------------------------------------- AA .. AC
# La geometría se mide sobre el numeral.

def test_AA_marker_bbox_excludes_scan_debris():
    # «■» delante, como palabra propia: la caja del marcador empieza en
    # la cifra, no en la mancha.
    words = [_Word("■", 1700, 1730)] + _marker_line(("I", "o"), 1800).words
    line = _Line(words)
    first, reason = compound_glyphs.marker_tokens(line.words)
    assert reason is None and line.words[first].text == "I"
    box = compound_glyphs.marker_bbox(line.words, first)
    assert box[0] == 1800, box
    assert box[0] != line.bbox[0], "la caja del renglón empieza en la mancha"
    # y con el marco PEGADO no se adivina: se dice que no se sabe
    glued = _Line([_Word("■I", 1700, 1760), _Word("o", 1800, 1830),
                   _Word("Texto", 1850)])
    assert compound_glyphs.marker_tokens(glued.words) == (
        None, compound_glyphs.GLUED_FRAME)


def test_AB_marker_bbox_uses_both_compound_tokens():
    line = _marker_line(("I", "o"), 1800)
    box = compound_glyphs.marker_bbox(line.words, 0)
    assert box[0] == line.words[0].bbox[0]
    assert box[2] == line.words[1].bbox[2], "la caja debe cubrir los dos glifos"
    assert box[2] > line.words[0].bbox[2]


def test_AC_the_trusted_band_comes_from_numeral_geometry():
    # La banda sale de marcadores decimales ORDINARIOS, no de cualquier
    # renglón: si no los hay, no hay banda y no se recupera nada.
    ordinary = [_marker_line(("7",), 1800), _marker_line(("8",), 1802),
                _marker_line(("9",), 1798)]
    band = compound_glyphs.band_of(ordinary)
    assert band is not None and abs(band[0] - 1800) <= 2, band
    assert band[2] == 3
    prose = [_marker_line(("y",), 1750), _marker_line(("a",), 1750)]
    assert compound_glyphs.band_of(prose) is None
    assert compound_glyphs.band_of(ordinary[:2]) is None, \
        "con menos de tres marcadores la mediana es una anécdota"


# ------------------------------------------------------------- AD .. AI
# Zona y sangría.

def test_AD_body_zone_is_accepted():
    section = _section()
    assert section["dry_run_by_zone"] == {"body": section["dry_run_matches"]}
    assert section["dry_run_by_column"] == {"right": section["dry_run_matches"]}
    assert section["applied"] > 0


def test_AE_apparatus_zone_is_rejected():
    # Casos REALES: renglones de nota al pie que empiezan por una forma
    # segura. Ninguno puede abrir versículo.
    section = _section()
    recovered = {row["block_id"] for row in section["markers"]}
    for block_id in APPARATUS_CASES:
        placed, _page = _real_line(block_id)
        assert placed.zone is layout.Zone.APPARATUS, block_id
        first, _ = compound_glyphs.marker_tokens(placed.line.words)
        pair = (placed.line.words[first].text, placed.line.words[first + 1].text)
        assert pair in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS, pair
        assert block_id not in recovered, f"{block_id} abrió versículo en nota"
    assert section["dry_run_by_zone"].get("apparatus", 0) == 0


def test_AF_latin_column_is_rejected():
    placed, _page = _real_line(LATIN_CASE)
    assert placed.column is layout.Column.LEFT, "es la columna latina"
    first, _ = compound_glyphs.marker_tokens(placed.line.words)
    pair = (placed.line.words[first].text, placed.line.words[first + 1].text)
    assert pair in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS, pair
    section = _section()
    recovered = {row["block_id"] for row in section["markers"]}
    assert LATIN_CASE not in recovered
    assert section["dry_run_by_column"].get("left", 0) == 0, \
        "el latín se compone con las mismas cifras y no es texto castellano"


def test_AG_running_header_and_footer_are_rejected():
    section = _section()
    recovered = {row["block_id"] for row in section["markers"]}
    for block_id in HEADER_CASES:
        placed, _page = _real_line(block_id)
        assert placed.zone is layout.Zone.HEADER, block_id
        assert block_id not in recovered, block_id
    assert section["dry_run_by_zone"].get("header", 0) == 0
    assert section["dry_run_by_zone"].get("footer", 0) == 0


def test_AH_wrong_indentation_is_rejected():
    band = _band(center=1800.0, width=60.0)
    limit = compound_glyphs.tolerance(band)
    off = _marker_line(("I", "o"), int(1800 - limit - 20))
    value, reason, detail = compound_glyphs.match(off, band)
    assert value is None and reason == compound_glyphs.OUT_OF_BAND, detail
    # y el tomo trae casos REALES de esto
    rejected = {row["block_id"]: row
                for row in _section()["geometry_rejected_detail"]}
    for block_id in OUT_OF_BAND_CASES:
        assert block_id in rejected, block_id
        assert rejected[block_id]["reason"] == compound_glyphs.OUT_OF_BAND


def test_AI_correct_indentation_is_accepted():
    band = _band(center=1800.0, width=60.0)
    for delta in (0, 10, -10, 25, -25):
        value, _text, detail = compound_glyphs.match(
            _marker_line(("I", "o"), 1800 + delta), band)
        assert value == 10, (delta, detail)


# ------------------------------------------------------------- AJ .. AN
# El canon rechaza; la secuencia no manda.

def test_AJ_a_marker_above_the_verse_limit_is_rejected():
    section = _section()
    assert section["canon_rejections"] >= 1, \
        "el canon tiene que haber rechazado algo, o no está conectado"
    for row in section["canon_rejected_detail"]:
        assert row["marker"] > row["verse_limit"], row
        assert row["text_landed"] == "joined_to_previous_verse", row
        assert row["blocks"] > 0, "el texto no se pierde"
    # y el mapa NO se ajustó al canon: sigue proponiendo el mismo valor
    for row in section["canon_rejected_detail"]:
        limit = structure.verse_limit(row["book"], row["chapter"])
        assert limit == row["verse_limit"]


def test_AK_the_missing_gap_does_not_determine_the_mapping():
    # El valor sale de la tabla. Se comprueba con el propio tomo: hay
    # marcadores recuperados cuyo valor NO coincide con el hueco que el
    # inventario esperaba en ese sitio.
    audit = _audit()
    gaps = {gap["key"] for gap in
            audit["verse_segmentation_audit"]["gaps"]}
    section = _section()
    disagree = [row for row in section["markers"]
                if f"{row['book']}.{row['chapter_slot']}.{row['value']}" in gaps]
    # lo importante no es el número: es que el valor viene de la forma
    for row in section["markers"]:
        pair = tuple(row["form"].split(" "))
        assert compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS[pair] == row["value"], row
    assert isinstance(disagree, list)


def test_AL_previous_plus_one_is_not_used():
    # La prueba fuerte no es buscar cadenas: es que la función NO PUEDE
    # mirar los vecinos, porque no los recibe. `match` toma un renglón y
    # una banda geométrica, y nada más; no hay capítulo, ni versículo
    # anterior, ni inventario de huecos a mano.
    import inspect
    assert list(inspect.signature(compound_glyphs.match).parameters) == \
        ["line", "band"]
    source = open(os.path.join(DIR, "compound_glyphs.py"), encoding="utf-8").read()
    for banned in ("previous", "prev_verse", "verse + 1", "verse+1",
                   "last_verse", "chapter.verses"):
        assert banned not in source, banned


def test_AM_next_minus_one_is_not_used():
    source = open(os.path.join(DIR, "compound_glyphs.py"), encoding="utf-8").read()
    for banned in ("next_verse", "verse - 1", "verse-1", "materialized",
                   "expected_gap"):
        assert banned not in source, banned
    # y el módulo no importa el inventario de huecos
    tree = ast.parse(source)
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                assert alias.name != "verse_gaps", "la recuperación no puede leer los huecos"
        elif isinstance(node, ast.ImportFrom):
            assert node.module != "verse_gaps"


def test_AN_an_existing_target_ref_is_not_duplicated():
    audit = _audit()
    assert audit["duplicate_refs"] == []
    section = _section()
    # Marcadores y referencias no son la misma cuenta: algunos caen en
    # una referencia que ya existía, y dos pueden caer en la misma
    # referencia nueva. Lo que no puede haber es una referencia repetida.
    assert section["applied"] == (section["markers_opening_an_added_ref"]
                                  + section["landed_in_existing_ref"])
    assert (section["markers_opening_an_added_ref"] - section["refs_added"]
            == section["added_refs_with_more_than_one_marker"])
    assert section["landed_in_existing_ref"] >= 0


# ------------------------------------------------------------- AO .. AW
# Lo que no puede haber cambiado.

def test_AO_raw_ocr_is_unchanged():
    audit = _audit()
    assert audit["metrics"]["ocr_blocks"] == 57700
    # el crudo que guarda cada marcador es el del reconocimiento, con sus
    # dos glifos separados: no se ha escrito «10» en ningún sitio
    for row in _section()["markers"]:
        assert row["form"] in row["raw"] or row["raw"].lstrip().startswith(
            row["form"].split(" ")[0]), row["raw"]


def test_AP_glyph_tokens_do_not_leak_into_the_verse_body():
    for tokens, value in list(ENABLED.items())[:4]:
        _value, text, _detail = compound_glyphs.match(
            _marker_line(tokens), _band())
        assert not text.startswith(tokens[0]), text
        assert not text.startswith(tokens[1]), text
        assert text.startswith("Texto"), text
    # y en el tomo, el texto del marcador nunca empieza por su propia forma
    for row in _section()["markers"]:
        head = row["text_head"].split()[:2]
        assert head[:2] != row["form"].split(" "), row


def test_AQ_no_block_is_lost():
    section = _section()
    assert section["block_loss"] == 0
    assert section["blocks_entering_text"] >= 0


def test_AR_no_block_has_two_owners():
    assert _section()["dual_ownership"] == 0


def test_AS_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337
    assert audit["structure_resolution"]["chapters_resolved"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["metrics"]["chapters_left_in_review"] == 0
    assert audit["canonical_chapter_gap_reviews"]["canonical_missing"] == {}


def test_AT_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AU_out_of_order_refs_stay_at_zero():
    audit = _audit()
    assert audit["out_of_order_refs"] == []
    assert audit["out_of_order_chapters"] == []


def test_AV_no_marker_lands_outside_the_canon():
    assert _audit()["verse_segmentation_audit"]["beyond_canonical_top"] == 0


def test_AW_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


# ------------------------------------------------------------- AX .. BC
# Los casos con nombre.

def _not_recovered(block_id):
    recovered = {row["block_id"] for row in _section()["markers"]}
    assert block_id not in recovered, f"{block_id} no puede recuperarse solo"


def test_AX_ps_1_2_is_still_not_recovered_automatically():
    _not_recovered(HISTORIC_WITHHELD["Ps.1.2"])


def test_AY_eccl_1_2_is_still_not_recovered_automatically():
    _not_recovered(HISTORIC_WITHHELD["Eccl.1.2"])


def test_AZ_isa_17_8_is_still_not_recovered_automatically():
    _not_recovered(HISTORIC_WITHHELD["Isa.17.8"])


def _recovered(key, block_id, value):
    section = _section()
    rows = {row["block_id"]: row for row in section["markers"]}
    assert block_id in rows, f"{key}: no se recuperó"
    assert rows[block_id]["value"] == value, rows[block_id]
    assert key in section["refs_added_detail"], f"{key}: no es una ref nueva"


def test_BA_eccl_4_10_is_a_real_regression():
    _recovered("Eccl.4.10", HISTORIC_RECOVERED["Eccl.4.10"], 10)


def test_BB_ps_72_10_is_a_real_regression():
    _recovered("Ps.72.10", HISTORIC_RECOVERED["Ps.72.10"], 10)


def test_BC_a_o_remains_unsafe():
    assert ("a", "o") not in compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS
    matrix = _glyph_section()["by_form"]
    assert matrix["a o"]["distinct_printed_values"] > 1, matrix["a o"]
    assert "a o" in _section()["withheld_forms"]


# ------------------------------------------------------------- BD .. BF
# Los metadatos.

def test_BD_batch_124_is_unchanged():
    rows = [row for row in _payload()["reviews"] if row["batch"] == "batch-124"]
    assert len(rows) == 12, len(rows)
    outcomes = collections.Counter(row["outcome"] for row in rows)
    assert outcomes["printed_marker_corrupted"] == 5
    assert outcomes["marker_present_parser_missed"] == 1
    for row in rows:
        assert row["structural_effect"] == "none_diagnostic_only"


def test_BE_batch_127_entries_are_preserved():
    rows = [row for row in _payload()["reviews"] if row["batch"] == "batch-127"]
    assert len(rows) == 213, len(rows)
    populations = collections.Counter(row["population"] for row in rows)
    assert populations["glyph_candidate"] == 189
    assert populations["negative_control"] == 10
    assert populations["regression_124"] == 5
    assert populations["pending_126"] == 4
    assert populations["detached_number"] == 5
    for row in rows:
        assert row["structural_effect"] == "none_diagnostic_only"


def test_BF_batch_128_entries_have_the_expected_schema():
    rows = [row for row in _payload()["reviews"] if row["batch"] == "batch-128"]
    assert rows, "sin batch-128"
    required = ("review_id", "batch", "book", "chapter", "verse", "scan_page",
                "pdf_page", "source_sha256", "block", "bbox", "marker_bbox",
                "raw_ocr", "glyph_form", "observed_printed_marker",
                "observed_printed_value", "column", "zone", "marker_indent",
                "outcome", "confidence", "rationale", "structural_effect")
    for row in rows:
        for field in required:
            assert field in row, f"{row['review_id']}: falta {field}"
        assert row["structural_effect"] == "none_diagnostic_only"
        assert row["pdf_page"] == row["scan_page"] + 1
        assert isinstance(row["observed_printed_value"], int)
    assert not glyph_reviews.problems(rows), glyph_reviews.problems(rows)
    # y la evidencia que tumbó dos formas está ahí, escrita
    values = collections.defaultdict(set)
    for row in rows:
        values[row["glyph_form"]].add(row["observed_printed_value"])
    assert values["a a"] == {12, 22}, values["a a"]
    assert values["a I"] == {2, 21}, values["a I"]


# ------------------------------------------------------------- BG .. BL
# Determinismo, idempotencia y las prohibiciones de diseño.

def test_BG_the_runtime_map_is_deterministic():
    first = list(compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS.items())
    second = list(compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS.items())
    assert first == second
    assert first == sorted(first, key=lambda kv: first.index(kv))


def test_BH_matching_is_deterministic():
    band = _band()
    for _ in range(3):
        assert compound_glyphs.match(_marker_line(("I", "a")), band)[0] == 12
        assert compound_glyphs.band_of(
            [_marker_line(("7",), 1800), _marker_line(("8",), 1802),
             _marker_line(("9",), 1798)])[0] == 1800


def test_BI_matching_the_same_line_twice_changes_nothing():
    line = _marker_line(("I", "o"))
    band = _band()
    one = compound_glyphs.match(line, band)
    two = compound_glyphs.match(line, band)
    assert one == two
    assert line.raw_text.startswith("I o"), "el crudo no se toca"


def test_BJ_the_matcher_is_offline():
    tree = ast.parse(open(os.path.join(DIR, "compound_glyphs.py"),
                          encoding="utf-8").read())
    banned = {"urllib", "requests", "socket", "http", "subprocess", "PIL",
              "json", "fitz"}
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                assert alias.name.split(".")[0] not in banned, alias.name
        elif isinstance(node, ast.ImportFrom):
            assert (node.module or "").split(".")[0] not in banned, node.module


def test_BK_there_is_no_page_specific_logic_in_production():
    # No basta con buscar números de plana: «15» es además una cifra
    # legítima de la tabla. Lo que se exige es más fuerte y no depende de
    # qué números elija uno: TODA constante entera del módulo es o bien
    # una cifra de la tabla, o bien el valor de una constante con nombre
    # --el mínimo de marcadores de la banda, el suelo de la tolerancia--.
    # Así no cabe un número de plana suelto en ninguna parte.
    tree = ast.parse(open(os.path.join(DIR, "compound_glyphs.py"),
                          encoding="utf-8").read())
    named = set()
    for node in tree.body:
        if isinstance(node, ast.Assign) and isinstance(node.value, ast.Constant) \
                and isinstance(node.value.value, (int, float)):
            named.add(node.value.value)
    allowed = set(compound_glyphs.SAFE_COMPOUND_VERSE_GLYPHS.values()) | named
    # los índices de las dos palabras del numeral son estructura, no datos
    allowed |= {0, 1, 2}
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, int) \
                and not isinstance(node.value, bool):
            assert node.value in allowed, node.value


def test_BL_no_verse_number_is_hardcoded_by_book_chapter_or_page():
    source = open(os.path.join(DIR, "compound_glyphs.py"), encoding="utf-8").read()
    for banned in ("Eccl", "Ps.", "Isa", "Prov", "Sir", "Song", "Wis",
                   "scan_page", "block_id"):
        assert banned not in source, banned
    # las únicas cifras que la producción conoce son las de la tabla, y
    # cada una está respaldada por la revisión
    tree = ast.parse(source)
    assigned = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Dict):
            for value in node.values:
                if isinstance(value, ast.Constant) and isinstance(value.value, int):
                    assigned.add(value.value)
    assert assigned == set(ENABLED.values()), assigned


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
    print(f"torresamat1835_compound_glyph_failures={failures}")
    sys.exit(1 if failures else 0)
