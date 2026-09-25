"""Restos de «'», «<» y «>» del OCR, escapados dos veces (TORRES-ENTITY-101).

El impreso de 1882 no usa apóstrofo ni corchetes angulares en el texto
bíblico. En el djvu.xml del Archive esos caracteres son motas del papel
(cajas de 2×2 a 12×12 px) o trozos de una letra mal leída: «testig'os»,
«lare'o», «c'eneral». No hay nada impreso que conservar.

Llegaron al módulo estropeados por dos pasos:

- osis.py escapaba con html.escape(t), que convierte «'» en «&#x27;»;
  osis2mod (SWORD 1.9.0) no entiende referencias numéricas y las deja como
  «&amp;##x27;». Por eso osis.py escapa ahora con quote=False y quita el
  ruido antes (ruido_en_crudo).
- el módulo del commit 9c036c87 se regeneró con el texto escapado otra vez:
  «&amp;##x27;» pasó a «&amp;amp;##x27;», «&gt;» a «&amp;gt;» y «&lt;» a
  «&amp;lt;». Al final del versículo limpia_final se comió el «;».

Quitar el resto deja una decisión: si iba entre dos palabras
(«celebrado'en», «No'trabarás», un solo token en el djvu.xml), va un
espacio; si iba dentro de una palabra mal leída («testig'os»), se une. Lo
decide el propio texto del módulo: las dos mitades son palabras del módulo
y la unión es menos frecuente que cualquiera de ellas. Las palabras que
tocan un resto no cuentan en ese léxico, porque son las que se juzgan.
"""
import collections
import re

# Formas del resto en el módulo (texto ya escapado). Dos veces la entidad
# quedó partida entre dos versículos: «&amp;amp» al final de uno (Sal
# 106:42, Ez 33:11) y «##x27;» al principio del siguiente.
RESTO = re.compile(r"&amp;(?:amp;##x27;?|gt;?|lt;?|amp(?=$|[\s<]))"
                   r"|^##x27;?")
# Los mismos caracteres en el texto sin escapar (osis.py).
CRUDO = re.compile(r"['<>]")

# Palabras de una letra que el impreso sí trae.
UNA_LETRA = {"y", "á", "é", "ó"}
PALABRA = re.compile(r"\w+")
PUNTUACION = ",.;:!?)"
# Frecuencia a partir de la cual una palabra es corriente en el módulo.
FRECUENTE = 100


def lexico(textos, marca=RESTO):
    """Frecuencia de cada palabra, sin las que tocan un resto."""
    frec = collections.Counter()
    for t in textos:
        for trozo in re.split(r"\s+", marca.sub("\x00", t)):
            if "\x00" in trozo:
                continue
            frec.update(p.lower() for p in PALABRA.findall(trozo))
    return frec


def _es_palabra(p, frec):
    p = p.lower()
    if len(p) == 1:
        return p in UNA_LETRA
    return frec[p] > 0


def separa(izq, der, frec):
    """True si el resto iba entre dos palabras, False si dentro de una."""
    if not _es_palabra(izq, frec):
        return False
    if not _es_palabra(der, frec):
        # La palabra de la derecha puede no salir en ningún otro sitio
        # («No'prostituyas», «y'añadian»): basta con que la izquierda sea
        # una palabra corriente y la derecha empiece palabra en minúscula.
        corriente = len(izq) == 1 or frec[izq.lower()] >= FRECUENTE
        return corriente and len(der) >= 3 and der[0].islower()
    union = frec[(izq + der).lower()]
    return union < min(frec[izq.lower()] or 1, frec[der.lower()] or 1) \
        or union == 0


def quita(texto, frec, marca=RESTO):
    """Texto sin los restos, con el espacio que corresponda."""
    s = marca.sub("\x00", texto)
    if "\x00" not in s:
        return texto
    s = re.sub(r"\x00+", "\x00", s)

    def entre_letras(m):
        izq, der = m.group(1), m.group(2)
        return f"{izq} {der}" if separa(izq, der, frec) else izq + der
    s = re.sub(r"(\w+)\x00(\w+)", entre_letras, s)
    # Delante de puntuación o al final: sin espacio.
    s = re.sub(r"[ ]*\x00[ ]*(?=[" + re.escape(PUNTUACION) + r"]|$)", "", s)
    # Delante del marcado de cierre: queda el espacio que ya había.
    s = re.sub(r"\x00(?=[ ]*<)", "", s)
    # Al principio o pegado a un signo de apertura: sin espacio detrás.
    s = re.sub(r"(^|(?<=[(¿¡]))[ ]*\x00[ ]*", "", s)
    # Entre espacios o pegado por un lado: un espacio si lo había.
    s = re.sub(r"[ ]+\x00[ ]*|[ ]*\x00[ ]+", " ", s)
    return s.replace("\x00", "")


def ruido_en_crudo(texto, frec):
    """Lo mismo sobre el texto sin escapar, antes de html.escape."""
    return quita(texto, frec, CRUDO)
