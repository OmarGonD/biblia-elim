"""Ornatos de lámina leídos como letras (ornatos.py).

    python3 test_ornatos.py
"""
import ornatos

ORNATO = "Il A Ú MN UU 1 ll AN Ñ Il IN"


def test_quita_el_tramo_y_deja_el_resto():
    texto = f"Dijole Dios: Yo estaré contigo {ORNATO} pueblo de Egypto, ofrecerás"
    nuevo, idx = ornatos.limpia(texto)
    assert nuevo == "Dijole Dios: Yo estaré contigo pueblo de Egypto, ofrecerás"
    assert len(idx) == len(ORNATO.split())


def test_verso_normal_no_cambia():
    texto = "Y á la de los que en el camino de la vida, y a un padre"
    assert ornatos.limpia(texto) == (texto, [])


def test_palabras_cortas_dentro_del_ornato_caen_con_el_tramo():
    nuevo, _ = ornatos.limpia(f"Dios de {ORNATO} y a {ORNATO} gloria")
    assert nuevo == "Dios de gloria"


def test_cita_al_margen_se_conserva():
    texto = "Cap. Ñ VI, v. 41. 3 Segun el texto hebreo"
    assert ornatos.limpia(texto) == (texto, [])


def test_verso_entero_de_ornato_se_conserva():
    assert ornatos.limpia(ORNATO) == (ORNATO, [])


def test_pocas_fichas_no_bastan():
    texto = "los Al pozos Il Ú marcharemos"
    assert ornatos.limpia(texto) == (texto, [])


def test_idempotente():
    texto = f"uno dos tres {ORNATO} cuatro cinco"
    una, _ = ornatos.limpia(texto)
    assert ornatos.limpia(una) == (una, [])


if __name__ == "__main__":
    for nombre, f in sorted(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
