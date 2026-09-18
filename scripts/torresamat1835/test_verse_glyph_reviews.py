"""
TORRES-1835-VERSE-GLYPH-FACSIMILE-127: qué imprime la plana ahí.

    python3 test_verse_glyph_reviews.py

Sin red y sin el testigo. La tanda 126 dejó dos mil y pico huecos cuyo
único indicio es un GLIFO suelto dentro del verso anterior, y la
tentación era evidente: el hueco pide un 2, el reconocimiento dejó una
«a», luego «a» vale 2. Esta tanda existe para no hacer eso.

    FACSIMILE ESTABLISHES THE GLYPH MAPPING

Lo que se hizo fue contar --por la forma EXACTA, sin quitar acentos, sin
unificar la caja y sin colapsar las compuestas--, sacar una muestra
reproducible y mirarla en el facsímil. Y la plana contestó las dos cosas
que hacían falta:

    que «a» es la cifra 2 catorce veces y la preposición dos, y que «y»
    es la conjunción doce veces y la cifra 7 una. La forma sola NO
    decide, en ninguna de las dos direcciones;

    que las compuestas --«I o», «a a», «1 4»-- no aparecieron nunca como
    castellano, porque dos glifos de un carácter seguidos al principio
    de un renglón no son una palabra de esta lengua.

De ahí sale la única conclusión que esta tanda se permite: quince formas
compuestas quedan como CANDIDATAS, diecinueve quedan marcadas como
inseguras, y ni una sola referencia se ha creado, movido ni renumerado.
Lo que estos tests vigilan es justamente eso: que el diagnóstico no se
haya convertido en una tabla de glifos dentro del programa.
"""
import ast
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import glyph_forms
import glyph_reviews
import parser as classifier
import verse_gaps

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
AUDIT = os.path.join(ROOT, "build", "torresamat1835-audit", "volume3.json")
REVIEWS = os.path.join(ROOT, "data", "torresamat1835",
                       "verse_boundary_reviews.json")

#: Los cinco que la 124 ya había confirmado. Se vuelven a exigir aquí
#: porque una correspondencia que deja de estar representada ha dejado de
#: estar demostrada, aunque el informe antiguo siga diciendo que sí.
REGRESSIONS = {
    "Ps.1.2": ("a", 2),
    "Eccl.1.2": ("a", 2),
    "Isa.17.8": ("S y", 8),
    "Eccl.4.10": ("I o", 10),
    "Ps.72.10": ("I o", 10),
}

#: Los cuatro que la 126 dejó pendientes y las tres cifras sueltas.
PENDING_126 = ("Song.2.2", "Isa.23.6", "Ps.105.5", "Ps.147.7")
DETACHED = ("Ps.32.2", "Isa.31.1", "Isa.58.6")


def _audit():
    if not os.path.isfile(AUDIT):
        raise AssertionError("falta la auditoría del tomo; ejecútala primero")
    with open(AUDIT, encoding="utf-8") as handle:
        return json.load(handle)


def _payload():
    with open(REVIEWS, encoding="utf-8") as handle:
        return json.load(handle)


def _section():
    return _audit()["verse_segmentation_audit"]["glyph_facsimile_validation"]


def _rows():
    return glyph_reviews.reviews_of(_payload())


def _form_rows():
    return glyph_reviews.of_population(_rows(),
                                       glyph_reviews.GLYPH_POPULATIONS)


# --------------------------------------------------------------- A .. E
# La forma es lo que el reconocimiento dejó, carácter por carácter.

def test_A_candidate_grouping_preserves_the_exact_glyph_form():
    for raw, expected in (("a sino que tiene puesta", "a"),
                          ("á los hijos de los hombres", "á"),
                          ("S y no se postrará", "S y"),
                          ("I o Si uno va á caer", "I o"),
                          (". I o Por eso paran", "I o"),
                          ("^o Tú ordenaste las tinieblas", "o")):
        assert glyph_forms.glyph_form(raw) == expected, raw
    # y el marco viaja aparte, sin borrarse
    assert glyph_forms.head_of(". I o Por eso")[0] == "."
    assert glyph_forms.head_of("^o Tú ordenaste")[0] == "^"


def test_B_a_and_a_with_accent_stay_distinct():
    assert glyph_forms.glyph_form("a Vanidad de vanidades") == "a"
    assert glyph_forms.glyph_form("á los hijos de los hombres") == "á"
    assert glyph_forms.glyph_form("a x") != glyph_forms.glyph_form("á x")
    section = _section()
    forms = section["forms_by_frequency"]
    if "a" in forms and "á" in forms:
        assert forms["a"] is not forms["á"]
        # y la plana dijo cosas distintas de cada una
        matrix = section["by_form"]
        assert matrix["a"]["printed_values"], "«a» sin ninguna cifra vista"
        assert not matrix["á"]["printed_values"], "«á» no es ninguna cifra"


def test_C_o_and_o_with_accent_stay_distinct():
    assert glyph_forms.glyph_form("o amigo dirá") == "o"
    assert glyph_forms.glyph_form("ó Iduméa y de Bosra") == "ó"
    assert glyph_forms.glyph_form("o x") != glyph_forms.glyph_form("ó x")


def test_D_uppercase_and_lowercase_stay_distinct():
    assert glyph_forms.glyph_form("S Cesó el festivo sonido") == "S"
    assert glyph_forms.glyph_form("s Cesó el festivo sonido") == "s"
    assert glyph_forms.glyph_form("Y ciertamente que") == "Y"
    assert glyph_forms.glyph_form("y ciertamente que") == "y"


def test_E_composite_forms_stay_distinct_from_the_single_glyph():
    assert glyph_forms.glyph_form("I o Si uno va") == "I o"
    assert glyph_forms.glyph_form("I Servid al Señor") == "I"
    assert glyph_forms.glyph_form("I o Si uno") != glyph_forms.glyph_form(
        "I Servid al")
    # una palabra castellana de dos letras NO entra en la forma
    assert glyph_forms.glyph_form("y el proceder del hombre") == "y"
    assert glyph_forms.glyph_form("S Tú eres siempre") == "S"


# --------------------------------------------------------------- F .. G
# De dónde NO sale el valor impreso.

def test_F_the_expected_gap_does_not_assign_the_printed_value():
    rows = _form_rows()
    assert rows, "sin revisiones"
    # Hay al menos un caso en que el hueco esperado y la cifra impresa
    # discrepan. Si no lo hubiera, no se podría distinguir una lectura
    # de una copia del valor esperado.
    disagree = [row for row in rows
                if row["outcome"] == glyph_reviews.CONFIRMED
                and row["observed_printed_value"]
                != row["expected_gap_diagnostic_only"]]
    assert disagree, "ninguna lectura se aparta del hueco esperado"
    # y el valor esperado está marcado como lo que es en TODAS
    for row in rows:
        assert "expected_gap_diagnostic_only" in row, row["review_id"]
    # las negativas no traen ninguna cifra, por mucho que el hueco pidiera una
    for row in rows:
        if row["outcome"] in glyph_reviews.NEGATIVE:
            assert row["observed_printed_value"] is None, row["review_id"]


def test_G_the_sequence_does_not_assign_the_printed_value():
    # La misma forma con la misma vecindad da cifras distintas: «a o» es
    # 20 en una plana y 10 en otra. Si la secuencia mandara, no podría
    # pasar.
    matrix = _section()["by_form"]
    assert matrix["a o"]["distinct_printed_values"] > 1, \
        "«a o» debería tener más de una cifra vista"
    # y ninguna revisión cita a los vecinos como fundamento
    for row in _rows():
        text = row["rationale"].lower()
        for banned in ("previous+1", "next-1", "anterior + 1", "siguiente - 1"):
            assert banned not in text, row["review_id"]


# --------------------------------------------------------------- H .. L
# Qué puede registrar una revisión, y qué no puede hacer.

def test_H_facsimile_review_records_a_confirmed_digit_confusion():
    rows = [row for row in _form_rows()
            if row["outcome"] == glyph_reviews.CONFIRMED]
    assert len(rows) >= 100, len(rows)
    for row in rows:
        assert isinstance(row["observed_printed_value"], int), row["review_id"]
        assert row["observed_text_context"], row["review_id"]
        assert row["source_sha256"], row["review_id"]
        assert row["pdf_page"] == row["scan_page"] + 1, row["review_id"]


def test_I_real_spanish_text_is_recorded_as_a_negative_control():
    controls = _section()["negative_controls"]
    assert controls, "sin controles negativos"
    # las formas que el castellano usa de verdad tienen el suyo
    for form in ("a", "á", "y", "ó"):
        assert form in controls, f"falta control negativo de «{form}»"
        assert controls[form], form
    for rows in controls.values():
        for row in rows:
            assert row["observed_text_context"], row
            assert row["outcome"] in glyph_reviews.NEGATIVE, row


def test_J_unreadable_stays_unresolved():
    # El desenlace existe en el vocabulario y NO cuenta como confirmación.
    assert glyph_reviews.UNREADABLE in glyph_reviews.OUTCOMES
    matrix = glyph_reviews.by_form([
        {"glyph_form": "?", "outcome": glyph_reviews.UNREADABLE,
         "observed_printed_value": None, "book": "Ps", "review_id": "x",
         "zone": "body"}])
    assert matrix["?"]["confirmed"] == 0
    verdict = glyph_reviews.classify(matrix)
    assert "?" not in verdict["candidate_for_automation"]
    assert "?" in verdict["unresolved"]


def test_K_a_positive_review_creates_no_runtime_mapping():
    # Ni el parser ni la recuperación conocen ninguna tabla de glifos.
    runtime = ("parser.py", "recovery.py", "recovery_candidates.py",
               "page_parser.py", "verse_gaps.py", "verse_markers.py",
               "structure.py", "osis_out.py")
    banned = ("GLYPH_MAP", "glyph_map", "GLYPH_TABLE", "observed_mapping")
    for name in runtime:
        text = open(os.path.join(DIR, name), encoding="utf-8").read()
        for token in banned:
            assert token not in text, f"{name}: {token}"
        # y tampoco la correspondencia escrita a mano
        tree = ast.parse(text)
        for node in ast.walk(tree):
            if not isinstance(node, ast.Dict):
                continue
            pairs = [(k, v) for k, v in zip(node.keys, node.values)
                     if isinstance(k, ast.Constant)
                     and isinstance(v, ast.Constant)]
            letters = [(k.value, v.value) for k, v in pairs
                       if isinstance(k.value, str) and len(k.value) <= 3
                       and isinstance(v.value, int)
                       and not isinstance(v.value, bool)]
            assert not letters, f"{name}: parece un mapa glifo->cifra {letters}"
    # el informe lo dice explícitamente
    assert _section()["runtime_glyph_map"] is None


def test_L_every_review_is_diagnostic_only():
    for row in _rows():
        assert row["structural_effect"] == glyph_reviews.DIAGNOSTIC_ONLY, \
            row["review_id"]
    assert _section()["structural_effect"] == glyph_reviews.DIAGNOSTIC_ONLY
    assert not _section()["schema_problems"], _section()["schema_problems"]


# --------------------------------------------------------------- M .. N
# Lo que la tanda NO ha tocado.

def test_M_the_glyph_reviews_change_no_verse_reference():
    """Las revisiones son evidencia; no mueven nada por sí solas.

    La 127 no creó ni una referencia. Una tanda POSTERIOR sí puede
    recuperar, y la 128 lo hizo con nueve formas compuestas -- pero por
    su propia regla, con su propia guarda y con su propia sección del
    informe. Lo que este test sigue vigilando es que la vía de las
    revisiones no sea nunca la que toca el texto: ni una entrada con
    efecto estructural, ni un recuento de recuperaciones en esta
    sección.
    """
    audit = _audit()
    assert audit["materialized_verse_refs"] == audit["verse_refs"]
    assert audit["verse_refs_in_review_slots"] == 0
    section = _section()
    assert "recovered" not in section
    assert section["structural_effect"] == glyph_reviews.DIAGNOSTIC_ONLY
    for row in _rows():
        assert row["structural_effect"] == glyph_reviews.DIAGNOSTIC_ONLY


def test_N_raw_ocr_is_unchanged():
    audit = _audit()
    assert audit["metrics"]["ocr_blocks"] == 57700, audit["metrics"]["ocr_blocks"]
    # y el crudo guardado en cada revisión es el del bloque, sin retocar
    for row in _form_rows():
        assert row["raw_ocr"] == row["raw_ocr"].rstrip("\n"), row["review_id"]
        assert row["raw_ocr"], row["review_id"]


# --------------------------------------------------------------- O .. Q
# La muestra.

def test_O_the_sample_is_deterministic():
    section = _section()
    first = list(section["sample"])
    assert first, "muestra vacía"
    assert len(first) == len(set(first)), "la muestra repite renglones"
    assert first == sorted(first), "la muestra no sale en orden estable"
    # el mismo inventario da la misma muestra, llamada dos veces
    rows = [_Fake(block_id=b) for b in first]
    assert [i.block_id for i in glyph_forms.sample(rows)] == \
           [i.block_id for i in glyph_forms.sample(rows)]


class _Fake(glyph_forms.Instance):
    def __init__(self, block_id):
        super().__init__(block_id=block_id, scan_page=1, pdf_page=2,
                         column="right", zone="body", bbox=(0, 0, 1, 1),
                         raw="a x", framing="", form="a", second=None)
        self.books = ["Ps"]
        self.gap_keys = ["Ps.1.2"]


def test_P_the_sample_stratifies_by_form():
    section = _section()
    forms = section["forms_by_frequency"]
    by_form = section["sample_by_form"]
    # toda forma que llega al suelo de revisión está representada
    for form, row in forms.items():
        if row["instances"] >= glyph_forms.REVIEW_FLOOR:
            assert form in by_form, f"forma sin muestra: {form!r}"
            assert by_form[form] == row["quota"], form
        else:
            assert form not in by_form, f"forma bajo el suelo muestreada: {form!r}"
    # y la muestra reparte también por libro
    assert len(section["sample_by_book"]) >= 5, section["sample_by_book"]


def test_Q_ambiguous_forms_carry_negative_controls():
    section = _section()
    matrix, controls = section["by_form"], section["negative_controls"]
    judged = set(section["candidate_for_automation"]) | set(section["unsafe_forms"])
    for form in sorted(matrix):
        if form in glyph_forms.AMBIGUOUS_WITH_SPANISH and form in judged:
            assert form in controls, \
                f"«{form}» puede ser castellano y no tiene control negativo"
    # y ninguna forma ambigua se ha declarado segura sin haber buscado el
    # castellano que la desmentiría
    for form in section["candidate_for_automation"]:
        assert form not in glyph_forms.AMBIGUOUS_WITH_SPANISH, \
            f"«{form}» es ambigua con el castellano y está dada por segura"


# --------------------------------------------------------------- R .. V
# Las cinco correspondencias que la 124 ya había visto.

def test_R_ps_1_2_regression():
    _regression("Ps.1.2")


def test_S_eccl_1_2_regression():
    _regression("Eccl.1.2")


def test_T_isa_17_8_regression():
    _regression("Isa.17.8")


def test_U_eccl_4_10_regression():
    _regression("Eccl.4.10")


def test_V_ps_72_10_regression():
    _regression("Ps.72.10")


def _regression(key):
    form, value = REGRESSIONS[key]
    book, chapter, verse = key.split(".")
    rows = [row for row in glyph_reviews.of_population(_rows(),
                                                       ("regression_124",))
            if row["book"] == book and row["chapter"] == int(chapter)
            and row["verse"] == int(verse)]
    assert rows, f"{key}: sin revisión de regresión en batch-127"
    row = rows[0]
    assert row["glyph_form"] == form, f"{key}: forma {row['glyph_form']!r}"
    assert row["outcome"] == glyph_reviews.CONFIRMED, f"{key}: {row['outcome']}"
    assert row["observed_printed_value"] == value, \
        f"{key}: {row['observed_printed_value']}"
    # y el enlace con la revisión antigua sigue en pie: mismo bloque
    old = [entry for entry in _payload()["reviews"]
           if entry.get("batch") == "batch-124"
           and f"{entry['book']}.{entry['chapter']}.{entry['verse']}" == key]
    if old:
        assert old[0]["marker_block"] == row["block"], \
            f"{key}: la 124 miró {old[0]['marker_block']} y la 127 {row['block']}"


# --------------------------------------------------------------- W .. X
# Los casos heredados.

def test_W_the_four_cases_pending_from_126_are_represented():
    rows = {f"{row['book']}.{row['chapter']}.{row['verse']}": row
            for row in glyph_reviews.of_population(_rows(), ("pending_126",))}
    for key in PENDING_126:
        assert key in rows, f"sin revisar: {key}"
        assert rows[key]["observed_text_context"], key
    # Los cuatro de la 126 siguen pendientes. La lista puede haber
    # CRECIDO -- una recuperación posterior cambia la vecindad y puede
    # destapar un hueco nuevo de esa misma clase -- y lo que no puede es
    # encogerse por la puerta de atrás ni partir un bloque.
    embedded = _audit()["verse_segmentation_audit"][
        "embedded_exact_marker_recovery"]
    still = {row["key"] for row in embedded["pending"]}
    for key in PENDING_126:
        assert key in still, f"{key} dejó de estar pendiente sin revisión"
    assert embedded["still_pending"] == len(embedded["pending"])
    assert embedded["still_pending"] >= 4, embedded["still_pending"]
    assert embedded["blocks_logically_split"] == 0


def test_X_the_detached_number_cases_are_represented():
    rows = glyph_reviews.of_population(_rows(), ("detached_number",))
    keys = {f"{row['book']}.{row['chapter']}.{row['verse']}" for row in rows}
    for key in DETACHED:
        assert key in keys, f"sin revisar: {key}"
    # ninguno resultó ser un marcador desprendido de verdad
    assert not [row for row in rows
                if row["outcome"] == glyph_reviews.DETACHED], \
        "hay un marcador desprendido: el modelo tendría que representarlo"


# --------------------------------------------------------------- Y .. AF
# Las regresiones del tomo y las prohibiciones.

def test_Y_the_chapter_map_is_unchanged():
    audit = _audit()
    assert audit["chapters"] == 337, audit["chapters"]
    assert audit["structure_resolution"]["chapters_resolved"] == 337
    assert audit["chapter_claims"]["unresolved"] == 0
    assert audit["metrics"]["chapters_left_in_review"] == 0
    assert audit["canonical_chapter_gap_reviews"]["canonical_missing"] == {}


def test_Z_duplicate_refs_stay_at_zero():
    assert _audit()["duplicate_refs"] == []


def test_AA_out_of_order_refs_stay_at_zero():
    audit = _audit()
    assert audit["out_of_order_refs"] == []
    assert audit["out_of_order_chapters"] == []


def test_AB_ocr_blocks_stay_at_57700():
    assert _audit()["metrics"]["ocr_blocks"] == 57700


def test_AC_the_volume_still_adds_up():
    # El total ya no es el de la 127 -- la 128 recuperó numerales
    # partidos -- pero lo que no puede cambiar es que todas las
    # referencias estén materializadas y ninguna quede en una ranura de
    # revisión.
    audit = _audit()
    assert audit["verse_refs"] == audit["materialized_verse_refs"]
    assert audit["verse_refs_in_review_slots"] == 0
    assert audit["verse_refs"] >= 3395, audit["verse_refs"]


def test_AD_the_inventory_is_offline():
    # Ni el inventario ni el análisis abren el facsímil ni la red. Se
    # miran los IMPORTS y no el texto: «.pdf» aparece dentro de
    # «self.pdf_page», que es un número de plana y no un fichero.
    banned = {"urllib", "urllib2", "requests", "socket", "http",
              "subprocess", "PIL", "fitz", "webbrowser"}
    for name in ("glyph_forms.py", "glyph_reviews.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                for alias in node.names:
                    assert alias.name.split(".")[0] not in banned, \
                        f"{name}: import {alias.name}"
            elif isinstance(node, ast.ImportFrom):
                assert (node.module or "").split(".")[0] not in banned, \
                    f"{name}: from {node.module}"
        text = open(os.path.join(DIR, name), encoding="utf-8").read()
        for token in ('"pdftoppm"', "open(", "'.pdf'", '".pdf"'):
            assert token not in text, f"{name}: {token}"


def test_AE_no_page_specific_logic_in_production():
    # Ninguna plana concreta aparece como número mágico.
    pages = {15, 105, 149, 276, 283, 534, 176, 463}
    for name in ("glyph_forms.py", "glyph_reviews.py"):
        tree = ast.parse(open(os.path.join(DIR, name), encoding="utf-8").read())
        for node in ast.walk(tree):
            if isinstance(node, ast.Constant) and \
                    isinstance(node.value, int) and \
                    not isinstance(node.value, bool):
                assert node.value not in pages, f"{name}: {node.value}"


def test_AF_no_runtime_glyph_substitution():
    # La lista de formas ambiguas dice QUÉ puede no ser cifra; no dice
    # qué cifra es. Comprobamos que no hay ningún número asociado.
    assert all(isinstance(form, str)
               for form in glyph_forms.AMBIGUOUS_WITH_SPANISH)
    assert not hasattr(glyph_forms, "GLYPH_MAP")
    assert not hasattr(glyph_reviews, "GLYPH_MAP")
    # y el clasificador de bloques sigue exigiendo cifras decimales
    assert classifier.framed_verse_marker("a Vanidad de vanidades") == (None, None)
    assert classifier.framed_verse_marker("I o Si uno va") == (None, None)
    assert classifier.framed_verse_marker("■ 139 Mi se lo") == (
        139, "Mi se lo")


def test_AG_the_class_is_the_one_the_inventory_names():
    section = _section()
    assert section["candidate_signal"] == verse_gaps.SWALLOWED_GLYPH
    # y las dos cuentas siguen separadas: renglones y huecos
    assert section["candidate_instances_total"] < section["candidate_gaps_total"]
    coverage = section["coverage"]
    assert sum(row["instances"] for row in coverage.values()) == \
        section["candidate_instances_total"]
    assert sum(row["not_reviewed"] for row in coverage.values()) > 0, \
        "el informe debería decir cuánto quedó sin mirar"


def test_AH_the_population_estimate_says_it_is_an_estimate():
    estimate = _section()["population_estimate"]
    assert "ESTIMACIÓN" in estimate["disclaimer"]
    assert 0.0 <= estimate["share_of_candidate_gaps"] <= 1.0


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
    print(f"torresamat1835_verse_glyph_failures={failures}")
    sys.exit(1 if failures else 0)
