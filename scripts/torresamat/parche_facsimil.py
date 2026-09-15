"""
Corrige versos sueltos del módulo TorresAmat ya compilado, cotejados a ojo
contra el facsímil de 1882.

Los datos de construcción (djvu.xml, texto.json) no viajan con el repo, así
que no se puede rehacer el módulo desde el principio. Este parche parte del
módulo instalado: lo exporta con mod2imp, sustituye solo las entradas de
CORRECCIONES -- y solo si el texto viejo coincide exactamente --, lo
reimporta con imp2vs y comprueba que la ida y vuelta no toca nada más.

Cada entrada lleva el tomo y la hoja del ítem de Internet Archive donde se
ha leído el impreso:

    https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111

Uso:

    python3 parche_facsimil.py [--salida DIR]

Deja el módulo corregido en DIR/modules/texts/ztext/torresamat (por defecto
salida/parche). Copiarlo a modulos/ y a ~/.sword es un paso aparte.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

DIR = os.path.dirname(os.path.abspath(__file__))
MODULO = "TorresAmat"
V11N = "Vulg"

# ref: (texto actual en el módulo, texto del impreso, dónde se ha leído)
CORRECCIONES = {
    # SALMO III. El «tú» de v. 4 salió como «44» y el v. 3 quedó pegado
    # delante del v. 5, dejando el 3 vacío.
    "Psalms 3:2": (
        "¡Ah Señor!;Cómo es que se han aumentado tanto mis perseguidores? "
        "Son muchísimos los que se han rebelado contra mí.",
        "¡Ah Señor! ¿Cómo es que se han aumentado tanto mis perseguidores? "
        "Son muchísimos los que se han rebelado contra mí.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:3": (
        "",
        "Muchos dicen de mí: Ya no tiene que esperar de su Dios salvacion "
        "ó amparo.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:4": (
        "Pero tú, oh Senor, 44 eres mi protector, mi gloria, y el que me "
        "haces levantar cabeza.",
        "Pero tú, oh Señor, tú eres mi protector, mi gloria, y el que me "
        "haces levantar cabeza.",
        "tomo III, hoja 11",
    ),
    "Psalms 3:5": (
        "Muchos dicen de mí: Ya no tiene que esperar de su Dios salvacion "
        "ó amparo. A veces clamé al Señor, y él me oyó benigno desde su "
        "santo monte",
        "A veces clamé al Señor, y él me oyó benigno desde su santo monte.",
        "tomo III, hoja 11",
    ),
    # SAN MATHEO, CAPITULO XII. El «$» es la llamada de nota 3 («Véase
    # Pan»), no texto; «ú solos» y «eon» son erratas de OCR, y el v. 11
    # perdió el renglón final.
    "Matthew 12:4": (
        "¿Cómo entró en la Casa de Dios, y comió los panes de la proposicion "
        "$, que no era lícito comer ni á él ni á los suyos, sino ú solos los "
        "sacerdotes?",
        "¿Cómo entró en la Casa de Dios, y comió los panes de la proposicion, "
        "que no era lícito comer ni á él ni á los suyos, sino á solos los "
        "sacerdotes?",
        "tomo IV, hoja 22",
    ),
    "Matthew 12:5": (
        "¿O no habeis leido en la Ley, cómo los sacerdotes en el templo "
        "trabajan en el sábado, y eon todo eso no pecan?",
        "¿O no habeis leido en la Ley, cómo los sacerdotes en el templo "
        "trabajan en el sábado, y con todo eso no pecan?",
        "tomo IV, hoja 22",
    ),
    "Matthew 12:11": (
        "Mas él les dijo: ¿Qué hombre habrá entre vosotros, que tenga una "
        "oveja, y si esta cae en una fosa en dia de",
        "Mas él les dijo: ¿Qué hombre habrá entre vosotros, que tenga una "
        "oveja, y si esta cae en una fosa en dia de sábado, no la levante y "
        "saque fuera?",
        "tomo IV, hoja 22",
    ),
}


def lee_imp(texto):
    """[(clave, cuerpo)] en el orden del fichero."""
    entradas, clave, cuerpo = [], None, []
    # escribe_imp termina cada cuerpo con su salto; el último no abre línea.
    if texto.endswith("\n"):
        texto = texto[:-1]
    for linea in texto.split("\n"):
        if linea.startswith("$$$"):
            if clave is not None:
                entradas.append((clave, "\n".join(cuerpo)))
            clave, cuerpo = linea[3:], []
        elif clave is not None:
            cuerpo.append(linea)
    if clave is not None:
        entradas.append((clave, "\n".join(cuerpo)))
    return entradas


def escribe_imp(entradas):
    return "".join(f"$$${clave}\n{cuerpo}\n" for clave, cuerpo in entradas)


def exporta(sword_path=None, nombre=MODULO):
    env = dict(os.environ)
    if sword_path:
        env["SWORD_PATH"] = sword_path
    r = subprocess.run(["mod2imp", nombre], capture_output=True, text=True,
                       env=env, check=True)
    return r.stdout


def conf_instalada():
    return os.path.join(os.path.expanduser("~"), ".sword", "mods.d",
                        "torresamat.conf")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--salida", default=os.path.join(DIR, "salida", "parche"))
    args = ap.parse_args()

    original = exporta()
    entradas = lee_imp(original)
    if escribe_imp(entradas) != original:
        sys.exit("el export no se puede reescribir sin pérdida; no se toca")

    pendientes = dict(CORRECCIONES)
    nuevas = []
    for clave, cuerpo in entradas:
        if clave in pendientes:
            viejo, nuevo, _hoja = pendientes.pop(clave)
            if cuerpo == nuevo:
                print(f"  ya corregido: {clave}")
            elif cuerpo != viejo:
                sys.exit(f"{clave}: el módulo no trae el texto esperado:\n"
                         f"  {cuerpo!r}")
            else:
                print(f"  corrige {clave}")
            cuerpo = nuevo
        nuevas.append((clave, cuerpo))
    if pendientes:
        sys.exit(f"claves no encontradas: {sorted(pendientes)}")
    parcheado = escribe_imp(nuevas)

    destino = os.path.join(args.salida, "modules", "texts", "ztext",
                           "torresamat")
    shutil.rmtree(args.salida, ignore_errors=True)
    os.makedirs(destino)
    with tempfile.TemporaryDirectory() as tmp:
        imp = os.path.join(tmp, "torresamat.imp")
        with open(imp, "w", encoding="utf-8") as f:
            f.write(parcheado)
        subprocess.run(["imp2vs", imp, "-z", "z", "-v", V11N, "-o", "."],
                       cwd=destino, check=True, capture_output=True)

        # Ida y vuelta: el módulo nuevo debe exportar exactamente el imp
        # parcheado, y diferir del original solo en CORRECCIONES. Va con
        # otro nombre: SWORD suma ~/.sword a SWORD_PATH, y con el mismo
        # nombre podría leerse el módulo instalado en lugar del nuevo.
        prueba = MODULO + "Parche"
        os.makedirs(os.path.join(tmp, "mods.d"))
        os.symlink(os.path.abspath(os.path.join(args.salida, "modules")),
                   os.path.join(tmp, "modules"))
        with open(conf_instalada(), encoding="utf-8") as src:
            conf = src.read().replace(f"[{MODULO}]", f"[{prueba}]", 1)
        with open(os.path.join(tmp, "mods.d", "torresamatparche.conf"), "w",
                  encoding="utf-8") as dst:
            dst.write(conf)
        vuelta = exporta(tmp, prueba)
    if vuelta != parcheado:
        sys.exit("la ida y vuelta del módulo parcheado no coincide")
    cambiadas = [c for (c, a), (_, b) in zip(entradas, lee_imp(vuelta))
                 if a != b]
    esperadas = [c for c in cambiadas if c in CORRECCIONES]
    if cambiadas != esperadas:
        sys.exit(f"cambió algo fuera de CORRECCIONES: {cambiadas}")
    print(f"módulo parcheado en {destino} ({len(cambiadas)} versos)")


if __name__ == "__main__":
    main()
