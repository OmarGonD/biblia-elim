"""
Notas al pie con la letra del cuerpo, pegadas al versículo (causa 1 de las
colas): los datos del facsímil dicen dónde empiezan y esta herramienta lo
traslada al módulo.

Algunas hojas componen las notas con una letra casi igual que la del texto,
y el corte por altura de segment.punto_de_corte las deja en el cuerpo. Lo
que sí las separa es un hueco vertical grande antes de la primera nota, que
empieza por su número. Para cada media hoja con ese hueco se toman las líneas
que quedaron del lado del cuerpo (hueco..corte), se localizan en el módulo
por secuencias de cuatro palabras y se escribe en barrido_notas.tsv el trozo
exacto del versículo que hay que quitar (referencia, texto, vacío), en el
formato de barrido_frases.tsv.

Necesita tomoI..IV.djvu.xml junto a load.py (véase README); el resultado va
al repositorio, así que no hace falta volver a ejecutarlo para regenerar el
módulo.

Uso:

    python3 fugas_notas.py [--modulo RAIZ] [--salida barrido_notas.tsv]
"""
import argparse
import collections
import difflib
import html
import os
import re
import statistics
import unicodedata

from load import TOMOS, X1, X2, TOP, BOT, load
from parche_facsimil import conf_de, exporta_aislado, lee_imp
from segment import (agrupar_lineas, canal, med_y, punto_de_corte,
                     recortar_margen, texto)

DIR = os.path.dirname(os.path.abspath(__file__))
K = 4
HUECO_MIN = 28
RE_VERSO = re.compile(r"^\d{1,3}[.,]\s")


def norm(s):
    s = unicodedata.normalize("NFD", html.unescape(s).lower())
    s = "".join(c for c in s if unicodedata.category(c) != "Mn")
    return re.findall(r"[a-z]+", s)


def mitades(page):
    g = canal(page)
    for ws in ([w for w in page["words"] if w[X2] <= g],
               [w for w in page["words"] if w[X1] > g]):
        lineas = recortar_margen(agrupar_lineas(ws)) if ws else []
        if lineas and med_y(lineas[0]) < page["h"] * 0.09:
            lineas = lineas[1:]
        yield lineas


def hueco_de_notas(lineas, corte):
    """Índice de la línea donde empiezan las notas, o None."""
    if len(lineas) < 8:
        return None
    huecos = [None] + [min(w[TOP] for w in lineas[i])
                       - max(w[BOT] for w in lineas[i - 1])
                       for i in range(1, len(lineas))]
    med = statistics.median(h for h in huecos if h is not None)
    for i in range(max(3, len(lineas) // 3), min(corte, len(lineas))):
        if huecos[i] >= max(HUECO_MIN, 3 * med):
            primera = lineas[i][0][5]
            if re.match(r"^\d{1,2}$", primera) or primera in ("+", "*", "T"):
                return i
    return None


def grupos():
    """[(tomo, pagina, lado, [líneas del cuerpo que son notas])]"""
    salida = []
    for t in TOMOS:
        for pi, p in enumerate(load(t)):
            for lado, lineas in enumerate(mitades(p)):
                if not lineas:
                    continue
                k = punto_de_corte(lineas)
                c = hueco_de_notas(lineas, k)
                if c is None or k - c < 2:
                    continue
                textos = [texto(l) for l in lineas[c:k]]
                if any(RE_VERSO.match(x) for x in textos):
                    continue          # hay versículos: no son notas
                salida.append((t, pi, lado, textos))
    return salida


def indice(entradas):
    idx, toks = collections.defaultdict(set), {}
    for clave, cuerpo in entradas:
        t = norm(re.sub(r"<[^>]+>", " ", cuerpo))
        toks[clave] = t
        for i in range(len(t) - K + 1):
            idx[tuple(t[i:i + K])].add(clave)
    return idx


def tramo(verso, textos):
    """Trozo exacto de `verso` (sin marcado) que casa con las líneas, o None.

    Va desde el primer hasta el último bloque de palabras que coinciden y se
    amplía por delante con la cifra o signo de la nota.
    """
    if "<" in verso:
        partes = re.split(r"<[^>]+>", verso)
        verso = max(partes, key=len)
    fichas = [(m.start(), m.end(), norm(m.group())) for m in re.finditer(r"\S+", verso)]
    a, plano = [], []
    for i, (_, _, t) in enumerate(fichas):
        for w in t:
            a.append(w)
            plano.append(i)
    b = norm(" ".join(textos))
    bloques = [m for m in difflib.SequenceMatcher(None, a, b, autojunk=False)
               .get_matching_blocks() if m.size >= 3]
    if not bloques or sum(m.size for m in bloques) < 0.5 * len(b):
        return None
    ini = plano[bloques[0].a]
    fin = plano[bloques[-1].a + bloques[-1].size - 1]
    while ini > 0 and re.fullmatch(r"[\d+*|©T]{1,3}", verso[fichas[ini - 1][0]:fichas[ini - 1][1]]):
        ini -= 1
    # la ficha de cifra pegada al texto («4 Todos»)
    if re.fullmatch(r"\d{1,2}", verso[fichas[ini][0]:fichas[ini][1]] or ""):
        pass
    return verso[fichas[ini][0] - (1 if fichas[ini][0] else 0):fichas[fin][1]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--modulo", default=os.path.join(DIR, "salida", "colas"))
    ap.add_argument("--salida", default=os.path.join(DIR, "barrido_notas.tsv"))
    args = ap.parse_args()
    entradas = lee_imp(exporta_aislado(args.modulo, conf_de(args.modulo)))
    cuerpos = dict(entradas)
    idx = indice(entradas)
    filas, vistos = [], set()
    for t, pi, lado, textos in grupos():
        n = norm(" ".join(textos))
        votos = collections.Counter()
        for i in range(len(n) - K + 1):
            for clave in idx.get(tuple(n[i:i + K]), ()):
                votos[clave] += 1
        if not votos:
            continue
        clave, v = votos.most_common(1)[0]
        if v < 0.5 * (len(n) - K + 1):
            continue
        quitar = tramo(cuerpos[clave], textos)
        if not quitar or cuerpos[clave].count(quitar) != 1 or (clave, quitar) in vistos:
            continue
        if len(cuerpos[clave].split(quitar)[0].strip()) < 20:
            continue                  # sería casi el verso entero
        vistos.add((clave, quitar))
        filas.append((clave, quitar, t, pi))
    with open(args.salida, "w", encoding="utf-8") as f:
        f.write("# Notas al pie con letra del cuerpo pegadas al verso (fugas_notas.py).\n"
                "# referencia<TAB>texto a quitar<TAB>(vacío); lo lee barrido.py.\n")
        for clave, quitar, t, pi in filas:
            f.write(f"{clave}\t{quitar}\t\n")
    print(f"{len(filas)} tramos de nota en {args.salida}")


if __name__ == "__main__":
    main()
