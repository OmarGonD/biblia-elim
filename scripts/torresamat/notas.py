"""
Las notas de Torres Amat, como comentario por capítulo.

Las notas son medio libro y son lo que distingue a esta Biblia: Torres
Amat explica, cita a los Padres, discute el hebreo y sale al paso de las
objeciones de su siglo. El segmentador ya las separa del texto en cada
página; lo que faltaba era colocarlas.

Y colocarlas versículo a versículo no se puede. La llamada de nota es un
superíndice diminuto, y el OCR o se lo come o lo pega a la palabra
vecina: contando llamadas en el cuerpo y notas al pie de la misma media
hoja, solo coinciden en el 6 % de los casos, y por geometría -- buscando
palabras pequeñas y levantadas -- salen "un", "NES", "sus" y barras del
canto. Atribuir con esos datos sería inventarse a qué versículo comenta
cada nota.

Así que se publican por capítulo, en el orden en que están impresas, como
un módulo de comentario aparte. El lector las tiene al lado del capítulo
que está leyendo, que es como se leen en el libro, y no se le miente
sobre a qué versículo pertenece cada una.
"""
import json, re, unicodedata
from load import load, X1, X2
from paginas import pagina
from cabeceras import medias_hojas
from segment import punto_de_corte, texto, canal
from canon import POR_OSIS
from construir import ORDEN, CORTES

def _clave(s, n=40):
    s = unicodedata.normalize("NFD", s)
    s = "".join(c for c in s if unicodedata.category(c) != "Mn")
    return re.sub(r"[^a-z]", "", s.lower())[:n]

def indice_versiculos(txt):
    """{arranque normalizado del versículo: (osis, cap)} para la búsqueda inversa."""
    idx = {}
    for ref, t in txt.items():
        k = _clave(t)
        if len(k) >= 20:
            idx.setdefault(k, ref.rsplit(":", 1)[0])
    return idx

def capitulo_de(lineas_cuerpo, idx):
    """A qué capítulo pertenece una media hoja, por su texto."""
    votos = {}
    for l in lineas_cuerpo:
        k = _clave(re.sub(r"^\s*\d{1,3}\s*[.,:;]\s*", "", texto(l)))
        for n in (40, 30, 24):
            ref = idx.get(k[:n])
            if ref:
                votos[ref] = votos.get(ref, 0) + 1
                break
    if not votos:
        return None
    return max(votos, key=votos.get)

RE_ARRANQUE = re.compile(r"^\s*(?:\d{1,2}|[*'’‘\"”“$e†‡~^])\s+(?=[A-ZÁÉÍÓÚ¡¿(])")

def agrupa_notas(lineas_nota):
    """Une los renglones de nota en notas sueltas."""
    notas, cur = [], []
    for l in lineas_nota:
        t = texto(l).strip()
        if not t or len(t) < 3:
            continue
        if RE_ARRANQUE.match(t) and cur:
            notas.append(cur); cur = [t]
        else:
            cur.append(t)
    if cur:
        notas.append(cur)
    out = []
    for n in notas:
        s = ""
        for t in n:
            t = t.strip()
            if s.endswith("-"):
                s = s[:-1] + t
            elif s:
                s += " " + t
            else:
                s = t
        s = re.sub(r"\s{2,}", " ", s).strip()
        if len(s) > 12:
            out.append(s)
    return out

def recoge(txt):
    """{(osis, cap): [notas]} de los cuatro tomos."""
    idx = indice_versiculos(txt)
    por_cap = {}
    for tomo in ("I", "II", "III", "IV"):
        orig = load(tomo)
        for i in range(len(orig)):
            p = pagina(tomo, i, orig)
            if not p["words"]:
                continue
            for cab, ls in medias_hojas(p):
                if not ls:
                    continue
                k = punto_de_corte(ls)
                if k == 0 or k >= len(ls):
                    continue
                ref = capitulo_de(ls[:k], idx)
                if not ref:
                    continue
                notas = agrupa_notas(ls[k:])
                if notas:
                    por_cap.setdefault(ref, []).extend(notas)
    return por_cap

if __name__ == "__main__":
    # El índice se construye con el texto ANTERIOR a la corrección
    # ortográfica: la búsqueda inversa compara con los renglones que salen
    # del OCR, y si el índice ya lleva "tercero" donde el papel dice
    # "tereero" no casa nada. Se perdían 340 medias hojas por esto.
    txt = json.load(open("texto_sin_orto.json", encoding="utf-8"))
    d = recoge(txt)
    tot = sum(len(v) for v in d.values())
    print(f"capítulos con notas: {len(d)}   notas: {tot}")
    json.dump({k: v for k, v in d.items()},
              open("notas.json", "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)
    for ref in ("Gen 5", "Gen 6", "Matt 6", "John 1"):
        if ref in d:
            print(f"\n── {ref}: {len(d[ref])} notas")
            for n in d[ref][:2]:
                print(f"   {n[:150]}")
