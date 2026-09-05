"""Genera el módulo de comentario con las notas, por capítulo."""
import json, html, re
from canon import POR_OSIS

def limpia(n):
    n = re.sub(r"^\s*(?:\d{1,2}|[*'’‘\"”“$e†‡~^])\s+", "", n)   # la llamada
    n = re.sub(r"\s+([,.;:!?])", r"\1", n)
    return re.sub(r"\s{2,}", " ", n).strip()

def genera(notas, destino):
    caps = 0
    with open(destino, "w", encoding="utf-8") as f:
        for ref, lista in sorted(notas.items()):
            osis, cap = ref.rsplit(" ", 1)
            cap = int(cap)
            if osis not in POR_OSIS or cap > POR_OSIS[osis]["caps"]:
                continue
            nver = POR_OSIS[osis]["versos"][cap - 1]
            buenas = [limpia(n) for n in lista]
            buenas = [n for n in buenas if len(n) > 12]
            if not buenas:
                continue
            caps += 1
            f.write(f"$$${osis} {cap}:1-{nver}\n")
            f.write(f"<p><i>Notas de Torres Amat al capítulo {cap}. "
                    f"Van en el orden en que están impresas; el original no "
                    f"permite saber a qué versículo pertenece cada una.</i></p>\n")
            for n in buenas:
                f.write(f"<p>{html.escape(n)}</p>\n")
    return caps

if __name__ == "__main__":
    notas = json.load(open("notas.json", encoding="utf-8"))
    c = genera(notas, "torresamat-notas.imp")
    print(f"capítulos escritos: {c}")
