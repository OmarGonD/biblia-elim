"""Extrae los cuatro tomos y deja el texto en un JSON intermedio."""
import json, sys
from load import load, TOMOS
from cabeceras import medias_hojas
from segment import punto_de_corte
from versiculos import corriente, une
from canon import CANON, POR_OSIS
from alinear import ensambla
from nombres import candidatos
from portadas import es_portada
from paginas import pagina, rehechas
from segment import canal
from load import X1, X2

# Los 73 libros del canon católico, en el orden del canon Vulgata de SWORD,
# repartidos como los reparte esta edición de 1882.
ORDEN = [L["osis"] for L in CANON if L["osis"] not in
         ("PrMan", "1Esd", "2Esd", "AddPs", "EpLao")]
CORTES = {"I": (0, 8), "II": (8, 20), "III": (20, 46), "IV": (46, 73)}

def sucesos_de(tomo, ambito):
    ev = []
    previa = False
    originales = load(tomo)
    for idx in range(len(originales)):
        p = pagina(tomo, idx, originales)
        if not p["words"]:
            continue
        g = canal(p)
        mitades = [[w for w in p["words"] if w[X2] <= g],
                   [w for w in p["words"] if w[X1] > g]]
        for (cab, ls), ws in zip(medias_hojas(p), mitades):
            # Una portada ocupa las dos caras del pliego: se colapsan las
            # consecutivas para que un libro deje un ancla y no dos.
            port = es_portada(ws)
            if port and not previa:
                ev.append(("libro", None, None))
            previa = port
            if not ls:
                continue
            ev += corriente(ls[:punto_de_corte(ls)], candidatos(cab, ambito))
    return ev

def main():
    todo, todos_avisos = {}, []
    for t in TOMOS:
        a, b = CORTES[t]
        libros = [POR_OSIS[o] for o in ORDEN[a:b]]
        ev = sucesos_de(t, set(ORDEN[a:b]))
        vers, avisos = ensambla(ev, libros)
        todos_avisos += [f"[tomo {t}] {x}" for x in avisos]
        for k, trozos in vers.items():
            todo["%s %d:%d" % k] = une(trozos)
        esp = sum(sum(L["versos"]) for L in libros)
        got = len(vers)
        print(f"tomo {t}: {got}/{esp} versículos ({100*got/esp:.1f}%), "
              f"{len(avisos)} avisos", flush=True)
        for L in libros:
            g = sum(1 for k in vers if k[0] == L["osis"])
            e = sum(L["versos"])
            marca = "  <-- REVISAR" if g / e < 0.60 else ""
            print(f"    {L['osis']:6} {g:5}/{e:5}  {100*g/e:5.1f}%{marca}")
    with open("texto.json", "w", encoding="utf-8") as f:
        json.dump(todo, f, ensure_ascii=False, indent=0)
    with open("avisos.txt", "w", encoding="utf-8") as f:
        f.write("\n".join(todos_avisos))
    esp_tot = sum(sum(L["versos"]) for L in CANON if L["osis"] in ORDEN)
    print(f"\nTOTAL: {len(todo)}/{esp_tot} versículos "
          f"({100*len(todo)/esp_tot:.1f}%), {len(todos_avisos)} avisos")

if __name__ == "__main__":
    main()
