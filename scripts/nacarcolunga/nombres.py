"""Nombres de libro de la BAC 1944 a candidatos OSIS."""
import re
import unicodedata

TABLA = {
    "GENESIS": ["Gen"], "EXODO": ["Exod"], "LEVITICO": ["Lev"],
    "NUMEROS": ["Num"], "DEUTERONOMIO": ["Deut"], "JOSUE": ["Josh"],
    "JUECES": ["Judg"], "RUTH": ["Ruth"], "RUT": ["Ruth"],
    "SAMUEL": ["1Sam", "2Sam"],
    "REYES": ["1Kgs", "2Kgs"],
    "CRONICAS": ["1Chr", "2Chr"], "CRONICA": ["1Chr", "2Chr"],
    "PARALIPOMENOS": ["1Chr", "2Chr"],
    "ESDRAS": ["Ezra"], "NEHEMIAS": ["Neh"],
    "TOBIAS": ["Tob"], "JUDIT": ["Jdt"], "JUDITH": ["Jdt"],
    "ESTER": ["Esth"], "ESTHER": ["Esth"],
    "MACABEOS": ["1Macc", "2Macc"],
    "JOB": ["Job"], "SALMOS": ["Ps"], "SALMO": ["Ps"],
    "PROVERBIOS": ["Prov"], "ECLESIASTES": ["Eccl"],
    "CANTAR": ["Song"], "CANTICO": ["Song"], "CANTARES": ["Song"],
    "SABIDURIA": ["Wis"], "ECLESIASTICO": ["Sir"],
    "ISAIAS": ["Isa"], "JEREMIAS": ["Jer"],
    "LAMENTACIONES": ["Lam"], "TRENOS": ["Lam"],
    "BARUC": ["Bar"], "BARUCH": ["Bar"],
    "EZEQUIEL": ["Ezek"], "EZECHIEL": ["Ezek"],
    "DANIEL": ["Dan"],
    "OSEAS": ["Hos"], "JOEL": ["Joel"], "AMOS": ["Amos"],
    "ABDIAS": ["Obad"], "JONAS": ["Jonah"],
    "MIQUEAS": ["Mic"], "NAHUM": ["Nah"], "HABACUC": ["Hab"],
    "SOFONIAS": ["Zeph"], "AGEO": ["Hag"], "AGGEO": ["Hag"],
    "ZACARIAS": ["Zech"], "MALAQUIAS": ["Mal"],
    "MATEO": ["Matt"], "MARCOS": ["Mark"], "LUCAS": ["Luke"],
    "HECHOS": ["Acts"], "APOSTOLES": ["Acts"],
    "ROMANOS": ["Rom"],
    "CORINTIOS": ["1Cor", "2Cor"], "CORINTHIOS": ["1Cor", "2Cor"],
    "GALATAS": ["Gal"], "EFESIOS": ["Eph"], "EPHESIOS": ["Eph"],
    "FILIPENSES": ["Phil"], "PHILIPENSES": ["Phil"],
    "COLOSENSES": ["Col"],
    "TESALONICENSES": ["1Thess", "2Thess"],
    "TIMOTEO": ["1Tim", "2Tim"], "TIMOTHEO": ["1Tim", "2Tim"],
    "TITO": ["Titus"], "FILEMON": ["Phlm"], "PHILEMON": ["Phlm"],
    "HEBREOS": ["Heb"], "SANTIAGO": ["Jas"],
    "PEDRO": ["1Pet", "2Pet"],
    "JUDAS": ["Jude"],
    "APOCALIPSIS": ["Rev"], "APOCALYPSI": ["Rev"],
}
JUAN_EVANGELIO = ["John"]
JUAN_EPISTOLAS = ["1John", "2John", "3John"]

ORDINAL = {
    "I": 1, "II": 2, "III": 3, "1": 1, "2": 2, "3": 3,
    "PRIMERO": 1, "PRIMERA": 1, "SEGUNDO": 2, "SEGUNDA": 2,
    "TERCERO": 3, "TERCERA": 3,
}


def sin_tildes(s):
    s = unicodedata.normalize("NFD", s)
    return "".join(c for c in s if unicodedata.category(c) != "Mn")


def _dist(a, b):
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


def _busca(tok):
    mejor, md = None, 99
    for nombre, osis in TABLA.items():
        d = _dist(tok, nombre)
        # TITO/TICO, JOB/JOY, AMOS/AMOR: con un error en cuatro
        # letras se cuela basura de la página (LEVITICO → TICO).
        tope = 0 if len(nombre) <= 4 else (1 if len(nombre) <= 8 else 2)
        if d < md and d <= tope:
            mejor, md = osis, d
    return mejor


def candidatos(cabecera, ambito):
    """OSIS compatibles con la cabecera corrida, o None."""
    if not cabecera:
        return None
    toks = sin_tildes(cabecera).upper().split()
    toks = [re.sub(r"[^A-Z0-9]", "", t) for t in toks]
    toks = [t for t in toks if t]
    san = "SAN" in toks or "SANTO" in toks
    ordinal = None
    for t in toks:
        if t in ORDINAL:
            ordinal = ORDINAL[t]
            break
    hallados = []
    for t in toks:
        if len(t) < 3:
            continue
        if t == "JUAN":
            # "SAN JUAN" es el evangelio; "I SAN JUAN" / "JUAN" a
            # secas, las epístolas. El ordinal manda.
            if ordinal or not san:
                hallados.append(JUAN_EPISTOLAS)
            else:
                hallados.append(JUAN_EVANGELIO)
            continue
        m = _busca(t)
        if m:
            hallados.append(m)
    if not hallados:
        return None
    c = list(hallados[0])
    if ordinal and len(c) >= ordinal:
        c = [c[ordinal - 1]]
    c = [x for x in c if x in ambito]
    return set(c) if c else None
