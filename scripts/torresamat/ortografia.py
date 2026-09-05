"""
Corrige erratas del reconocimiento óptico usando el propio texto como
diccionario.

Un diccionario español moderno no sirve aquí: marcaría "crió", "cubrian",
"movia", "Egypto", "Israél" o "tambien" como faltas, y son la ortografía
correcta de la edición de 1882. Tocarlas sería estropear el texto, no
arreglarlo.

El corpus sí sirve. Una palabra que sale quinientas veces es como se
escribe en este libro; una que sale una sola vez y está a un carácter de
otra que sale quinientas es casi siempre un tropiezo del OCR. Así
"trelnta" cae en "treinta" y "eastigando" en "castigando", mientras
"triplíquese" o "colocáronle" -- raras pero legítimas -- se quedan como
están, porque no hay ninguna palabra frecuente a un paso de ellas.

Dos operaciones, las dos con la misma exigencia: el resultado tiene que
ser palabra frecuente de este mismo texto.

  1. Una sola edición de distancia, y con un único candidato posible.
  2. Partir palabras pegadas ("yamuerto", "Ydeallí"), que el OCR de esta
     edición junta a capricho.
"""
import re, collections

RE_TOK = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+")

RARO = 2          # una o dos apariciones en toda la Biblia
FRECUENTE = 15    # desde aquí se considera palabra buena del libro
MIN_LARGO = 4     # por debajo, demasiadas colisiones

def frecuencias(textos):
    f = collections.Counter()
    for t in textos:
        f.update(RE_TOK.findall(t))
    return f

def _borrados(p):
    return {p[:i] + p[i + 1:] for i in range(len(p))}

def construye_indice(frec):
    """Índice de borrados: para buscar vecinos a distancia 1 sin comparar todo."""
    idx = collections.defaultdict(set)
    for w, c in frec.items():
        if c < FRECUENTE or len(w) < MIN_LARGO:
            continue
        idx[w].add(w)
        for d in _borrados(w):
            idx[d].add(w)
    return idx

# Pares que confunde el reconocimiento óptico de esta edición, vistos en el
# texto: "trelnta" por treinta, "eastigando" por castigando, "encmigos" por
# enemigos, "yolvieron" por volvieron, "Juién" por Quién, "Bi" por Si.
#
# Exigir que la única diferencia sea uno de estos pares es lo que separa la
# errata del plural: sin ello el corrector proponía "Evangelios" ->"Evangelio",
# "limpian" -> "limpia" o "potro" -> "otro", que son palabras distintas y buenas.
# Fuera quedan a propósito a/o y o/e: son las vocales de género y de
# conjugación del castellano, y admitirlas convertía "casos" en "casas",
# "Hermanas" en "Hermanos" y "entraren" en "entraron" -- palabras buenas
# las seis. Cuesta perder "Josus" -> "Jesus", pero sale a cuenta.
CONFUSIONES = {
    frozenset("li"), frozenset("ce"), frozenset("nm"), frozenset("un"),
    frozenset("yv"), frozenset("tf"), frozenset("BS"),
    frozenset("JQ"), frozenset("rn"), frozenset("hb"), frozenset("IL"),
}
# s/g se probó y se quitó: proponía "desollar" -> "degollar", que son dos
# verbos distintos y los dos caben en el Antiguo Testamento.

def _confundible(a, b):
    """Solo sustitución, ni al principio ni al final, y de un par conocido."""
    if len(a) != len(b):
        return False
    difs = [(i, x, y) for i, (x, y) in enumerate(zip(a, b)) if x != y]
    if len(difs) != 1:
        return False
    i, x, y = difs[0]
    # Ni el primer carácter ni el último: al final viven el plural y el
    # tiempo verbal, y al principio la sustitución cambia la palabra entera
    # ("tachada" por "fachada").
    if i == 0 or i == len(a) - 1:
        return False
    return frozenset((x.lower(), y.lower())) in CONFUSIONES or \
           frozenset((x, y)) in CONFUSIONES

def _dist1(a, b):
    if a == b:
        return False
    la, lb = len(a), len(b)
    if abs(la - lb) > 1:
        return False
    if la == lb:                                   # sustitución
        return sum(x != y for x, y in zip(a, b)) == 1
    if la > lb:
        a, b, la, lb = b, a, lb, la
    i = 0                                          # inserción
    while i < la and a[i] == b[i]:
        i += 1
    return a[i:] == b[i + 1:]

def _mismo_molde(o, c):
    """No convertir may/min: "Ather" no debe volverse "ather"."""
    return o[:1].isupper() == c[:1].isupper()

def candidatos_d1(pal, frec, idx):
    vistos = set(idx.get(pal, ()))
    for d in _borrados(pal):
        vistos |= idx.get(d, set())
    return {c for c in vistos
            if _confundible(pal, c) and _mismo_molde(pal, c)
            and frec[c] >= FRECUENTE}

# Partir palabras pegadas se probó y se descartó. El OCR de esta edición
# las junta de verdad ("Puesque", "sumadre", "portodas"), pero no hay forma
# de distinguirlas de las palabras buenas que son raras en este corpus sin
# un diccionario del castellano de la época: el corrector proponía
# "repartirlas" -> "repartir las", "alcázares" -> "alcázar es",
# "escuchamos" -> "escucha mos", "Estuve" -> "Es tuve" y "Demuestra" ->
# "De muestra". Se deja sin tocar antes que estropear el texto.
FUNCION = {
    "y", "e", "o", "u", "de", "del", "la", "el", "los", "las", "un", "una",
    "en", "con", "por", "para", "que", "no", "ni", "se", "su", "sus", "mas",
    "si", "al", "lo", "le", "les", "me", "te", "nos", "os", "es", "ha", "he",
    "pues", "como", "cuando", "porque", "sino", "sobre", "hasta", "desde",
}

def parte_pegada(pal, frec):
    """Parte "Puesque" en "Pues que", pero no "bajóle" en "bajó le"."""
    n = len(pal)
    salidas = []
    for i in range(1, n - 2):
        a, b = pal[:i], pal[i:]
        if a.lower() not in FUNCION:
            continue
        if frec.get(b, 0) >= FRECUENTE or frec.get(b.lower(), 0) >= FRECUENTE:
            salidas.append(f"{a} {b}")
    return salidas[0] if len(salidas) == 1 else None

def tabla_correcciones(frec, idx):
    """{errata: arreglo} con solo lo inequívoco."""
    tabla = {}
    for pal, c in frec.items():
        if c > RARO or len(pal) < MIN_LARGO:
            continue
        cand = candidatos_d1(pal, frec, idx)
        if len(cand) == 1:
            tabla[pal] = cand.pop()
            continue

    return tabla

def aplica(texto, tabla):
    def rep(m):
        return tabla.get(m.group(0), m.group(0))
    return RE_TOK.sub(rep, texto)
