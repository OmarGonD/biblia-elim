"""Encabezados de salmo pegados al versículo anterior (TORRES-PSALM-GLUE-101).

    python3 test_cabeceras.py

Sal 146:11 traía detrás de «…en su misericordia.» el encabezado del Salmo
CXLVII, su argumento, el salmo entero (que ya está en 147:1-9), el pie de la
lámina de Doré y el encabezado del CXLVIII (tomo III, hojas 69-72).
"""
import re

import cabeceras_pegadas as cab
import parche_facsimil as parche

ROMANO = re.compile(r"SALMOS?\b")


def _plano(t):
    return re.sub(r"<[^>]+>", "", t)


def test_ningun_encabezado_queda_en_el_texto_nuevo():
    for ref, (_viejo, nuevo, hoja) in cab.CABECERAS.items():
        assert not ROMANO.search(_plano(nuevo)), ref
        assert hoja.startswith("tomo III, hoja "), ref


def test_sal_146_11_y_147():
    viejo, nuevo, _h = cab.CABECERAS["Psalms 146:11"]
    assert nuevo == ("Se complace sí en aquellos que le temen y adoran, y en "
                     "los que confian en su misericordia. "
                     '<chapter eID="gen16785" osisID="Ps.146"/>')
    # Lo quitado es el Sal 147 entero, que ya está en su sitio.
    assert "Él despide el granizo" in viejo
    assert cab.CABECERAS["Psalms 147:9"][1].startswith(
        "No ha hecho otro tanto")
    assert cab.CABECERAS["Psalms 147:9"][1].endswith(
        'preceptos. Aleluya. <chapter eID="gen16796" osisID="Ps.147"/>')
    # 147:2 pierde la copia del 147:3, y 147:1 recupera «Dios.».
    assert cab.CABECERAS["Psalms 147:2"][1].endswith("que moran dentro de tí.")
    assert parche.cambios()["Psalms 147:1"][1].endswith("oh Sion, á tu Dios.")


def test_cabecera_de_pagina_en_medio_del_versiculo():
    nuevo = cab.CABECERAS["Psalms 18:15"][1]
    assert "la meditacion de mi corazon que hare yo siempre en tu " \
           "acatamiento." in nuevo


def test_traslado_117_15_y_anadidos():
    v14 = cab.CABECERAS["Psalms 117:14"]
    v15 = cab.CABECERAS["Psalms 117:15"]
    assert v14[1] == ("El Señor es mi fortaleza y mi gloria; el Señor se ha "
                      "constituido salvacion mia.")
    assert v15[0] == "" and v15[1] in v14[0]
    assert "Prescrito por la Ley" not in v15[1]
    assert cab.CABECERAS["Psalms 83:13"][1].startswith(
        "No dejará sin bienes") and "que pone en tí su esperanza. <chapter" \
        in cab.CABECERAS["Psalms 83:13"][1]
    assert "de tu divina cara. <chapter" in cab.CABECERAS["Psalms 139:14"][1]


def test_la_comprobacion_rechaza_lo_que_no_es_quitar_un_encabezado():
    guardado = dict(cab.CABECERAS)
    try:
        viejo, nuevo, hoja = guardado["Psalms 6:11"]
        # Quitar texto que no es encabezado.
        cab.CABECERAS["Psalms 6:11"] = (viejo, viejo[:40], hoja)
        _rechaza()
        # Añadir algo que no está declarado.
        cab.CABECERAS["Psalms 6:11"] = (viejo, nuevo.replace(
            "ignominia.", "ignominia y oprobio."), hoja)
        _rechaza()
    finally:
        cab.CABECERAS.clear()
        cab.CABECERAS.update(guardado)


def _rechaza():
    try:
        parche._comprueba_cabeceras()
    except ValueError:
        return
    raise AssertionError("se aceptó una entrada mal formada")


def test_aplica_sobre_el_texto_del_modulo():
    entradas = [(ref, viejo) for ref, (viejo, _n, _h)
                in cab.CABECERAS.items()]
    autorizados = {ref: (v, n) for ref, (v, n, _h)
                   in cab.CABECERAS.items()}
    nuevas = dict(parche.aplica(entradas, autorizados))
    for ref, (_v, nuevo, _h) in cab.CABECERAS.items():
        assert nuevas[ref] == nuevo, ref


if __name__ == "__main__":
    for nombre, f in list(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
