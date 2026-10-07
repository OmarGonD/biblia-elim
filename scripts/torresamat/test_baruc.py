"""Baruc íntegro, capítulos canónicos, límites OSIS y notas de sus páginas."""
import collections
import re

import baruc

PLAN = baruc.load_plan()
RECORDS = {r["reference"]: r for r in PLAN["corrections"]}


def test_canon_completo():
    assert PLAN["chapter_counts"] == [22, 35, 38, 37, 9, 72]
    assert len(RECORDS) == 213
    assert all(r["text"].strip() and r["witness"].startswith("https://") for r in RECORDS.values())
    assert sum(not re.sub(r"<[^>]+>", "", r["before"]).strip() for r in RECORDS.values()) == 25


def test_ubicacion_de_los_capitulos():
    assert RECORDS["Baruch 3:1"]["text"].startswith("Y ahora, oh Señor todopoderoso")
    assert RECORDS["Baruch 3:27"]["text"].startswith("No fueron estos escogidos")
    assert RECORDS["Baruch 4:1"]["text"].startswith("La Sabiduría, este es el Libro")
    assert RECORDS["Baruch 4:27"]["text"].startswith("Hijos, tened buen ánimo")
    assert RECORDS["Baruch 5:1"]["text"].startswith("Desnúdate, oh Jerusalem")
    assert RECORDS["Baruch 6:1"]["text"].startswith("Por los pecados")


def test_conserva_la_oracion_omitida_en_la_web():
    assert "y ayunaban, y oraban" in RECORDS["Baruch 1:5"]["text"]


def test_no_hay_ezequiel_intercalado():
    text = " ".join(r["text"] for r in RECORDS.values()).casefold()
    for foreign in ["ruedas", "cara de buey", "trono de piedra", "electro", "quebar"]:
        assert foreign not in text
    assert RECORDS["Baruch 6:72"]["text"].endswith("lejos de la ignominia.")


def test_cierres_osis_canonicos():
    for chapter, count in enumerate(PLAN["chapter_counts"], 1):
        endings = [r["reference"] for r in RECORDS.values()
                   if re.search(r'<chapter .*osisID="Bar\.' + str(chapter) + r'"', r["after"])]
        assert endings == [f"Baruch {chapter}:{count}"]
    assert '<div eID=' in RECORDS["Baruch 6:72"]["after"]


def test_idempotencia_y_no_sobrescribe_texto_ajeno():
    for key, r in RECORDS.items():
        assert baruc.correct(key, r["before"], RECORDS) == (r["after"], int(r["before"] != r["after"]))
        assert baruc.correct(key, r["after"], RECORDS) == (r["after"], 0)
    try:
        baruc.correct("Baruch 3:1", "Un texto distinto.", RECORDS)
    except ValueError:
        pass
    else:
        raise AssertionError("no debe sobrescribir una fuente desconocida")
    assert baruc.correct("Mark 2:17", "llamar á convertir", RECORDS) == ("llamar á convertir", 0)


def test_notas_siguen_las_paginas_corregidas():
    notes = {r["reference"]: r["after"] for r in PLAN["notes"]}
    assert len(notes) == 213 and len(PLAN["note_groups"]) == 12
    assert "de 3:23 a 4:8" in notes["Bar.3.27"]
    assert "de 4:9 a 4:34" in notes["Bar.4.27"]
    assert "de 4:35 a 6:4" in notes["Bar.5.1"]
    assert "de 6:30 a 6:48" in notes["Bar.6.31"]
    assert "Continúa el Profeta implorando" not in " ".join(notes.values())


def test_enlaces_de_notas_no_cruzan_huecos_ni_capitulos():
    entries = [("Baruch 1:1", "nota"), ("Baruch 1:2", "nota"),
               ("Baruch 1:3", ""), ("Baruch 1:4", "nota"),
               ("Baruch 2:1", "nota"), ("Baruch 2:2", "nota"),
               ("Ezekiel 1:1", "nota")]
    assert baruc.compress_ranges(entries) == [
        ("Baruch 1:1-2", "nota"), ("Baruch 1:4", "nota"),
        ("Baruch 2:1-2", "nota"), ("Ezekiel 1:1", "nota")]


if __name__ == "__main__":
    for name, f in sorted(globals().copy().items()):
        if name.startswith("test_"):
            f()
            print("ok", name)
