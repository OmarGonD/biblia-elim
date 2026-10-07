"""Ortografía y ruido de las notas de Torres Amat (notas_barrido.py).

    python3 test_notas.py
"""
import barrido
import notas_barrido as nb

TABLA = nb.une_tablas(barrido.TABLA, nb.TABLA_NOTAS)


def limpia(p):
    return nb.limpia_parrafo(p, TABLA)[0]


def test_corrige_palabras_de_las_dos_tablas():
    assert limpia("Esto es conjeburas, tá eres el Hijo.") == "Esto es conjeturas, tú eres el Hijo."


def test_cita_con_corchete():
    assert limpia("[sai XL, v. 3.—Malach, HIT, vu. L.").startswith("Isai. XL, v. 3.")


def test_quita_el_numero_de_llamada():
    assert limpia("2 Esta es la significacion del verbo.") == "Esta es la significacion del verbo."


def test_citas_con_romano_mal_leido():
    assert limpia("De mi poder. Levit. ATV, v. 2.") == "De mi poder. Levit. XIV, v. 2."
    assert nb.limpia_parrafo("Malach, HIT, vu. 1.", TABLA)[0] == "Malach. III, v. 1."
    assert nb.limpia_parrafo("Cap. XL, v. 3", TABLA, "Mark")[0] == "Cap. XL, v. 3"   # ya es romano
    assert nb.limpia_parrafo("IL. Reg. IX, w. 5", TABLA)[0] == "II. Reg. IX, v. 5"
    assert nb.limpia_parrafo("Ps. XXXTL 0.6 y 4ct. X1V,v. 14.", TABLA)[0] == "Ps. XXXII, v. 6 y Act. XIV, v. 14."


def test_griego_leido_por_la_pagina_se_conserva():
    lect = [["eftdvat,", "ἐξιστάναι,", 56, 84]]
    p = nb.pon_griego("verbo griego eftdvat, del cual viene", lect)
    assert p == "verbo griego ἐξιστάναι, del cual viene"
    assert nb.limpia_parrafo(p, TABLA)[0] == "verbo griego ἐξιστάναι, del cual viene"
    # el griego de una sola palabra al final no es una cola de ornato
    assert nb.limpia_parrafo("significa convulsion. ἐξιστάναι", TABLA)[0] == "significa convulsion. ἐξιστάναι"


def test_cola_de_ornato_tras_el_final():
    assert limpia("significa convulsion. A Y") == "significa convulsion."
    assert limpia("Véase Mitra, Cap. XXIV, v. 2.") == "Véase Mitra, Cap. XXIV, v. 2."


def test_parrafo_de_ruido_se_descarta_pero_la_cita_no():
    assert limpia("Eddie y A &lt;= p") is None
    assert limpia("Malach. TIT, v. 1.") == "Malach. III, v. 1."


def test_idempotente():
    p = "La palabra cx goyud:, de la cual viene espasmos, significa convulsion. ) _ === A — 1.8 |], LAA == AA A Ak;"
    una = limpia(p)
    assert limpia(una) == una


def test_entrada_conserva_la_nota_del_capitulo():
    c = "<p><i>Notas al capítulo 1.</i></p> <p>Eddie y A &lt;= p</p> <p>Texto bueno de la nota.</p>"
    nuevo, _ = nb.limpia_entrada(c, TABLA)
    assert nuevo == "<p><i>Notas al capítulo 1.</i></p> <p>Texto bueno de la nota.</p>"


def test_reparte_notas_por_hojas_y_huecos():
    orden = [f"Gen 1:{i}" for i in range(1, 11)] + [f"Gen 2:{i}" for i in range(1, 6)]
    g = [{"desde": "Gen 1:1", "hasta": "Gen 1:4", "notas": ["a"]},
         {"desde": None, "hasta": None, "notas": ["b"]},
         {"desde": "Gen 1:7", "hasta": "Gen 2:2", "notas": ["c"]}]
    tramos, rangos = nb.reparte(g, orden)
    assert tramos[("Gen", 1)] == [[1, 4, [0]], [5, 6, [1]], [7, 10, [2]]]
    assert tramos[("Gen", 2)] == [[1, 2, [2]]]
    assert rangos[1][2] is True      # el hueco es ubicación aproximada


if __name__ == "__main__":
    for nombre, f in sorted(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
