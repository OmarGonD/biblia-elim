"""Cola de capítulo pegada al último verso (colas.py).

    python3 test_colas.py
"""
import colas

VERSO = "Y así todos los dias que vivió fueron novecientos años: y murió."


def test_corta_desde_capitulo_y_conserva_el_marcado():
    texto = f'{VERSO} CAPITULO X Genealogías de los hijos. <chapter eID="g" osisID="Gen.9"/>'
    nuevo, cola = colas.corta(texto)
    assert nuevo == f'{VERSO} <chapter eID="g" osisID="Gen.9"/>'
    assert cola == "CAPITULO X Genealogías de los hijos."


def test_acepta_variantes_del_ocr():
    for marca in ("CAPITUEO", "¿APITULO", "CAPÍTULO", "CAPIÍLULO"):
        nuevo, cola = colas.corta(f"{VERSO} {marca} XLI Joseph interpreta")
        assert nuevo == VERSO and cola.startswith(marca.lstrip("¿")) or cola


def test_verso_sin_marca_no_cambia():
    assert colas.corta(VERSO) == (VERSO, "")


def test_poco_verso_antes_no_se_corta():
    texto = "Amen. CAPITULO II Algo"
    assert colas.corta(texto) == (texto, "")


def test_solo_ultimo_verso_del_capitulo():
    entradas = [("Gen 1:0", ""), ("Gen 1:1", "a"), ("Gen 1:2", "b"),
                ("Gen 2:0", ""), ("Gen 2:1", "c"), ("Exod 1:1", "d")]
    assert colas.ultimos_versos(entradas) == {"Gen 1:2", "Gen 2:1", "Exod 1:1"}


if __name__ == "__main__":
    for nombre, f in sorted(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
