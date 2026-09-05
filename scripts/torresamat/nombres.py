"""
Nombres de libro tal como salen en la cabecera corrida de esta edición,
a candidatos OSIS.

El ordinal (I/II/III/IV REYES) se pierde a menudo en el OCR, y en esta
edición "Reyes" son los cuatro libros de la Vulgata: I-II Reyes son
Samuel y III-IV Reyes son los Reyes hebreos. Por eso cada nombre lleva
un conjunto de candidatos y no uno solo: basta con acotar la región del
canon, que del orden ya se encarga la alineación.
"""
import re
from cabeceras import sin_tildes

TABLA = {
    "GENESIS": ["Gen"], "EXODO": ["Exod"], "LEVITICO": ["Lev"],
    "NUMEROS": ["Num"], "DEUTERONOMIO": ["Deut"], "JOSUE": ["Josh"],
    "JUECES": ["Judg"], "RUTH": ["Ruth"], "RUT": ["Ruth"],
    "REYES": ["1Sam", "2Sam", "1Kgs", "2Kgs"],
    "PARALIPOMENON": ["1Chr", "2Chr"], "PARALTPOMENON": ["1Chr", "2Chr"],
    "ESDRAS": ["Ezra", "Neh"], "NEHEMIAS": ["Neh"],
    "TOBIAS": ["Tob"], "JUDITH": ["Jdt"], "ESTHER": ["Esth"], "JOB": ["Job"],
    "SALMOS": ["Ps"], "SALMO": ["Ps"], "PSALMOS": ["Ps"],
    "PROVERBIOS": ["Prov"], "ECLESIASTES": ["Eccl"], "ECCLESIASTES": ["Eccl"],
    "CANTAR": ["Song"], "CANTICO": ["Song"], "CANTARES": ["Song"],
    "SABIDURIA": ["Wis"], "ECLESIASTICO": ["Sir"], "ECCLESIASTICO": ["Sir"],
    "ISAIAS": ["Isa"], "ISATAS": ["Isa"], "JEREMIAS": ["Jer"],
    "LAMENTACIONES": ["Lam"], "THRENOS": ["Lam"], "TRENOS": ["Lam"],
    "BARUCH": ["Bar"], "BARUC": ["Bar"],
    "EZECHIEL": ["Ezek"], "EZEQUIEL": ["Ezek"],
    "DANIEL": ["Dan"], "DANTEL": ["Dan"],
    "OSEAS": ["Hos"], "JOEL": ["Joel"], "AMOS": ["Amos"], "ABDIAS": ["Obad"],
    "JONAS": ["Jonah"], "MICHEAS": ["Mic"], "MIQUEAS": ["Mic"],
    "NAHUM": ["Nah"], "HABACUC": ["Hab"], "SOPHONIAS": ["Zeph"],
    "AGGEO": ["Hag"], "ZACHARIAS": ["Zech"], "MALACHIAS": ["Mal"],
    "MACHABEOS": ["1Macc", "2Macc"], "MACABEOS": ["1Macc", "2Macc"],
    "MATHEO": ["Matt"], "MATEO": ["Matt"], "MARCOS": ["Mark"],
    "LUCAS": ["Luke"], "HECHOS": ["Acts"], "APOSTOLES": ["Acts"],
    "ROMANOS": ["Rom"], "CORINTHIOS": ["1Cor", "2Cor"],
    "CORINTIOS": ["1Cor", "2Cor"], "GALATAS": ["Gal"],
    "EPHESIOS": ["Eph"], "EFESIOS": ["Eph"],
    "PHILIPENSES": ["Phil"], "FILIPENSES": ["Phil"],
    "COLOSENSES": ["Col"],
    "THESALONICENSES": ["1Thess", "2Thess"],
    "TESALONICENSES": ["1Thess", "2Thess"],
    "TIMOTHEO": ["1Tim", "2Tim"], "TIMOTEO": ["1Tim", "2Tim"],
    "TITO": ["Titus"], "PHILEMON": ["Phlm"], "FILEMON": ["Phlm"],
    "HEBREOS": ["Heb"], "SANTIAGO": ["Jas"],
    "PEDRO": ["1Pet", "2Pet"],
    "JUDAS": ["Jude"], "APOCALYPSI": ["Rev"], "APOCALIPSIS": ["Rev"],
}
# "SAN JUAN" es el evangelio; "JUAN" a secas, en las epístolas, es ambiguo.
JUAN_EVANGELIO = ["John"]
JUAN_EPISTOLAS = ["1John", "2John", "3John"]

def _dist(a, b):
    """Levenshtein acotada, para tragar las erratas del OCR."""
    if abs(len(a) - len(b)) > 3:
        return 99
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1,
                           prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]

def candidatos(cabecera, ambito):
    """
    cabecera: texto normalizado de la cabecera corrida
    ambito:   OSIS válidos en este tomo
    Devuelve el conjunto de OSIS compatibles, o None si no se reconoce.
    """
    if not cabecera:
        return None
    toks = sin_tildes(cabecera).upper().split()
    san = "SAN" in toks
    for t in toks:
        if len(t) < 3:
            continue
        if t == "JUAN":
            c = JUAN_EVANGELIO if san else JUAN_EPISTOLAS
            c = [x for x in c if x in ambito]
            return set(c) if c else None
        mejor, md = None, 99
        for nombre, osis in TABLA.items():
            d = _dist(t, nombre)
            if d < md and d <= max(1, len(nombre) // 5):
                mejor, md = osis, d
        if mejor:
            c = [x for x in mejor if x in ambito]
            if c:
                return set(c)
    return None
