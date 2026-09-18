"""
La forma de la TINTA: ¿distingue el 1 impreso del 2 impreso?

    FACSIMILE LABELS THE CLASS. FACSIMILE PIXELS PROVIDE THE FEATURES.

La 129 midió la caja que el reconocimiento asigna al token «a» y terminó
bloqueada: esa caja describe cómo segmentó el OCR, no el contorno de lo
que hay impreso. El mismo 2 impreso salía de 32 a 82 píxeles. Esta tanda
va a la plana y mide la mancha de tinta.

Todo lo que hay aquí es aritmética sobre una máscara de blanco y negro:
contar píxeles, encontrar componentes conexos, sacar el centro de masas.
Ni una línea de aprendizaje automático, y a propósito: la pregunta es si
existe una propiedad FÍSICA SIMPLE Y EXPLICABLE, no si algo la adivina.
Una regla que no se puede escribir en una línea no sirve para lo que
viene después, que es decidir el número de un versículo.

De dónde sale cada cosa, que es lo que hay que poder auditar:

    LA ETIQUETA la pone el facsímil, leída por una persona y escrita en
    `observed_printed_value`. Nunca el hueco que falta, nunca los
    versículos vecinos, nunca el segundo glifo.

    LA LOCALIZACIÓN la da la caja del OCR, y sólo eso: dice DÓNDE mirar.
    Sus dimensiones no entran en ninguna medida -- es justo lo que la 129
    demostró inservible.

    EL RECORTE se cierra por la derecha a medio camino del segundo token
    para que su tinta no entre en la máscara. Es aislamiento físico, no
    lectura: da igual qué glifo sea el segundo, sólo importa dónde
    empieza.
"""
import collections
from typing import Dict, List, Optional, Sequence, Tuple

#: Margen alrededor de la caja del OCR, en píxeles del escaneo. Generoso
#: porque la caja del reconocimiento a veces recorta el glifo, que es
#: parte del problema que traemos de la 129.
CROP_PAD = 14

#: Umbral nominal y la familia con la que se comprueba que una separación
#: no depende de un número mágico. Se aplica igual a las dos clases y a
#: los controles: nada aquí mira la etiqueta.
THRESHOLD_DELTAS = (-20, -10, 0, 10, 20)

#: Un componente por debajo de esta fracción del área del recorte es
#: mota del escaneo. El corte se fija por tamaño relativo y sin mirar
#: etiquetas: borrar «el componente que estropea el 1» sería fabricar el
#: resultado.
MIN_COMPONENT_AREA_RATIO = 0.004


def crop_box(first_bbox: Sequence[int], second_bbox: Sequence[int],
             pad: int = CROP_PAD) -> Tuple[int, int, int, int]:
    """La ventana del recorte, en coordenadas del escaneo.

    Por la derecha se corta a medio camino entre el final del primer
    token y el principio del segundo: así la tinta del segundo dígito no
    puede entrar en la máscara. Se usa su POSICIÓN, no su identidad.
    """
    x0 = first_bbox[0] - pad
    y0 = first_bbox[1] - pad
    y1 = first_bbox[3] + pad
    halfway = (first_bbox[2] + second_bbox[0]) // 2
    x1 = min(first_bbox[2] + pad, halfway)
    if x1 <= x0:
        x1 = first_bbox[2]
    return (x0, y0, x1, y1)


def scale_box(box: Sequence[int], scale_x: float, scale_y: float,
              limits: Tuple[int, int]) -> Tuple[int, int, int, int]:
    """La misma ventana en píxeles de la imagen renderizada."""
    width, height = limits
    return (max(0, int(box[0] * scale_x)), max(0, int(box[1] * scale_y)),
            min(width, int(box[2] * scale_x)), min(height, int(box[3] * scale_y)))


def otsu_threshold(histogram: Sequence[int]) -> int:
    """El umbral de Otsu sobre un histograma de 256 niveles.

    Determinista y sin parámetros: el mismo recorte da siempre el mismo
    umbral, y el procedimiento es idéntico para las dos clases. No se
    ajusta por etiqueta, que es la trampa que haría inservible todo lo
    demás.
    """
    total = sum(histogram)
    if not total:
        return 128
    sum_all = sum(index * count for index, count in enumerate(histogram))
    sum_background, weight_background, best, cut = 0.0, 0, -1.0, 128
    for level in range(256):
        weight_background += histogram[level]
        if not weight_background:
            continue
        weight_foreground = total - weight_background
        if not weight_foreground:
            break
        sum_background += level * histogram[level]
        mean_background = sum_background / weight_background
        mean_foreground = (sum_all - sum_background) / weight_foreground
        between = (weight_background * weight_foreground
                   * (mean_background - mean_foreground) ** 2)
        if between > best:
            best, cut = between, level
    return cut


def binarize(pixels: Sequence[Sequence[int]], threshold: int) -> List[List[int]]:
    """La máscara de tinta: 1 donde hay tinta, 0 donde hay papel.

    El facsímil es tinta oscura sobre papel claro, así que la tinta es lo
    que queda POR DEBAJO del umbral. No se suaviza, no se dilata y no se
    rellena nada: lo que se mide tiene que ser lo que está impreso.
    """
    return [[1 if value <= threshold else 0 for value in row]
            for row in pixels]


def components(mask: Sequence[Sequence[int]],
               min_area: int = 0) -> List[dict]:
    """Los componentes conexos de tinta, con vecindad de ocho.

    Se recorre en anchura con una pila propia, sin recursión: un
    componente grande desbordaría la pila de Python y el fallo aparecería
    sólo en las planas más entintadas.
    """
    if not mask or not mask[0]:
        return []
    height, width = len(mask), len(mask[0])
    seen = [[False] * width for _ in range(height)]
    found = []
    for start_y in range(height):
        for start_x in range(width):
            if not mask[start_y][start_x] or seen[start_y][start_x]:
                continue
            stack, cells = [(start_y, start_x)], []
            seen[start_y][start_x] = True
            while stack:
                y, x = stack.pop()
                cells.append((y, x))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = y + dy, x + dx
                        if 0 <= ny < height and 0 <= nx < width \
                                and mask[ny][nx] and not seen[ny][nx]:
                            seen[ny][nx] = True
                            stack.append((ny, nx))
            if len(cells) < min_area:
                continue
            ys = [cell[0] for cell in cells]
            xs = [cell[1] for cell in cells]
            found.append({
                "area": len(cells), "cells": cells,
                "bbox": (min(xs), min(ys), max(xs) + 1, max(ys) + 1),
            })
    found.sort(key=lambda item: (-item["area"], item["bbox"]))
    return found


def select_component(found: List[dict],
                     anchor: Sequence[int]) -> Optional[dict]:
    """El componente que cae dentro de la caja que dio el reconocimiento.

    `anchor` es esa caja, y se usa SÓLO para elegir cuál de las manchas
    es la que interesa. Sus medidas no se miran: el ancho de la caja del
    OCR fue exactamente lo que la 129 demostró que no vale.
    """
    best, best_overlap = None, 0
    for item in found:
        x0, y0, x1, y1 = item["bbox"]
        overlap_x = max(0, min(x1, anchor[2]) - max(x0, anchor[0]))
        overlap_y = max(0, min(y1, anchor[3]) - max(y0, anchor[1]))
        overlap = overlap_x * overlap_y
        if overlap > best_overlap:
            best, best_overlap = item, overlap
    return best


def holes(component: dict, mask_shape: Tuple[int, int]) -> int:
    """Huecos cerrados dentro del componente.

    Se rellena el fondo desde el borde de la caja del componente: lo que
    quede sin alcanzar y no sea tinta es un hueco. Se informa aunque no
    decida nada -- un 1 y un 2 de esta fundición no tienen ninguno, así
    que se espera que no aporte, y eso también hay que poder enseñarlo.
    """
    x0, y0, x1, y1 = component["bbox"]
    width, height = x1 - x0, y1 - y0
    filled = [[False] * width for _ in range(height)]
    ink = {(cell[0] - y0, cell[1] - x0) for cell in component["cells"]}
    stack = []
    for x in range(width):
        stack.extend([(0, x), (height - 1, x)])
    for y in range(height):
        stack.extend([(y, 0), (y, width - 1)])
    while stack:
        y, x = stack.pop()
        if not (0 <= y < height and 0 <= x < width):
            continue
        if filled[y][x] or (y, x) in ink:
            continue
        filled[y][x] = True
        stack.extend([(y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)])
    seen, count = set(), 0
    for y in range(height):
        for x in range(width):
            if filled[y][x] or (y, x) in ink or (y, x) in seen:
                continue
            count += 1
            stack = [(y, x)]
            while stack:
                cy, cx = stack.pop()
                if not (0 <= cy < height and 0 <= cx < width):
                    continue
                if (cy, cx) in seen or (cy, cx) in ink or filled[cy][cx]:
                    continue
                seen.add((cy, cx))
                stack.extend([(cy + 1, cx), (cy - 1, cx),
                              (cy, cx + 1), (cy, cx - 1)])
    return count


def features(component: dict, all_components: List[dict],
             mask_shape: Tuple[int, int]) -> dict:
    """Las medidas de la mancha. Todas aritmética sobre la máscara.

    Los cocientes de masa van normalizados dentro de la caja de tinta,
    así que no dependen ni del recorte ni de la resolución: son forma, no
    tamaño. El ancho y el alto sí van en píxeles, porque la pregunta de
    la tanda es precisamente si el 1 impreso es más estrecho.
    """
    x0, y0, x1, y1 = component["bbox"]
    width, height = x1 - x0, y1 - y0
    cells = component["cells"]
    area = len(cells)
    sum_x = sum(cell[1] for cell in cells)
    sum_y = sum(cell[0] for cell in cells)
    centre_x = (sum_x / area - x0) / width if width else 0.0
    centre_y = (sum_y / area - y0) / height if height else 0.0
    mid_x, mid_y = x0 + width / 2.0, y0 + height / 2.0
    left = sum(1 for cell in cells if cell[1] < mid_x)
    top = sum(1 for cell in cells if cell[0] < mid_y)
    columns = collections.Counter(cell[1] - x0 for cell in cells)
    rows = collections.Counter(cell[0] - y0 for cell in cells)
    total_area = sum(item["area"] for item in all_components) or 1
    return {
        "ink_bbox_width": width,
        "ink_bbox_height": height,
        "ink_bbox_aspect_ratio": (width / height) if height else None,
        "ink_pixels": area,
        "ink_density": area / (width * height) if width and height else None,
        "centroid_x": round(centre_x, 5),
        "centroid_y": round(centre_y, 5),
        "left_mass_ratio": round(left / area, 5),
        "right_mass_ratio": round((area - left) / area, 5),
        "top_mass_ratio": round(top / area, 5),
        "bottom_mass_ratio": round((area - top) / area, 5),
        "peak_column": (max(columns, key=lambda k: (columns[k], -k))
                        if columns else None),
        "peak_column_ratio": round(
            max(columns.values()) / height, 5) if columns and height else None,
        "peak_row": (max(rows, key=lambda k: (rows[k], -k)) if rows else None),
        "peak_row_ratio": round(
            max(rows.values()) / width, 5) if rows and width else None,
        "components": len(all_components),
        "largest_component_area_ratio": round(area / total_area, 5),
        "holes": holes(component, mask_shape),
    }


#: Las medidas escalares que se estudian una a una. El orden es estable
#: para que el informe salga siempre igual.
SCALAR_FEATURES = (
    "ink_bbox_width", "ink_bbox_height", "ink_bbox_aspect_ratio",
    "ink_pixels", "ink_density", "centroid_x", "centroid_y",
    "left_mass_ratio", "right_mass_ratio", "top_mass_ratio",
    "bottom_mass_ratio", "peak_column_ratio", "peak_row_ratio",
    "largest_component_area_ratio",
)


def overlap(rows: List[dict], key: str) -> dict:
    """¿Se pisan las dos clases en esta medida?

    Se mira en los dos sentidos porque no se sabe de antemano cuál de las
    clases queda por encima: una medida sirve si TODOS los unos caen a un
    lado de TODOS los doses, sea el lado que sea.
    """
    ones = [row["features"][key] for row in rows
            if row["label"] == 1 and row["features"].get(key) is not None]
    twos = [row["features"][key] for row in rows
            if row["label"] == 2 and row["features"].get(key) is not None]
    if not ones or not twos:
        return {"separable": None, "n_printed_1": len(ones),
                "n_printed_2": len(twos)}
    ones_below = max(ones) < min(twos)
    ones_above = min(ones) > max(twos)
    return {
        "n_printed_1": len(ones), "n_printed_2": len(twos),
        "min_printed_1": min(ones), "max_printed_1": max(ones),
        "min_printed_2": min(twos), "max_printed_2": max(twos),
        "separable": ones_below or ones_above,
        "direction": ("printed_1_below" if ones_below else
                      "printed_1_above" if ones_above else None),
        "printed_2_inside_printed_1_range":
            sum(1 for value in twos if min(ones) <= value <= max(ones)),
    }


def zero_inversion_rule(rows: List[dict], key: str) -> dict:
    """La mejor regla de esta medida SIN una sola inversión de clase.

    Se admite una banda de abstención: no decidir es barato, decidir mal
    abre el versículo equivocado. Se informa además de cuántos casos de
    la clase minoritaria quedan resueltos, porque una regla que sólo sabe
    decir «dos» no sirve para nada aquí: lo que hay que decidir es
    justamente el uno.
    """
    values = sorted({row["features"][key] for row in rows
                     if row["features"].get(key) is not None})
    if not values:
        return {"exists": False}
    best = None
    for low in values:
        for high in values:
            if high < low:
                continue
            wrong = 0
            for row in rows:
                value = row["features"].get(key)
                if value is None:
                    continue
                if value <= low and row["label"] != 1:
                    wrong += 1
                if value >= high and row["label"] != 2:
                    wrong += 1
            if wrong:
                continue
            decided = sum(1 for row in rows
                          if row["features"].get(key) is not None
                          and (row["features"][key] <= low
                               or row["features"][key] >= high))
            ones = sum(1 for row in rows if row["label"] == 1
                       and row["features"].get(key) is not None
                       and row["features"][key] <= low)
            candidate = {"exists": True, "low": low, "high": high,
                         "decided": decided, "printed_1_decided": ones}
            if best is None or (candidate["printed_1_decided"],
                                candidate["decided"]) > \
                    (best["printed_1_decided"], best["decided"]):
                best = candidate
    if best is None:
        return {"exists": False}
    total = len(rows)
    best["total"] = total
    best["coverage"] = round(best["decided"] / total, 4) if total else 0.0
    best["abstentions"] = total - best["decided"]
    best["printed_1_total"] = sum(1 for row in rows if row["label"] == 1)
    best["useless_for_printed_1"] = best["printed_1_decided"] == 0
    return best


def verdict(rows: List[dict]) -> dict:
    """¿Hay alguna medida que sostenga una regla útil?

    Útil quiere decir dos cosas a la vez: sin inversiones y capaz de
    decidir al menos un caso de 1 impreso. Sin lo segundo la regla no
    contesta la pregunta de la familia «a*», que es cuándo el glifo vale
    uno.
    """
    useful = []
    for key in SCALAR_FEATURES:
        rule = zero_inversion_rule(rows, key)
        if rule.get("exists") and not rule.get("useless_for_printed_1"):
            useful.append({"feature": key, **rule})
    return {
        "safe_for_runtime_experiment": bool(useful),
        "useful_features": useful,
        "reason": ("ninguna medida de tinta decide un solo caso de 1 impreso "
                   "sin invertir alguna clase" if not useful
                   else "hay al menos una medida con regla sin inversiones"),
    }
