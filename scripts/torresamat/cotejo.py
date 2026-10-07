"""Restituciones de Torres Amat por cotejo (0.9.18 → 0.9.19).

Uso: python3 scripts/torresamat/cotejo.py --origen modulos
     --salida build/torresamat-cotejo

cotejo.json registra el texto anterior, la restitución y sus testigos.
También restituye cuatro versículos cuyo texto estaba en otra entrada.
Se exige igualdad exacta; nunca se sobrescribe una entrada desconocida.
"""
import argparse
import json
import os
import re
import sys

from barrido import reescribe
from parche_facsimil import INSTALADO, conf_de

DIR = os.path.dirname(os.path.abspath(__file__))
TABLA = os.path.join(DIR, "cotejo.json")
VERSION = "0.9.19"


def lee_cotejo(ruta=TABLA):
    with open(ruta, encoding="utf-8") as f:
        plan = json.load(f)
    cambios = {}
    for record in plan["corrections"]:
        key = record["reference"]
        if key in cambios or record["before"] == record["after"]:
            raise ValueError(f"{key}: corrección duplicada o sin cambio")
        if not record["witnesses"]:
            raise ValueError(f"{key}: faltan testigos del cotejo")
        cambios[key] = record
    return cambios


def corrige(clave, texto, cambios):
    record = cambios.get(clave)
    if record is None or texto == record["after"]:
        return texto, 0
    if texto != record["before"]:
        raise ValueError(f"{clave}: el texto no coincide con el cotejo")
    nuevo = record["after"]
    if re.findall(r"<[^>]*>", texto) != re.findall(r"<[^>]*>", nuevo):
        raise ValueError(f"{clave}: la restitución alteraría el marcado OSIS")
    return nuevo, 1


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "cotejo"))
    ap.add_argument("--tabla", default=TABLA)
    args = ap.parse_args()
    conf = conf_de(args.origen)
    if not re.search(r"^Version=0\.9\.(?:18|19)$", conf, re.M):
        sys.exit("El cotejo requiere TorresAmat 0.9.18 o 0.9.19; aplicar primero preposiciones.py")
    cambios = lee_cotejo(args.tabla)
    total, versos, destino = reescribe(
        args.origen, args.salida, lambda key, text: corrige(key, text, cambios))
    config = os.path.join(args.salida, "mods.d", "torresamat.conf")
    with open(config, encoding="utf-8") as f:
        conf = f.read()
    conf = re.sub(r"^Version=0\.9\.18$", f"Version={VERSION}", conf, flags=re.M)
    with open(config, "w", encoding="utf-8") as f:
        f.write(conf)
    print(f"{total} entradas restituidas en {versos} versos; módulo en {destino}")


if __name__ == "__main__":
    main()
