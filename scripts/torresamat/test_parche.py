"""Correcciones y mecanismo del parche de facsímil (TORRES-FACSIMILE-102,
-103, -104).

    python3 test_parche.py

Mt 12:6 acababa «…mayor que el templo. i»: la «i» es una mota del papel
(tomo IV, hoja 22; djvu.xml, caja de 3×8 px con confianza 0 en el blanco
del renglón). El impreso acaba en «el templo.».
"""
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile

import parche_facsimil as parche

# Texto del módulo instalado antes del parche (mod2imp TorresAmat).
MT_12_5 = ("¿O no habeis leido en la Ley, cómo los sacerdotes en el templo "
           "trabajan en el sábado, y con todo eso no pecan?")
MT_12_6 = "Pues yo os digo, que aquí está uno que es mayor que el templo. i"
MT_12_7 = ("Que si vosotros supieseis bien lo que significa: Mas quiero la "
           "misericordia, que no el sacrificio: jamás hubierais condenado á "
           "los inocentes.")


def _solo(ref):
    return {ref: parche.cambios()[ref]}


def test_mt_12_6_sin_la_mota():
    entradas = [("Matthew 12:5", MT_12_5), ("Matthew 12:6", MT_12_6),
                ("Matthew 12:7", MT_12_7)]
    nuevas = dict(parche.aplica(entradas, _solo("Matthew 12:6")))
    assert nuevas["Matthew 12:6"] == (
        "Pues yo os digo, que aquí está uno que es mayor que el templo.")
    # Control: el texto legítimo de alrededor no se toca.
    assert nuevas["Matthew 12:5"] == MT_12_5
    assert nuevas["Matthew 12:7"] == MT_12_7
    viejo, nuevo, hoja = parche.CORRECCIONES["Matthew 12:6"]
    assert hoja == "tomo IV, hoja 22"
    # Solo se quita la mota: el resto del verso queda igual.
    assert viejo == nuevo + " i"


def test_texto_inesperado_no_se_parchea():
    # Si el módulo ya no trae la lectura con la mota, el parche se detiene
    # en vez de pisar un texto que nadie ha cotejado.
    entradas = [("Matthew 12:6", "Pues yo os digo, que aquí está uno.")]
    try:
        parche.aplica(entradas, _solo("Matthew 12:6"))
    except ValueError:
        pass
    else:
        raise AssertionError("se parcheó un texto inesperado")


def test_ya_corregido_se_respeta():
    nuevo = parche.CORRECCIONES["Matthew 12:6"][1]
    nuevas = parche.aplica([("Matthew 12:6", nuevo)],
                           _solo("Matthew 12:6"))
    assert nuevas == [("Matthew 12:6", nuevo)]


# Mt 12:9-11 del módulo instalado antes del parche. El 12:11 ya viene
# corregido por TORRES-FACSIMILE-101.
MT_12_9 = "Habiendo partido de allí, entró en la synagoga de ellos"
MT_12_10 = ("Donde se hallabaun hombre que tenia seca una mano; y preguntaron "
            "á Jesus, para hallar motivo de acusarle: ¿Si era lícito curar en "
            "dia de sábado?")
MT_12_11 = ("Mas él les dijo: ¿Qué hombre habrá entre vosotros, que tenga una "
            "oveja, y si esta cae en una fosa en dia de sábado, no la levante "
            "y saque fuera?")


def test_mt_12_10_hallaba_un():
    # Tomo IV, hoja 22: «Donde se hallaba un hombre»; el OCR fundió las
    # dos palabras, que el impreso compone casi sin espacio.
    entradas = [("Matthew 12:9", MT_12_9), ("Matthew 12:10", MT_12_10),
                ("Matthew 12:11", MT_12_11)]
    autorizados = {r: parche.cambios()[r]
                   for r in ("Matthew 12:10", "Matthew 12:11")}
    nuevas = dict(parche.aplica(entradas, autorizados))
    assert nuevas["Matthew 12:10"].startswith(
        "Donde se hallaba un hombre que tenia seca una mano;")
    assert "hallabaun" not in nuevas["Matthew 12:10"]
    # Solo cambia esa palabra: el resto del verso es el mismo.
    assert nuevas["Matthew 12:10"] == MT_12_10.replace(
        "hallabaun", "hallaba un")
    # Controles: 12:9 no se toca; 12:11 ya estaba corregido y se respeta.
    assert nuevas["Matthew 12:9"] == MT_12_9
    assert nuevas["Matthew 12:11"] == MT_12_11
    assert parche.CORRECCIONES["Matthew 12:10"][2] == "tomo IV, hoja 22"


def test_salmo4_v3_vuelve_y_v5_se_queda_con_lo_suyo():
    """Tomo III, hoja 11: el OCR leyó «5.» el número del v. 3 y lo pegó
    delante del v. 5. Instalado con TORRES-PSALM-TITLES-101 pero ausente de
    esta tabla: sin estas entradas el módulo instalado no se regeneraba
    desde modulos/ de git."""
    todos = parche.cambios()
    viejo3, nuevo3 = todos["Psalms 4:3"]
    viejo5, nuevo5 = todos["Psalms 4:5"]
    assert viejo3 == "" and nuevo3.startswith("Oh hijos de los hombres")
    assert nuevo5.startswith("Enojaos, y no querais pecar mas")
    assert nuevo3 not in nuevo5
    # Solo se quita el prefijo del v. 3: el resto del v. 5 no se toca.
    assert viejo5 == nuevo3 + " " + nuevo5
    assert parche.CORRECCIONES["Psalms 4:5"][2] == "tomo III, hoja 11"


def _hay_sword():
    return all(shutil.which(h) for h in ("imp2vs", "mod2imp"))


CONF = """[TorresAmat]
DataPath=./modules/texts/ztext/torresamat/
ModDrv=zText
CompressType=ZIP
BlockType=BOOK
Encoding=UTF-8
SourceType=OSIS
Versification=Vulg
Lang=es
"""


def _arbol(raiz, entradas):
    """Árbol SWORD mínimo (mods.d/ + modules/) con estas entradas."""
    destino = os.path.join(raiz, "modules", "texts", "ztext", "torresamat")
    os.makedirs(destino)
    os.makedirs(os.path.join(raiz, "mods.d"))
    with open(os.path.join(raiz, "mods.d", "torresamat.conf"), "w",
              encoding="utf-8") as f:
        f.write(CONF)
    imp = os.path.join(raiz, "x.imp")
    with open(imp, "w", encoding="utf-8") as f:
        f.write(parche.escribe_imp(entradas))
    subprocess.run(["imp2vs", imp, "-z", "z", "-v", "Vulg", "-o", "."],
                   cwd=destino, check=True, capture_output=True)


def _parchea(origen, salida):
    return subprocess.run(
        [sys.executable, os.path.join(parche.DIR, "parche_facsimil.py"),
         "--origen", origen, "--salida", salida],
        capture_output=True, text=True)


def test_regenera_desde_origen_conocido_e_idempotente():
    """Desde un árbol SWORD cualquiera (no ~/.sword): cambian solo las
    referencias autorizadas, la segunda pasada no cambia nada y deja los
    mismos bytes, y un verso de control queda como estaba."""
    if not _hay_sword():
        print("  (sin imp2vs/mod2imp: se salta)")
        return
    control = ("Matthew 12:7", MT_12_7)
    viejos = [(ref, viejo) for ref, (viejo, _n) in parche.cambios().items()]
    with tempfile.TemporaryDirectory() as tmp:
        origen = os.path.join(tmp, "origen")
        _arbol(origen, viejos + [control])
        uno, dos = os.path.join(tmp, "uno"), os.path.join(tmp, "dos")
        r = _parchea(origen, uno)
        assert r.returncode == 0, r.stderr
        assert f"({len(viejos)} versos)" in r.stdout, r.stdout
        conf = parche.conf_de(uno)
        salida = dict(parche.lee_imp(parche.exporta_aislado(uno, conf)))
        origen_imp = parche.lee_imp(parche.exporta_aislado(origen, conf))
        autorizados, _frec = parche.autoriza(origen_imp)
        for ref, (_v, nuevo) in autorizados.items():
            assert salida[ref] == nuevo, ref
        assert salida["Matthew 12:7"] == MT_12_7
        # Segunda pasada sobre su propia salida: nada que cambiar.
        r = _parchea(uno, dos)
        assert r.returncode == 0, r.stderr
        assert "(0 versos)" in r.stdout, r.stdout
        cmp = filecmp.dircmp(os.path.join(uno, "modules", "texts", "ztext",
                                          "torresamat"),
                             os.path.join(dos, "modules", "texts", "ztext",
                                          "torresamat"))
        assert not cmp.diff_files and not cmp.left_only and \
            not cmp.right_only, cmp.report()
        for f in cmp.common_files:
            assert filecmp.cmp(os.path.join(cmp.left, f),
                               os.path.join(cmp.right, f), shallow=False), f


def test_texto_inesperado_detiene_la_regeneracion():
    if not _hay_sword():
        return
    viejos = [(ref, viejo) for ref, (viejo, _n) in parche.cambios().items()]
    raro = [(r, "texto que nadie ha cotejado" if r == "Matthew 12:10" else t)
            for r, t in viejos]
    with tempfile.TemporaryDirectory() as tmp:
        _arbol(os.path.join(tmp, "origen"), raro)
        r = _parchea(os.path.join(tmp, "origen"), os.path.join(tmp, "uno"))
        assert r.returncode != 0 and "Matthew 12:10" in r.stderr, r.stderr


if __name__ == "__main__":
    for nombre, f in list(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
