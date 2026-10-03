#!/usr/bin/env python3
"""Costo del NT completo a partir de los tokens REALES medidos en una corrida de la API (logs/uso_api.jsonl).
Los precios no están en el repositorio: se pasan por argumento (USD por millón de tokens).
Uso: informe_costo.py --modelo claude-sonnet-5-5 --refs John.1 --precio-ent 3 --precio-sal 15
Supuestos que se pueden cambiar: --descuento-batch (0,5), --mult-cache-lectura (0,1), --mult-cache-escritura (1,25)."""
import argparse, collections, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pipeline

RAIZ = pipeline.RAIZ


def leer_uso(modelo, refs, ruta=pipeline.USO):
    ultimo = {}
    if os.path.exists(ruta):
        with open(ruta, encoding="utf-8") as f:
            for l in f:
                x = json.loads(l)
                if x["modelo"] == modelo and x["ref"] in refs:
                    ultimo[x["ref"]] = x       # la última medición de cada versículo
    return ultimo


def calcular(uso, palabras_corrida, n_calls_nt, palabras_nt):
    """Tokens por palabra medidos y extrapolación al NT. Devuelve un dict con todo lo necesario."""
    n = len(uso)
    ent = sum(x["input_tokens"] for x in uso.values())
    sal = sum(x["output_tokens"] for x in uso.values())
    c_esc = sum(x["cache_creation_input_tokens"] for x in uso.values())
    c_lec = sum(x["cache_read_input_tokens"] for x in uso.values())
    # el prompt de sistema es lo único cacheado: su tamaño = máx(escritura + lectura) por llamada
    sis = max((x["cache_creation_input_tokens"] + x["cache_read_input_tokens"] for x in uso.values()), default=0)
    var = ent + c_esc + c_lec - n * sis                     # entrada que no es el prompt de sistema
    por_pal = {"entrada_variable": var / palabras_corrida, "salida": sal / palabras_corrida}
    return {"llamadas": n, "palabras": palabras_corrida, "entrada_no_cacheada": ent, "salida": sal,
            "cache_escritura": c_esc, "cache_lectura": c_lec, "prompt_sistema": sis, "por_palabra": por_pal,
            "nt": {"llamadas": n_calls_nt, "palabras": palabras_nt,
                   "entrada_variable": por_pal["entrada_variable"] * palabras_nt,
                   "salida": por_pal["salida"] * palabras_nt,
                   "prompt_sistema_total": sis * n_calls_nt}}


def costo(c, p_ent, p_sal, desc_batch, m_lec, m_esc, con_cache=True, batch=True):
    nt = c["nt"]
    d = desc_batch if batch else 1.0
    ent = nt["entrada_variable"] / 1e6 * p_ent
    sal = nt["salida"] / 1e6 * p_sal
    sis_tot = nt["prompt_sistema_total"] / 1e6
    if con_cache:    # una escritura y el resto lecturas (mejor caso; en Batch la caché es de mejor esfuerzo)
        sis = (c["prompt_sistema"] / 1e6 * p_ent * m_esc) + ((nt["llamadas"] - 1) * c["prompt_sistema"] / 1e6 * p_ent * m_lec)
    else:
        sis = sis_tot * p_ent
    return (ent + sal + sis) * d


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--modelo", default=pipeline.MODELO_API)
    ap.add_argument("--refs", default="John.1")
    ap.add_argument("--precio-ent", type=float)
    ap.add_argument("--precio-sal", type=float)
    ap.add_argument("--descuento-batch", type=float, default=0.5)
    ap.add_argument("--mult-cache-lectura", type=float, default=0.1)
    ap.add_argument("--mult-cache-escritura", type=float, default=1.25)
    ap.add_argument("--salida", default=os.path.join(RAIZ, "reports", "costo_nt_api.md"))
    a = ap.parse_args(argv)
    datos = pipeline.Datos()
    refs = datos.versiculos(a.refs)
    uso = leer_uso(a.modelo, set(refs))
    if not uso:
        print("Sin mediciones en %s para %s (¿se ejecutó la corrida real?)" % (pipeline.USO, a.modelo))
        return 1
    pal_corrida = sum(len(datos.tisch[tuple([r.split(".")[0], int(r.split(".")[1]), int(r.split(".")[2])])]) for r in uso)
    todos = [r for r in ("%s.%d.%d" % k for k in sorted(datos.tisch)) if not pipeline.excluido(r)]
    pal_nt = sum(len(datos.tisch[(r.split(".")[0], int(r.split(".")[1]), int(r.split(".")[2]))]) for r in todos)
    c = calcular(uso, pal_corrida, len(todos), pal_nt)
    o = ["# Costo del NT completo por API (medido)\n",
         "Modelo `%s` · corrida sobre `%s`: %d llamadas, %d palabras. Tokens **reales** devueltos por la API (`logs/uso_api.jsonl`).\n"
         % (a.modelo, a.refs, c["llamadas"], c["palabras"]),
         "## Tokens medidos\n", "| | Corrida | Por palabra | NT completo (extrapolado) |", "|---|---|---|---|",
         "| Entrada variable (sin prompt de sistema) | {:,} | {:.1f} | {:,.0f} |".format(
             round(c["por_palabra"]["entrada_variable"] * c["palabras"]), c["por_palabra"]["entrada_variable"], c["nt"]["entrada_variable"]),
         "| Salida | {:,} | {:.1f} | {:,.0f} |".format(c["salida"], c["por_palabra"]["salida"], c["nt"]["salida"]),
         "| Prompt de sistema (por llamada) | {:,} | — | {:,.0f} ({:,} llamadas) |\n".format(c["prompt_sistema"], c["nt"]["prompt_sistema_total"], c["nt"]["llamadas"]),
         "Caché de prompt en la corrida: escritura {:,} tokens, lectura {:,} tokens, entrada no cacheada {:,} tokens.\n".format(
             c["cache_escritura"], c["cache_lectura"], c["entrada_no_cacheada"]),
         "El NT se extrapola con %s palabras en %s llamadas (sin Jn 8:12–8:53, excluido)." % ("{:,}".format(pal_nt), "{:,}".format(len(todos)))]
    if a.precio_ent is None or a.precio_sal is None:
        o.append("\nSin precios (`--precio-ent`, `--precio-sal`, en USD por millón de tokens) no se calcula el costo en dinero.")
    else:
        o += ["\n## Costo (USD)\n", "Precios usados: entrada %.2f, salida %.2f USD/MTok; descuento Batch %.0f %%; lectura de caché ×%.2f, escritura ×%.2f (supuestos configurables).\n"
              % (a.precio_ent, a.precio_sal, 100 * (1 - a.descuento_batch), a.mult_cache_lectura, a.mult_cache_escritura),
              "| Escenario | Costo NT |", "|---|---|"]
        for nombre, cache, batch in (("Síncrona, sin caché", False, False), ("Síncrona, con caché de prompt", True, False),
                                     ("Batch, sin caché", False, True), ("Batch, con caché de prompt (mejor caso)", True, True)):
            o.append("| %s | %s |" % (nombre, "${:,.0f}".format(costo(c, a.precio_ent, a.precio_sal, a.descuento_batch, a.mult_cache_lectura, a.mult_cache_escritura, cache, batch))))
    os.makedirs(os.path.dirname(a.salida), exist_ok=True)
    with open(a.salida, "w", encoding="utf-8") as f:
        f.write("\n".join(o) + "\n")
    print("\n".join(o))
    return 0


if __name__ == "__main__":
    sys.exit(main())
