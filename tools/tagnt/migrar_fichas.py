#!/usr/bin/env python3
"""Migra las fichas (clave vieja: ref OSIS + Strong) a la clave nueva:
ref OSIS + posición de la palabra en Tisch + Strong, con los datos TAGNT de esa palabra.

Entrada : data/fichas_interlineal/*.json            (no se modifican)
Salida  : data/fichas_v2/<Libro>.<cap>.json         + reports/migracion_fichas.json
Reglas (decididas por el responsable del proyecto):
 - Un solo candidato (misma ref y Strong en Tisch): se asigna.
 - Varios candidatos: si la forma de la ficha identifica una sola aparición, solo a esa; si no, se
   copia a cada posición coincidente. Siempre con revisar_ocurrencia = true.
 - Sin pareja TAGNT: tagnt = null y revisar_tagnt = true.
 - traducciones_comparadas se vacía (las citas de memoria no se conservan).
"""
import collections, glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import alinear, contexto, tagnt

RAIZ = alinear.RAIZ
ENTRADA = os.path.join(RAIZ, "data", "fichas_interlineal")
SALIDA = os.path.join(RAIZ, "data", "fichas_v2")
INFORME = os.path.join(RAIZ, "reports", "migracion_fichas.json")


def en_pericopa(ref):
    o, c, v = ref.split("#")[0].split(".")
    return o == "John" and ((c == "7" and int(v) == 53) or (c == "8" and 1 <= int(v) <= 11))


def main():
    if "--forzar" not in sys.argv:
        sys.exit("migrar_fichas.py ya cumplió su función: data/fichas_v2/ está en el formato de v3 "
                 "(tools/convertir_fichas_v2_a_v3.py). Volver a ejecutarlo lo sobrescribiría con el formato viejo; usa --forzar.")
    tisch = alinear.leer_tisch()
    al = json.load(open(alinear.SALIDA, encoding="utf-8"))["v"]
    por_clave = {}
    tag_vers = {}
    for p in tagnt.leer():
        tag_vers.setdefault(p.ref, []).append(p)
        por_clave[p.clave] = p
    fichas = []
    for ruta in sorted(glob.glob(os.path.join(ENTRADA, "*.json"))):
        fichas += json.load(open(ruta, encoding="utf-8"))
    stats = collections.Counter()
    rev = collections.defaultdict(list)
    salida = collections.defaultdict(list)
    for f in fichas:
        o, c, v = f["ref"].split(".")
        k = (o, int(c), int(v))
        cand = [t for t in tisch[k] if t["strong"] == f["strong"]]
        if not cand:
            stats["sin_candidato"] += 1
            rev["sin_candidato"].append(f["ref"] + "|" + f["strong"])
            continue
        ambiguo = len(cand) > 1
        destino = cand
        if ambiguo:
            exactas = [t for t in cand if alinear.norm(t["forma"]) == alinear.norm(f["forma"])]
            destino = exactas if len(exactas) == 1 else (exactas or cand)
            stats["ambiguas_una" if len(destino) == 1 else "ambiguas_copiadas"] += 1
            rev["revisar_ocurrencia"].append("%s|%s -> pos %s" % (f["ref"], f["strong"], [t["pos"] for t in destino]))
        al_v = {x[0]: x for x in al[f["ref"]]}
        for t in destino:
            n = dict(f)
            n["pos_tisch"] = t["pos"]
            n["traducciones_comparadas"] = None
            if ambiguo:
                n["revisar_ocurrencia"] = True
            _, clave, calidad = al_v[t["pos"]]
            n["alineacion"] = calidad or "sin_pareja"
            if clave is None:
                n["tagnt"] = None
                n["ref_estandar"] = None
                n["revisar_tagnt"] = True
                stats["sin_pareja_tagnt"] += 1
                rev["sin_pareja_tagnt"].append("%s|%s pos %d (%s)" % (f["ref"], f["strong"], t["pos"], t["forma"]))
                if en_pericopa(f["ref"]):
                    n["variante_textual"] = {
                        "pasaje": "Jn 7:53–8:11",
                        "nota": ("Perícopa de la adúltera: NA28 la imprime entre dobles corchetes. La lectura de "
                                 "Tischendorf no tiene palabra correspondiente en el TAGNT."),
                    }
                    stats["pericopa"] += 1
                    rev["pericopa"].append("%s|%s pos %d" % (f["ref"], f["strong"], t["pos"]))
            else:
                p = por_clave[clave]
                pals = tag_vers[p.ref]
                n.update(contexto.info(pals, pals.index(p)))
                n["ref_estandar"] = p.ref_estandar
                if not p.en("NA28"):
                    n["no_en_na28"] = True
                    n["nota_na28"] = ("La palabra de Tischendorf no está en NA28 según el TAGNT "
                                      "(presente en: %s)." % (", ".join(p.ediciones) or "ninguna edición"))
                    stats["no_en_na28"] += 1
                    rev["no_en_na28"].append("%s|%s pos %d" % (f["ref"], f["strong"], t["pos"]))
                if en_pericopa(clave) or en_pericopa(f["ref"]):
                    n["variante_textual"] = {
                        "pasaje": "Jn 7:53–8:11",
                        "nota": ("Perícopa de la adúltera: NA28 la imprime entre dobles corchetes. "
                                 "En el TAGNT esta palabra aparece en: %s; ausente en: %s."
                                 % (", ".join(p.ediciones) or "ninguna", ", ".join(p.ausente_en()) or "ninguna")),
                    }
                    stats["pericopa"] += 1
                    rev["pericopa"].append("%s|%s pos %d" % (f["ref"], f["strong"], t["pos"]))
                if calidad.startswith("strong") or "otro_strong" in calidad:
                    rev["forma_o_strong_distinto"].append("%s|%s pos %d (%s)" % (f["ref"], f["strong"], t["pos"], calidad))
                if "caso_regido_ambiguo" in n:
                    rev["caso_regido_ambiguo"].append("%s|%s pos %d: %s" % (f["ref"], f["strong"], t["pos"],
                                                                           n["caso_regido_ambiguo"]))
                if n.get("variante") and "variante_textual" not in n:
                    rev["variante"].append("%s|%s pos %d (ausente en %s)" % (f["ref"], f["strong"], t["pos"],
                                                                           ", ".join(n["ausente_en"])))
            stats["fichas_salida"] += 1
            salida["%s.%02d" % (o, int(c))].append(n)
    os.makedirs(SALIDA, exist_ok=True)
    for nombre, lista in salida.items():
        lista.sort(key=lambda x: (int(x["ref"].split(".")[2]), x["pos_tisch"]))
        with open(os.path.join(SALIDA, nombre + ".json"), "w", encoding="utf-8") as fh:
            json.dump(lista, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
    os.makedirs(os.path.dirname(INFORME), exist_ok=True)
    with open(INFORME, "w", encoding="utf-8") as fh:
        json.dump({"fichas_entrada": len(fichas), "estadisticas": stats, "revision": rev}, fh,
                  ensure_ascii=False, indent=1)
    print(len(fichas), dict(stats))
    print({k: len(v) for k, v in rev.items()})


if __name__ == "__main__":
    main()
