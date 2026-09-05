"""
Dice en qué pliego del escaneo cae cada laguna.

Para volver a pasar el OCR sobre las páginas que fallan hay que saber
cuáles son. Se vuelve a recorrer la extracción anotando de qué pliego sale
cada suceso, se mapea cada capítulo a su rango de pliegos y las lagunas se
sitúan entre el pliego del capítulo anterior y el del siguiente.
"""
import json, collections
from load import load, X1, X2
from cabeceras import medias_hojas
from segment import punto_de_corte, canal
from versiculos import corriente
from nombres import candidatos
from portadas import es_portada
from canon import POR_OSIS
from construir import ORDEN, CORTES
from alinear import ensambla

def sucesos_con_pagina(tomo, ambito):
    """Como sucesos_de(), pero cada suceso lleva su número de pliego."""
    ev, pags, previa = [], [], False
    for idx, p in enumerate(load(tomo)):
        if not p["words"]:
            continue
        g = canal(p)
        mitades = [[w for w in p["words"] if w[X2] <= g],
                   [w for w in p["words"] if w[X1] > g]]
        for (cab, ls), ws in zip(medias_hojas(p), mitades):
            port = es_portada(ws)
            if port and not previa:
                ev.append(("libro", None, None)); pags.append(idx)
            previa = port
            if not ls:
                continue
            nuevos = corriente(ls[:punto_de_corte(ls)], candidatos(cab, ambito))
            ev += nuevos
            pags += [idx] * len(nuevos)
    return ev, pags

def mapa_paginas():
    """{(osis, cap): (pliego_min, pliego_max, tomo)} de lo que sí se leyó."""
    fuera = {}
    for t, (a, b) in CORTES.items():
        ambito = set(ORDEN[a:b])
        ev, pags = sucesos_con_pagina(t, ambito)
        # reproducir el reparto por capítulo mirando el texto ya asignado
        libros = [POR_OSIS[o] for o in ORDEN[a:b]]
        vers, _ = ensambla(ev, libros)
        # aproximación: se recorre en paralelo, atribuyendo cada versículo
        # leído al pliego donde apareció su primer trozo
        vistos = collections.defaultdict(list)
        cap_actual = None
        for s, pg in zip(ev, pags):
            if s[0] == "vers":
                vistos[pg].append(s[1])
        # rango de pliegos por capítulo, vía los versículos presentes
        yield t, ev, pags, vers

if __name__ == "__main__":
    texto = json.load(open("texto.json", encoding="utf-8"))
    # capítulos vacíos
    vacios = []
    for o in ORDEN:
        L = POR_OSIS[o]
        for c, n in enumerate(L["versos"], start=1):
            if not any(texto.get(f"{o} {c}:{v}") for v in range(1, n + 1)):
                vacios.append((o, c))
    print(f"capítulos vacíos: {len(vacios)}")

    # pliego de cada capítulo con texto, por tomo
    ubic = {}
    for t, (a, b) in CORTES.items():
        ambito = set(ORDEN[a:b])
        ev, pags = sucesos_con_pagina(t, ambito)
        libros = [POR_OSIS[o] for o in ORDEN[a:b]]
        from alinear import segmenta, alinea_capitulos
        obs = segmenta(ev)
        # pliego de cada capítulo candidato: el del primer versículo suyo
        pg_de_obs = []
        i = 0
        idx_ev = {id(s): pags[k] for k, s in enumerate(ev)}
        for cnd in obs:
            pgs = [idx_ev.get(id(s)) for s in cnd["sucesos"]]
            pgs = [x for x in pgs if x is not None]
            pg_de_obs.append(min(pgs) if pgs else None)
        canon_caps = [(L["osis"], c, n) for L in libros
                      for c, n in enumerate(L["versos"], start=1)]
        plan = alinea_capitulos(obs, canon_caps)
        for osis, cap, idxs in plan:
            pgs = [pg_de_obs[k] for k in idxs if pg_de_obs[k] is not None]
            if pgs:
                ubic[(osis, cap)] = (t, min(pgs), max(pgs))
    json.dump({f"{k[0]} {k[1]}": v for k, v in ubic.items()},
              open("ubicacion.json", "w"), ensure_ascii=False)

    print(f"capítulos ubicados: {len(ubic)}\n")
    print("laguna            tomo  pliegos a rehacer")
    objetivo = collections.defaultdict(set)
    for o, c in vacios:
        antes = ubic.get((o, c - 1)) if c > 1 else None
        idx = ORDEN.index(o)
        desp = ubic.get((o, c + 1))
        if not desp and idx + 1 < len(ORDEN):
            desp = ubic.get((ORDEN[idx + 1], 1))
        if not antes and idx > 0:
            po = ORDEN[idx - 1]
            antes = ubic.get((po, POR_OSIS[po]["caps"]))
        if antes and desp and antes[0] == desp[0]:
            t, lo, hi = antes[0], antes[2], desp[1]
            if 0 <= hi - lo <= 12:
                for p in range(lo, hi + 1):
                    objetivo[t].add(p)
                print(f"  {o} {c:<12} {t:4}  {lo}-{hi}")
                continue
        print(f"  {o} {c:<12} {'?':4}  sin acotar")
    print()
    for t in sorted(objetivo):
        print(f"tomo {t}: {len(objetivo[t])} pliegos -> {sorted(objetivo[t])}")
    json.dump({t: sorted(v) for t, v in objetivo.items()},
              open("rehacer.json", "w"))
