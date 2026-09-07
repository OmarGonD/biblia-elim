"""Módulo de comentario con las notas, agrupadas por capítulo."""
import html
import json
import os
import re

from canon import POR_OSIS

DIR = os.path.dirname(os.path.abspath(__file__))

# Correcciones cotejadas con el facsímil o inequívocas por contexto. No se
# usa un corrector ortográfico indiscriminado: en una obra bíblica abundan
# nombres propios, arcaísmos y tecnicismos que no deben modernizarse por
# conjetura.
CORRECCIONES_OCR = (
    (
        r"el Espíritu de Dios que ha recibido\.\s*\(2\) La caridad fraterna "
        r"es el signo mis tico de que estamos en gracia, de que pasado de la "
        r"muerte del pecado a la vida justicia y de la grada\.",
        "(1) No peca mientras se deje gobernar por el Espíritu de Dios que "
        "ha recibido. (2) La caridad fraterna es el signo más auténtico de "
        "que estamos en gracia, de que hemos pasado de la muerte del pecado "
        "a la vida de la justicia y de la gracia.",
    ),
    (r"\ben tonces\b", "entonces"),
    (r"\bEs píritu\b", "Espíritu"),
    (r"\bEspíritu Samo\b", "Espíritu Santo"),
    (r"\bvisi ble\b", "visible"),
    (r"\bsin em:\s*bargo\b", "sin embargo"),
    (r"\btoio\b", "todo"),
    (r"\besle\b", "este"),
    (r"\besie\b", "este"),
    (r"\bJerjsalén\b", "Jerusalén"),
    (r"\blugnr\b", "lugar"),
    (r"\bviaie\b", "viaje"),
    (r"\bJocl\b", "Joel"),
    (r"\blimites\b", "límites"),
    (r"\bjudias\b", "judías"),
    (r"\bjudio\b", "judío"),
    (r"\blinea\b", "línea"),
    (r"\bintima\b", "íntima"),
    (r"\btitulo\b", "título"),
)


def corrige_ocr(texto):
    """Aplica únicamente enmiendas verificadas y reproducibles."""
    for patron, reemplazo in CORRECCIONES_OCR:
        texto = re.sub(patron, reemplazo, texto)
    return texto


def limpia(n):
    n = corrige_ocr(n)
    n = re.sub(r"^\s*(?:\d{1,2}|\([0-9ivx]+\)|[*'’‘\"”“$e†‡~^])\s+", "", n)
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
            buenas = [n for n in buenas if len(n) > 20]
            if not buenas:
                continue
            caps += 1
            f.write(f"$$${osis} {cap}:1-{nver}\n")
            f.write(f"<p><i>Notas de Nácar-Colunga al capítulo {cap}. "
                    f"Van en el orden impreso; el original no permite "
                    f"saber a qué versículo pertenece cada una.</i></p>\n")
            for n in buenas:
                f.write(f"<p>{html.escape(n)}</p>\n")
    return caps


if __name__ == "__main__":
    path = os.path.join(DIR, "notas.json")
    notas = json.load(open(path, encoding="utf-8"))
    dest = os.path.join(DIR, "salida", "nacarcolunga-notas.imp")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    c = genera(notas, dest)
    print(f"capítulos con notas: {c}")
