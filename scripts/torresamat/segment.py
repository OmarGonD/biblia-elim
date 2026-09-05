"""Segmenta cada pliego en: cabecera / cuerpo / notas, por página del libro."""
import statistics, re
from load import X1, X2, TOP, BOT, CONF, TXT

def agrupar_lineas(ws):
    """Agrupa palabras en líneas por solapamiento vertical de la caja."""
    ws = sorted(ws, key=lambda w: w[TOP])
    out, cur = [], []
    for w in ws:
        if not cur:
            cur = [w]; continue
        cb = statistics.median(x[BOT] for x in cur)
        ct = statistics.median(x[TOP] for x in cur)
        if w[TOP] < cb - (cb - ct) * 0.45:
            cur.append(w)
        else:
            out.append(cur); cur = [w]
    if cur: out.append(cur)
    return [sorted(l, key=lambda w: w[X1]) for l in out]

def med_h(line):
    return statistics.median(w[BOT] - w[TOP] for w in line)

def med_y(line):
    return statistics.median(w[TOP] for w in line)

def texto(line):
    return " ".join(w[TXT] for w in line)

def canal(page):
    """Centro de la canal entre las dos páginas del pliego."""
    ws = page["words"]
    if not ws: return page["w"] // 2
    W = page["w"]
    lo, hi = int(W * 0.40), int(W * 0.60)
    hist = [0] * (W // 50 + 2)
    for w in ws:
        for b in range(w[X1] // 50, min(w[X2] // 50, len(hist) - 1) + 1):
            hist[b] += 1
    mejor, mval = W // 2, None
    for b in range(lo // 50, hi // 50):
        v = sum(hist[max(0, b - 2):b + 3])
        if mval is None or v < mval:
            mval, mejor = v, b * 50
    return mejor

def recortar_margen(lineas):
    """Quita el ruido de la lámina vecina: palabras fuera del bloque de texto."""
    if not lineas: return lineas
    anchos = [l[-1][X2] for l in lineas if len(l) >= 5]
    izqs   = [l[0][X1]  for l in lineas if len(l) >= 5]
    if not anchos: return lineas
    der = statistics.median(anchos); izq = statistics.median(izqs)
    tol = (der - izq) * 0.06
    out = []
    for l in lineas:
        keep = [w for w in l if w[X1] >= izq - tol * 2 and w[X2] <= der + tol]
        if keep: out.append(keep)
    return out

def punto_de_corte(lineas):
    """Un solo cambio cuerpo->notas: maximiza (grandes arriba)+(pequeñas abajo)."""
    if len(lineas) < 4: return len(lineas)
    grande = [1 if med_h(l) >= 56 else 0 for l in lineas]
    n = len(grande)
    sufijo_peq = [0] * (n + 1)
    for i in range(n - 1, -1, -1):
        sufijo_peq[i] = sufijo_peq[i + 1] + (1 - grande[i])
    mejor, best_k = -1, n
    acc = 0
    for k in range(n + 1):
        score = acc + sufijo_peq[k]
        if score > mejor: mejor, best_k = score, k
        if k < n: acc += grande[k]
    return best_k

RE_CABECERA = re.compile(r"CAP[IÍ]TULO|^\s*\d{1,3}\s")

def media_pagina(ws, page_h):
    """Devuelve (cuerpo, notas) como listas de líneas para media hoja."""
    lineas = agrupar_lineas(ws)
    if not lineas: return [], []
    lineas = recortar_margen(lineas)
    if not lineas: return [], []
    # fuera la cabecera: primera línea si está muy arriba y trae el nombre corrido
    if med_y(lineas[0]) < page_h * 0.09:
        lineas = lineas[1:]
    if not lineas: return [], []
    k = punto_de_corte(lineas)
    return lineas[:k], lineas[k:]

def pliego(page):
    """Devuelve [(cuerpo, notas), (cuerpo, notas)] -- página izquierda y derecha."""
    g = canal(page); h = page["h"]
    izq = [w for w in page["words"] if w[X2] <= g]
    der = [w for w in page["words"] if w[X1] >  g]
    return [media_pagina(izq, h), media_pagina(der, h)]
