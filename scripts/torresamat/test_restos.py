"""Fusiones de Mateo 5 y erratas de letra."""
from fusion import mejor_corte, limpia_lado, RE_CORTE
from erratas import aplicar_texto, PALABRAS, inicial_yv_nm


def test_cortes_mateo5():
    casos = [
        ("Asegúrote de cierto, que de allí no saldrás, hasta que pagues "
         "el último maravedí. Habeis oido que se dijo á vuestros mayores: "
         "No cometerás adulterio.",
         "En verdad te digo, que no saldrás de allí sin que hayas pagado "
         "hasta el último centavo.",
         "Oísteis que fue dicho: No cometerás adulterio.",
         "Habeis oido"),
        ("Pero yo os digo: que cualquiera que despidiere á su mujer, si no "
         "es por causa de adulterio, la expone á ser adúltera; y el que se "
         "casare con la repudiada, es asimismo adúltero, Tambien habeis oido "
         "que se dijo á vuestros mayores: No jurarás en falso; antes bien "
         "cumplirás los juramentos hechos al Señor",
         "Mas Yo os digo: Quienquiera repudie a su mujer, si no es por causa "
         "de fornicación, se hace causa de que se cometa adulterio.",
         "Oísteis también que fue dicho a los antepasados: No perjurarás, "
         "sino que cumplirás al Señor lo que has jurado.",
         "Tambien habeis"),
        ("porque es la ciudad ó corte del gran rey: Ni tampoco jurarels "
         "por yuestra cabeza, pues no está en vuestra mano el hacer blanco "
         "ó negro un solo cabello.",
         "ni por Jerusalén, porque es la ciudad del gran Rey.",
         "Ni jures tampoco por tu cabeza, porque eres incapaz de hacer "
         "blanco o negro uno solo de tus cabellos.",
         "Ni tampoco"),
    ]
    for ta, prev, this, arranque in casos:
        pos = mejor_corte(ta, prev, this)
        assert pos is not None, ta[:40]
        assert arranque in ta[pos:], (ta[pos:pos+40], arranque)


def test_erratas_mateo5():
    voc = {"vuestra", "jurareis", "jureis", "perezca", "algun", "alguno",
           "cielos", "vallado", "llamó", "verá", "verán", "ni"}
    frec = {w: 10 for w in voc}
    t = ("Ni tampoco jurarels por yuestra cabeza. "
         "mejor te está que perezeca uno. "
         "pretende de tí aleun préstamo. "
         "verán á4 Dios. sí, sí: 0 no, no. "
         "pacíficos 1: porque ellos. prójimo Y, y odio.")
    nv, ch = aplicar_texto(t, voc, frec)
    assert "jurareis" in nv
    assert "vuestra" in nv
    assert "perezca" in nv
    assert "algun préstamo" in nv
    assert "á4" not in nv
    assert "ó no" in nv
    assert "pacíficos:" in nv
    assert "prójimo Y," not in nv


def test_yuestra_no_toca_yerno():
    voc = {"yerno", "vuestra", "lista", "hasta"}
    assert inicial_yv_nm("yerno", voc) is None
    assert inicial_yv_nm("yuestra", voc) == "vuestra"
    nv, _ = aplicar_texto("desde el reptil liasta las aves", voc, {"lista": 20})
    assert "liasta" in nv  # no es "lista": es "hasta" mal leído, otra clase


if __name__ == "__main__":
    test_cortes_mateo5()
    test_erratas_mateo5()
    test_yuestra_no_toca_yerno()
    print("ok")
