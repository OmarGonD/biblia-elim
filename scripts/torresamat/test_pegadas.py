"""Pruebas de las particiones de Mateo 5 y de los falsos positivos conocidos."""
import collections
from pegadas import (aplicar, parte_corpus, sino_condicional, sies_si_es,
                     sin_tildes)

def _big(*pares, n=100):
    c = collections.Counter()
    for a, b in pares:
        c[(sin_tildes(a.lower()), sin_tildes(b.lower()))] = n
    return c


def test_mateo5():
    big = _big(
        ("yo", "os"), ("que", "con"), ("si", "es"), ("si", "la"),
        ("ni", "se"), ("ni", "tampoco"), ("si", "no"), ("no", "amais"),
        n=80,
    )
    voc = set()
    frec = {"yoos": 11, "quecon": 1, "sies": 5, "sila": 1,
            "nise": 8, "nitampoco": 3, "sino": 1000}
    assert parte_corpus("Yoos", big, voc, frec) == "Yo os"
    assert parte_corpus("Quecon", big, voc, frec) == "Que con"
    assert parte_corpus("Nise", big, voc, frec) == "Ni se"
    assert parte_corpus("Nitampoco", big, voc, frec) == "Ni tampoco"
    assert parte_corpus("sila", big, voc, frec) == "si la"
    t = ("Y sies tu mano derecha la que te sirve. "
         "Que sino amais sino á los que os aman.")
    nv, ch = aplicar(t, big, voc, frec)
    assert "si es tu mano" in nv
    assert "si no amais" in nv
    assert "sino á los" in nv  # la conjunción se queda


def test_no_rompe_morfologia():
    big = _big(("con", "traer"), ("su", "puesto"), ("se", "parándose"),
               ("a", "qué"), n=80)
    voc = set()
    frec = {"contraer": 4, "supuesto": 21, "separándose": 5, "aqué": 9}
    assert parte_corpus("contraer", big, voc, frec) is None
    assert parte_corpus("supuesto", big, voc, frec) is None
    assert parte_corpus("separándose", big, voc, frec) is None
    # "aqué" es aquí/aquél del OCR, no "a qué"
    assert parte_corpus("aqué", big, voc, frec) is None


def test_enclitico_de_verbo():
    big = _big(("crió", "los"), ("yo", "os"), n=50)
    voc = set()
    frec = {"criólos": 3, "yoos": 11}
    assert parte_corpus("criólos", big, voc, frec) is None
    assert parte_corpus("Yoos", big, voc, frec) == "Yo os"


def test_cuatro_trozos():
    big = _big(("a", "lo"), ("lo", "que"), ("que", "contestó"), n=50)
    voc = {"contestó", "contestó".lower()}
    frec = {"aloquecontestó": 1}
    assert parte_corpus("Aloquecontestó", big, voc, frec) == "A lo que contestó"


def test_sino_y_sies_contexto():
    assert sino_condicional("Que", "amais")
    assert sino_condicional("mas", "quieres")
    assert not sino_condicional("nada", "á")
    assert not sino_condicional("no", "que")
    assert sies_si_es("tu")
    assert sies_si_es("la")
    assert not sies_si_es("siempre")


if __name__ == "__main__":
    test_mateo5()
    test_no_rompe_morfologia()
    test_enclitico_de_verbo()
    test_cuatro_trozos()
    test_sino_y_sies_contexto()
    print("ok")
