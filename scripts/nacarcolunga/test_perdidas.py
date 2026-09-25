"""Regresiones de NACAR-OCR-106: el parser registra la pérdida estructural.

Casos del facsímil: Ap 11:12 «enemi- | gos. ¹³» (Princeton 1495), Jos 5:1
«*7 amorreos» (319, la inicial «5» leída a trozos), 1 Tes 3:7 «gran con- |
[suelo] por vuestra fe» (1363, borde recortado junto al medianil).
Controles: la inicial grande del capítulo, el desborde de Job, la nota al
pie colada, un verso completo.

Ejecutar desde este directorio:

    python3 test_perdidas.py
"""
from construir import no_ensamblados, registra_perdidas, separa_perdidas
from versiculos import _sucesos_linea, corriente

ORIGEN = {"pagina": 1, "columna": 0}


def renglon(texto, top, x1=855, x2=None, alto=36):
    palabras = texto.split()
    x2 = x2 or 1543
    paso = (x2 - x1) / max(1, len(palabras))
    return [(int(x1 + k * paso), int(x1 + (k + 1) * paso) - 8, top,
             top + alto, 90, p) for k, p in enumerate(palabras)]


def columna(*spec):
    """spec: (texto, x1) con interlineado normal."""
    return [renglon(t, 100 + 38 * i, x1) for i, (t, x1) in enumerate(spec)]


RELLENO = [("renglón de relleno para medir la columna", 855)] * 6


def tipos(ev):
    return [(s[1], s[2]) for s in ev if s[0] == "perdida"]


def test_fragmento_quitado_antes_de_la_marca():
    ev = _sucesos_linea(renglon("gos. 13 Y en aquella hora", 0),
                        "gos. 13 Y en aquella hora", None, ORIGEN)
    assert ev[0][:3] == ("perdida", "fragmento_descartado", "gos.")
    assert ev[1][:3] == ("vers", 13, "Y en aquella hora")


def test_marca_que_solo_aparece_quitando_basura():
    ev = _sucesos_linea(renglon("*7 amorreos, a occidente", 0),
                        "*7 amorreos, a occidente", None, ORIGEN)
    assert ev[0][:3] == ("perdida", "marca_tras_basura", "*")
    assert ev[1][:2] == ("vers", 7)


def test_renglon_descartado_queda_registrado():
    ev = _sucesos_linea(renglon("Circuncisión. |", 0), "Circuncisión. |",
                        None, ORIGEN)
    assert ev[0][:2] == ("perdida", "descarte:titulo")
    # Sin letras no es texto: no se registra.
    assert _sucesos_linea(renglon("| —", 0), "| —", None, ORIGEN) == []


def test_borde_recortado_tras_palabra_partida():
    col = columna(*RELLENO,
                  ("vosotros, 7 hemos recibido gran con-", 884),
                  ("por vuestra fe en medio de", 971),
                  ("nuestras necesidades y tribu-", 975),
                  ("Ahora ya vivimos, sabiendo", 1023))
    assert tipos(corriente(col, {"1Thess"}, ORIGEN)) == [
        ("borde_perdido", "por vuestra fe en medio de"),
        ("borde_perdido", "Ahora ya vivimos, sabiendo")]


def test_sangrias_que_no_son_recorte():
    # Verso 1 junto a la inicial grande del capítulo.
    col = columna(*RELLENO, ("1 Palabras de Amós, de los pas-", 1000),
                  ("tores de Tecua, de la visión que", 1000))
    assert tipos(corriente(col, {"Amos"}, ORIGEN)) == []
    # Desborde de Job y nota al pie colada.
    col = columna(*RELLENO, ("Puesto que Job dice: «Yo soy ino-", 855),
                  ("[cente,", 1400))
    assert tipos(corriente(col, {"Job"}, ORIGEN)) == []
    col = columna(*RELLENO, ("embriagante; no comerá uvas, ni fres-", 855),
                  ("(1) Esta consagración personal, singularí-", 950))
    assert tipos(corriente(col, {"Num"}, ORIGEN)) == []
    # Sin palabra partida la sangría no dice nada.
    col = columna(*RELLENO, ("el que habita bajo la protección", 855),
                  ("| del Altísimo, que mora a la", 971))
    assert tipos(corriente(col, {"Ps"}, ORIGEN)) == []


def _registro(sucesos, textos):
    ev, senales = separa_perdidas(sucesos)
    assert not any(s[0] == "perdida" for s in ev)
    import importlib.util
    import os
    spec = importlib.util.spec_from_file_location(
        "alinear_ta", os.path.join(os.path.dirname(__file__), "..",
                                   "torresamat", "alinear.py"))
    al = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(al)
    vers, _, proc = al.ensambla(ev, [{"osis": "Josh", "versos": [9]}])
    senales += no_ensamblados(ev, vers)
    return registra_perdidas(ev, senales, proc, textos), vers


def o(top):
    return {"pagina": 319, "columna": 0, "top": top}


def test_registro_por_verso():
    ev = [("libro", "Josh", None), ("cap", 5, {"Josh"}),
          ("vers", 1, "Cuando todos los reyes de los", {"Josh"}, True, o(1)),
          ("perdida", "marca_tras_basura", "*", {"Josh"}, o(2)),
          ("vers", 7, "amorreos, a occidente del Jordán,", {"Josh"}, True,
           o(2)),
          ("vers", 2, "Entonces dijo Yave a Josué:", {"Josh"}, True, o(3)),
          ("vers", 3, "Hízose Josué cuchillos de pie-", {"Josh"}, True,
           o(4)),
          ("perdida", "fragmento_descartado", "dra", {"Josh"}, o(5)),
          ("perdida", "fragmento_descartado", "ABiOs,", {"Josh"}, o(5)),
          ("vers", 4, "La causa de esta circuncisión", {"Josh"}, True,
           o(5)),
          ("perdida", "borde_perdido", "todos los varones", {"Josh"}, o(6)),
          ("sigue", "todos los varones", {"Josh"}, o(6)),
          ("vers", 5, "Todo el pueblo salido estaba circuncidado.",
           {"Josh"}, True, o(7))]
    textos = {"Josh 1:1": "Cuando todos los reyes de los",
              "Josh 1:2": "Entonces dijo Yave a Josué:",
              "Josh 1:3": "Hízose Josué cuchillos de pie-",
              "Josh 1:4": "La causa de esta circuncisión todos los varones",
              "Josh 1:5": "Todo el pueblo salido estaba circuncidado.",
              "Josh 1:7": "amorreos, a occidente del Jordán,"}
    reg, _ = _registro(ev, textos)
    t = {k: sorted({x["tipo"] for x in v["senales"]}) for k, v in reg.items()}
    assert reg["Josh 1:1"]["truncado"]
    assert "continuacion_desplazada" in t["Josh 1:1"]
    assert "contenido_desplazado" in t["Josh 1:7"]
    assert reg["Josh 1:3"]["truncado"]
    assert set(t["Josh 1:3"]) >= {"guion_final", "fragmento_descartado",
                                  "descarte:basura"}
    # Pérdida probable, no truncado.
    assert reg["Josh 1:4"]["perdida_probable"]
    assert not reg["Josh 1:4"]["truncado"]
    # Verso completo: sin entrada.
    assert "Josh 1:5" not in reg and "Josh 1:2" not in reg
    # La geometría de los renglones queda para volver al facsímil.
    assert [x["top"] for x in reg["Josh 1:4"]["renglones"]] == [5, 6]


def test_renglon_que_el_alineador_no_coloca_es_solo_evidencia():
    ev = [("libro", "Josh", None), ("cap", 5, {"Josh"}),
          ("sigue", "de una introducción suelta", {"Josh"}, o(1)),
          ("vers", 1, "Cuando todos los reyes.", {"Josh"}, True, o(2)),
          ("vers", 2, "Entonces dijo Yave.", {"Josh"}, True, o(3))]
    reg, vers = _registro(ev, {"Josh 1:1": "Cuando todos los reyes.",
                               "Josh 1:2": "Entonces dijo Yave."})
    assert not any("introducción" in t for v in vers.values() for t in v)
    assert all(not v["truncado"] and not v["perdida_probable"]
               for v in reg.values())


if __name__ == "__main__":
    test_fragmento_quitado_antes_de_la_marca()
    test_marca_que_solo_aparece_quitando_basura()
    test_renglon_descartado_queda_registrado()
    test_borde_recortado_tras_palabra_partida()
    test_sangrias_que_no_son_recorte()
    test_registro_por_verso()
    test_renglon_que_el_alineador_no_coloca_es_solo_evidencia()
    print("ok")
