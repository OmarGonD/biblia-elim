"""Versificación NRSVA (hebreo/griego + deuterocanónicos) leída de SWORD.

Nácar-Colunga traduce de los originales, no de la Vulgata, así que no
sirve el canon Vulg de Torres Amat: los Salmos son 150, Samuel y Reyes
van partidos a la hebrea, y Daniel hebreo tiene 12 capítulos. NRSVA es
el sistema de SWORD que cubre eso y además los deuterocanónicos.
"""
import re

SRC_NRSVA = "/usr/include/sword/canon_nrsva.h"
SRC_NT = "/usr/include/sword/canon.h"

# Orden impreso de la BAC 1944, leído de las cabeceras corridas.
# Los doce profetas no siguen el orden habitual (Joel, Jonás y Abdías
# van después de Sofonías). Pablo abre por Tesalonicenses, no por Romanos.
# El alineador recorre la corriente en este orden; el de NRSVA pondría
# Job sobre Isaías y Romanos sobre Tesalonicenses.
ORDEN = [
    "Gen", "Exod", "Lev", "Num", "Deut",
    "Josh", "Judg", "Ruth", "1Sam", "2Sam", "1Kgs", "2Kgs",
    "1Chr", "2Chr", "Ezra", "Neh", "Tob", "Jdt", "Esth", "1Macc", "2Macc",
    "Isa", "Jer", "Lam", "Bar", "Ezek", "Dan",
    "Hos", "Amos", "Mic", "Nah", "Hab", "Zeph",
    "Joel", "Jonah", "Obad", "Hag", "Zech", "Mal",
    "Job", "Ps", "Prov", "Eccl", "Song", "Wis", "Sir",
    "Matt", "Mark", "Luke", "John", "Acts",
    "1Thess", "2Thess", "1Cor", "2Cor", "Gal", "Rom",
    "Phil", "Eph", "Col", "Phlm", "1Tim", "2Tim", "Titus", "Heb",
    "Jas", "1Pet", "2Pet", "1John", "2John", "3John", "Jude", "Rev",
]


def _libros(src, nombre):
    txt = open(src, encoding="utf-8", errors="replace").read()
    m = re.search(
        r"struct sbook %s\[\]\s*=\s*\{(.*?)\n\};" % nombre, txt, re.S)
    out = []
    for b in re.finditer(
            r'\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(\d+)\}', m.group(1)):
        if not b.group(2):
            continue
        out.append({"nombre": b.group(1), "osis": b.group(2),
                    "caps": int(b.group(4))})
    return out


def cargar():
    ot = _libros(SRC_NRSVA, "otbooks_nrsva")
    nt = _libros(SRC_NT, "ntbooks")
    libros = ot + nt
    txt = open(SRC_NRSVA, encoding="utf-8", errors="replace").read()
    m = re.search(r"int vm_nrsva\[\]\s*=\s*\{(.*?)\n\};", txt, re.S)
    nums = [int(x) for x in re.findall(r"\d+", m.group(1))]
    i = 0
    for L in libros:
        L["versos"] = nums[i:i + L["caps"]]
        i += L["caps"]
    return libros


CANON = cargar()
POR_OSIS = {L["osis"]: L for L in CANON}

SRC_LENINGRAD = "/usr/include/sword/canon_leningrad.h"


def _salmos_hebreo():
    """Versos por salmo contando el título, como numera la Biblia hebrea.

    Nácar-Colunga imprime el título del salmo como versículo 1 (o 1-2),
    igual que el texto hebreo; NRSVA, la versificación del módulo, lo deja
    sin número. Leningrad es la numeración hebrea que trae SWORD.
    """
    libros = _libros(SRC_LENINGRAD, "otbooks_leningrad")
    txt = open(SRC_LENINGRAD, encoding="utf-8", errors="replace").read()
    m = re.search(r"int vm_leningrad\[\]\s*=\s*\{(.*?)\n\};", txt, re.S)
    nums = [int(x) for x in re.findall(r"\d+", re.sub(r"//.*", "", m.group(1)))]
    i = 0
    for L in libros:
        if L["osis"] == "Ps":
            return nums[i:i + L["caps"]]
        i += L["caps"]
    raise ValueError("Leningrad sin Salmos")


SALMOS_HEBREO = _salmos_hebreo()

# Salmo -> cuántos versículos impresos ocupa el título (1 o 2).
TITULO_SALMOS = {
    c: h - n
    for c, (h, n) in enumerate(zip(SALMOS_HEBREO, POR_OSIS["Ps"]["versos"]), 1)
    if h != n
}
assert len(SALMOS_HEBREO) == 150 and all(d in (1, 2) for d in TITULO_SALMOS.values())


if __name__ == "__main__":
    cat = [POR_OSIS[o] for o in ORDEN]
    print(f"libros: {len(cat)}  capítulos: {sum(L['caps'] for L in cat)}  "
          f"versículos: {sum(sum(L['versos']) for L in cat)}")
    for L in cat[:5] + cat[18:22] + cat[-3:]:
        print(f"  {L['osis']:8} {L['nombre'][:22]:22} {L['caps']:3} caps, "
              f"{sum(L['versos']):5} vv")
