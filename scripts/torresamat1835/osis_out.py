"""
OSIS de la edición 1832-1835 a partir del modelo neutral.

Sólo sale texto bíblico. El paratexto -- la marca de división y el
argumento del editor -- se conserva en el modelo con su procedencia, pero
no entra en ningún versículo: es exactamente lo que contaminó 158
versículos de la edición de 1882.

La inscripción del salmo va marcada <seg type="x-psalm-title"> dentro de
su versículo nativo, no <title type="psalm"> y no en un versículo 0. Es
la representación que fijó e223d8d5: SWORD saca <title> como <h3>, el
versículo se leería como ausente y el visor lo supliría con otra Biblia.
"""
import html
from model import BlockKind

TITLE_SEG_TYPE = "x-psalm-title"


def verse_content(verse) -> str:
    """Contenido OSIS de un versículo: inscripción marcada y cuerpo."""
    parts = []
    for block in verse.blocks:
        if not block.is_canonical:
            continue
        text = html.escape(block.text.strip(), quote=False)
        if not text:
            continue
        if block.kind is BlockKind.CANONICAL_TITLE:
            parts.append(f'<seg type="{TITLE_SEG_TYPE}">{text}</seg>')
        else:
            parts.append(text)
    return " ".join(parts)


def generate(edition, *, work_id=None) -> str:
    """OSIS determinista: mismo modelo, mismos bytes."""
    work = work_id or edition.edition_id
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<osis xmlns="http://www.bibletechnologies.net/2003/OSIS/namespace">',
        f'  <osisText osisIDWork="{work}" osisRefWork="bible" xml:lang="es">',
        '    <header>',
        f'      <work osisWork="{work}">',
        '        <title>La Sagrada Biblia — Torres Amat (1835)</title>',
        '        <identifier type="OSIS">Bible.es.TorresAmat1835</identifier>',
        f'        <refSystem>Bible.{edition.versification}</refSystem>',
        '      </work>',
        '    </header>',
    ]
    for osis_id in sorted(edition.books):
        book = edition.books[osis_id]
        out.append(f'    <div type="book" osisID="{osis_id}">')
        for chapter_no in sorted(book.chapters):
            chapter = book.chapters[chapter_no]
            out.append(f'      <chapter osisID="{osis_id}.{chapter_no}">')
            for verse_no in sorted(chapter.verses):
                content = verse_content(chapter.verses[verse_no])
                if not content:
                    continue
                ref = f"{osis_id}.{chapter_no}.{verse_no}"
                out.append(f'        <verse osisID="{ref}">{content}</verse>')
            out.append('      </chapter>')
        out.append('    </div>')
    out += ['  </osisText>', '</osis>', '']
    return "\n".join(out)
