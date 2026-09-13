"""Regresiones del filtro de cabecera corrida.

Ejecutar desde este directorio:

    python3 test_cabeceras.py
"""
from cabeceras import es_running_header, quita_cabecera
from load import TXT
from versiculos import corriente


def linea(*palabras, top=50):
    words = []
    x = 10
    for p in palabras:
        words.append((x, x + 20 * len(p), top, top + 20, 90, p))
        x += 20 * len(p) + 8
    return words


def test_salmos_exacto():
    assert es_running_header("SALMOS", {"Ps"})
    assert es_running_header("Salmos", {"Ps"})


def test_almos_ocr():
    assert es_running_header("ALMOS", {"Ps"})
    assert es_running_header("SALM0S", {"Ps"})


def test_uppercase_legitimo_no_es_header():
    assert not es_running_header("YAVE", {"Ps"})
    assert not es_running_header("SELA", {"Ps"})
    assert not es_running_header("AMOS", {"Ps"})
    assert not es_running_header("DIOS", {"Ps"})
    assert not es_running_header("ALMOS", {"Gen"})
    assert not es_running_header("Bienaventurado el varón", {"Ps"})
    assert not es_running_header("¿POR qué se amotinan", {"Ps"})


def test_amos_si_es_el_libro():
    assert es_running_header("AMOS", {"Amos"})


def test_quita_cabecera_columna_almos():
    lineas = [linea("ALMOS", top=400), linea("Se", "reúnen", "los", top=440)]
    out = quita_cabecera(lineas, page_h=2000, cands={"Ps"})
    assert out[0][0][TXT] == "Se"


def test_quita_cabecera_no_come_yave():
    lineas = [linea("YAVE", top=400)]
    out = quita_cabecera(lineas, page_h=2000, cands={"Ps"})
    assert out[0][0][TXT] == "YAVE"


def test_corriente_salta_almos():
    ev = corriente(
        [linea("ALMOS"),
         linea("1", "Bienaventurado", "el", "varón")],
        {"Ps"})
    textos = " ".join(s[2] if s[0] == "vers" else s[1]
                      for s in ev if s[0] in ("vers", "sigue"))
    assert "ALMOS" not in textos
    assert "Bienaventurado" in textos


if __name__ == "__main__":
    test_salmos_exacto()
    test_almos_ocr()
    test_uppercase_legitimo_no_es_header()
    test_amos_si_es_el_libro()
    test_quita_cabecera_columna_almos()
    test_quita_cabecera_no_come_yave()
    test_corriente_salta_almos()
    print("ok")
