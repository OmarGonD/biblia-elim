"""
Parte las palabras que el OCR pegó y se le escaparon al corrector.

El corrector de pasada.py exige que las otras Biblias traigan la pareja
en el mismo versículo. Torres Amat tiene frases suyas --"Que con toda
verdad os digo", "Yo os digo"-- y ahí el testigo no acompaña. También
se negaba a partir "Yoos" porque "os" es enclítico, regla pensada para
el verbo ("criólos"), no para el pronombre sujeto.

Esta pasada pregunta al propio texto: si "yo os" ya sale cientos de
veces bien separado y "Yoos" no es palabra de las otras Biblias, se
parte. La pareja tiene que ser más frecuente que la forma pegada, para
no abrir "contraer" en "con traer" ni "supuesto" en "su puesto".
"""
import collections, re, unicodedata
from corrector import RE_TOK, ENCLITICOS, sin_tildes

# Prefijos que el OCR pega. Las de una letra solo se admiten si son
# éstas: "álos" es "á los", no una partición salvaje.
FUNC = {
    "porque", "cuando", "donde", "quien", "quienes", "aunque", "tambien",
    "también", "despues", "después", "mientras", "siempre", "nunca",
    "tampoco", "entonces", "ahora", "para", "por", "con", "sin",
    "los", "las", "del", "una", "unos", "unas", "sus", "nos", "les",
    "mis", "tus", "que", "qué", "yo", "tú", "tu", "si", "sí", "ni", "no",
    "de", "en", "el", "él", "la", "un", "se", "te", "me", "le", "os",
    "al", "su", "mi", "ya", "mas", "más", "lo", "es", "ha", "he", "hay",
    "fue", "fué", "son", "ser", "á", "a", "y", "o", "ó", "e", "é",
    "este", "esta", "esto", "ese", "esa", "eso", "aquel", "aquella",
    "todo", "toda", "todos", "todas", "otro", "otra", "muy", "tan",
    "pues", "asi", "así", "cual", "cuál", "como", "cómo", "hacia",
    "hasta", "entre", "sobre", "bajo", "ante", "tras", "cada", "pero",
    "aun", "aún", "solo", "sólo", "tal", "he", "has", "han", "hemos",
    "sois", "era", "será", "sido", "ello", "ella", "ellos", "ellas",
}
# Una letra solo: la preposición á/ó. "y" y "e" se admiten más abajo
# cuando el resto es una palabra larga ("Y arrojándose", "E inmediatamente").
FUNC_1 = {"á", "ó"}

# "sies" es el subjuntivo arcaico de ser y también el "si es" pegado.
# "sino" y "Sila" se resuelven por contexto en aplicar(), no aquí.

# Palabras reales, nombres, o erratas de otra clase (y/v, ll/El) que
# el partidor no debe abrir.
NO_PARTIR = {
    "demuestro", "yerá", "yerán", "yeran",
    "elelos", "elamó", "elamo", "elamor",
    "yallado", "siselos", "elle", "tual", "esele",
    "deal", "deme", "mien", "elos", "semelas",
    "aqué", "aque",  # "aquí"/"aquél" del OCR, no "a qué"
}

# Tras "sino" estas palabras delatan la conjunción ("no X sino Y"), no
# el "si no" condicional.
SINO_SIGUE_CONJ = {
    "que", "á", "a", "para", "el", "la", "los", "las", "un", "una", "de",
    "en", "por", "con", "al", "del", "su", "sus", "lo", "le", "se", "me",
    "te", "nos", "os", "mas", "más", "tambien", "también", "porque",
    "cuando", "como", "quien", "cual", "este", "esta", "estos", "estas",
    "todo", "toda", "todos", "todas", "otro", "otra", "otros", "otras",
    "aquel", "aquella", "uno", "unos", "unas", "mi", "tu", "tus", "mis",
    "él", "ella", "ellos", "ellas", "yo", "tú", "nosotros", "vosotros",
    "antes", "despues", "después", "hasta", "entre", "sobre", "sin",
    "contra", "hacia", "según", "durante", "mediante", "so", "cabe",
}

# Delante de "si no" condicional suele haber nexo de cláusula.
SINO_ANTES_COND = {
    "que", "y", "mas", "más", "pero", "porque", "si", "cuando", "aunque",
    "como", "pues",
}

# Tras "sies" / "sila", el artículo o posesivo delata "si es" / "si la".
SIES_SIGUE = {
    "tu", "tú", "su", "el", "la", "un", "una", "mi", "mis", "tus", "sus",
    "este", "esta", "estos", "estas", "eso", "esa", "aquel", "aquella",
    "todo", "toda", "los", "las", "otro", "otra", "algún", "algun",
    "alguna", "vuestro", "vuestra", "nuestro", "nuestra",
}


def _clave(a, b):
    return (sin_tildes(a.lower()), sin_tildes(b.lower()))


def bigramas_de(textos):
    """Conteo de parejas seguidas ya separadas en el propio texto."""
    c = collections.Counter()
    for t in textos:
        ws = [sin_tildes(w.lower()) for w in RE_TOK.findall(t)]
        c.update(zip(ws, ws[1:]))
    return c


def vocabulario_testigos(testigos):
    voc = set()
    for d in testigos.values():
        for t in d.values():
            for w in RE_TOK.findall(t):
                voc.add(w.lower())
                voc.add(sin_tildes(w.lower()))
    return voc


def _es_func(p):
    # Sin quitar tildes: "Yá" no es la conjunción "ya", es "Y"+"á".
    return p.lower() in FUNC


def _cortes(pal):
    """Índices donde se puede cortar, respetando las de una letra."""
    n = len(pal)
    out = []
    for i in range(1, n - 1):
        a = pal[:i]
        if i == 1 and a.lower() not in FUNC_1 and a.lower() not in {"y", "e", "a"}:
            continue
        out.append(i)
    return out


def _es_palabra(p, voc):
    q = p.lower()
    return q in voc or sin_tildes(q) in voc or _es_func(p)


def _prefijo_ok(a, b):
    """El primer trozo tiene que ser funcional, no un recorte de nombre."""
    # "a qué" abre "aqué" (aquí/aquél). La "a" suelta solo vale
    # delante de resto largo: "A lo que contestó".
    if len(a) == 1 and a.lower() == "a" and len(b) < 5:
        return False
    if _es_func(a):
        return True
    al = a.lower()
    if al in FUNC_1:
        return True
    # conjunción y/e delante de palabra larga: "Y arrojándose"
    if al in {"y", "e", "a"} and len(b) >= 5:
        return True
    return False


def parte_corpus(pal, big, voc, frec_pegada, k=2):
    """
    pal -> "a b" o "a b c", o None.

    Hace falta:
      - que pal no sea palabra de las otras Biblias
      - que el primer trozo sea palabra funcional (el OCR pega "Que"+"con",
        no parte morfología)
      - que cada pareja seguida sea más frecuente en el texto ya separado
        que la forma pegada: así "por lo" gana a "Porlo" y "con traer"
        no gana a "contraer".
    """
    if len(pal) < 4:
        return None
    low = pal.lower()
    st = sin_tildes(low)
    # Sila/Silo son topónimos; la minúscula "sila" sí es "si la".
    if (low in voc or st in voc) and not (st == "sila" and pal[0].islower()) \
            and st != "sies":
        return None

    freq_w = max(frec_pegada.get(low, 0), 1)
    umbral = freq_w * k

    def vale_par(a, b):
        if not a or not b:
            return False
        if not _es_palabra(b, voc) and not _es_func(b):
            return False
        bl = b.lower()
        if bl in ENCLITICOS and not _es_func(a) and a.lower() not in FUNC_1 \
                and a.lower() not in {"y", "e"}:
            return False
        if len(b) < 2 and b.lower() not in FUNC_1:
            return False
        return big.get(_clave(a, b), 0) > umbral

    dos = []
    tres = []
    for i in _cortes(pal):
        a, b = pal[:i], pal[i:]
        if not _prefijo_ok(a, b):
            continue
        if vale_par(a, b):
            dos.append([a, b])
        # Tres (o cuatro) trozos solo si el resto entero NO es ya una
        # palabra: "Deella" es "De ella", no "De el la"; "Enelaño" no
        # tiene "elaño" y sí se parte en tres. "Aloquecontestó" pide
        # cuatro: "A lo que contestó".
        if _es_palabra(b, voc):
            continue
        for j in range(1, max(1, len(b) - 1)):
            if j == 1 and b[:1].lower() not in FUNC_1:
                continue
            m, rest = b[:j], b[j:]
            if vale_par(a, m) and vale_par(m, rest):
                tres.append([a, m, rest])
            if _es_palabra(rest, voc):
                continue
            for k in range(1, max(1, len(rest) - 1)):
                if k == 1 and rest[:1].lower() not in FUNC_1:
                    continue
                m2, c = rest[:k], rest[k:]
                if vale_par(a, m) and vale_par(m, m2) and vale_par(m2, c):
                    tres.append([a, m, m2, c])

    if not dos and not tres:
        return None

    def clave_ord(p):
        pares = [_clave(p[i], p[i + 1]) for i in range(len(p) - 1)]
        minimo = min(big.get(pr, 0) for pr in pares)
        # Prefijo funcional más largo primero ("Ya que" gana a "Y a que",
        # "porque al" gana a "por que al"), y a igualdad la pareja más
        # frecuente.
        return (-len(p[0]), -minimo, -(len(p) - 1))

    salidas = dos + tres
    salidas.sort(key=clave_ord)
    mejor = salidas[0]
    # Empate verdadero entre dos particiones distintas: no arriesgar.
    if len(salidas) > 1 and clave_ord(salidas[0]) == clave_ord(salidas[1]) \
            and salidas[0] != salidas[1]:
        return None
    return " ".join(mejor)


def sino_condicional(prev, nxt):
    """True si este "sino" es el "si no" condicional, no la conjunción."""
    if not nxt:
        return False
    n = sin_tildes(nxt.lower())
    if n in SINO_SIGUE_CONJ:
        return False
    p = sin_tildes(prev.lower()) if prev else ""
    # 2.ª persona o subjuntivo: amais, quereis, tuviereis, fuere...
    if re.search(r"(ais|eis|áis|éis|areis|ereis|aréis|eréis|iréis)$", n):
        return True
    if n in {"fuere", "fuera", "tuviere", "hubiere", "hiciere",
             "quiere", "quieres", "quereis", "queréis", "tienes", "teneis",
             "tenéis", "sois", "haceis", "hacéis", "amais", "amáis",
             "hubiera", "lava", "escaparon"}:
        return True
    if p in SINO_ANTES_COND and len(n) >= 4:
        return True
    return False


def sies_si_es(nxt):
    if not nxt:
        return False
    return sin_tildes(nxt.lower()) in SIES_SIGUE


def tokens_con_vecinos(texto):
    toks = list(RE_TOK.finditer(texto))
    out = []
    for i, m in enumerate(toks):
        prev = toks[i - 1].group(0) if i else ""
        nxt = toks[i + 1].group(0) if i + 1 < len(toks) else ""
        out.append((m, prev, nxt))
    return out


def aplicar(texto, big, voc, frec_pegada):
    """Devuelve (nuevo_texto, lista de (pal, arreglo))."""
    cambios = []
    piezas = []
    pos = 0
    for m, prev, nxt in tokens_con_vecinos(texto):
        pal = m.group(0)
        piezas.append(texto[pos:m.start()])
        pos = m.end()
        arreglo = None
        low = pal.lower()
        st = sin_tildes(low)
        if st in NO_PARTIR or low in NO_PARTIR:
            piezas.append(pal)
            continue
        # "hijo de Semelas": nombre propio, no "Se me las".
        if pal[0].isupper() and prev and sin_tildes(prev.lower()) in {"de", "del"}:
            piezas.append(pal)
            continue
        # "Hasido decretado" es "Ha sido", no el "Has ido" del prefijo más largo.
        if st == "hasido":
            arreglo = "Ha sido" if pal[0].isupper() else "ha sido"
        elif st == "sino" or low == "sino":
            if sino_condicional(prev, nxt):
                arreglo = "Si no" if pal[0].isupper() else "si no"
        elif st == "sies" or low == "sies":
            if sies_si_es(nxt):
                arreglo = "Si es" if pal[0].isupper() else "si es"
        elif st == "sila" and pal[0].islower():
            # Sila/Silo son topónimos; la minúscula es "si la".
            arreglo = "si la"
        if arreglo is None:
            arreglo = parte_corpus(pal, big, voc, frec_pegada)
        if arreglo and arreglo != pal:
            cambios.append((pal, arreglo))
            piezas.append(arreglo)
        else:
            piezas.append(pal)
    piezas.append(texto[pos:])
    return "".join(piezas), cambios
