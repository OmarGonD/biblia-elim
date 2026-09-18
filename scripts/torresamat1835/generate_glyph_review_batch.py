"""
Prepara los recortes para validar formas OCR contra el facsímil.

    python3 generate_glyph_review_batch.py --inventory <inventory.json> \
        --pdf <witness.pdf> --form 'I o' --sheet 12

Lo que se mira en esta tanda no es un rótulo de capítulo sino la CABEZA
de un renglón: el sitio donde el impreso pone el número del versículo y
el reconocimiento dejó otra cosa. Por eso el recorte se ensancha mucho
hacia la izquierda --el numeral suele quedarse FUERA de la caja que el
OCR registró, que es justamente por lo que no se leyó-- y poco hacia
abajo: lo que hace falta ver es la cifra y el principio de su renglón,
no media plana.

Cada recorte lleva encima una franja con su índice y su identificador de
bloque, y los recortes se pegan en pliegos. Sin la franja no se puede
decir después a qué renglón correspondía cada imagen, y una revisión que
no se puede volver a localizar no es evidencia de nada.

Esto NO lee nada ni decide nada: sólo prepara imágenes. La lectura se
anota a mano en `verse_boundary_reviews.json`, y es la plana la que
establece la correspondencia, nunca este programa.
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import generate_numeral_review_batch as batch
import source_ocr

#: El numeral vive a la izquierda de la caja del renglón --cuando el OCR
#: no lo metió dentro-- así que ahí el margen es ancho, y entra además el
#: blanco del margen, que es lo que deja ver la SANGRÍA: en esta edición
#: el renglón que abre versículo entra un poco y el que continúa no, y
#: esa diferencia decide tanto como la forma de la cifra.
#:
#: A la derecha no hace falta el renglón entero: con el número y las
#: primeras palabras se ve si lo que sigue empieza frase o va a media.
#: Recortar ahí es lo que permite subir la resolución del numeral, que
#: es lo único que de verdad se está mirando.
PAD_LEFT = 380
WINDOW_RIGHT = 820
PAD_ABOVE = 130
PAD_BELOW = 130

#: La franja del rótulo se dibuja pequeña y se amplía después. La fuente
#: de mapa de bits que trae PIL por defecto no se puede pedir en otro
#: cuerpo, y un rótulo que no se lee es peor que no ponerlo: si no se
#: distingue «form='t'» de «form='I'» la revisión se anota en la forma
#: equivocada y el inventario entero queda envenenado.
CAPTION_H = 60
CAPTION_ZOOM = 3


def page_geometry(xml_path, pages):
    """(ancho, alto) del escaneo por plana. Sin esto no se escala nada."""
    want, out = set(pages), {}
    for page in source_ocr.read_pages(xml_path):
        if page.scan_page in want:
            out[page.scan_page] = (page.width, page.height)
            want.discard(page.scan_page)
            if not want:
                break
    return out


def crop_one(page_png, bbox, *, scan_width, scan_height, scale, caption):
    from PIL import Image, ImageDraw
    image = Image.open(page_png)
    sx = image.size[0] / float(scan_width)
    sy = image.size[1] / float(scan_height)
    x0, y0, _x1, y1 = bbox
    box = (max(0, int((x0 - PAD_LEFT) * sx)),
           max(0, int((y0 - PAD_ABOVE) * sy)),
           min(image.size[0], int((x0 + WINDOW_RIGHT) * sx)),
           min(image.size[1], int((y1 + PAD_BELOW) * sy)))
    piece = image.crop(box).convert("RGB")
    if scale != 1:
        piece = piece.resize((piece.size[0] * scale, piece.size[1] * scale),
                             Image.LANCZOS)
    strip = Image.new("RGB", (max(1, piece.size[0] // CAPTION_ZOOM),
                              CAPTION_H // CAPTION_ZOOM), "#101010")
    ImageDraw.Draw(strip).text((4, 3), caption, fill="#ffffff")
    strip = strip.resize((piece.size[0], CAPTION_H), Image.NEAREST)
    canvas = Image.new("RGB", (piece.size[0], piece.size[1] + CAPTION_H),
                       "white")
    canvas.paste(strip, (0, 0))
    canvas.paste(piece, (0, CAPTION_H))
    ImageDraw.Draw(canvas).line(
        [0, CAPTION_H - 1, canvas.size[0], CAPTION_H - 1],
        fill="#101010", width=3)
    return canvas, box


def sheet(images, out_path):
    """Pega los recortes en un pliego vertical."""
    from PIL import Image
    width = max(img.size[0] for img in images)
    height = sum(img.size[1] for img in images) + 8 * len(images)
    canvas = Image.new("RGB", (width, height), "#8a8a8a")
    y = 0
    for img in images:
        canvas.paste(img, (0, y))
        y += img.size[1] + 8
    canvas.save(out_path)
    return canvas.size


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--inventory", required=True)
    ap.add_argument("--pdf", required=True)
    ap.add_argument("--xml", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--blocks", help="fichero con un block_id por línea")
    ap.add_argument("--form", action="append", help="sólo esta forma; repetible")
    ap.add_argument("--dpi", type=int, default=300)
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--sheet", type=int, default=10)
    args = ap.parse_args()

    data = json.load(open(args.inventory, encoding="utf-8"))
    by_id = {row["block_id"]: row for row in data["instances"]}
    if args.blocks:
        wanted = [line.strip() for line in open(args.blocks, encoding="utf-8")
                  if line.strip()]
    elif args.form:
        forms = set(args.form)
        wanted = [row["block_id"] for row in data["instances"]
                  if row["glyph_form"] in forms]
    else:
        wanted = list(data["sample"])
    rows = [by_id[b] for b in wanted if b in by_id]
    rows.sort(key=lambda r: r["block_id"])

    digest = batch.sha256_of(args.pdf)
    os.makedirs(args.out, exist_ok=True)
    pages = page_geometry(args.xml, {r["scan_page"] for r in rows})

    manifest, images, sheets, index = [], [], [], 0
    for row in rows:
        scan = row["scan_page"]
        geometry = pages.get(scan)
        if geometry is None or not row["bbox"]:
            continue
        png = batch.render_page(args.pdf, row["pdf_page"], args.out, args.dpi)
        # Corto a propósito: el rótulo tiene que caber en la franja a
        # cuerpo legible. El crudo completo va en el manifiesto, que es
        # donde se consulta, no en la imagen.
        caption = (f"[{index:03d}] {row['block_id']} pdf{row['pdf_page']} "
                   f"form={row['glyph_form']!r}")
        image, box = crop_one(png, row["bbox"], scan_width=geometry[0],
                              scan_height=geometry[1], scale=args.scale,
                              caption=caption)
        images.append(image)
        manifest.append({
            "index": index, "block_id": row["block_id"],
            "glyph_form": row["glyph_form"], "scan_page": scan,
            "pdf_page": row["pdf_page"], "bbox": row["bbox"],
            "crop_box_in_render": list(box), "render_dpi": args.dpi,
            "scale": args.scale, "raw": row["raw"],
            "expected_values_diagnostic_only":
                row["expected_values_diagnostic_only"],
            "sheet": len(sheets),
        })
        index += 1
        if len(images) == args.sheet:
            name = f"sheet{len(sheets):03d}.png"
            sheet(images, os.path.join(args.out, name))
            sheets.append(name)
            images = []
    if images:
        name = f"sheet{len(sheets):03d}.png"
        sheet(images, os.path.join(args.out, name))
        sheets.append(name)

    with open(os.path.join(args.out, "manifest.json"), "w",
              encoding="utf-8") as handle:
        json.dump({"pdf_sha256": digest, "page_mapping": "pdf_page = scan_page + 1",
                   "pad": {"left": PAD_LEFT, "window_right": WINDOW_RIGHT,
                           "above": PAD_ABOVE, "below": PAD_BELOW},
                   "sheets": sheets, "crops": manifest},
                  handle, ensure_ascii=False, indent=1)
    print(f"crops={len(manifest)} sheets={len(sheets)} out={args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
