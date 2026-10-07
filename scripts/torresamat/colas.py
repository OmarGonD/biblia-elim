"""
Quita del último verso de cada capítulo el encabezado «CAPITULO XXIX» y el
argumento del capítulo siguiente que el OCR le pegó al final.

Solo se corta en el último versículo del capítulo, desde la ficha que se
parece a «CAPITULO» hasta el final, y solo si antes quedan al menos
MIN_ANTES caracteres de verso. El marcado del final (cierre del capítulo)
se conserva. Lo cortado no se pierde: se escribe, con su
referencia, en colas_quitadas.tsv, por si algún día se quiere devolver como
encabezado de capítulo.

Uso:

    python3 colas.py [--origen RAIZ] [--salida DIR]
"""
import argparse
import difflib
import os
import re

from barrido import DIR, INSTALADO, LETRA, reescribe
from parche_facsimil import conf_de, exporta_aislado, lee_imp

MIN_ANTES = 20
CAPITULO = ("CAPITULO", "CAPÍTULO")
REGISTRO = os.path.join(DIR, "colas_quitadas.tsv")
RE_FICHA = re.compile(r"\S+")
RE_MARCA = re.compile(r"<[^>]*>")


def parece_capitulo(ficha):
    pal = re.sub(r"[^%s]" % LETRA, "", ficha)
    if not 6 <= len(pal) <= 10 or pal != pal.upper():
        return False
    return max(difflib.SequenceMatcher(None, pal, c).ratio()
               for c in CAPITULO) >= 0.8


def corta(texto):
    """(verso sin la cola, cola) o (texto, "")."""
    for m in RE_FICHA.finditer(texto):
        if parece_capitulo(m.group()):
            antes = texto[:m.start()].rstrip()
            if len(antes) >= MIN_ANTES:
                cola = texto[m.start():]
                # El marcado del final (cierre del capítulo…) se queda.
                marcas = RE_MARCA.findall(cola)
                sin_marcas = RE_MARCA.sub("", cola).strip()
                return (antes + (" " + " ".join(marcas) if marcas else ""),
                        sin_marcas)
            return texto, ""
    return texto, ""


def ultimos_versos(entradas):
    """Claves del último versículo de cada capítulo."""
    ultimos, previo = set(), None
    for clave, _ in entradas:
        m = re.match(r"(.*) (\d+):(\d+)$", clave)
        if not m or m.group(3) == "0" or m.group(2) == "0":
            continue
        capitulo = (m.group(1), m.group(2))
        if previo and previo[0] != capitulo:
            ultimos.add(previo[1])
        previo = (capitulo, clave)
    if previo:
        ultimos.add(previo[1])
    return ultimos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "colas"))
    ap.add_argument("--registro", default=REGISTRO)
    args = ap.parse_args()

    ultimos = ultimos_versos(
        lee_imp(exporta_aislado(args.origen, conf_de(args.origen))))
    quitadas = {}

    def transforma(clave, cuerpo):
        if clave not in ultimos:
            return cuerpo, 0
        nuevo, cola = corta(cuerpo)
        if not cola:
            return cuerpo, 0
        quedan = RE_MARCA.sub("", nuevo).split() + cola.split()
        if RE_MARCA.findall(nuevo) != RE_MARCA.findall(cuerpo) or \
                RE_MARCA.sub("", cuerpo).split() != quedan:
            raise SystemExit(f"{clave}: el corte cambió otra cosa")
        quitadas[clave] = cola.replace("\n", " ").replace("\t", " ")
        return nuevo, 1

    total, versos, destino = reescribe(args.origen, args.salida, transforma)
    previas = {}
    if os.path.exists(args.registro):
        with open(args.registro, encoding="utf-8") as f:
            previas = dict(l.rstrip("\n").split("\t", 1) for l in f if "\t" in l)
    previas.update(quitadas)
    with open(args.registro, "w", encoding="utf-8") as f:
        for clave, cola in previas.items():
            f.write(f"{clave}\t{cola}\n")
    print(f"{total} colas quitadas; módulo en {destino}")


if __name__ == "__main__":
    main()
