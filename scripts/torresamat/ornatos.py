"""
Quita del módulo TorresAmat los ornatos de lámina que el OCR leyó como
letras («Il A Ú MN UU 1 ll AN…») dentro de un verso.

Solo cae un tramo denso en fichas que no son palabra (de 1 a 3 caracteres,
que no sean una palabra corta del español), con al menos MIN_ORNATOS de
ellas; las palabras cortas que quedan dentro del tramo caen con él. Un tramo
que lleve algo de cita al margen («v. 41», «XIV», cifras 2-9) no se toca, ni
el que sería el verso entero. El resto del verso queda byte a byte igual.

Uso:

    python3 ornatos.py [--origen RAIZ] [--salida DIR]
"""
import argparse
import os
import re

from barrido import DIR, INSTALADO, reescribe

MIN_RACHA = 4
MIN_RACHA_SUELTA = 3

PALABRAS_CORTAS = set(
    "a e o u y á é ó ú de en la el lo al se no ni si me mi te tu su un es "
    "ya yo ha he le ve va os fe ay oh ah id ir da di do dé sé vé ten ver "
    "ser por con sin los las les mis tus sus una uno del que mas más pues "
    "como muy son fué fue hé tal dos tres seis diez cien mil".split())

LETRAS = "A-Za-zÁÉÍÓÚÜÑáéíóúüñ\u0370-\u03ff\u1f00-\u1fff"   # y el griego de las notas
RE_FICHA = re.compile(r"\S+")
RE_CITA = re.compile(r"v\.|Cap\.|Véase|^[IVXLC]{2,}[,.]$|[2-9][.,]")


def es_ornato(ficha):
    palabra = re.sub(r"[^%s0-9]" % LETRAS, "", ficha)
    if "<" in ficha or ">" in ficha:
        return False
    if not palabra:
        return True
    if palabra.lower() in PALABRAS_CORTAS:
        return False
    if len(palabra) <= 2:
        return True
    return len(palabra) == 3 and (
        palabra.isupper()
        or not (palabra.islower()
                or palabra[0].isupper() and palabra[1:].islower()))


MIN_ORNATOS = 6
RE_ROMANA = re.compile(r"^[IVXLC]{2,}[,.]?$")


def es_debil(ficha):
    """Ficha que puede formar parte de un tramo de ornato: de hasta tres
    letras, o toda en mayúsculas hasta seis, o sin letras."""
    letras = re.sub(r"[^%s]" % LETRAS, "", ficha)
    if "<" in ficha or ">" in ficha:
        return False
    return len(letras) <= 3 or (letras.isupper() and len(letras) <= 6)


def es_ornato_fuerte(ficha):
    letras = re.sub(r"[^%s]" % LETRAS, "", ficha)
    if es_ornato(ficha):
        return True
    return (len(letras) >= 3 and letras.isupper() and len(letras) <= 6
            and not RE_ROMANA.match(ficha))


def rachas(fichas, minimo=MIN_ORNATOS):
    """[(i, j)] de tramos de ornato: fichas débiles seguidas, con al menos
    `minimo` fuertes y la mitad o más de fuertes; el tramo se recorta hasta
    la primera y la última fuerte, de modo que las palabras cortas (a, y,
    de…) solo caen si están entre ornato."""
    fuerte = [es_ornato_fuerte(f[2]) for f in fichas]
    debil = [es_debil(f[2]) for f in fichas]
    salida, i = [], 0
    while i < len(fichas):
        if not debil[i]:
            i += 1
            continue
        j = i
        while j < len(fichas) and debil[j]:
            j += 1
        idx = [k for k in range(i, j) if fuerte[k]]
        if len(idx) >= minimo and 2 * len(idx) >= j - i:
            salida.append((idx[0], idx[-1] + 1))
        i = j
    return salida


def limpia(texto):
    """(texto sin los ornatos, índices de las fichas quitadas)."""
    fichas = [(m.start(), m.end(), m.group()) for m in RE_FICHA.finditer(texto)]
    quitar = []
    for i, j in rachas(fichas):
        if j - i == len(fichas):
            continue                    # sería el verso entero
        if any(RE_CITA.search(f[2]) for f in fichas[i:j]):
            continue                    # cita al margen, no ornato
        quitar.append((i, j))
    if not quitar:
        return texto, []
    trozos, ultimo, idx = [], 0, []
    for i, j in quitar:
        trozos.append(texto[ultimo:fichas[i][0]])
        ultimo = fichas[j - 1][1]
        idx.extend(range(i, j))
    trozos.append(texto[ultimo:])
    nuevo = re.sub(r"[ \t]{2,}", " ", "".join(trozos)).strip()
    return nuevo, idx


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "ornatos"))
    args = ap.parse_args()

    def transforma(clave, cuerpo):
        nuevo, idx = limpia(cuerpo)
        if idx:
            quitadas = set(idx)
            quedan = [f for k, f in enumerate(cuerpo.split())
                      if k not in quitadas]
            if nuevo.split() != quedan:
                raise SystemExit(f"{clave}: la limpieza cambió otra cosa")
        return nuevo, len(idx)

    total, versos, destino = reescribe(args.origen, args.salida, transforma)
    print(f"{total} fichas de ornato quitadas en {versos} versos; "
          f"módulo en {destino}")


if __name__ == "__main__":
    main()
