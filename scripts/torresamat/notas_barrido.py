"""
Ortografía y ruido de las notas de Torres Amat (módulo de comentario
TorresAmatNotas): aplica a cada párrafo de cada capítulo las mismas
correcciones de palabras que el texto (barrido_palabras.tsv más
barrido_palabras_notas.tsv), quita los tramos de ornato de lámina leídos como
letras y descarta los párrafos que son solo ruido.

Parte de notas_paginas.json (notas_paginas.py): cada nota con el rango de
versículos de su media hoja. Se limpian los párrafos, se reparten los
versículos de cada capítulo en tramos que comparten las mismas hojas, y cada
tramo (`$$$Libro cap:a-b`, BlockType=CHAPTER) lleva las notas de sus hojas,
cada grupo con su cabecera «de la página que va de 1:27 a 2:5». Se reimporta
con imp2vs -b 3 y se comprueba la ida y vuelta.

Uso:

    python3 notas_barrido.py [--paginas JSON] [--texto RAIZ] [--salida DIR]

--texto es el árbol SWORD del texto TorresAmat (por defecto ~/.sword), de
donde salen el orden y el número de versículos de cada capítulo.
"""
import argparse
import collections
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import barrido
import citas_notas
import ornatos
from parche_facsimil import V11N, escribe_imp, exporta, lee_imp, exporta_aislado, conf_de, INSTALADO

DIR = os.path.dirname(os.path.abspath(__file__))
TABLA_NOTAS = os.path.join(DIR, "barrido_palabras_notas.tsv")
GRIEGO = os.path.join(DIR, "griego_palabras.json")
NOMBRE = "TorresAmatNotas"
RE_P = re.compile(r"<p>(.*?)</p>", re.S)
MIN_PARRAFO = 12


def une_tablas(*rutas):
    exactas, sin_caja = {}, {}
    for r in rutas:
        e, s = barrido.lee_tabla(r)
        exactas.update(e)
        sin_caja.update(s)
    return exactas, sin_caja


def palabras_reales(texto):
    return [w for w in re.findall(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ\u0370-\u03ff\u1f00-\u1fff]+", texto)
            if len(w) >= 4 and (w.islower() or w[0].isupper() and w[1:].islower())]


RE_CITA = re.compile(r"\b(?:Cap|Véase|Vease|Gen|Exod|Levit|Num|Deut|Deuter|Jos|Josue|Judic|Reg|Paral|Esdr|Tob|Job|Ps|Psalm|Prov|Eccl|Sap|Eccli|Isai|Jerem|Ezech|Dan|Os|Joel|Amos|Mich|Hab|Zach|Malach|Matth|Marc|Luc|Joann|Act|Rom|Cor|Gal|Eph|Phil|Col|Thess|Tim|Tit|Hebr|Heb|Jac|Petr|Apoc|Mach|Lev|Zac)\b\.?")


def es_ruido(texto):
    """Solo ruido: fichas de ornato y ninguna cita ni palabra de verdad."""
    fichas = texto.split()
    if len(texto) < MIN_PARRAFO:
        return True
    if RE_CITA.search(texto):
        return False
    reales = palabras_reales(texto)
    return len(fichas) >= 5 and len(reales) <= 1


# Citas cuya primera letra el OCR leyó como «[»: «[sai XL, v. 3» es «Isai. XL, v. 3».
PRE = [(re.compile(r"\[sai\b\.?"), "Isai.")]
# Restos de ornato tras el final de la nota: «…significa convulsion. A Y».
RE_COLA = re.compile(r"(?<=[.?!])(?:\s+[^\W\d_]{1,2}[,.;:]?){1,6}\s*$")


# El número de llamada con que empieza cada nota («1 Isai. XL…»): es de la hoja,
# no de la obra, y solo confunde.
RE_LLAMADA = re.compile(r"^\s*(?:\d{1,2}|[*'’‘\"”“$e†‡~^|+])\s+(?=\S)")


def pon_griego(texto, lecturas):
    """Sustituye cada palabra que el OCR leyó mal por su lectura griega (una vez)."""
    for token, griego, *_ in lecturas:
        texto = re.sub(r"(?<!\S)" + re.escape(html.escape(token)) + r"(?!\S)", lambda m: html.escape(griego), texto, count=1)
    return texto


def limpia_parrafo_una(interior, tabla, libro=None):
    """(párrafo limpio o None, cambios)."""
    texto = RE_LLAMADA.sub("", html.unescape(interior), count=1)
    for patron, cambio in PRE:
        texto = patron.sub(cambio, texto)
    texto, n = barrido.corrige(texto, tabla)
    texto, k = citas_notas.repara(texto, libro)
    n += k
    texto, k = RE_COLA.subn("", texto)
    n += k
    texto, idx = ornatos.limpia(texto)
    n += len(idx)
    texto = re.sub(r"\s+([,.;:!?])", r"\1", texto)
    texto = re.sub(r"\s{2,}", " ", texto).strip()
    if es_ruido(texto):
        return None, n
    return html.escape(texto), n


def limpia_parrafo(interior, tabla, libro=None):
    """limpia_parrafo_una hasta que no cambie nada (una cola deja otra a la vista)."""
    total = 0
    for _ in range(5):
        nuevo, n = limpia_parrafo_una(interior, tabla, libro)
        total += n
        if nuevo is None or n == 0 or nuevo == interior:
            return nuevo, total
        interior = nuevo
    return nuevo, total


def limpia_entrada(cuerpo, tabla):
    """El cuerpo de un capítulo: párrafos <p>…</p> separados por un espacio."""
    interiores = RE_P.findall(cuerpo)
    if RE_P.sub("", cuerpo).strip():
        sys.exit("un capítulo trae algo más que párrafos <p>")
    salida, n = [], 0
    for interior in interiores:
        if interior.startswith("<i>"):          # la nota explicativa del capítulo
            salida.append(f"<p>{interior}</p>")
            continue
        nuevo, k = limpia_parrafo(interior, tabla)
        n += k
        if nuevo is not None:
            salida.append(f"<p>{nuevo}</p>")
        else:
            n += 1
    return " ".join(salida), n


RE_CLAVE = re.compile(r"(.*) (\d+):(\d+)$")
APROX = "aproximadamente"


def reparte(grupos, orden):
    """{(libro, cap): [(a, b, [indices de grupo])]}: tramos de versículos
    con el mismo conjunto de hojas. Un grupo sin rango toma el hueco entre
    el anterior y el siguiente de su libro."""
    pos = {k: i for i, k in enumerate(orden)}
    libro = lambda k: RE_CLAVE.match(k).group(1)
    rangos = []
    for g in grupos:
        rangos.append((pos[g["desde"]], pos[g["hasta"]], False) if g["desde"] else None)
    for i, r in enumerate(rangos):
        if r is not None:
            continue
        ant = next((rangos[j] for j in range(i - 1, -1, -1) if rangos[j]), None)
        sig = next((rangos[j] for j in range(i + 1, len(rangos)) if rangos[j]), None)
        if ant and sig and libro(orden[ant[1]]) == libro(orden[sig[0]]) and ant[1] + 1 <= sig[0] - 1:
            rangos[i] = (ant[1] + 1, sig[0] - 1, True)
    por_verso = collections.defaultdict(list)
    for i, r in enumerate(rangos):
        if r:
            for p in range(r[0], r[1] + 1):
                por_verso[p].append(i)
    tramos = collections.defaultdict(list)
    for p in sorted(por_verso):
        m = RE_CLAVE.match(orden[p])
        cap = (m.group(1), int(m.group(2)))
        v = int(m.group(3))
        gs = por_verso[p]
        t = tramos[cap]
        if t and t[-1][2] == gs and t[-1][1] == v - 1:
            t[-1][1] = v
        else:
            t.append([v, v, gs])
    return tramos, rangos


def cabecera(g, orden, aprox):
    d, h = RE_CLAVE.match(g["desde"] or orden[0]), RE_CLAVE.match(g["hasta"] or orden[0])
    if g["desde"] is None:
        return "Notas de una página impresa cuyo versículo exacto no se puede determinar."
    ref = lambda m: f"{int(m.group(2))}:{int(m.group(3))}"
    return f"Notas de Torres Amat de la página impresa que va de {ref(d)} a {ref(h)}."


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--paginas", default=os.path.join(DIR, "notas_paginas.json"))
    ap.add_argument("--texto", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "notas"))
    args = ap.parse_args()

    conf = open(os.path.join(DIR, "..", "..", "modulos", "mods.d", "torresamatnotas.conf"),
                encoding="utf-8").read()
    orden = [k for k, _ in lee_imp(exporta_aislado(args.texto, conf_de(args.texto)))
             if RE_CLAVE.match(k) and not k.endswith(":0")]
    grupos = json.load(open(args.paginas, encoding="utf-8"))
    tabla = une_tablas(barrido.TABLA, TABLA_NOTAS)
    griego = json.load(open(GRIEGO, encoding="utf-8")) if os.path.exists(GRIEGO) else {}

    limpios, total = [], 0
    for g in grupos:
        ps = []
        for n in g["notas"]:
            libro = RE_CLAVE.match(g["desde"]).group(1) if g["desde"] else None
            lecturas = griego.get(f"{g['tomo']}|{g['pagina']}", [])
            nuevo, k = limpia_parrafo(pon_griego(html.escape(n), lecturas), tabla, libro)
            total += k
            if nuevo is not None:
                ps.append(nuevo)
        limpios.append(ps)
    tramos, rangos = reparte(grupos, orden)

    entradas, huecos = [], 0
    for cap in sorted(tramos, key=lambda c: orden.index(f"{c[0]} {c[1]}:{tramos[c][0][0]}")):
        for a, b, gs in tramos[cap]:
            partes = []
            for i in gs:
                if not limpios[i]:
                    continue
                aprox = rangos[i][2]
                partes.append(f"<p><i>{html.escape(cabecera(grupos[i], orden, aprox))}"
                              f"{' (ubicación aproximada)' if aprox else ''}</i></p>")
                partes += [f"<p>{p}</p>" for p in limpios[i]]
            if not partes:
                huecos += 1
                continue
            entradas.append((f"{cap[0]} {cap[1]}:{a}-{b}", " ".join(partes)))
    imp = "".join(f"$$${k}\n{c}\n" for k, c in entradas)

    destino = os.path.join(args.salida, "modules", "comments", "zcom", "torresamatnotas")
    shutil.rmtree(args.salida, ignore_errors=True)
    os.makedirs(destino)
    os.makedirs(os.path.join(args.salida, "mods.d"))
    open(os.path.join(args.salida, "mods.d", "torresamatnotas.conf"), "w",
         encoding="utf-8").write(conf)
    with tempfile.TemporaryDirectory() as tmp:
        f = os.path.join(tmp, "n.imp")
        open(f, "w", encoding="utf-8").write(imp)
        subprocess.run(["imp2vs", f, "-z", "z", "-b", "3", "-v", V11N, "-o", "."],
                       cwd=destino, check=True, capture_output=True)
    with tempfile.TemporaryDirectory() as tmp:
        os.makedirs(os.path.join(tmp, "mods.d"))
        os.symlink(os.path.abspath(os.path.join(args.salida, "modules")),
                   os.path.join(tmp, "modules"))
        open(os.path.join(tmp, "mods.d", "v.conf"), "w", encoding="utf-8").write(
            conf.replace(f"[{NOMBRE}]", f"[{NOMBRE}Vuelta]", 1))
        vuelta = exporta(tmp, NOMBRE + "Vuelta")
    n_notas = sum(len(p) for p in limpios)
    print(f"{total} cambios; {n_notas} notas en {len(entradas)} tramos de versículos; "
          f"comentario en {destino}")
    if not vuelta.strip():
        sys.exit("el comentario reimportado sale vacío")


if __name__ == "__main__":
    main()
