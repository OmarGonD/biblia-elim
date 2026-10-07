"""
Barrido ortográfico del módulo TorresAmat: sustituye, palabra entera, las
formas que no son palabra (errores del OCR: «Td» por «Id», «dle» por «de»,
«eracia» por «gracia», «Dayid» por «David»…) por la forma correcta.

Las sustituciones salen de barrido_palabras.tsv (error<TAB>corrección), que
crece por fases. Solo se toca texto, nunca el marcado <…>; el resto del
módulo no cambia. Parte, como parche_facsimil.py, de un árbol SWORD, lo
exporta con mod2imp, reescribe y reimporta con imp2vs, y comprueba que la
ida y vuelta exporta exactamente lo reescrito y que solo difieren versos con
palabras de la tabla.

Uso:

    python3 barrido.py [--origen RAIZ] [--salida DIR] [--tabla TSV]

RAIZ (por defecto ~/.sword) solo se lee. DIR (por defecto salida/barrido)
queda con un árbol SWORD completo; copiarlo a modulos/ y a ~/.sword es un
paso aparte. Aplicado otra vez sobre DIR no cambia nada.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

from parche_facsimil import (INSTALADO, MODULO, V11N, conf_de, escribe_imp,
                             exporta_aislado, lee_imp)

DIR = os.path.dirname(os.path.abspath(__file__))
TABLA = os.path.join(DIR, "barrido_palabras.tsv")

LETRA = "A-Za-zÁÉÍÓÚÜÑáéíóúüñ"
# Una palabra puede llevar pegada una cifra de llamada o de lámina («4ngel»,
# «5eñor»): la tabla la ve entera y, si no la trae, corrige solo las letras.
RE_TROZO = re.compile(r"(<[^>]*>)|([0-9]{1,2}[%s]+|[%s]+)" % (LETRA, LETRA))


def lee_tabla(ruta):
    """(exactas, sin_caja): la clave toda en minúscula vale en cualquier caja."""
    exactas, sin_caja = {}, {}
    with open(ruta, encoding="utf-8") as f:
        for n, linea in enumerate(f, 1):
            linea = linea.rstrip("\n")
            if not linea or linea.startswith("#"):
                continue
            partes = linea.split("\t")
            if len(partes) != 2 or not all(partes):
                raise ValueError(f"{ruta}:{n}: se esperaba error<TAB>corrección")
            malo, bueno = partes
            destino = sin_caja if malo == malo.lower() else exactas
            if malo in destino and destino[malo] != bueno:
                raise ValueError(f"{ruta}:{n}: «{malo}» repetida con otra corrección")
            if malo == bueno:
                raise ValueError(f"{ruta}:{n}: «{malo}» no cambia nada")
            destino[malo] = bueno
    # Una corrección puede ser, a su vez, una forma de la tabla («sleryo» ->
    # «sieryo» -> «siervo»): se sigue hasta el final para que una pasada baste.
    for destino in (exactas, sin_caja):
        for malo, bueno in destino.items():
            for _ in range(5):
                sig = exactas.get(bueno) or sin_caja.get(bueno.lower())
                if sig is None or sig == bueno:
                    break
                bueno = sig
            else:
                raise ValueError(f"{ruta}: «{malo}» no termina en una forma válida")
            destino[malo] = bueno
    return exactas, sin_caja


def con_caja(modelo, palabra):
    if len(modelo) > 1 and modelo.isupper():
        return palabra.upper()
    if modelo[0].isupper():
        return palabra[0].upper() + palabra[1:]
    return palabra


# El OCR leyó la «ó» disyuntiva como cero: «los canales 0 bebederos». Un
# cero suelto entre dos palabras no puede ser otra cosa en un texto sin cifras.
RE_CERO = re.compile(r"(?<=[a-záéíóúñ,;:])( )0(?= [a-záéíóúñ])")
# Punto que el OCR coló tras un artículo o una preposición, ante nombre propio:
# «de la. Palestina», «al. Señor». («el.» se deja: puede ser «él.»)
RE_PUNTO = re.compile(r"\b(la|los|las|de|del|al|á|en|un|una|con|por|para|su|sus)\.(?= [A-ZÁÉÍÓÚ])")
# Palabra gramatical repetida por el salto de renglón: «á á dioses», «y y».
RE_REPETIDA = re.compile(r"\b(y|á|e|de|del|en|el|la|los|las|un|una|con|por|para|al|su|sus|mi|tu)([ \t]+)\1\b", re.I)
REGLAS = [(RE_CERO, r"\1ó"), (RE_PUNTO, r"\1"), (RE_REPETIDA, r"\1")]
# barrido_notas.tsv (fugas_notas.py) va antes: las frases se anclan al texto ya sin ellas.
FRASES_NOTAS = os.path.join(DIR, "barrido_notas.tsv")
FRASES = os.path.join(DIR, "barrido_frases.tsv")


def aplica_reglas(texto):
    """(texto, cambios): las reglas hasta que no cambien nada («e e e»)."""
    total = 0
    while True:
        antes = total
        for regla, reemplazo in REGLAS:
            texto, k = regla.subn(reemplazo, texto)
            total += k
        if total == antes:
            return texto, total




def lee_frases(rutas):
    """{referencia: [(viejo, nuevo)]} de los ficheros de frases, en orden."""
    frases = {}
    for ruta in ([rutas] if isinstance(rutas, str) else rutas):
        if not os.path.exists(ruta):
            continue
        with open(ruta, encoding="utf-8") as f:
            for n, linea in enumerate(f, 1):
                linea = linea.rstrip("\n")
                if not linea or linea.startswith("#"):
                    continue
                partes = linea.split("\t")
                if len(partes) != 3 or not partes[0] or not partes[1] \
                        or partes[1] == partes[2]:
                    raise ValueError(f"{ruta}:{n}: se esperaba ref<TAB>viejo<TAB>nuevo")
                frases.setdefault(partes[0], []).append((partes[1], partes[2]))
    return frases


CORTES = os.path.join(DIR, "barrido_cortes.tsv")


def lee_cortes(ruta):
    """{referencia: [(inicio, fin)]}: quitar desde `inicio` hasta `fin`."""
    cortes = {}
    if not os.path.exists(ruta):
        return cortes
    with open(ruta, encoding="utf-8") as f:
        for n, linea in enumerate(f, 1):
            linea = linea.rstrip("\n")
            if not linea or linea.startswith("#"):
                continue
            partes = linea.split("\t")
            if len(partes) != 3 or not all(partes):
                raise ValueError(f"{ruta}:{n}: se esperaba ref<TAB>inicio<TAB>fin")
            cortes.setdefault(partes[0], []).append((partes[1], partes[2]))
    return cortes


def aplica_cortes(clave, texto, cortes):
    """Quita cada tramo desde la primera aparición de `inicio` hasta la
    primera de `fin` que le sigue (ambos incluidos). `inicio` ha de aparecer
    una sola vez; si ya no aparece, el corte está hecho."""
    n = 0
    for inicio, fin in cortes.get(clave, ()):
        a = texto.count(inicio)
        if a == 0:
            continue
        if a > 1:
            sys.exit(f"{clave}: «{inicio}» aparece {a} veces")
        i = texto.index(inicio)
        j = texto.find(fin, i + len(inicio))
        if j < 0:
            j = texto.find(fin, i)
            if j < 0:
                sys.exit(f"{clave}: no hay «{fin}» después de «{inicio}»")
        texto = re.sub(r"[ \t]{2,}", " ", texto[:i] + texto[j + len(fin):]).strip()
        n += 1
    return texto, n


def aplica_frases(clave, texto, frases):
    """Sustituye cada frase anclada al versículo: exactamente una vez, o ya
    hecha. Cualquier otra cosa detiene el barrido."""
    n = 0
    for viejo, nuevo in frases.get(clave, ()):
        if texto.count(viejo) == 1:
            texto = re.sub(r"[ \t]{2,}", " ", texto.replace(viejo, nuevo)).strip()
            n += 1
        elif texto.count(viejo) > 1 or nuevo not in texto:
            sys.exit(f"{clave}: «{viejo}» no está una sola vez en el verso")
    return texto, n


def corrige(texto, tabla):
    """El texto con las palabras de la tabla sustituidas, y cuántas."""
    exactas, sin_caja = tabla
    n = 0

    def busca(pal):
        if pal in exactas:
            return exactas[pal]
        if pal.lower() in sin_caja:
            return con_caja(pal, sin_caja[pal.lower()])
        return None

    def cambia(m):
        nonlocal n
        if m.group(1):
            return m.group(1)
        pal = m.group(2)
        nueva = busca(pal)
        cifras = ""
        if nueva is None and pal[0].isdigit():
            cifras, pal = re.match(r"[0-9]+", pal).group(), pal.lstrip("0123456789")
            nueva = busca(pal)
        if nueva is None:
            return cifras + pal
        n += 1
        return cifras + nueva

    texto, k = aplica_reglas(RE_TROZO.sub(cambia, texto))
    return texto, n + k


def solo_palabras(viejo, nuevo):
    """Las dos versiones solo difieren en palabras (mismo marcado y blancos)."""
    def forma(t):
        t = aplica_reglas(t)[0]
        t = RE_TROZO.sub(lambda m: m.group(1) or "\0", t)
        return re.sub(r"\0( ?\0)*", "\0", t)   # «queá» -> «que á»
    return forma(viejo) == forma(nuevo)


def reescribe(origen, salida, transforma):
    """Exporta `origen`, aplica transforma(clave, cuerpo) -> (cuerpo, n) a
    cada entrada, reimporta en `salida` y comprueba la ida y vuelta.

    Devuelve (cambios, versos, destino). transforma debe detener el proceso
    (SystemExit) si un cambio se sale de lo que se propone.
    """
    if os.path.abspath(salida) == os.path.abspath(origen):
        sys.exit("--salida no puede ser el --origen")
    conf = conf_de(origen)
    original = exporta_aislado(origen, conf)
    entradas = lee_imp(original)
    if escribe_imp(entradas) != original:
        sys.exit("el export no se puede reescribir sin pérdida; no se toca")

    nuevas, total, versos = [], 0, 0
    for clave, cuerpo in entradas:
        nuevo, n = transforma(clave, cuerpo)
        total += n
        versos += bool(n)
        nuevas.append((clave, nuevo))
    reescrito = escribe_imp(nuevas)

    destino = os.path.join(salida, "modules", "texts", "ztext", "torresamat")
    shutil.rmtree(salida, ignore_errors=True)
    os.makedirs(destino)
    os.makedirs(os.path.join(salida, "mods.d"))
    with open(os.path.join(salida, "mods.d", "torresamat.conf"), "w",
              encoding="utf-8") as f:
        f.write(conf)
    with tempfile.TemporaryDirectory() as tmp:
        imp = os.path.join(tmp, "torresamat.imp")
        with open(imp, "w", encoding="utf-8") as f:
            f.write(reescrito)
        subprocess.run(["imp2vs", imp, "-z", "z", "-v", V11N, "-o", "."],
                       cwd=destino, check=True, capture_output=True)

    if exporta_aislado(salida, conf) != reescrito:
        sys.exit("la ida y vuelta del módulo corregido no coincide")
    return total, versos, destino


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--origen", default=INSTALADO)
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "barrido"))
    ap.add_argument("--tabla", default=TABLA)
    args = ap.parse_args()
    tabla = lee_tabla(args.tabla)
    notas = lee_frases(FRASES_NOTAS)
    frases = lee_frases(FRASES)
    cortes = lee_cortes(CORTES)

    def transforma(clave, cuerpo):
        nuevo, n = corrige(cuerpo, tabla)
        if n and not solo_palabras(cuerpo, nuevo):
            sys.exit(f"{clave}: el barrido cambió algo más que palabras")
        # Primero las notas de fugas_notas.py, luego los cortes a mano, y al
        # final las frases, ancladas al texto ya sin ellos.
        nuevo, a = aplica_frases(clave, nuevo, notas)
        nuevo, c = aplica_cortes(clave, nuevo, cortes)
        nuevo, k = aplica_frases(clave, nuevo, frases)
        nuevo, r = aplica_reglas(nuevo)       # lo que los cortes dejan junto
        return nuevo, n + a + c + k + r

    total, versos, destino = reescribe(args.origen, args.salida, transforma)
    print(f"{total} palabras corregidas en {versos} versos; "
          f"módulo en {destino}")


if __name__ == "__main__":
    main()
