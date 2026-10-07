"""
Ubica las notas de Torres Amat en el rango de versículos de su página.

notas.py las publicó por capítulo porque la llamada de nota no se puede
leer. Pero sí se sabe en qué media hoja está cada nota y qué versículos
trae el cuerpo de esa media hoja: es el rango de versículos que la nota
comenta, con la precisión de una página. Eso evita que una nota de la hoja
que va de Mc 1:27 a Mc 2:5 aparezca junto a Mc 1:12, o que las del
principio del capítulo siguiente caigan en el anterior.

Para cada media hoja se localiza el cuerpo en el módulo de texto por
secuencias de cuatro palabras, ponderando cada una por lo rara que es (las
frases hechas no cuentan), y se queda el tramo contiguo de versículos con
más peso. Las notas se parten con notas.agrupa_notas.

Necesita tomoI..IV.djvu.xml junto a load.py (véase README). El resultado,
notas_paginas.json, va al repositorio: notas_barrido.py parte de él, de modo
que no hace falta volver a ejecutar esto.

Uso:

    python3 notas_paginas.py [--modulo RAIZ] [--salida notas_paginas.json]
"""
import argparse
import collections
import html
import json
import os
import re
import unicodedata

from cabeceras import medias_hojas
from load import TOMOS, load
from notas import agrupa_notas
from paginas import pagina
from parche_facsimil import conf_de, exporta_aislado, lee_imp
from segment import punto_de_corte, texto

DIR = os.path.dirname(os.path.abspath(__file__))
K = 4
MIN_PESO = 3.0        # peso mínimo de un versículo para contar
HUECO = 4             # versículos que puede saltarse un tramo contiguo
RE_CLAVE = re.compile(r"(.*) (\d+):(\d+)$")


def norm(s):
    s = unicodedata.normalize("NFD", html.unescape(s).lower())
    s = "".join(c for c in s if unicodedata.category(c) != "Mn")
    return re.findall(r"[a-z]+", s)


class Indice:
    def __init__(self, entradas):
        self.orden = [k for k, _ in entradas if RE_CLAVE.match(k) and not k.endswith(":0")]
        self.pos = {k: i for i, k in enumerate(self.orden)}
        self.idx = collections.defaultdict(set)
        for k, v in entradas:
            if k not in self.pos:
                continue
            t = norm(re.sub(r"<[^>]+>", " ", v))
            for i in range(len(t) - K + 1):
                self.idx[tuple(t[i:i + K])].add(k)

    def pesos(self, lineas):
        t = norm(" ".join(texto(l) for l in lineas))
        w = collections.Counter()
        for i in range(len(t) - K + 1):
            vs = self.idx.get(tuple(t[i:i + K]), ())
            if 0 < len(vs) <= 3:
                for k in vs:
                    w[k] += 1 / len(vs)
        return w

    def rango(self, lineas):
        """(primer versículo, último, peso) del tramo contiguo más pesado."""
        w = self.pesos(lineas)
        ks = sorted((self.pos[k] for k, x in w.items() if x >= 1.0))
        if not ks:
            return None
        mejor, ini, acum = None, 0, 0.0
        grupos, cur = [], [ks[0]]
        for a in ks[1:]:
            if a - cur[-1] <= HUECO:
                cur.append(a)
            else:
                grupos.append(cur)
                cur = [a]
        grupos.append(cur)
        for g in grupos:
            peso = sum(w[self.orden[i]] for i in g)
            if mejor is None or peso > mejor[2]:
                mejor = (self.orden[g[0]], self.orden[g[-1]], peso)
        return mejor if mejor[2] >= MIN_PESO else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modulo", default=os.path.join(DIR, "salida", "barrido3"))
    ap.add_argument("--salida", default=os.path.join(DIR, "notas_paginas.json"))
    args = ap.parse_args()
    indice = Indice(lee_imp(exporta_aislado(args.modulo, conf_de(args.modulo))))
    grupos, sin_rango, sin_notas = [], 0, 0
    for tomo in TOMOS:
        originales = load(tomo)
        for i in range(len(originales)):
            p = pagina(tomo, i, originales)
            if not p["words"]:
                continue
            for lado, (cab, ls) in enumerate(medias_hojas(p)):
                if not ls:
                    continue
                k = punto_de_corte(ls)
                if k == 0 or k >= len(ls):
                    continue
                notas = agrupa_notas(ls[k:])
                if not notas:
                    sin_notas += 1
                    continue
                r = indice.rango(ls[:k])
                if r is None:
                    sin_rango += 1
                    r = (None, None, 0.0)
                grupos.append({"tomo": tomo, "pagina": i, "lado": lado,
                               "desde": r[0], "hasta": r[1], "peso": round(r[2], 1),
                               "notas": notas})
    with open(args.salida, "w", encoding="utf-8") as f:
        json.dump(grupos, f, ensure_ascii=False, indent=0)
        f.write("\n")
    n = sum(len(g["notas"]) for g in grupos)
    print(f"{len(grupos)} medias hojas con notas ({n} notas); {sin_rango} sin rango")


if __name__ == "__main__":
    main()
