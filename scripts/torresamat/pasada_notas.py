"""La misma corrección sobre las notas, con el capítulo entero por testigo."""
import json, collections
from corrector import (RE_TOK, RARO, MIN_LARGO, frecuencias, construye_indice,
                       candidatos, parte_pegada, Testigo)
from testigos import descarga
from canon import POR_OSIS

def main():
    notas = json.load(open("notas.json", encoding="utf-8"))
    texto = json.load(open("texto.json", encoding="utf-8"))
    tg = Testigo(descarga())
    frec = frecuencias(list(texto.values()) +
                       [n for l in notas.values() for n in l])
    idx = construye_indice(frec)

    cache = {}
    def evidencia(ref):
        """Bigramas y palabras de todo el capítulo en las otras Biblias.

        Las notas van por capítulo, no por versículo, así que el testigo
        tiene que ser el capítulo entero.
        """
        if ref in cache:
            return cache[ref]
        osis, cap = ref.rsplit(" ", 1)
        n = POR_OSIS[osis]["versos"][int(cap) - 1] if osis in POR_OSIS else 40
        big = set()
        for v in range(1, n + 1):
            big |= tg.bigramas_del_pasaje(f"{osis} {cap}:{v}")
        cache[ref] = big
        return big

    sust = part = 0
    for ref, lista in notas.items():
        nueva = []
        big = evidencia(ref)
        for nota in lista:
            piezas, pos = [], 0
            for m in RE_TOK.finditer(nota):
                pal = m.group(0)
                piezas.append(nota[pos:m.start()]); pos = m.end()
                arr = None
                if len(pal) >= MIN_LARGO and frec.get(pal, 0) <= RARO:
                    for c in sorted(candidatos(pal, frec, idx),
                                    key=lambda x: -frec.get(x, 0)):
                        if tg.es_palabra(pal):
                            break
                        if tg.es_palabra(c):
                            arr = c; sust += 1; break
                if arr is None and len(pal) >= 4 and not tg.es_palabra(pal):
                    p = parte_pegada(pal, big)
                    if p:
                        arr = p; part += 1
                piezas.append(arr if arr else pal)
            piezas.append(nota[pos:])
            nueva.append("".join(piezas))
        notas[ref] = nueva
    print(f"notas: {sust} sustituciones, {part} partidas")
    json.dump(notas, open("notas.json", "w", encoding="utf-8"),
              ensure_ascii=False, indent=0)

if __name__ == "__main__":
    main()
