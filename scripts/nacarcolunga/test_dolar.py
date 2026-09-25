"""Regresiones de parte_dolar(): el ⁵ leído 6 y el ⁶ leído «$» (NACAR-OCR-107).

Sal 118 (hoja Princeton 1020): «⁵ En la angustia invoqué a Yave, | y me oyó
Yave poniéndome en salvo. ⁶ Está por mí Yave…». El OCR leyó el ⁵ como 6 y
el ⁶ como «$»: el 5 quedaba vacío y el 6 llevaba los dos.

Ejecutar desde este directorio:

    python3 test_dolar.py
"""
import json
import os

from versiculos import parte_dolar

DIR = os.path.dirname(os.path.abspath(__file__))
O1 = {"pagina": 1020, "columna": 1, "top": 1903}
O2 = {"pagina": 1020, "columna": 1, "top": 1978}


def _sal118():
    vers = {("Ps", 118, 4): ["Diga, pues, Israel"],
            ("Ps", 118, 6): ["En la angustia invoqué a Yave, |",
                             "y me oyó Yave poniéndome en salvo.",
                             "$ Está por mí Yave: ¿Qué puedo",
                             "temer, | qué podrán. hacerme los"],
            ("Ps", 118, 7): ["Está Yave por mí"]}
    proc = {("Ps", 118, 6): [O1]}
    sucesos = [("vers", 6, "En la angustia invoqué a Yave, |", {"Ps"}, True,
                O1),
               ("sigue", "y me oyó Yave poniéndome en salvo.", {"Ps"}, O1),
               ("sigue", "$ Está por mí Yave: ¿Qué puedo", {"Ps"}, O2)]
    return vers, proc, sucesos


def test_sal_118_5_y_6():
    vers, proc, sucesos = _sal118()
    assert parte_dolar(vers, proc, sucesos) == [("Ps", 118, 6)]
    assert vers[("Ps", 118, 5)] == ["En la angustia invoqué a Yave, |",
                                     "y me oyó Yave poniéndome en salvo."]
    assert vers[("Ps", 118, 6)] == ["Está por mí Yave: ¿Qué puedo",
                                     "temer, | qué podrán. hacerme los"]
    # Cada verso abre en su renglón.
    assert proc[("Ps", 118, 5)] == [O1] and proc[("Ps", 118, 6)] == [O2]


def test_dolar_a_mitad_de_renglon():
    # Lc 10 / Jer 17: «… de Yave. ⁶ Será como …» en el mismo renglón.
    vers = {("Jer", 17, 4): ["x"],
            ("Jer", 17, 6): ["Así dice Yave: Maldito",
                             "su corazón de Yave. $ Será como"],
            ("Jer", 17, 7): ["Bendito"]}
    sucesos = [("sigue", "su corazón de Yave. $ Será como", {"Jer"}, O2)]
    assert parte_dolar(vers, {}, sucesos) == [("Jer", 17, 6)]
    assert vers[("Jer", 17, 5)] == ["Así dice Yave: Maldito",
                                     "su corazón de Yave."]
    assert vers[("Jer", 17, 6)] == ["Será como"]


def test_sin_senal_completa_no_se_toca():
    base, _p, sucesos = _sal118()
    casos = []
    # El 5 ya tiene texto.
    v = dict(base)
    v[("Ps", 118, 5)] = ["algo"]
    casos.append(v)
    # Sin el 4 delante o sin el 7 detrás.
    v = dict(base)
    del v[("Ps", 118, 4)]
    casos.append(v)
    v = dict(base)
    del v[("Ps", 118, 7)]
    casos.append(v)
    # Dos «$»: no se sabe cuál es la marca.
    v = dict(base)
    v[("Ps", 118, 6)] = base[("Ps", 118, 6)] + ["$ otra"]
    casos.append(v)
    for v in casos:
        antes = {k: list(t) for k, t in v.items()}
        assert parte_dolar(v, {}, sucesos) == []
        assert v == antes


def test_otra_cifra_no_se_toca():
    # Mt 2:5: el «$» es el 6 y el hueco es el 4; en el 5 no se parte.
    vers = {("Matt", 2, 3): ["x"],
            ("Matt", 2, 5): ["En Belén de Judá, pues así está escrito por "
                             "el profeta: $ «Y tú, Belén"],
            ("Matt", 2, 6): ["y"]}
    sucesos = [("vers", 5, vers[("Matt", 2, 5)][0], {"Matt"}, True, O1)]
    assert parte_dolar(vers, {}, sucesos) == []


def test_texto_construido():
    """texto.json (salida de construir.py) trae el 118:5 y el 118:6."""
    ruta = os.path.join(DIR, "texto.json")
    if not os.path.exists(ruta):
        print("  (sin texto.json: se salta)")
        return
    texto = json.load(open(ruta, encoding="utf-8"))
    assert texto["Ps 118:5"] == ("En la angustia invoqué a Yave, y me oyó "
                                 "Yave poniéndome en salvo.")
    assert texto["Ps 118:6"].startswith("Está por mí Yave: ¿Qué puedo temer")
    assert "$" not in texto["Ps 118:6"]


if __name__ == "__main__":
    for nombre, f in list(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
