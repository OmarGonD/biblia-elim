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


if __name__ == "__main__":
    cat = [POR_OSIS[o] for o in ORDEN]
    print(f"libros: {len(cat)}  capítulos: {sum(L['caps'] for L in cat)}  "
          f"versículos: {sum(sum(L['versos']) for L in cat)}")
    for L in cat[:5] + cat[18:22] + cat[-3:]:
        print(f"  {L['osis']:8} {L['nombre'][:22]:22} {L['caps']:3} caps, "
              f"{sum(L['versos']):5} vv")
