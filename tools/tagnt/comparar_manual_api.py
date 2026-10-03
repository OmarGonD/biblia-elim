#!/usr/bin/env python3
"""Compara las fichas del generador manual con las de la API para los mismos versículos.
Genera reports/comparacion_jn1_manual_vs_api.md: 4 fichas de prueba + 10 al azar (semilla fija) + métricas globales.
Uso: .venv-fichas/bin/python tools/tagnt/comparar_manual_api.py [--refs John.1] [--api-modelo claude-sonnet-5-5] [--semilla 7]"""
import argparse, os, random, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import alinear, pipeline

RAIZ = pipeline.RAIZ
# Las 4 fichas de prueba del piloto: πρὸς, ἦν, λόγος y θεός (sin artículo) de Jn 1:1
PRUEBA = [("John.1.1", 10), ("John.1.1", 3), ("John.1.1", 5), ("John.1.1", 14)]
CAMPOS = ("glosa_interlineal", "rango_semantico", "construccion", "sentido_en_contexto", "matiz",
          "variantes_textuales", "notas_traduccion", "otros_usos", "nivel_certeza")
RE_REF = re.compile(r"\b(?:[123]\s)?[A-ZÁÉÍÓÚ][a-záéíóú]{1,5}\.?\s\d+:\d+(?:-\d+)?")


def norm(t):
    return alinear.norm(t or "")


def jaccard(a, b):
    a, b = set(a), set(b)
    return 1.0 if not a and not b else len(a & b) / len(a | b)


def metricas(m, a):
    """Diferencias entre la ficha manual (m) y la de la API (a)."""
    rm = [norm(x) for x in (m["rango_semantico"] or [])]
    ra = [norm(x) for x in (a["rango_semantico"] or [])]
    om = {norm(x) for x in RE_REF.findall(" ".join(m["otros_usos"] or []))}
    oa = {norm(x) for x in RE_REF.findall(" ".join(a["otros_usos"] or []))}
    def largo(c):
        return len(c or "")
    return {
        "glosa_igual": norm(m["glosa_interlineal"]) == norm(a["glosa_interlineal"]),
        "rango_jaccard": jaccard(rm, ra),
        "certeza_igual": m["nivel_certeza"] == a["nivel_certeza"],
        "variantes_coinciden": bool(m["variantes_textuales"]) == bool(a["variantes_textuales"]),
        "otros_usos_jaccard": jaccard(om, oa),
        "sentido_largo_manual": largo(m["sentido_en_contexto"]),
        "sentido_largo_api": largo(a["sentido_en_contexto"]),
        "campos_nulos_distintos": [c for c in CAMPOS if (m[c] is None) != (a[c] is None)],
    }


def bloque(titulo, m, a):
    L = ["### " + titulo, ""]
    for c in CAMPOS:
        vm, va = m[c], a[c]
        fmt = lambda v: "—" if v is None else ("; ".join(v) if isinstance(v, list) else str(v))
        marca = "" if fmt(vm) == fmt(va) else "  ⟵ difiere"
        L.append("**%s**%s" % (c, marca))
        L.append("- manual: %s" % fmt(vm))
        L.append("- API: %s" % fmt(va))
        L.append("")
    return "\n".join(L)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--refs", default="John.1")
    ap.add_argument("--manual-modelo", default=pipeline.MODELO_MANUAL)
    ap.add_argument("--api-modelo", default=pipeline.MODELO_API)
    ap.add_argument("--semilla", type=int, default=7)
    ap.add_argument("--raiz-cache", default=pipeline.CACHE)
    ap.add_argument("--salida", default=os.path.join(RAIZ, "reports", "comparacion_jn1_manual_vs_api.md"))
    a = ap.parse_args(argv)
    datos = pipeline.Datos()
    cm, ca = pipeline.Cache(a.manual_modelo, a.raiz_cache), pipeline.Cache(a.api_modelo, a.raiz_cache)
    pares = {}
    for ref in datos.versiculos(a.refs):
        fm, fa = cm.get(ref), ca.get(ref)
        if fm is None or fa is None:
            continue
        sol = pipeline.solicitud(datos, ref)
        for x, y, w in zip(fm, fa, sol["palabras"]):
            pares[(ref, x["pos_tisch"])] = (x, y, w)
    if not pares:
        print("No hay versículos con ficha manual y de la API a la vez (¿falta la corrida de la API?)")
        return 1
    rnd = random.Random(a.semilla)
    candidatos = sorted(k for k, (_, _, w) in pares.items() if w["nivel"] == "completo" and k not in PRUEBA)
    azar = sorted(rnd.sample(candidatos, min(10, len(candidatos))), key=lambda k: (alinear_clave(k[0]), k[1]))
    ms = [metricas(m, y) for m, y, _ in pares.values()]
    n = len(ms)
    pct = lambda f: 100.0 * sum(1 for x in ms if x[f]) / n
    prom = lambda f: sum(x[f] for x in ms) / n
    o = ["# Comparación Jn 1: generador manual vs. API\n",
         "Manual: `%s` · API: `%s` · prompt `%s` · semilla del azar: %d. Generado por `tools/tagnt/comparar_manual_api.py`.\n"
         % (a.manual_modelo, a.api_modelo, pipeline.prompt_hash(), a.semilla),
         "## Métricas sobre las %d fichas comparadas\n" % n,
         "| Métrica | Valor |\n|---|---|",
         "| Misma glosa (normalizada) | %.1f %% |" % pct("glosa_igual"),
         "| Mismo `nivel_certeza` | %.1f %% |" % pct("certeza_igual"),
         "| Coincide si hay o no `variantes_textuales` | %.1f %% |" % pct("variantes_coinciden"),
         "| Solape medio de `rango_semantico` (Jaccard) | %.2f |" % prom("rango_jaccard"),
         "| Solape medio de pasajes en `otros_usos` (Jaccard) | %.2f |" % prom("otros_usos_jaccard"),
         "| Largo medio de `sentido_en_contexto` manual / API (caracteres) | %.0f / %.0f |" % (prom("sentido_largo_manual"), prom("sentido_largo_api")),
         "| Fichas con algún campo nulo en uno solo de los dos | %d |\n" % sum(1 for x in ms if x["campos_nulos_distintos"]),
         "## Las 4 fichas de prueba (πρὸς, ἦν, λόγος, θεός — Jn 1:1)\n"]
    for k in PRUEBA:
        if k in pares:
            m, y, w = pares[k]
            o.append(bloque("%s pos. %d · %s" % (k[0], k[1], w["forma"]), m, y))
    o.append("## 10 fichas al azar (palabras completas)\n")
    for k in azar:
        m, y, w = pares[k]
        o.append(bloque("%s pos. %d · %s" % (k[0], k[1], w["forma"]), m, y))
    os.makedirs(os.path.dirname(a.salida), exist_ok=True)
    with open(a.salida, "w", encoding="utf-8") as f:
        f.write("\n".join(o))
    print("ok", a.salida, "fichas comparadas:", n)
    return 0


def alinear_clave(ref):
    o, c, v = ref.split(".")
    return (o, int(c), int(v))


if __name__ == "__main__":
    sys.exit(main())
