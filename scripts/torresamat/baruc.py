"""Baruc completo de Torres Amat, cotejado y realineado (0.9.19 → 0.9.20).

El plan baruc.json contiene los 213 versículos y sus testigos. Las notas se
asocian de nuevo a las páginas impresas. Solo se escribe el árbol de salida;
antes de instalar, se verifica la ida y vuelta y todo el contenido ajeno a Baruc.
"""
import argparse
import collections
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

from barrido import reescribe
from parche_facsimil import conf_de, exporta_aislado, lee_imp, escribe_imp

DIR = Path(__file__).resolve().parent
ROOT = DIR.parent.parent
PLAN = DIR / "baruc.json"
HELPER = ROOT / "build/tests/torresamat_notes_patch"


def load_plan(path=PLAN):
    plan = json.loads(Path(path).read_text())
    counts = [22, 35, 38, 37, 9, 72]
    expected = [f"Baruch {c}:{v}" for c, n in enumerate(counts, 1) for v in range(1, n + 1)]
    records = plan["corrections"]
    if plan["chapter_counts"] != counts or [r["reference"] for r in records] != expected:
        raise ValueError("el plan no contiene los 213 versículos canónicos de Baruc")
    before = collections.Counter(t for r in records for t in re.findall(r"<[^>]+>", r["before"]))
    after = collections.Counter(t for r in records for t in re.findall(r"<[^>]+>", r["after"]))
    if before != after:
        raise ValueError("el plan pierde o inventa marcado OSIS")
    return plan


def correct(key, text, records):
    record = records.get(key)
    if record is None or text == record["after"]:
        return text, 0
    if text != record["before"]:
        raise ValueError(f"{key}: contenido distinto del cotejado; no se sobrescribe")
    return record["after"], 1


def snapshot(helper, root, output):
    result = subprocess.run([str(helper), "snapshot", str(root), str(output)],
                            check=True, text=True, capture_output=True)
    digest = re.search(r"notes_other_sha256=([a-f0-9]{64})", result.stdout)
    if not digest:
        raise ValueError("no se pudo verificar el comentario fuera de Baruc")
    return digest[1], dict(lee_imp(output.read_text()))


def compress_ranges(entries):
    """Link equal consecutive entries without dropping their coverage."""
    result = []
    start = end = body = None
    prev = None
    for key, text in entries:
        m = re.fullmatch(r"(.+) (\d+):(\d+)", key)
        current = (m[1], int(m[2]), int(m[3])) if m else None
        if text and prev and current and current[:2] == prev[:2] and \
                current[2] == prev[2] + 1 and body == text:
            end = current[2]
        else:
            if start is not None:
                result.append((start + (f"-{end}" if end is not None else ""), body))
            start = key if text else None
            end = None
            body = text
        prev = current
    if start is not None:
        result.append((start + (f"-{end}" if end is not None else ""), body))
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--origen", type=Path, default=ROOT / "modulos")
    ap.add_argument("--salida", type=Path, default=ROOT / "build/baruc-corregido")
    ap.add_argument("--plan", type=Path, default=PLAN)
    ap.add_argument("--notes-helper", type=Path, default=HELPER)
    args = ap.parse_args()
    if not args.notes_helper.is_file():
        raise ValueError("Compilar antes: cmake --build build --target torresamat_notes_patch")
    conf = conf_de(str(args.origen))
    if not re.search(r"^Version=0\.9\.(?:19|20)$", conf, re.M):
        raise ValueError("La reconstrucción requiere TorresAmat 0.9.19 o 0.9.20")
    plan = load_plan(args.plan)
    records = {r["reference"]: r for r in plan["corrections"]}
    total, verses, dest = reescribe(str(args.origen), str(args.salida),
                                  lambda k, t: correct(k, t, records))
    conf_path = args.salida / "mods.d/torresamat.conf"
    conf_path.write_text(re.sub(r"^Version=0\.9\.19$", "Version=0.9.20",
                               conf_path.read_text(), flags=re.M))
    # Preserve the existing compressed commentary, including its links and
    # every non-Baruc entry. Writes run only in the isolated output tree.
    src = args.origen / "modules/comments/zcom/torresamatnotas"
    dst = args.salida / "modules/comments/zcom/torresamatnotas"
    shutil.copytree(src, dst)
    notes_conf = args.origen / "mods.d/torresamatnotas.conf"
    shutil.copy2(notes_conf, args.salida / "mods.d/torresamatnotas.conf")
    before_hash, before_notes = snapshot(args.notes_helper, args.salida,
                                         args.salida / "notes-before.imp")
    expected_notes = {r["reference"]: r["after"] for r in plan["notes"]}
    for record in plan["notes"]:
        if before_notes[record["reference"]] not in (record["before"], record["after"]):
            raise ValueError(f"{record['reference']}: notas distintas de las cotejadas; no se sobrescriben")
    # zCom exposes a read-only module through SWMgr. Use the established
    # imp2vs writer over the full native export, preserving linked ranges.
    native_imp = args.salida / "notes-native-before.imp"
    subprocess.run([str(args.notes_helper), "export", str(args.salida), str(native_imp)],
                   check=True, text=True, capture_output=True)
    entries = []
    for key, text in lee_imp(native_imp.read_text()):
        m = re.fullmatch(r"Baruch (\d+):(\d+)", key)
        if m and int(m[1]) > 0 and int(m[2]) > 0:
            text = expected_notes[f"Bar.{m[1]}.{m[2]}"]
        entries.append((key, text))
    note_imp = args.salida / "notes-corrected.imp"
    note_imp.write_text(escribe_imp(compress_ranges(entries)))
    shutil.rmtree(dst)
    dst.mkdir(parents=True)
    subprocess.run(["imp2vs", str(note_imp.resolve()), "-z", "z", "-b", "3",
                    "-v", "Vulg", "-o", "."], cwd=dst, check=True, capture_output=True)
    after_hash, after_notes = snapshot(args.notes_helper, args.salida,
                                       args.salida / "notes-after.imp")
    if before_hash != after_hash or after_notes != expected_notes:
        raise ValueError("la reconstrucción alteró otras notas o no coincide con el cotejo")
    notes_path = args.salida / "mods.d/torresamatnotas.conf"
    notes_path.write_text(re.sub(r"^Version=0\.9\.5$", "Version=0.9.6",
                                notes_path.read_text(), flags=re.M))
    verification = {"bible_changed_entries": total, "bible_changed_verses": verses,
                    "baruc_verses": 213, "notes_other_sha256": after_hash,
                    "notes_changed_entries": sum(before_notes[k] != v for k, v in after_notes.items())}
    (args.salida / "baruc-verification.json").write_text(json.dumps(verification, indent=2) + "\n")
    print(f"{total} entradas bíblicas corregidas; Baruc completo, 213 versículos; "
          f"{verification['notes_changed_entries']} entradas de notas reubicadas")


if __name__ == "__main__":
    main()
