"""Regresiones de NACAR-OCR-105: epígrafes de salmo aparte del verso anterior.

Casos de Princeton 1020 (Sal 117/118 «Canto triunfal.»), 967 (Sal 14
«Seguridad del justo… | impíos.»), 1018 (Sal 110, título sin «1» tras el
epígrafe), 965 (Sal 9, cabecera «9» sola), 986 (Sal 51-52, «21» suelto) y
962 (Sal 1, epígrafe delante del primer verso del libro).

Ejecutar desde este directorio:

    python3 test_epigrafes_salmo.py
"""
import importlib.util
import os

from construir import asigna_epigrafes, separa_epigrafes
from front_matter import separa_front_matter
from versiculos import corriente

DIR = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location(
    "alinear_ta", os.path.join(DIR, "..", "torresamat", "alinear.py"))
alinear = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(alinear)


def renglon(texto, top, alto=36):
    out, x = [], 70
    for p in texto.split():
        out.append((x, x + 17 * len(p), top, top + alto, 90, p))
        x += 17 * len(p) + 12
    return out


def columna(*spec):
    """spec: (texto, blanco sobre el renglón anterior)."""
    out, top = [], 100
    for t, hueco in spec:
        if out:
            top = out[-1][0][3] + hueco
        out.append(renglon(t, top))
    return out


ORIGEN = {"pagina": 1020, "columna": 1}

SAL_117_118 = columna(
    ("relleno para medir la altura del renglón", 0),
    ("1 Alabad a Yave las gentes todas, |", 2),
    ("alabadle todos los pueblos (1).", 2),
    ("2 Porque claramente se ha mani-", 2),
    ("nidad. | ¡Aleluya!", 2),
    ("118. (Vulg. 117.)", 90),
    ("Canto triunfal.", 37),
    ("1 Alabad a Yave, porque es bueno,", 66),
    ("porque es eterna su misericordia (2).", 2),
)


def test_canto_triunfal_no_se_pega_a_117_2():
    ev = corriente(SAL_117_118, {"Ps"}, ORIGEN)
    assert [s[1] for s in ev if s[0] == "epigrafe"] == ["Canto triunfal."]
    assert not any(s[0] == "sigue" and "Canto triunfal" in s[1] for s in ev)


def test_epigrafe_se_ata_al_salmo_que_encabeza():
    ev = [("libro", "Ps", None)] + corriente(SAL_117_118, {"Ps"}, ORIGEN)
    ev, epis = separa_epigrafes(ev)
    assert not any(s[0] == "epigrafe" for s in ev)
    vers, avisos, proc = alinear.ensambla(
        ev, [{"osis": "Ps", "versos": [2, 29]}])
    epi = asigna_epigrafes(epis, proc, avisos)
    assert epi[("Ps", 2)][0] == "Canto triunfal."
    assert avisos == []
    assert vers[("Ps", 1, 2)][-1] == "nidad. | ¡Aleluya!"
    assert vers[("Ps", 2, 1)][0].startswith("Alabad a Yave, porque")


def test_epigrafe_de_dos_renglones_no_se_pierde():
    # El primer renglón lo tomaba es_titulo() por título y se perdía; el
    # segundo acababa en Sal 13:6 («…«Bien me proveyó.» impíos.»).
    col = columna(
        ("a Yave: «Bien me proveyó.»", 0),
        ("14 (Vulg. 13.)", 73),
        ("Seguridad del justo en el castigo de los", 42),
        ("impíos.", 4),
        ("1 Al maestro del coro. De David.", 26),
        ("Dice en su corazón el necio: «No", 7),
    )
    ev = corriente(col, {"Ps"}, ORIGEN)
    assert [s[1] for s in ev if s[0] == "epigrafe"] == [
        "Seguridad del justo en el castigo de los", "impíos."]
    assert not any(s[0] == "sigue" and "impíos" in s[1] for s in ev)
    # La continuación del verso 1 sigue en el cuerpo (NACAR-OCR-103).
    assert any(s[0] == "sigue" and "Dice en su corazón" in s[1] for s in ev)


def test_lo_que_sigue_al_blanco_no_es_epigrafe():
    # Sal 110: el título «Salmo de David.» cuyo «1» no leyó el OCR va
    # después del blanco; no es parte del epígrafe.
    col = columna(
        ("relleno para medir la altura del renglón", 0),
        ("110 (Vulg. 109.)", 60),
        ("El Mesías, rey y sacerdote eterno, según", 30),
        ("el orden de Melquisedec.", 3),
        ("Salmo de David.", 40),
        ("1 Oráculo de Yave a mi Señor:", 3),
    )
    ev = corriente(col, {"Ps"}, ORIGEN)
    assert [s[1] for s in ev if s[0] == "epigrafe"] == [
        "El Mesías, rey y sacerdote eterno, según",
        "el orden de Melquisedec."]
    assert any(s[0] == "sigue" and s[1] == "Salmo de David." for s in ev)


def test_cabecera_de_numero_solo_con_verso_1_detras():
    col = columna(
        ("relleno para medir la altura del renglón", 0),
        ("9", 60),
        ("Dios, juez supremo, que juzga y castiga", 40),
        ("a las gentes y a los impíos de su pueblo.", 3),
        ("1 Al maestro del coro. Al Mut-", 30),
    )
    ev = corriente(col, {"Ps"}, ORIGEN)
    assert len([s for s in ev if s[0] == "epigrafe"]) == 2


def test_numero_de_verso_solo_no_se_lleva_el_texto():
    # Sal 51 (Princeton 986): «21» es el número del verso, solo en su
    # renglón; detrás viene la cabecera del 52. El texto del verso no se
    # pierde ni pasa a epígrafe.
    col = columna(
        ("relleno para medir la altura del renglón", 0),
        ("de Jerusalén.", 2),
        ("21", 2),
        ("ciones y holocaustos. | Entonces", 30),
        ("pondrán becerros en tu altar,", 3),
        ("52 (Vulg. 51.)", 50),
        ("Oración contra un enemigo jactancioso.", 34),
        ("1 y 2 Al maestro del coro, Mas", 30),
    )
    ev = corriente(col, {"Ps"}, ORIGEN)
    sigue = [s[1] for s in ev if s[0] == "sigue"]
    assert "ciones y holocaustos. | Entonces" in sigue
    assert "pondrán becerros en tu altar," in sigue
    assert [s[1] for s in ev if s[0] == "epigrafe"] == [
        "Oración contra un enemigo jactancioso."]


def test_numero_solo_sin_salmo_detras_no_es_cabecera():
    col = columna(
        ("relleno para medir la altura del renglón", 0),
        ("6", 2),
        ("Desbórdanse de sus cubos las aguas", 30),
        ("7 Sus ramas crecen como en aguas", 2),
    )
    ev = corriente(col, {"Ps"}, ORIGEN)
    assert not any(s[0] == "epigrafe" for s in ev)
    assert any(s[0] == "sigue" and "Desbórdanse" in s[1] for s in ev)


def test_epigrafe_del_primer_salmo_no_va_a_la_introduccion():
    ev = [("libro", "Ps", None),
          ("sigue", "Introducción al libro de los Salmos, con", {"Ps"}),
          ("sigue", "muchas líneas de prólogo sin versos.", {"Ps"}),
          ("epigrafe", "Las dos sendas: La del justo y la del", {"Ps"},
           None),
          ("epigrafe", "impío.", {"Ps"}, None),
          ("vers", 1, "Bienaventurado el varón", {"Ps"}, True, None),
          ("vers", 2, "sino que se complace", {"Ps"}, True, None),
          ("vers", 3, "Será como árbol", {"Ps"}, True, None)]
    out, intros = separa_front_matter(ev)
    assert "impío." not in " ".join(intros.get("Ps", []))
    assert [s[1] for s in out if s[0] == "epigrafe"] == [
        "Las dos sendas: La del justo y la del", "impío."]


if __name__ == "__main__":
    test_canto_triunfal_no_se_pega_a_117_2()
    test_epigrafe_se_ata_al_salmo_que_encabeza()
    test_epigrafe_de_dos_renglones_no_se_pierde()
    test_lo_que_sigue_al_blanco_no_es_epigrafe()
    test_cabecera_de_numero_solo_con_verso_1_detras()
    test_numero_de_verso_solo_no_se_lleva_el_texto()
    test_numero_solo_sin_salmo_detras_no_es_cabecera()
    test_epigrafe_del_primer_salmo_no_va_a_la_introduccion()
    print("ok")
