"""
Mide si cada capítulo está bien alineado, comparándolo con la Vulgata.

Torres Amat traduce de la Vulgata versículo a versículo, así que la
longitud de cada versículo castellano sigue de cerca la del latino. Si el
capítulo está bien colocado, la correlación entre las dos series de
longitudes es alta en el desplazamiento 0; si se ha corrido un puesto,
gana el desplazamiento +1 o -1. Eso da una medida objetiva del
alineamiento sin necesidad de un texto de referencia en español.
"""
import json, subprocess, os, pickle, re, statistics
from canon import POR_OSIS
from construir import ORDEN

CACHE = "vulgata_limpia.pkl"

def vulgata():
    if os.path.exists(CACHE):
        with open(CACHE, "rb") as f:
            return pickle.load(f)
    out = {}
    for o in ORDEN:
        L = POR_OSIS[o]
        for c in range(1, L["caps"] + 1):
            try:
                r = subprocess.run(["diatheke", "-b", "VulgClementine", "-k",
                                    f"{o} {c}"], capture_output=True,
                                   text=True, timeout=60)
            except Exception:
                continue
            for ln in r.stdout.splitlines():
                m = re.match(r"^.+?\s(\d+):(\d+):\s?(.*)$", ln)
                if m:
                    # El texto sale con el marcado OSIS dentro (<div>, <l>,
                    # <lg>…). Si no se quita, la "longitud" del versículo
                    # latino es sobre todo marcado y la correlación mide
                    # ruido: con él, el validador daba por corridos
                    # capítulos que estaban bien y al revés.
                    t = re.sub(r"<[^>]*>", " ", m.group(3))
                    t = re.sub(r"\s{2,}", " ", t).strip()
                    out[f"{o} {c}:{int(m.group(2))}"] = t
    with open(CACHE, "wb") as f:
        pickle.dump(out, f, -1)
    return out

def corr(xs, ys):
    if len(xs) < 5:
        return None
    try:
        return statistics.correlation(xs, ys)
    except Exception:
        return None

def revisa(texto, vul):
    """Devuelve (bien, corridos, sin_juicio) y la lista de capítulos corridos."""
    bien = corridos = indef = 0
    lista = []
    for o in ORDEN:
        L = POR_OSIS[o]
        for c, n in enumerate(L["versos"], start=1):
            mejor, mejor_d = None, None
            for d in (0, 1, -1):
                xs, ys = [], []
                for v in range(1, n + 1):
                    a = texto.get(f"{o} {c}:{v}")
                    b = vul.get(f"{o} {c}:{v + d}")
                    if a and b:
                        xs.append(len(a)); ys.append(len(b))
                r = corr(xs, ys)
                if r is not None and (mejor is None or r > mejor):
                    mejor, mejor_d = r, d
            if mejor is None or mejor < 0.30:
                indef += 1
            elif mejor_d == 0:
                bien += 1
            else:
                corridos += 1
                lista.append((f"{o} {c}", mejor_d, round(mejor, 2)))
    return bien, corridos, indef, lista

if __name__ == "__main__":
    import sys
    vul = vulgata()
    print(f"versículos de la Vulgata en caché: {len(vul)}")
    for fich in sys.argv[1:] or ["texto.json"]:
        t = json.load(open(fich, encoding="utf-8"))
        b, c, i, lista = revisa(t, vul)
        tot = b + c + i
        print(f"\n{fich}: {len(t)} versículos")
        print(f"   capítulos bien alineados : {b:5} ({100*b/tot:.1f}%)")
        print(f"   capítulos corridos       : {c:5}")
        print(f"   sin juicio (pocos datos) : {i:5}")
        for x in lista[:8]:
            print(f"      corrido {x[1]:+d}  {x[0]}  (r={x[2]})")

def informe(texto, vul, destino="revisar.txt"):
    """Deja la lista de capítulos sospechosos para quien revise."""
    _b, _c, _i, lista = revisa(texto, vul)
    vacios = []
    for o in ORDEN:
        L = POR_OSIS[o]
        for c, n in enumerate(L["versos"], start=1):
            if not any(texto.get(f"{o} {c}:{v}") for v in range(1, n + 1)):
                vacios.append(f"{o} {c}")
    with open(destino, "w", encoding="utf-8") as f:
        f.write("Capítulos que hay que mirar antes de fiarse de este texto.\n")
        f.write("Generado por validar.py; ver el README de esta carpeta.\n\n")
        f.write(f"== Sin una sola línea ({len(vacios)}) ==\n")
        for v in vacios:
            f.write(f"  {v}\n")
        f.write(f"\n== Posible corrimiento de un versículo ({len(lista)}) ==\n")
        f.write("   La correlación de longitudes con la Vulgata casa mejor\n")
        f.write("   desplazada que en su sitio. Es indicio, no prueba: en las\n")
        f.write("   pruebas acertó en unos capítulos y falló en otros, y por eso\n")
        f.write("   no se corrige solo.\n")
        for ref, d, r in lista:
            f.write(f"  {ref:16} {d:+d}  (r={r})\n")
    return len(vacios), len(lista)
