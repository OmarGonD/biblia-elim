"""
Registro y verificación de un tomo descargado a mano.

BNE no admite descarga automatizada (su dominio devuelve 403 a este tipo
de cliente), así que los tomos se bajan con un navegador. Esto es lo que
convierte esa descarga humana en un artefacto reproducible:

    python3 register_source.py --status
    python3 register_source.py --volume 1 --file ~/Descargas/tomo1.pdf
    python3 register_source.py --volume 1 --file ~/Descargas/tomo1.pdf --record

Sin --record nunca se escribe un checksum: se compara contra el que ya
hay y se falla si no cuadra. --record es un acto humano explícito, para
la primera vez que un tomo entra en el proyecto.

El fichero se copia a la caché (build/torresamat1835-cache por defecto).
Nada toca ~/.sword ni ningún módulo del usuario.
"""
import argparse
import hashlib
import json
import os
import shutil
import sys

DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(DIR))
MANIFEST = os.path.join(ROOT, "data", "torresamat1835", "source_manifest.json")


class SourceError(Exception):
    """El artefacto no es el que el manifest describe."""


def sha256_of(path, chunk=1 << 20):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(chunk), b""):
            digest.update(block)
    return digest.hexdigest()


def load_manifest(path=MANIFEST):
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def cache_dir(manifest, override=None):
    if override:
        return override
    env = os.environ.get(manifest["cache"]["env_override"])
    return env or os.path.join(ROOT, manifest["cache"]["default"])


def find_volume(manifest, number):
    for volume in manifest["volumes"]:
        if volume["volume"] == number:
            return volume
    raise SourceError(f"el manifest no tiene un tomo {number}")


def entry_for(volume, filename):
    for entry in volume.get("files", []):
        if entry["filename"] == filename:
            return entry
    return None


def verify_artifact(path, entry, *, record=False):
    """Comprueba que `path` es el artefacto que `entry` describe.

    Un nombre de fichero no es identidad: se comprueban tamaño y SHA-256.
    Devuelve el digest calculado.
    """
    if not os.path.isfile(path):
        raise SourceError(f"no existe: {path}")
    size = os.path.getsize(path)
    if size == 0:
        raise SourceError(f"{os.path.basename(path)}: fichero vacío")

    expected_size = entry.get("size")
    if expected_size is not None and size != expected_size:
        raise SourceError(
            f"{entry['filename']}: {size} bytes, el manifest dice "
            f"{expected_size}; no es el mismo artefacto")

    digest = sha256_of(path)
    expected = entry.get("sha256")
    if expected is None:
        if not record:
            raise SourceError(
                f"{entry['filename']}: no hay sha256 registrado. Comprueba la "
                f"procedencia y vuelve a ejecutar con --record para fijarlo "
                f"(sha256 de este fichero: {digest})")
        return digest
    if digest != expected:
        raise SourceError(
            f"{entry['filename']}: sha256 {digest} != {expected} del manifest; "
            f"el artefacto ha cambiado, no se registra")
    return digest


def register(number, path, *, manifest_path=MANIFEST, cache=None, record=False,
             filename=None):
    manifest = load_manifest(manifest_path)
    volume = find_volume(manifest, number)
    name = filename or os.path.basename(path)

    entry = entry_for(volume, name)
    if entry is None:
        if not record:
            raise SourceError(
                f"tomo {number}: el manifest no describe ningún fichero "
                f"«{name}». Un PDF no es el tomo que dice su nombre: añádelo "
                f"con --record sólo si has comprobado de dónde sale")
        entry = {"format": os.path.splitext(name)[1].lstrip(".").upper() or None,
                 "filename": name, "url": None, "size": None, "sha256": None,
                 "page_count": None, "witness": None,
                 "acquisition": "manual", "verified": False}
        volume.setdefault("files", []).append(entry)

    digest = verify_artifact(path, entry, record=record)

    target_dir = cache_dir(manifest, cache)
    os.makedirs(target_dir, exist_ok=True)
    target = os.path.join(target_dir, name)
    if os.path.abspath(path) != os.path.abspath(target):
        shutil.copy2(path, target)

    if record:
        entry["sha256"] = digest
        entry["size"] = os.path.getsize(target)
        entry["verified"] = True
        entry["acquisition"] = entry.get("acquisition") or "manual"
        volume["status"] = "verified"
        with open(manifest_path, "w", encoding="utf-8") as handle:
            json.dump(manifest, handle, ensure_ascii=False, indent=2)
            handle.write("\n")
    return {"volume": number, "filename": name, "sha256": digest,
            "cached": target, "recorded": record}


def status(manifest_path=MANIFEST, cache=None):
    """Estado por tomo, separando metadata de artefactos.

    Son cuatro cosas distintas y antes se contaban como una sola, lo que
    daba «artefactos verificados 0/6» con un artefacto verificado en la
    mano:

      * la metadata de la edición (portada, imprenta, años, tomos);
      * la metadata de cada tomo (año, contenido, paginación);
      * los artefactos adquiridos (el fichero está en la caché);
      * los artefactos verificados (además cuadra su sha256).

    Y la cuenta que de verdad importa para la Biblia es sobre los tomos
    1-5: el 6 son las notas en forma de diccionario.
    """
    manifest = load_manifest(manifest_path)
    target_dir = cache_dir(manifest, cache)
    rows = []
    for volume in manifest["volumes"]:
        files = volume.get("files", [])
        cached, verified = [], []
        for entry in files:
            path = os.path.join(target_dir, entry["filename"])
            if not os.path.isfile(path):
                continue
            cached.append(entry)
            try:
                verify_artifact(path, entry)
            except SourceError:
                continue
            verified.append(entry)
        rows.append({
            "volume": volume["volume"],
            "year": volume.get("publication_year"),
            "corpus_role": volume.get("corpus_role"),
            "required_for_biblical_corpus":
                bool(volume.get("required_for_biblical_corpus")),
            "metadata": volume.get("metadata_status"),
            "acquisition": volume.get("acquisition"),
            "status": volume.get("status"),
            "files_described": len(files),
            "files_with_checksum": sum(1 for f in files if f.get("sha256")),
            "files_cached": len(cached),
            "files_verified": len(verified),
            "release_capable_files": sum(
                1 for f in verified if f.get("may_produce_release_artifact")),
            "witness_roles": sorted({w.get("role") for w in volume["witnesses"]
                                     if w.get("role")}),
        })
    return manifest, target_dir, rows


def summarise(manifest, rows):
    """Los conteos, cada uno con su nombre."""
    corpus = [r for r in rows if r["required_for_biblical_corpus"]]
    extra = [r for r in rows if not r["required_for_biblical_corpus"]]

    def acquired(group):
        return sum(1 for r in group if r["files_cached"])

    def verified(group):
        return sum(1 for r in group if r["files_verified"])

    edition = manifest.get("edition", {})
    edition_ok = all(edition.get(f) for f in
                     ("edition_statement", "city", "publisher", "years",
                      "volumes_total", "versification"))
    return {
        "edition_metadata": "verified" if edition_ok else "incomplete",
        "volumes_total": len(rows),
        "volume_metadata_verified": sum(1 for r in rows
                                        if r["metadata"] == "verified"),
        "biblical_corpus_volumes": len(corpus),
        "biblical_corpus_metadata_verified":
            sum(1 for r in corpus if r["metadata"] == "verified"),
        "artifacts_acquired": acquired(rows),
        "artifacts_verified": verified(rows),
        "biblical_corpus_artifacts_acquired": acquired(corpus),
        "biblical_corpus_artifacts_verified": verified(corpus),
        "release_capable_artifacts_verified":
            sum(1 for r in rows if r["release_capable_files"]),
        "supplementary_volumes": len(extra),
        "biblical_corpus_ready": verified(corpus) == len(corpus),
    }


def print_status(manifest_path=MANIFEST, cache=None):
    manifest, target_dir, rows = status(manifest_path, cache)
    print(manifest["bibliographic_citation"])
    print(f"cache: {target_dir}\n")
    for row in rows:
        print(f"Volume {row['volume']}:")
        print(f"    year              : {row['year'] or '(unknown)'}")
        print(f"    corpus role       : {row['corpus_role']}"
              f"{'' if row['required_for_biblical_corpus'] else '  (not Bible text)'}")
        print(f"    volume metadata   : {row['metadata']}")
        print(f"    artifacts         : {row['files_verified']} verified / "
              f"{row['files_cached']} acquired / {row['files_described']} described"
              f"  ({row['files_with_checksum']} with sha256)")
        print(f"    acquisition       : {row['acquisition']}")
        print(f"    witness roles     : {', '.join(row['witness_roles']) or '-'}")
        print(f"    status            : {row['status']}")

    s = summarise(manifest, rows)
    total, corpus = s["volumes_total"], s["biblical_corpus_volumes"]
    print()
    print(f"edition metadata                 : {s['edition_metadata']}")
    print(f"volume metadata                  : "
          f"{s['volume_metadata_verified']}/{total} verified")
    print(f"biblical corpus volumes          : {corpus} "
          f"(volumes {', '.join(str(r['volume']) for r in rows if r['required_for_biblical_corpus'])})")
    print(f"  metadata                       : "
          f"{s['biblical_corpus_metadata_verified']}/{corpus} verified")
    print(f"  artifacts acquired             : "
          f"{s['biblical_corpus_artifacts_acquired']}/{corpus}")
    print(f"  artifacts verified             : "
          f"{s['biblical_corpus_artifacts_verified']}/{corpus}")
    print(f"supplementary volumes            : {s['supplementary_volumes']} "
          f"(volume 6, notes dictionary -- not needed for the Bible corpus)")
    print(f"artifacts acquired (all volumes) : {s['artifacts_acquired']}/{total}")
    print(f"artifacts verified (all volumes) : {s['artifacts_verified']}/{total}")
    print(f"release-capable artifacts        : "
          f"{s['release_capable_artifacts_verified']}/{corpus} verified "
          f"(only a distributable_primary witness may ship)")
    print(f"biblical corpus ready            : "
          f"{'YES' if s['biblical_corpus_ready'] else 'NO'}")
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--status", action="store_true")
    ap.add_argument("--volume", type=int)
    ap.add_argument("--file")
    ap.add_argument("--cache")
    ap.add_argument("--record", action="store_true",
                    help="fija el sha256 de un artefacto nuevo (acto humano)")
    args = ap.parse_args()

    if args.status or not args.volume:
        print_status(cache=args.cache)
        return 0
    if not args.file:
        sys.exit("hace falta --file con el tomo descargado")
    try:
        done = register(args.volume, args.file, cache=args.cache,
                        record=args.record)
    except SourceError as exc:
        sys.exit(f"ERROR: {exc}")
    print(f"tomo {done['volume']}: {done['filename']}")
    print(f"  sha256 {done['sha256']}")
    print(f"  caché  {done['cached']}")
    print("  registrado en el manifest" if done["recorded"] else "  verificado")
    return 0


if __name__ == "__main__":
    sys.exit(main())
