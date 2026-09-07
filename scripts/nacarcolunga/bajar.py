"""Baja de Internet Archive el djvu.xml y el scandata de la BAC 1944."""
import os
from urllib.parse import quote
from urllib.request import urlretrieve

HERE = os.path.dirname(os.path.abspath(__file__))
DIR = os.path.join(HERE, "fuentes")
TESSDATA = os.path.join(HERE, "tessdata")
BASE = "https://archive.org/download/SagradaBibliaNacarColunga19441Edicin/"
STEM = "Sagrada Biblia Nacar-Colunga (1944) (1ª Edición)"
SPA = "https://github.com/tesseract-ocr/tessdata/raw/main/spa.traineddata"

FICHEROS = {
    "djvu.xml": STEM + "_djvu.xml",
    "scandata.xml": STEM + "_scandata.xml",
}


def main():
    os.makedirs(DIR, exist_ok=True)
    os.makedirs(TESSDATA, exist_ok=True)
    spa = os.path.join(TESSDATA, "spa.traineddata")
    if not os.path.exists(spa) or os.path.getsize(spa) < 1000:
        print("bajando spa.traineddata…")
        urlretrieve(SPA, spa)
        print(f"  {os.path.getsize(spa)} bytes")
    cfg_src = "/usr/share/tessdata/configs"
    cfg_dst = os.path.join(TESSDATA, "configs")
    if os.path.isdir(cfg_src) and not os.path.exists(os.path.join(cfg_dst, "hocr")):
        import shutil
        shutil.copytree(cfg_src, cfg_dst, dirs_exist_ok=True)
    for dest, name in FICHEROS.items():
        path = os.path.join(DIR, dest)
        if os.path.exists(path) and os.path.getsize(path) > 1000:
            print(f"ya está {dest} ({os.path.getsize(path)} bytes)")
            continue
        url = BASE + quote(name)
        print(f"bajando {dest}…")
        urlretrieve(url, path)
        print(f"  {os.path.getsize(path)} bytes")


if __name__ == "__main__":
    main()
