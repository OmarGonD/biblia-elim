"""
Rescata a mano los capítulos que el alineador dejó vacíos.

En casi todos el texto está en el OCR y bien leído: lo que falló fue la
asignación. Aquí se dice, capítulo por capítulo, de qué media hoja sale y
por dónde empieza, y los versículos se toman de la corriente de sucesos
tal cual -- que es más fiel que transcribir a ojo del facsímil.

Cada entrada de RESCATES se ha comprobado mirando la página escaneada:
que el capítulo es el que dice, que la numeración impresa es la que se
copia, y que no se invade el capítulo siguiente.
"""
import json, re, unicodedata
from load import load
from cabeceras import medias_hojas
from segment import punto_de_corte, canal
from versiculos import corriente, une
from nombres import candidatos
from portadas import es_portada
from paginas import pagina
from canon import POR_OSIS
from construir import ORDEN, CORTES
from load import X1, X2

# (osis, cap, tomo, [(pliego, lado), ...], ancla, arreglos)
#   fuentes:  las medias hojas por las que se reparte el capítulo, en orden
#   ancla:    trozo del comienzo del capítulo, para situarlo en la corriente
#   arreglos: {nº de versículo: por dónde empieza}. Sirve para los dos
#             estropicios del OCR: el versículo que perdió su número y
#             quedó pegado al anterior, y el que lo tiene mal leído
#             (Daniel 1:2 sale como "9"). Cada uno se ha comprobado en el
#             facsímil antes de escribirlo aquí.
#   pegar:    trozos que el OCR tomó por número de versículo sin serlo. Uno
#             solo hace estrago: en Ezequiel 1 un "17" espurio dentro del
#             versículo 9 se tragaba por monotonía los versículos 10 a 16.
RESCATES = [
    ("2Cor", 7, "IV", [(236, 1)], "Teniendo pues",
     {9: "Al presente me alegro"}, []),
    # El encabezado de Colosenses 4 dice "CAPITULO VI" en el facsímil: es
    # errata de la edición de 1882, la carta tiene cuatro capítulos. El
    # versículo 1 cierra la cara izquierda y el resto va en la derecha.
    ("Col", 4, "IV", [(273, 0), (273, 1)], "Amos, tratad", {}, []),
    ("1Kgs", 3, "II", [(78, 1), (79, 0)], "Salomon pues, afianzado", {}, []),
    ("Bar", 5, "III", [(277, 1)], "Desnúdate, oh Jerusalem", {}, []),
    ("Mal", 4, "III", [(390, 1)], "llegará aquel dia semejante", {}, []),
    # De Jeremías 45 solo se leen los versículos 3 a 5: los dos primeros
    # caen al pie de la media hoja anterior y el segmentador los da por
    # nota. Se rescata lo que hay.
    ("Jer", 45, "III", [(257, 0)], "Tú has exclamado", {}, []),
    # La cara izquierda del pliego 280 es el prefacio a Ezequiel, todo en
    # letra de nota: por eso el umbral fijo daba la página entera por
    # aparato y el capítulo se perdía. El texto empieza al pie.
    ("Ezek", 1, "III", [(280, 0), (280, 1), (281, 0), (281, 1)],
     "en el mes cuarto",
     {18: "Asimismo las ruedas tenian", 22: "Y sobre las cabezas de los animales"},
     ["volvian cuando andaban"]),
    ("Dan", 1, "III", [(322, 0), (322, 1), (323, 0)],
     "de Joakim rey de Judá", {2: "Y el Señor entregó en sus manos"}, []),
    ("Ps", 114, "III", [(59, 1)], "Amé al Señor",
     {3: "Cercáronme mortales angustias"}, []),
    ("Ps", 115, "III", [(59, 1), (60, 0)], "Creí", {}, [], -9),
    ("Ps", 130, "III", [(65, 0)], "no he sentido bajamente", {}, []),
    ("Ps", 147, "III", [(69, 1), (70, 0)], "Alaba al Señor, oh Jerusalem", {}, [], -11),
]

# Cola de nota al pie que se cuela en el último versículo de la media hoja.
RE_COLA_NOTA = re.compile(
    r"\s+(?:[0-9*]{0,3}\s*)?[OÓ] (?:uniros|á no dejaros|del|la)\b.*$")

# Transcripción a mano desde el facsímil, para lo que el OCR no da.
#
# El Salmo 1 cae al pie de la página del prefacio al Salterio, compuesto
# entero en letra de nota: no hay forma de separarlo por altura de letra,
# porque el aparato que lo rodea mide lo mismo. Son seis versículos y se
# copian leyéndolos de la imagen (tomo III, pliego 10).
TRANSCRITOS = {
    "Ps 1:1": "Dichoso aquel varon que no se deja llevar de los consejos de "
              "los malos, ni se detiene en el camino de los pecadores, ni se "
              "asienta en la cátedra pestilencial de los libertinos:",
    "Ps 1:2": "Sino que tiene puesta toda su voluntad en la Ley del Señor, y "
              "está meditando en ella dia y noche.",
    "Ps 1:3": "Él será como el árbol plantado junto á las corrientes de las "
              "aguas, el cual dará su fruto en el debido tiempo, y cuya hoja "
              "no caerá nunca: y cuanto él hiciere tendrá próspero efecto.",
    "Ps 1:4": "No así los impíos, no así; sino que serán como el tamo ó polvo "
              "que el viento arroja de la superficie de la tierra.",
    "Ps 1:5": "Por tanto no prevalecerán los impíos en juicio: ni los "
              "pecadores estarán en la asamblea de los justos.",
    "Ps 1:6": "Porque conoce el Señor y premia el proceder de los justos; mas "
              "la senda de los impíos terminará en la perdicion.",
}

def _norm(s):
    """
    Sin tildes, sin puntuación y SIN ESPACIOS: el OCR de esta edición pega
    palabras a capricho ("Enelaño trigésimo", "Masel rey David"), y con los
    espacios dentro ningún anclaje casaba.
    """
    s = unicodedata.normalize("NFD", s)
    s = "".join(c for c in s if unicodedata.category(c) != "Mn")
    return re.sub(r"[^a-z]", "", s.lower())

def sucesos_media_hoja(tomo, pliego, lado):
    originales = load(tomo)
    a, b = CORTES[tomo]
    ambito = set(ORDEN[a:b])
    p = pagina(tomo, pliego, originales)
    g = canal(p)
    mitades = [[w for w in p["words"] if w[X2] <= g],
               [w for w in p["words"] if w[X1] > g]]
    cab, ls = medias_hojas(p)[lado]
    if not ls:
        return []
    return corriente(ls[:punto_de_corte(ls, recuperar=True)], candidatos(cab, ambito))

def rescata(osis, cap, tomo, fuentes, ancla, arreglos=None, pegar=(), desplaza=0):
    """Devuelve {(osis,cap,ver): texto} para ese capítulo."""
    ev = []
    for pliego, lado in fuentes:
        ev += sucesos_media_hoja(tomo, pliego, lado)
    ev = list(ev)
    tope = POR_OSIS[osis]["versos"][cap - 1]
    ancla_n = _norm(ancla)
    ini = None
    for i, s in enumerate(ev):
        if s[0] == "vers" and ancla_n in _norm(s[2]):
            ini = i
            break
    if ini is None:
        return {}, f"{osis} {cap}: no encuentro el ancla {ancla!r}"
    # Marcas falsas: se degradan a continuación del versículo en curso.
    for trozo in pegar or ():
        t_n = _norm(trozo)
        for i, s2 in enumerate(ev):
            if s2[0] == "vers" and _norm(s2[2]).startswith(t_n):
                ev[i] = ("sigue", s2[2]) + tuple(s2[3:])
                break

    # Corregir los números mal leídos ANTES de repartir. Si se hace
    # después ya no sirve de nada: un número leído de más -- Daniel 1:2
    # como "9" -- se traga por monotonía todos los versículos que vienen
    # detrás, y luego no hay de dónde separarlos.
    for n, inicio in (arreglos or {}).items():
        ini_n = _norm(inicio)
        for i, s2 in enumerate(ev):
            if s2[0] == "vers" and _norm(s2[2]).startswith(ini_n):
                ev[i] = ("vers", n) + tuple(s2[2:])
                break

    # Esta edición numera algunos salmos a continuación del anterior en
    # vez de empezar por 1 -- el 115 arranca en el 10, siguiendo al 114, y
    # el 147 en el 12, siguiendo al 146 --, que es como los trae la
    # Vulgata impresa. El desplazamiento los devuelve a su numeración.
    if desplaza:
        ev = [("vers", s2[1] + desplaza) + tuple(s2[2:]) if s2[0] == "vers" else s2
              for s2 in ev]

    out, ver, trozos = {}, 0, []
    def cierra():
        if ver and trozos:
            out[(osis, cap, ver)] = une(trozos)
    for s in ev[ini:]:
        if s[0] == "cap" and ver:            # empieza el capítulo siguiente
            break
        if s[0] == "vers":
            n = s[1]
            if not (1 <= n <= tope) or n <= ver:
                if ver:
                    trozos.append(s[2])
                continue
            cierra()
            ver, trozos = n, [s[2]]
        elif s[0] == "sigue" and ver:
            trozos.append(s[1])
    cierra()
    # Versículos que el OCR dejó sin número: se parten del anterior por el
    # texto que en el facsímil abre el versículo.
    for n, inicio in (arreglos or {}).items():
        ini_n = _norm(inicio)
        if (osis, cap, n) in out:
            continue
        for (o, c, v) in sorted(out):
            if v >= n:
                continue
            cuerpo = out[(o, c, v)]
            pos = _norm(cuerpo).find(ini_n)
            if pos < 0:
                continue
            # recortar sobre el texto real usando la misma proporción
            palabras = cuerpo.split()
            for i in range(len(palabras)):
                if _norm(" ".join(palabras[i:])).startswith(ini_n):
                    out[(o, c, v)] = " ".join(palabras[:i]).strip(" ,;:")
                    out[(o, c, n)] = " ".join(palabras[i:]).strip()
                    break
            break
    return out, None

def main():
    texto = json.load(open("texto.json", encoding="utf-8"))
    nuevos = fallos = 0
    for entrada in RESCATES:
        osis, cap, tomo, fuentes, ancla, arreglos, pegar = entrada[:7]
        desplaza = entrada[7] if len(entrada) > 7 else 0
        d, err = rescata(osis, cap, tomo, fuentes, ancla, arreglos, pegar, desplaza)
        if err:
            print("  " + err); fallos += 1; continue
        tope = POR_OSIS[osis]["versos"][cap - 1]
        puestos = 0
        ultimo = max(v for _o, _c, v in d) if d else 0
        for (o, c, v), t in sorted(d.items()):
            if v == ultimo:
                t = RE_COLA_NOTA.sub("", t)
            k = f"{o} {c}:{v}"
            if not texto.get(k):
                texto[k] = t; puestos += 1
        nuevos += puestos
        print(f"  {osis} {cap:<3} {puestos:3} versículos de {tope}")
    for k, t in TRANSCRITOS.items():
        if not texto.get(k):
            texto[k] = t; nuevos += 1
    print(f"  transcritos a mano: {len(TRANSCRITOS)}")
    json.dump(texto, open("texto.json", "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)
    print(f"\nrescatados: {nuevos} versículos, {fallos} fallos")

if __name__ == "__main__":
    main()
