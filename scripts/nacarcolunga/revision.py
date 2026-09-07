"""Genera una cola de cotejo visual, ordenada por riesgo.

No corrige ni completa texto: cada entrada apunta a la hoja/columna/caja
del facsímil que debe revisar una persona.
"""
import json
import os
import re

from canon import ORDEN, POR_OSIS

DIR = os.path.dirname(os.path.abspath(__file__))
TOK = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+")


def refs_canon():
    for libro in ORDEN:
        for cap, total in enumerate(POR_OSIS[libro]["versos"], 1):
            for ver in range(1, total + 1):
                yield f"{libro} {cap}:{ver}"


def main():
    texto = json.load(open(os.path.join(DIR, "texto.json"), encoding="utf-8"))
    origen = json.load(open(os.path.join(DIR, "procedencia.json"), encoding="utf-8"))
    reconstruidos = set(open(os.path.join(DIR, "reconstruidos.txt"), encoding="utf-8").read().splitlines())
    cola = []
    for ref in refs_canon():
        t = texto.get(ref)
        fuentes = origen.get(ref, [])
        conf = min((x.get("confianza", 100) for x in fuentes), default=0)
        motivos, riesgo = [], 0
        if not t:
            motivos.append("ausente"); riesgo += 100
        elif len(TOK.findall(t)) < 5:
            motivos.append("fragmento"); riesgo += 70
        if ref in reconstruidos:
            motivos.append("reconstruido-con-testigos"); riesgo += 35
        if t and re.search(r"[&#%]|\b(?:GÉNE|SAN)\b", t):
            motivos.append("artefacto-ocr"); riesgo += 30
        if conf and conf < 80:
            motivos.append("baja-confianza"); riesgo += int(80-conf)
        if motivos:
            cola.append({"ref": ref, "riesgo": riesgo, "motivos": motivos,
                         "texto": t, "origen": fuentes})
    cola.sort(key=lambda x: (-x["riesgo"], x["ref"]))
    out = os.path.join(DIR, "revision.json")
    json.dump(cola, open(out, "w", encoding="utf-8"), ensure_ascii=False, indent=2)
    print(f"{len(cola)} entradas: {out}")


if __name__ == "__main__":
    main()
