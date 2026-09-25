"""Regresiones de la fusión DjVu + Tesseract.

Ejecutar desde este directorio:

    python3 test_load.py
"""
from load import fusion_numeros


def pagina(words):
    return {"w": 1000, "h": 1000, "words": words}


def test_numero_ya_leido_no_se_duplica():
    djvu = pagina([(100, 120, 200, 220, 0, "11")])
    tess = pagina([(101, 121, 200, 220, 91, "11"),
                   (140, 220, 200, 220, 95, "Dijo")])
    got = fusion_numeros(djvu, tess)["words"]
    assert [w[5] for w in got] == ["11", "Dijo"], got


def test_digito_truncado_se_corrige_sin_anadir_otro():
    djvu = pagina([(100, 120, 200, 220, 0, "11")])
    tess = pagina([(101, 112, 200, 220, 91, "1"),
                   (140, 220, 200, 220, 95, "Dijo")])
    got = fusion_numeros(djvu, tess)["words"]
    assert [w[5] for w in got] == ["11", "Dijo"], got


def test_volado_leido_como_letra_no_deja_dos_lecturas():
    # Sal 3 (Princeton 963): Tesseract lee el ⁶ como «S» y el DjVu trae «6»
    # en la misma caja. Añadir el «6» sin quitar la «S» dejaba «6 S A veces»
    # y la «S» acababa dentro del texto de Sal 3:5.
    djvu = pagina([(115, 126, 850, 870, 0, "6")])
    tess = pagina([(115, 128, 850, 870, 68, "S"),
                   (165, 194, 850, 870, 91, "A"),
                   (232, 324, 850, 870, 94, "veces")])
    got = fusion_numeros(djvu, tess)["words"]
    assert [w[5] for w in got] == ["6", "A", "veces"], got


def test_letra_lejos_de_un_numero_no_se_toca():
    djvu = pagina([(700, 712, 500, 520, 0, "7")])
    tess = pagina([(115, 128, 850, 870, 68, "S"),
                   (165, 194, 850, 870, 91, "A")])
    got = fusion_numeros(djvu, tess)["words"]
    assert [w[5] for w in got][:2] == ["S", "A"], got


def test_capitular_s_no_es_numero():
    # Sant (Princeton 1463): la capitular de «SIMON» es mucho más alta que
    # el texto; aunque el DjVu la lea como «5», no es un volado.
    djvu = pagina([(79, 120, 1384, 1444, 0, "5")])
    tess = pagina([(79, 120, 1384, 1444, 72, "S"),
                   (130, 220, 1390, 1410, 90, "IMON;,"),
                   (230, 320, 1390, 1410, 90, "hermano"),
                   (330, 360, 1390, 1410, 90, "de")])
    got = fusion_numeros(djvu, tess)["words"]
    assert [w[5] for w in got][:2] == ["S", "IMON;,"], got


if __name__ == "__main__":
    test_numero_ya_leido_no_se_duplica()
    test_digito_truncado_se_corrige_sin_anadir_otro()
    test_volado_leido_como_letra_no_deja_dos_lecturas()
    test_letra_lejos_de_un_numero_no_se_toca()
    test_capitular_s_no_es_numero()
    print("ok")
