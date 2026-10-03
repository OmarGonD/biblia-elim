#!/usr/bin/env python3
"""Conversión ÚNICA de data/fichas_v2/*.json (formato de la migración) al formato de fichas v3.

Cambios (decididos por el responsable del proyecto):
  - variante_textual {pasaje, nota} y nota_na28 se integran en `variantes_textuales` (texto).
  - se añade `lema` (del módulo Tisch por posición) y la procedencia: generador='manual',
    modelo='manual-legacy', prompt_hash=null, generado_en=null.
  - `nivel_certeza` se conserva tal cual (sin inventar valores).
  - se elimina `glosa_tagnt` (glosa en inglés: no se distribuye) y `traducciones_comparadas`
    (las citas van en la tabla `citas`, por ref_estandar).
  - `notas_traduccion` pasa de lista a texto (en v3 es texto).
Es idempotente: un archivo ya convertido no se toca. Después de ejecutarlo, los JSON se versionan en git.
"""
import glob, json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "tagnt"))
import alinear

RAIZ = alinear.RAIZ
DIR = os.path.join(RAIZ, "data", "fichas_v2")
ORDEN = ["ref", "pos_tisch", "strong", "forma", "lema", "glosa_interlineal", "rango_semantico", "construccion",
         "sentido_en_contexto", "matiz", "variantes_textuales", "notas_traduccion", "otros_usos", "nivel_certeza",
         "alineacion", "tagnt", "ref_estandar", "morfologia_tagnt", "dstrong", "strong_tagnt", "ediciones",
         "ausente_en", "variante", "no_en_na28", "caso_regido", "caso_regido_ambiguo", "revisar_ocurrencia",
         "revisar_tagnt", "generador", "modelo", "prompt_hash", "generado_en"]


def convertir(f, lema):
    if f.get("generador"):                     # ya en formato v3
        return f
    n = dict(f)
    partes = []
    vt = n.pop("variante_textual", None)
    if vt:
        partes.append(vt["nota"])
    na = n.pop("nota_na28", None)
    if na:
        partes.append(na)
    n["variantes_textuales"] = " ".join(partes) or None
    nt = n.get("notas_traduccion")
    n["notas_traduccion"] = "\n".join(nt) if isinstance(nt, list) else nt
    n["lema"] = lema
    for k in ("glosa_tagnt", "traducciones_comparadas"):
        n.pop(k, None)
    n.update(generador="manual", modelo="manual-legacy", prompt_hash=None, generado_en=None)
    extra = set(n) - set(ORDEN)
    if extra:
        raise ValueError("campos sin destino en %s: %s" % (n["ref"], sorted(extra)))
    return {k: n[k] for k in ORDEN if k in n}


def main():
    tisch = alinear.leer_tisch()
    tot = 0
    for ruta in sorted(glob.glob(os.path.join(DIR, "*.json"))):
        with open(ruta, encoding="utf-8") as fh:
            fichas = json.load(fh)
        nuevas = []
        for f in fichas:
            o, c, v = f["ref"].split(".")
            lema = tisch[(o, int(c), int(v))][f["pos_tisch"] - 1]["lema"] or None
            nuevas.append(convertir(f, lema))
        with open(ruta, "w", encoding="utf-8") as fh:
            json.dump(nuevas, fh, ensure_ascii=False, indent=1)
            fh.write("\n")
        tot += len(nuevas)
    print("convertidas/verificadas:", tot)


if __name__ == "__main__":
    main()
