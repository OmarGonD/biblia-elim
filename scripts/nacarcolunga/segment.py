"""Segmenta cada página: dos columnas, cabecera, cuerpo, notas."""
import statistics

from load import X1, X2, TOP, BOT, TXT


def agrupar_lineas(ws):
    ws = [w for w in ws if w[BOT] - w[TOP] >= 8]
    ws = sorted(ws, key=lambda w: w[TOP])
    out, cur = [], []
    for w in ws:
        if not cur:
            cur = [w]
            continue
        cb = statistics.median(x[BOT] for x in cur)
        ct = statistics.median(x[TOP] for x in cur)
        if w[TOP] < cb - (cb - ct) * 0.45:
            cur.append(w)
        else:
            out.append(cur)
            cur = [w]
    if cur:
        out.append(cur)
    return [sorted(l, key=lambda w: w[X1]) for l in out]


def med_h(line):
    return statistics.median(w[BOT] - w[TOP] for w in line)


def med_y(line):
    return statistics.median(w[TOP] for w in line)


def texto(line):
    return " ".join(w[TXT] for w in line)


def canal(page):
    """Centro del medianil entre las dos columnas."""
    ws = page["words"]
    if not ws:
        return page["w"] // 2
    W = page["w"]
    lo, hi = int(W * 0.38), int(W * 0.62)
    paso = 40
    hist = [0] * (W // paso + 2)
    for w in ws:
        for b in range(w[X1] // paso, min(w[X2] // paso, len(hist) - 1) + 1):
            hist[b] += 1
    mejor, mval = W // 2, None
    for b in range(lo // paso, hi // paso):
        v = sum(hist[max(0, b - 2):b + 3])
        if mval is None or v < mval:
            mval, mejor = v, b * paso
    return mejor


def columnas(page):
    """Una o dos columnas, según el medianil separe de verdad.

    Las páginas de tesseract ya se cortaron al 48 % al OCR; hay que
    reutilizar ese corte. Recalcularlo sobre las cajas nuevas lo desplaza
    y vuelve a mezclar las columnas (Mateo 5:33 → «fué mentos»).
    """
    ws = [w for w in page["words"] if w[BOT] - w[TOP] >= 8]
    n = len(ws)
    if n < 40:
        return [ws]
    g = page.get("canal") or canal(page)
    izq = [w for w in ws if w[X2] <= g]
    der = [w for w in ws if w[X1] > g]
    if len(izq) < n * 0.18 or len(der) < n * 0.18:
        return [ws]
    return [izq, der]


def recortar_margen(lineas):
    if not lineas:
        return lineas
    anchos = [l[-1][X2] for l in lineas if len(l) >= 4]
    izqs = [l[0][X1] for l in lineas if len(l) >= 4]
    if not anchos:
        return lineas
    der = statistics.median(anchos)
    izq = statistics.median(izqs)
    tol = max(30, (der - izq) * 0.08)
    out = []
    for l in lineas:
        keep = [w for w in l if w[X1] >= izq - tol * 2 and w[X2] <= der + tol]
        if keep:
            out.append(keep)
    return out


def punto_de_corte(lineas):
    """Un solo cambio cuerpo→notas: maximiza grandes arriba + pequeñas abajo.

    En esta edición el cuerpo anda por 47 px y las notas por 41 (el 87 %),
    así que el umbral se mide en la propia columna: la mediana del tercio
    de arriba por 0.90. Un umbral fijo se come la poesía (letra un poco
    más pequeña a mitad de página) o deja las notas pegadas al cuerpo.
    """
    if len(lineas) < 6:
        return len(lineas)
    alturas = [med_h(l) for l in lineas]
    arriba = alturas[:max(4, len(alturas) // 3)]
    u = sorted(arriba)[len(arriba) // 2] * 0.90
    grande = [1 if h >= u else 0 for h in alturas]
    n = len(grande)
    suf = [0] * (n + 1)
    for i in range(n - 1, -1, -1):
        suf[i] = suf[i + 1] + (1 - grande[i])
    mejor, best_k, acc = -1, n, 0
    for k in range(n + 1):
        s = acc + suf[k]
        if s > mejor:
            mejor, best_k = s, k
        if k < n:
            acc += grande[k]
    # Si el corte deja casi todo como nota, no era una página con aparato.
    if best_k < max(3, n // 5):
        return n
    return best_k


def cuerpo_y_notas(ws):
    lineas = recortar_margen(agrupar_lineas(ws))
    if not lineas:
        return [], []
    k = punto_de_corte(lineas)
    return lineas[:k], lineas[k:]
