"""Regresiones de la política de completar.py (NACAR-FALLBACK-101).

Un verso con texto de Nácar-Colunga no se sustituye por otra traducción,
aunque sea más corto que el testigo, y la etapa no depende de otras Biblias.

Ejecutar desde este directorio:

    python3 test_completar.py
"""
import hashlib
import json
import os
import re
import socket
import tempfile

import completar
import osis
from completar import completar_todo, solapamiento, toks

SAL_17_7 = ("Ostenta tu magnífica piedad, tú que salvas del enemigo a los que "
            "a ti se acogen.")
SAL_17_7_RVG = ("Muestra tus maravillosas misericordias, tú que con tu diestra "
                "salvas a los que en ti confían de los que se levantan contra "
                "ellos .")
# Sal 16:3 tal como sale del parser: es_titulo() descarta el renglón
# «| son de mí muy honrados, | en ellos» (NACAR-OCR-103).
SAL_16_3 = "Los santos que en la tierra están, tengo todas mis delicias."
SAL_16_3_RV = ("Sino á los santos que están en la tierra, y á los íntegros: "
               "toda mi afición en ellos.")


def antes_se_sustituia(src, wit):
    """El criterio retirado: menos del 80 % de las palabras del testigo."""
    return (solapamiento(toks(src), toks(wit)) >= 3 and len(toks(wit)) >= 6 and
            len(toks(src)) < max(8, int(len(toks(wit)) * 0.80)))


def test_source_presente_mas_corto_que_testigo_se_conserva():
    src = "Tomó Judá para Her, su primogénito, una mujer llamada Tamar."
    wit = ("Ahora bien, tomó Judá para Er, su primogénito, una mujer que se "
           "llamaba Tamar y era hermosa a los ojos de todos.")
    assert antes_se_sustituia(src, wit)
    nuevo, tocados = completar_todo({"Gen 38:6": src})
    assert nuevo == {"Gen 38:6": src}
    assert tocados == []


def test_sal_17_7_conserva_nacar():
    assert antes_se_sustituia(SAL_17_7, SAL_17_7_RVG)
    nuevo, tocados = completar_todo({"Ps 17:7": SAL_17_7})
    assert nuevo["Ps 17:7"] == SAL_17_7
    assert tocados == []


def test_sal_16_3_no_oculta_la_perdida_del_parser():
    # completar.py ya no tapa con Reina-Valera el renglón que perdió el
    # parser: el verso queda como lo dejó construir.py, a la vista.
    assert antes_se_sustituia(SAL_16_3, SAL_16_3_RV)
    nuevo, tocados = completar_todo({"Ps 16:3": SAL_16_3})
    assert nuevo["Ps 16:3"] == SAL_16_3
    assert tocados == []


def test_truncado_con_guion_tambien_se_conserva():
    # Una de las pocas señales estructurales de corte disponibles hoy.
    # Sin metadata explícita de cuerpo perdido, tampoco se rellena.
    src = "Yave habló a Moisés, dicien-"
    nuevo, tocados = completar_todo({"Lev 18:1": src})
    assert nuevo["Lev 18:1"] == src
    assert tocados == []


def test_verso_sin_texto_lo_suple_el_visor_no_completar():
    # completar.py no rellena versos sin texto: osis.py no los escribe y el
    # visor los suple al leer el módulo, desde otra Biblia y con su aviso.
    nuevo, tocados = completar_todo({"Ps 99:1": ""})
    assert nuevo["Ps 99:1"] == ""
    assert tocados == []
    with tempfile.TemporaryDirectory() as tmp:
        destino = os.path.join(tmp, "prueba.osis.xml")
        osis.genera({"Ps 99:1": "", "Ps 99:2": "Yave es grande en Sión."}, destino)
        xml = open(destino, encoding="utf-8").read()
    assert 'osisID="Ps.99.1"' not in xml
    assert re.search(r'osisID="Ps\.99\.2"><seg type="[^"]+">Yave es grande', xml)


def test_main_funciona_sin_testigos_ni_red():
    # Directorio sin fuentes/ ni testigos.pkl; descarga, diatheke y red
    # bloqueados. main() debe dejar texto.json idéntico y reconstruidos vacío.
    def prohibido(*a, **k):
        raise AssertionError("completar.main() no debe usar testigos ni red")

    texto = {"Ps 17:7": SAL_17_7, "Ps 16:3": SAL_16_3, "Exod 37:3": "",
             "Lev 18:1": "Yave habló a Moisés, dicien-"}
    guardado = (completar.DIR, completar.CACHE, completar.descarga,
                completar.subprocess.run, socket.socket)
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "texto.json")
        with open(path, "w", encoding="utf-8") as f:
            json.dump(texto, f, ensure_ascii=False, indent=0)
        antes = hashlib.sha256(open(path, "rb").read()).hexdigest()
        try:
            completar.DIR = tmp
            completar.CACHE = os.path.join(tmp, "fuentes", "testigos.pkl")
            completar.descarga = prohibido
            completar.subprocess.run = prohibido
            socket.socket = prohibido
            completar.main()
        finally:
            (completar.DIR, completar.CACHE, completar.descarga,
             completar.subprocess.run, socket.socket) = guardado
        assert hashlib.sha256(open(path, "rb").read()).hexdigest() == antes
        assert open(os.path.join(tmp, "reconstruidos.txt"), encoding="utf-8").read() == ""
        assert not os.path.exists(os.path.join(tmp, "fuentes"))


if __name__ == "__main__":
    test_source_presente_mas_corto_que_testigo_se_conserva()
    test_sal_17_7_conserva_nacar()
    test_sal_16_3_no_oculta_la_perdida_del_parser()
    test_truncado_con_guion_tambien_se_conserva()
    test_verso_sin_texto_lo_suple_el_visor_no_completar()
    test_main_funciona_sin_testigos_ni_red()
    print("ok")
