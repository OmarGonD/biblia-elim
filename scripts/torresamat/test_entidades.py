"""Restos de entidades escapadas dos veces (TORRES-ENTITY-101).

    python3 test_entidades.py

Sal 147:6 decía «Él despide el granizo en menudos pedazos
&amp;amp;##x27;: al rigor de su frio…»: el «'» del OCR es una mota, osis2mod
lo dejó como «&amp;##x27;» y la regeneración de 9c036c87 lo escapó otra vez.
"""
import collections
import os
import shutil
import subprocess
import tempfile

import entidades
import osis
import parche_facsimil as parche

APOS = "&amp;amp;##x27;"

# Texto del módulo (mod2imp TorresAmat) antes de TORRES-ENTITY-101.
SAL_147_6 = ("Él despide el granizo en menudos pedazos " + APOS + ": al "
             "rigor de su frio ¿quién resistirá?")
SAM_19_20 = ("Envió " + APOS + " pues Saul =&amp;gt; soldados: para prender "
             "á David")
# Léxico mínimo: lo que el módulo trae en otros versículos.
FREC = collections.Counter({"celebrado": 13, "en": 20039, "no": 9266,
                            "que": 28748, "el": 25562, "de": 55022})


def test_sal_147_6():
    assert entidades.quita(SAL_147_6, FREC) == (
        "Él despide el granizo en menudos pedazos: al rigor de su frio "
        "¿quién resistirá?")


def test_entre_espacios_y_pegado_por_un_lado():
    assert entidades.quita(SAM_19_20, FREC) == (
        "Envió pues Saul = soldados: para prender á David")
    assert entidades.quita("otros &amp;gt;soldados", FREC) == "otros soldados"
    assert entidades.quita("llamó Gersan" + APOS + ", diciendo", FREC) == \
        "llamó Gersan, diciendo"


def test_final_de_versiculo_y_marcado():
    # Al final, limpia_final se comió el «;» de la entidad.
    assert entidades.quita("siete &amp;amp;##x27", FREC) == "siete"
    assert entidades.quita("boca. &amp;amp", FREC) == "boca."
    assert entidades.quita(
        'los celos.&amp;amp;##x27 <chapter eID="gen1" osisID="Num.5"/>',
        FREC) == 'los celos. <chapter eID="gen1" osisID="Num.5"/>'
    assert entidades.quita("frutos " + APOS + APOS + " du", FREC) == \
        "frutos du"
    # Entidad partida entre Ez 33:11 y Ez 33:12.
    assert entidades.quita("##x27; 12. Tú pues", FREC) == "12. Tú pues"


def test_entre_palabras_espacio_y_dentro_de_palabra_se_une():
    assert entidades.quita("celebrado" + APOS + "en todo", FREC) == \
        "celebrado en todo"
    # La palabra de la derecha no sale en otro sitio, pero «No» es
    # corriente y la derecha empieza palabra.
    assert entidades.quita("No" + APOS + "prostituyas", FREC) == \
        "No prostituyas"
    # «testig» no es palabra: el resto estaba dentro de «testigos».
    assert entidades.quita("testig" + APOS + "os", FREC) == "testigos"
    assert entidades.quita("c" + APOS + "eneral", FREC) == "ceneral"


def test_no_toca_entidades_legitimas_ni_marcado():
    t = ('<seg type="x-psalm-title">Aleluya.</seg> Alaba &amp; canta '
         '&lt;x&gt; <chapter eID="gen1" osisID="Ps.147"/>')
    assert entidades.quita(t, FREC) == t


def test_lexico_no_cuenta_lo_que_toca_un_resto():
    frec = entidades.lexico(["lare" + APOS + "o camino", "largo camino"])
    assert frec["lare"] == 0 and frec["o"] == 0
    assert frec["largo"] == 1 and frec["camino"] == 2


def test_quita_restos_solo_quita_restos():
    entradas = [("Psalms 147:6", SAL_147_6), ("Psalms 147:7", "Pero él")]
    nuevas, limpias = parche.quita_restos(entradas, FREC)
    assert limpias == {"Psalms 147:6"}
    assert dict(nuevas)["Psalms 147:7"] == "Pero él"
    assert "&amp;" not in dict(nuevas)["Psalms 147:6"]


def test_osis_no_emite_referencias_numericas():
    t = osis.contenido_verso("menudos pedazos ': al rigor = > soldados",
                             FREC)
    assert t == "menudos pedazos: al rigor = soldados", t
    assert "&#" not in osis.contenido_verso("celebrado'en todo", FREC)
    # «&» impreso («&c.») se escapa una sola vez.
    assert osis.contenido_verso("y lo demás &c.") == "y lo demás &amp;c."


def _hay_sword():
    return shutil.which("osis2mod") and shutil.which("mod2imp")


def test_osis2mod_ida_y_vuelta():
    """El OSIS de osis.py llega al módulo sin restos."""
    if not _hay_sword():
        print("  (sin osis2mod/mod2imp: se salta)")
        return
    texto = {"Ps 147:6": "Él despide el granizo en menudos pedazos ': al "
                         "rigor de su frio ¿quién resistirá?"}
    with tempfile.TemporaryDirectory() as tmp:
        xml = os.path.join(tmp, "t.osis.xml")
        osis.genera(texto, xml, orden=["Ps"])
        mod = os.path.join(tmp, "modules", "texts", "ztext", "t")
        os.makedirs(mod)
        os.makedirs(os.path.join(tmp, "mods.d"))
        subprocess.run(["osis2mod", mod, xml, "-z", "z", "-v", "Vulg"],
                       check=True, capture_output=True)
        with open(os.path.join(tmp, "mods.d", "t.conf"), "w") as f:
            f.write("[EntidadesPrueba]\nDataPath=./modules/texts/ztext/t/\n"
                    "ModDrv=zText\nSourceType=OSIS\nEncoding=UTF-8\n"
                    "Versification=Vulg\nCompressType=ZIP\nBlockType=BOOK\n")
        imp = parche.exporta(tmp, "EntidadesPrueba")
        v = dict(parche.lee_imp(imp))["Psalms 147:6"]
        assert v.startswith("Él despide el granizo en menudos pedazos: al "
                            "rigor de su frio"), v
        assert "##x27" not in imp and "&amp;" not in imp


if __name__ == "__main__":
    for nombre, f in list(globals().items()):
        if nombre.startswith("test_"):
            f()
            print("ok", nombre)
