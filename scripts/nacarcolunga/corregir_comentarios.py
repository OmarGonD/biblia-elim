"""Corrige y reconstruye el módulo local NacarColungaNotas.

Parte de la exportación del módulo instalado para no repetir el OCR completo.
Las reglas viven en comentario.py y también se aplican en futuras generaciones.
"""
import argparse
from datetime import datetime
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from canon import CANON
from comentario import CORRECCIONES_OCR, corrige_ocr


def extiende_rangos_capitulares(imp):
    """Restaura los rangos que mod2imp omite al exportar enlaces zCom."""
    import re

    por_nombre = {libro["nombre"]: libro for libro in CANON}
    patron = re.compile(r"^\$\$\$(.+) (\d+):(\d+)$", re.MULTILINE)

    def rango(coincidencia):
        nombre, cap_s, verso_s = coincidencia.groups()
        libro = por_nombre.get(nombre)
        cap = int(cap_s)
        verso = int(verso_s)
        if (not libro or verso not in (0, 1) or cap < 1 or
                cap > len(libro["versos"])):
            return coincidencia.group(0)
        ultimo = libro["versos"][cap - 1]
        return f"$$${libro['osis']} {cap}:1-{ultimo}"

    return patron.sub(rango, imp)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--instalar", action="store_true")
    args = parser.parse_args()

    original = subprocess.check_output(
        ["mod2imp", "NacarColungaNotas"], text=True,
        stderr=subprocess.DEVNULL)
    corregido = extiende_rangos_capitulares(corrige_ocr(original))
    if corregido.count("\n$$$") != original.count("\n$$$"):
        raise RuntimeError("La corrección alteró el número de entradas del módulo")
    cambios = []
    for patron, _ in CORRECCIONES_OCR:
        import re
        cantidad = len(re.findall(patron, original))
        if cantidad:
            cambios.append((patron, cantidad))

    print(f"Correcciones aplicadas: {sum(n for _, n in cambios)}")
    for patron, cantidad in cambios:
        print(f"  {cantidad:2}  {patron}")
    if not args.instalar:
        print("Vista previa solamente; usa --instalar para reconstruir el módulo.")
        return

    sword_dir = Path(os.environ.get("SWORD_PATH", Path.home() / ".sword"))
    destino = sword_dir / "modules/comments/zcom/nacarcolunganotas"
    with tempfile.TemporaryDirectory(prefix="nacar-notas-") as td:
        temporal = Path(td)
        imp = temporal / "nacarcolunga-notas.imp"
        datos = temporal / "datos"
        datos.mkdir()
        imp.write_text(corregido, encoding="utf-8")
        subprocess.run(
            ["imp2vs", str(imp), "-z", "z", "-b", "3", "-v", "NRSVA",
             "-o", str(datos)],
            check=True, stdout=subprocess.DEVNULL,
        )
        generados = [archivo for archivo in datos.iterdir()
                     if archivo.suffix in (".czs", ".czv", ".czz",
                                           ".bzs", ".bzv", ".bzz")]
        if not generados or any(archivo.stat().st_size == 0
                                for archivo in generados):
            nombres = ", ".join(archivo.name for archivo in datos.iterdir())
            raise RuntimeError(
                f"imp2vs no produjo un módulo válido (salida: {nombres})")
        if destino.exists():
            sello = datetime.now().strftime("%Y%m%d-%H%M%S")
            respaldo = destino.with_name(
                f"nacarcolunganotas.respaldo-{sello}")
            shutil.copytree(destino, respaldo)
            print(f"Respaldo del módulo anterior: {respaldo}")
        destino.mkdir(parents=True, exist_ok=True)
        for testamento in ("ot", "nt"):
            for letra in ("s", "v", "z"):
                origen = datos / f"{testamento}.cz{letra}"
                if not origen.exists():
                    origen = datos / f"{testamento}.bz{letra}"
                if origen.exists():
                    shutil.copy2(origen,
                                 destino / f"{testamento}.cz{letra}")
    print(f"Módulo corregido instalado en {destino}")


if __name__ == "__main__":
    main()
