"""Extrae el djvu.xml y deja el texto en un JSON intermedio."""
import importlib.util
import json
import os

DIR = os.path.dirname(os.path.abspath(__file__))

from cabeceras import cabecera_pagina, quita_cabecera
from canon import ORDEN, POR_OSIS
from front_matter import separa_front_matter
from load import load
from segment import cuerpo_y_notas, columnas, texto
from versiculos import corriente, une

_spec = importlib.util.spec_from_file_location(
    "alinear_ta", os.path.join(DIR, "..", "torresamat", "alinear.py"))
ensambla = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ensambla)
ensambla = ensambla.ensambla

AMBITO = set(ORDEN)


def _nums_verso(trozos):
    return [s[1] for s in trozos if s[0] == "vers"]


def _continua_numeracion(trozos, max_prev):
    """¿Esta página sigue el capítulo que ya estaba en curso?"""
    if max_prev < 1:
        return False
    nums = _nums_verso(trozos)
    if not nums:
        return False
    return any(n == max_prev or n == max_prev + 1 or
               (0 <= n - max_prev <= 4) for n in nums[:15])


def asigna_libro(paginas):
    """Identidad de libro por cabecera, con recorte en el cambio.

    Una página sin cabecera hereda el libro anterior, salvo que el
    siguiente encabezado sea otro libro *y* la página no continúe la
    numeración: entonces es front-matter del libro nuevo (Princeton
    955, ya Salmos, pegada a Job 42:17).
    """
    assigned = None
    max_vers = 0
    pending = []

    def flush(nuevo):
        nonlocal pending, assigned, max_vers
        if not pending:
            return
        if assigned and nuevo and nuevo != assigned:
            dest = assigned if any(
                _continua_numeracion(p["trozos"], max_vers)
                for p in pending) else nuevo
        else:
            dest = nuevo or assigned
        for p in pending:
            p["libro"] = dest
            if dest == assigned:
                nums = _nums_verso(p["trozos"])
                if nums:
                    max_vers = max(max_vers, max(nums))
        pending = []

    for p in paginas:
        cands = p.get("cands")
        if cands and len(cands) == 1:
            libro = next(iter(cands))
            flush(libro)
            p["libro"] = libro
            assigned = libro
            nums = _nums_verso(p["trozos"])
            if nums:
                max_vers = max(nums)
        else:
            pending.append(p)
    flush(None)


def _extrae_pagina(idx, p):
    if len(p["words"]) < 20:
        return None
    cands, caps_cab, _cab = cabecera_pagina(p, AMBITO)
    notas_col = []
    trozos_pag = []
    for col_idx, col in enumerate(columnas(p)):
        cuerpo, notas = cuerpo_y_notas(col)
        cuerpo = quita_cabecera(cuerpo, p["h"], cands)
        trozos_pag += corriente(cuerpo, cands, {
            "pagina": idx, "columna": col_idx, "fuente": p.get("src"),
        })
        for l in notas:
            t = texto(l).strip()
            if t and sum(c.isalpha() for c in t) >= 8:
                notas_col.append(t)
    return {
        "idx": idx,
        "cands": cands,
        "caps_cab": caps_cab,
        "trozos": trozos_pag,
        "notas": notas_col,
        "src": p.get("src"),
    }


def sucesos_y_notas(pages):
    crudas = []
    empezado = False
    for idx, p in enumerate(pages):
        info = _extrae_pagina(idx, p)
        if info is None:
            continue
        hubo = any(s[0] in ("vers", "cap") for s in info["trozos"])
        if not empezado:
            # Princeton: Génesis empieza ~hoja 101. El PDF, ~105.
            # La cabecera roja «GÉNESIS» manda; el índice es el respaldo.
            cands = info["cands"]
            es_gen = cands == {"Gen"} or (cands and "Gen" in cands)
            if hubo and (es_gen or idx >= 100):
                empezado = True
            else:
                continue
        if not hubo:
            continue
        crudas.append(info)

    asigna_libro(crudas)

    ev, notas_pag = [], []
    previa_libro = None
    for info in crudas:
        libro = info.get("libro")
        if libro and libro != previa_libro:
            ev.append(("libro", libro, None))
            previa_libro = libro
        ev += info["trozos"]
        if info["notas"]:
            notas_pag.append((info["cands"], info["caps_cab"],
                              info["notas"], info["idx"]))
    return ev, notas_pag


def main():
    pages = load()
    print(f"{len(pages)} páginas cargadas", flush=True)
    ev, notas_pag = sucesos_y_notas(pages)
    ev, intros_frags = separa_front_matter(ev)
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
    intros = {k: une(v) for k, v in intros_frags.items() if v}
    with open(os.path.join(DIR, "introducciones.json"), "w",
              encoding="utf-8") as f:
        json.dump(intros, f, ensure_ascii=False, indent=0)

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
