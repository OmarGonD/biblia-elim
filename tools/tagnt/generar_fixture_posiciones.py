#!/usr/bin/env python3
"""Genera tests/data/posiciones_tisch.json: entradas crudas de Tisch y las posiciones que da el alineamiento (alinear.py).
El test en C (tests/sqlite_interlinear_test.cc) comprueba que parse_w_tags cuenta igual. Reproducible (semilla fija)."""
import json, os, random, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import alinear

SALIDA = os.path.join(alinear.RAIZ, "tests", "data", "posiciones_tisch.json")
OBLIGATORIOS = ["John.1.1", "John.1.4", "Matt.5.4", "Matt.5.5"]   # 1:4 lectura propia de Tisch; Mt 5:4-5 reordenados
SINTETICO = ('<w lemma="strong:G1 lemma.Strong:α" morph="robinson:N-NSM">α</w> '
             '<w lemma="strong:G2 lemma.Strong:β" morph="robinson:N-NSM"></w> '
             '<w lemma="strong:G3 lemma.Strong:γ" morph="robinson:PREP">γ·</w> '
             '<w morph="robinson:CONJ">δ</w> '
             '<w lemma="strong:G5 lemma.Strong:ε" morph="robinson:CONJ">ε</w>')


def main():
    out = subprocess.run(["mod2imp", "Tisch"], capture_output=True, check=True).stdout.decode("utf-8", "replace")
    crudo, actual = {}, None
    for l in out.splitlines():
        m = re.match(r"\$\$\$(.+) (\d+):(\d+)$", l)
        if m:
            o = alinear.SWORD_A_OSIS.get(m[1])
            actual = "%s.%d.%d" % (o, int(m[2]), int(m[3])) if o and int(m[2]) > 0 else None
        elif actual and l.strip():
            crudo[actual] = l
    tisch = alinear.leer_tisch(out)
    ref = lambda r: tuple(int(x) if x.isdigit() else x for x in r.split("."))
    todos = sorted((r for r in crudo if ref(r) in tisch), key=ref)
    todos = [r for r in todos if not (r.startswith("John.8.") and 12 <= int(r.split(".")[2]) <= 53)]
    azar = random.Random(20261002).sample([r for r in todos if r not in OBLIGATORIOS], 36)
    items = []
    for r in OBLIGATORIOS + sorted(azar, key=ref):
        items.append({"ref": r, "raw": crudo[r],
                      "palabras": [{"pos": w["pos"], "forma": w["forma"], "strong": w["strong"]} for w in tisch[ref(r)]]})
    sint = alinear.leer_tisch("$$$John 1:1\n" + SINTETICO + "\n")[("John", 1, 1)]
    items.append({"ref": "sintetico", "raw": SINTETICO,
                  "palabras": [{"pos": w["pos"], "forma": w["forma"], "strong": w["strong"]} for w in sint]})
    os.makedirs(os.path.dirname(SALIDA), exist_ok=True)
    with open(SALIDA, "w", encoding="utf-8") as f:
        json.dump(items, f, ensure_ascii=False, indent=1)
        f.write("\n")
    print(len(items), "entradas en", SALIDA)


if __name__ == "__main__":
    main()
