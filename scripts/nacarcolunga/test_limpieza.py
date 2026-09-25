"""Regresiones de los errores de OCR de NACAR-OCR-102.

Casos cotejados con Princeton: Sal 3 (963), Sal 25 (973), Sal 51-52
(986-987), Sal 118 (1020), Job 30:20 (943), Heb 9:14 (1451), Éx 24:6 (185).

Ejecutar desde este directorio:

    python3 test_limpieza.py
"""
import json
import os
import tempfile
from collections import Counter

from construir import titulos_de_salmo
from limpieza import (aplica_erratas, carga_erratas, cierra_exclamaciones,
                      repara_cortes_ocr, repara_guiones, vocabulario_min)
from versiculos import une

VOCAB = Counter({"Yave": 5724, "Yavel": 220, "Dios": 3858, "caso": 60,
                 "el": 17066, "e": 694, "mil": 559, "mi": 3086, "Mil": 3,
                 "Mi": 107, "Israel": 2343, "Isra": 0})


def test_exclamacion_leida_como_l():
    assert (cierra_exclamaciones("¡Oh Yavel ¡Cómo se han", VOCAB)
            == "¡Oh Yave! ¡Cómo se han")
    assert (cierra_exclamaciones("porque sé, ¡oh Diosl, que tú", VOCAB)
            == "porque sé, ¡oh Dios!, que tú")
    assert (cierra_exclamaciones("¡Clamo a ti, y no me haces casol", VOCAB)
            == "¡Clamo a ti, y no me haces caso!")
    # «¡» perdido por el OCR, pero con «oh» delante.
    assert cierra_exclamaciones("dijo: «Oh Yavel Abrele", VOCAB) == \
        "dijo: «Oh Yave! Abrele"


def test_palabras_reales_en_l_no_se_tocan():
    for t in ("¡Ojalá que todo el pueblo", "¡contra mil, contra",
              "¡Escucha, Israel, oye", "¡y Mil setecientos"):
        assert cierra_exclamaciones(t, VOCAB) == t
    # Sin exclamación abierta no hay señal.
    assert cierra_exclamaciones("ante Yavel. Y dijo", VOCAB) == \
        "ante Yavel. Y dijo"


def test_guion_leido_como_punto_o_dos_puntos():
    vocab = vocabulario_min(["multiplicado multiplicado", "Absalón " * 5,
                             "multi", "dijo " * 50, "dijole " * 3,
                             "angustia " * 9])
    assert une(repara_guiones(["¡Cómo se han multi.", "plicado mis"],
                              vocab)) == "¡Cómo se han multiplicado mis"
    assert une(repara_guiones(["al huir de Ab:", "salón, su hijo"],
                              vocab)) == "al huir de Absalón, su hijo"
    # «dijo. le» no: la palabra suelta es más frecuente que la unida.
    assert une(repara_guiones(["y dijo.", "le respondió"], vocab)) == \
        "y dijo. le respondió"
    # Una vocal sola no abre renglón: «angusti: a» no se une.
    assert une(repara_guiones(["su angusti:", "a la tierra"], vocab)) == \
        "su angusti: a la tierra"


def test_marca_de_titulo_de_dos_versos():
    vers = {("Ps", 52, 1): ["y"], ("Ps", 52, 2): ["Al maestro del coro"],
            ("Ps", 52, 3): ["¿Por qué te glorias"],
            ("Ps", 51, 1): ["2 Al maestro del coro. Salmo"],
            ("Ps", 3, 1): ["y Salmo de David"]}
    out, _ = titulos_de_salmo(vers, {})
    assert out[("Ps", 52, 0)] == ["Al maestro del coro"]
    assert out[("Ps", 51, 0)] == ["Al maestro del coro. Salmo"]
    assert out[("Ps", 52, 1)] == ["¿Por qué te glorias"]
    # Título de un verso: no hay marca «¹ y ²» que quitar.
    assert out[("Ps", 3, 0)] == ["y Salmo de David"]


def test_apostrofo_y_guion_bajo_solo_si_sale_palabra():
    # Princeton: Gn 4:11 «reci-bir», Gn 6:9 «son las», Mt 7:11 «bue-nas»,
    # Is 4:3 «Jerusalén», Jos 23 «tribus». Jc 11:35 «dijo» no se convierte
    # en «djo».
    vocab = vocabulario_min(
        ["recibir "] * 5 + ["buenas "] * 5 + ["jerusalén "] * 5
        + ["tribus "] * 5 + ["son "] * 5 + ["las "] * 5
        + ["bue "] * 5 + ["nas "] * 5 + ["jeru "] * 5 + ["salén "] * 5
        + ["djo"])
    assert repara_cortes_ocr("para reci'bir de mano", vocab) == \
        "para recibir de mano"
    assert repara_cortes_ocr("Estas son'las generaciones", vocab) == \
        "Estas son las generaciones"
    assert repara_cortes_ocr("cosas bue'nas a", vocab) == "cosas buenas a"
    assert repara_cortes_ocr("de Jeru'salén serán", vocab) == \
        "de Jerusalén serán"
    assert repara_cortes_ocr("distribuido por tri_bus en", vocab) == \
        "distribuido por tribus en"
    assert repara_cortes_ocr("y d'jo: Ah", vocab) == "y d'jo: Ah"
    assert repara_cortes_ocr("contra SAN_M la vida", vocab) == \
        "contra SAN_M la vida"
    # «a'la» no se convierte en «ala».
    vocab2 = vocabulario_min(["ala "] * 5 + ["la "] * 5 + ["a"])
    assert repara_cortes_ocr("edifica a'la Iglesia", vocab2) == \
        "edifica a'la Iglesia"


def test_erratas_documentadas():
    erratas = carga_erratas()
    assert all(e["fuente"].startswith("Princeton") for e in erratas)
    textos = {"Ps 118:2": "iga Israel que es bueno",
              "Ps 3:7": "Tú hicres en la mejilla"}
    avisos = []
    aplica_erratas(textos, [e for e in erratas
                            if e["ref"] in textos], avisos)
    assert textos == {"Ps 118:2": "Diga Israel que es bueno",
                      "Ps 3:7": "Tú hieres en la mejilla"}
    assert avisos == []


def test_verso_ausente_se_alta_solo_si_el_facsimil_lo_trae():
    textos = {}
    avisos = []
    aplica_erratas(textos, [{"ref": "Phil 2:15", "alta": True,
                             "dice": "sencillos", "fuente": "Princeton 1417"}],
                   avisos)
    assert textos == {"Phil 2:15": "sencillos"} and avisos == []
    # Sin «alta» un verso que no está no se inventa.
    aplica_erratas(textos, [{"ref": "Phil 2:99", "lee": "a", "dice": "b",
                             "fuente": "x"}], avisos)
    assert "Phil 2:99" not in textos and avisos


def test_errata_ausente_no_toca_y_avisa():
    textos = {"Ps 118:2": "Diga Israel"}
    avisos = []
    aplica_erratas(textos, [{"ref": "Ps 118:2", "lee": "iga Israel",
                             "dice": "Diga Israel", "fuente": "x"},
                            {"ref": "Ps 9:9", "lee": "a", "dice": "b",
                             "fuente": "x"}], avisos)
    assert textos == {"Ps 118:2": "Diga Israel"}
    # La que ya dice lo impreso no se reescribe ni avisa. La de un verso
    # que no está, sí.
    assert len(avisos) == 1 and "Ps 9:9" not in textos


def test_errata_sin_fuente_se_rechaza():
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False,
                                     encoding="utf-8") as f:
        json.dump([{"ref": "Ps 3:1", "lee": "sor", "dice": "son"}], f)
    try:
        carga_erratas(f.name)
    except ValueError:
        pass
    else:
        raise AssertionError("errata sin fuente aceptada")
    finally:
        os.unlink(f.name)


if __name__ == "__main__":
    test_exclamacion_leida_como_l()
    test_palabras_reales_en_l_no_se_tocan()
    test_guion_leido_como_punto_o_dos_puntos()
    test_apostrofo_y_guion_bajo_solo_si_sale_palabra()
    test_marca_de_titulo_de_dos_versos()
    test_erratas_documentadas()
    test_verso_ausente_se_alta_solo_si_el_facsimil_lo_trae()
    test_errata_ausente_no_toca_y_avisa()
    test_errata_sin_fuente_se_rechaza()
    print("ok")
