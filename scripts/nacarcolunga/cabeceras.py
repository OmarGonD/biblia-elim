"""Lee la cabecera corrida de cada página (libro y, a veces, capítulo)."""
import re

from load import BOT, TOP
from nombres import candidatos, sin_tildes
from segment import agrupar_lineas, med_y, texto


def normaliza(s):
    s = sin_tildes(s).upper()
    s = re.sub(r"[^A-Z0-9 ]", " ", s)
    fuera = {"AL", "EL", "LA", "DE", "DEL", "LOS", "LAS", "SE", "UN",
             "SANTO", "LIBRO", "LIBROS", "PAGS"}
    toks = []
    for w in s.split():
        if w in ("I", "II", "III", "IV", "1", "2", "3"):
            toks.append(w)
        elif len(w) >= 3 and w not in fuera:
            toks.append(w)
    return " ".join(toks).strip()


RE_SOLO_NUM = re.compile(r"^[\dIVXLC\s.,;:]+$", re.I)
RE_CAP_EN_CAB = re.compile(r"\b(\d{1,3})\b")


def es_linea_cabecera(t):
    """¿Esta línea es la cabecera corrida y no el arranque del cuerpo?

    No se puede recortar por altura: en esta edición el cuerpo empieza
    a veces al 6 % de la página, más arriba que el umbral que serviría
    para quitar 'GÉNESIS, 3'. Hay que mirar el contenido.
    """
    t = t.strip()
    if not t:
        return True
    if RE_SOLO_NUM.match(t) and len(t) <= 16:
        return True
    n = normaliza(t)
    if not n:
        return True
    toks = n.split()
    if len(toks) <= 6 and candidatos(n, _TODO):
        return True
    return False


def _caps_en(s):
    nums = []
    for m in RE_CAP_EN_CAB.finditer(s):
        n = int(m.group(1))
        if 1 <= n <= 150:
            nums.append(n)
    return nums[:3]


def _limpia_ocr_cab(s):
    if not s:
        return s
    s = s.replace("LSAN", "I SAN").replace("IISAN", "II SAN")
    s = s.replace("IIISAN", "III SAN")
    s = re.sub(r"SANJUAN", "SAN JUAN", s, flags=re.I)
    return s


def cabecera_pagina(page, ambito):
    """(cands OSIS, [capítulos mencionados], texto de cabecera).

    En el facsímil de Princeton el título va en rojo: se prefiere
    cabecera_roja / cabecera guardadas en el pickle. Si no, la franja
    de arriba de las cajas (OCR del PDF, que mezcla el v. 1).
    """
    for raw in (page.get("cabecera_roja"), page.get("cabecera")):
        raw = _limpia_ocr_cab(raw or "")
        cab = normaliza(raw)
        cands = candidatos(cab, ambito)
        if cands:
            caps = [n for n in _caps_en(cab) if n <= 150]
            if len(caps) >= 2 and caps[-1] > 80 and caps[0] <= 80:
                caps = caps[:-1]
            return cands, caps, cab
    hlim = page["h"] * 0.07
    banda = [w for w in page["words"]
             if w[TOP] < hlim and (w[BOT] - w[TOP]) >= 12]
    ls = [l for l in agrupar_lineas(banda) if es_linea_cabecera(texto(l))]
    raw = " ".join(texto(l) for l in ls[:3])
    cab = normaliza(raw)
    cands = candidatos(cab, ambito)
    # Capítulo: el número pequeño que acompaña al nombre. El de página
    # (hasta ~1500) se descarta; si quedan dos (GÉNESIS, 6, 7) se
    # conservan los dos.
    caps = [n for n in _caps_en(cab) if n <= 150]
    if len(caps) >= 2 and caps[-1] > 80 and caps[0] <= 80:
        caps = caps[:-1]
    return cands, caps, cab


def quita_cabecera(lineas, page_h):
    if not lineas:
        return lineas
    i = 0
    while i < min(3, len(lineas)):
        if med_y(lineas[i]) < page_h * 0.12 and es_linea_cabecera(texto(lineas[i])):
            i += 1
            continue
        break
    return lineas[i:]


# Se rellena al importar construir; aquí un default amplio para
# es_linea_cabecera cuando se usa suelta.
from canon import ORDEN as _TODO
_TODO = set(_TODO)
