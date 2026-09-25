"""Columnas fundidas por el OCR (TORRES-PSALM-ALIGN-101).

    python3 test_columnas.py

Hoja 59 del tomo III: Sal 112 e «In exitu» (Vulg 113:1-8) en una columna,
«Non nobis» (impreso 1-18, Vulg 113:9-26) en la otra; el OCR las leyó
renglón a renglón.
"""
import re

import columnas_fundidas as col
import parche_facsimil as parche


def _plano(t):
    return re.sub(r"<[^>]+>", "", t).strip()


def test_sal_113_completo_y_en_orden():
    todos = parche.cambios()
    v = {n: _plano(todos[f"Psalms 113:{n}"][1]) for n in range(1, 27)}
    assert all(v.values())
    assert v[1].startswith("Aleluya. Cuando Israél salió")
    assert v[8].startswith("Que convirtió la peña")
    assert v[9].startswith("No áÁ NOSOTROS")
    assert v[14].startswith("Orejas") and v[15].startswith("Tienen manos")
    assert v[26].startswith("Nosotros sí, los que vivimos")
    # Ninguna copia del 114/115 queda dentro del 113.
    texto = " ".join(v.values())
    for copia in ("Amé al Señor", "Acepto seré yo", "Creí d Dios",
                  "De gran precio", "SALMO"):
        assert copia not in texto, copia


def test_sal_112_sin_in_exitu():
    todos = parche.cambios()
    for n in range(1, 10):
        t = todos[f"Psalms 112:{n}"][1]
        assert "Israél salió" not in t and "Grandeza de Dios" not in t, n
    assert _plano(todos["Psalms 112:9"][1]).endswith("rodeada de hijos.")


def test_hoja_65_y_cabeceras():
    todos = parche.cambios()
    assert todos["Psalms 131:2"][1].startswith("De cómo juró al Señor")
    assert todos["Psalms 131:3"][1].startswith("No me meteré yo")
    assert "No me meteré" not in todos["Psalms 131:5"][1]
    assert todos["Psalms 132:3"][1].endswith("vida sempiterna.")
    assert todos["Psalms 133:2"][1].startswith("Levantad por las noches")
    assert todos["Psalms 133:3"][1].startswith("Bendígate desde Sion")
    assert todos["Psalms 58:4"][1].endswith("hombres de gran fuerza.")
    assert todos["Psalms 77:38"][1].endswith("á todo su enojo;")
    assert "TOMO III" not in todos["II Maccabees 15:40"][1]
    for ref, (_v, nuevo, hoja) in col.COLUMNAS.items():
        assert "SALMO" not in _plano(nuevo), ref
        assert hoja.startswith("tomo "), ref


def test_la_guarda_rechaza_texto_inventado():
    guardado = dict(col.COLUMNAS)
    try:
        viejo, nuevo, hoja = guardado["Psalms 131:3"]
        col.COLUMNAS["Psalms 131:3"] = (viejo, nuevo + " y otro renglón",
                                        hoja)
        try:
            parche._comprueba_columnas()
        except ValueError:
            pass
        else:
            raise AssertionError("se aceptó texto que no estaba")
    finally:
        col.COLUMNAS.clear()
        col.COLUMNAS.update(guardado)


if __name__ == "__main__":
    for nombre, f in list(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
