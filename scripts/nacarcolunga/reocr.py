"""
Vuelve a pasar el OCR: baja el jp2, parte las dos columnas y las lee
con tesseract en español.

El djvu.xml de Archive sale de ABBYY en página entera y mezcla
columnas; los versos quedan a medias («fué mentos»). Tesseract sobre
cada columna por separado lee el castellano de 1944.
"""
import os
import pickle
import re
import subprocess
import sys
import tempfile
from urllib.parse import quote
from urllib.request import urlretrieve

from hocr import carga

DIR = os.path.dirname(os.path.abspath(__file__))
REOCR = os.path.join(DIR, "reocr")
TESSDATA = os.path.join(DIR, "tessdata")
STEM = "Sagrada Biblia Nacar-Colunga (1944) (1ª Edición)"
BASE = "https://archive.org/download/SagradaBibliaNacarColunga19441Edicin/"
N_PAGINAS = 1512
N_PRINCETON = 1516

# Facsímil de Princeton (Canon 5D, misma ed. 1944). El barrido
# completo usa el número de hoja como índice (leaf 1474 = p. 1372).
PRINCETON_BASE = "https://archive.org/download/sagradabibliaver00naca/"
PRINCETON_DIR = os.path.join(DIR, "fuentes", "princeton")
PRINCETON_ZIP = os.path.join(DIR, "fuentes", "princeton_jp2.zip")
PRINCETON_LEAF = {
    1469: 1472,  # legado: 1 Juan en el índice del PDF
    1470: 1473,
    1471: 1474,
    1472: 1475,
    1473: 1476,
}


def url_jp2(idx):
    z = STEM + "_jp2.zip"
    inner = f"{STEM}_jp2/{STEM}_{idx:04d}.jp2"
    return BASE + quote(z) + "/" + quote(inner)


def url_jp2_princeton(leaf):
    z = "sagradabibliaver00naca_jp2.zip"
    inner = f"sagradabibliaver00naca_jp2/sagradabibliaver00naca_{leaf:04d}.jp2"
    return PRINCETON_BASE + quote(z) + "/" + quote(inner)


def tesseract(png, hocr_base, dpi=600, psm=4, extra=None):
    # tessdata/spa.traineddata es tessdata_best, no el modelo entero.
    env = os.environ.copy()
    env["TESSDATA_PREFIX"] = TESSDATA
    env["OMP_THREAD_LIMIT"] = "1"
    cmd = ["tesseract", png, hocr_base, "-l", "spa", "--psm", str(psm),
           "--dpi", str(dpi), "hocr"]
    if extra:
        cmd.extend(extra)
    subprocess.run(
        cmd, check=True, env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def preprocesa_lectura(origen, destino):
    """Variante robusta para tinta tenue, inclinación y compresión JPEG."""
    subprocess.run([
        "magick", origen, "-colorspace", "Gray", "-deskew", "40%",
        "-contrast-stretch", "0x2%", "-threshold", "68%", destino,
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def solapa(a, b):
    ax1, ax2, ay1, ay2 = a[:4]
    bx1, bx2, by1, by2 = b[:4]
    inter = max(0, min(ax2, bx2) - max(ax1, bx1)) * max(0, min(ay2, by2) - max(ay1, by1))
    area = max(1, (ax2 - ax1) * (ay2 - ay1) + (bx2 - bx1) * (by2 - by1) - inter)
    return inter / area


def consenso(base, alternativa):
    """Conserva orden de PSM 4 y solo reemplaza cajas coincidentes mejores.

    PSM 4 lee la estructura de columna; PSM 6 sobre imagen limpiada suele
    rescatar caracteres. No se añaden palabras aisladas de la segunda
    pasada: podrían ser ruido o una nota al pie.
    """
    palabras = []
    for w in base["words"]:
        candidatas = [x for x in alternativa["words"] if solapa(w, x) >= .45]
        mejor = max(candidatas, key=lambda x: x[4], default=w)
        palabras.append(mejor if mejor[4] >= w[4] + 8 else w)
    return {"w": base["w"], "h": base["h"], "words": palabras}


def tesseract_txt(png, dpi=350, psm=6):
    env = os.environ.copy()
    env["TESSDATA_PREFIX"] = TESSDATA
    env["OMP_THREAD_LIMIT"] = "1"
    r = subprocess.run(
        ["tesseract", png, "stdout", "-l", "spa", "--psm", str(psm),
         "--dpi", str(dpi)],
        check=False, env=env, capture_output=True, text=True)
    return re.sub(r"\s+", " ", (r.stdout or "")).strip()


def vips(*args):
    subprocess.run(["vips", *args], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def medianil(png, W, H):
    """Eje del canal entre las dos columnas, por tinta.

    Un 48 % fijo se come 1-3 letras del final de la columna izquierda
    en los rectos (1 Juan 3, pág. 1471: el medianil está al 52 %) y
    recorta el arranque de la derecha en los versos. El facsímil de
    Archive viene de un PDF muy comprimido: si además cortamos la
    columna, Tesseract no puede reconstruir «Dios» / «porque».
    """
    fallback = W * 48 // 100
    gray = png + ".gray"
    try:
        subprocess.run(
            ["magick", png, "-colorspace", "Gray", "-depth", "8",
             f"gray:{gray}"],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        data = open(gray, "rb").read()
    except Exception:
        return fallback
    finally:
        if os.path.exists(gray):
            os.remove(gray)
    if len(data) < W * H:
        return fallback
    ink = [0] * W
    for y in range(int(H * 0.10), int(H * 0.72)):
        row = data[y * W:(y + 1) * W]
        for x, p in enumerate(row):
            if p < 150:
                ink[x] += 1
    k = 15
    lo, hi = int(W * 0.38), int(W * 0.62)
    mejor, mval = fallback, None
    for x in range(lo, hi):
        a, b = max(0, x - k), min(W, x + k + 1)
        v = sum(ink[a:b]) / (b - a)
        if mval is None or v < mval:
            mval, mejor = v, x
    return mejor


def cabecera_roja(png, W, H, tmp, dpi=350):
    """OCR de la cabecera corrida en tinta roja (franja superior).

    En esta edición el título del libro y los capítulos van en rojo
    encima de la raya; el número de página, en negro. Aislar el rojo
    evita mezclar el v. 1 del cuerpo («Ved qué amor…») con «I SAN JUAN».
    """
    hb = max(40, int(H * 0.048))
    head = os.path.join(tmp, "head.png")
    red = os.path.join(tmp, "red.png")
    vips("crop", png, head, "0", "0", str(W), str(hb))
    # Píxel rojo (tinta BAC) → negro; el resto, blanco.
    subprocess.run(
        ["magick", head, "-colorspace", "sRGB", "-fx",
         "(u.r - u.g) + (u.r - u.b) > 0.28 && u.g < 0.55 ? 0 : 1",
         red],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    roja = tesseract_txt(red, dpi=dpi, psm=6)
    gris = tesseract_txt(head, dpi=dpi, psm=7)
    return roja, gris


def tamano_previo(idx):
    dest = os.path.join(REOCR, f"{idx:04d}.pkl")
    if os.path.exists(dest) and os.path.getsize(dest) > 100:
        with open(dest, "rb") as f:
            d = pickle.load(f)
        w, h = d.get("w"), d.get("h")
        if w and h:
            return int(w), int(h)
    return None, None


def escala_pagina(page, tw, th):
    """Lleva cajas al tamaño del PDF de Archive para segment.py."""
    if not tw or not th or tw == page["w"]:
        return page
    sx, sy = tw / page["w"], th / page["h"]
    words = []
    for x1, x2, top, bot, conf, t in page["words"]:
        words.append((int(x1 * sx), int(x2 * sx),
                      int(top * sy), int(bot * sy), conf, t))
    canal = page.get("canal")
    if canal:
        canal = int(canal * sx)
    out = {"w": tw, "h": th, "canal": canal, "words": words,
           "src": page.get("src")}
    for k in ("cabecera_roja", "cabecera"):
        if page.get(k):
            out[k] = page[k]
    return out


def jp2_princeton(leaf):
    os.makedirs(PRINCETON_DIR, exist_ok=True)
    cache = os.path.join(PRINCETON_DIR, f"{leaf:04d}.jp2")
    if os.path.exists(cache) and os.path.getsize(cache) > 1000:
        return cache
    nested = os.path.join(
        PRINCETON_DIR, "sagradabibliaver00naca_jp2",
        f"sagradabibliaver00naca_{leaf:04d}.jp2")
    if os.path.exists(nested) and os.path.getsize(nested) > 1000:
        return nested
    urlretrieve(url_jp2_princeton(leaf), cache)
    if os.path.getsize(cache) < 1000:
        raise RuntimeError(f"jp2 Princeton vacío leaf {leaf}")
    return cache


def una(idx, force=False, princeton=False, princeton_full=False, ensemble=False):
    dest = os.path.join(REOCR, f"{idx:04d}.pkl")
    if (not force) and os.path.exists(dest) and os.path.getsize(dest) > 100:
        if not princeton:
            return dest
        with open(dest, "rb") as f:
            old = pickle.load(f)
        if (str(old.get("src") or "").startswith("princeton")
                and old.get("cabecera_roja") is not None):
            return dest
    os.makedirs(REOCR, exist_ok=True)
    prev_w, prev_h = (None, None) if princeton_full else tamano_previo(idx)
    with tempfile.TemporaryDirectory(prefix=f"nc{idx:04d}-") as tmp:
        png = os.path.join(tmp, "p.png")
        dpi = 600
        src = "archive-pdf"
        if princeton:
            leaf = idx if princeton_full else PRINCETON_LEAF.get(idx)
            if leaf is None:
                raise RuntimeError(f"sin hoja Princeton para índice {idx}")
            jp2 = jp2_princeton(leaf)
            dpi = 350
            src = f"princeton:{leaf}"
        else:
            jp2 = os.path.join(tmp, "p.jp2")
            urlretrieve(url_jp2(idx), jp2)
            if os.path.getsize(jp2) < 1000:
                raise RuntimeError(f"jp2 vacío {idx}")
        vips("copy", jp2, png)
        info = subprocess.check_output(
            ["identify", "-format", "%w %h", png], text=True)
        W, H = (int(x) for x in info.split())
        mid = medianil(png, W, H)
        izq, der = os.path.join(tmp, "L.png"), os.path.join(tmp, "R.png")
        vips("crop", png, izq, "0", "0", str(mid), str(H))
        vips("crop", png, der, str(mid), "0", str(W - mid), str(H))
        tesseract(izq, os.path.join(tmp, "L"), dpi=dpi)
        tesseract(der, os.path.join(tmp, "R"), dpi=dpi)
        a = carga(os.path.join(tmp, "L.hocr"), dx=0)
        b = carga(os.path.join(tmp, "R.hocr"), dx=mid)
        if ensemble:
            lizq, lder = os.path.join(tmp, "L-clean.png"), os.path.join(tmp, "R-clean.png")
            preprocesa_lectura(izq, lizq)
            preprocesa_lectura(der, lder)
            tesseract(lizq, os.path.join(tmp, "L6"), dpi=dpi, psm=6)
            tesseract(lder, os.path.join(tmp, "R6"), dpi=dpi, psm=6)
            a = consenso(a, carga(os.path.join(tmp, "L6.hocr"), dx=0))
            b = consenso(b, carga(os.path.join(tmp, "R6.hocr"), dx=mid))
        page = {"w": W, "h": H, "canal": mid, "src": src,
                "words": a["words"] + b["words"]}
        if princeton:
            roja, gris = cabecera_roja(png, W, H, tmp, dpi=dpi)
            page["cabecera_roja"] = roja
            page["cabecera"] = gris
        page = escala_pagina(page, prev_w, prev_h)
    with open(dest, "wb") as f:
        pickle.dump(page, f, -1)
    return dest


def main():
    args = sys.argv[1:]
    jobs = 3
    force = False
    princeton = False
    princeton_full = False
    ensemble = False
    out = []
    i = 0
    while i < len(args):
        if args[i] == "-j":
            jobs = int(args[i + 1])
            i += 2
            continue
        if args[i] in ("-f", "--force"):
            force = True
            i += 1
            continue
        if args[i] in ("--princeton",):
            princeton = True
            i += 1
            continue
        if args[i] == "--ensemble":
            ensemble = True
            i += 1
            continue
        out.append(args[i])
        i += 1
    args = out
    if princeton and not args:
        # Barrido completo: índice = hoja Princeton.
        princeton_full = True
        indices = list(range(N_PRINCETON))
        jobs = max(jobs, 4)
    elif princeton:
        indices = [int(a) for a in args]
        force = True
        princeton_full = True
    else:
        indices = [int(a) for a in args] if args else list(range(N_PAGINAS))
        if args:
            force = True
    from functools import partial
    from multiprocessing import Pool
    ok = fail = 0
    with Pool(jobs, maxtasksperchild=8) as pool:
        worker = partial(una_safe, force=force, princeton=princeton,
                         princeton_full=princeton_full, ensemble=ensemble)
        for i, r in enumerate(pool.imap_unordered(worker, indices), 1):
            if r:
                ok += 1
            else:
                fail += 1
            if i % 25 == 0 or i == len(indices):
                print(f"{i}/{len(indices)}  ok={ok} fail={fail}", flush=True)
    print(f"listo ok={ok} fail={fail}")
    if fail:
        sys.exit(1)


def una_safe(idx, force=False, princeton=False, princeton_full=False, ensemble=False):
    try:
        una(idx, force=force, princeton=princeton,
            princeton_full=princeton_full, ensemble=ensemble)
        return True
    except Exception as e:
        sys.stderr.write(f"fallo {idx}: {e}\n")
        return False


if __name__ == "__main__":
    main()
