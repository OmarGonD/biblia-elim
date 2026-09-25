"""Regresiones de la correspondencia documentada verso impreso -> NRSVA.

Sal 13 (Princeton 967): el título es el v. 1 impreso y el v. 6 impreso
reparte en NRSVA 13:5-6 tras su segundo separador de hemistiquio.

Ejecutar desde este directorio:

    python3 test_correspondencias.py
"""
import json
import os
import tempfile

from construir import (aplica_correspondencias, carga_correspondencias,
                       titulos_de_salmo)
from versiculos import une

SAL_13 = {
    ("Ps", 13, 1): ["Al maestro del coro. Salmo de", "David."],
    ("Ps", 13, 2): ["¿Hasta cuándo, por fin, te olvi-",
                    "darás, Yave, de mí? | ¿Hasta cuándo"],
    ("Ps", 13, 3): ["¿Hasta cuándo mandarás dolo-"],
    ("Ps", 13, 4): ["¡Mírame ya, óyeme, Yave, Dios"],
    ("Ps", 13, 5): ["Que no pueda decir mi enemigo:"],
    ("Ps", 13, 6): ["Después de haber esperado en",
                    "tu piedad. | Que se alegre mi corazón",
                    "con tu socorro, | que pueda cantar",
                    "a Yave: «Bien me proveyó.»"],
    ("Ps", 1, 1): ["Salmo vecino, intacto."],
}


def _proc(vers):
    return {k: [{"top": k[2]}] for k in vers}


def test_dato_documentado_se_valida():
    corr = carga_correspondencias()
    assert set(corr) == {("Ps", 13)}
    impresos, cortes = corr[("Ps", 13)]
    assert impresos == {1: [0], 2: [1], 3: [2], 4: [3], 5: [4], 6: [5, 6]}
    assert cortes == {6: 2}


def test_sal_13_titulo_y_reparto():
    avisos = []
    vers, proc = aplica_correspondencias(
        dict(SAL_13), _proc(SAL_13), avisos, carga_correspondencias())
    vers, proc = titulos_de_salmo(vers, proc)
    t = {k: une(v) for k, v in vers.items()}
    assert avisos == []
    assert t[("Ps", 13, 0)] == "Al maestro del coro. Salmo de David."
    assert t[("Ps", 13, 1)].startswith("¿Hasta cuándo, por fin")
    assert t[("Ps", 13, 4)] == "Que no pueda decir mi enemigo:"
    assert t[("Ps", 13, 5)] == ("Después de haber esperado en tu piedad. "
                                "Que se alegre mi corazón con tu socorro,")
    assert t[("Ps", 13, 6)] == "que pueda cantar a Yave: «Bien me proveyó.»"
    assert sorted(v for (_, c, v) in t if c == 13) == [0, 1, 2, 3, 4, 5, 6]
    # Ni duplica ni pierde texto: la suma es el salmo impreso entero.
    assert " ".join(t[("Ps", 13, v)] for v in range(7)) == " ".join(
        une(SAL_13[("Ps", 13, v)]) for v in range(1, 7))
    assert t[("Ps", 1, 1)] == "Salmo vecino, intacto."
    assert proc[("Ps", 13, 5)] == proc[("Ps", 13, 6)] == [{"top": 6}]
    assert proc[("Ps", 13, 0)] == [{"top": 1}]


def test_verso_impreso_ausente_no_se_rellena():
    datos = dict(SAL_13)
    del datos[("Ps", 13, 5)]
    vers, _ = aplica_correspondencias(
        datos, _proc(datos), [], carga_correspondencias())
    assert ("Ps", 13, 4) not in vers
    assert ("Ps", 13, 3) in vers and ("Ps", 13, 5) in vers


def test_sin_separador_no_reparte_y_avisa():
    datos = dict(SAL_13)
    datos[("Ps", 13, 6)] = ["Después de haber esperado en tu piedad."]
    avisos = []
    vers, proc = aplica_correspondencias(
        datos, _proc(datos), avisos, carga_correspondencias())
    assert vers[("Ps", 13, 5)] == ["Después de haber esperado en tu piedad."]
    assert ("Ps", 13, 6) not in vers and ("Ps", 13, 6) not in proc
    assert len(avisos) == 1 and "Ps 13:6" in avisos[0]


def _rechaza(dato):
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False,
                                     encoding="utf-8") as f:
        json.dump({"Ps 13": dato}, f)
    try:
        carga_correspondencias(f.name)
    except ValueError:
        return True
    finally:
        os.unlink(f.name)
    return False


def test_datos_incoherentes_se_rechazan():
    base = {"1": [0], "2": [1], "3": [2], "4": [3], "5": [4], "6": [5, 6]}
    assert not _rechaza({"impresos": base, "cortes": {"6": 2}})
    # Destino repetido.
    assert _rechaza({"impresos": dict(base, **{"5": [3]}),
                     "cortes": {"6": 2}})
    # Falta un verso impreso.
    assert _rechaza({"impresos": {k: v for k, v in base.items() if k != "3"},
                     "cortes": {"6": 2}})
    # NRSVA 6 sin origen.
    assert _rechaza({"impresos": dict(base, **{"6": [5]})})
    # Reparto en dos sin corte documentado.
    assert _rechaza({"impresos": base})


if __name__ == "__main__":
    test_dato_documentado_se_valida()
    test_sal_13_titulo_y_reparto()
    test_verso_impreso_ausente_no_se_rellena()
    test_sin_separador_no_reparte_y_avisa()
    test_datos_incoherentes_se_rechazan()
    print("ok")
