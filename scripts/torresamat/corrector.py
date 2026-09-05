"""
Corrige erratas del OCR preguntando a las otras Biblias, versículo a
versículo.

Tres intentos hicieron falta, y los dos primeros están contados porque
explican por qué el tercero es así.

Un diccionario español moderno no sirve: marcaría "crió", "cubrian",
"Egypto" o "tambien" como faltas, y son la ortografía correcta de 1882.

El propio texto como diccionario --lo frecuente es bueno, lo rarísimo es
errata-- arregla mucho, pero no distingue la palabra rara y buena de la
errata, y se le escapan las que se repiten, que son legión porque la
confusión está en la tipografía y no en el azar.

Lo que vale es preguntar a las Biblias ya instaladas. La Reina-Valera de
1909 está a veintisiete años de esta edición y la Platense usa además la
misma versificación Vulgata: se lee el mismo versículo en las dos y se ve
qué palabra pusieron ellas.

Y hay que preguntar versículo a versículo, no de una vez, porque la misma
grafía es errata en un sitio y palabra en otro. "cineo" es errata de
"cinco" en Números 7:41 --la Platense dice ahí "cinco machos cabríos"-- y
es el nombre del pueblo cineo en otros pasajes. Un diccionario global
protege las dos o estropea las dos; el testigo del pasaje acierta en cada
una.
"""
import re, collections, unicodedata

RE_TOK = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+")
RARO = 12
MIN_LARGO = 4

# Fuera quedan a propósito a/o y o/e: son las vocales de género y
# conjugación, y admitirlas convertía "casos" en "casas".
CONFUSIONES = {
    frozenset("li"), frozenset("ce"), frozenset("nm"), frozenset("un"),
    frozenset("yv"), frozenset("tf"), frozenset("BS"), frozenset("JQ"),
    frozenset("rn"), frozenset("hb"), frozenset("IL"),
}

def sin_tildes(s):
    s = unicodedata.normalize("NFD", s)
    return "".join(c for c in s if unicodedata.category(c) != "Mn")

def frecuencias(textos):
    f = collections.Counter()
    for t in textos:
        f.update(RE_TOK.findall(t))
    return f

def _borrados(p):
    return {p[:i] + p[i + 1:] for i in range(len(p))}

def construye_indice(frec, minimo=8):
    idx = collections.defaultdict(set)
    for w, c in frec.items():
        if c < minimo or len(w) < MIN_LARGO:
            continue
        idx[w].add(w)
        for d in _borrados(w):
            idx[d].add(w)
    return idx

def _confundible(a, b):
    if len(a) != len(b):
        return False
    difs = [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y]
    if len(difs) != 1:
        return False
    i, x, y = difs[0]
    if i == 0 or i == len(a) - 1:
        return False
    return (frozenset((x.lower(), y.lower())) in CONFUSIONES
            or frozenset((x, y)) in CONFUSIONES)

def candidatos(pal, frec, idx):
    vistos = set(idx.get(pal, ()))
    for d in _borrados(pal):
        vistos |= idx.get(d, set())
    return {c for c in vistos
            if _confundible(pal, c) and pal[:1].isupper() == c[:1].isupper()}

# Pronombres que en castellano se pegan detrás del verbo. "criólos",
# "echóles", "inspiróle" son una palabra, no dos, y partirlas es
# destrozar el idioma.
ENCLITICOS = {"le", "les", "lo", "los", "la", "las", "me", "te", "se",
              "nos", "os", "sela", "selo", "selas", "selos"}

def parte_pegada(pal, bigramas):
    """
    "Enel" -> "En el", pero solo si las otras Biblias traen esas dos
    palabras juntas y en ese orden en el mismo pasaje.

    Que las dos partes existan por separado no basta: con eso el corrector
    partía "contengan" en "con tengan", "señalen" en "señal en" y
    "producidos" en "producid os", porque "con", "tengan", "señal", "en",
    "producid" y "os" son todas palabras. Lo que no aparece en ningún
    pasaje es la pareja seguida.
    """
    if len(pal) < 4:
        return None

    def trozos(resto, acc):
        """Parte en dos o tres, exigiendo que cada pareja seguida exista."""
        if len(acc) == 3:
            return
        if acc and resto:
            if (sin_tildes(acc[-1].lower()),
                    sin_tildes(resto.lower())) in bigramas \
                    and resto.lower() not in ENCLITICOS and len(resto) >= 2:
                salidas.append(" ".join(acc + [resto]))
        for i in range(2, len(resto) - 1):
            a, b = resto[:i], resto[i:]
            if len(b) < 2 or b.lower() in ENCLITICOS:
                continue
            if acc and (sin_tildes(acc[-1].lower()),
                        sin_tildes(a.lower())) not in bigramas:
                continue
            if (sin_tildes(a.lower()), sin_tildes(b.lower())) in bigramas:
                salidas.append(" ".join(acc + [a, b]))
            trozos(b, acc + [a])

    salidas = []
    trozos(pal, [])
    unicas = sorted(set(salidas), key=lambda s: (-s.count(" "), s))
    if not unicas:
        return None
    # La partición con más trozos es la que explica la palabra entera:
    # "Enelaño" es "En el año", no "Enel año".
    mejores = [s for s in unicas if s.count(" ") == unicas[0].count(" ")]
    return mejores[0] if len(mejores) == 1 else None

class Testigo:
    """Las otras Biblias, consultables por pasaje."""

    def __init__(self, testigos, ventana=4):
        self.t = testigos
        self.ventana = ventana
        self.voc = set()
        for d in testigos.values():
            for txt in d.values():
                for w in RE_TOK.findall(txt):
                    self.voc.add(w.lower()); self.voc.add(sin_tildes(w.lower()))

    def es_palabra(self, pal):
        p = pal.lower()
        return p in self.voc or sin_tildes(p) in self.voc

    def bigramas_del_pasaje(self, ref):
        """Parejas de palabras seguidas en el mismo pasaje de las otras Biblias."""
        libro, cv = ref.rsplit(" ", 1)
        cap, ver = cv.split(":")
        out = set()
        for d in self.t.values():
            for dv in range(-self.ventana, self.ventana + 1):
                txt = d.get(f"{libro} {cap}:{int(ver) + dv}")
                if not txt:
                    continue
                ws = [sin_tildes(w.lower()) for w in RE_TOK.findall(txt)]
                out.update(zip(ws, ws[1:]))
        return out

    def palabras_del_pasaje(self, ref):
        """Vocabulario del mismo versículo en las otras Biblias, con margen.

        El margen es amplio a propósito: la Reina-Valera numera por el canon hebreo y
        esta Biblia por la Vulgata: en los Salmos y en algún profeta el
        mismo texto cae un número más allá.
        """
        libro, cv = ref.rsplit(" ", 1)
        cap, ver = cv.split(":")
        out = set()
        for d in self.t.values():
            for dv in range(-self.ventana, self.ventana + 1):
                txt = d.get(f"{libro} {cap}:{int(ver) + dv}")
                if not txt:
                    continue
                for w in RE_TOK.findall(txt):
                    out.add(w.lower()); out.add(sin_tildes(w.lower()))
        return out

    def dictamen(self, ref, pal, cand):
        """'corregir', 'dejar' o None si el pasaje no lo aclara."""
        aqui = self.palabras_del_pasaje(ref)
        if not aqui:
            return None
        p, c = sin_tildes(pal.lower()), sin_tildes(cand.lower())
        if p in aqui:
            return "dejar"
        if c in aqui:
            return "corregir"
        return None
