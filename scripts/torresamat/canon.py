"""Versificación Vulgata leída de la propia cabecera de SWORD."""
import re

SRC = "/usr/include/sword/canon_vulg.h"

def cargar():
    txt = open(SRC, encoding="utf-8", errors="replace").read()
    libros = []
    for m in re.finditer(r"struct sbook (ot|nt)books_vulg\[\]\s*=\s*\{(.*?)\n\};", txt, re.S):
        for b in re.finditer(r'\{"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(\d+)\}', m.group(2)):
            libros.append({"nombre": b.group(1), "osis": b.group(2), "caps": int(b.group(4))})
    m = re.search(r"int vm_vulg\[\]\s*=\s*\{(.*?)\n\};", txt, re.S)
    nums = [int(x) for x in re.findall(r"\d+", m.group(1))]
    i = 0
    for L in libros:
        L["versos"] = nums[i:i + L["caps"]]
        i += L["caps"]
    return libros

CANON = cargar()
POR_OSIS = {L["osis"]: L for L in CANON}

if __name__ == "__main__":
    print(f"libros: {len(CANON)}  capítulos: {sum(L['caps'] for L in CANON)}  "
          f"versículos: {sum(sum(L['versos']) for L in CANON)}")
    for L in CANON[:6] + CANON[15:20] + CANON[-6:]:
        print(f"  {L['osis']:8} {L['nombre'][:22]:22} {L['caps']:3} caps, "
              f"{sum(L['versos']):5} vv  (cap1={L['versos'][0]})")
