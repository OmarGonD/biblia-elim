"""Detección de encabezados de capítulo en el cuerpo."""
import re
from cabeceras import sin_tildes

ROMANO = {"I":1,"V":5,"X":10,"L":50,"C":100,"D":500,"M":1000}

def de_romano(s):
    s = s.upper()
    if not s or any(c not in ROMANO for c in s):
        return None
    tot, prev = 0, 0
    for c in reversed(s):
        v = ROMANO[c]
        tot += -v if v < prev else v
        prev = max(prev, v)
    return tot or None

ORDINALES = {
    "PRIMERO": 1, "PRIMERA": 1, "SEGUNDO": 2, "TERCERO": 3, "CUARTO": 4,
    "QUINTO": 5, "SEXTO": 6, "SEPTIMO": 7, "OCTAVO": 8, "NOVENO": 9, "DECIMO": 10,
    "UNICO": 1,
}

# el OCR confunde estas letras con romanos
ARREGLOS = str.maketrans({"1":"I","l":"I","|":"I","!":"I","0":"O","5":"V","U":"V","T":"I"})

# En el Salterio el encabezado no dice "CAPITULO" sino "SALMO VIII". Sin
# esta variante los salmos se fundían unos con otros -- doce de las
# veintidós lagunas del texto eran salmos -- porque la única frontera que
# quedaba era el reinicio de la numeración, y los salmos empiezan a
# menudo en el versículo 2 o 3, con el título ocupando el 1.
RE_CAP = re.compile(
    r"^\s*(?:CAP[IT]{1,2}[UO]?L[O0]|P?SALM[O0])\s+([A-Z0-9|!lI.]{1,12})", re.I)

def encabezado_capitulo(linea_txt):
    """Devuelve el número de capítulo si la línea es un encabezado, si no None."""
    t = sin_tildes(linea_txt).strip()
    t = re.sub(r"\s+", " ", t)
    m = RE_CAP.match(t)
    if not m:
        return None
    resto = t[m.start(1):].strip()
    pal = resto.split()[0].strip(".,:;") if resto.split() else ""
    if pal.upper() in ORDINALES:
        return ORDINALES[pal.upper()]
    cand = pal.translate(ARREGLOS).upper().strip(".,:;")
    # Nada de tirar los caracteres que sobran: "Salmo de David" dejaba la
    # D de "de" y devolvía el capítulo 500. Si queda algo que no es cifra
    # romana, no era un encabezado.
    if not cand or any(ch not in ROMANO for ch in cand):
        return None
    n = de_romano(cand)
    return n if n and n <= 150 else None
