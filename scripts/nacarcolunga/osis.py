"""Genera el OSIS a partir del JSON intermedio."""
import html
import json
import os
import re

from canon import ORDEN, POR_OSIS

DIR = os.path.dirname(os.path.abspath(__file__))

CABECERA = """<?xml version="1.0" encoding="UTF-8"?>
<osis xmlns="http://www.bibletechnologies.net/2003/OSIS/namespace"
      xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
      xsi:schemaLocation="http://www.bibletechnologies.net/2003/OSIS/namespace \
http://www.bibletechnologies.net/osisCore.2.1.1.xsd">
 <osisText osisIDWork="NacarColunga" osisRefWork="bible" xml:lang="es">
  <header>
   <work osisWork="NacarColunga">
    <title>Sagrada Biblia (Nácar-Colunga) — reconstrucción automática</title>
    <identifier type="OSIS">Bible.es.NacarColunga.1944</identifier>
    <language type="IETF">es</language>
    <rights type="x-copyright">Obra protegida. Copia local de uso privado, no distribuir.</rights>
    <refSystem>Bible.NRSVA</refSystem>
   </work>
  </header>
"""


def limpia_final(s):
    s = s.replace("|", " ")
    s = re.sub(r"[*†‡]+", " ", s)
    # Algunos testigos llegan doblemente escapados (``&amp;#x27;``).  Se
    # decodifican antes de volver a escribir XML: así un apóstrofo sigue
    # siendo un apóstrofo y no una entidad que el lector OSIS procese mal.
    s = html.unescape(html.unescape(s))
    # Restos de OCR, no entidades deliberadas del texto impreso.  Si se
    # dejan pasar, html.escape los hace XML válido, pero osis2mod los vuelve
    # a interpretar como entidades mal formadas y deja basura visible.
    s = s.replace("&quot;", '"')
    s = re.sub(r"&#(?:x[0-9A-Fa-f]+|\d+)?;?", " ", s)
    s = re.sub(r"\s*[”“]\s*", " ", s)
    s = re.sub(r"\(\s*\)", "", s)
    s = re.sub(r"\s+([,.;:!?])", r"\1", s)
    s = re.sub(r"\s{2,}", " ", s)
    return s.strip(" ,;:")


def genera(texto, destino, introducciones=None):
    faltan = 0
    introducciones = introducciones or {}
    reconstruidos_path = os.path.join(DIR, "reconstruidos.txt")
    reconstruidos = set(open(reconstruidos_path, encoding="utf-8").read().splitlines()) if os.path.exists(reconstruidos_path) else set()
    with open(destino, "w", encoding="utf-8") as f:
        f.write(CABECERA)
        for osisid in ORDEN:
            L = POR_OSIS[osisid]
            f.write(f'  <div type="book" osisID="{osisid}">\n')
            intro = introducciones.get(osisid)
            if intro:
                intro = limpia_final(intro)
                if intro:
                    f.write('   <div type="introduction"><p>'
                            f"{html.escape(intro, quote=False)}</p></div>\n")
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
                    ref = f"{osisid} {c}:{v}"
                    estado = "reconstruido-testigos" if ref in reconstruidos else "ocr-facsímil"
                    f.write(f'    <verse osisID="{osisid}.{c}.{v}"><seg type="{estado}">'
                            f"{html.escape(t, quote=False)}</seg></verse>\n")
                f.write("   </chapter>\n")
            f.write("  </div>\n")
        f.write(" </osisText>\n</osis>\n")
    return faltan


if __name__ == "__main__":
    texto = json.load(open(os.path.join(DIR, "texto.json"), encoding="utf-8"))
    intro_path = os.path.join(DIR, "introducciones.json")
    introducciones = {}
    if os.path.exists(intro_path):
        introducciones = json.load(open(intro_path, encoding="utf-8"))
    total = sum(sum(POR_OSIS[o]["versos"]) for o in ORDEN)
    dest = os.path.join(DIR, "salida", "nacarcolunga.osis.xml")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    faltan = genera(texto, dest, introducciones)
    print(f"OSIS escrito: {total - faltan}/{total} versículos con texto "
          f"({100 * (total - faltan) / total:.1f}%), {faltan} vacíos")
