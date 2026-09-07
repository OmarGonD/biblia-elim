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


if __name__ == "__main__":
    test_numero_ya_leido_no_se_duplica()
    test_digito_truncado_se_corrige_sin_anadir_otro()
    print("ok")
