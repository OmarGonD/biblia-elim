"""Convierte las líneas de cuerpo en una corriente de versículos."""
import re

from cabeceras import es_running_header
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
    return _abre(t)[0]


def _abre(t):
    """(abre_versiculo, lo que quita_basura tuvo que quitar para verlo)."""
    for cand in (t, quita_basura(t)):
        ini = _abre_tal_cual(cand)
        if ini:
            return ini, ("" if cand is t else t[:len(t) - len(cand)].strip())
    return None, ""


def _abre_tal_cual(cand):
    if cand:
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


# Cabecera impresa de cada salmo: «14 (Vulg. 13.)». Es la única marca de
# capítulo del Salterio: los salmos no llevan el «3 1» de los demás libros.
# Con pocas letras y muchas cifras, es_ruido la descartaba, y un salmo
# nuevo solo se notaba cuando la numeración volvía a 1.
#
# El OCR cuela puntuación en la cabecera: «118. (Vulg. 117.)»,
# «121: (Vulg. 120.)», «44 (Vulg: 43.)», «119. (Vulg. 118:)». Sin la del
# 118 el Sal 117, de dos versos, no llegaba a los 5 que exige el reinicio
# por numeración y su texto acababa pegado al Sal 118:1-2.
RE_CABECERA_SALMO = re.compile(
    r"^\s*(\d{1,3})\s*[.,:]?\s*\(\s*Vulg\s*[.,:]?\s*\d{1,3}\s*[.,:]?\s*\)\s*$")


# Cabecera de salmo que el OCR no deja en su forma limpia: número suelto
# en los salmos que la Vulgata numera igual («5», «148,»), dos números
# («114, 115 (Vulg. 113.) (1).»), paréntesis sin cerrar («22 (Vulg. 21»)
# o restos alrededor («| 85 (Vulg. 84.)», «66 (Vulg. 65.) +»). No abre
# capítulo -- eso sigue siendo cosa de RE_CABECERA_SALMO y del reinicio de
# la numeración --: solo avisa de que puede venir un epígrafe, que se
# confirma si detrás abre salmo (verso 1 o 2).
RE_CABECERA_LAXA = re.compile(
    r"^\W{0,3}(\d{1,3})(?:\s*,?\s*\d{1,3})?\s*[.,:]?\s*"
    r"(?:\(\s*Vulg\b.*)?\W{0,3}$")


# Palabra partida con guion al final del texto.
RE_CORTE = re.compile(r"[A-Za-záéíóúüñÁÉÍÓÚÜÑ][-¬‐]$")


# Renglones sangrados que no son recorte: desborde de Job («[ticia.») y
# primera línea de nota al pie colada en el cuerpo («(1) Esta …»).
RE_NO_BORDE = re.compile(r"^\s*(?:\[|\(\s*[\dIl]\s*\))")


def _margen(lineas):
    """(borde izquierdo, borde derecho, ancho medio de letra), solo palabras."""
    ws = [[w for w in l if any(c.isalpha() for c in w[5])] for l in lineas]
    x1s = [min(w[0] for w in p) for p in ws if p]
    x2s = [max(w[1] for w in p) for p in ws if p]
    anchos = sorted((w[1] - w[0]) / len(w[5]) for p in ws for w in p)
    if not x1s:
        return None, None, 0
    letra = anchos[len(anchos) // 2]
    # Cada borde es donde empiezan (o acaban) más renglones, no un
    # percentil: los restos de la otra columna (Éx 26, «oro», «rar», «las»)
    # lo corrían y todo renglón normal parecía sangrado.
    paso = max(1, int(letra))

    def moda(xs, elige):
        cuenta = {}
        for x in xs:
            cuenta[x // paso] = cuenta.get(x // paso, 0) + 1
        b = max(cuenta, key=lambda k: (cuenta[k], -k))
        return elige(x for x in xs if x // paso == b)

    return moda(x1s, min), moda(x2s, max), letra


def _umbral(lineas):
    """Blanco que separa bloques: medio alto de renglón de la columna."""
    altos = sorted(max(w[3] for w in l) - min(w[2] for w in l) for l in lineas)
    return altos[len(altos) // 2] / 2 if altos else 0


def epigrafes(lineas, cands=None):
    """Índices de los renglones que la composición aparta como epígrafe.

    es_titulo() solo mira el texto y toma por epígrafe cualquier
    continuación sin cifra ni punto final: «| son de mí muy honrados, | en
    ellos» (Sal 16:3), «¡Oh Yave! ¿Quién es el que podrá» (Sal 15:1). En la
    página el epígrafe va apartado por blanco: un bloque de uno a tres
    renglones con hueco arriba y abajo (o el borde de la columna), sin
    marca de verso («Seguridad del justo en el castigo de los | impíos.»).
    Una continuación va pegada a sus vecinos con el interlineado normal.

    El hueco se mide contra la mitad del alto de renglón de la columna. La
    posición horizontal no sirve: el epígrafe va en negrita y sus cajas de
    OCR no dan márgenes fiables.
    """
    if not lineas:
        return set()
    caja = [(min(w[2] for w in l), max(w[3] for w in l)) for l in lineas]
    umbral = _umbral(lineas)
    bloques, cur = [], [0]
    for i in range(1, len(lineas)):
        if caja[i][0] - caja[i - 1][1] >= umbral:
            bloques.append(cur)
            cur = [i]
        else:
            cur.append(i)
    bloques.append(cur)
    # Solo cuentan las palabras: el filete de la columna («|») y las comas
    # sueltas del medianil estiran la caja hasta el borde
    # («, Separación de Abram y Lot. |»).
    palabras = [[w for w in l if any(c.isalpha() for c in w[5])]
                for l in lineas]
    x1s = sorted(min(w[0] for w in ws) for ws in palabras if ws)
    x2s = sorted(max(w[1] for w in ws) for ws in palabras if ws)
    if x1s:
        izq, der = x1s[len(x1s) // 5], x2s[len(x2s) * 4 // 5]

    def centrado(i):
        ws = palabras[i]
        if not ws:
            return False
        a = min(w[0] for w in ws) - izq
        z = der - max(w[1] for w in ws)
        return a >= 40 and z >= 40 and abs(a - z) <= 0.25 * (a + z) + 10

    def abre(i):
        # La cabecera «95 (Vulg. 94.)» y el titulillo con número de página
        # no son marca de verso: con ellas en el bloque, el epígrafe del
        # salmo («Exhortación a la alabanza y obediencia») pasaba al cuerpo.
        t = texto(lineas[i]).strip()
        return (not RE_CABECERA_SALMO.match(t) and not es_ruido(t)
                and not es_running_header(t, cands)
                and abre_versiculo(t) is not None)

    out = set()
    for k, b in enumerate(bloques):
        # El primer bloque corto de la columna es la zona del titulillo: su
        # número de página («114 E MN = 7 NC EVITE», «176 DEUTERO») parece
        # marca de verso y no debe rescatar la basura de al lado.
        if len(b) <= 3 and (k == 0 or not any(abre(i) for i in b)):
            out.update(b)
        elif k > 0 and texto(lineas[bloques[k - 1][-1]]).strip().endswith(
                (".", "!", "?", "»", ")")):
            # Un verso que acaba en punto, un blanco y, antes de la primera
            # marca de verso, un renglón centrado: es epígrafe pegado a su
            # verso («Separación de Abram y Lot.» sobre «⁵ También Lot», Gn
            # 13). Centrado, porque tras un blanco también sigue la prosa de
            # una introducción o una estrofa (Eclo 51), y esas van a ancho
            # completo o sangradas.
            for i in b:
                if abre(i):
                    break
                # Sin el filete final, un renglón que acaba en coma sigue
                # la frase («Alabad al escudo de Abraham, |», Eclo 51).
                fin = texto(lineas[i]).strip().rstrip(" |")
                if centrado(i) and not fin.endswith((",", ";", ":", "-")):
                    out.add(i)
    return out


def _descarte(motivo, l, t, cands, origen):
    """Renglón que el parser no pasa al texto: queda como evidencia.

    Solo si lleva letras (dos o más): «|» o «—» sueltos no son texto.
    """
    if sum(c.isalpha() for c in t) < 2:
        return []
    return [("perdida", "descarte:" + motivo, t, cands, _origen(l, origen))]


def _sucesos_linea(l, t, cands, origen, epigrafe=True):
    if es_ruido(t):
        return _descarte("ruido", l, t, cands, origen)
    if epigrafe and es_titulo(t):
        return _descarte("titulo", l, t, cands, origen)
    if es_running_header(t, cands):
        return _descarte("titulillo", l, t, cands, origen)
    out = []
    ini, quitado = _abre(t)
    # quita_basura() deja ver la marca quitando lo que va delante; si eso
    # lleva minúsculas era texto, casi siempre el final del verso anterior
    # («enemi- | gos. 13 Y en aquella hora», Ap 11:12). Se pierde del
    # texto, pero queda registrado.
    if ini and any(c.islower() for c in quitado):
        out.append(("perdida", "fragmento_descartado", quitado, cands,
                    _origen(l, origen)))
    elif ini and quitado:
        # Marca que solo aparece quitando basura («*7 amorreos», la inicial
        # «5» de Jos 5 leída a trozos). Sola no prueba nada; construir.py
        # la usa si además la numeración retrocede detrás.
        out.append(("perdida", "marca_tras_basura", quitado, cands,
                    _origen(l, origen)))
    if ini and ini[0] == "capver":
        out.append(("cap", ini[1], cands))
        resto = ini[2]
        trozos = parte_internos(resto)
        out.append(("vers", 1, trozos[0][1], cands, True, _origen(l, origen)))
        for num, txt in trozos[1:]:
            out.append(("vers", num, txt, cands, False, _origen(l, origen)))
        return out
    resto = ini[2] if ini else t
    trozos = parte_internos(resto)
    primero, cola = trozos[0], trozos[1:]
    if ini:
        out.append(("vers", ini[1], primero[1], cands, True, _origen(l, origen)))
    elif primero[1]:
        out.append(("sigue", primero[1], cands, _origen(l, origen)))
    for num, txt in cola:
        out.append(("vers", num, txt, cands, False, _origen(l, origen)))
    return out


def corriente(lineas, cands=None, origen=None):
    """
    Sucesos:
      ("cap", n, cands)
      ("cap", n, cands, "cabecera")   cabecera impresa de salmo
      ("vers", n, texto, cands, True/False)
      ("sigue", texto, cands)
      ("epigrafe", texto, cands, origen)   epígrafe editorial de salmo

    La cabecera de salmo abre capítulo justo delante de su primer verso,
    no en su propio renglón. Lo que va entre medias se reparte así: el
    epígrafe («Seguridad del justo en el castigo de los | impíos.») sale
    como suceso propio y construir.py lo guarda aparte; lo que viene detrás
    del epígrafe sin número de verso (el título «Salmo de David.» cuyo «1»
    no leyó el OCR) sigue yendo a donde iba.

    El epígrafe son los renglones que siguen a la cabecera, hasta tres, y
    acaba en la primera marca de verso o en el primer blanco: en la página
    va apartado del título y del verso 1, aunque a veces casi pegado (Sal
    91, 12 px). Antes se pegaba al último verso del salmo anterior (Sal
    17:15 «…Canto triunfal de David.», Sal 117:2 «…¡Aleluya! Canto
    triunfal.») o, si es_titulo() lo tomaba por título, se perdía.
    """
    out = []
    cabecera = None
    en_epigrafe = epigrafes(lineas, cands)
    umbral = _umbral(lineas)
    epigrafe = None      # renglones del epígrafe en curso, o None
    previo = None        # último renglón con tinta, para medir el blanco
    laxa = []            # epígrafe tras cabecera laxa: (suceso, renglón, i)
    # Páginas de Salmos o sin titulillo leído (Sal 7, 9, 22, 42, 85, 148,
    # 149 caen en hojas sin él); la confirmación por verso 1-2 hace el resto.
    salmos = not cands or "Ps" in cands
    margen, derecho, letra = _margen(lineas)
    anterior = ""        # texto del último renglón que pasó al cuerpo
    alto = 2 * _umbral(lineas)   # alto mediano de renglón
    capitular = False    # ¿el renglón anterior llevaba la inicial grande?
    tras_verso_1 = False  # ¿el renglón anterior abría el verso 1?
    tras_capitulo = 0    # renglones junto a la inicial grande del capítulo
    for i, l in enumerate(lineas):
        t = texto(l).strip()
        m = RE_CABECERA_SALMO.match(t)
        if m:
            # Una cabecera laxa sin confirmar («21», número de verso solo en
            # su renglón, Sal 51) no se lleva los renglones que guardaba.
            for _suceso, lx, ix in laxa or ():
                out += _sucesos_linea(lx, texto(lx).strip(), cands, origen,
                                      ix in en_epigrafe)
            cabecera = int(m.group(1))
            epigrafe, previo, laxa = 0, l, None
            continue
        if (salmos and epigrafe is None and cabecera is None
                and RE_CABECERA_LAXA.match(t)):
            epigrafe, previo, laxa = 0, l, []
            continue
        if epigrafe is not None:
            if es_ruido(t) or es_running_header(t, cands):
                previo = l
                continue
            hueco = min(w[2] for w in l) - max(w[3] for w in previo)
            ini = abre_versiculo(t)
            if (ini is not None or epigrafe == 3
                    or (epigrafe and hueco >= umbral)):
                if laxa is not None:
                    # Tras una cabecera laxa solo es epígrafe si detrás
                    # abre salmo; si no, esos renglones siguen su camino.
                    abre_salmo = ini is not None and (
                        ini[0] == "capver" or ini[1] <= 2)
                    for suceso, lx, ix in laxa:
                        if abre_salmo:
                            out.append(suceso)
                        else:
                            out += _sucesos_linea(
                                lx, texto(lx).strip(), cands, origen,
                                ix in en_epigrafe)
                    laxa = None
                epigrafe = None
            else:
                suceso = ("epigrafe", t, cands, _origen(l, origen))
                if laxa is not None:
                    laxa.append((suceso, l, i))
                else:
                    out.append(suceso)
                epigrafe += 1
                previo = l
                continue
        # Entre la cabecera del salmo y su primer verso solo hay epígrafe,
        # aunque vaya pegado al verso («Canto a la providencia de Dios sobre
        # | el justo.», Sal 91).
        nuevos = _sucesos_linea(l, t, cands, origen,
                                i in en_epigrafe or cabecera is not None)
        cuerpo = [s for s in nuevos if s[0] in ("vers", "sigue")]
        ws = [w for w in l if any(c.isalpha() for c in w[5])]
        sangria = (min(w[0] for w in ws) - margen
                   if ws and margen is not None else 0)
        if (cuerpo and cuerpo[0][0] == "sigue" and RE_CORTE.search(anterior)
                and not tras_capitulo and not capitular and not tras_verso_1
                and not RE_NO_BORDE.match(t) and sangria >= 3 * letra
                and derecho - max(w[1] for w in ws) <= 2 * letra
                and max(w[1] for w in ws) - min(w[0] for w in ws)
                >= (derecho - margen) / 2):
            # Palabra partida al final del renglón, y el siguiente empieza
            # lejos del margen pero llega al derecho como la prosa
            # justificada: el OCR perdió el principio de ese renglón («gran
            # con- | [suelo] por vuestra fe», 1 Tes 3:7, junto al
            # medianil). No cuentan los renglones junto a la inicial grande
            # del capítulo o del libro, que los sangra, el desborde de Job
            # («ino- | [cente,», corto y a la derecha), la nota al pie colada
            # («(1) Esta…»), ni un título que no llega al borde derecho.
            nuevos.insert(nuevos.index(cuerpo[0]), (
                "perdida", "borde_perdido", t, cands, _origen(l, origen)))
        if cuerpo:
            anterior = (cuerpo[-1][2] if cuerpo[-1][0] == "vers"
                        else cuerpo[-1][1])
        capitular = any(w[3] - w[2] >= 1.6 * alto for w in l)
        # La inicial del capítulo no siempre se lee (Am 1:1) ni se lee
        # grande (Gn 6:1, «0» de 51 px): el renglón tras el que abre el
        # verso 1 va sangrado por ella.
        tras_verso_1 = bool(cuerpo) and any(
            s[0] == "vers" and s[1] == 1 and s[4] for s in cuerpo)
        # «12 ¹ Viendo Jonatán que las cir- | cunstancias…»: la inicial del
        # capítulo ocupa dos renglones y sangra los siguientes sin que falte
        # nada (1 Mac 12:1, Am 2:1, 2 Re 6:1).
        if any(s[0] == "cap" and len(s) == 3 for s in nuevos):
            tras_capitulo = 2
        elif tras_capitulo:
            tras_capitulo -= 1
        if cabecera is not None:
            i = next((k for k, s in enumerate(nuevos)
                      if s[0] in ("vers", "cap")), None)
            if i is not None:
                if nuevos[i][0] == "cap":
                    nuevos[i] = nuevos[i][:3] + ("cabecera",)
                else:
                    nuevos.insert(i, ("cap", cabecera, cands, "cabecera"))
                cabecera = None
        out += nuevos
    if laxa:
        # Fin de columna sin confirmar: los renglones siguen su camino.
        for _suceso, lx, ix in laxa:
            out += _sucesos_linea(lx, texto(lx).strip(), cands, origen,
                                  ix in en_epigrafe)
    if cabecera is not None:
        out.append(("cap", cabecera, cands, "cabecera"))
    return out


def corrige_repetidos(sucesos):
    """Número de verso leído una unidad de más: «4, 6, 6, 7» es «4, 5, 6, 7».

    El OCR confunde el volado pequeño: sobre todo 5 con 6 (también 15, 25…),
    y a veces 2 con 3 o 18 con 19. El verso impreso n-1 llega como n, se
    queda vacío y su texto se funde con el verdadero n (Sal 13:5, Sal 17:5,
    Lv 13:2). La señal es estructural: un número repetido, con su predecesor
    ausente justo delante, y confirmado detrás por n+1 o por el fin del
    capítulo. Con n+2 detrás («4, 6, 6, 8») no se sabe cuál de los dos es el
    erróneo y se deja como está.
    """
    out = list(sucesos)
    vers = []

    def repasa():
        for k in range(1, len(vers) - 1):
            (_, a), (i, b), (_, c) = vers[k - 1], vers[k], vers[k + 1]
            d = vers[k + 2][1] if k + 2 < len(vers) else None
            if b == c and a == b - 2 and d in (None, 1, c + 1):
                out[i] = out[i][:1] + (b - 1,) + out[i][2:]
        vers.clear()

    for i, s in enumerate(out):
        if s[0] in ("libro", "cap"):
            repasa()
        elif s[0] == "vers":
            vers.append((i, s[1]))
    repasa()
    return out


RE_DOLAR = re.compile(r"(?:^|\s)\$(?:\s|$)")


def _origenes(sucesos):
    """Texto de cada renglón -> su procedencia (el primero que aparezca)."""
    out = {}
    for s in sucesos:
        if s[0] == "vers" and len(s) > 5:
            out.setdefault(s[2], s[5])
        elif s[0] == "sigue" and len(s) > 3:
            out.setdefault(s[1], s[3])
    return out


def parte_dolar(vers, procedencia, sucesos):
    """Volado ⁶ leído «$» con el ⁵ leído 6: «… salvo. $ Está por mí …».

    Variante de corrige_repetidos() que no puede verse antes de ensamblar:
    el ⁵ llega como 6 y el ⁶ no se lee como cifra sino como un «$» suelto,
    así que no hay número repetido. El verso 5 queda vacío y el 6 lleva los
    dos (Sal 118:5-6, Lc 10:5-6, Prov 10:5-6). Partir antes de ensamblar
    cambia la longitud de capítulos que el alineador compara y descoloca
    otros (Lv 9-10); por eso se parte aquí, sobre los versos ya colocados.

    Señal: el verso b acaba en 6, b-1 está vacío, b-2 y b+1 tienen texto y
    b lleva un único «$» suelto con texto delante y detrás. Lo de delante es
    b-1 y lo de detrás b. Con otra cifra el «$» puede ser otro número
    (Mt 2:5, «$» es el 6 con el 4 perdido) y no se toca.
    """
    origen = _origenes(sucesos)
    partidos = []
    for clave in sorted(vers):
        osis, cap, b = clave
        if b % 10 != 6:
            continue
        previo = (osis, cap, b - 1)
        if vers.get(previo) or not vers.get((osis, cap, b - 2)) or \
                not vers.get((osis, cap, b + 1)):
            continue
        trozos = vers[clave]
        con = [k for k, t in enumerate(trozos) if RE_DOLAR.search(t)]
        if len(con) != 1 or len(RE_DOLAR.findall(trozos[con[0]])) != 1:
            continue
        k = con[0]
        antes, despues = (t.strip() for t in RE_DOLAR.split(trozos[k]))
        delante = [t for t in trozos[:k] + [antes] if t.strip()]
        detras = [t for t in [despues] + trozos[k + 1:] if t.strip()]
        if not delante or not detras or trozos[k] not in origen:
            continue
        vers[previo] = delante
        vers[clave] = detras
        if clave in procedencia:
            procedencia[previo] = procedencia[clave]
        procedencia[clave] = [origen[trozos[k]]]
        partidos.append(clave)
    return partidos


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
