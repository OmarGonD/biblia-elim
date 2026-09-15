"""Regresiones de la cabecera impresa de salmo («14 (Vulg. 13.)»).

El OCR leyó el separador de hemistiquio de Sal 14:5 («a su tiempo, |
porque está Dios…») como «tiempo, 1 porque». Ese 1 a mitad de renglón
abría un capítulo candidato con la cola del salmo, y el alineador corría
los salmos 15, 16 y 17 un puesto.

Ejecutar desde este directorio:

    python3 test_salmos_cabecera.py
"""
import importlib.util
import os

from versiculos import corriente

DIR = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "alinear_ta", os.path.join(DIR, "..", "torresamat", "alinear.py"))
alinear = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(alinear)


def linea(texto, top=50):
    words = []
    x = 10
    for p in texto.split():
        words.append((x, x + 20 * len(p), top, top + 20, 90, p))
        x += 20 * len(p) + 8
    return words


def lineas(*textos):
    return [linea(t, top=50 + 30 * i) for i, t in enumerate(textos)]


SAL_14_15 = (
    "14 (Vulg. 13.)",
    "1 Al maestro del coro. De David.",
    "2 Mira Yave desde lo alto de los",
    "4 ¿Se han vuelto del todo locos los",
    "6 Ya temblarán con terror a su",
    "tiempo, 1 porque está Dios con la",
    "generación de los justos.",
    "6 Queréis frustrar los consejos del",
    "7 Venga ya de Dios la salvación",
    "15 (Vulg. 14.)",
    "1 Salmo de David.",
    "2 El que anda en integridad y obra",
)


def test_cabecera_es_capitulo():
    ev = corriente(lineas("14 (Vulg. 13.)", "1 Al maestro del coro."), {"Ps"})
    assert ev[0] == ("cap", 14, {"Ps"}, "cabecera")
    assert ev[1][0] == "vers" and ev[1][1] == 1


def test_cabecera_no_es_verso_ni_se_pierde():
    ev = corriente(lineas("16 (Vulg. 15.)"), {"Ps"})
    assert ev == [("cap", 16, {"Ps"}, "cabecera")]


def test_epigrafe_queda_antes_del_capitulo():
    # El capítulo se abre delante del verso 1, no en la cabecera: el
    # epígrafe no cambia de sitio respecto a antes de leer cabeceras.
    ev = corriente(lineas("7 Venga ya de Dios la salvación",
                          "15 (Vulg. 14.)",
                          "Condiciones de pureza del que ha de",
                          "estar ante el Señor.",
                          "1 Salmo de David."), {"Ps"})
    tipos = [s[0] for s in ev]
    assert tipos.index("cap") == len(tipos) - 2
    assert ev[-2] == ("cap", 15, {"Ps"}, "cabecera")
    assert ev[-1][0] == "vers" and ev[-1][1] == 1
    assert any(s[0] == "sigue" and "Señor" in s[1] for s in ev[:-2])


def test_marca_interna_no_parte_salmo_con_cabecera():
    ev = corriente(lineas(*SAL_14_15), {"Ps"})
    obs = alinear.segmenta(ev)
    assert [c["num_cap"] for c in obs] == [14, 15]
    assert obs[0]["nums"][-3:] == [1, 6, 7]
    assert obs[1]["nums"] == [1, 2]


def test_sin_cabecera_la_marca_interna_sigue_reiniciando():
    # Prosa y Torres Amat: sin cabecera impresa la regla no cambia.
    ev = corriente(lineas(*SAL_14_15[1:9]), {"Ps"})
    obs = alinear.segmenta(ev)
    assert len(obs) == 2
    assert obs[1]["nums"] == [1, 6, 7]


def test_con_cabecera_marca_al_inicio_sigue_pudiendo_reiniciar():
    ev = corriente(lineas(
        "14 (Vulg. 13.)",
        "1 Al maestro del coro.",
        "6 Queréis frustrar los consejos.",
        "1 Comienza realmente otro bloque.",
    ), {"Ps"})
    obs = alinear.segmenta(ev)

    assert len(obs) == 2
    assert obs[0]["num_cap"] == 14
    assert obs[1]["nums"] == [1]


if __name__ == "__main__":
    test_cabecera_es_capitulo()
    test_cabecera_no_es_verso_ni_se_pierde()
    test_epigrafe_queda_antes_del_capitulo()
    test_marca_interna_no_parte_salmo_con_cabecera()
    test_sin_cabecera_la_marca_interna_sigue_reiniciando()
    test_con_cabecera_marca_al_inicio_sigue_pudiendo_reiniciar()
    print("ok")
