"""
Da la mejor versión disponible de cada pliego.

Si existe un rehecho en reocr/ -- pasado por tesseract con el modelo
español sobre la imagen original -- se usa ese; si no, el de archive.org.
La estructura es la misma en los dos casos, así que el resto del pipeline
no se entera.
"""
import os
import hocr

DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "reocr")

def rehechas():
    if not os.path.isdir(DIR):
        return set()
    out = set()
    for f in os.listdir(DIR):
        if f.endswith(".hocr"):
            t, resto = f[:-5].split("_", 1)
            out.add((t, int(resto)))
    return out

_cache = {}

def pagina(tomo, idx, originales):
    if (tomo, idx) in rehechas():
        k = (tomo, idx)
        if k not in _cache:
            _cache[k] = hocr.carga(os.path.join(DIR, f"{tomo}_{idx}.hocr"))
        return _cache[k]
    return originales[idx]
