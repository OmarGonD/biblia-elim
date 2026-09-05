"""
Reconocimiento de la marca de versículo.

Los versículos que "faltaban" casi nunca faltaban: estaban pegados al
anterior porque no se reconoció su número. Tres causas, vistas en el
texto:

  Gen 2:6   "...cultivase: JJ 6. Salia empero..."   basura delante
  Gen 4:7   "...tu rostro? T. ¿No es cierto..."     el 7 leído como T
  Gen 3:14  "...he comido, 14 Dijo entonces..."     número en mitad del renglón
"""
import re

# El OCR de esta edición confunde estas letras con dígitos. Se excluyen
# A, O, Y y E: son palabras del castellano y darían falsos positivos en
# cuanto una frase acabe en una de ellas.
LETRA_DIGITO = {"T": "7", "I": "1", "l": "1", "|": "1", "i": "1",
                "S": "5", "B": "8", "G": "6", "Z": "2", "b": "6"}

def lee_numero(tok):
    """Número de versículo tolerando las confusiones del OCR, o None."""
    t = tok.strip(" .,:;)(")
    if not t or len(t) > 3:
        return None
    if t.isdigit():
        n = int(t)
        return n if 1 <= n <= 176 else None
    c = "".join(LETRA_DIGITO.get(ch, ch) for ch in t)
    if c.isdigit():
        n = int(c)
        return n if 1 <= n <= 176 else None
    return None

# Basura del canto o de la lámina vecina delante del número: trozos cortos
# sin vocal minúscula, que no son palabra de ninguna lengua.
RE_BASURA = re.compile(r"^(?:[^\wáéíóúñÁÉÍÓÚÑ]+|[A-Z|=]{1,3}(?![a-záéíóúñ]))\s*")

def quita_basura(t, veces=3):
    for _ in range(veces):
        nuevo = RE_BASURA.sub("", t, count=1)
        if nuevo == t:
            break
        t = nuevo
    return t.strip()

# Número en mitad del renglón: detrás de puntuación de cierre y delante de
# mayúscula o de signo de apertura. Exigir las dos cosas es lo que impide
# que "y vivió 930 años" se tome por el versículo 930.
CORTE_INTERNO = True

# El punto tras el número se pierde a menudo ("…mostrara. 3 Mas entró
# despues…") y delante suele colarse basura del canto ("…forjó. 0 19. ¡Ay
# de aquel…"), así que ambos son opcionales. Lo que no se afloja es el
# contexto: puntuación de cierre delante y mayúscula o signo de apertura
# detrás. Eso es lo que impide que "y vivió 930 años" se tome por marca.
RE_INTERNO = re.compile(
    r"(?<=[.,;:!?])\s+(?:[A-Za-z0-9|=]{1,2}\s+)?(\d{1,3})\s*[.,]?\s+"
    r"(?=[A-ZÁÉÍÓÚÑ¿¡])")

def parte_internos(t):
    """
    Parte un renglón donde empieza otro versículo.
    Devuelve [(numero_o_None, texto)]: el primer trozo sigue al versículo
    en curso, los demás abren uno nuevo.
    """
    if not CORTE_INTERNO:
        return [(None, t)]
    partes = RE_INTERNO.split(t)          # [texto, num, texto, num, texto...]
    salida = [(None, partes[0].strip())]
    for k in range(1, len(partes), 2):
        salida.append((int(partes[k]), partes[k + 1].strip()))
    return salida
