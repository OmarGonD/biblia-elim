"""
Modelo intermedio neutral de la edición Torres Amat 1832-1835.

Se construye esto antes que ningún OSIS. El defecto que hundió la edición
de 1882 no fue de OCR: fue que el importador sólo tenía dos categorías,
«encabezado que sé numerar» y «texto», así que todo lo que no supo numerar
acabó siendo cuerpo del versículo en curso. 158 argumentos del editor
entraron así en el texto bíblico.

Aquí los conceptos van separados desde el principio, y hay una categoría
explícita para lo que no se ha podido clasificar. Nada se convierte en
texto bíblico por descarte.
"""
from dataclasses import dataclass, field
from enum import Enum
from typing import Optional


class BlockKind(Enum):
    #: Texto bíblico numerado.
    VERSE = "Verse"
    #: Inscripción del salmo que la Vulgata cuenta como versículo.
    CANONICAL_TITLE = "CanonicalTitle"
    #: «SALMO LIII» / «CAPITULO XXIX»: la marca de división del impreso.
    CHAPTER_HEADING = "ChapterHeading"
    #: El argumento del editor bajo esa marca. NO es texto bíblico.
    EDITORIAL_HEADING = "EditorialHeading"
    FOOTNOTE = "Footnote"
    MARGINAL_NOTE = "MarginalNote"
    PAGE_HEADER = "PageHeader"
    PAGE_FOOTER = "PageFooter"
    APPARATUS = "Apparatus"
    PARAGRAPH = "Paragraph"
    #: No se ha podido clasificar con confianza. Queda para revisión y
    #: nunca se une a nada.
    UNCLASSIFIED = "UnclassifiedBlock"


#: Lo único que el exportador canónico puede emitir como texto bíblico.
CANONICAL_KINDS = frozenset({BlockKind.VERSE, BlockKind.CANONICAL_TITLE})

#: Paratexto: se conserva con su procedencia, fuera del cuerpo.
PARATEXT_KINDS = frozenset({
    BlockKind.CHAPTER_HEADING, BlockKind.EDITORIAL_HEADING,
    BlockKind.FOOTNOTE, BlockKind.MARGINAL_NOTE,
    BlockKind.PAGE_HEADER, BlockKind.PAGE_FOOTER, BlockKind.APPARATUS,
})


@dataclass(frozen=True)
class Provenance:
    """De dónde sale cada bloque. Sin esto no se puede auditar nada."""
    witness: str
    volume: Optional[str] = None
    page: Optional[int] = None
    line: Optional[int] = None
    column: Optional[str] = None


@dataclass
class Block:
    kind: BlockKind
    text: str
    provenance: Provenance
    #: Número de capítulo o de versículo cuando se conoce. None cuando el
    #: bloque se ha reconocido pero su número no: eso NO lo degrada a
    #: texto.
    number: Optional[int] = None
    #: Motivo por el que hace falta que lo mire una persona.
    review_reason: Optional[str] = None

    @property
    def review_required(self) -> bool:
        return self.review_reason is not None

    @property
    def is_canonical(self) -> bool:
        return self.kind in CANONICAL_KINDS


@dataclass
class Verse:
    number: int
    blocks: list = field(default_factory=list)

    @property
    def body(self) -> str:
        """Sólo texto bíblico. Un paratexto no puede entrar aquí."""
        return " ".join(b.text for b in self.blocks if b.is_canonical).strip()

    @property
    def has_canonical_title(self) -> bool:
        return any(b.kind is BlockKind.CANONICAL_TITLE for b in self.blocks)


@dataclass
class Chapter:
    number: int
    verses: dict = field(default_factory=dict)
    paratext: list = field(default_factory=list)

    def verse(self, n: int) -> Verse:
        return self.verses.setdefault(n, Verse(number=n))


@dataclass
class Book:
    osis_id: str
    chapters: dict = field(default_factory=dict)

    def chapter(self, n: int) -> Chapter:
        return self.chapters.setdefault(n, Chapter(number=n))


@dataclass
class Edition:
    edition_id: str
    versification: str = "Vulg"
    books: dict = field(default_factory=dict)
    #: Bloques que nadie ha sabido clasificar, con su procedencia.
    review_queue: list = field(default_factory=list)

    def book(self, osis_id: str) -> Book:
        return self.books.setdefault(osis_id, Book(osis_id=osis_id))
