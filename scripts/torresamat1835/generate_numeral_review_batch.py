"""
Prepara una tanda de recortes para revisar numerales contra el facsímil.

    python3 generate_numeral_review_batch.py --audit <volume3.json> \
        --pdf <witness.pdf> --priority 1 --limit 20

Todo lo que produce vive en build/torresamat1835-review/, que no va al
repositorio: los recortes son del escaneo de un tercero y además se
regeneran solos. Lo que sí hay que poder repetir es CÓMO se hizo cada
uno, y para eso el manifiesto local guarda el sha256 del PDF, la plana,
la caja y los parámetros del recorte. Con eso, el mismo recorte sale
igual en cualquier máquina.

El recorte se ensancha a propósito respecto de la caja que dio el
reconocimiento. Cuando el OCR parte un numeral («SALMO C X I X.» en vez
de «SALMO CXIX»), la caja que registró puede no cubrir el numeral
entero, y recortar justo por ella escondería precisamente lo que se va a
mirar. El ensanchado queda anotado en el manifiesto.

Esto NO lee nada: sólo prepara imágenes para que las lea una persona.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import numeral_review

#: Márgenes del recorte, en píxeles de la plana escaneada. Generosos a la
#: izquierda y a la derecha porque el numeral puede haberse salido de la
#: caja; generosos abajo para que se vea el argumento del editor, que es
#: lo que confirma de qué capítulo se trata.
PAD_X = 260
PAD_ABOVE = 140
PAD_BELOW = 420

#: Anclado a la raíz del repositorio, no al directorio de trabajo: si no,
#: lanzarlo desde scripts/ creaba un build/ anidado ahí dentro.
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "build", "torresamat1835-review")


def sha256_of(path, chunk=1 << 20):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(chunk), b""):
            digest.update(block)
    return digest.hexdigest()


def render_page(pdf, pdf_page, out_dir, dpi):
    stem = os.path.join(out_dir, f"page{pdf_page:04d}")
    produced = f"{stem}-{pdf_page}.png"
    if not os.path.isfile(produced):
        subprocess.run(["pdftoppm", "-f", str(pdf_page), "-l", str(pdf_page),
                        "-r", str(dpi), "-png", pdf, stem], check=True)
    return produced


def crop(page_png, bbox, out_path, *, scan_width, scan_height, scale=4):
    from PIL import Image
    image = Image.open(page_png)
    sx = image.size[0] / float(scan_width)
    sy = image.size[1] / float(scan_height)
    x0, y0, x1, y1 = bbox
    box = (max(0, int((x0 - PAD_X) * sx)),
           max(0, int((y0 - PAD_ABOVE) * sy)),
           min(image.size[0], int((x1 + PAD_X) * sx)),
           min(image.size[1], int((y1 + PAD_BELOW) * sy)))
    piece = image.crop(box)
    piece = piece.resize((piece.size[0] * scale, piece.size[1] * scale),
                         Image.LANCZOS)
    piece.save(out_path)
    return box, piece.size


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--audit", required=True)
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--priority", type=int, action="append",
                    help="sólo esta prioridad; repetible")
    ap.add_argument("--book", action="append")
    ap.add_argument("--page", type=int, action="append",
                    help="sólo estas planas del escaneo; repetible")
    ap.add_argument("--limit", type=int, default=20)
    ap.add_argument("--dpi", type=int, default=200)
    ap.add_argument("--out", default=OUT)
    args = ap.parse_args()

    with open(args.audit, encoding="utf-8") as handle:
        report = json.load(handle)
    claims = report["chapter_claims"]
    queue = numeral_review.build(
        claims["claims"],
        permissive_groups=claims.get("permissive_value_collision_detail"),
        competing_groups=claims.get("competing_claim_groups_detail"))

    if args.priority:
        queue = [e for e in queue if e.priority in set(args.priority)]
    if args.book:
        queue = [e for e in queue if e.book in set(args.book)]
    if args.page:
        queue = [e for e in queue if e.scan_page in set(args.page)]
    queue = queue[:args.limit]

    os.makedirs(args.out, exist_ok=True)
    digest = sha256_of(args.pdf)
    entries = []
    for entry in queue:
        if not entry.bbox:
            continue
        page_png = render_page(args.pdf, entry.pdf_page, args.out, args.dpi)
        name = f"{entry.book}_p{entry.scan_page:04d}_{entry.block_id}.png"
        path = os.path.join(args.out, name)
        box, size = crop(page_png, entry.bbox, path,
                         scan_width=3402, scan_height=4837)
        record = entry.as_dict()
        record.update({
            "crop_file": name, "crop_box_in_render": list(box),
            "crop_size": list(size), "render_dpi": args.dpi,
            "pad_x": PAD_X, "pad_above": PAD_ABOVE, "pad_below": PAD_BELOW,
            "source_sha256": digest,
            "source_filename": os.path.basename(args.pdf),
        })
        entries.append(record)
        print(f"  P{entry.priority} {entry.book:5} scan={entry.scan_page:4} "
              f"{entry.block_id:14} raw={entry.raw_heading[:34]!r} -> {name}")

    manifest = os.path.join(args.out, "batch.json")
    with open(manifest, "w", encoding="utf-8") as handle:
        json.dump({"source_sha256": digest,
                   "source_filename": os.path.basename(args.pdf),
                   "page_mapping": "pdf_page = scan_page + 1",
                   "note": "local only; crops are not versioned",
                   "entries": entries}, handle, ensure_ascii=False, indent=1)
    print(f"\n{len(entries)} recortes en {args.out}; manifiesto {manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
