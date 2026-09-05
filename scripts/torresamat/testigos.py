"""
Las otras Biblias en español, como testigos.

El corrector de ortografía se quedaba corto por una razón de fondo: no
tenía un diccionario del castellano. Usar el propio texto como diccionario
resuelve la mitad del problema -- no marca "crió" ni "Egypto" como faltas,
que es lo que haría un diccionario moderno -- pero deja la otra mitad sin
resolver, porque no sabe distinguir una palabra rara y buena de una errata.

Las Biblias que ya vienen instaladas sí lo saben. La Reina-Valera de 1909
está a veintisiete años de esta edición y la Platense es del 48: entre las
tres cubren el vocabulario del castellano bíblico de la época, con su
ortografía, sus nombres propios y sus arcaísmos. Lo que aparece en ellas
es palabra; lo que no aparece en ninguna y está a un carácter de algo que
sí, es errata.

Y para las dudas concretas sirven además como testigo del pasaje: la
Platense usa la misma versificación Vulgata, así que se puede leer el
mismo versículo en las dos y ver qué dice.
"""
import os, pickle, re, subprocess, unicodedata

MODULOS = ("SpaRV", "SpaRVG", "SpaPlatense")
CACHE = "testigos.pkl"
RE_TOK = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+")
RE_LINEA = re.compile(r"^(.+?)\s(\d+):(\d+):\s?(.*)$")

def _libros():
    from canon import CANON
    return [L["osis"] for L in CANON]

def descarga():
    """{modulo: {'Osis c:v': texto}}"""
    if os.path.exists(CACHE):
        with open(CACHE, "rb") as f:
            return pickle.load(f)
    out = {}
    for mod in MODULOS:
        d = {}
        for osis in _libros():
            try:
                r = subprocess.run(["diatheke", "-b", mod, "-k", osis],
                                   capture_output=True, text=True, timeout=120)
            except Exception:
                continue
            for ln in r.stdout.splitlines():
                m = RE_LINEA.match(ln)
                if not m:
                    continue
                t = re.sub(r"<[^>]*>", " ", m.group(4))
                t = re.sub(r"\s{2,}", " ", t).strip()
                if t:
                    d[f"{osis} {int(m.group(2))}:{int(m.group(3))}"] = t
        out[mod] = d
    with open(CACHE, "wb") as f:
        pickle.dump(out, f, -1)
    return out

def vocabulario(testigos):
    """Todas las palabras que usan las otras Biblias, en minúscula y sin tildes."""
    voc = set()
    for d in testigos.values():
        for t in d.values():
            for w in RE_TOK.findall(t):
                voc.add(w.lower())
                voc.add(_sin_tildes(w.lower()))
    return voc

def _sin_tildes(s):
    s = unicodedata.normalize("NFD", s)
    return "".join(c for c in s if unicodedata.category(c) != "Mn")

def es_palabra(pal, voc):
    p = pal.lower()
    return p in voc or _sin_tildes(p) in voc

if __name__ == "__main__":
    t = descarga()
    for m, d in t.items():
        print(f"  {m:14} {len(d)} versículos")
    v = vocabulario(t)
    print(f"vocabulario conjunto: {len(v)} formas")
    for w in ("arda", "anda", "pares", "panes", "temia", "reimado", "cineo",
              "slete", "reinado", "tereero"):
        print(f"   {w:10} {'palabra' if es_palabra(w, v) else '— no aparece'}")
