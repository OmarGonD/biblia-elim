"""Genera el OSIS a partir del JSON intermedio.

Cada versículo del JSON es un texto, o bien {"titulo": …, "texto": …} cuando
el facsímil dice que ese versículo es (o empieza por) el título del salmo;
ver titulos.py.
"""
import collections, json, re, html
from canon import CANON, POR_OSIS
from entidades import lexico, ruido_en_crudo, CRUDO
from construir import ORDEN
from titulos import marca_titulo

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

def contenido_verso(t, frec=None):
    """Contenido OSIS de un versículo del JSON, o "" si no trae texto.

    El «'», «<» y «>» del OCR es ruido (entidades.py) y se quita antes de
    escapar; se escapa sin quote porque osis2mod no entiende «&#x27;».
    frec es el léxico de todo el texto (entidades.lexico).
    """
    frec = frec if frec is not None else collections.Counter()
    if isinstance(t, dict):
        titulo = limpia_final(ruido_en_crudo(t.get("titulo", ""), frec))
        cuerpo = limpia_final(ruido_en_crudo(t.get("texto", ""), frec))
        if titulo:
            return marca_titulo(titulo, cuerpo)
        t = cuerpo
    else:
        t = limpia_final(ruido_en_crudo(t, frec))
    return html.escape(t, quote=False) if t else ""


def _textos(texto):
    for t in texto.values():
        if isinstance(t, dict):
            yield t.get("titulo", "")
            yield t.get("texto", "")
        else:
            yield t

def genera(texto, destino, orden=ORDEN):
    faltan = 0
    frec = lexico(_textos(texto), CRUDO)
    with open(destino, "w", encoding="utf-8") as f:
        f.write(CABECERA)
        for osisid in orden:
            L = POR_OSIS[osisid]
            f.write(f'  <div type="book" osisID="{osisid}">\n')
            for c, nver in enumerate(L["versos"], start=1):
                f.write(f'   <chapter osisID="{osisid}.{c}">\n')
                for v in range(1, nver + 1):
                    t = texto.get(f"{osisid} {c}:{v}")
                    if t is None:
                        faltan += 1
                        continue
                    t = contenido_verso(t, frec)
                    if not t:
                        faltan += 1
                        continue
                    f.write(f'    <verse osisID="{osisid}.{c}.{v}">'
                            f'{t}</verse>\n')
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
