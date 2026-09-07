"""Adapta el hOCR de tesseract a la estructura de load.py."""
import re
import xml.etree.ElementTree as ET

RE_BBOX = re.compile(r"bbox (\d+) (\d+) (\d+) (\d+)")
RE_CONF = re.compile(r"x_wconf (\d+)")


def carga(path, dx=0):
    """{w, h, words}; dx se suma a X (columna derecha)."""
    with open(path, encoding="utf-8") as f:
        txt = f.read()
    txt = re.sub(r"<!DOCTYPE[^>]*>", "", txt, count=1)
    txt = txt.replace("&nbsp;", " ")
    raiz = ET.fromstring(txt)
    W = H = 0
    for pg in raiz.iter("{http://www.w3.org/1999/xhtml}div"):
        if "ocr_page" in (pg.get("class") or ""):
            m = RE_BBOX.search(pg.get("title") or "")
            if m:
                W, H = int(m.group(3)), int(m.group(4))
            break
    palabras = []
    for w in raiz.iter("{http://www.w3.org/1999/xhtml}span"):
        if "ocrx_word" not in (w.get("class") or ""):
            continue
        t = "".join(w.itertext()).strip()
        if not t:
            continue
        title = w.get("title") or ""
        mb = RE_BBOX.search(title)
        if not mb:
            continue
        x1, y1, x2, y2 = (int(mb.group(i)) for i in range(1, 5))
        mc = RE_CONF.search(title)
        conf = int(mc.group(1)) if mc else 0
        palabras.append((x1 + dx, x2 + dx, min(y1, y2), max(y1, y2), conf, t))
    return {"w": W, "h": H, "words": palabras}
