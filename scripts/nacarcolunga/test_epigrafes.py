"""Regresiones de NACAR-OCR-103: es_titulo() no debe tragarse texto bíblico.

Geometría tomada de Princeton 967 (Sal 13-16), 121 (Gn 13), 1007 (Sal 91)
y 1496 (Ap 12): renglones de 36 px con interlineado de 1-7 px; el
epígrafe va aislado por 26-60 px de blanco.

Ejecutar desde este directorio:

    python3 test_epigrafes.py
"""
from versiculos import corriente

IZQ, DER = 70, 770


def renglon(texto, top, x1=IZQ, x2=None, alto=36):
    palabras = texto.split()
    if x2 is None:
        x2 = x1 + 17 * len(texto)
    paso = (x2 - x1) / max(1, len(palabras))
    return [(int(x1 + k * paso), int(x1 + (k + 1) * paso) - 8,
             top, top + alto, 90, p) for k, p in enumerate(palabras)]


def columna(*spec):
    """spec: (texto, hueco_sobre_el_anterior, x1, x2)."""
    out, top = [], 100
    for i, (t, hueco, x1, x2) in enumerate(spec):
        if i:
            top = out[-1][0][3] + hueco
        out.append(renglon(t, top, x1, x2))
    return out


def lleno(t, hueco=2):
    return (t, hueco, IZQ, DER)


def centro(t, hueco):
    ancho = 17 * len(t)
    x1 = IZQ + (DER - IZQ - ancho) // 2
    return (t, hueco, x1, x1 + ancho)


def textos(ev):
    return " ".join(s[2] if s[0] == "vers" else s[1]
                    for s in ev if s[0] in ("vers", "sigue"))


# Sal 16:2-4 (Princeton 967, col. dcha.)
SAL_16 = columna(
    lleno("Algo de relleno para medir la columna"),
    lleno("2 Yo digo a Yave: Mi señor eres"),
    lleno("tú, | no hay bien para mí fuera de ti."),
    lleno("3 Los santos que en la tierra están,"),
    lleno("| son de mí muy honrados, | en ellos"),
    lleno("tengo todas mis delicias."),
    lleno("4 Multiplican sus ídolos los que"),
    lleno("| se van tras los dioses ajenos. | No"),
    lleno("libaré yo sus sangrientas libaciones, |"),
)


def test_continuacion_de_verso_se_conserva():
    t = textos(corriente(SAL_16, {"Ps"}))
    assert "| son de mí muy honrados, | en ellos" in t
    assert "| se van tras los dioses ajenos. | No" in t


def test_sal_13_lineas_recuperadas():
    col = columna(
        lleno("2 ¿Hasta cuándo, por fin, te olvi-"),
        lleno("3 ¿Hasta cuándo mandarás dolo-"),
        lleno("res sobre mi alma | y penas de con-"),
        lleno("*tinuo sobre mi corazón? | ¿Hasta"),
        lleno("euango mis enemigos triunfarán de"),
        lleno("4 ¡Mírame ya, óyeme, Yave, Dios"),
        lleno("6 Que no pueda decir mi enemigo:"),
        lleno("«Le vencí.» | Que mis enemigos se"),
        lleno("regocijarían si yo cayese,"),
    )
    t = textos(corriente(col, {"Ps"}))
    assert "*tinuo sobre mi corazón? | ¿Hasta" in t
    assert "«Le vencí.» | Que mis enemigos se" in t


def test_epigrafe_aislado_sigue_fuera():
    # Sal 13/14: cabecera, epígrafe de dos renglones, verso 1.
    col = columna(
        lleno("con tu socorro, | que pueda cantar"),
        lleno("a Yave: «Bien me proveyó.»"),
        centro("14 (Vulg. 13.)", 73),
        lleno("Seguridad del justo en el castigo de los", 42),
        centro("impíos.", 4),
        lleno("1 Al maestro del coro. De David.", 26),
        lleno("Dice en su corazón el necio: «No", 7),
        lleno("hay Dios.» | Todos obran torpemente,"),
    )
    t = textos(corriente(col, {"Ps"}))
    assert "Seguridad del justo" not in t
    # La continuación del verso 1, pegada a él, sí se conserva.
    assert "Dice en su corazón el necio" in t


def test_epigrafe_pegado_al_primer_verso_del_salmo():
    # Sal 91 (Princeton 1007): 12 px entre epígrafe y verso 1.
    col = columna(
        lleno("relleno de la columna para medir la"),
        lleno("altura normal de los renglones y"),
        centro("91 (Vulg. 90.)", 80),
        lleno("Canto a la providencia de Dios sobre", 48),
        centro("el justo.", 5),
        lleno("1 El que habita bajo la protección", 12),
        lleno("del Altísimo, | que mora a la sombra"),
    )
    t = textos(corriente(col, {"Ps"}))
    assert "Canto a la providencia" not in t
    assert "del Altísimo" in t


def test_titulo_centrado_pegado_al_verso():
    # Gn 13 (Princeton 121): «el nombre de Yave.», blanco, título centrado
    # con coma y filete sueltos, y «⁵ También Lot» a 1 px.
    tit = ", Separación de Abram y Lot. |"
    col = columna(
        lleno("allí alzara al principio, e invocó allí"),
        lleno("otro renglón de relleno para la medida"),
        ("el nombre de Yave.", 1, IZQ, IZQ + 330),
        (tit, 39, IZQ, DER),
        lleno("5 También Lot, que acompañaba", 1),
        lleno("a Abram, tenía rebaños, ganados y"),
    )
    # Las palabras del título van centradas; la coma y el filete, en los
    # bordes, como en el OCR.
    col[3] = ([(IZQ - 70, IZQ - 57, *col[3][0][2:5], ",")]
              + [(x1, x2, t, b, c, w) for (x1, x2, t, b, c, w) in renglon(
                  "Separación de Abram y Lot.", col[3][0][2], 225, 645)]
              + [(DER + 15, DER + 17, *col[3][0][2:5], "|")])
    t = textos(corriente(col, {"Gen"}))
    assert "Separación" not in t
    assert "También Lot" in t


def test_renglon_centrado_con_coma_no_es_titulo():
    # Eclo 51: letanía centrada tras un blanco; acaba en coma y el filete.
    col = columna(
        lleno("porque es eterna su misericordia."),
        lleno("relleno de la columna para medir la"),
        centro("Alabad al escudo de Abraham, |", 30),
        lleno("12 Alabad a la roca de Isac, | porque", 2),
    )
    assert "Alabad al escudo de Abraham" in textos(corriente(col, {"Sir"}))


def test_continuacion_al_pie_con_su_verso():
    # Ap 12:7 (Princeton 1496): título, blanco, verso y su continuación al
    # pie de la columna.
    col = columna(
        lleno("relleno de la columna para medir la"),
        lleno("altura normal de los renglones y"),
        centro("La batalla en el cielo", 69),
        lleno("7 Y hubo una batalla en el cielo (5):", 56),
        lleno("Miguel y sus ángeles peleaban con el", 1),
    )
    t = textos(corriente(col, {"Rev"}))
    assert "La batalla en el cielo" not in t
    assert "Miguel y sus ángeles peleaban con el" in t


if __name__ == "__main__":
    test_continuacion_de_verso_se_conserva()
    test_sal_13_lineas_recuperadas()
    test_epigrafe_aislado_sigue_fuera()
    test_epigrafe_pegado_al_primer_verso_del_salmo()
    test_titulo_centrado_pegado_al_verso()
    test_renglon_centrado_con_coma_no_es_titulo()
    test_continuacion_al_pie_con_su_verso()
    print("ok")
