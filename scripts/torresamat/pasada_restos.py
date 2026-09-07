"""Aplica fusión de versículos vacíos y erratas de letra."""
import collections, json, os, pickle, sys

from fusion import aplicar as aplicar_fusion
from erratas import aplicar_texto, RE_TOK
from testigos import descarga


def _platense(testigos):
    return testigos.get("SpaPlatense") or testigos.get("SpaPlatense".lower()) or {}


def _voc(testigos):
    from testigos import vocabulario
    return vocabulario(testigos)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else "texto.json"
    dest = sys.argv[2] if len(sys.argv) > 2 else src
    t = json.load(open(src, encoding="utf-8"))

    plat_path = os.path.join(os.path.dirname(src) or ".", "platense.pkl")
    if os.path.exists(plat_path):
        plat = pickle.load(open(plat_path, "rb"))
    else:
        plat = descarga().get("SpaPlatense", {})

    voc_path = os.path.join(os.path.dirname(src) or ".", "voc.pkl")
    if os.path.exists(voc_path):
        voc = pickle.load(open(voc_path, "rb"))
    else:
        voc = _voc(descarga())

    # vacíos: canon vs keys
    from canon import POR_OSIS
    from construir import ORDEN
    vacios = []
    for osis in ORDEN:
        L = POR_OSIS[osis]
        for c, nver in enumerate(L["versos"], start=1):
            for v in range(1, nver + 1):
                k = f"{osis} {c}:{v}"
                if not t.get(k):
                    vacios.append(k)

    nuevo, fusionados = aplicar_fusion(t, vacios, plat)
    frec = collections.Counter()
    for v in nuevo.values():
        frec.update(w.lower() for w in RE_TOK.findall(v))

    n_err = 0
    tocados_err = 0
    ejemplos = []
    for ref, s in list(nuevo.items()):
        ns, ch = aplicar_texto(s, voc, frec)
        if ns != s:
            nuevo[ref] = ns
            tocados_err += 1
            n_err += len(ch)
            if len(ejemplos) < 15:
                ejemplos.append((ref, ch[:3]))

    json.dump(nuevo, open(dest, "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)
    print(f"versículos fusionados : {len(fusionados)}")
    print(f"erratas aplicadas     : {n_err} en {tocados_err} versículos")
    print(f"vacíos que quedan     : {sum(1 for k in vacios if not nuevo.get(k))}")
    print("\n── fusiones (muestra) ──")
    for k in fusionados[:12] + [x for x in fusionados if x.startswith("Matt 5:")]:
        print(f"  {k:16} {nuevo[k][:70]}")
    print("\n── erratas (muestra) ──")
    for ref, ch in ejemplos:
        print(f"  {ref:16} {ch}")


if __name__ == "__main__":
    main()
