"""
Palabras griegas de las notas de Torres Amat.

El OCR de archive.org leyó el griego con el modelo del español y lo dejó
irreconocible («eftdvat», «labyasey»). Pero el motor sabe que no está seguro:
esas palabras salen con confianza 0. Esta herramienta busca en las líneas de
nota los trozos sospechosos (confianza baja, no son español ni número romano
ni abreviatura de cita), recorta cada uno de la imagen original del pliego, lo
lee con el modelo griego antiguo de tesseract (grc) y con el moderno (ell), y
solo acepta la lectura si los dos coinciden sin acentos: así el ruido que se
parece al griego (ornatos, español mal leído) no pasa.

El resultado, griego_palabras.json, va al repositorio y lo aplica
notas_barrido.py: {"tomo|pagina": [[token leído, griego], ...]}. Las
imágenes se bajan una a una de archive.org (el zip no hace falta entero) y se
tiran. Necesita tomoI..IV.djvu.xml junto a load.py, tesseract con grc y ell
(TESSDATA_PREFIX con grc.traineddata de tessdata_best y la carpeta configs/ de tesseract, que trae el formato tsv), vips y hunspell con
un diccionario español (--diccionario RUTA sin extensión).

Uso:

    python3 griego_ocr.py --diccionario es [--salida griego_palabras.json]
"""
import argparse
import concurrent.futures
import html
import json
import os
import re
import subprocess
import tempfile
import unicodedata
import urllib.parse

import barrido
import citas_notas
from load import CONF, TOMOS, TXT, X1, X2, load
from segment import (agrupar_lineas, canal, punto_de_corte, recortar_margen)

DIR = os.path.dirname(os.path.abspath(__file__))
ITEM = "la-sagrada-biblia-vulgata-tomo-iiv_202111"
BASE = f"https://archive.org/download/{ITEM}"
MARGEN = 20
CONF_MAX = 30            # confianza del OCR español por debajo de la cual se sospecha
RE_ROMANO = re.compile(r"^[ivxlcm]+$", re.I)
ABREV = {a.lower() for a in citas_notas.ABREV}
RE_GRIEGO = re.compile(r"[Ͱ-Ͽἀ-῿]")


def q(s):
    return urllib.parse.quote(s)


def sospechosas(diccionario):
    """{(tomo, pagina): [(token, caja)]} de palabras de nota con pinta de griego."""
    raw = []
    for t in TOMOS:
        for pi, p in enumerate(load(t)):
            g = canal(p)
            for ws in ([w for w in p["words"] if w[X2] <= g],
                       [w for w in p["words"] if w[X1] > g]):
                if not ws:
                    continue
                ls = recortar_margen(agrupar_lineas(ws))
                if len(ls) < 6:
                    continue
                k = punto_de_corte(ls)
                for l in ls[k:]:
                    for w in l:
                        raw.append((t, pi, w))
    letras = lambda s: re.sub(r"[^A-Za-zÁÉÍÓÚáéíóúñÑ]", "", s)
    toks = sorted({letras(w[TXT]).lower() for _, _, w in raw})
    malas = set(subprocess.run(["hunspell", "-d", diccionario, "-l"],
                               input="\n".join(toks), capture_output=True,
                               text=True).stdout.split("\n"))
    tabla = barrido.lee_tabla(barrido.TABLA)
    out = {}
    for t, pi, w in raw:
        tok = letras(w[TXT])
        bajo = tok.lower()
        if (len(w[TXT]) >= 4 and tok == bajo and bajo in malas and w[CONF] <= CONF_MAX
                and not RE_ROMANO.match(bajo) and bajo not in ABREV
                and barrido.corrige(bajo, tabla)[1] == 0):
            out.setdefault((t, pi), []).append((w[TXT], (w[0], w[1], w[2], w[3])))
    return out


def lee(png, lengua, tessdata):
    """(texto, confianza media) de tesseract para una palabra suelta."""
    r = subprocess.run(["tesseract", png, "-", "-l", lengua, "--psm", "8", "--dpi", "300", "tsv"],
                       capture_output=True, text=True,
                       env=dict(os.environ, TESSDATA_PREFIX=tessdata))
    texto, confs = "", []
    for linea in r.stdout.splitlines()[1:]:
        c = linea.split("\t")
        if len(c) == 12 and c[11].strip():
            texto += c[11].strip()
            confs.append(float(c[10]))
    return texto, (sum(confs) / len(confs) if confs else 0.0)


def clave_griega(g):
    """La palabra sin acentos, espíritus, mayúsculas ni puntuación: σ final = σ."""
    d = unicodedata.normalize("NFD", g.lower())
    d = "".join(c for c in d if unicodedata.category(c) == "Ll" or c == "σ")
    return d.replace("ς", "σ")


def normaliza(g):
    """La ϛ (estigma) del impreso, que tesseract da como ς dentro de palabra, es «στ»."""
    g = g.replace("ϛ", "στ")
    return re.sub(r"ς(?=[Ͱ-Ͽἀ-῿])", "στ", g)


def vocabulario(modulos=("TR", "Tisch")):
    """{clave sin acentos: forma acentuada más frecuente} del Nuevo Testamento griego."""
    cuenta = {}
    for m in modulos:
        r = subprocess.run(["mod2imp", m], capture_output=True)
        for w in re.findall(r"[\u0370-\u03ff\u1f00-\u1fff]+",
                            html.unescape(r.stdout.decode("utf-8", "replace"))):
            if w != w.lower():
                continue
            cuenta.setdefault(clave_griega(w), {}).setdefault(w, 0)
            cuenta[clave_griega(w)][w] += 1
    # entre las formas de una clave, la que lleva acento y es más frecuente
    return {k: max(v, key=lambda f: (sum(1 for c in unicodedata.normalize("NFD", f)
                                         if unicodedata.category(c) == "Mn"), v[f]))
            for k, v in cuenta.items()}


def acentua(g, vocab):
    """Restituye los acentos si la palabra está en el Nuevo Testamento; si no, como sale."""
    m = re.match(r"^(\W*)(\w+)(\W*)$", g)
    if not m:
        return g
    mejor = vocab.get(clave_griega(m.group(2)))
    return m.group(1) + (mejor or m.group(2)) + m.group(3)


def procesa(clave, palabras, tessdata):
    tomo, pagina = clave
    nom = f"LA SAGRADA BIBLIA - Vulgata tomo {tomo}"
    url = f"{BASE}/{q(nom + '_jp2.zip')}/{q(nom + '_jp2/' + nom + '_' + format(pagina, '04d') + '.jp2')}"
    res = []
    with tempfile.TemporaryDirectory() as tmp:
        jp2 = os.path.join(tmp, "p.jp2")
        subprocess.run(["curl", "-sL", "--max-time", "300", "-o", jp2, url], check=False)
        if not os.path.getsize(jp2):
            return clave, None
        for token, (x1, x2, y1, y2) in palabras:
            png = os.path.join(tmp, "w.png")
            r = subprocess.run(["vips", "extract_area", jp2, png, str(max(0, x1 - MARGEN)),
                                str(max(0, y1 - MARGEN)), str(x2 - x1 + 2 * MARGEN),
                                str(y2 - y1 + 2 * MARGEN)], capture_output=True)
            if r.returncode or not os.path.exists(png):
                continue
            grc, cg = lee(png, "grc", tessdata)
            mod, cm = lee(png, "ell", tessdata)
            grc = normaliza(grc)
            n = len(RE_GRIEGO.findall(grc))
            solo_letras = re.sub(r"[^\w]", "", grc)
            # El modelo antiguo y el moderno han de coincidir (sin acentos ni
            # espíritus): si no, es ruido que se parece al griego.
            if (n >= 4 and n >= 0.8 * len(solo_letras) and clave_griega(grc) == clave_griega(normaliza(mod))
                    and not re.search(r"(.)\1\1", grc) and not re.search(r"\w[Α-Ω]", grc)):
                res.append([token, grc, round(cg), round(cm)])
    return clave, res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--diccionario", required=True)
    ap.add_argument("--tessdata", default=os.environ.get("TESSDATA_PREFIX", "/usr/share/tessdata"))
    ap.add_argument("--salida", default=os.path.join(DIR, "griego_palabras.json"))
    ap.add_argument("--hilos", type=int, default=6)
    ap.add_argument("--cache", default=None,
                    help="JSON lines con las páginas ya leídas; permite reanudar")
    ap.add_argument("--max-paginas", type=int, default=0)
    ap.add_argument("--tomo", default=None)
    ap.add_argument("--desde", type=int, default=0)
    ap.add_argument("--hasta", type=int, default=10 ** 6)
    args = ap.parse_args()
    sosp = sospechosas(args.diccionario)
    claves = [c for c in sorted(sosp)
              if (not args.tomo or c[0] == args.tomo) and args.desde <= c[1] < args.hasta]
    if args.max_paginas:
        claves = claves[:args.max_paginas]
    print(f"{sum(len(sosp[c]) for c in claves)} palabras sospechosas en {len(claves)} páginas", flush=True)
    cache = args.cache or args.salida + ".cache"
    hechas, salida = set(), {}
    if os.path.exists(cache):
        for linea in open(cache, encoding="utf-8"):
            k, res = json.loads(linea)
            hechas.add(k)
            if res:
                salida[k] = res
    pendientes = [c for c in claves if "%s|%d" % c not in hechas]
    print(f"{len(hechas)} páginas ya hechas, {len(pendientes)} por hacer", flush=True)
    with concurrent.futures.ThreadPoolExecutor(args.hilos) as ex, \
            open(cache, "a", encoding="utf-8") as cf:
        futuros = [ex.submit(procesa, c, sosp[c], args.tessdata) for c in pendientes]
        for i, f in enumerate(concurrent.futures.as_completed(futuros), 1):
            clave, res = f.result()
            if res is not None:
                cf.write(json.dumps(["%s|%d" % clave, res], ensure_ascii=False) + "\n")
                cf.flush()
            if res:
                salida["%s|%d" % clave] = res
            if i % 25 == 0:
                print(f"  {i}/{len(claves)} páginas, {sum(len(v) for v in salida.values())} lecturas", flush=True)
    vocab = vocabulario()
    for lecturas in salida.values():
        for lec in lecturas:
            lec[1] = acentua(lec[1], vocab)
    with open(args.salida, "w", encoding="utf-8") as fh:
        json.dump(salida, fh, ensure_ascii=False, indent=0, sort_keys=True)
        fh.write("\n")
    print(f"{sum(len(v) for v in salida.values())} palabras griegas leídas en {len(salida)} páginas")


if __name__ == "__main__":
    main()
