#!/usr/bin/env python3
"""Alineamiento Tisch (módulo SWORD) -> TAGNT, por versículo.

Posición Tisch = índice 1-based de la palabra (<w>) dentro del versículo, tal como la
entrega `mod2imp Tisch`. Posición TAGNT = el número #NN del TAGNT. Cada palabra de
Tisch recibe su posición TAGNT o null; no se fuerzan parejas.
Versificación: Tisch (SWORD) y TAGNT numeran distinto en algunos pasajes (p. ej. Jn 1:39-48, Jn 7:53-8:11,
2 Co 13:12-13). Pasada 1: por versículo. Pasada 2: los versículos con <70 % de parejas se descartan y se
realinean contra las palabras TAGNT libres del libro. La pareja se guarda como "ref#pos" TAGNT (la ref puede
ser otro versículo).
"""
import json, os, re, subprocess, sys, unicodedata
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tagnt

RAIZ = os.path.normpath(os.path.join(tagnt.AQUI, "..", ".."))
SALIDA = os.path.join(RAIZ, "data", "tagnt_alineacion", "tisch_a_tagnt.json")
SWORD_A_OSIS = {
    "Matthew": "Matt",
    "Mark": "Mark",
    "Luke": "Luke",
    "John": "John",
    "Acts": "Acts",
    "Romans": "Rom",
    "I Corinthians": "1Cor",
    "II Corinthians": "2Cor",
    "Galatians": "Gal",
    "Ephesians": "Eph",
    "Philippians": "Phil",
    "Colossians": "Col",
    "I Thessalonians": "1Thess",
    "II Thessalonians": "2Thess",
    "I Timothy": "1Tim",
    "II Timothy": "2Tim",
    "Titus": "Titus",
    "Philemon": "Phlm",
    "Hebrews": "Heb",
    "James": "Jas",
    "I Peter": "1Pet",
    "II Peter": "2Pet",
    "I John": "1John",
    "II John": "2John",
    "III John": "3John",
    "Jude": "Jude",
    "Revelation of John": "Rev",
}
RE_W = re.compile(r'<w lemma="([^"]*)" morph="([^"]*)">([^<]*)</w>')


def norm(s):
    s = unicodedata.normalize("NFD", s)
    s = "".join(c for c in s if not unicodedata.combining(c))
    s = re.sub(r"[^\w]", "", s.lower().replace("ς", "σ"))
    return s


def leer_tisch(imp=None):
    """{(osis, cap, ver): [ {pos, forma, strong, morph} ]} con posición sobre TODAS las <w>."""
    nombres = SWORD_A_OSIS
    out = imp if imp is not None else subprocess.run(["mod2imp", "Tisch"], capture_output=True,
                                                    check=True).stdout.decode("utf-8", "replace")
    res, actual = {}, None
    for linea in out.splitlines():
        if linea.startswith("$$$"):
            m = re.match(r"\$\$\$(.+) (\d+):(\d+)$", linea)
            actual = None
            if m and m[1] in nombres and int(m[2]) > 0:
                actual = (nombres[m[1]], int(m[2]), int(m[3]))
                res[actual] = []
        elif actual:
            for m in RE_W.finditer(linea):
                st = re.search(r"strong:G(\d+)", m[1])
                mo = re.search(r"robinson:(\S+)", m[2])
                res[actual].append({"pos": len(res[actual]) + 1,
                                    "forma": unicodedata.normalize("NFC", m[3].strip()),
                                    "strong": "G%d" % int(st[1]) if st else "",
                                    "morph": mo[1] if mo else ""})
    return {k: v for k, v in res.items() if v}


def tagnt_por_versiculo():
    """{(osis, cap, ver) en versificación Tisch/KJV: [Palabra]}"""
    res = {}
    for p in tagnt.leer():
        osis, cap, ver = p.ref.split(".")
        k = (osis, int(cap), int(ver))
        if p.ref_alt.startswith("["):
            c2, v2 = p.ref_alt.strip("[]").split(".")[:2]
            k = (osis, int(c2), int(v2))
        res.setdefault(k, []).append(p)
    for v in res.values():
        v.sort(key=lambda p: p.pos)
    return res


# Strongs que Tisch (SWORD) y TAGNT reparten distinto para el mismo lema o formas supletivas.
FAMILIAS = [{"G1473", "G2249", "G4771", "G5210", "G1683", "G1438", "G4572", "G846"},
            {"G1492", "G3708", "G3700", "G1506", "G991"},
            {"G3004", "G2036", "G4483", "G2046", "G5346"}]


def _fuertes(p):
    s = {p.strong}
    for a in p.alt_strongs:
        s.add(tagnt.strong_simple(a))
    for fam in FAMILIAS:
        if s & fam:
            s |= fam
    return s


def _puntaje(t, p):
    igual = norm(t["forma"]) == norm(p.griego)
    if t["strong"] not in _fuertes(p):
        return 2 if igual else None   # misma forma, otro Strong: se marca 'forma_otro_strong'
    if igual:
        return 3 if t["strong"] == p.strong else 2.5
    return 1.5 if t["strong"] == p.strong else 1


def nw(tw, pw):
    """Needleman-Wunsch. Devuelve [(i, j|None)] con índices 0-based de tw/pw."""
    n, m = len(tw), len(pw)
    GAP = -0.3
    S = [[0.0] * (m + 1) for _ in range(n + 1)]
    B = [[None] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        S[i][0] = S[i - 1][0] + GAP * 3
        B[i][0] = "u"
    for j in range(1, m + 1):
        S[0][j] = S[0][j - 1] + GAP
        B[0][j] = "l"
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            best, bk = S[i - 1][j] + GAP * 3, "u"
            if S[i][j - 1] + GAP > best:
                best, bk = S[i][j - 1] + GAP, "l"
            sc = _puntaje(tw[i - 1], pw[j - 1])
            if sc is not None and S[i - 1][j - 1] + sc > best:
                best, bk = S[i - 1][j - 1] + sc, "d"
            S[i][j], B[i][j] = best, bk
    i, j, res = n, m, []
    while i > 0 or j > 0:
        k = B[i][j]
        if k == "d":
            res.append((i - 1, j - 1))
            i, j = i - 1, j - 1
        elif k == "u":
            res.append((i - 1, None))
            i -= 1
        else:
            j -= 1
    return sorted(res)


def calidad(t, p):
    if norm(t["forma"]) != norm(p.griego):
        return "strong"
    return "forma" if t["strong"] in _fuertes(p) else "forma_otro_strong"


def construir():
    tisch = leer_tisch()
    tag = tagnt_por_versiculo()
    asign = {}   # (osis,cap,ver,pos_tisch) -> Palabra
    usadas = set()
    # Pasada 1: por versículo
    for k, tw in tisch.items():
        pw = tag.get(k, [])
        pares = [(i, j) for i, j in nw(tw, pw) if j is not None]
        if len(pares) < 0.7 * len(tw):
            continue                  # versículo desplazado: se resuelve en la pasada 2
        for i, j in pares:
            asign[k + (tw[i]["pos"],)] = pw[j]
            usadas.add(pw[j].clave)
    # Pasada 2: por libro, versículos no resueltos contra palabras TAGNT libres
    libres_por_libro = {}   # por (libro, capítulo): evita parejas lejanas de palabras funcionales
    for p in tagnt.leer():
        if p.clave not in usadas:
            o, c, _ = p.ref.split(".")
            libres_por_libro.setdefault((o, int(c)), []).append(p)
    pendientes, bloques = {}, {}
    for k, tw in tisch.items():
        if not any(k + (t["pos"],) in asign for t in tw):
            # bloque anómalo del módulo (p. ej. Jn 8:53 con 375 palabras): se procesa al final
            destino = bloques if len(tw) > 150 else pendientes
            destino.setdefault(k[:2], []).extend((k, t) for t in tw)
    stats = {"palabras": 0, "forma": 0, "strong": 0, "forma_otro_strong": 0, "sin_pareja": 0,
             "recuperadas_pasada2": 0, "reordenadas_pasada3": 0, "no_na28": 0}
    for grupo in (pendientes, bloques):
        for libro, filas in grupo.items():
            es_bloque = grupo is bloques
            libres = [p for p in libres_por_libro.get(libro, []) if p.clave not in usadas]
            for i, j in nw([t for _, t in filas], libres):
                if j is not None:
                    (k, t) = filas[i]
                    if not es_bloque and abs(int(libres[j].ref.split(".")[2]) - k[2]) > 3:
                        continue     # pareja lejana: no se fuerza
                    asign[k + (t["pos"],)] = libres[j]
                    usadas.add(libres[j].clave)
                    stats["recuperadas_pasada2"] += 1
    # Pasada 3: reordenamientos dentro del capítulo (±3 versículos), sin exigir orden
    reorden = set()
    libres_cap = {}
    for p in tagnt.leer():
        if p.clave not in usadas:
            o, c, v = p.ref.split(".")
            libres_cap.setdefault((o, int(c)), []).append((int(v), p))
    for k, tw in tisch.items():
        for t in tw:
            kk = k + (t["pos"],)
            if kk in asign:
                continue
            mejor = None
            for v, p in libres_cap.get(k[:2], []):
                if p.clave in usadas or abs(v - k[2]) > 3 or _puntaje(t, p) is None:
                    continue
                sc = (_puntaje(t, p), -abs(v - k[2]), -p.pos)
                if mejor is None or sc > mejor[0]:
                    mejor = (sc, p)
            if mejor:
                asign[kk] = mejor[1]
                usadas.add(mejor[1].clave)
                reorden.add(kk)
    salida = {}
    ejemplos = {"sin_pareja": [], "strong": [], "no_na28": [], "otro_verso": []}
    for k in sorted(tisch):
        ref = "%s.%d.%d" % k
        filas = []
        for t in tisch[k]:
            stats["palabras"] += 1
            p = asign.get(k + (t["pos"],))
            if p is None:
                stats["sin_pareja"] += 1
                if len(ejemplos["sin_pareja"]) < 40:
                    ejemplos["sin_pareja"].append((ref, t["pos"], t["forma"], t["strong"]))
                filas.append([t["pos"], None, ""])
                continue
            q = calidad(t, p)
            stats[q] += 1
            flag = q
            if k + (t["pos"],) in reorden:
                stats["reordenadas_pasada3"] += 1
                flag += "+reorden"
            if not p.en("NA28"):
                stats["no_na28"] += 1
                flag += "+noNA28"
                if len(ejemplos["no_na28"]) < 25:
                    ejemplos["no_na28"].append((ref, t["pos"], t["forma"], p.griego, "+".join(p.ediciones)))
            if q == "strong" and len(ejemplos["strong"]) < 25:
                ejemplos["strong"].append((ref, t["pos"], t["forma"], p.griego, p.strong))
            if p.ref.split(".")[:3] != ref.split(".")[:3] and len(ejemplos["otro_verso"]) < 12:
                ejemplos["otro_verso"].append((ref, t["pos"], p.clave))
            filas.append([t["pos"], p.clave, flag])
        salida[ref] = filas
    return salida, stats, ejemplos


if __name__ == "__main__":
    sal, st, ej = construir()
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    with open(SALIDA, "w", encoding="utf-8") as f:
        json.dump({"nota": "ref OSIS (versificacion de Tisch/SWORD) -> [pos_tisch, 'ref#pos' TAGNT|null, calidad]",
                   "v": sal}, f, ensure_ascii=False, separators=(",", ":"))
    print(json.dumps(st, ensure_ascii=False, indent=1))
    for k, v in ej.items():
        print("##", k)
        for x in v:
            print("  ", x)
