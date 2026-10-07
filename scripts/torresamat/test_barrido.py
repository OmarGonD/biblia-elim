"""Barrido ortográfico de Torres Amat.

    python3 test_barrido.py

Mt 28:19 salió «Td pues, é instruid…»: la «I» de «Id» leída como «T». El
barrido sustituye, palabra entera, las formas de barrido_palabras.tsv.
"""
import re

import barrido

TABLA = barrido.lee_tabla(barrido.TABLA)


def corrige(texto):
    return barrido.corrige(texto, TABLA)[0]


def test_mateo_28_19():
    assert corrige("Td pues, é instruid á todas las naciones") == \
        "Id pues, é instruid á todas las naciones"


def test_palabra_entera_y_caja():
    # «dle» -> «de» no entra en «dlexxx»; la mayúscula se traslada.
    assert corrige("dle Mesopotamia, Dle y dlexxx") == \
        "de Mesopotamia, De y dlexxx"


def test_marcado_intacto():
    texto = '<note n="dle">dle</note>'
    assert corrige(texto) == '<note n="dle">de</note>'


def test_cifra_pegada():
    assert corrige("el 4ngel del Señor") == "el ángel del Señor"
    assert corrige("al 5eñor Dios") == "al Señor Dios"


def test_a_pegada_se_parte():
    assert corrige("Convocóá todos") == "Convocó á todos"


def test_cero_suelto_es_o_disyuntiva():
    assert corrige("en los canales 0 bebederos") == "en los canales ó bebederos"
    assert corrige("tenia 10 años") == "tenia 10 años"


def test_punto_tras_articulo_ante_nombre_propio():
    assert corrige("hasta el mar de la. Palestina") == "hasta el mar de la Palestina"
    assert corrige("respondió el. Caminaron") == "respondió el. Caminaron"
    assert corrige("dijo á la gente. Y fué") == "dijo á la gente. Y fué"


def test_palabra_gramatical_repetida():
    assert corrige("subirás á á presentarte y y le dirás") == \
        "subirás á presentarte y le dirás"
    assert corrige("mil y mil") == "mil y mil"


def test_frase_anclada_al_verso():
    frases = {"Mark 1:7": [("digno vi de", "digno ni de")]}
    assert barrido.aplica_frases("Mark 1:7", "no soy digno vi de x", frases) == \
        ("no soy digno ni de x", 1)
    assert barrido.aplica_frases("Mark 1:7", "no soy digno ni de x", frases) == \
        ("no soy digno ni de x", 0)
    assert barrido.aplica_frases("Mark 1:8", "digno vi de", frases) == \
        ("digno vi de", 0)


def test_corte_por_inicio_y_fin():
    cortes = {"Gen 1:1": [("Nota uno", "fin nota.")]}
    t = "Texto del verso. Nota uno que sobra fin nota. Sigue el verso."
    assert barrido.aplica_cortes("Gen 1:1", t, cortes) == \
        ("Texto del verso. Sigue el verso.", 1)
    assert barrido.aplica_cortes("Gen 1:1", "Texto del verso. Sigue el verso.",
                                 cortes)[1] == 0


def test_nombre_propio_exacto():
    # Con mayúscula inicial la clave vale solo así: «Dayid» -> «David».
    assert corrige("Dayid") == "David"
    assert corrige("dayid") == "dayid"


def test_solo_palabras():
    assert barrido.solo_palabras("Td pues <x/>", "Id pues <x/>")
    assert barrido.solo_palabras("queá", "que á")
    assert not barrido.solo_palabras("Td pues", "Id pues <x/>")
    assert not barrido.solo_palabras("Td 4", "Id 5")


def test_tabla_coherente():
    exactas, sin_caja = TABLA
    for destino in (exactas, sin_caja):
        for malo, bueno in destino.items():
            assert malo != bueno
            # una pasada basta: la corrección no vuelve a ser una forma mala
            assert bueno not in exactas and bueno.lower() not in sin_caja, \
                (malo, bueno)
            assert re.fullmatch(r"[0-9]{0,2}[A-Za-zÁÉÍÓÚÜÑáéíóúüñ]+", malo)


def test_idempotente():
    texto = "Td pues, Dayid en Esypto; el 4ngel y la sepulero."
    una = corrige(texto)
    assert corrige(una) == una


if __name__ == "__main__":
    pruebas = [f for n, f in sorted(globals().items())
               if n.startswith("test_")]
    for f in pruebas:
        f()
        print("ok", f.__name__)
