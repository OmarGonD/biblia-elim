"""Títulos de salmo numerados como versículo (TORRES-PSALM-TITLES-101).

    python3 test_titulos.py

La ida y vuelta por osis2mod/imp2vs se salta si faltan las herramientas de
SWORD.
"""
import os
import re
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

import osis
import parche_facsimil as parche
from titulos import TIPO_TITULO, marca_titulo

SEG = f'<seg type="{TIPO_TITULO}">'
ABSALOM = "Salmo de David cuando temeroso iba huyendo de su hijo Absalom"
AH_SENOR = ("¡Ah Señor! ¿Cómo es que se han aumentado tanto mis "
            "perseguidores? Son muchísimos los que se han rebelado contra mí.")

CONF = """[{nombre}]
DataPath=./modules/texts/ztext/{dir}/
ModDrv=zText
CompressType=ZIP
BlockType=BOOK
Encoding=UTF-8
SourceType=OSIS
Versification=Vulg
Lang=es
GlobalOptionFilter=OSISHeadings
"""


def test_marca_titulo():
    assert marca_titulo(ABSALOM) == f"{SEG}{ABSALOM}</seg>"
    assert (marca_titulo("Para el fin: Por Maeleth.", "Dijo el insensato.")
            == f"{SEG}Para el fin: Por Maeleth.</seg> Dijo el insensato.")
    assert marca_titulo("A & B <c>") == f"{SEG}A &amp; B &lt;c&gt;</seg>"
    try:
        marca_titulo("  ")
    except ValueError:
        pass
    else:
        raise AssertionError("un título vacío no debe marcarse")


def test_contenido_verso():
    assert osis.contenido_verso(AH_SENOR) == AH_SENOR
    assert osis.contenido_verso({"titulo": ABSALOM}) == marca_titulo(ABSALOM)
    assert (osis.contenido_verso({"titulo": "Por Maeleth.",
                                  "texto": "Dijo el insensato"})
            == marca_titulo("Por Maeleth.", "Dijo el insensato"))
    assert osis.contenido_verso({"texto": AH_SENOR}) == AH_SENOR
    assert osis.contenido_verso({"titulo": "", "texto": ""}) == ""


def test_genera_marca_sin_mover_el_numero():
    texto = {"Ps 3:1": {"titulo": ABSALOM}, "Ps 3:2": AH_SENOR}
    with tempfile.TemporaryDirectory() as tmp:
        destino = os.path.join(tmp, "ps.xml")
        osis.genera(texto, destino, orden=["Ps"])
        xml = open(destino, encoding="utf-8").read()
        ET.fromstring(xml)  # bien formado
    assert f'<verse osisID="Ps.3.1">{SEG}{ABSALOM}</seg></verse>' in xml
    assert f'<verse osisID="Ps.3.2">{AH_SENOR}</verse>' in xml
    # El título no se sale del versículo: ni a un slot :0 ni a un <title>
    # de capítulo. <title type="psalm"> es justo lo que no se emite: SWORD
    # lo sacaría como <h3>, el versículo quedaría «Missing» y el visor lo
    # supliría con otra Biblia (ver tests/content_resolver_test.cc).
    assert '<verse osisID="Ps.3.0"' not in xml
    assert "x-psalm-title" in xml
    # El único <title> del documento es el de la cabecera OSIS; dentro del
    # cuerpo no se emite ninguno.
    cuerpo_xml = xml[xml.index("</header>"):]
    assert "<title" not in cuerpo_xml, cuerpo_xml[:200]
    assert '<title type="psalm"' not in xml


def test_cambios_autorizados():
    todos = parche.cambios()
    fusion = set()
    for origen, destino, _l, _m, _r in parche.FUSIONES:
        fusion.add(origen)
        fusion.add(destino)
    assert set(todos) == (set(parche.CORRECCIONES) | set(parche.TITULOS)
                          | set(parche.CABECERAS) | set(parche.COLUMNAS)
                          | fusion)
    for ref, (viejo, titulo, cuerpo, hoja) in parche.TITULOS.items():
        assert ref.startswith("Psalms "), ref
        assert hoja.startswith("tomo III, hoja "), ref
        assert titulo.strip(), ref
        # La inscripción es el primer versículo del salmo (a veces se
        # reparte en dos, Sal 50 y 51). Eso deja fuera por estructura el
        # encabezado editorial de capítulo, que es prosa del editor y cae
        # siempre al final del salmo anterior: no es inscripción canónica
        # y no debe marcarse nunca como título. Tarea aparte.
        assert int(ref.rsplit(":", 1)[1]) <= 2, ref
        nuevo = todos[ref][1]
        assert nuevo.startswith(SEG), ref
        # Lo que el módulo ya traía no se pierde: o era vacío (el OCR
        # perdió el título) o está entero dentro del versículo nuevo.
        plano = re.sub(r"<[^>]+>", "", nuevo)
        if ref in parche.IMPRESO:
            # Erratas del OCR corregidas contra el facsímil
            # (TORRES-PSALM-TITLE-OCR-101); _comprueba_impreso las acota.
            assert plano == "%s %s" % parche.impreso(ref), ref
            continue
        if ref in parche.SUSTITUYE_BASURA:
            # Basura de OCR sustituida entera (Sal 71, Sal 70).
            continue
        if ref in parche.DUPLICADO_DELANTE:
            # Copia exacta de otro versículo, delante del título. Se
            # descarta: el otro versículo la conserva.
            otro = parche.TITULOS[parche.DUPLICADO_DELANTE[ref]][0]
            assert viejo == f"{otro} {titulo} {cuerpo}", ref
            assert titulo in plano and cuerpo in plano, ref
            assert otro not in plano, ref
            continue
        if ref in parche.TRASLADOS:
            # El resto no se pierde: va entero a su versículo.
            destino = todos[parche.TRASLADOS[ref]][1]
            assert viejo.startswith(titulo) and viejo.endswith(destino), ref
            assert cuerpo in viejo, ref
            continue
        assert viejo == "" or viejo in plano or plano.endswith(viejo), ref
    assert parche.SUSTITUYE_BASURA == {"Psalms 71:1", "Psalms 70:1"}
    # Sal 3 y 4: el título se marca, el texto es el mismo.
    for ref in ("Psalms 3:1", "Psalms 4:1"):
        viejo, titulo, cuerpo, _ = parche.TITULOS[ref]
        assert viejo == titulo and cuerpo == "", ref
    # Sal 52:1: título y primer renglón comparten el versículo impreso.
    viejo, titulo, cuerpo, _ = parche.TITULOS["Psalms 52:1"]
    assert titulo.startswith("Para el fin: ")
    assert viejo == titulo[len("Para el fin: "):] + " " + cuerpo
    # Las dos tablas son disjuntas: una errata de contenido nunca se marca
    # como título, y cambios() lo hace explícito levantando ValueError si
    # alguna ref apareciese en ambas.
    assert not (set(parche.CORRECCIONES) & set(parche.TITULOS))
    for ref in parche.CORRECCIONES:
        assert not todos[ref][1].startswith(SEG), ref


def test_titulo_y_cuerpo_no_se_duplican():
    """Sal 52:1: título y primer renglón comparten el versículo impreso. El
    texto que el módulo ya traía sigue entero y una sola vez, con el título
    marcado delante y el cuerpo detrás, sin marcar."""
    todos = parche.cambios()
    viejo, nuevo = todos["Psalms 52:1"]
    _v, titulo, cuerpo, _h = parche.TITULOS["Psalms 52:1"]
    assert nuevo == f"{SEG}{titulo}</seg> {cuerpo}", nuevo
    plano = re.sub(r"<[^>]+>", "", nuevo)
    assert plano.count(cuerpo) == 1, nuevo
    assert plano.count(titulo) == 1, nuevo
    # Nada se mueve a otro versículo ni se pierde.
    assert viejo.endswith(cuerpo), viejo
    assert "Psalms 52:0" not in todos and "Psalms 52:2" not in todos


# Segundo lote, cotejado en el facsímil: versículos que el impreso da solo
# como inscripción. ref -> hoja del tomo III.
LOTE_2 = {"Psalms 5:1": 11, "Psalms 6:1": 11, "Psalms 18:1": 16,
          "Psalms 21:1": 17, "Psalms 29:1": 19, "Psalms 35:1": 24,
          "Psalms 39:1": 25, "Psalms 43:1": 26, "Psalms 59:1": 34,
          "Psalms 59:2": 34, "Psalms 88:1": 47, "Psalms 99:1": 51}


def test_segundo_lote_solo_marca():
    """Se marca la inscripción en su propio versículo sin tocar su texto
    (ni sus erratas de OCR) y el primer versículo de cuerpo no se marca."""
    todos = parche.cambios()
    for ref, hoja in LOTE_2.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert viejo and titulo == viejo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{viejo}</seg>"), ref
    # Controles: el cuerpo empieza en el versículo siguiente y no se toca.
    for ref in ("Psalms 5:2", "Psalms 6:2", "Psalms 18:2", "Psalms 21:2",
                "Psalms 29:2", "Psalms 35:2", "Psalms 39:2", "Psalms 43:2",
                "Psalms 59:3", "Psalms 88:2", "Psalms 99:2"):
        assert ref not in todos, ref
    # Sal 59: la inscripción ocupa el 1 y el 2, como en el 50 y el 51.
    assert todos["Psalms 59:1"][1].startswith(SEG + "Para el fin: Por")
    assert todos["Psalms 59:2"][1].startswith(SEG + "Cuando quemó")


# Tercer lote, mismo criterio: «1.» solo inscripción, cuerpo desde el «2.».
LOTE_3 = {"Psalms 7:1": 12, "Psalms 10:1": 13, "Psalms 11:1": 13,
          "Psalms 17:1": 15, "Psalms 19:1": 16, "Psalms 20:1": 16,
          "Psalms 30:1": 19, "Psalms 33:1": 21, "Psalms 37:1": 25,
          "Psalms 40:1": 26, "Psalms 44:1": 27, "Psalms 45:1": 27,
          "Psalms 46:1": 28, "Psalms 47:1": 28, "Psalms 48:1": 28}


def test_tercer_lote_solo_marca():
    todos = parche.cambios()
    for ref, hoja in LOTE_3.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert viejo and titulo == viejo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{viejo}</seg>"), ref
        # Control: el versículo 2, primero del cuerpo, no se marca ni se
        # toca.
        assert ref.replace(":1", ":2") not in todos, ref
    # Casos apartados de este lote, que no se marcan solo por estructura:
    # Sal 9 (el «1.» de la segunda parte está fundido en el v. 1) y Sal 41
    # (al módulo le falta «Para el fin:», que el impreso pone delante del
    # número).
    # Sal 9 solo se marca con la segunda parte trasladada a 9:22
    # (test_salmo_9_segunda_parte).
    assert "Psalms 9:1" not in LOTE_3
    # Sal 41 no se marca tal cual: solo con el «Para el fin:» recuperado
    # (test_recupera_para_el_fin_delante_del_numero).
    assert "Psalms 41:1" not in LOTE_3
    assert todos["Psalms 41:1"][1].startswith(SEG + "Para el fin: ")
    # Los lotes no se pisan.
    assert not set(LOTE_3) & set(LOTE_2)


# Cuarto lote. Sal 53 en 1-2, como el 50/51/59.
LOTE_4 = {"Psalms 53:1": 30, "Psalms 53:2": 30, "Psalms 54:1": 30,
          "Psalms 60:1": 34}


def test_cuarto_lote_solo_marca():
    todos = parche.cambios()
    for ref, hoja in LOTE_4.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert viejo and titulo == viejo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{viejo}</seg>"), ref
    # Controles: primer versículo de cuerpo sin marcar ni tocar.
    for ref in ("Psalms 53:3", "Psalms 54:2", "Psalms 60:2"):
        assert ref not in todos, ref
    # Apartados: el impreso pone texto delante del «1.» («Para el fin:»,
    # «Salmo de David.») que al módulo le falta; son recuperación, no marca.
    for c in (55, 56, 57, 58, 61, 62, 63, 64):
        ref = f"Psalms {c}:1"
        assert ref not in LOTE_4, c
        # Si ya está resuelto, es con el texto recuperado delante, nunca
        # marcando el título incompleto.
        if ref in todos:
            viejo, nuevo = todos[ref]
            assert nuevo != f"{SEG}{viejo}</seg>", c
    # Ningún lote pisa a otro.
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))
    primero = {"Psalms 3:1", "Psalms 4:1", "Psalms 50:1", "Psalms 50:2",
               "Psalms 51:1", "Psalms 51:2", "Psalms 52:1"}
    assert not primero & set().union(*lotes)


# Quinto lote.
LOTE_5 = {"Psalms 66:1": 36, "Psalms 67:1": 36, "Psalms 68:1": 37,
          "Psalms 69:1": 38, "Psalms 74:1": 40, "Psalms 75:1": 40,
          "Psalms 76:1": 40}


def test_quinto_lote_solo_marca():
    todos = parche.cambios()
    for ref, hoja in LOTE_5.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert viejo and titulo == viejo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{viejo}</seg>"), ref
        # Control: el versículo 2 es cuerpo y no se toca.
        assert ref.replace(":1", ":2") not in todos, ref
    # Sal 71 queda aparte: el impreso es «1. Salmo sobre Salomon, figura de
    # Christo.» y el módulo trae basura de OCR; es recuperación.
    # Nunca se marca tal cual; solo sustituida por el impreso
    # (test_recupera_titulos_enteros).
    assert "Psalms 71:1" not in LOTE_5
    viejo, nuevo = todos["Psalms 71:1"]
    assert nuevo != f"{SEG}{viejo}</seg>"
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Sexto lote.
LOTE_6 = {"Psalms 79:1": 44, "Psalms 82:1": 45, "Psalms 83:1": 46,
          "Psalms 101:1": 51, "Psalms 141:1": 68}


def test_sexto_lote_solo_marca():
    todos = parche.cambios()
    for ref, hoja in LOTE_6.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert viejo and titulo == viejo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{viejo}</seg>"), ref
        assert ref.replace(":1", ":2") not in todos, ref
    # Apartados: prefijo impreso delante del «1.» que falta en el módulo
    # (Sal 80 «Para el fin:», 87 «Cántico y Salmo.», 91 «Salmo y
    # Cántico.») y Sal 84, cuyo 84:1 funde título y v. 2 (84:2 vacío).
    assert "Psalms 84:2" not in LOTE_6
    for c in (80, 84, 87, 91):
        ref = f"Psalms {c}:1"
        assert ref not in LOTE_6, c
        if ref in todos:
            viejo, nuevo = todos[ref]
            assert nuevo != f"{SEG}{viejo}</seg>", c
    # Sal 84 solo se marca partiendo el v. 2 (test_salmo_84_parte_el_v2).
    assert todos["Psalms 84:1"][1] != f"{SEG}{todos['Psalms 84:1'][0]}</seg>"
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Recuperación: «Para el fin:» impreso delante del «1.», perdido por el OCR.
RECUPERA_1 = {"Psalms 41:1": 26, "Psalms 55:1": 31, "Psalms 56:1": 31,
              "Psalms 57:1": 31, "Psalms 58:1": 31}


def test_recupera_para_el_fin_delante_del_numero():
    todos = parche.cambios()
    for ref, hoja in RECUPERA_1.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        # Solo se añade el prefijo impreso; lo que había sigue entero, una
        # vez y al final, dentro del título.
        assert titulo == "Para el fin: " + viejo and cuerpo == "", ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}Para el fin: {viejo}</seg>", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count("Para el fin:") == 1, ref
        assert plano.count(viejo) == 1, ref
        # Control: el v. 2 (cuerpo) no se toca.
        assert ref.replace(":1", ":2") not in todos, ref
    # Erratas del resto del título no se corrigen de paso.
    assert "d Philisthéos" in todos["Psalms 55:1"][1]
    assert "ú tu siervo" in todos["Psalms 57:1"][1]
    # El resto del grupo va en su propio lote, no en este.
    for c in (61, 62, 63, 64, 80, 87, 91):
        assert f"Psalms {c}:1" not in RECUPERA_1, c
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Recuperación, segundo lote: ref -> (prefijo impreso delante del «1.», hoja)
RECUPERA_2 = {"Psalms 61:1": ("Para el fin: ", 34),
              "Psalms 62:1": ("Salmo de David. ", 35),
              "Psalms 63:1": ("Para el fin: ", 35),
              "Psalms 64:1": ("Para el fin: Salmo de David. ", 35),
              "Psalms 80:1": ("Para el fin: ", 45),
              "Psalms 87:1": ("Cántico y Salmo. ", 47),
              "Psalms 91:1": ("Salmo y Cántico. ", 49)}


def test_recupera_prefijos_segundo_lote():
    todos = parche.cambios()
    for ref, (prefijo, hoja) in RECUPERA_2.items():
        viejo, titulo, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert titulo == prefijo + viejo and cuerpo == "", ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{prefijo}{viejo}</seg>", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count(prefijo.strip()) == 1, ref
        assert plano.count(viejo) == 1, ref
        # Control: el v. 2 (cuerpo) no se toca.
        assert ref.replace(":1", ":2") not in todos, ref
    # Sal 61:2-3: el v. 3 está fundido en el 2 y el 3 vacío. Es cuerpo, no
    # título: esta tarea no lo redistribuye.
    assert "Psalms 61:2" not in todos and "Psalms 61:3" not in todos
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Títulos ausentes (v. 1 vacío) o ilegibles (Sal 71) en el módulo, leídos
# enteros en el impreso: ref -> (título impreso, hoja).
RECUPERA_ENTEROS = {
    "Psalms 8:1": ("Al fin: para los lagares: Salmo de David.", 12),
    "Psalms 38:1": ("Para el fin, á Idithun: Cántico de David.", 25),
    "Psalms 71:1": ("Salmo sobre Salomon, figura de Christo.", 38),
    "Psalms 107:1": ("Cántico y Salmo del mismo David.", 55),
    "Psalms 108:1": ("Salmo de David: para el fin.", 55),
    "Psalms 139:1": ("Para el fin: Salmo de David.", 67),
    "Psalms 145:1": ("Aleluya: de Aggéo y de Zacharias.", 69),
}


def test_recupera_titulos_enteros():
    todos = parche.cambios()
    for ref, (titulo, hoja) in RECUPERA_ENTEROS.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo == "", ref
        assert todos[ref] == (viejo, f"{SEG}{titulo}</seg>"), ref
        # El valor anterior exigido es el vacío, salvo la basura de Sal 71.
        if ref == "Psalms 71:1":
            assert viejo.startswith("Salmo 1 sobre &amp;gt;podas"), viejo
        else:
            assert viejo == "", ref
        # Controles: ni el v. 2 ni el v. 0 (cabecera del capítulo) cambian.
        # El cuerpo que también falta (8:2, 38:2-3, 145:2-3) no se rellena.
        for v in (0, 2, 3):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS)]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


def test_valor_anterior_inesperado_detiene_el_parche():
    """Un título vacío que ya no lo está, o una basura distinta de la
    cotejada, detiene el parche: no se pisa texto que nadie ha leído."""
    todos = parche.cambios()
    for ref, texto in (("Psalms 8:1", "algo que el OCR recuperó"),
                       ("Psalms 71:1", "Salmo sobre Salomon, otra lectura")):
        try:
            parche.aplica([(ref, texto)], {ref: todos[ref]})
        except ValueError as e:
            assert ref in str(e), e
        else:
            raise AssertionError(f"{ref}: se parcheó un valor inesperado")
    # Ya aplicado: se respeta (idempotencia).
    ref = "Psalms 8:1"
    assert parche.aplica([(ref, todos[ref][1])], {ref: todos[ref]}) == [
        (ref, todos[ref][1])]


def test_salmo_84_parte_el_v2():
    """Hoja 46: «1. Para el fin: Salmo para los hijos de Coré.» / «2. Oh
    Señor, tú has derramado…». El OCR juntó ambos en 84:1 con el «2»
    dentro y dejó vacío el 84:2."""
    todos = parche.cambios()
    v1, t1 = todos["Psalms 84:1"]
    v2, t2 = todos["Psalms 84:2"]
    titulo = "Para el fin: Salmo para los hijos de Coré."
    assert t1 == f"{SEG}{titulo}</seg>"
    assert v2 == "" and t2.startswith("Oh Señor, tá has derramado")
    # Cada fragmento una vez; el número suelto «2» no pasa a ningún sitio.
    assert v1 == titulo + " 2 " + t2
    junto = re.sub(r"<[^>]+>", "", t1) + " " + t2
    assert junto.count(titulo) == 1 and junto.count("Oh Señor") == 1
    assert parche.TRASLADOS["Psalms 84:1"] == "Psalms 84:2"
    # Controles: 84:0 y 84:3 no se tocan; la errata «tá» se queda.
    assert "Psalms 84:0" not in todos and "Psalms 84:3" not in todos


def test_salmo_9_segunda_parte():
    """Hoja 13 (título en la 12): el impreso numera de nuevo desde el 1
    la «Segunda parte, que es el Salmo X segun los Hebreos»; su «1.» es
    Vulgata 9:22.
    Solo se separa ese verso del título 9:1; el resto de la segunda parte,
    entrelazado en 9:2-21, no se toca aquí."""
    todos = parche.cambios()
    v1, t1 = todos["Psalms 9:1"]
    v22, t22 = todos["Psalms 9:22"]
    titulo = "Para el fin: por los ocultos arcanos del Hijo: Salmo de — David,"
    assert t1 == f"{SEG}{titulo}</seg>"
    assert v22 == "" and t22.startswith("¿Y por qué, oh Señor, te has")
    assert t22.endswith("en la tribulaclon?")
    assert v1 == titulo + " " + t22
    assert parche.TRASLADOS["Psalms 9:1"] == "Psalms 9:22"
    for v in [0] + list(range(2, 22)) + list(range(23, 40)):
        assert f"Psalms 9:{v}" not in todos, v


def test_traslado_incoherente_se_rechaza():
    viejo = dict(parche.CORRECCIONES)
    try:
        parche.CORRECCIONES["Psalms 84:2"] = ("", "Oh Señor, otro texto",
                                              "tomo III, hoja 46")
        try:
            parche.cambios()
        except ValueError as e:
            assert "Psalms 84:1" in str(e), e
        else:
            raise AssertionError("traslado incoherente aceptado")
    finally:
        parche.CORRECCIONES.clear()
        parche.CORRECCIONES.update(viejo)


# Título y cuerpo en el mismo «1.» impreso: ref -> (título, hoja).
COMPARTIDOS_1 = {"Psalms 12:1": ("Para el fin: Salmo de Dayid.", 13),
                 "Psalms 13:1": ("Para el fin: Salmo de David.", 14),
                 "Psalms 14:1": ("Salmo de David.", 14),
                 "Psalms 15:1": ("Inscripcion de titulo: Del mismo David.",
                                 14),
                 "Psalms 16:1": ("Oracion de David.", 14),
                 "Psalms 22:1": ("Salmo de David.", 17)}


def test_titulo_y_cuerpo_compartidos_primer_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja) in COMPARTIDOS_1.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo, ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{titulo}</seg> {cuerpo}", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        # Cada fragmento una vez; el cuerpo queda fuera del título.
        assert plano.count(titulo) == 1 and plano.count(cuerpo) == 1, ref
        assert cuerpo not in re.search(r"<seg[^>]*>(.*?)</seg>",
                                       nuevo).group(1), ref
        if ref == "Psalms 13:1":
            continue
        # Nada se pierde ni se inventa: título + cuerpo = texto del módulo.
        assert viejo == titulo + " " + cuerpo, ref
        # Control: el v. 2 no se toca.
        assert ref.replace(":1", ":2") not in todos, ref
    # Primer renglón del cuerpo de cada uno, leído en el impreso.
    inicio = {"Psalms 12:1": "¡Hasta cuándo, oh Señor",
              "Psalms 13:1": "Dijo en su corazon el insensato",
              "Psalms 14:1": "¡Ah! Señor, ¿quién morará",
              "Psalms 15:1": "Sálvame, oh Senor",
              "Psalms 16:1": "Atiende, oh Señor, á mi justicia",
              "Psalms 22:1": "A El Señor me pastorea"}
    for ref, comienzo in inicio.items():
        assert parche.TITULOS[ref][2].startswith(comienzo), ref


def test_salmo_13_v2_trasladado():
    """Hoja 14: «1. Para el fin: Salmo de David. / Dijo en su corazon… no
    hay uno siquiera. / 2. El Señor echó desde el cielo…». El OCR pegó el
    v. 2 al 1 y dejó vacío el 13:2."""
    todos = parche.cambios()
    viejo, titulo, cuerpo, _h = parche.TITULOS["Psalms 13:1"]
    v2, t2 = todos["Psalms 13:2"]
    assert v2 == "" and t2.startswith("El Señor echó desde el cielo")
    assert cuerpo.endswith("no hay uno siquiera.")
    assert viejo == titulo + " " + cuerpo + " " + t2
    assert parche.TRASLADOS["Psalms 13:1"] == "Psalms 13:2"
    assert "Psalms 13:0" not in todos and "Psalms 13:3" not in todos
    # Los traslados anteriores siguen intactos.
    assert parche.TRASLADOS["Psalms 84:1"] == "Psalms 84:2"
    assert parche.TRASLADOS["Psalms 9:1"] == "Psalms 9:22"
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1),
             {"Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Segundo lote compartido. Corte en el salto de renglón del «1.»:
COMPARTIDOS_2 = {
    "Psalms 23:1": ("Para el primer dia de la semana: Salmo de David.", 17,
                    "Y Del Señor es la tierra"),
    "Psalms 24:1": ("Para el fin: Salmo de David.", 18, "Á tí, oh Señor"),
    "Psalms 25:1": ("Para el fin: Salmo de David.", 18,
                    "a! y Oh, Señor, seas tú mi Juez"),
    "Psalms 26:1": ("Salmo de David antes de ser ungido.", 18,
                    "El Señor es mi luz"),
    "Psalms 28:1": ("Salmo de Dayid, cuando se concluyó el Tabernáculo.", 19,
                    "Presentad al Señor"),
}
# Título impreso DELANTE del «1.», perdido por el OCR; el v. 1 es cuerpo.
PREFIJO_Y_CUERPO = {"Psalms 27:1": ("Salmo del mismo David.", 19),
                    "Psalms 32:1": ("Salmo de David.", 20)}


def test_titulo_y_cuerpo_compartidos_segundo_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in COMPARTIDOS_2.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
        assert todos[ref][1] == f"{SEG}{titulo}</seg> {cuerpo}", ref
        assert ref.replace(":1", ":2") not in todos, ref
    for ref, (titulo, hoja) in PREFIJO_Y_CUERPO.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        # El v. 1 del módulo entero, una vez, como cuerpo tras el título.
        assert t == titulo and cuerpo == viejo, ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{titulo}</seg> {viejo}", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count(titulo) == 1 and plano.count(viejo) == 1, ref
        assert ref.replace(":1", ":2") not in todos, ref
    # Sal 31: el título se recupera. El «2.» fundido («o el hombre…», «Pi»)
    # sigue dentro del 31:1; el 31:2 se llena con el impreso de la hoja 20.
    assert parche.TITULOS["Psalms 31:1"][1] == (
        "Del mismo David, Salmo de inteligencia.")
    assert "o el hombre" in parche.TITULOS["Psalms 31:1"][2]
    # El 31:2 vacío se suple desde la hoja 20, no desde otra Biblia.
    assert todos["Psalms 31:2"][0] == ""
    assert todos["Psalms 31:2"][1].startswith("Dichoso el hombre")
    # Traslados autorizados intactos, y ningún lote pisa a otro.
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), {"Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Tercer lote compartido: ref -> (título tal cual en el módulo, hoja,
# comienzo impreso del cuerpo del «1.»).
COMPARTIDOS_3 = {
    "Psalms 36:1": ("Salmo del mismo David.", 24, "No envidies"),
    "Psalms 42:1": ("¡Salmo de David.", 26, "Júzgame tú, oh Dios"),
    "Psalms 49:1": ("Salmo de d para Asaph.", 29, "El Dios de los dioses"),
    "Psalms 72:1": ("¡ Salmo de Asaph.", 39, "¡Cuán bondadoso es Dios"),
    "Psalms 73:1": ("Salmo de inteligencia de Asaph.", 39,
                    "¿Y por qué, oh Dios"),
}


def test_titulo_y_cuerpo_compartidos_tercer_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in COMPARTIDOS_3.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{titulo}</seg> {cuerpo}", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count(titulo) == 1 and plano.count(cuerpo) == 1, ref
        for v in (0, 2):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    # Sal 34, 65 y 70: el título ya está marcado. El v. 2 no se rellena ni
    # se parte (en el 65 la basura «e 2,» sigue en el cuerpo del 1).
    for c in (34, 65, 70):
        assert f"Psalms {c}:1" in parche.TITULOS
    assert todos["Psalms 34:2"] == (
        "", "Ármate y abraza el escudo, y sal á defenderme.")
    assert "Psalms 65:2" not in todos and "Psalms 70:2" not in todos
    assert "e 2," in parche.TITULOS["Psalms 65:1"][2]
    # Traslados autorizados intactos, y ningún lote pisa a otro.
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3),
             {"Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Cuarto lote compartido.
COMPARTIDOS_4 = {
    "Psalms 77:1": ("Inteligencia, ¿mstruccion de Asaph,.", 41,
                    "Escucha, pueblo mio"),
    "Psalms 78:1": ("Salmo de Asaph.", 44, "Oh Dios, los Gentiles"),
    "Psalms 81:1": ("Salmo de Asaph.", 45, "Presente está Dios"),
    "Psalms 86:1": ("4 los hijos de Coré. Salmo y Cántico,", 47,
                    "Sobre los montes santos"),
    "Psalms 89:1": ("Oracion de M OYSÉs, varon de Dios.", 48,
                    "Señor, en todas épocas"),
}
PREFIJO_Y_CUERPO_2 = {
    "Psalms 85:1": ("Oracion del mismo David.", 46, "Inelina, Señor"),
    "Psalms 90:1": ("Alabanza y Cántico de David.", 48,
                    "El que se acoge al asilo"),
    "Psalms 92:1": ("Salmo y Cántico del mismo David, para la víspera del "
                    "sábado, que es cuando fué criada la tierra.", 49,
                    "El Señor reinó"),
}


def test_titulo_y_cuerpo_compartidos_cuarto_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in COMPARTIDOS_4.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
        assert todos[ref][1] == f"{SEG}{titulo}</seg> {cuerpo}", ref
    for ref, (titulo, hoja, comienzo) in PREFIJO_Y_CUERPO_2.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        # El módulo no conserva nada de la inscripción: el v. 1 es cuerpo.
        assert t == titulo and cuerpo == viejo, ref
        assert viejo.startswith(comienzo) and titulo not in viejo, ref
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{titulo}</seg> {viejo}", ref
    for ref in list(COMPARTIDOS_4) + list(PREFIJO_Y_CUERPO_2):
        nuevo = todos[ref][1]
        plano = re.sub(r"<[^>]+>", "", nuevo)
        cuerpo = parche.TITULOS[ref][2]
        assert plano.count(cuerpo) == 1, ref
        for v in (0, 2):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), {"Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Quinto lote compartido.
COMPARTIDOS_5 = {
    "Psalms 96:1": ("Salmo de David, cuando fué restaurada su tierra.", 50,
                    "El Señor es el que reina"),
    "Psalms 97:1": ("Salmo del mismo David.", 50, "Cantad al Señor un"),
    "Psalms 98:1": ("Salmo del mismo David.", 51, "Reina ya el Señor"),
    "Psalms 100:1": ("Salmo del mismo David.", 51, "Cantaré, Señor"),
    "Psalms 102:1": ("Del mismo David.", 52, "Bendice, oh alma mia"),
}


def test_titulo_y_cuerpo_compartidos_quinto_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in COMPARTIDOS_5.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
        assert todos[ref][1] == f"{SEG}{titulo}</seg> {cuerpo}", ref
    # Sal 100: el título va DETRÁS del «1.» y el módulo lo conserva entero;
    # no hay nada que recuperar.
    assert parche.TITULOS["Psalms 100:1"][0].startswith(
        "Salmo del mismo David.")
    # Sal 94: título impreso delante del «1.», perdido; v. 1 entero cuerpo.
    viejo, t, cuerpo, h = parche.TITULOS["Psalms 94:1"]
    assert (t, h) == ("Alabanza ó Cántico del mismo David.",
                      "tomo III, hoja 50")
    assert cuerpo == viejo and viejo.startswith("Venid, regocijémonos")
    # Sal 95, mixto: prefijo recuperado + la parte del título que el
    # módulo conserva; el cuerpo es el resto, sin tocar.
    viejo, t, cuerpo, h = parche.TITULOS["Psalms 95:1"]
    parte = "Cuando se reedificó la Casa de Dios despues de la cautividad."
    assert t == "Cántico del mismo David, cantado. " + parte
    assert viejo == parte + " " + cuerpo and cuerpo.startswith("Cantad")
    for ref in list(COMPARTIDOS_5) + ["Psalms 94:1", "Psalms 95:1"]:
        plano = re.sub(r"<[^>]+>", "", todos[ref][1])
        viejo, t, cuerpo, _h = parche.TITULOS[ref]
        assert plano == t + " " + cuerpo, ref
        assert plano.count(cuerpo) == 1 and viejo in plano, ref
        for v in (0, 2):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    # Sal 93: título y cuerpo del «1.» recuperados del impreso. El 93:2
    # («Haz pues brillar…») no se toca.
    assert parche.TITULOS["Psalms 93:1"][1].startswith("Salmo del mismo David")
    assert "Jehovah" in parche.TITULOS["Psalms 93:1"][2]
    assert "Psalms 93:2" not in todos
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), set(COMPARTIDOS_5),
             {"Psalms 94:1", "Psalms 95:1", "Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Sexto lote compartido.
COMPARTIDOS_6 = {"Psalms 103:1": ("Del mismo David.", 52, "Oh alma mia"),
                 "Psalms 109:1": ("Salmo de David.", 58, "D El Señor dijo")}
# «Aleluya» (y lo que le sigue) impreso delante del «1.», perdido.
ALELUYA = {"Psalms 104:1": ("Aleluya.", 53, "Alabad al Señor, é"),
           "Psalms 105:1": ("Aleluya.", 54, "Alabad al Señor porque"),
           "Psalms 106:1": ("Aleluya.", 54, "Alabad al Señor, porque"),
           "Psalms 110:1": ("Aleluya.", 58, "Oh Señor, loarte he"),
           "Psalms 111:1": ("Aleluya: del regreso de Aggéo y de Zacharias.",
                            58, "Bienayenturado el hombre")}


def test_titulo_y_cuerpo_compartidos_sexto_lote():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in COMPARTIDOS_6.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
    for ref, (titulo, hoja, comienzo) in ALELUYA.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo == viejo, ref
        assert viejo.startswith(comienzo) and "Aleluya" not in viejo, ref
    for ref in list(COMPARTIDOS_6) + list(ALELUYA):
        viejo, t, cuerpo, _h = parche.TITULOS[ref]
        nuevo = todos[ref][1]
        assert nuevo == f"{SEG}{t}</seg> {cuerpo}", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count(cuerpo) == 1 and plano.count(t) == 1, ref
        for v in (0, 2):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    # Sal 112: «Aleluya.» recuperado. Los versos del 113 que el OCR dejó
    # dentro de 112:1-8 pasan a su sitio (TORRES-PSALM-ALIGN-101).
    assert parche.TITULOS["Psalms 112:1"][1] == "Aleluya."
    assert "Cuando Israél" in parche.TITULOS["Psalms 112:1"][2]
    assert "Cuando Israél" not in todos["Psalms 112:1"][1]
    assert todos["Psalms 113:1"][1].startswith(
        f"{SEG}Aleluya.</seg> Cuando Israél")
    assert "Psalms 112:2" in parche.COLUMNAS
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), set(COMPARTIDOS_5), set(COMPARTIDOS_6),
             set(ALELUYA),
             {"Psalms 94:1", "Psalms 95:1", "Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Séptimo lote: título impreso delante del primer número, perdido.
PREFIJO_Y_CUERPO_3 = {
    "Psalms 114:1": ("Aleluya.", 59, "Amé al Señor"),
    "Psalms 115:1": ("Aleluya.", 59, "Creí d Dios"),
    "Psalms 116:1": ("Aleluya.", 60, "Alabad al Señor, naciones"),
    "Psalms 117:1": ("Aleluya.", 60, "Alabad al Señor, porque es tan"),
    "Psalms 118:1": ("Aleluya.", 60, "Bienaventurados los que proceden"),
    "Psalms 120:1": ("Cántico gradual.", 63, "Aleé mis ojos"),
}


def test_septimo_lote_sal_114_120():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in PREFIJO_Y_CUERPO_3.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo == viejo, ref
        assert viejo.startswith(comienzo), ref
        nuevo = todos[ref][1]
        # Lo escrito es el impreso (IMPRESO recorta la cabecera del 115:1).
        t_imp, c_imp = parche.impreso(ref)
        assert c_imp in viejo, ref
        assert nuevo == f"{SEG}{t_imp}</seg> {c_imp}", ref
        assert re.sub(r"<[^>]+>", "", nuevo).count(c_imp) == 1, ref
    # Sal 118: la letra de estrofa «ALEPH.» no forma parte del título.
    assert "ALEPH" not in todos["Psalms 118:1"][1].upper().split("</SEG>")[0]
    # Sal 119: corte en el salto de renglón del «1.».
    viejo, t, cuerpo, h = parche.TITULOS["Psalms 119:1"]
    assert (t, h) == ("Cántico de los grados, 0 gradual.", "tomo III, hoja 63")
    assert viejo == t + " " + cuerpo and "Clamé al Señor" in cuerpo
    for ref in list(PREFIJO_Y_CUERPO_3) + ["Psalms 119:1"]:
        for v in (0, 2):
            otro = ref.replace(":1", f":{v}")
            assert otro not in todos or otro in parche.COLUMNAS, (ref, v)
    # Sal 113: 113:1 no era la inscripción («Cuando Israél…» estaba dentro
    # de 112). TITULOS no lo marca; las columnas fundidas llevan «Aleluya.»
    # y el v. 1 a su sitio (TORRES-PSALM-ALIGN-101).
    for v in (1, 2, 3):
        assert f"Psalms 113:{v}" not in parche.TITULOS
    assert {"Psalms 112:2", "Psalms 112:3"} <= set(parche.COLUMNAS)
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), set(COMPARTIDOS_5), set(COMPARTIDOS_6),
             set(ALELUYA), set(PREFIJO_Y_CUERPO_3),
             {"Psalms 119:1", "Psalms 94:1", "Psalms 95:1", "Psalms 84:1",
              "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Octavo lote: ref -> (título, hoja, comienzo del cuerpo).
GRADUALES = {
    "Psalms 121:1": ("Cántico gradual.", 63, "Gran contento tuve"),
    "Psalms 124:1": ("Cántico gradual,", 64, "Los que ponen en el Señor"),
    "Psalms 125:1": ("Cántico gradual.", 64, "Cuando el Señor hará"),
    "Psalms 126:1": ("Cántico gradual de Salomon.", 64, "SI el Señor"),
    "Psalms 127:1": ("Cántico gradual.", 64, "Bienaventurados todos"),
    "Psalms 128:1": ("Cántico gradual.", 65, "Muchas veces me han"),
}


def test_octavo_lote_graduales():
    todos = parche.cambios()
    for ref, (titulo, hoja, comienzo) in GRADUALES.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert h == f"tomo III, hoja {hoja}", ref
        assert t == titulo and cuerpo.startswith(comienzo), ref
        assert viejo == titulo + " " + cuerpo, ref
        assert todos[ref][1] == "%s%s</seg> %s" % ((SEG,)
                                                   + parche.impreso(ref)), ref
    # Sal 124: el impreso trae «Cántico gradual.» con punto.
    assert parche.impreso("Psalms 124:1")[0] == "Cántico gradual."
    # Sal 122: título delante del «1.»; el v. 1 entero es cuerpo.
    viejo, t, cuerpo, h = parche.TITULOS["Psalms 122:1"]
    assert (t, h) == ("Cántico gradual.", "tomo III, hoja 64")
    assert cuerpo == viejo and viejo.startswith("Átí Señor")
    # Sal 128: el v. 2 impreso sigue pegado en el cuerpo del 1, sin tocar.
    assert "2 Muchas veces" in parche.TITULOS["Psalms 128:1"][2]
    for ref in list(GRADUALES) + ["Psalms 122:1"]:
        plano = re.sub(r"<[^>]+>", "", todos[ref][1])
        assert plano.count(parche.TITULOS[ref][2]) == 1, ref
        for v in (0, 2):
            assert ref.replace(":1", f":{v}") not in todos, (ref, v)
    # Sal 123: título y cuerpo del «1.» recuperados. El 123:2 no se toca.
    assert parche.TITULOS["Psalms 123:1"][1] == "Cántico gradual."
    assert parche.TITULOS["Psalms 123:1"][0] == ""
    assert "Psalms 123:2" not in todos
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), set(COMPARTIDOS_5), set(COMPARTIDOS_6),
             set(ALELUYA), set(PREFIJO_Y_CUERPO_3), set(GRADUALES),
             {"Psalms 122:1", "Psalms 119:1", "Psalms 94:1", "Psalms 95:1",
              "Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


# Noveno lote y apartados leídos en el jp2 de 1882. ref -> (título, hoja,
# cómo queda el cuerpo respecto del texto viejo).
# "corte": título + espacio + cuerpo == viejo.
# "entero": el v. 1 del módulo es el cuerpo; el título se antepone.
# "prefijo": el título empieza por lo recuperado y sigue con el arranque
# del módulo.
# "vacio": el módulo no tenía el verso.
# "basura": la inscripción del módulo se sustituye; el cuerpo se queda.
# "copia": delante del título hay una copia exacta de otro versículo.
NOVENO = {
    "Psalms 129:1": ("Cántico gradual.", 65, "corte"),
    "Psalms 131:1": ("Cántico gradual.", 65, "corte"),
    "Psalms 132:1": ("Cántico gradual de David.", 65, "corte"),
    "Psalms 134:1": ("Aleluya.", 66, "corte"),
    "Psalms 135:1": ("Aleluya.", 66, "corte"),
    "Psalms 137:1": ("Del mismo David.", 67, "corte"),
    "Psalms 138:1": ("Para el fin: Salmo de David.", 67, "corte"),
    "Psalms 140:1": ("Salmo de David,", 68, "corte"),
    "Psalms 144:1": ("Alabanza inspirada al mismo David.", 69, "corte"),
    "Psalms 148:1": ("Alehiya.", 72, "corte"),
    "Psalms 149:1": ("Aleluya.", 72, "corte"),
    "Psalms 150:1": ("Aleluya,", 72, "corte"),
    "Psalms 130:1": ("Cántico gradual de David.", 65, "entero"),
    "Psalms 136:1": ("Salmo de David, para Jeremías.", 66, "entero"),
    "Psalms 147:1": ("Aleluya.", 69, "completado"),
    "Psalms 31:1": ("Del mismo David, Salmo de inteligencia.", 20, "entero"),
    "Psalms 112:1": ("Aleluya.", 59, "recortado"),
    "Psalms 142:1": ("Salmo de David: E. Cuando le perseguia su hijo "
                     "Absalom 7,", 68, "prefijo"),
    "Psalms 143:1": ("Salmo de David: Contra Goliath.", 68, "prefijo"),
    "Psalms 65:1": ("Para el fin: Salmo y Cántico de la Resurreccion,", 35,
                    "prefijo"),
    "Psalms 133:1": ("Cántico gracual.", 65, "copia"),
    "Psalms 34:1": ("Salmo del mismo David.", 21, "vacio"),
    "Psalms 123:1": ("Cántico gradual.", 64, "vacio"),
    "Psalms 146:1": ("Aleluya.", 69, "vacio"),
    "Psalms 93:1": ("Salmo del mismo David, para el cuarto dia de la semana.",
                    49, "vacio"),
    "Psalms 70:1": ("Salmo de David: De los hijos de Jonadab, y de los "
                    "primeros cautivos.", 38, "basura"),
}


def test_noveno_lote_y_apartados():
    todos = parche.cambios()
    for ref, (titulo, hoja, modo) in NOVENO.items():
        viejo, t, cuerpo, h = parche.TITULOS[ref]
        assert (t, h) == (titulo, f"tomo III, hoja {hoja}"), ref
        nuevo = todos[ref][1]
        # Lo que se escribe, con las erratas de IMPRESO ya corregidas.
        t_imp, c_imp = parche.impreso(ref)
        assert nuevo == f"{SEG}{t_imp}</seg> {c_imp}", ref
        plano = re.sub(r"<[^>]+>", "", nuevo)
        assert plano.count(c_imp) == 1, ref
        assert plano.count(t_imp) == 1, ref
        if modo == "corte":
            assert viejo == titulo + " " + cuerpo, ref
        elif modo == "entero":
            assert cuerpo == viejo and viejo, ref
            assert plano == f"{titulo} {viejo}", ref
        elif modo == "completado":
            # El cuerpo es el verso entero más lo que el OCR perdió
            # (TORRES-PSALM-GLUE-101, cabeceras_pegadas.AÑADIDOS).
            assert cuerpo == viejo + parche.AÑADIDOS[ref] and viejo, ref
            assert plano == f"{titulo} {cuerpo}", ref
        elif modo == "recortado":
            # El impreso es un trozo del verso del OCR; lo demás era de la
            # otra columna o una cabecera (TORRES-PSALM-ALIGN-101).
            assert t_imp == titulo and c_imp in viejo and c_imp != viejo, ref
            assert plano == f"{t_imp} {c_imp}", ref
        elif modo == "prefijo":
            assert plano.endswith(viejo) and plano != viejo, ref
            assert viejo.endswith(cuerpo), ref
        elif modo == "vacio":
            assert viejo == "" and cuerpo, ref
        elif modo == "basura":
            assert ref in parche.SUSTITUYE_BASURA, ref
            assert cuerpo in viejo and titulo not in viejo, ref
        elif modo == "copia":
            otro = parche.TITULOS[parche.DUPLICADO_DELANTE[ref]][0]
            assert viejo == f"{otro} {titulo} {cuerpo}", ref
            assert otro not in plano, ref
        else:
            raise AssertionError(modo)
        for v in (0, 2):
            otro = ref.replace(":1", f":{v}")
            if otro in ("Psalms 31:2", "Psalms 34:2"):
                continue
            # Sal 147:2 solo pierde la copia del v. 3 (TORRES-PSALM-GLUE-101).
            # ... y los versos de columnas fundidas (TORRES-PSALM-ALIGN-101).
            if otro in parche.REPETIDOS or otro in parche.COLUMNAS:
                continue
            assert otro not in todos, (ref, v)
    # Sal 113: el «1.» impreso («Cuando Israél…») no estaba en 113:1, que
    # traía la segunda numeración. No se marca en TITULOS: las columnas
    # fundidas lo ponen en su verso (TORRES-PSALM-ALIGN-101).
    for v in (1, 2, 3):
        assert f"Psalms 113:{v}" not in parche.TITULOS
        assert f"Psalms 113:{v}" in parche.COLUMNAS
    assert parche.TRASLADOS == {"Psalms 84:1": "Psalms 84:2",
                                "Psalms 9:1": "Psalms 9:22",
                                "Psalms 13:1": "Psalms 13:2"}
    assert parche.DUPLICADO_DELANTE == {"Psalms 133:1": "Psalms 132:1"}
    lotes = [set(LOTE_2), set(LOTE_3), set(LOTE_4), set(LOTE_5),
             set(LOTE_6), set(RECUPERA_1), set(RECUPERA_2),
             set(RECUPERA_ENTEROS), set(COMPARTIDOS_1), set(COMPARTIDOS_2),
             set(PREFIJO_Y_CUERPO), set(COMPARTIDOS_3), set(COMPARTIDOS_4),
             set(PREFIJO_Y_CUERPO_2), set(COMPARTIDOS_5), set(COMPARTIDOS_6),
             set(ALELUYA), set(PREFIJO_Y_CUERPO_3), set(GRADUALES),
             set(NOVENO),
             {"Psalms 122:1", "Psalms 119:1", "Psalms 94:1", "Psalms 95:1",
              "Psalms 84:1", "Psalms 9:1"}]
    assert sum(map(len, lotes)) == len(set().union(*lotes))


def test_encabezado_pegado_no_es_titulo():
    """El «SALMO …» y el argumento del salmo siguiente, pegados al último
    versículo, se quitan. No se marcan como título."""
    todos = parche.cambios()
    for ref, hoja, queda in (
        ("Psalms 4:10", "tomo III, hoja 11", "mi esperanza +,"),
        ("Psalms 52:7", "tomo III, hoja 30", "gozo Israél."),
        ("Psalms 130:3", "tomo III, hoja 65", "siempre jamás."),
    ):
        viejo, nuevo, h = parche.CORRECCIONES[ref]
        assert h == hoja, ref
        assert ref not in parche.TITULOS, ref
        assert not nuevo.startswith(SEG), ref
        assert "SALMO" not in nuevo, ref
        assert nuevo.endswith(queda + " " + viejo[viejo.rindex("<chapter"):]), ref
        assert todos[ref] == (viejo, nuevo)


def test_sin_textos_de_otras_biblias():
    # Recuperados del facsímil de Torres Amat, no de la Reina-Valera.
    # «Jehovah» del Sal 93:1 está en el impreso (glosa en cursiva), no es
    # la Reina-Valera.
    for ref in ("Psalms 50:1", "Psalms 51:1", "Psalms 52:1", "Psalms 41:1",
                "Psalms 55:1", "Psalms 56:1", "Psalms 57:1", "Psalms 58:1",
                "Psalms 61:1", "Psalms 62:1", "Psalms 63:1", "Psalms 64:1",
                "Psalms 80:1", "Psalms 87:1", "Psalms 91:1", "Psalms 8:1",
                "Psalms 38:1", "Psalms 71:1", "Psalms 107:1", "Psalms 108:1",
                "Psalms 139:1", "Psalms 145:1", "Psalms 27:1",
                "Psalms 32:1", "Psalms 85:1", "Psalms 90:1", "Psalms 92:1",
                "Psalms 94:1", "Psalms 95:1", "Psalms 130:1", "Psalms 136:1",
                "Psalms 142:1", "Psalms 143:1", "Psalms 31:1",
                "Psalms 34:1", "Psalms 65:1", "Psalms 70:1", "Psalms 93:1",
                "Psalms 112:1", "Psalms 123:1", "Psalms 146:1",
                "Psalms 147:1"):
        nuevo = parche.cambios()[ref][1]
        assert "Jehová" not in nuevo and "Músico principal" not in nuevo, ref


def _herramientas():
    return all(shutil.which(t) for t in ("imp2vs", "osis2mod", "mod2imp",
                                         "diatheke"))


def _modulo(tmp, nombre):
    d = nombre.lower()
    datos = os.path.join(tmp, "modules", "texts", "ztext", d)
    os.makedirs(datos)
    os.makedirs(os.path.join(tmp, "mods.d"), exist_ok=True)
    with open(os.path.join(tmp, "mods.d", d + ".conf"), "w",
              encoding="utf-8") as f:
        f.write(CONF.format(nombre=nombre, dir=d))
    return datos


def _exporta(tmp, nombre):
    env = dict(os.environ, SWORD_PATH=tmp)
    out = subprocess.run(["mod2imp", nombre], capture_output=True, text=True,
                         env=env, check=True).stdout
    return dict(parche.lee_imp(out))


def _diatheke(tmp, nombre, ref):
    env = dict(os.environ, SWORD_PATH=tmp)
    return subprocess.run(["diatheke", "-b", nombre, "-f", "HTMLHREF",
                           "-k", ref], capture_output=True, text=True,
                          env=env, check=True).stdout


def test_ida_y_vuelta_imp2vs():
    """El camino del parche: imp -> imp2vs -> SWORD."""
    if not _herramientas():
        print("  saltado: faltan herramientas de SWORD")
        return
    todos = parche.cambios()
    refs = ["Psalms 3:1", "Psalms 3:2", "Psalms 4:1", "Psalms 50:1",
            "Psalms 51:1", "Psalms 52:1"]
    cuerpos = {r: todos[r][1] for r in refs if r in todos}
    cuerpos["Psalms 3:2"] = todos["Psalms 3:2"][1]
    with tempfile.TemporaryDirectory() as tmp:
        nombre = "TaTitulosImpPrueba"
        datos = _modulo(tmp, nombre)
        imp = os.path.join(tmp, "p.imp")
        with open(imp, "w", encoding="utf-8") as f:
            f.write(parche.escribe_imp(list(cuerpos.items())))
        subprocess.run(["imp2vs", imp, "-z", "z", "-v", "Vulg", "-o", "."],
                       cwd=datos, check=True, capture_output=True)
        vuelta = _exporta(tmp, nombre)
        for ref, cuerpo in cuerpos.items():
            assert vuelta.get(ref) == cuerpo, (ref, vuelta.get(ref))
        # Leído de SWORD: el título sigue en su versículo, marcado, y el
        # cuerpo que comparte slot con él no se ha ido a otra parte.
        assert vuelta["Psalms 3:1"] == f"{SEG}{ABSALOM}</seg>"
        assert vuelta["Psalms 52:1"].startswith(SEG)
        assert vuelta["Psalms 52:1"].endswith("No hay Dios.")
        assert SEG not in vuelta["Psalms 3:2"]
        # diatheke descarta el type del <seg> en sus formatos de salida, así
        # que aquí solo dice dónde cae cada texto; la marca se comprueba
        # arriba en crudo (mod2imp) y, ya renderizada como
        # <span class="x-psalm-title"> y disponible, en content_resolver_test.
        html = _diatheke(tmp, nombre, "Psalms 3:1")
        assert "Psalms 3:1" in html and ABSALOM in html, html
        html = _diatheke(tmp, nombre, "Psalms 3:2")
        assert "Psalms 3:2" in html and "¡Ah Señor!" in html, html
        assert ABSALOM not in html, html


def test_ida_y_vuelta_osis2mod():
    """El camino de la reconstrucción: JSON -> osis.py -> osis2mod."""
    if not _herramientas():
        print("  saltado: faltan herramientas de SWORD")
        return
    texto = {"Ps 3:1": {"titulo": ABSALOM}, "Ps 3:2": AH_SENOR,
             "Ps 52:1": {"titulo": "Para el fin: Por Maeleth. Salmo de "
                                   "inteligencia de David.",
                         "texto": "Dijo el insensato en su corazon: No hay "
                                  "Dios."}}
    with tempfile.TemporaryDirectory() as tmp:
        nombre = "TaTitulosOsisPrueba"
        datos = _modulo(tmp, nombre)
        xml = os.path.join(tmp, "ps.xml")
        osis.genera(texto, xml, orden=["Ps"])
        r = subprocess.run(["osis2mod", datos, xml, "-v", "Vulg", "-z", "z"],
                           capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr
        # osis2mod cierra el capítulo en su último versículo escrito con
        # <chapter eID=…/>; eso es suyo, no del texto.
        vuelta = {ref: re.sub(r'\s*<chapter eID="[^"]*" osisID="[^"]*"/>$',
                              "", cuerpo)
                  for ref, cuerpo in _exporta(tmp, nombre).items()}
        assert vuelta["Psalms 3:1"] == f"{SEG}{ABSALOM}</seg>", vuelta
        assert vuelta["Psalms 3:2"] == AH_SENOR, vuelta["Psalms 3:2"]
        assert vuelta["Psalms 52:1"].startswith(SEG)
        assert vuelta["Psalms 52:1"].endswith("Dijo el insensato en su "
                                              "corazon: No hay Dios.")


def test_erratas_de_titulo_graduales():
    """TORRES-PSALM-TITLE-OCR-101: Sal 119:1, 124:1 y 133:1 contra el
    facsímil (tomo III, hojas 63-65)."""
    todos = parche.cambios()
    assert todos["Psalms 119:1"][1] == (
        f"{SEG}Cántico de los grados, ó gradual.</seg> Clamé al Señor en "
        "mi tribulacion, y me atendió.")
    assert "eme" not in todos["Psalms 119:1"][1].split()
    assert todos["Psalms 124:1"][1].startswith(f"{SEG}Cántico gradual.</seg>")
    assert todos["Psalms 133:1"][1].startswith(f"{SEG}Cántico gradual.</seg>")
    # Los demás graduales (120-132) ya traen el título del impreso.
    for c in range(120, 133):
        t = parche.impreso(f"Psalms {c}:1")[0]
        assert t.startswith("Cántico gradual") and t.endswith("."), (c, t)
    # La guarda no admite reescribir un título entero.
    guardado = dict(parche.IMPRESO)
    try:
        parche.IMPRESO["Psalms 124:1"] = ("Salmo de otro.", None, "x")
        try:
            parche.cambios()
        except ValueError:
            pass
        else:
            raise AssertionError("se aceptó un título que no es errata")
    finally:
        parche.IMPRESO.clear()
        parche.IMPRESO.update(guardado)


if __name__ == "__main__":
    for nombre, f in sorted(globals().items()):
        if nombre.startswith("test_") and callable(f):
            f()
            print(f"ok {nombre}")
