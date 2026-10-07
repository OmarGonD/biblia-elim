"""Regresiones semánticas: distinguir á, ó, de y la conjunción ú válida."""
import preposiciones

FRASES = preposiciones.lee_frases(preposiciones.TABLA)


def test_marcos_2_17_y_anclaje():
    antes = ("Habiéndolo oido Jesus les dijo: Los que están buenos no necesitan "
             "de médico, sino los que están enfermos: así yo no he venido á "
             "llamar d convertir á los justos, sino á los pecadores.")
    esperado = antes.replace("llamar d convertir", "llamar á convertir")
    nuevo, n = preposiciones.corrige("Mark 2:17", antes, FRASES)
    assert nuevo == esperado and n > 0
    assert preposiciones.corrige("Mark 2:17", nuevo, FRASES) == (nuevo, 0)
    assert preposiciones.corrige("Mark 2:18", antes, FRASES) == (antes, 0)


def test_d_como_preposicion():
    t = ("Los criados y ministros que habian ido d prender d Jesus estaban á la "
         "lumbre, porque hacia frio, y se calentaban: Pedro estaba tambien con "
         "ellos á la lumbre, calentándose.")
    nuevo, n = preposiciones.corrige("John 18:18", t, FRASES)
    assert nuevo == t.replace("ido d prender d Jesus", "ido á prender á Jesus") and n > 0


def test_d_como_explicacion_y_marcado():
    t = ("Príncipe de Magdiel, principe de Hiram: estos son los principes de "
         "Edom d Iduméa moradores cada cual en la tierra de su mando: Edom es "
         "el mismo Esaú padre de los Tdumeéos. "
         '<chapter eID="gen992" osisID="Gen.36"/>')
    nuevo, n = preposiciones.corrige("Genesis 36:43", t, FRASES)
    assert nuevo == t.replace("Edom d Iduméa", "Edom ó Iduméa") and n > 0


def test_de_por_contexto():
    t = ("No te postres delante de ellos, ni les sirvas: porque Yo soy el Señor "
         "Dios tuyo, Dios celoso, que castigo la maldad de los padres en los "
         "hijos hasta la tercera y cuarta generacion, d aquellos, digo, que me "
         "aborrecen")
    nuevo, n = preposiciones.corrige("Exodus 20:5", t, FRASES)
    assert nuevo == t.replace("generacion, d aquellos", "generacion, de aquellos") and n > 0


def test_conjuncion_u_valida_no_cambia():
    t = "Las libaciones ú ofrendas de vino que se han de derramar por cada víctima"
    assert preposiciones.corrige("Numbers 28:14", t, FRASES) == (t, 0)


def test_fragmento_ambiguo_no_se_reconstruye():
    t = ("Antes del juicio d de al juez, asegúrate de tu Justicia, "
         "y antes que hables aprende.")
    assert preposiciones.corrige("Sirach 18:19", t, FRASES) == (t, 0)


def test_texto_diferente_se_rechaza():
    try:
        preposiciones.corrige("Mark 2:17", "Otro texto.", FRASES)
    except SystemExit:
        pass
    else:
        raise AssertionError("debe rechazar un texto ajeno al fragmento revisado")


if __name__ == "__main__":
    for n, f in sorted(globals().copy().items()):
        if n.startswith("test_"):
            f()
            print("ok", n)
