"""
Etapa entre construir.py y osis.py. Con la política actual no completa nada.

Contrato (NACAR-FALLBACK-101):
  - Ningún verso de texto.json se modifica: el texto de Nácar-Colunga,
    completo o a medias, sale como lo dejó construir.py. texto.json no se
    reescribe.
  - Un verso sin texto no se rellena. osis.py no lo escribe y el visor lo
    suple al leer el módulo, desde otra Biblia y con su aviso.
  - reconstruidos.txt se escribe vacío: osis.py y revision.py lo leen para
    marcar versos «reconstruido-testigos», que hoy son cero.
  - No usa otras Biblias: ni diatheke, ni testigos.pkl, ni red.

Antes se tenía por incompleto todo verso con menos palabras que el 80 % de
un testigo (Reina-Valera o Platense) y se cambiaba por él. Eso comparaba la
longitud de dos traducciones distintas: la auditoría encontró 4.602 versos
sustituidos (21.377 palabras de Nácar perdidas) y, en el facsímil, versos
completos como Sal 17:7 o Gn 38:6 cambiados por Reina-Valera sin rastro en
la interfaz. No hay metadata que distinga un verso truncado de uno
completo, así que no se decide por longitud, puntuación ni parecido.

Las herramientas de cotejo (descarga, testigo_nrsva, mejor_testigo, tejer)
siguen aquí para una recuperación futura basada en metadata explícita de
cuerpo perdido, pero main() no las llama.
"""
import json
import os
import pickle
import re
import subprocess
import unicodedata

from canon import ORDEN, TITULO_SALMOS

DIR = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(DIR, "fuentes", "testigos.pkl")
RE_TOK = re.compile(r"[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+")
RE_LINEA = re.compile(r"^(.+?)\s(\d+):(\d+):\s?(.*)$")
MODULOS = ("SpaRV", "SpaPlatense", "SpaRVG")


def sin_tildes(s):
    s = unicodedata.normalize("NFD", s)
    return "".join(c for c in s if unicodedata.category(c) != "Mn")


def nrm(w):
    return sin_tildes(w.lower())


def toks(s):
    return RE_TOK.findall(s or "")


def limpia_diatheke(s):
    s = re.sub(r"<[^>]+>", " ", s)
    s = re.sub(r"\s+", " ", s).strip()
    return s


# --- Herramientas de cotejo con otras Biblias --------------------------------
# No las usa main(). Se conservan para una recuperación futura que solo actúe
# sobre versos con cuerpo perdido marcado de forma explícita por el parser, y
# para auditar la política retirada. descarga() lanza diatheke si no existe
# fuentes/testigos.pkl.


def descarga():
    if os.path.exists(CACHE):
        with open(CACHE, "rb") as f:
            return pickle.load(f)
    out = {}
    for mod in MODULOS:
        d = {}
        for osis in ORDEN:
            try:
                r = subprocess.run(
                    ["diatheke", "-b", mod, "-k", osis],
                    capture_output=True, text=True, timeout=180)
            except Exception:
                continue
            for ln in r.stdout.splitlines():
                m = RE_LINEA.match(ln)
                if not m:
                    continue
                t = limpia_diatheke(m.group(4))
                if t:
                    d[f"{osis} {int(m.group(2))}:{int(m.group(3))}"] = t
        out[mod] = d
        print(f"  {mod:14} {len(d)} versículos", flush=True)
    os.makedirs(os.path.dirname(CACHE), exist_ok=True)
    with open(CACHE, "wb") as f:
        pickle.dump(out, f, -1)
    return out


def casa(a, b):
    """¿La misma palabra, o un trozo (mentos/juramentos, eslos/cielos)?"""
    na, nb = nrm(a), nrm(b)
    if na == nb:
        return True
    if len(na) >= 4 and len(nb) >= 4 and (nb.endswith(na) or na.endswith(nb)):
        return True
    if len(na) >= 5 and len(nb) >= 5 and abs(len(na) - len(nb)) <= 2:
        d = sum(x != y for x, y in zip(na, nb)) + abs(len(na) - len(nb))
        return d <= 1
    return False


def solapamiento(ocr_t, wit_t):
    """Cuántas palabras del OCR aparecen en orden en el testigo."""
    j = 0
    n = 0
    for w in ocr_t:
        for k in range(j, len(wit_t)):
            if casa(w, wit_t[k]):
                n += 1
                j = k + 1
                break
    return n


RE_TITULO_RVG = re.compile(r"^«[^»]*»\s*")
# Reina-Valera 1909 abre el verso en versalitas: «… su hijo. ¡OH Jehová».
RE_VERSALITAS = re.compile(r"[¡¿]?[A-ZÁÉÍÓÚÜÑ]{2,}")


def testigo_nrsva(mod, ref, wit):
    """El verso del testigo con la numeración de Nácar-Colunga (NRSVA).

    En Salmos los testigos no numeran igual: la Platense va por la Vulgata
    (otra cuenta de salmos, título como versículo) y las Reina-Valera meten
    el título dentro del versículo 1. construir.py ya saca el título de
    Nácar-Colunga a su propio <title>, así que el testigo tiene que llegar
    sin él o el relleno lo volvería a pegar al verso.
    """
    if not wit or not ref.startswith("Ps "):
        return wit
    if mod == "SpaPlatense":
        return None
    cap, ver = (int(x) for x in ref[3:].split(":"))
    if ver != 1:
        return wit
    if mod == "SpaRVG":
        # SpaRVG pone «…» delante del v. 1 también en salmos sin título
        # hebreo («El piadoso será prosperado…», Sal 1): nunca es verso.
        return RE_TITULO_RVG.sub("", wit) or None
    if cap not in TITULO_SALMOS:
        return wit
    m = RE_VERSALITAS.search(wit)
    return wit[m.start():] if m else None


def mejor_testigo(ocr, testigos, ref):
    ocr_t = toks(ocr)
    mejor, sc = None, -1
    for mod, d in testigos.items():
        wit = testigo_nrsva(mod, ref, d.get(ref))
        if not wit:
            continue
        s = solapamiento(ocr_t, toks(wit))
        if s > sc:
            sc, mejor = s, wit
    return mejor, sc


def _pos_palabra(s, n):
    """Inicio en s de la n-ésima palabra (0-index)."""
    for i, m in enumerate(RE_TOK.finditer(s)):
        if i == n:
            return m.start()
    return 0


def tejer(ocr, wit):
    """
    Conserva el castellano del OCR donde coincide y rellena los huecos
    con el testigo, copiando su puntuación.

    «También habéis oído que fué mentos» + RV →
    «También habéis oído que fué dicho á los antiguos: … juramentos.»
    """
    ocr_t = toks(ocr)
    wit_t = toks(wit)
    if not ocr_t or not wit_t:
        return None
    i = 0
    pref = []
    while i < len(ocr_t) and not any(casa(ocr_t[i], w) for w in wit_t[:8]):
        if len(ocr_t[i]) >= 3:
            pref.append(ocr_t[i])
        i += 1
    start_j = 0
    vistos = 0
    if i < len(ocr_t):
        for j, w in enumerate(wit_t):
            if casa(ocr_t[i], w):
                start_j = j
                break
        k = i
        for w in wit_t[start_j:]:
            for p in range(k, min(k + 5, len(ocr_t))):
                if casa(ocr_t[p], w):
                    vistos += 1
                    k = p + 1
                    break
    if vistos < 3:
        return None
    cola = wit[_pos_palabra(wit, start_j):].strip()
    if pref:
        return " ".join(pref) + " " + cola
    return cola


def completar_todo(texto):
    """(texto, versos tocados) con la política actual: nada se toca.

    Un verso con texto de Nácar se conserva; uno sin texto no se rellena
    (lo suple el visor). No hay testigos que consultar.
    """
    return dict(texto), []


def main():
    path = os.path.join(DIR, "texto.json")
    texto = json.load(open(path, encoding="utf-8"))
    _, tocados = completar_todo(texto)
    # texto.json no se reescribe: sale byte a byte como lo dejó construir.py.
    with open(os.path.join(DIR, "reconstruidos.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(tocados))
    print(f"versículos: {len(texto)}; completados con otras Biblias: "
          f"{len(tocados)}")


if __name__ == "__main__":
    main()
