"""Convierte las líneas de cuerpo en una corriente de versículos."""
import re

from segment import texto

LETRA_DIGITO = {"T": "7", "I": "1", "l": "1", "|": "1", "i": "1",
                "S": "5", "B": "8", "G": "6", "Z": "2"}


def lee_numero(tok):
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


RE_BASURA = re.compile(
    r"^(?:[^\wáéíóúñÁÉÍÓÚÑ]+|[A-Za-z|=*]{1,3}(?![a-záéíóúñá]))\s*")


def quita_basura(t, veces=3):
    for _ in range(veces):
        nuevo = RE_BASURA.sub("", t, count=1)
        if nuevo == t:
            break
        t = nuevo
    return t.strip()


# Número a principio de renglón. En esta edición el versículo 1 de un
# capítulo nuevo se imprime "3 1 Pero la serpiente" (capítulo + verso).
RE_VERSO = re.compile(r"^\s*[-–—.]?\s*(\d{1,3})\s*[.,:;]?\s+(.*)$")
RE_CAP_VER = re.compile(
    r"^\s*(\d{1,3})\s+1\s+(?=[A-ZÁÉÍÓÚÑÜ¿¡«\"“])(.*)$")

# Número a mitad de renglón, detrás de cierre. En el Sermón de la
# Montaña el verso siguiente sigue la frase y va en minúscula:
# "trono de Dios, 35 ni por la tierra…". Exigir mayúscula dejaba
# esos versos pegados al anterior.
#
# No vale partir ante cualquier cifra. Esta edición escribe las
# cantidades en letra («veinte años»), pero el OCR cuela números de
# página («6 ya que polvo») y basura («arrojas 1 me»). Tratarlos
# todos como verso corría 1 Macabeos del 76 % al 67 %.
RE_INTERNO = re.compile(
    r"(?<=[.,;:!?»)])\s+(?:[A-Za-z0-9|=*]{1,2}\s+)?(\d{1,3})\s*[.,]?\s+"
    r"(?=[A-Za-záéíóúñüÁÉÍÓÚÑÜ¿¡«\"“])")

# Sin puntuación delante, solo si sigue mayúscula: "8 Tarde y mañana".
# "vivió 930 años" no dispara (minúscula y además > 176).
RE_INTERNO_LAXO = re.compile(
    r"(?<=\s)(\d{1,3})\s+(?=[A-ZÁÉÍÓÚÑÜ¿¡«\"“])")


def parte_internos(t):
    partes = RE_INTERNO.split(t)
    if len(partes) == 1:
        partes = RE_INTERNO_LAXO.split(t)
    salida = [(None, partes[0].strip())]
    for k in range(1, len(partes), 2):
        n = int(partes[k])
        if 1 <= n <= 176:
            salida.append((n, partes[k + 1].strip()))
        else:
            salida[-1] = (salida[-1][0],
                          (salida[-1][1] + " " + partes[k] + " " +
                           partes[k + 1]).strip())
    return salida


def abre_versiculo(t):
    """(numero, resto) si el renglón abre versículo, si no None."""
    for cand in (t, quita_basura(t)):
        m = RE_CAP_VER.match(cand)
        if m:
            cap = int(m.group(1))
            if 1 <= cap <= 150:
                return ("capver", cap, m.group(2).strip())
        m = RE_VERSO.match(cand)
        if m:
            n = int(m.group(1))
            resto = m.group(2).strip()
            if 1 <= n <= 176:
                return ("vers", n, resto)
    return None


def es_ruido(t):
    t = t.strip()
    if not t:
        return True
    letras = sum(c.isalpha() for c in t)
    if letras < 3:
        return True
    if letras / max(1, len(t)) < 0.40:
        return True
    return False


def es_titulo(t):
    """Epígrafes de sección: 'La creación del universo', 'El diluvio'.

    No puede tragarse la continuación de un verso: «dicho a los
    antiguos: No perjurarás,» y «antes cumplirás… jura-» no llevan
    cifra y no acaban en punto, y con la regla vieja Mateo 5:33
    quedaba en «fué mentos».
    """
    t = t.strip()
    if not t:
        return False
    if re.search(r"\d", t):
        return False
    if t[0].islower() or t.endswith(("-", "¬", "‐", ",", ";", ":")):
        return False
    if len(t) > 70 or len(t) < 8:
        return False
    if t.endswith((".", "»", "!", "?")):
        return False
    return True


def _origen(linea, base):
    """Ubicación verificable de una lectura en el facsímil."""
    if not base:
        return None
    out = dict(base)
    out.update({
        "x1": min(w[0] for w in linea), "x2": max(w[1] for w in linea),
        "top": min(w[2] for w in linea), "bot": max(w[3] for w in linea),
        "confianza": round(sum(w[4] for w in linea) / len(linea), 1),
    })
    return out


def corriente(lineas, cands=None, origen=None):
    """
    Sucesos:
      ("cap", n, cands)
      ("vers", n, texto, cands, True/False)
      ("sigue", texto, cands)
    """
    out = []
    for l in lineas:
        t = texto(l).strip()
        if es_ruido(t) or es_titulo(t):
            continue
        ini = abre_versiculo(t)
        if ini and ini[0] == "capver":
            out.append(("cap", ini[1], cands))
            resto = ini[2]
            trozos = parte_internos(resto)
            out.append(("vers", 1, trozos[0][1], cands, True, _origen(l, origen)))
            for num, txt in trozos[1:]:
                out.append(("vers", num, txt, cands, False, _origen(l, origen)))
            continue
        resto = ini[2] if ini else t
        trozos = parte_internos(resto)
        primero, cola = trozos[0], trozos[1:]
        if ini:
            out.append(("vers", ini[1], primero[1], cands, True, _origen(l, origen)))
        elif primero[1]:
            out.append(("sigue", primero[1], cands))
        for num, txt in cola:
            out.append(("vers", num, txt, cands, False, _origen(l, origen)))
    return out


def une(trozos):
    buf = ""
    for t in trozos:
        t = t.strip()
        if not t:
            continue
        t = t.replace("|", " ")
        if buf.endswith(("¬", "-", "‐")):
            buf = buf[:-1] + t.lstrip()
        elif buf:
            buf += " " + t
        else:
            buf = t
    buf = re.sub(r"\s+([,.;:!?])", r"\1", buf)
    buf = re.sub(r"\s{2,}", " ", buf)
    return buf.strip()
