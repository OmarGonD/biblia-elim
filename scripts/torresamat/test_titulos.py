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
    assert osis.contenido_verso(AH_SENOR) == AH_SENOR.replace("'", "&#x27;")
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
    assert set(todos) == set(parche.CORRECCIONES) | set(parche.TITULOS)
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
        assert viejo == "" or viejo in plano or plano.endswith(viejo), ref
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


def test_sin_textos_de_otras_biblias():
    # Recuperados del facsímil de Torres Amat, no de la Reina-Valera.
    for ref in ("Psalms 50:1", "Psalms 51:1", "Psalms 52:1"):
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


if __name__ == "__main__":
    for nombre, f in sorted(globals().items()):
        if nombre.startswith("test_") and callable(f):
            f()
            print(f"ok {nombre}")
