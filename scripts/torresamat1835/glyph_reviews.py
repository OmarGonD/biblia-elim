"""
Lo que la plana dijo de cada forma, y qué se puede concluir de ello.

    REVIEWED IS NOT AUTOMATABLE.

Una revisión positiva dice lo que hay impreso en UN renglón. No dice lo
que hay impreso en los otros ciento y pico de esa misma forma, y menos
todavía autoriza a convertir la forma en una cifra en tiempo de
ejecución. Este módulo mantiene esas dos cosas separadas a propósito:

    reviewed_instances          lo mirado
    confirmed_digit_confusions  lo mirado que resultó ser cifra
    candidate_for_automation    las formas que ADEMÁS cumplen el criterio
    unsafe_forms                las que la plana desmiente
    unresolved_forms            las que no tienen evidencia suficiente

y el criterio de la tercera es duro, porque el error que se paga caro es
convertir una preposición en un número de versículo.

Aquí NO hay ningún mapa de glifos. `observed` es un registro de lo visto,
lleno por las revisiones, y no lo lee ni el parser ni la recuperación: el
único consumidor es el informe. Si algún día una forma se automatiza,
será con una regla escrita a mano, con su contexto y sus exclusiones, y
en otra tanda.

Lo que la evidencia de esta tanda enseñó y conviene no perder:

    LA FORMA SOLA NO BASTA. «a» es la cifra 2 catorce veces y la
    preposición dos veces; «y» es la conjunción doce veces y la cifra 7
    una. Quien mire sólo el glifo se equivoca en las dos direcciones.

    LA SANGRÍA SEPARA. En esta edición el renglón que abre versículo
    entra y el que continúa no. Es geometría, no lectura, y es la
    condición objetiva que el criterio exige.

    LA SANGRÍA SE MIDE MAL CUANDO HAY BASURA DELANTE. La caja del
    renglón empieza en la mancha, no en la cifra, y entonces la medida
    sale a ras aunque el marcador esté en su sitio. Una regla futura
    tendrá que medir sobre la caja de la PROPIA cifra.
"""
import collections
from typing import Dict, List, Optional

BATCH = "batch-127"
COMPOUND_BATCH = "batch-128"
#: Todas las tandas que miran GLIFOS. La sección de fronteras del audit
#: las excluye en bloque: preguntan otra cosa y tienen otro vocabulario
#: de desenlaces, y sumarlas allí haría ilegibles las dos cuentas.
GLYPH_BATCHES = (BATCH, COMPOUND_BATCH)

#: De qué población sale cada revisión. La matriz de formas se calcula
#: SÓLO con las de `GLYPH_POPULATIONS`, que son renglones de esta clase:
#: la muestra estratificada, los controles negativos buscados a propósito
#: y las cinco correspondencias que la 124 había confirmado y que aquí se
#: vuelven a mirar. Las otras son casos heredados --los cuatro que la 126
#: dejó pendientes y las cifras sueltas-- y meterlas en la matriz
#: inventariaría formas que no pertenecen a esta población.
GLYPH_POPULATIONS = ("glyph_candidate", "negative_control", "regression_124")
COMPOUND_POPULATIONS = ("compound_exhaustive",)
CARRIED_POPULATIONS = ("pending_126", "detached_number")

CONFIRMED = "confirmed_digit_confusion"
REAL_TEXT = "real_spanish_text"
DEBRIS = "punctuation_or_scan_debris"
DETACHED = "detached_marker"
COMPOUND = "compound_marker"
UNREADABLE = "unreadable_facsimile"
WRONG = "wrong_candidate"
OTHER = "other"

OUTCOMES = (CONFIRMED, REAL_TEXT, DEBRIS, DETACHED, COMPOUND, UNREADABLE,
            WRONG, OTHER)

#: Los desenlaces que dicen «aquí no había cifra». Uno solo de estos en
#: una forma basta para que la forma no se automatice: es precisamente el
#: caso que convertiría texto castellano en un número de versículo.
NEGATIVE = frozenset({REAL_TEXT, DEBRIS, WRONG})

#: Mínimo de renglones mirados para poder decir algo de una forma. No es
#: un porcentaje: es que por debajo de esto la muestra no distingue una
#: regularidad de una casualidad.
MIN_REVIEWED = 4

#: Ningún desenlace de esta tanda puede tocar el texto.
DIAGNOSTIC_ONLY = "none_diagnostic_only"


def reviews_of(payload, *, batch: str = BATCH) -> List[dict]:
    """Las revisiones de una tanda, en orden estable."""
    rows = [row for row in (payload or {}).get("reviews", [])
            if row.get("batch") == batch]
    return sorted(rows, key=lambda row: row.get("review_id") or "")


def reviews_of_glyph_batches(payload) -> List[dict]:
    """Las revisiones de TODAS las tandas que miran glifos, en orden.

    La 127 inventarió y la 128 agotó lo que la 127 dejó a medias. Son la
    misma pregunta y se leen juntas; lo que no se mezcla nunca es esto
    con las revisiones de FRONTERA de la 124, que preguntan otra cosa.
    """
    rows = [row for row in (payload or {}).get("reviews", [])
            if row.get("batch") in GLYPH_BATCHES]
    return sorted(rows, key=lambda row: row.get("review_id") or "")


def of_population(rows: List[dict], populations) -> List[dict]:
    """Las revisiones de una población concreta."""
    wanted = frozenset(populations)
    return [row for row in rows if row.get("population") in wanted]


def problems(rows: List[dict]) -> List[str]:
    """Lo que hace inservible una revisión. Se dice, no se silencia."""
    found = []
    for row in rows:
        rid = row.get("review_id", "?")
        if row.get("outcome") not in OUTCOMES:
            found.append(f"{rid}: unknown outcome {row.get('outcome')!r}")
        if row.get("structural_effect") != DIAGNOSTIC_ONLY:
            found.append(f"{rid}: structural_effect must be {DIAGNOSTIC_ONLY}")
        if row.get("outcome") == CONFIRMED and row.get(
                "observed_printed_value") is None:
            found.append(f"{rid}: confirmed without a printed value")
        if row.get("outcome") != CONFIRMED and row.get(
                "observed_printed_value") is not None and \
                row.get("outcome") != COMPOUND:
            found.append(f"{rid}: printed value on a non-confirmed outcome")
        if not row.get("glyph_form"):
            found.append(f"{rid}: no glyph form")
        if row.get("population") not in (GLYPH_POPULATIONS + CARRIED_POPULATIONS
                                         + COMPOUND_POPULATIONS):
            found.append(f"{rid}: unknown population {row.get('population')!r}")
    return found


def by_form(rows: List[dict]) -> dict:
    """La matriz FORMA -> VALOR IMPRESO, con sus negativos al lado.

    El valor sale de `observed_printed_value`, que lo puso la revisión
    mirando la plana. El hueco que se esperaba viaja en el mismo registro
    pero por separado y no entra en esta cuenta: si entrase, la matriz
    estaría midiendo la versificación en vez del impreso.
    """
    buckets: Dict[str, List[dict]] = collections.defaultdict(list)
    for row in rows:
        buckets[row["glyph_form"]].append(row)
    out = {}
    for form, group in buckets.items():
        outcomes = collections.Counter(row["outcome"] for row in group)
        values = collections.Counter(
            row["observed_printed_value"] for row in group
            if row["outcome"] == CONFIRMED
            and row["observed_printed_value"] is not None)
        out[form] = {
            "glyph_form": form,
            "reviewed": len(group),
            "by_outcome": dict(sorted(outcomes.items())),
            "confirmed": outcomes.get(CONFIRMED, 0),
            "negative": sum(outcomes.get(name, 0) for name in NEGATIVE),
            "printed_values": dict(sorted(
                (str(value), count) for value, count in values.items())),
            "distinct_printed_values": len(values),
            "books": sorted({row["book"] for row in group}),
            "zones": dict(sorted(collections.Counter(
                str(row.get("zone")) for row in group).items())),
            "review_ids": [row["review_id"] for row in group],
        }
    return dict(sorted(out.items(),
                       key=lambda kv: (-kv[1]["reviewed"], kv[0])))


def classify(matrix: dict) -> dict:
    """Qué se puede hacer con cada forma, y por qué.

    Una forma entra en `candidate_for_automation` sólo si se han mirado
    al menos `MIN_REVIEWED` renglones suyos, TODOS resultaron ser la
    misma cifra impresa, y NINGUNO resultó ser castellano, mancha ni
    candidato equivocado. Basta un negativo para que la forma pase a
    `unsafe`: no se compensa con frecuencia, porque el daño de convertir
    una preposición en versículo no lo deshace acertar en otros cien.

    `unsafe` no significa irrecuperable. Significa que la forma sola no
    decide, y que hará falta una condición objetiva --la sangría es la
    candidata-- medida y validada aparte.
    """
    safe, unsafe, unresolved = {}, {}, {}
    for form, row in matrix.items():
        reason = None
        if row["negative"] and row["confirmed"]:
            reason = ("la plana da unas veces cifra y otras castellano o "
                      "mancha: la forma sola no decide")
        elif row["negative"] and not row["confirmed"]:
            reason = "la plana nunca dio una cifra aquí"
        elif row["distinct_printed_values"] > 1:
            reason = ("la misma forma corresponde a mas de una cifra "
                      "impresa")
        elif row["confirmed"] < MIN_REVIEWED:
            reason = (f"sólo {row['confirmed']} renglones confirmados; "
                      f"hacen falta {MIN_REVIEWED}")
        entry = dict(row)
        entry["verdict_reason"] = reason
        if reason is None:
            entry["observed_value"] = next(iter(row["printed_values"]))
            safe[form] = entry
        elif row["negative"] or row["distinct_printed_values"] > 1:
            unsafe[form] = entry
        else:
            unresolved[form] = entry
    return {"candidate_for_automation": safe, "unsafe": unsafe,
            "unresolved": unresolved}


def negative_controls(rows: List[dict]) -> dict:
    """Los renglones que resultaron NO ser cifra, por forma.

    Son la mitad que de verdad protege: sin ellos una tabla de
    correspondencias sólo demuestra que se buscaron confirmaciones.
    """
    found: Dict[str, List[dict]] = collections.defaultdict(list)
    for row in rows:
        if row["outcome"] in NEGATIVE:
            found[row["glyph_form"]].append({
                "review_id": row["review_id"], "block": row["block"],
                "scan_page": row["scan_page"], "raw_ocr": row["raw_ocr"],
                "observed_text_context": row["observed_text_context"],
                "indent_px": row.get("indent_px"),
                "outcome": row["outcome"],
            })
    return {form: rows_ for form, rows_ in sorted(found.items())}


def coverage(matrix: dict, forms_table: dict) -> dict:
    """Qué parte de cada forma se miró y qué parte se quedó fuera.

    Sin esta cuenta la matriz se lee como si hablara de toda la
    población, y no habla: habla de la muestra.
    """
    out = {}
    for form, row in sorted(forms_table.items()):
        reviewed = matrix.get(form, {}).get("reviewed", 0)
        out[form] = {
            "instances": row["instances"],
            "candidate_gaps": row["candidate_gaps"],
            "reviewed": reviewed,
            "not_reviewed": row["instances"] - reviewed,
        }
    return out


def estimate(matrix: dict, forms_table: dict) -> dict:
    """Cuánta población tocarían las formas seguras. ES UNA ESTIMACIÓN.

    Se dice en el nombre y se repite en el informe: proyectar la
    proporción de la muestra sobre el resto NO es haber mirado el resto.
    Sirve para decidir si la 128 merece la pena, no para recuperar nada.
    """
    safe = classify(matrix)["candidate_for_automation"]
    instances = sum(forms_table.get(form, {}).get("instances", 0)
                    for form in safe)
    gaps = sum(forms_table.get(form, {}).get("candidate_gaps", 0)
               for form in safe)
    total_instances = sum(row["instances"] for row in forms_table.values())
    total_gaps = sum(row["candidate_gaps"] for row in forms_table.values())
    return {
        "disclaimer": ("ESTIMACIÓN DESCRIPTIVA, no un resultado medido: "
                       "supone que lo no mirado se comporta como lo mirado, "
                       "y eso es justamente lo que no se ha comprobado."),
        "safe_forms": len(safe),
        "instances_in_safe_forms": instances,
        "candidate_gaps_in_safe_forms": gaps,
        "share_of_instances": (round(instances / total_instances, 4)
                               if total_instances else 0.0),
        "share_of_candidate_gaps": (round(gaps / total_gaps, 4)
                                    if total_gaps else 0.0),
    }
