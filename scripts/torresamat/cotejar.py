"""
Coteja capítulo a capítulo contra la Vulgata por NOMBRES PROPIOS.

La correlación de longitudes de validar.py sirve para levantar sospechas,
pero se equivoca mucho: daba por corridos Isaías 53, Hechos 1, Daniel 1 y
Malaquías 1, y los cuatro estaban bien. Un capítulo de versos parecidos en
largo casa igual de bien desplazado que en su sitio.

Los nombres propios no engañan. "Jerusalem", "Israel", "David", "Jacob",
"Nabuchodonosor" se escriben casi igual en el latín de la Vulgata y en el
castellano de Torres Amat, y están repartidos de forma desigual por los
versículos: si el versículo 7 nuestro menciona a Babylonia y el 7 latino
también, el capítulo está en su sitio; si quien la menciona es el 8, está
corrido. Es una prueba de contenido, no de forma.
"""
import re, unicodedata
from canon import POR_OSIS

def _nombres(txt):
    """Nombres propios normalizados a su raíz, para cruzar latín y español."""
    out = set()
    for pal in re.findall(r"\b[A-ZÁÉÍÓÚÑ][A-Za-zÁÉÍÓÚÜÑáéíóúüñ]{3,}", txt or ""):
        p = unicodedata.normalize("NFD", pal)
        p = "".join(c for c in p if unicodedata.category(c) != "Mn").lower()
        # las desinencias latinas cambian; la raíz aguanta
        out.add(p[:5])
    return out

# Palabras que empiezan frase y no son nombres: aparecen en todas partes y
# solo añaden ruido.
RUIDO = {"esto", "porqu", "y", "el", "la", "los", "las", "mas", "pero",
         "cuand", "todos", "todo", "señor", "senor", "domin", "dios", "deus",
         "aquel", "enton", "asi", "para", "sino", "haec", "dixit", "et"}

def puntua_desfase(texto, vul, osis, cap, n, d):
    """Cuántos nombres propios comparten nuestros versículos y los latinos."""
    aciertos = fallos = 0
    for v in range(1, n + 1):
        a = texto.get(f"{osis} {cap}:{v}")
        b = vul.get(f"{osis} {cap}:{v + d}")
        if not a or not b:
            continue
        na = _nombres(a) - RUIDO
        nb = _nombres(b) - RUIDO
        if not na or not nb:
            continue
        if na & nb:
            aciertos += 1
        else:
            fallos += 1
    return aciertos, fallos

def veredicto(texto, vul, osis, cap):
    """(mejor desfase, detalle) o (None, ...) si no hay datos para decidir."""
    n = POR_OSIS[osis]["versos"][cap - 1]
    tabla = {}
    for d in (0, 1, -1):
        tabla[d] = puntua_desfase(texto, vul, osis, cap, n, d)
    total = sum(a + f for a, f in tabla.values())
    if total < 6:
        return None, tabla
    mejor = max(tabla, key=lambda d: (tabla[d][0] - tabla[d][1], -abs(d)))
    a0, f0 = tabla[0]
    am, fm = tabla[mejor]
    # Para mover un capítulo hay que ganarle al 0 con holgura Y acertar de
    # verdad: sin un solo nombre en común, "menos fallos" no es prueba de
    # nada. Job 38 -- el discurso desde el torbellino, que casi no tiene
    # nombres propios -- daba (0 aciertos, 13 fallos) en el desfase -1 y
    # salía marcado como corrido sin serlo.
    if mejor != 0 and (am < 2 or (am - fm) - (a0 - f0) < 3):
        mejor = 0
    return mejor, tabla
