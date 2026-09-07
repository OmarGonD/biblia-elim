"""
Completa versos que el OCR dejó a medias, con las otras Biblias.

No se mete griego en el castellano: «mentos» no es una palabra griega,
es el final de «juramentos». El griego (Tisch) y las Biblias testigo
dicen qué iba en el hueco; el castellano que se rellena sale de la
Reina-Valera y de la Platense, que son de la misma época, y se
conservan las palabras que el OCR sí leyó (También, fué, pues).

Solo se toca un verso si hay al menos tres palabras del OCR, en orden,
dentro del testigo. Si no, se deja el fragmento: completar a ciegas
sería sustituir Nácar-Colunga por otra versión.
"""
import json
import os
import pickle
import re
import subprocess
import unicodedata

from canon import ORDEN

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


def mejor_testigo(ocr, testigos, ref):
    ocr_t = toks(ocr)
    mejor, sc = None, -1
    for d in testigos.values():
        wit = d.get(ref)
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


def hay_que_completar(ocr, wit):
    ot, wt = toks(ocr), toks(wit)
    if len(wt) < 6:
        return False
    if len(ot) >= max(8, int(len(wt) * 0.80)):
        return False
    return True


def completar_todo(texto, testigos):
    nuevo = dict(texto)
    tocados = []
    for ref, ocr in texto.items():
        wit, sc = mejor_testigo(ocr, testigos, ref)
        if not wit or sc < 3:
            continue
        if not hay_que_completar(ocr, wit):
            continue
        t = tejer(ocr, wit)
        if not t or t == ocr:
            continue
        if len(toks(t)) <= len(toks(ocr)):
            continue
        nuevo[ref] = t
        tocados.append(ref)
    return nuevo, tocados


def main():
    path = os.path.join(DIR, "texto.json")
    texto = json.load(open(path, encoding="utf-8"))
    print("testigos…", flush=True)
    tg = descarga()
    nuevo, tocados = completar_todo(texto, tg)
    json.dump(nuevo, open(path, "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "reconstruidos.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(tocados))
    print(f"versículos completados: {len(tocados)}")
    for ref in ("Matt 5:33", "Matt 5:34", "Matt 5:35", "Matt 5:36",
                "Matt 5:27", "Matt 5:6"):
        if ref in tocados:
            print(f"  {ref}: {nuevo[ref][:160]}")


if __name__ == "__main__":
    main()
