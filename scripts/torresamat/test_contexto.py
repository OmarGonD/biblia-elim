"""Regresiones del parche contextual, incluyendo marcado y rechazos."""
import contexto

FRASES = contexto.lee_frases(contexto.TABLA)


def test_marcos_y_segunda_pasada():
    antes = ("Y diciendo: $e ha cumplido ya el tiempo, y el reino de Dios está "
             "cerca: haced penitencia, y creed al Evangelio. «")
    esperado = ("Y diciendo: se ha cumplido ya el tiempo, y el reino de Dios está "
                "cerca: haced penitencia, y creed al Evangelio.")
    nuevo, n = contexto.corrige("Mark 1:15", antes, FRASES)
    assert nuevo == esperado and n > 0
    assert contexto.corrige("Mark 1:15", nuevo, FRASES) == (esperado, 0)
    assert contexto.corrige("Mark 1:16", antes, FRASES) == (antes, 0)


def test_condicional_y_afirmacion():
    texto = "Antorcha de tu cuerpo son tus ojos. $1 tu ojo estuviere puro y sano,"
    assert contexto.corrige("Luke 11:34", texto, FRASES)[0] \
        == "Antorcha de tu cuerpo son tus ojos. Si tu ojo estuviere puro y sano,"
    texto = "$1, oh Señor Dios de Israél, confírmense hoy tus promesas"
    assert contexto.corrige("I Kings 8:26", texto, FRASES)[0] \
        == "Sí, oh Señor Dios de Israél, confírmense hoy tus promesas"


def test_marcos_1_39_iba_y_segunda_pasada():
    antes = ("Tba pues Jesus predicando en sus synagogas, y por toda la Galiléa, "
             "y expelia los demonios.")
    esperado = antes.replace("Tba", "Iba", 1)
    assert contexto.corrige("Mark 1:39", antes, FRASES) == (esperado, 1)
    assert contexto.corrige("Mark 1:39", esperado, FRASES) == (esperado, 0)
    assert contexto.corrige("Mark 1:40", antes, FRASES) == (antes, 0)


def test_marcos_1_45_no_y_segunda_pasada():
    antes = ("Mas aquel hombre, así que se fué, comenzó á hablar de su curacion, "
             "y á publicarla por todas partes, de modo que ya ho podia Jesus "
             "entrar manifiestamente en la ciudad, sino que andaba fuera por "
             "lugares solitarios, y acudian á él de todas partes")
    esperado = antes.replace("ya ho podia", "ya no podia", 1)
    assert contexto.corrige("Mark 1:45", antes, FRASES) == (esperado, 1)
    assert contexto.corrige("Mark 1:45", esperado, FRASES) == (esperado, 0)
    assert contexto.corrige("Mark 1:44", antes, FRASES) == (antes, 0)


def test_pronombre_por_contexto():
    t = "Cifra tus delicias en el Señor, y te otorgará cuanto desea $uU corazon."
    assert contexto.corrige("Psalms 36:4", t, FRASES)[0] \
        == "Cifra tus delicias en el Señor, y te otorgará cuanto desea tu corazon."


def test_marcado_y_ortografia_antigua():
    texto = ('Príncipe de Magdiel, principe de Hiram: estos son los principes de '
             'Edom d Iduméa moradores cada cual en la tierra de su mando: £dom es '
             'el mismo Esaú padre de los Tdumeéos. '
             '<chapter eID="gen992" osisID="Gen.36"/>')
    nuevo, n = contexto.corrige("Genesis 36:43", texto, FRASES)
    assert nuevo == texto.replace("£dom", "Edom") and n > 0
    # La tabla solo resuelve las erratas revisadas: deja las dudosas intactas.
    assert "Tdumeéos" in nuevo


def test_conflicto_no_se_adivina():
    try:
        contexto.corrige("Mark 1:15", "Un texto diferente.", FRASES)
    except SystemExit:
        pass
    else:
        raise AssertionError("debe rechazar un texto que no coincide con el parche")


def test_protege_atributos_osis():
    try:
        contexto.corrige("Mark 1:15", '<note n="$e">nota</note>',
                         {"Mark 1:15": [('$e', 'se')]})
    except ValueError:
        pass
    else:
        raise AssertionError("no puede cambiar un atributo OSIS")


if __name__ == "__main__":
    for n, f in sorted(globals().copy().items()):
        if n.startswith("test_"):
            f()
            print("ok", n)
