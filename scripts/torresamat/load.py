"""Carga los djvu.xml a una estructura compacta y la deja en caché."""
import xml.etree.ElementTree as ET, pickle, os, sys

TOMOS = ["I", "II", "III", "IV"]

def parse(path):
    pages = []; cur = None
    for ev, el in ET.iterparse(path, events=("start", "end")):
        if ev == "start" and el.tag == "OBJECT":
            cur = {"w": int(el.get("width", 0)), "h": int(el.get("height", 0)), "words": []}
        elif ev == "end" and el.tag == "WORD":
            c = el.get("coords", "")
            try:
                x1, y2, x2, y1 = [int(v) for v in c.split(",")[:4]]
            except ValueError:
                el.clear(); continue
            t = (el.text or "").strip()
            if t:
                cur["words"].append((x1, x2, min(y1, y2), max(y1, y2),
                                     int(el.get("x-confidence", 0)), t))
            el.clear()
        elif ev == "end" and el.tag == "OBJECT":
            pages.append(cur); el.clear()
    return pages

def load(tomo):
    cache = f"cache_{tomo}.pkl"
    if os.path.exists(cache):
        with open(cache, "rb") as f: return pickle.load(f)
    pages = parse(f"tomo{tomo}.djvu.xml")
    with open(cache, "wb") as f: pickle.dump(pages, f, -1)
    return pages

# campos de cada palabra
X1, X2, TOP, BOT, CONF, TXT = range(6)

if __name__ == "__main__":
    for t in TOMOS:
        p = load(t)
        print(f"tomo {t}: {len(p)} páginas, {sum(len(x['words']) for x in p)} palabras")
