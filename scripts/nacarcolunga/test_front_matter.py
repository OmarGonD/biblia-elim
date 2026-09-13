"""Regresiones del recorte de front-matter.

Ejecutar desde este directorio:

    python3 test_front_matter.py
"""
from construir import asigna_libro
from front_matter import (
    es_continuacion,
    indice_apertura,
    run_es_apertura,
    separa_front_matter,
)


def V(n, text, start=True, cands=None):
    return ("vers", n, text, cands or {"Ps"}, start, None)


def S(text, cands=None):
    return ("sigue", text, cands or {"Ps"})


def L(osis):
    return ("libro", osis, None)


def C(n, cands=None):
    return ("cap", n, cands)


def test_run_intro_salmos_no_es_apertura():
    # 1 "Las denominaciones", luego 7/8/23: el 1 suelto no abre capítulo.
    assert not run_es_apertura([1])
    assert not run_es_apertura([1, 7, 8])
    assert not run_es_apertura([1, 23, 8, 2, 68])
    assert run_es_apertura([1, 2, 3, 4, 6])
    assert run_es_apertura([1, 2, 3])
    assert run_es_apertura([1, 2, 4, 5, 6, 7])
    assert not run_es_apertura([1, 2])


def test_salmos_intro_se_queda_fuera():
    ev = [
        L("Job"),
        V(17, "y murió Job anciano y más colmado de días.",
          cands={"Job"}),
        L("Ps"),
        S("himmos, salmos, loas, etc. El li los Salmos 1-41."),
        V(89, "Los 1:", start=False),
        V(1, "Las denominaciones de los Salmos de David"),
        S("el principal autor, pues son muchos los Salmos"),
        V(7, "lleva la inscripción: a Yave con ocasión de lo de Cus",
          start=False),
        V(8, "no directamente de los a antigua tradición judía?",
          start=True),
        V(1, "Bienaventurado el varón que no anda en consejo de impíos"),
        V(2, "Antes tiene en la ley de Yave su complacencia"),
        V(3, "Será como árbol que se planta a la vera del arroyo"),
        V(4, "No así los impíos, sino como paja"),
        V(6, "Porque conoce Yave el camino de los justos"),
        V(1, "¿Por qué se amotinan las gentes, y trazan las naciones planes vanos?"),
        V(2, "Se reúnen los reyes de la tierra"),
    ]
    canon, intros = separa_front_matter(ev)
    textos = " ".join(s[2] if s[0] == "vers" else s[1]
                      for s in canon if s[0] in ("vers", "sigue"))
    assert "Bienaventurado el varón" in textos
    assert "amotinan las gentes" in textos
    assert "Las denominaciones" not in textos
    assert "lleva la inscripción" not in textos
    assert "himmos, salmos" not in textos
    intro = " ".join(intros["Ps"])
    assert "Las denominaciones" in intro
    assert "himmos, salmos" in intro
    job = " ".join(s[2] for s in canon if s[0] == "vers" and s[3] == {"Job"})
    assert "murió Job" in job
    assert "himmos" not in job


def test_genesis_empieza_en_seguida():
    ev = [
        L("Gen"),
        V(1, "En el principio creó Dios los cielos y la tierra.",
          cands={"Gen"}),
        V(2, "La tierra era caos y vacío", cands={"Gen"}),
        V(3, "Dijo Dios: «Haya luz»", cands={"Gen"}),
    ]
    canon, intros = separa_front_matter(ev)
    assert "Gen" not in intros
    assert any(s[0] == "vers" and "principio" in s[2] for s in canon)


def test_libro_a_mitad_de_capitulo_no_recorta():
    ev = [
        V(18, "y los puso en el firmamento", cands={"Gen"}),
        V(19, "y hubo tarde y mañana, día cuarto.", cands={"Gen"}),
        V(20, "Dijo luego Dios: «Llénense las aguas»", cands={"Gen"}),
        L("Gen"),
        V(21, "E hizo Dios los grandes monstruos", cands={"Gen"}),
        V(22, "y los bendijo, diciendo", cands={"Gen"}),
    ]
    canon, intros = separa_front_matter(ev)
    assert "Gen" not in intros
    assert any(s[0] == "vers" and s[1] == 21 for s in canon)
    assert any(s[0] == "vers" and s[1] == 20 for s in canon)


def test_proverbios_titulo_corto():
    ev = [
        L("Prov"),
        S("Título y fin del libro.", cands={"Prov"}),
        V(1, "Sentencias de Salomón, hijo de David", cands={"Prov"}),
        V(2, "Para aprender sabiduría y honestidad", cands={"Prov"}),
        V(3, "Alcanzar disciplina y discreción", cands={"Prov"}),
    ]
    canon, intros = separa_front_matter(ev)
    textos = " ".join(s[2] for s in canon if s[0] == "vers")
    assert "Sentencias de Salomón" in textos
    assert intros.get("Prov") == ["Título y fin del libro."]


def test_continuacion_detecta_numeros_altos():
    assert es_continuacion(
        [V(21, "x", cands={"Gen"}), V(22, "y", cands={"Gen"})],
        [19, 20])
    assert not es_continuacion(
        [V(1, "Las denominaciones"), V(7, "lleva")],
        [16, 17])
    # Un 18 suelto en la intro de Salmos no continúa Job 42:17.
    assert not es_continuacion(
        [S("himmos, salmos"), V(18, "Al maestro del coro"),
         V(1, "Las denominaciones")],
        [16, 17])


def test_no_se_traga_un_libro_entero_hasta_un_1_2_3_tardio():
    ev = [L("1Cor"), S("introducción a la iglesia de Corinto", cands={"1Cor"})]
    for n in range(4, 20):
        ev.append(V(n, f"verso {n} de un capítulo ya empezado",
                    cands={"1Cor"}))
    ev.append(V(1, "otro capítulo", cands={"1Cor"}))
    ev.append(V(2, "sigue el otro", cands={"1Cor"}))
    ev.append(V(3, "y el tercero", cands={"1Cor"}))
    canon, intros = separa_front_matter(ev)
    textos = " ".join(s[2] for s in canon if s[0] == "vers")
    assert "verso 4 de un capítulo" in textos
    assert "verso 10 de un capítulo" in textos
    assert "1Cor" not in intros


def test_indice_apertura_ignora_el_1_de_la_intro():
    window = [
        S("autor de todo él"),
        V(1, "Las denominaciones de los Salmos de David"),
        V(7, "lleva la inscripción", start=False),
        V(8, "no directamente de los", start=True),
        V(1, "Bienaventurado el varón"),
        V(2, "Antes tiene en la ley"),
        V(3, "Será como árbol"),
    ]
    i = indice_apertura(window)
    assert i is not None
    assert window[i][2].startswith("Bienaventurado")


def test_pagina_sin_cabecera_antes_de_salmos_cambia_de_libro():
    paginas = [
        {"cands": {"Job"}, "trozos": [
            V(16, "y el Señor bendijo los últimos años de Job",
              cands={"Job"}),
            V(17, "y murió Job anciano y más colmado de días.",
              cands={"Job"}),
        ], "libro": None},
        {"cands": None, "trozos": [
            S("himmos, salmos, loas, etc."),
            V(107, "compuesto de fra", start=False, cands=None),
        ], "libro": None},
        {"cands": {"Ps"}, "trozos": [
            V(1, "Las denominaciones de los Salmos de David"),
        ], "libro": None},
    ]
    asigna_libro(paginas)
    assert paginas[0]["libro"] == "Job"
    assert paginas[1]["libro"] == "Ps"
    assert paginas[2]["libro"] == "Ps"


def test_pagina_sin_cabecera_en_mitad_del_libro_se_queda():
    paginas = [
        {"cands": {"Gen"}, "trozos": [V(10, "a", cands={"Gen"})],
         "libro": None},
        {"cands": None, "trozos": [V(11, "b", cands=None),
                                  V(12, "c", cands=None)],
         "libro": None},
        {"cands": {"Gen"}, "trozos": [V(13, "d", cands={"Gen"})],
         "libro": None},
    ]
    asigna_libro(paginas)
    assert [p["libro"] for p in paginas] == ["Gen", "Gen", "Gen"]


if __name__ == "__main__":
    test_run_intro_salmos_no_es_apertura()
    test_salmos_intro_se_queda_fuera()
    test_genesis_empieza_en_seguida()
    test_libro_a_mitad_de_capitulo_no_recorta()
    test_proverbios_titulo_corto()
    test_continuacion_detecta_numeros_altos()
    test_no_se_traga_un_libro_entero_hasta_un_1_2_3_tardio()
    test_indice_apertura_ignora_el_1_de_la_intro()
    test_pagina_sin_cabecera_antes_de_salmos_cambia_de_libro()
    test_pagina_sin_cabecera_en_mitad_del_libro_se_queda()
    print("ok")
