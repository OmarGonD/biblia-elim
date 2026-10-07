"""Erratas OCR inequívocas de 1882, ancladas al texto y al versículo.

Uso: python3 contexto.py --origen ../../modulos --salida salida/contexto

La tabla contexto.tsv conserva cada fragmento anterior y su corrección.
El origen solo se lee. Se reimporta y verifica el árbol SWORD de salida;
después se convierte con el conversor habitual a SQLite (véase README.md).
No se corrige texto en el backend ni durante la lectura.
"""
import argparse
import os
import re

from barrido import aplica_frases, lee_frases, reescribe
from parche_facsimil import INSTALADO

DIR = os.path.dirname(os.path.abspath(__file__))
TABLA = os.path.join(DIR, "contexto.tsv")
VERSION = "0.9.17"


def corrige(clave, texto, frases):
    nuevo, cambios = aplica_frases(clave, texto, frases)
    # Las llamadas de nota mal leídas están en el texto visible. El marcado
    # OSIS (incluidos cierres de capítulo) debe conservarse exactamente.
    if re.findall(r"<[^>]*>", texto) != re.findall(r"<[^>]*>", nuevo):
        raise ValueError(f"{clave}: la corrección alteraría el marcado OSIS")
    return nuevo, cambios


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "contexto"))
    ap.add_argument("--tabla", default=TABLA)
    args = ap.parse_args()
    frases = lee_frases(args.tabla)
    total, versos, destino = reescribe(
        args.origen, args.salida, lambda clave, texto: corrige(clave, texto, frases))
    conf = os.path.join(args.salida, "mods.d", "torresamat.conf")
    with open(conf, encoding="utf-8") as f:
        contenido = f.read()
    # La conversión automática detecta la nueva revisión del texto; no se
    # cambia la revisión de las notas ni se rebaja una versión posterior.
    contenido = re.sub(r"^Version=0\.9\.(?:14|15|16)$", f"Version={VERSION}",
                       contenido, flags=re.M)
    with open(conf, "w", encoding="utf-8") as f:
        f.write(contenido)
    print(f"{total} fragmentos corregidos en {versos} versos; módulo en {destino}")


if __name__ == "__main__":
    main()
