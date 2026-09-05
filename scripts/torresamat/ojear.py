"""Enseña por encima qué hay en cada media hoja de un rango de pliegos."""
import sys
from rescatar import sucesos_media_hoja

def ojear(tomo, desde, hasta, filtro=None):
    for pg in range(desde, hasta + 1):
        for lado in (0, 1):
            ev = sucesos_media_hoja(tomo, pg, lado)
            if not ev: continue
            vs = [e for e in ev if e[0] == "vers"]
            cs = [e for e in ev if e[0] == "cap"]
            if not vs: continue
            prim = vs[0]; ult = vs[-1]
            marca = ""
            if filtro:
                txt = " ".join(e[2] if e[0]=="vers" else e[1] for e in ev if e[0] in ("vers","sigue"))
                if filtro.lower() in txt.lower(): marca = "   <<< AQUÍ"
            print(f"  {tomo}/{pg:>3} lado {lado}: {len(vs):3} vv, "
                  f"{len(cs)} encabezados, del {prim[1]} al {ult[1]}"
                  f"  | {prim[2][:40]}{marca}")

if __name__ == "__main__":
    t, a, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    ojear(t, a, b, sys.argv[4] if len(sys.argv) > 4 else None)
