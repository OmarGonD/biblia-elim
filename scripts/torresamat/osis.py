"""Genera el OSIS a partir del JSON intermedio."""
import json, re, html
from canon import CANON, POR_OSIS
from construir import ORDEN

# Las llamadas a nota van en el texto como superíndices que el OCR convierte
# en asteriscos, interrogantes y comillas sueltas. Las notas no se importan
# -- son medio libro y no se han revisado --, así que la llamada se quita.
def limpia_final(s):
    s = re.sub(r"[*†‡]+", " ", s)
    s = re.sub(r"\s*[”“]\s*", " ", s)
    s = re.sub(r"\s*\?(?=\s*[.,;:])", "", s)      # "tierra?." -> "tierra."
    s = re.sub(r"\s+([,.;:!?])", r"\1", s)
    s = re.sub(r"\(\s*\)", "", s)
    s = re.sub(r"\s{2,}", " ", s)
    return s.strip(" ,;:")

CABECERA = """<?xml version="1.0" encoding="UTF-8"?>
<osis xmlns="http://www.bibletechnologies.net/2003/OSIS/namespace"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.bibletechnologies.net/2003/OSIS/namespace \
http://www.bibletechnologies.net/osisCore.2.1.1.xsd">
 <osisText osisIDWork="TorresAmat" osisRefWork="bible" xml:lang="es">
  <header>
   <work osisWork="TorresAmat">
    <title>La Sagrada Biblia (Torres Amat)</title>
    <identifier type="OSIS">Bible.es.TorresAmat.1882</identifier>
    <language type="IETF">es</language>
    <rights type="x-copyright">Dominio público</rights>
    <refSystem>Bible.Vulg</refSystem>
   </work>
  </header>
"""

def genera(texto, destino):
    faltan = 0
    with open(destino, "w", encoding="utf-8") as f:
        f.write(CABECERA)
        for osisid in ORDEN:
            L = POR_OSIS[osisid]
            f.write(f'  <div type="book" osisID="{osisid}">\n')
            for c, nver in enumerate(L["versos"], start=1):
                f.write(f'   <chapter osisID="{osisid}.{c}">\n')
                for v in range(1, nver + 1):
                    t = texto.get(f"{osisid} {c}:{v}")
                    if t is None:
                        faltan += 1
                        continue
                    t = limpia_final(t)
                    if not t:
                        faltan += 1
                        continue
                    f.write(f'    <verse osisID="{osisid}.{c}.{v}">'
                            f'{html.escape(t)}</verse>\n')
                f.write("   </chapter>\n")
            f.write("  </div>\n")
        f.write(" </osisText>\n</osis>\n")
    return faltan

if __name__ == "__main__":
    texto = json.load(open("texto.json", encoding="utf-8"))
    total = sum(sum(POR_OSIS[o]["versos"]) for o in ORDEN)
    faltan = genera(texto, "torresamat.osis.xml")
    print(f"OSIS escrito: {total - faltan}/{total} versículos con texto "
          f"({100*(total-faltan)/total:.1f}%), {faltan} vacíos")
