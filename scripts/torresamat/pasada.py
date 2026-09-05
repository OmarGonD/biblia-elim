"""Aplica el corrector al texto, versículo a versículo."""
import json, re, collections
from corrector import (RE_TOK, RARO, MIN_LARGO, frecuencias, construye_indice,
                       candidatos, parte_pegada, Testigo)
from testigos import descarga

def main():
    t = json.load(open("texto_sin_orto.json", encoding="utf-8"))
    tg = Testigo(descarga())
    frec = frecuencias(t.values())
    idx = construye_indice(frec)

    cuenta = collections.Counter()
    ejemplos = collections.defaultdict(list)
    nuevo = {}
    for ref, v in t.items():
        piezas = []
        pos = 0
        for m in RE_TOK.finditer(v):
            pal = m.group(0)
            piezas.append(v[pos:m.start()]); pos = m.end()
            arreglo = None
            # Sustituir una letra: solo se mira si la grafía es rara, porque
            # si sale mucho es que así se escribe en este libro.
            if len(pal) >= MIN_LARGO and frec.get(pal, 0) <= RARO:
                for c in sorted(candidatos(pal, frec, idx),
                                key=lambda x: -frec.get(x, 0)):
                    d = tg.dictamen(ref, pal, c)
                    if d == "dejar":
                        arreglo = None; break
                    if d == "corregir":
                        arreglo = c; break
                if arreglo:
                    cuenta["sustitucion"] += 1
                    if len(ejemplos["sustitucion"]) < 12:
                        ejemplos["sustitucion"].append((ref, pal, arreglo))
            # Partir palabras pegadas, en cambio, no depende de la
            # frecuencia: "Enel" sale ciento y pico veces porque el OCR
            # junta siempre igual, y con el filtro de rareza no se miraba.
            if arreglo is None and len(pal) >= 4 and not tg.es_palabra(pal):
                part = parte_pegada(pal, tg.bigramas_del_pasaje(ref))
                if part:
                    arreglo = part
                    cuenta["partida"] += 1
                    if len(ejemplos["partida"]) < 12:
                        ejemplos["partida"].append((ref, pal, part))
            piezas.append(arreglo if arreglo else pal)
        piezas.append(v[pos:])
        nuevo[ref] = "".join(piezas)

    tocados = sum(1 for k in t if nuevo[k] != t[k])
    print(f"sustituciones avaladas por el pasaje: {cuenta['sustitucion']}")
    print(f"palabras pegadas partidas          : {cuenta['partida']}")
    print(f"versículos tocados                 : {tocados}")
    for clase in ("sustitucion", "partida"):
        print(f"\n── {clase} ──")
        for ref, a, b in ejemplos[clase]:
            print(f"   {ref:14} {a:16} -> {b}")
    json.dump(nuevo, open("texto.json", "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)

if __name__ == "__main__":
    main()
