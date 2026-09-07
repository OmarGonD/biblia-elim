"""Extrae el djvu.xml y deja el texto en un JSON intermedio."""
import importlib.util
import json
import os

DIR = os.path.dirname(os.path.abspath(__file__))

from cabeceras import cabecera_pagina, quita_cabecera
from canon import ORDEN, POR_OSIS
from load import load
from segment import cuerpo_y_notas, columnas, texto
from versiculos import corriente, une

_spec = importlib.util.spec_from_file_location(
    "alinear_ta", os.path.join(DIR, "..", "torresamat", "alinear.py"))
ensambla = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ensambla)
ensambla = ensambla.ensambla

AMBITO = set(ORDEN)


def sucesos_y_notas(pages):
    ev, notas_pag = [], []
    previa_libro = None
    empezado = False
    for idx, p in enumerate(pages):
        if len(p["words"]) < 20:
            continue
        cands, caps_cab, _cab = cabecera_pagina(p, AMBITO)
        # El prólogo nombra Génesis, Hechos, Salmos: no hay versos.
        notas_col = []
        trozos_pag = []
        for col_idx, col in enumerate(columnas(p)):
            cuerpo, notas = cuerpo_y_notas(col)
            cuerpo = quita_cabecera(cuerpo, p["h"])
            trozos_pag += corriente(cuerpo, cands, {
                "pagina": idx, "columna": col_idx, "fuente": p.get("src"),
            })
            for l in notas:
                t = texto(l).strip()
                if t and sum(c.isalpha() for c in t) >= 8:
                    notas_col.append(t)
        hubo = any(s[0] in ("vers", "cap") for s in trozos_pag)
        if not empezado:
            # Princeton: Génesis empieza ~hoja 101. El PDF, ~105.
            # La cabecera roja «GÉNESIS» manda; el índice es el respaldo.
            es_gen = cands == {"Gen"} or (cands and "Gen" in cands)
            if hubo and (es_gen or idx >= 100):
                empezado = True
            else:
                continue
        if not hubo:
            continue
        libro = None
        if cands and len(cands) == 1:
            libro = next(iter(cands))
        if libro and libro != previa_libro:
            ev.append(("libro", None, None))
            previa_libro = libro
        ev += trozos_pag
        if notas_col:
            notas_pag.append((cands, caps_cab, notas_col, idx))
    return ev, notas_pag


def main():
    pages = load()
    print(f"{len(pages)} páginas cargadas", flush=True)
    ev, notas_pag = sucesos_y_notas(pages)
    libros = [POR_OSIS[o] for o in ORDEN]
    vers, avisos, procedencia = ensambla(ev, libros)
    todo = {}
    for k, trozos in vers.items():
        todo["%s %d:%d" % k] = une(trozos)

    out_json = os.path.join(DIR, "texto.json")
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(todo, f, ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "procedencia.json"), "w", encoding="utf-8") as f:
        json.dump({"%s %d:%d" % k: v for k, v in procedencia.items()}, f,
                  ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "avisos.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(avisos))

    # Notas agrupadas por capítulo, a falta de poder colocarlas verso a verso.
    notas = {}
    for cands, caps_cab, lineas, _idx in notas_pag:
        osis = None
        if cands and len(cands) == 1:
            osis = next(iter(cands))
        cap = caps_cab[0] if caps_cab else None
        if osis and cap:
            notas.setdefault(f"{osis} {cap}", []).append(une(lineas))
    with open(os.path.join(DIR, "notas.json"), "w", encoding="utf-8") as f:
        json.dump(notas, f, ensure_ascii=False, indent=0)

    esp = sum(sum(L["versos"]) for L in libros)
    print(f"\nTOTAL: {len(todo)}/{esp} versículos "
          f"({100 * len(todo) / esp:.1f}%), {len(avisos)} avisos")
    for L in libros:
        g = sum(1 for k in vers if k[0] == L["osis"])
        e = sum(L["versos"])
        marca = "  <-- REVISAR" if e and g / e < 0.60 else ""
        print(f"    {L['osis']:6} {g:5}/{e:5}  {100 * g / e:5.1f}%{marca}")
    print(f"notas en {len(notas)} capítulos")


if __name__ == "__main__":
    main()
