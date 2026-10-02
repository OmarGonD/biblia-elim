#!/usr/bin/env python3
"""Parser del TAGNT (STEPBible, CC BY 4.0). Estructura de columnas en
data/sources/tagnt/SOURCE.md. No modifica los datos: solo los lee y los
normaliza (referencia, Strong sin ceros, ediciones como lista, enlaces)."""
import os, re, unicodedata
from dataclasses import dataclass, field, asdict

AQUI = os.path.dirname(os.path.abspath(__file__))
DIR_DATOS = os.path.normpath(os.path.join(AQUI, "..", "..", "data", "sources", "tagnt"))
ARCHIVOS = ("TAGNT_Mat-Jhn.txt", "TAGNT_Act-Rev.txt")

# Orden de las ediciones tal como las nombra el encabezado del archivo.
EDICIONES = ("NA28", "NA27", "Tyn", "SBL", "WH", "Treg", "TR", "Byz")

# Código del TAGNT -> código OSIS
OSIS = {
    "Mat": "Matt", "Mrk": "Mark", "Luk": "Luke", "Jhn": "John", "Act": "Acts", "Rom": "Rom",
    "1Co": "1Cor", "2Co": "2Cor", "Gal": "Gal", "Eph": "Eph", "Php": "Phil", "Col": "Col",
    "1Th": "1Thess", "2Th": "2Thess", "1Ti": "1Tim", "2Ti": "2Tim", "Tit": "Titus",
    "Phm": "Phlm", "Heb": "Heb", "Jas": "Jas", "1Pe": "1Pet", "2Pe": "2Pet", "1Jn": "1John",
    "2Jn": "2John", "3Jn": "3John", "Jud": "Jude", "Rev": "Rev",
}

RE_FILA = re.compile(r"^([1-3A-Za-z]{3})\.(\d+)\.(\d+)([\[\(\{][^#\t]*[\]\)\}])?#(\d+)=(\S+)$")
RE_GREGO = re.compile(r"^(.*?)\s*\(([^()]*)\)\s*$")
RE_STRONG = re.compile(r"^([GH])(\d+)([A-Za-z]?)")


@dataclass
class Palabra:
    ref: str                 # "John.1.1" (OSIS, versificación NRSV del TAGNT)
    pos: int                 # número de palabra en el versículo (TAGNT)
    ref_alt: str = ""        # versificación alterna: "[17.14]", "(13.38)", "{6.22}"
    tipo: str = ""           # NKO, N(k)O, K, KO, O, ...
    griego: str = ""
    translit: str = ""
    ingles: str = ""
    dstrong: str = ""        # Strong extendido tal como viene: "G2424G"
    strong: str = ""         # Strong simple sin ceros: "G2424"
    gramatica: str = ""      # "N-GSM-P"
    lema: str = ""
    glosa: str = ""
    ediciones: list = field(default_factory=list)
    ediciones_raw: str = ""
    otras_fuentes: list = field(default_factory=list)  # NIV, KJV, manuscritos (05, 032, P66...), versiones
    var_significado: str = ""
    var_ortografia: str = ""
    espanol: str = ""
    submeaning: str = ""
    conjoin: str = ""        # "#04»05:G3056" tal cual
    enlace_dir: str = ""     # "»" (a una palabra posterior), "«" (anterior) o ""
    enlace_pos: int = 0
    enlace_strong: str = ""
    ssi: str = ""            # sStrong+Instance: "G5207_A"
    alt_strongs: list = field(default_factory=list)
    nota_variante: str = ""
    entre_corchetes: bool = False   # la palabra abre con "[[" / "[": variante o texto dudoso

    @property
    def clave(self):
        return f"{self.ref}#{self.pos:02d}"

    def en(self, edicion):
        return edicion in self.ediciones

    def a_dict(self):
        return asdict(self)


def strong_simple(dstrong):
    m = RE_STRONG.match(dstrong or "")
    return f"{m.group(1)}{int(m.group(2))}" if m else ""


def _ediciones(cel):
    # "NA28+NA27+...", puede traer "moved »2: Treg"; se toman los nombres conocidos
    return [e for e in EDICIONES if re.search(r"(^|[+\s:])" + e + r"(\W|$)", cel.split("moved")[0])] \
        if cel.strip() else []


def _otras(cel):
    base = cel.split("moved")[0]
    return [t.strip() for t in base.split("+") if t.strip() and t.strip() not in EDICIONES]


def parsear_linea(linea):
    """Devuelve Palabra o None si la línea no es una fila de palabra."""
    c = linea.rstrip("\n").split("\t")
    m = RE_FILA.match(c[0].strip()) if c else None
    if not m:
        return None
    libro, cap, ver, alt, pos, tipo = m.groups()
    c += [""] * (14 - len(c))
    g = RE_GREGO.match(c[1].strip())
    griego, translit = (g.group(1), g.group(2)) if g else (c[1].strip(), "")
    griego = unicodedata.normalize("NFC", griego)
    dstrong, _, gram = c[3].partition("=")
    lema, _, glosa = c[4].partition("=")
    conj = c[10].strip()
    mc = re.match(r"^#\d+([»«])(\d+)(?::(\S+))?", conj)
    alt_s = [s for s in c[12].split() if s]
    pal = Palabra(
        ref=f"{OSIS.get(libro, libro)}.{int(cap)}.{int(ver)}", pos=int(pos), ref_alt=alt or "",
        tipo=tipo, griego=griego, translit=translit, ingles=c[2].strip(),
        dstrong=dstrong.strip(), strong=strong_simple(dstrong), gramatica=gram.strip(),
        lema=unicodedata.normalize("NFC", lema.strip()), glosa=glosa.strip(), ediciones=_ediciones(c[5]), ediciones_raw=c[5].strip(), otras_fuentes=_otras(c[5]),
        var_significado=c[6].strip(), var_ortografia=c[7].strip(), espanol=c[8].strip(),
        submeaning=c[9].strip(), conjoin=conj,
        enlace_dir=mc.group(1) if mc else "", enlace_pos=int(mc.group(2)) if mc else 0,
        enlace_strong=strong_simple(mc.group(3)) if mc and mc.group(3) else "",
        ssi=c[11].strip(), alt_strongs=alt_s, nota_variante=c[13].strip(),
        entre_corchetes=griego.lstrip().startswith("["))
    return pal


def leer(rutas=None, filtro=None):
    """Itera Palabra en orden de archivo. filtro: función(Palabra)->bool opcional."""
    rutas = rutas or [os.path.join(DIR_DATOS, a) for a in ARCHIVOS]
    for ruta in rutas:
        with open(ruta, encoding="utf-8-sig") as f:
            for linea in f:
                p = parsear_linea(linea)
                if p and (filtro is None or filtro(p)):
                    yield p


def versiculo(ref, rutas=None):
    """Lista de Palabra de un versículo OSIS ('John.1.1'), todas las variantes."""
    libro = ref.split(".")[0]
    tag = next((k for k, v in OSIS.items() if v == libro), libro)
    arch = ARCHIVOS[0] if tag in ("Mat", "Mrk", "Luk", "Jhn") else ARCHIVOS[1]
    rutas = rutas or [os.path.join(DIR_DATOS, arch)]
    return [p for p in leer(rutas, lambda p: p.ref == ref)]


def texto_de(palabras, edicion):
    """Palabras que pertenecen a una edición, en orden de posición."""
    return [p for p in sorted(palabras, key=lambda p: p.pos) if p.en(edicion)]


if __name__ == "__main__":
    import sys
    for p in versiculo(sys.argv[1] if len(sys.argv) > 1 else "John.1.1"):
        print(p.pos, p.griego, p.dstrong, p.gramatica, p.tipo, "+".join(p.ediciones))
