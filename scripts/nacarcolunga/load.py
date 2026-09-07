"""Carga el djvu.xml a una estructura compacta y la deja en caché."""
import os
import pickle
import xml.etree.ElementTree as ET

DIR = os.path.dirname(os.path.abspath(__file__))
XML = os.path.join(DIR, "fuentes", "djvu.xml")
CACHE = os.path.join(DIR, "fuentes", "cache.pkl")
REOCR = os.path.join(DIR, "reocr")
LEAF2PAGE = os.path.join(DIR, "fuentes", "princeton_leaf2page.json")

X1, X2, TOP, BOT, CONF, TXT = range(6)


def parse(path):
    pages, cur = [], None
    for ev, el in ET.iterparse(path, events=("start", "end")):
        if ev == "start" and el.tag == "OBJECT":
            cur = {"w": int(el.get("width", 0)),
                   "h": int(el.get("height", 0)),
                   "words": []}
        elif ev == "end" and el.tag == "WORD" and cur is not None:
            c = el.get("coords", "")
            try:
                x1, y2, x2, y1 = [int(v) for v in c.split(",")[:4]]
            except ValueError:
                el.clear()
                continue
            t = (el.text or "").strip()
            if t:
                top, bot = min(y1, y2), max(y1, y2)
                cur["words"].append((x1, x2, top, bot,
                                     int(el.get("x-confidence", 0) or 0), t))
            el.clear()
        elif ev == "end" and el.tag == "OBJECT" and cur is not None:
            pages.append(cur)
            el.clear()
    return pages


def _es_marca(t):
    if t.isdigit():
        return 1 <= int(t) <= 176
    if len(t) > 4:
        return False
    return not any(c.isalpha() for c in t)


def fusion_numeros(djvu, tess):
    """Tesseract lee mejor el castellano y peor los números de verso
    (salen ?, *, %). Se recuperan del djvu.xml por posición, escalando
    porque el jp2 no tiene el mismo tamaño que el XML."""
    if not tess["words"] or not djvu["words"] or not tess["w"] or not djvu["w"]:
        return tess
    sx = tess["w"] / djvu["w"]
    sy = tess["h"] / djvu["h"]
    digits = []
    for w in djvu["words"]:
        t = w[TXT].strip(" .")
        if t.isdigit() and 1 <= int(t) <= 176:
            cx = (w[X1] + w[X2]) / 2 * sx
            cy = (w[TOP] + w[BOT]) / 2 * sy
            digits.append((cx, cy, t, w))
    usado = set()
    # El OCR de Princeton suele leer la cifra, pero a veces pierde uno de
    # sus dígitos (``11`` -> ``1``).  Antes se añadían *todos* los números
    # del DjVu que quedaban sin usar.  Si Tesseract sí había leído la caja,
    # eso dejaba dos marcas superpuestas (``11 1 Dijo``); versiculos.py lo
    # toma por ``capítulo 11, versículo 1`` y el capítulo entero se corre.
    # Vincular primero las cajas que ya están encima de la misma marca hace
    # que el DjVu corrija una lectura incompleta, sin duplicarla.
    cerca = 55
    def numero_cercano(w):
        cx = (w[X1] + w[X2]) / 2
        cy = (w[TOP] + w[BOT]) / 2
        mejor, distancia = None, float("inf")
        for i, (dx, dy, _num, _dw) in enumerate(digits):
            if i in usado:
                continue
            # La coordenada Y pesa algo más: dos números en la misma
            # columna, pero en renglones distintos, no son la misma marca.
            d = abs(dx - cx) + abs(dy - cy) * 1.5
            if d < distancia:
                mejor, distancia = i, d
        return mejor if distancia < cerca else None

    out = []
    for tw in tess["words"]:
        t = tw[TXT]
        mejor = numero_cercano(tw) if _es_marca(t) else None
        if mejor is not None:
            num = digits[mejor][2]
            usado.add(mejor)
            # Conservamos una lectura válida solo cuando coincide.  ``1``
            # sobre la caja impresa de ``11`` sigue siendo un error: el
            # DjVu aporta el dígito perdido y evita que se abra un capítulo
            # falso en mitad de la página.
            if t != num:
                out.append((tw[X1], tw[X2], tw[TOP], tw[BOT], tw[CONF], num))
                continue
        out.append(tw)
    # Números de verso que tesseract ni siquiera marcó con un «?».
    for i, (dx, dy, num, dw) in enumerate(digits):
        if i in usado:
            continue
        x1 = int(dw[X1] * sx)
        x2 = int(dw[X2] * sx)
        top = int(dw[TOP] * sy)
        bot = int(dw[BOT] * sy)
        out.append((x1, x2, top, bot, dw[CONF], num))
    return {"w": tess["w"], "h": tess["h"], "words": out}


def _leaf2page():
    import json
    if not os.path.exists(LEAF2PAGE):
        return {}
    raw = json.load(open(LEAF2PAGE, encoding="utf-8"))
    return {int(k): int(v) for k, v in raw.items()}


def _con_cabecera(fused, tess):
    fused["canal"] = tess.get("canal") or tess["w"] * 48 // 100
    fused["src"] = tess.get("src")
    for k in ("cabecera_roja", "cabecera"):
        if tess.get(k) is not None:
            fused[k] = tess[k]
    return fused


def load():
    if os.path.exists(CACHE):
        with open(CACHE, "rb") as f:
            djvu = pickle.load(f)
    else:
        if not os.path.exists(XML):
            raise SystemExit(
                "No está fuentes/djvu.xml. Ejecuta primero: python3 bajar.py")
        djvu = parse(XML)
        os.makedirs(os.path.dirname(CACHE), exist_ok=True)
        with open(CACHE, "wb") as f:
            pickle.dump(djvu, f, -1)
    n = 0
    n_pkl = 0
    if os.path.isdir(REOCR):
        for name in os.listdir(REOCR):
            if name.endswith(".pkl") and name[:4].isdigit():
                n_pkl = max(n_pkl, int(name[:4]) + 1)
    n_out = max(len(djvu), n_pkl)
    pages = [{"w": 0, "h": 0, "words": []} for _ in range(n_out)]
    for i, p in enumerate(djvu):
        pages[i] = p
    l2p = _leaf2page()
    if os.path.isdir(REOCR):
        for i in range(n_out):
            rec = os.path.join(REOCR, f"{i:04d}.pkl")
            if not (os.path.exists(rec) and os.path.getsize(rec) > 100):
                continue
            with open(rec, "rb") as f:
                tess = pickle.load(f)
            src = str(tess.get("src") or "")
            if src.startswith("princeton:"):
                try:
                    leaf = int(src.split(":", 1)[1])
                except ValueError:
                    leaf = i
                printed = l2p.get(leaf)
                j = (printed + 99) if printed is not None else None
                if j is not None and 0 <= j < len(djvu) and djvu[j]["w"]:
                    fused = fusion_numeros(djvu[j], tess)
                    pages[i] = _con_cabecera(fused, tess)
                else:
                    pages[i] = tess
            else:
                base = djvu[i] if i < len(djvu) else tess
                fused = fusion_numeros(base, tess)
                pages[i] = _con_cabecera(fused, tess)
            n += 1
        if n:
            print(f"  reocr: {n} páginas de tesseract", flush=True)
    return pages


if __name__ == "__main__":
    p = load()
    print(f"{len(p)} páginas, {sum(len(x['words']) for x in p)} palabras")
    for i, pg in enumerate(p):
        if pg["words"]:
            print(f"  primera con texto: {i}  {pg['w']}x{pg['h']}  "
                  f"{len(pg['words'])} palabras")
            break
