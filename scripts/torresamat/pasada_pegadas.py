"""Aplica pegadas.py al texto. Escribe texto.json y un informe."""
import collections, json, os, pickle, sys

from pegadas import (RE_TOK, aplicar, bigramas_de, vocabulario_testigos,
                     parte_corpus, sino_condicional, sies_si_es, sin_tildes)
from testigos import descarga


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "texto.json"
    t = json.load(open(src, encoding="utf-8"))
    voc_path = os.path.join(os.path.dirname(src) or ".", "voc.pkl")
    if os.path.exists(voc_path):
        voc = pickle.load(open(voc_path, "rb"))
    else:
        voc = vocabulario_testigos(descarga())
    frec = collections.Counter()
    for v in t.values():
        frec.update(w.lower() for w in RE_TOK.findall(v))
    big = bigramas_de(t.values())

    nuevo = {}
    cambios = []  # (ref, pal, arreglo)
    for ref, v in t.items():
        nv, ch = aplicar(v, big, voc, frec)
        nuevo[ref] = nv
        for pal, arr in ch:
            cambios.append((ref, pal, arr))

    dest = sys.argv[2] if len(sys.argv) > 2 else "texto.json"
    json.dump(nuevo, open(dest, "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)

    tipos = collections.Counter((p, a) for _, p, a in cambios)
    tocados = len({r for r, _, _ in cambios})
    print(f"partidas: {len(cambios)}  tipos: {len(tipos)}  "
          f"versículos: {tocados}")
    print("\n── tipos ──")
    for (p, a), c in tipos.most_common():
        print(f"  {c:4}  {p:20} -> {a}")
    inf = os.path.splitext(dest)[0] + ".pegadas.tsv"
    with open(inf, "w", encoding="utf-8") as f:
        for ref, pal, arr in cambios:
            f.write(f"{ref}\t{pal}\t{arr}\n")
    print(f"\ninforme: {inf}")


if __name__ == "__main__":
    main()
