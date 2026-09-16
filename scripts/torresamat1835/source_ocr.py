"""
Lectura del OCR estructurado del testigo (DjVu XML de Internet Archive).

Se lee en streaming, página a página: el XML del tomo 3 son 29 MB y 652
páginas, y no hace falta tenerlas todas en memoria a la vez.

Aquí no se interpreta nada: sólo se entrega lo que el OCR dice, con sus
coordenadas intactas. Quién es versículo, título o nota lo deciden capas
de más arriba (layout.py y parser.py), que es donde se puede auditar.

Coordenadas DjVu: coords="x0,y1,x1,y0", con el eje y creciendo hacia
abajo. Se normalizan a (x0, y0, x1, y1) con y0 < y1.
"""
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from typing import Iterator, List, Optional, Tuple

BBox = Tuple[int, int, int, int]


@dataclass(frozen=True)
class SourceWord:
    text: str
    bbox: BBox
    confidence: int


@dataclass
class SourceLine:
    """Un renglón tal y como lo dio el OCR, con su caja."""
    index: int
    words: List[SourceWord]
    bbox: BBox

    @property
    def raw_text(self) -> str:
        """El texto crudo del OCR. No se corrige nunca."""
        return " ".join(w.text for w in self.words if w.text).strip()

    @property
    def height(self) -> int:
        return self.bbox[3] - self.bbox[1]

    @property
    def width(self) -> int:
        return self.bbox[2] - self.bbox[0]

    @property
    def confidence(self) -> int:
        if not self.words:
            return 0
        return sum(w.confidence for w in self.words) // len(self.words)


@dataclass
class SourcePage:
    scan_page: int
    width: int
    height: int
    dpi: Optional[int]
    lines: List[SourceLine] = field(default_factory=list)


def _bbox_of(words) -> BBox:
    return (min(w.bbox[0] for w in words), min(w.bbox[1] for w in words),
            max(w.bbox[2] for w in words), max(w.bbox[3] for w in words))


def _parse_coords(raw: str) -> Optional[BBox]:
    parts = raw.split(",")
    if len(parts) < 4:
        return None
    try:
        x0, y1, x1, y0 = (int(v) for v in parts[:4])
    except ValueError:
        return None
    if y0 > y1:
        y0, y1 = y1, y0
    if x0 > x1:
        x0, x1 = x1, x0
    return (x0, y0, x1, y1)


def read_pages(path: str, *, first: int = 0,
               limit: Optional[int] = None) -> Iterator[SourcePage]:
    """Páginas del OCR, una a una. Memoria acotada por página."""
    page = None
    number = -1
    yielded = 0
    for event, element in ET.iterparse(path, events=("start", "end")):
        if event == "start" and element.tag == "OBJECT":
            number += 1
            dpi = None
            page = SourcePage(scan_page=number,
                              width=int(element.get("width") or 0),
                              height=int(element.get("height") or 0),
                              dpi=dpi)
        elif event == "end" and element.tag == "PARAM" and page is not None:
            if element.get("name") == "DPI":
                try:
                    page.dpi = int(element.get("value"))
                except (TypeError, ValueError):
                    pass
        elif event == "end" and element.tag == "LINE" and page is not None:
            words = []
            for node in element.findall("WORD"):
                bbox = _parse_coords(node.get("coords", ""))
                text = (node.text or "").strip()
                if bbox is None or not text:
                    continue
                try:
                    confidence = int(node.get("x-confidence", 0))
                except ValueError:
                    confidence = 0
                words.append(SourceWord(text, bbox, confidence))
            if words:
                page.lines.append(SourceLine(len(page.lines), words,
                                             _bbox_of(words)))
            element.clear()
        elif event == "end" and element.tag == "OBJECT":
            done, page = page, None
            element.clear()
            if done.scan_page < first:
                continue
            yield done
            yielded += 1
            if limit is not None and yielded >= limit:
                return


def pages_from_fixture(data) -> Iterator[SourcePage]:
    """Páginas a partir de un fixture JSON con la misma geometría.

    Permite probar la capa geométrica sin llevar al repositorio el OCR de
    un testigo que no se puede redistribuir.
    """
    for raw in data["pages"]:
        page = SourcePage(scan_page=raw["scan_page"], width=raw["width"],
                          height=raw["height"], dpi=raw.get("dpi"))
        for index, item in enumerate(raw["lines"]):
            x0, y0, x1, y1 = item["bbox"]
            word = SourceWord(item["text"], (x0, y0, x1, y1),
                              int(item.get("confidence", 0)))
            page.lines.append(SourceLine(index, [word], (x0, y0, x1, y1)))
        yield page
