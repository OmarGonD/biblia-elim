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

def umbral_relativo(alturas):
    """
    Altura de referencia medida en la propia página. El cuerpo va siempre
    arriba, así que el tercio superior lo da; las notas de esta edición se
    componen a un 87 % de esa altura.
    """
    if len(alturas) < 6:
        return 0.0
    arriba = alturas[:max(4, len(alturas) // 3)]
    return sorted(arriba)[len(arriba) // 2] * 0.93

def punto_de_corte(lineas, recuperar=False):
    """Un solo cambio cuerpo->notas: maximiza (grandes arriba)+(pequeñas abajo)."""
    if len(lineas) < 4: return len(lineas)
    alturas = [med_h(l) for l in lineas]

    def corte_con(u):
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
        return best_k

    # El umbral fijo es el que mejor alinea: probado contra la Vulgata,
    # 1205 capítulos en su sitio frente a 1165-1185 midiendo en la página.
    # Pero en las hojas compuestas con letra algo menor da la página
    # entera por notas y el capítulo se pierde -- Ezequiel 1, Daniel 1 --.
    #
    # Bajar el umbral para todos sale caro: recupera esas páginas y
    # descoloca otras siete. Así que el pipeline general se queda con el
    # umbral fijo, y solo rescatar.py -- que actúa sobre páginas
    # comprobadas a mano contra el facsímil -- pide recuperar=True.
    k = corte_con(56)
    if k == 0 and recuperar:
        rel = umbral_relativo(alturas)
        if rel:
            k = corte_con(rel)
    return k

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
