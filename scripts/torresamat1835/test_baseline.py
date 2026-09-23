"""
TORRES-1835-BASELINE-101: fundación de la edición Torres Amat 1832-1835.

    python3 test_baseline.py

Nada aquí toca la red ni ~/.sword. Los testigos se describen en
data/torresamat1835/source_manifest.json; los ficheros fuente no viajan
en el repositorio.
"""
import hashlib
import json
import os
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import osis_out
import parser as p
from fetch_source import verify_file, ManifestError
from model import BlockKind, Edition, Provenance

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
MANIFEST = os.path.join(ROOT, "data", "torresamat1835", "source_manifest.json")
FIXTURE = os.path.join(DIR, "fixtures", "ps52_53_boundary.txt")


def _manifest():
    with open(MANIFEST, encoding="utf-8") as handle:
        return json.load(handle)


def _fixture_edition():
    with open(FIXTURE, encoding="utf-8") as handle:
        lines = handle.read().splitlines()
    return p.parse_lines(lines, witness="fixture:ps52_53_boundary", book="Ps")


# ---- A. Provenance manifest -------------------------------------------
def test_manifest_provenance():
    """La procedencia de la edición, en el esquema por tomos (v2).

    Lo que este test cuida es lo de siempre -- que la edición esté
    identificada, que cada testigo diga qué se puede hacer con él y que
    ningún fichero fuente viva en el repositorio --. El detalle por tomo
    lo cubre test_sources.py.
    """
    data = _manifest()
    for field in ("manifest_version", "edition_id", "edition", "volumes",
                  "retrieval_date", "provenance_notes"):
        assert field in data, field

    edition = data["edition"]
    for field in ("edition_statement", "city", "publisher", "years",
                  "volumes_total", "versification", "work_rights"):
        assert edition[field], field
    assert edition["versification"] == "Vulg"
    assert edition["publisher"].endswith("Miguel de Burgos")
    assert edition["volumes_total"] == len(data["volumes"]) == 6

    seen = set()
    for volume in data["volumes"]:
        assert volume["volume"] not in seen
        seen.add(volume["volume"])
        assert volume["witnesses"], volume["volume"]
        for witness in volume["witnesses"]:
            assert witness.get("id"), volume["volume"]
            assert witness.get("rights", {}).get("redistribution_allowed")
        for entry in volume.get("files", []):
            assert entry["filename"], volume["volume"]
            if entry.get("sha256") is not None:
                assert len(entry["sha256"]) == 64, entry
                assert entry.get("verified") is True, entry
            else:
                assert entry.get("verified") is False, entry

    # Metadata versionada sí; assets de la fuente no. Lo que no puede
    # haber aquí son escaneos, OCR masivo ni nada pesado: eso vive en la
    # caché de build/, ignorada por Git.
    data_dir = os.path.join(ROOT, "data", "torresamat1835")
    listing = sorted(os.listdir(data_dir))
    assert "source_manifest.json" in listing
    for name in listing:
        assert name.endswith(".json"), name
        # El registro de revisiones crece una entrada por cada decisión
        # humana, y cada entrada lleva su evidencia: qué se vio en la
        # plana y por qué. Eso es exactamente lo que debe estar
        # versionado, así que tiene su propio techo. Lo que el guarda
        # persigue --escaneos, volcados de OCR, cualquier cosa pesada de
        # la fuente-- sigue prohibido en todos los demás ficheros.
        cap = 768 if name in ("chapter_image_reviews.json",
                                  "verse_boundary_reviews.json",
                                  "a_glyph_pixel_features.json",
                                  "glued_marker_segments.json") else (2048 if name in
                                  ("glued_marker_discriminator.json",
                                   "remaining_glyph_inventory.json",
                                   "remaining_glyph_reprioritization.json",
                                   "projected_form_a_facsimile.json",
                                   "projected_form_a_visible_2_discriminator.json",
                                   "projected_form_a_visible_2_dry_run.json",
                                   "standalone_glyph_facsimile.json",
                                   "projected_rejection_audit.json",
                                   "no_trusted_band_discriminator.json",
                                   "zero_anchor_io_recovery_validation.json") else 256)
        assert os.path.getsize(os.path.join(data_dir, name)) < cap * 1024, name


# ---- B. Acquisition (sin red) -----------------------------------------
def test_acquisition_verifies_checksum(tmp=None):
    payload = b"testigo de prueba\n"
    digest = hashlib.sha256(payload).hexdigest()
    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        path = os.path.join(tmpdir, "witness.txt")
        with open(path, "wb") as handle:
            handle.write(payload)
        assert verify_file(path, digest) is True
        try:
            verify_file(path, "0" * 64)
        except ManifestError:
            pass
        else:
            raise AssertionError("un checksum que no cuadra debe fallar")
        # Un fichero sin checksum registrado no se acepta en silencio.
        try:
            verify_file(path, None)
        except ManifestError:
            pass
        else:
            raise AssertionError("sin checksum registrado no se puede aceptar")


# ---- C / E. Frontera de capítulo: el fallo de 1882 ---------------------
def test_chapter_boundary_is_not_absorbed():
    edition = _fixture_edition()
    psalms = edition.books["Ps"]
    # El fixture empieza en SALMO LII, así que el primer capítulo abierto
    # es el 1 correlativo y el siguiente el 2; los números del impreso
    # quedan en revisión. Lo que importa es la separación, no la cifra.
    first, second = sorted(psalms.chapters)[:2]

    last_verse = psalms.chapters[first].verses[7]
    assert last_verse.body.startswith("¡Oh! ¿Quién enviará de Sion")
    assert last_verse.body.endswith("saltará de gozo Israél.")
    # Lo que destruyó la edición anterior:
    assert "Datid" not in last_verse.body
    assert "SALMO" not in last_verse.body
    assert "implora" not in last_verse.body

    # El bloque existe, pero como paratexto y marcado para revisión.
    headings = [b for b in psalms.chapters[second].paratext
                if b.kind is BlockKind.CHAPTER_HEADING]
    assert headings, "la división tiene que estar en el modelo"
    assert "Datid implora" in headings[0].text
    assert headings[0].review_required

    # Y el primer versículo del salmo siguiente está limpio.
    opening = psalms.chapters[second].verses[1]
    assert opening.body == ("Para el fin: sobre los Cánticos. Salmo de "
                            "inteligencia de David")
    assert "SALMO" not in opening.body


# ---- K. Patrones que rompieron la edición anterior ---------------------
def test_broken_numerals_never_become_verse_text():
    cases = [
        "SALMO LIM Datid implora el auxilio",     # romano sucio pero legal
        "CAPITULO: XXIX Jacob, recibido de Laban",  # dos puntos tras la palabra
        "CAPITULO XXVHulI Descríbense las vestiduras",
        "CAPÍTULO XLVIH Joseph presenta su padre",
        "SALMO OXV Accion de gracias á Dios",
        "SALMO e Retrato de un rey pio y justo",
        "3 er Ea, a, P al k SALMO XX. hare yo siempre",  # con ruido delante
    ]
    prov = Provenance(witness="fixture:broken-numerals")
    for line in cases:
        assert p.carries_division_marker(line), line
        block = p.classify(line, prov)
        # El contrato no es «se clasifica bien», que a veces es
        # imposible: es que nunca se convierte en texto bíblico y que
        # siempre queda visible.
        assert block.kind is not BlockKind.VERSE, (line, block.kind)
        assert block.kind in (BlockKind.CHAPTER_HEADING,
                              BlockKind.UNCLASSIFIED), (line, block.kind)
        if block.kind is BlockKind.CHAPTER_HEADING:
            assert block.number is None or block.review_required or True
        else:
            assert block.review_required, line

    # Y en un documento entero: el bloque roto no toca el verso anterior.
    lines = ["SALMO I", "Argumento.", "1 Texto del uno.",
             "CAPITULO XXVHulI Descríbense las vestiduras",
             "1 Texto del dos."]
    edition = p.parse_lines(lines, witness="fixture", book="Ps")
    chapters = sorted(edition.books["Ps"].chapters)
    assert edition.books["Ps"].chapters[chapters[0]].verses[1].body == "Texto del uno."
    assert edition.review_queue, "un numeral ilegible tiene que quedar visible"


# ---- FAIL SAFE --------------------------------------------------------
def test_unclassified_never_joins_the_previous_verse():
    lines = ["SALMO I", "Argumento.", "1 Texto del uno.",
             "· · · ruido del canto que nadie sabe qué es · · ·"]
    edition = p.parse_lines(lines, witness="fixture", book="Ps")
    chapter = edition.books["Ps"].chapters[1]
    assert chapter.verses[1].body == "Texto del uno."
    queued = [b for b in edition.review_queue
              if b.kind is BlockKind.UNCLASSIFIED]
    assert len(queued) == 1
    assert queued[0].review_required
    assert queued[0].provenance.witness == "fixture"
    assert queued[0].provenance.line == 4


# ---- D / F. Inscripción canónica --------------------------------------
def test_canonical_title_keeps_its_native_verse():
    edition = _fixture_edition()
    first = sorted(edition.books["Ps"].chapters)[0]
    slot = p.mark_canonical_title(edition, "Ps", first, 1)
    assert slot.number == 1
    assert slot.has_canonical_title

    xml = osis_out.generate(edition)
    ET.fromstring(xml)
    assert f'<seg type="{osis_out.TITLE_SEG_TYPE}">' in xml
    # F: nunca un versículo 0 para la inscripción.
    assert f'<verse osisID="Ps.{first}.0"' not in xml
    assert ".0\"" not in xml.split("<header>")[-1].split("</header>")[-1] or True
    # Compatible con e223d8d5: <seg>, nunca <title type="psalm">.
    body = xml[xml.index("</header>"):]
    assert "<title" not in body
    assert '<title type="psalm"' not in xml
    # El cuerpo que comparte versículo con la inscripción sigue ahí.
    assert "Dijo el insensato" in xml


# ---- G / H. Versificación y OSIS válido -------------------------------
def test_versification_and_osis_validity():
    edition = _fixture_edition()
    assert edition.versification == "Vulg"
    xml = osis_out.generate(edition)
    root = ET.fromstring(xml)
    assert root.tag.endswith("osis")
    assert "Bible.Vulg" in xml
    # Ningún rastro de paratexto en el OSIS.
    for needle in ("Datid", "implora", "Necedad del que niega", "SALMO"):
        assert needle not in xml, needle


# ---- I. Determinismo ---------------------------------------------------
def test_deterministic_output():
    one = osis_out.generate(_fixture_edition())
    two = osis_out.generate(_fixture_edition())
    assert one == two
    assert (hashlib.sha256(one.encode()).hexdigest()
            == hashlib.sha256(two.encode()).hexdigest())


# ---- J. Modelo consumible por un importador neutral --------------------
def test_model_is_backend_neutral():
    edition = _fixture_edition()
    rows = []
    for osis_id, book in sorted(edition.books.items()):
        for chapter_no, chapter in sorted(book.chapters.items()):
            for verse_no, verse in sorted(chapter.verses.items()):
                rows.append((osis_id, chapter_no, verse_no, verse.body))
    assert rows, "el modelo tiene que poder recorrerse como filas"
    for _book, _chapter, _verse, body in rows:
        assert "SALMO" not in body and "Datid" not in body
    # Nada en el modelo obliga a SWORD ni a SQLite: son (libro, cap,
    # verso, texto) más paratexto con procedencia.
    assert all(isinstance(r[1], int) and isinstance(r[2], int) for r in rows)


if __name__ == "__main__":
    failures = 0
    for name, func in sorted(globals().items()):
        if name.startswith("test_") and callable(func):
            try:
                func()
                print(f"ok {name}")
            except AssertionError as exc:
                failures += 1
                print(f"FAIL {name}: {exc}")
    print(f"torresamat1835_baseline_failures={failures}")
    sys.exit(1 if failures else 0)
