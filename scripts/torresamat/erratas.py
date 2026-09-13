"""
Erratas de letra y basura de lámina que el corrector de pasaje no alcanza.

El de pasada.py no toca la primera ni la última letra -- así "yuestra"
(y/v) y "mi por el cielo" (n/m) se quedan -- y pide que la otra Biblia
traiga la palabra en el mismo versículo, de modo que "jurarels" y
"perezeca" (frases de Torres Amat) tampoco caen.
"""
import re
from corrector import RE_TOK, sin_tildes

# Inequívocas: la forma pegada no es palabra.
PALABRAS = {
    "yuestra": "vuestra", "Yuestra": "Vuestra",
    "jurarels": "jurareis",
    "perezeca": "perezca",
    "aleun": "algun", "Aleun": "Algun",
    "elelos": "cielos", "Elelos": "Cielos",
    "yallado": "vallado",
    "Elamó": "Llamó", "elamó": "llamó",
    "yerá": "verá", "yerán": "verán", "Yerá": "Verá", "Yerán": "Verán",
}

# Primera letra y/v. n/m se deja: "madre" y "naciones" son palabra.
INICIAL = {("y", "v"), ("Y", "V")}

RE_LIMPIA = [
    (re.compile(r"á4"), "á"),                      # "á4 Dios"
    (re.compile(r"Á4"), "Á"),
    (re.compile(r"(?<=\s)4(?=\s+[A-Za-záéíóúñ])"), "á"),
    (re.compile(r"\b0 no\b"), "ó no"),
    (re.compile(r": 0 no"), ": ó no"),
    (re.compile(r"(?<=[a-záéíóúñ])\s+\d{1,2}:(?=\s+[a-záéíóúñ])"), ":"),
    (re.compile(r"\s+\|(?=\s|$)"), ""),
    (re.compile(r"\s+Y(?=[,.])"), ""),             # "tierra Y." "prójimo Y,"
    (re.compile(r"(?<=:\s)mi por\b"), "ni por"),    # "jureis…: mi por el cielo"
    # Catchword / footnote callout glued to the verse (hOCR: low-confidence
    # "o", "//", then a leftover word such as "WMestas." in its own block).
    # Applied only at module rebuild; the renderer does not strip this.
    (re.compile(r",\s+o\s+//\s+\S+\.?\s*$"), "."),
    (re.compile(r"\s{2,}"), " "),
]


def _es_palabra(p, voc):
    q = p.lower()
    return q in voc or sin_tildes(q) in voc


def inicial_yv_nm(pal, voc):
    """yuestra -> vuestra, si el original no es palabra y el candidato sí."""
    if len(pal) < 4:
        return None
    if _es_palabra(pal, voc):
        return None
    a = pal[0]
    for x, y in INICIAL:
        if a != x:
            continue
        cand = y + pal[1:]
        if _es_palabra(cand, voc) and not _es_palabra(pal, voc):
            return cand
    return None


def extra_letra(pal, voc, frec_ta):
    """Solo la letra doblada: cuarrto -> cuarto, Lameech -> Lamech."""
    if len(pal) < 6 or _es_palabra(pal, voc):
        return None
    cands = []
    for i in range(1, len(pal) - 1):
        if pal[i].lower() != pal[i - 1].lower():
            continue
        c = pal[:i] + pal[i + 1:]
        if _es_palabra(c, voc) and frec_ta.get(c.lower(), 0) >= 2:
            cands.append(c)
    return cands[0] if len(set(cands)) == 1 else None


def aplicar_texto(s, voc, frec_ta):
    """Devuelve (nuevo, lista de (de, a))."""
    cambios = []
    t = s
    for pal, arr in PALABRAS.items():
        if pal in t:
            n = t.count(pal)
            t = t.replace(pal, arr)
            cambios.extend([(pal, arr)] * n)
    piezas, pos = [], 0
    for m in RE_TOK.finditer(t):
        pal = m.group(0)
        piezas.append(t[pos:m.start()])
        pos = m.end()
        arr = inicial_yv_nm(pal, voc)
        if arr is None:
            arr = extra_letra(pal, voc, frec_ta)
        if arr and arr != pal:
            cambios.append((pal, arr))
            piezas.append(arr)
        else:
            piezas.append(pal)
    piezas.append(t[pos:])
    t = "".join(piezas)
    for rx, rep in RE_LIMPIA:
        t2 = rx.sub(rep, t)
        if t2 != t:
            cambios.append((rx.pattern, rep))
        t = t2
    return t.strip(), cambios
