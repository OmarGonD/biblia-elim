"""Convierte las líneas de cuerpo en una corriente de versículos."""
import re
from segment import texto
from capitulos import encabezado_capitulo
from marcas import lee_numero, quita_basura, parte_internos

# El OCR de esta edición confunde de forma sistemática:
#   ó -> 6 / d / 0      á -> 4      ll -> Nl / ll      v -> y
# Solo se corrige lo que es inequívoco por el contexto.
SUSTITUCIONES = [
    (r"(?<=\s)4(?=\s+[a-záéíóúñ])", "á"),      # "4 Dios"  -> "á Dios"
    (r"(?<=\s)6(?=\s)", "ó"),                    # " 6 "     -> " ó "
    (r"(?<=\s)d(?=\s+(cuerpos|las|los|una|un|el|la)\b)", "ó"),
    (r"\bNl(?=[aeiouáéíóú])", "ll"),             # "Nlamó"   -> "llamó"
    (r"\bE\s?hizo\b", "E hizo"),
    (r"€", "e"),
    (r"\s+([,.;:!?])", r"\1"),
    (r"\s{2,}", " "),
]

# El número de versículo abre el renglón. En Lamentaciones, y solo ahí, va
# detrás del nombre de la letra hebrea del acróstico -- "ALEPH. 1.",
# "GHIMEL. 3." --, que el OCR además destroza ("BrrmH.", "DaterH,"), así que
# el prefijo se admite como una palabra suelta cualquiera y no por lista.
RE_VERSO = re.compile(r"^\s*[-–—.]?\s*(\d{1,3})\s*[.,:;]\s*(.*)$")
# Con acróstico el punto tras el número se pierde a menudo, así que ahí -- y
# solo ahí, detrás de la letra hebrea -- el separador es opcional.
RE_VERSO_ACROSTICO = re.compile(
    r"^\s*[A-Za-zÀ-ÿ]{2,9}\s*[.,]\s*(\d{1,3})\s*[.,:;]?\s+(.*)$")

def casa_verso(t):
    return RE_VERSO.match(t) or RE_VERSO_ACROSTICO.match(t)

def limpia(s):
    for pat, rep in SUSTITUCIONES:
        s = re.sub(pat, rep, s)
    return s.strip()

def es_ruido(t):
    """Líneas que son basura de la lámina vecina o del borde del escaneo."""
    t = t.strip()
    if not t:
        return True
    letras = sum(c.isalpha() for c in t)
    if letras < 3:
        return True
    if letras / max(1, len(t)) < 0.45:
        return True
    # una tirada de mayúsculas sueltas sin vocales de palabra real
    if len(t) <= 6 and t.upper() == t and not re.search(r"[AEIOU][a-z]", t):
        return True
    return False

RE_ABRE = re.compile(r"^\s*[-–—.]?\s*([\dTIl|iSBGZb]{1,3})\s*[.,:;]\s*(.*)$")
# Probado y descartado: aceptar el número sin punto a principio de renglón
# ("3 Mas entró despues…"). Recupera versículos pero mete más falsos
# positivos de los que arregla -- las listas y genealogías empiezan renglón
# con número y mayúscula --, y la alineación bajaba del 89,7 % al 88,1 %.

def abre_versiculo(t):
    """
    (numero, resto) si el renglón abre versículo, si no None.
    Se prueba primero tal cual y luego quitando la basura de delante:
    el número suele venir precedido del ruido del canto -- "JJ 6." --,
    y sin quitarlo el versículo se pegaba al anterior.
    """
    for cand in (t, quita_basura(t)):
        m = RE_VERSO.match(cand)
        if m:
            return int(m.group(1)), m.group(2).strip()
        m = RE_ABRE.match(cand)
        if m:
            n = lee_numero(m.group(1))
            if n:
                return n, m.group(2).strip()
        m = RE_VERSO_ACROSTICO.match(cand)
        if m:
            return int(m.group(1)), m.group(2).strip()
    return None

def corriente(lineas_cuerpo, cands=None):
    """
    Devuelve una lista de sucesos:
      ("cap", n)   encabezado de capítulo
      ("vers", n, texto_inicial)
      ("sigue", texto)
    """
    out = []
    for l in lineas_cuerpo:
        t = texto(l).strip()
        n = encabezado_capitulo(t)
        if n:
            out.append(("cap", n, cands))
            continue
        if es_ruido(t):
            continue
        # Un renglón puede abrir versículo y además contener el arranque
        # del siguiente: en esta edición el versículo no siempre empieza
        # renglón, y "…he comido, 14 Dijo entonces…" son dos.
        ini = abre_versiculo(t)
        resto = ini[1] if ini else t
        trozos = parte_internos(resto)
        primero, cola = trozos[0], trozos[1:]
        if ini:
            out.append(("vers", ini[0], primero[1], cands, True))
        elif primero[1]:
            out.append(("sigue", primero[1], cands))
        for num, txt in cola:
            out.append(("vers", num, txt, cands, False))
    return out

def une(trozos):
    """Une líneas respetando la partición de palabra a final de renglón."""
    buf = ""
    for t in trozos:
        t = t.strip()
        if not t:
            continue
        if buf.endswith("-"):
            buf = buf[:-1] + t
        elif buf:
            buf += " " + t
        else:
            buf = t
    return limpia(buf)
