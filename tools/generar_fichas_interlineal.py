#!/usr/bin/env python3
"""Fichas enriquecidas del interlineal (ver interlineal_enriquecido_prompt.md).

Las fichas se escriben por capítulos en data/fichas_interlineal/<Libro>.<cap>.json
(arreglo de objetos con ref, strong, forma y los campos del prompt). Este script:

  --preparar Matt 2   imprime las palabras del capítulo (Tisch: forma, Strong,
                      lema, morfología) para redactar las fichas
  --instalar          junta todos los capítulos del repo en
                      ~/.config/xiphos/interlineal_enriquecido.json (lo que lee la app)
  --lista             reescribe y muestra tools/fichas_progreso.md

El progreso sale de los archivos del repo: un capítulo está hecho si existe
su archivo <Libro>.<cap 2 dígitos>.json. Solo llevan ficha las palabras con
algo que aportar; artículos y partículas triviales se omiten (regla 6).
"""
import argparse, glob, json, os, re, subprocess, sys

AQUI = os.path.dirname(os.path.abspath(__file__))
DIR_APP = os.path.join(os.environ.get("XDG_CONFIG_HOME", os.path.expanduser("~/.config")), "xiphos")
SALIDA = os.path.join(DIR_APP, "interlineal_enriquecido.json")
PROGRESO_MD = os.path.join(AQUI, "fichas_progreso.md")
MODELO = os.environ.get("FICHAS_MODELO", "claude-opus-5-5")

# (OSIS, nombre SWORD, español)
LIBROS = [
    ("Matt", "Matthew", "Mateo"), ("Mark", "Mark", "Marcos"), ("Luke", "Luke", "Lucas"),
    ("John", "John", "Juan"), ("Acts", "Acts", "Hechos"), ("Rom", "Romans", "Romanos"),
    ("1Cor", "I Corinthians", "1 Corintios"), ("2Cor", "II Corinthians", "2 Corintios"),
    ("Gal", "Galatians", "Gálatas"), ("Eph", "Ephesians", "Efesios"),
    ("Phil", "Philippians", "Filipenses"), ("Col", "Colossians", "Colosenses"),
    ("1Thess", "I Thessalonians", "1 Tesalonicenses"), ("2Thess", "II Thessalonians", "2 Tesalonicenses"),
    ("1Tim", "I Timothy", "1 Timoteo"), ("2Tim", "II Timothy", "2 Timoteo"),
    ("Titus", "Titus", "Tito"), ("Phlm", "Philemon", "Filemón"), ("Heb", "Hebrews", "Hebreos"),
    ("Jas", "James", "Santiago"), ("1Pet", "I Peter", "1 Pedro"), ("2Pet", "II Peter", "2 Pedro"),
    ("1John", "I John", "1 Juan"), ("2John", "II John", "2 Juan"), ("3John", "III John", "3 Juan"),
    ("Jude", "Jude", "Judas"), ("Rev", "Revelation of John", "Apocalipsis"),
]

def cargar(ruta, defecto):
    try:
        with open(ruta, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return defecto


def guardar(ruta, datos):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    tmp = ruta + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(datos, f, ensure_ascii=False, indent=1)
    os.replace(tmp, ruta)


def versiculos(sword):
    """{(cap, ver): [palabras]} del libro, desde Tisch."""
    out = subprocess.run(["mod2imp", "Tisch"], capture_output=True, check=True).stdout.decode("utf-8", "replace")
    res, actual = {}, None
    for linea in out.splitlines():
        if linea.startswith("$$$"):
            m = re.match(r"\$\$\$%s (\d+):(\d+)$" % re.escape(sword), linea)
            actual = (int(m[1]), int(m[2])) if m else None
            if actual:
                res[actual] = []
        elif actual:
            for m in re.finditer(r'<w lemma="([^"]*)" morph="([^"]*)">([^<]*)</w>', linea):
                st = re.search(r"strong:(G\d+)", m[1])
                lema = re.search(r"lemma\.Strong:(\S+)", m[1])
                mo = re.search(r"robinson:(\S+)", m[2])
                if st:
                    res[actual].append({"forma": m[3].strip(), "strong": st[1],
                                        "lema": lema[1] if lema else "",
                                        "morph": mo[1] if mo else ""})
    return {k: v for k, v in res.items() if v and k[0] > 0}



DATOS = os.path.join(os.path.dirname(AQUI), "data", "fichas_interlineal")


def capitulos(osis):
    """{cap: [versículos con ficha]} según los archivos del repo."""
    res = {}
    for ruta in glob.glob(os.path.join(DATOS, "%s.*.json" % osis)):
        for f in cargar(ruta, []):
            m = re.match(r"^%s\.(\d+)\.(\d+)$" % re.escape(osis), f.get("ref", ""))
            if m:
                res.setdefault(int(m[1]), set()).add(int(m[2]))
    return res


def escribir_estado():
    filas = ["# Progreso de fichas enriquecidas del interlineal", "",
             "Generado por `tools/generar_fichas_interlineal.py --lista`. No editar a mano.", "",
             "| Libro | Estado | Capítulos con fichas | Versículos con fichas |", "|---|---|---|---|"]
    for osis, sword, es in LIBROS:
        vs = versiculos(sword)
        caps_total = sorted({c for c, _ in vs})
        hechos = capitulos(osis)
        completos = [c for c in caps_total
                     if os.path.exists(os.path.join(DATOS, "%s.%02d.json" % (osis, c)))]
        nver = sum(len(x) for x in hechos.values())
        estado = "pendiente" if not hechos else ("hecho" if len(completos) == len(caps_total) else "en curso")
        filas.append("| %s (%s) | %s | %d/%d | %d/%d |" % (es, osis, estado, len(completos),
                                                         len(caps_total), nver, len(vs)))
    with open(PROGRESO_MD, "w", encoding="utf-8") as f:
        f.write("\n".join(filas) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preparar", nargs=2, metavar=("LIBRO", "CAP"))
    ap.add_argument("--instalar", action="store_true")
    ap.add_argument("--lista", action="store_true")
    a = ap.parse_args()
    if a.preparar:
        libro = next((l for l in LIBROS if l[0] == a.preparar[0]), None)
        if not libro:
            sys.exit("Libro desconocido. Códigos: " + " ".join(l[0] for l in LIBROS))
        cap = int(a.preparar[1])
        # Compacto: un Strong por versículo y sin artículos ni partículas triviales.
        triviales = {"G3588", "G1161", "G2532", "G846", "G1063", "G1510", "G3739", "G3778",
                     "G3754", "G3361", "G3756", "G4771", "G1473", "G2249", "G5209", "G3767"}
        for (c, v), ps in sorted(versiculos(libro[1]).items()):
            if c == cap:
                vistos, sal = set(), []
                for p in ps:
                    if p["strong"] in triviales or p["strong"] in vistos:
                        continue
                    vistos.add(p["strong"])
                    sal.append("%s|%s|%s|%s" % (p["forma"], p["strong"], p["lema"], p["morph"]))
                print("%s.%d.%d  %s" % (libro[0], c, v, " ".join(sal)))
    elif a.instalar:
        todas = []
        for ruta in sorted(glob.glob(os.path.join(DATOS, "*.json"))):
            todas += cargar(ruta, [])
        guardar(SALIDA, todas)
        print("%d fichas instaladas en %s" % (len(todas), SALIDA))
    else:
        escribir_estado()
        print(open(PROGRESO_MD, encoding="utf-8").read())


if __name__ == "__main__":
    main()
