"""
Adquisición reproducible de los testigos de la edición 1832-1835.

    python3 fetch_source.py --list
    python3 fetch_source.py --witness ia-lasagradabiblia01unkngoog

No toca ~/.sword ni ningún dato del usuario: descarga a una caché local
(por defecto build/torresamat1835-cache, o TORRESAMAT1835_CACHE) y
verifica SHA-256 contra data/torresamat1835/source_manifest.json.

Lo que NO hace, a propósito: aceptar un fichero cuyo checksum no esté
registrado. Un testigo que cambia debajo es exactamente lo que arruina la
reproducibilidad, así que se para y lo dice. Para registrar un checksum
nuevo hace falta --record, que es un acto humano y deliberado.
"""
import argparse
import hashlib
import json
import os
import sys
import urllib.request

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
MANIFEST = os.path.join(ROOT, "data", "torresamat1835", "source_manifest.json")
DEFAULT_CACHE = os.environ.get(
    "TORRESAMAT1835_CACHE",
    os.path.join(ROOT, "build", "torresamat1835-cache"))


class ManifestError(Exception):
    """El testigo no es el que el manifest dice."""


def sha256_of(path, chunk=1 << 20):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(chunk), b""):
            digest.update(block)
    return digest.hexdigest()


def verify_file(path, expected):
    """True si el fichero es el registrado; ManifestError si no.

    `expected` a None significa «no hay checksum registrado»: eso no se
    acepta en silencio, que es precisamente el agujero que se quiere
    evitar.
    """
    if not expected:
        raise ManifestError(
            f"{os.path.basename(path)}: no hay sha256 registrado en el "
            f"manifest; usa --record tras comprobar la procedencia")
    actual = sha256_of(path)
    if actual != expected:
        raise ManifestError(
            f"{os.path.basename(path)}: sha256 {actual} != {expected} del "
            f"manifest; la fuente ha cambiado, no se continúa")
    return True


def load_manifest(path=MANIFEST):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def witnesses(manifest):
    """Testigos de todos los tomos, indexados por id (esquema v2).

    Un mismo testigo (el juego de la BNE) aparece en varios tomos; aquí
    se funde en una entrada que recuerda a qué tomos pertenece y qué
    ficheros de esos tomos le corresponden.
    """
    found = {}
    for volume in manifest["volumes"]:
        for witness in volume.get("witnesses", []):
            entry = found.setdefault(witness["id"], dict(witness, volumes=[],
                                                         files=[]))
            entry["volumes"].append(volume["volume"])
            for handle in volume.get("files", []):
                if handle.get("witness") == witness["id"]:
                    entry["files"].append(dict(handle, volume=volume["volume"]))
    return found


def describe(witness):
    rights = witness.get("rights", {})
    lines = [f"  {witness['id']}  [{witness.get('role', '?')}]",
             f"    institución    {witness.get('institution', '?')}",
             f"    tomos          {witness.get('volumes', [])}"]
    for key, value in (("url", witness.get("url") or witness.get("record_url")),
                       ("persistent_id", witness.get("persistent_id")),
                       ("acceso", witness.get("automated_access")),
                       ("redistribución", rights.get("redistribution_allowed")),
                       ("uso comercial", rights.get("commercial_use")),
                       ("release", "sí" if witness.get(
                           "may_produce_release_artifact") else "NO")):
        if value:
            lines.append(f"    {key:14} {value}")
    if rights.get("note"):
        lines.append(f"    aviso          {rights['note']}")
    return "\n".join(lines)


def fetch(witness, cache=DEFAULT_CACHE, record=False, retries=3):
    if witness.get("automated_access") == "blocked":
        raise ManifestError(
            f"{witness['id']}: la fuente no admite descarga automatizada. "
            f"Descarga los tomos a mano y regístralos con register_source.py.")
    files = witness.get("files") or []
    if not files:
        raise ManifestError(f"{witness['id']}: el manifest no lista ficheros")

    os.makedirs(cache, exist_ok=True)
    print(describe(witness))
    done = []
    for entry in files:
        target = os.path.join(cache, entry["filename"])
        if not os.path.exists(target):
            last = None
            for attempt in range(1, retries + 1):
                try:
                    print(f"    descargando ({attempt}/{retries}) {entry['filename']}")
                    urllib.request.urlretrieve(entry["url"], target + ".part")
                    os.replace(target + ".part", target)
                    break
                except Exception as exc:          # noqa: BLE001
                    last = exc
            else:
                raise ManifestError(f"{entry['filename']}: no se pudo bajar: {last}")
        else:
            print(f"    en caché {entry['filename']}")

        # Registrar un checksum nuevo es cosa de register_source.py, que
        # escribe en el tomo que corresponde. Aquí sólo se verifica.
        verify_file(target, entry.get("sha256"))
        print("    sha256 ok")
        done.append(target)
    return done


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--witness")
    ap.add_argument("--cache", default=DEFAULT_CACHE)
    ap.add_argument("--record", action="store_true",
                    help="registra el sha256 de un fichero nuevo (acto humano)")
    args = ap.parse_args()

    manifest = load_manifest()
    known = witnesses(manifest)
    if args.list or not args.witness:
        edition = manifest["edition"]
        print(f"edición: {manifest['display_name']}  "
              f"({edition['years']}, {edition['publisher']})")
        for witness in known.values():
            print(describe(witness))
        print("\nEl estado por tomo está en register_source.py --status")
        return 0

    if args.witness not in known:
        sys.exit(f"testigo desconocido: {args.witness}")
    try:
        paths = fetch(known[args.witness], cache=args.cache, record=args.record)
    except ManifestError as exc:
        sys.exit(f"ERROR: {exc}")
    if args.record:
        with open(MANIFEST, "w", encoding="utf-8") as handle:
            json.dump(manifest, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
    print(f"  {len(paths)} fichero(s) verificados en {args.cache}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
