"""Revisión contextual de conectores de Torres Amat 1882 (0.9.17 → 0.9.18).

Uso desde la raíz: python3 scripts/torresamat/preposiciones.py
    --origen modulos --salida build/torresamat-preposiciones

Cada cambio de preposiciones.tsv está anclado a su versículo y fragmento.
No hay sustituciones globales: d puede ser á, ó, de o ruido; ú también
puede ser una conjunción válida. El árbol de origen solo se lee.
"""
import argparse
import os
import re
import sys

from barrido import lee_frases, reescribe
from contexto import corrige
from parche_facsimil import INSTALADO, conf_de

DIR = os.path.dirname(os.path.abspath(__file__))
TABLA = os.path.join(DIR, "preposiciones.tsv")
VERSION = "0.9.18"


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "preposiciones"))
    ap.add_argument("--tabla", default=TABLA)
    args = ap.parse_args()
    conf = conf_de(args.origen)
    if not re.search(r"^Version=0\.9\.(?:17|18)$", conf, re.M):
        sys.exit("La revisión requiere TorresAmat 0.9.17 o 0.9.18; aplicar primero contexto.py")
    frases = lee_frases(args.tabla)
    total, versos, destino = reescribe(
        args.origen, args.salida, lambda clave, texto: corrige(clave, texto, frases))
    config = os.path.join(args.salida, "mods.d", "torresamat.conf")
    with open(config, encoding="utf-8") as f:
        conf = f.read()
    conf = re.sub(r"^Version=0\.9\.17$", f"Version={VERSION}", conf, flags=re.M)
    with open(config, "w", encoding="utf-8") as f:
        f.write(conf)
    print(f"{total} fragmentos corregidos en {versos} versos; módulo en {destino}")


if __name__ == "__main__":
    main()
