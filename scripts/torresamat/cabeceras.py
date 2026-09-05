"""Extrae la cabecera corrida de cada media hoja, normalizada."""
import re, unicodedata
from load import X1, X2, TOP, BOT, CONF, TXT
from segment import agrupar_lineas, med_y, texto, canal, recortar_margen

def sin_tildes(s):
    s = unicodedata.normalize("NFD", s)
    return "".join(c for c in s if unicodedata.category(c) != "Mn")

def normaliza_cabecera(s):
    s = sin_tildes(s).upper()
    s = re.sub(r"CAP[IT]{1,2}[UO]L[O0].*$", " ", s)   # "CAPITULO XVII. 24"
    s = re.sub(r"[^A-Z ]", " ", s)
    # fuera las palabras de una o dos letras que son basura de OCR,
    # salvo los ordinales romanos del nombre del libro
    fuera = {"AL", "EL", "LA", "DE", "DEL", "LOS", "LAS", "SE", "UN"}
    toks = []
    for w in s.split():
        if w in ("I", "II", "III", "IV", "V"):
            toks.append(w)
        elif len(w) >= 3 and w not in fuera:
            toks.append(w)
    return " ".join(toks).strip()

def cabecera_de(ws, page_h):
    """
    Devuelve (texto de la cabecera corrida, líneas de la página sin ella).

    Para quitarla del cuerpo hay que ser estricto -- una línea de más que
    se coma es texto perdido --, pero para leer de ella el nombre del libro
    conviene mirar toda la banda de arriba: la cabecera se parte, se tuerce
    y se mezcla con el ruido del canto, y exigiendo que fuese justo la
    primera línea se quedaban sin identificar dos de cada tres páginas.
    """
    ls = agrupar_lineas(ws)
    if not ls:
        return "", []
    banda = " ".join(texto(l) for l in ls if med_y(l) < page_h * 0.16)
    cab = normaliza_cabecera(banda)
    ls = recortar_margen(ls)
    if not ls:
        return cab, []
    if med_y(ls[0]) < page_h * 0.09:
        return cab, ls[1:]
    return cab, ls

def medias_hojas(page):
    """[(cabecera, lineas_restantes), ...] izquierda y derecha."""
    g = canal(page)
    out = []
    for ws in ([w for w in page["words"] if w[X2] <= g],
               [w for w in page["words"] if w[X1] > g]):
        if not ws:
            out.append(("", []))
        else:
            out.append(cabecera_de(ws, page["h"]))
    return out
