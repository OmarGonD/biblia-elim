"""
¿Separa el ancho del glifo «a» el 1 impreso del 2 impreso?

    FACSIMILE LABELS THE CLASS. GEOMETRY MAY PREDICT IT.

La 128 dejó fuera de la recuperación todas las formas compuestas que
empiezan por «a», porque ese glifo resultó ser unas veces el 1 impreso y
otras el 2 — y equivocar la primera cifra no deja un versículo sin abrir,
abre el equivocado. Al cerrarla se apuntó una hipótesis: el 1 de estilo
antiguo parece bastante más estrecho que el 2, así que quizá la caja del
propio glifo los separe.

Este módulo mide esa hipótesis y NO la implementa. No hay aquí ninguna
regla que el parser pueda llamar, y a propósito: lo que se midió dice que
no la habría.

    EL RESULTADO. Las dos clases se solapan por completo. El glifo «a»
    más estrecho de todo el tomo mide 32 píxeles y la plana imprime un 2;
    los dos únicos casos de 1 impreso miden 34, que es justo donde vive
    el grueso de los doses. Cualquier umbral que acepte los dos unos se
    lleva por delante veintiún doses.

    POR QUÉ. La caja que el reconocimiento asigna al token no es una
    medida del glifo impreso: para el MISMO 2 impreso va de 32 a 82
    píxeles según lo que el reconocimiento haya metido dentro. Se está
    midiendo la decisión del OCR, no la tipografía.

La etiqueta sale exclusivamente de `observed_printed_value` — la primera
cifra de lo que se leyó en la plana --. Ni el hueco que falta, ni los
versículos vecinos, ni el segundo glifo entran en ella: el segundo glifo
sería fuga de la variable objetivo, porque «a 4» ya insinúa un 24.
"""
import collections
import statistics
from typing import Dict, List, Optional

#: Las tandas que miraron glifos «a» compuestos y dejaron una cifra
#: impresa escrita. De aquí sale la etiqueta, y de ningún otro sitio.
LABEL_BATCHES = ("batch-127", "batch-128", "batch-129")

#: Qué primera cifra puede llevar una etiqueta. No hay más clases: el
#: estudio pregunta por el primer glifo de un numeral de dos cifras.
PRINTED_1 = 1
PRINTED_2 = 2

#: Ningún desenlace de esta tanda puede tocar el texto.
DIAGNOSTIC_ONLY = "none_diagnostic_only"


def is_a_compound(form: Optional[str]) -> bool:
    """¿Es una forma compuesta cuyo PRIMER token es exactamente «a»?

    Comparación exacta, carácter por carácter. «á» no es «a», y no se
    normaliza para que lo parezca: es justo la diferencia que separa la
    preposición castellana del glifo que puede ser cifra.
    """
    if not form or " " not in form:
        return False
    one, two = form.split(" ", 1)
    # El segundo token tiene que ser un glifo de verdad: un espacio mide
    # un carácter y no es nada.
    return one == "a" and len(two) == 1 and not two.isspace()


def label_of(printed_value: Optional[int]) -> Optional[int]:
    """La PRIMERA cifra impresa. Es la etiqueta, y sale sólo de la plana."""
    if printed_value is None:
        return None
    return int(str(abs(int(printed_value)))[0])


def dataset(reviews: List[dict], geometry: Dict[str, dict]) -> List[dict]:
    """Las instancias etiquetadas y medibles, una por renglón.

    Se cruza lo que dijo la plana con la caja que el reconocimiento dio a
    cada palabra. Un renglón sin caja separable no entra: estimar dónde
    empieza el glifo dentro de una palabra con el marco pegado sería
    inventarse la medida que se está estudiando.
    """
    seen, rows = set(), []
    for review in sorted(reviews, key=lambda r: r.get("review_id") or ""):
        if review.get("batch") not in LABEL_BATCHES:
            continue
        if not is_a_compound(review.get("glyph_form")):
            continue
        block = review.get("block")
        if block in seen:
            continue
        label = label_of(review.get("observed_printed_value"))
        if label is None:
            continue
        measured = geometry.get(block)
        if measured is None:
            continue
        seen.add(block)
        width = measured["width"]
        height = measured["height"]
        band = measured.get("band")
        rows.append({
            "block": block, "form": review["glyph_form"],
            "printed_value": review["observed_printed_value"],
            "label": label, "book": review.get("book"),
            "page": measured["page"], "width": width, "height": height,
            "aspect": (width / height) if height else None,
            "normalized_width": (width / band[1]) if band else None,
        })
    return rows


def _quantiles(values: List[float]) -> dict:
    ordered = sorted(values)
    if not ordered:
        return {"n": 0}
    pick = lambda p: ordered[min(len(ordered) - 1, int(len(ordered) * p))]
    median = statistics.median(ordered)
    return {
        "n": len(ordered), "min": ordered[0], "p10": pick(0.10),
        "p25": pick(0.25), "median": median, "p75": pick(0.75),
        "p90": pick(0.90), "max": ordered[-1],
        "mad": statistics.median([abs(v - median) for v in ordered]),
        "mean_secondary": round(statistics.fmean(ordered), 3),
    }


def stats(rows: List[dict], key: str) -> dict:
    """Los cuantiles de una medida, por clase. La media va como secundaria."""
    out = {}
    for label in (PRINTED_1, PRINTED_2):
        values = [row[key] for row in rows
                  if row["label"] == label and row[key] is not None]
        out[str(label)] = _quantiles(values)
    return out


def overlap(rows: List[dict], key: str) -> dict:
    """Cuánto se pisan las dos clases en una medida.

    La pregunta de la tanda en una línea: ¿el mayor de los unos queda por
    debajo del menor de los doses? Si no, no hay umbral que los separe, y
    contar cuántos se pisan dice lo grave que es.
    """
    ones = [row[key] for row in rows
            if row["label"] == PRINTED_1 and row[key] is not None]
    twos = [row[key] for row in rows
            if row["label"] == PRINTED_2 and row[key] is not None]
    if not ones or not twos:
        return {"separable": None, "reason": "una de las clases está vacía",
                "n_printed_1": len(ones), "n_printed_2": len(twos)}
    return {
        "max_printed_1": max(ones), "min_printed_2": min(twos),
        "separable": max(ones) < min(twos),
        "printed_2_at_or_below_max_printed_1":
            sum(1 for value in twos if value <= max(ones)),
        "printed_2_below_min_printed_1":
            sum(1 for value in twos if value < min(ones)),
        "n_printed_1": len(ones), "n_printed_2": len(twos),
    }


def thresholds(rows: List[dict], key: str = "width") -> List[dict]:
    """Un umbral simple, barrido sobre todo el rango observado.

    `value <= T` predice 1 y `value > T` predice 2. Se cuentan las dos
    inversiones por separado, que son las que importan: un 2 llamado 1
    abre el versículo equivocado; un 1 llamado 2, también.
    """
    values = sorted({row[key] for row in rows if row[key] is not None})
    out = []
    for cut in values:
        tp1 = sum(1 for r in rows if r["label"] == PRINTED_1 and r[key] <= cut)
        fp1 = sum(1 for r in rows if r["label"] == PRINTED_2 and r[key] <= cut)
        tp2 = sum(1 for r in rows if r["label"] == PRINTED_2 and r[key] > cut)
        fp2 = sum(1 for r in rows if r["label"] == PRINTED_1 and r[key] > cut)
        out.append({"threshold": cut, "tp_printed_1": tp1,
                    "fp_printed_1": fp1, "tp_printed_2": tp2,
                    "fp_printed_2": fp2, "class_inversions": fp1 + fp2})
    return out


def abstention_band(rows: List[dict], key: str = "width") -> dict:
    """La mejor banda de abstención SIN inversiones, si existe.

    Se prefiere precisión a cobertura: es legítimo no decidir. Lo que no
    es legítimo es decidir mal. Se busca el par (T1, T2) que más casos
    resuelve sin equivocarse en ninguno, y se dice además cuántos de la
    clase minoritaria quedan resueltos -- que es donde se ve si la regla
    sirve para algo o sólo sabe decir «dos».
    """
    values = [row[key] for row in rows if row[key] is not None]
    if not values:
        return {"exists": False}
    lo, hi = int(min(values)) - 2, int(max(values)) + 2
    best = None
    for low in range(lo, hi + 1):
        for high in range(low, hi + 1):
            bad_two = sum(1 for r in rows if r["label"] == PRINTED_2
                          and r[key] is not None and r[key] <= low)
            bad_one = sum(1 for r in rows if r["label"] == PRINTED_1
                          and r[key] is not None and r[key] >= high)
            if bad_two or bad_one:
                continue
            decided = sum(1 for r in rows if r[key] is not None
                          and (r[key] <= low or r[key] >= high))
            ones = sum(1 for r in rows if r["label"] == PRINTED_1
                       and r[key] is not None and r[key] <= low)
            if best is None or decided > best["decided"]:
                best = {"exists": True, "low": low, "high": high,
                        "decided": decided, "total": len(rows),
                        "printed_1_decided": ones,
                        "printed_1_total": sum(1 for r in rows
                                               if r["label"] == PRINTED_1)}
    if best is None:
        return {"exists": False}
    best["useless_for_printed_1"] = best["printed_1_decided"] == 0
    return best


def leave_one_out(rows: List[dict], key: str = "width") -> dict:
    """Validación dejando uno fuera, con el umbral óptimo del resto.

    Con una clase de dos ejemplares el acierto global no dice nada: se
    consigue el 98 % llamando «dos» a todo. Por eso se informa aparte del
    acierto SOBRE LA CLASE MINORITARIA, que es la única cifra que
    contesta a la pregunta de la tanda.
    """
    def best_cut(sample):
        values = sorted({r[key] for r in sample if r[key] is not None})
        best = None
        for cut in values:
            errors = sum(1 for r in sample if r[key] is not None
                         and (r["label"] == PRINTED_1) != (r[key] <= cut))
            if best is None or errors < best[0]:
                best = (errors, cut)
        return best[1] if best else 0

    errors, errors_on_1, total_1 = 0, 0, 0
    for index, row in enumerate(rows):
        if row[key] is None:
            continue
        cut = best_cut(rows[:index] + rows[index + 1:])
        predicted = PRINTED_1 if row[key] <= cut else PRINTED_2
        if row["label"] == PRINTED_1:
            total_1 += 1
            if predicted != row["label"]:
                errors_on_1 += 1
        if predicted != row["label"]:
            errors += 1
    return {"errors": errors, "n": len(rows),
            "errors_on_printed_1": errors_on_1, "n_printed_1": total_1,
            "accuracy_is_majority_baseline": errors_on_1 == total_1,
            "note": ("con una clase de dos ejemplares, acertar el 98% es "
                     "llamar «dos» a todo: mírese `errors_on_printed_1`")}


def by_book(rows: List[dict]) -> dict:
    out = {}
    for book in sorted({row["book"] for row in rows if row["book"]}):
        sample = [row for row in rows if row["book"] == book]
        widths = [row["width"] for row in sample]
        out[book] = {
            "n": len(sample),
            "printed_1": sum(1 for r in sample if r["label"] == PRINTED_1),
            "printed_2": sum(1 for r in sample if r["label"] == PRINTED_2),
            "width_median": statistics.median(widths),
            "width_min": min(widths), "width_max": max(widths),
        }
    return out


def by_page(rows: List[dict]) -> dict:
    pages = collections.defaultdict(list)
    for row in rows:
        pages[row["page"]].append(row["width"])
    medians = [statistics.median(v) for v in pages.values()]
    return {
        "pages": len(pages),
        "page_median_width_min": min(medians) if medians else None,
        "page_median_width_max": max(medians) if medians else None,
        "note": ("la escala cambia de plana a plana; se informa para poder "
                 "juzgar si normalizar cerraría el solape, y no lo cierra"),
    }


def verdict(rows: List[dict]) -> dict:
    """Si el ancho puede sostener una recuperación futura, y por qué no."""
    width = overlap(rows, "width")
    band = abstention_band(rows, "width")
    safe = bool(width.get("separable")) or bool(
        band.get("exists") and not band.get("useless_for_printed_1"))
    return {
        "safe_for_future_runtime": safe,
        "reason": ("las dos clases se solapan y la única banda sin "
                   "inversiones no resuelve ni un solo caso de 1 impreso: "
                   "sabe decir «dos» y nada más"
                   if not safe else "separación reproducible"),
        "width_separable": width.get("separable"),
        "abstention_band": band,
    }
