"""Regresiones de corrige_repetidos(): volado leído una unidad de más.

El OCR lee el ⁵ pequeño como 6 (Sal 13, Sal 17, Éx 19), el ² como 3 (Lv 13)
o el ¹⁸ como 19 (Heb 11): «4, 6, 6, 7» es en el facsímil «4, 5, 6, 7».

Ejecutar desde este directorio:

    python3 test_repetidos.py
"""
import importlib.util
import os

from versiculos import corrige_repetidos

DIR = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "alinear_ta", os.path.join(DIR, "..", "torresamat", "alinear.py"))
alinear = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(alinear)


def v(n, texto="x", inicio=True):
    return ("vers", n, texto, {"Ps"}, inicio, None)


def nums(ev):
    return [s[1] for s in ev if s[0] == "vers"]


def test_cinco_leido_como_seis():
    ev = [("cap", 17, {"Ps"}, "cabecera"), v(4), v(6, "Y mis pies"),
          v(6, "Te invoco"), v(7)]
    out = corrige_repetidos(ev)
    assert nums(out) == [4, 5, 6, 7]
    assert out[2][2] == "Y mis pies" and out[3][2] == "Te invoco"


def test_fin_de_capitulo_confirma():
    # Sal 13: el ⁵ leído como 6 es el penúltimo verso del salmo.
    ev = [("cap", 13, {"Ps"}, "cabecera"), v(4), v(6, "Que no pueda"),
          v(6, "Después de haber esperado"), ("cap", 14, {"Ps"}, "cabecera"),
          v(1)]
    assert nums(corrige_repetidos(ev)) == [4, 5, 6, 1]


def test_marca_a_mitad_de_renglon_y_otras_cifras():
    ev = [v(1, inicio=False), v(3, inicio=False), v(3, inicio=False), v(4)]
    assert nums(corrige_repetidos(ev)) == [1, 2, 3, 4]
    ev = [v(124), v(126), v(126), v(127)]
    assert nums(corrige_repetidos(ev)) == [124, 125, 126, 127]


def test_ambiguo_no_se_toca():
    # Con n+2 detrás no se sabe si se perdió el 7 o el 5.
    ev = [v(4), v(6), v(6), v(8)]
    assert corrige_repetidos(ev) == ev
    # Sin predecesor ausente no hay señal.
    ev = [v(5), v(6), v(6), v(7)]
    assert corrige_repetidos(ev) == ev


def test_no_cruza_capitulos():
    ev = [v(4), v(6), ("cap", 2, {"Ps"}, "cabecera"), v(6), v(7)]
    assert corrige_repetidos(ev) == ev


def test_ensambla_deja_cada_verso_en_su_sitio():
    ev = [("libro", "Ps", None), ("cap", 1, {"Ps"}, "cabecera"),
          v(1, "uno"), v(2, "dos"), v(3, "tres"), v(4, "cuatro"),
          v(6, "cinco"), v(6, "seis"), v(7, "siete")]
    vers, _, _ = alinear.ensambla(
        corrige_repetidos(ev), [{"osis": "Ps", "versos": [7]}])
    assert vers[("Ps", 1, 5)] == ["cinco"]
    assert vers[("Ps", 1, 6)] == ["seis"]


if __name__ == "__main__":
    test_cinco_leido_como_seis()
    test_fin_de_capitulo_confirma()
    test_marca_a_mitad_de_renglon_y_otras_cifras()
    test_ambiguo_no_se_toca()
    test_no_cruza_capitulos()
    test_ensambla_deja_cada_verso_en_su_sitio()
    print("ok")
