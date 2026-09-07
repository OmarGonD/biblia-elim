"""
Parte los versículos que el OCR fundió en el anterior.

Cuando se come el número, el texto del N acaba pegado al N-1 y el N
queda vacío: Mateo 5:26 trae el 27, el 27 no tiene nada, y las flechas
no avanzan. El corte válido es casi siempre una puntuación seguida de
mayúscula ("…maravedí. Habeis oido…"). Se acepta solo si la Platense
--misma versificación-- reconoce el trozo de la derecha como el versículo
que faltaba.
"""
import re
from corrector import RE_TOK, sin_tildes

STOP = {
    "el", "la", "los", "las", "un", "una", "de", "del", "al", "a", "y", "e",
    "o", "que", "se", "te", "me", "le", "lo", "nos", "os", "su", "sus", "mi",
    "tu", "por", "con", "sin", "para", "en", "no", "ni", "si", "mas", "ya",
    "es", "son", "fue", "ser", "ha", "he",
}

SYN = {
    "oisteis": "oido", "oido": "oido", "habeis": "oido",
    "dicho": "dicho", "dijo": "dicho",
    "tambien": "tambien", "asimismo": "tambien",
    "juraras": "jurar", "perjuraras": "jurar", "jureis": "jurar",
    "prestado": "prestamo", "prestamo": "prestamo",
    "amaras": "amar", "odio": "odio", "odiaras": "odio",
    "adulterio": "adulterio", "adultera": "adulterio",
    "adultero": "adulterio", "cometeras": "cometer",
}

# Puntuación fuerte + (espacio o número de verso tragado) + mayúscula.
# "maravedí. Habeis oido" y "Aran. 43 Estos que siguen".
RE_CORTE = re.compile(
    r'(?<=[.!?;:])[^\nA-ZÁÉÍÓÚÑ¡¿]{1,12}(?=[A-ZÁÉÍÓÚÑ¡¿])'
    r'|(?<=,)\s+(?=(?:Habeis|Habéis|Yo\s|Ni\s|Mas\s|Más\s|Pero\s|'
    r'Tambien|También|Porque\s|Pues\s|Que\s|Ámad|Amad|No\s))'
)

# El número de versículo, o basura de lámina, que queda en el corte.
RE_COLA = re.compile(
    r'[\s,;:]*(?:\d{1,3}[.)]?|[O0]\.|[/|]|»)+\s*$'
)
RE_CABEZA = re.compile(
    r'^\s*(?:[\d.)(/|,]+\s*)+'
)


def _norm(w):
    w = sin_tildes(w.lower())
    return SYN.get(w, w)


def _toks(s):
    return [_norm(w) for w in RE_TOK.findall(s)]


def _content(ws, minl=4):
    return [w for w in ws if w not in STOP and len(w) >= minl]


def _cover(a, b):
    sa, sb = set(a), set(b)
    return (len(sa & sb) / len(sb)) if sb else 0.0


def puntos_de_corte(s):
    return [m.start() for m in RE_CORTE.finditer(s)]


def mejor_corte(ta, plat_prev, plat_this, umbral=2.40):
    """Posición en ta donde empieza plat_this, o None."""
    pts = puntos_de_corte(ta)
    if not pts:
        return None
    pc = _content(_toks(plat_prev or ""))
    nc = _content(_toks(plat_this or ""))
    if len(nc) < 2:
        return None
    best = None
    for pos in pts:
        left, right = ta[:pos], ta[pos:]
        if len(left.strip()) < 25 or len(right.strip()) < 20:
            continue
        cl, cr = _content(_toks(left)), _content(_toks(right))
        cn = _cover(cr, nc)
        cp = _cover(cl, pc) if pc else 0.5
        leak = _cover(cr, pc) if pc else 0.0
        head = nc[:5]
        hh = sum(1 for h in head if h in cr[:15]) / max(1, len(head))
        if cn < 0.40 or hh < 0.4:
            continue
        score = 2 * cn + cp + 1.5 * hh - leak
        if score < umbral:
            continue
        # Primero el arranque de N (hh), luego la cobertura, luego el
        # corte más a la derecha: el versículo vacío es la cola.
        cand = (hh, cn, pos, score)
        if best is None or cand[:3] > best[:3]:
            best = cand
    return best[2] if best else None


def limpia_lado(s, cola=False):
    s = s.strip()
    if cola:
        s = RE_COLA.sub("", s)
    else:
        s = RE_CABEZA.sub("", s)
    return s.strip(" ,;:")


def partir_vacio(texto, vacio, platense):
    """
    Si vacio está vacío y el anterior trae los dos versos, los parte.
    Devuelve (prev_ref, nuevo_prev, nuevo_vacio) o None.
    """
    osis, cv = vacio.rsplit(" ", 1)
    cap, ver = map(int, cv.split(":"))
    if ver < 2:
        return None
    prev = f"{osis} {cap}:{ver - 1}"
    ta = texto.get(prev)
    pn = platense.get(vacio)
    if not ta or not pn or texto.get(vacio):
        return None
    pos = mejor_corte(ta, platense.get(prev), pn)
    if pos is None:
        return None
    izq = limpia_lado(ta[:pos], cola=True)
    der = limpia_lado(ta[pos:], cola=False)
    if len(izq) < 20 or len(der) < 20:
        return None
    return prev, izq, der


def aplicar(texto, vacios, platense):
    """Devuelve (nuevo_texto, lista de refs partidas)."""
    nuevo = dict(texto)
    hechos = []
    ordenados = sorted(vacios, key=lambda k: (
        k.split()[0],
        int(k.rsplit(" ", 1)[1].split(":")[0]),
        int(k.rsplit(" ", 1)[1].split(":")[1]),
    ))
    for vacio in ordenados:
        r = partir_vacio(nuevo, vacio, platense)
        if not r:
            continue
        prev, izq, der = r
        nuevo[prev] = izq
        nuevo[vacio] = der
        hechos.append(vacio)
    return nuevo, hechos
