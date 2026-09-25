"""Extrae el djvu.xml y deja el texto en un JSON intermedio."""
import importlib.util
import json
import os
import re

DIR = os.path.dirname(os.path.abspath(__file__))

from cabeceras import cabecera_pagina, quita_cabecera
from canon import ORDEN, POR_OSIS, SALMOS_HEBREO, TITULO_SALMOS
from front_matter import separa_front_matter
from load import load
from segment import cuerpo_y_notas, columnas, texto
from limpieza import (aplica_erratas, carga_erratas, cierra_exclamaciones,
                      repara_cortes_ocr, repara_guiones, vocabulario,
                      vocabulario_min)
from versiculos import (RE_CORTE, _descarte, corriente, corrige_repetidos,
                        parte_dolar, une)

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
        limpio = quita_cabecera(cuerpo, p["h"], cands)
        base = {"pagina": idx, "columna": col_idx, "fuente": p.get("src")}
        for l in cuerpo:
            if l not in limpio:
                trozos_pag += _descarte("titulillo", l, texto(l).strip(),
                                        cands, base)
        trozos_pag += corriente(limpio, cands, base)
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


def separa_epigrafes(sucesos):
    """Saca los epígrafes de salmo de la corriente antes del alineador.

    El alineador compartido con Torres Amat pega al verso en curso todo
    suceso que no sea verso: así acababa «Canto triunfal.» al final de Sal
    117:2. Cada epígrafe se ata al primer verso que lo sigue -- el del
    salmo que encabeza -- por su procedencia, que es un objeto único por
    renglón y que ensambla() conserva.
    """
    out, pendientes, epis = [], [], []
    for s in sucesos:
        if s[0] == "epigrafe":
            pendientes.append(s)
            continue
        if s[0] == "vers" and pendientes:
            epis.append(([p[1] for p in pendientes],
                         [p[3] for p in pendientes if p[3]], s[5]))
            pendientes = []
        out.append(s)
    if pendientes:
        epis.append(([p[1] for p in pendientes],
                     [p[3] for p in pendientes if p[3]], None))
    return out, epis


def asigna_epigrafes(epis, procedencia, avisos):
    """{(osis, cap): (texto, procedencia)} según el verso que sigue."""
    donde = {id(o): (osis, cap)
             for (osis, cap, _v), origenes in procedencia.items()
             for o in origenes}
    out = {}
    for trozos, origenes, siguiente in epis:
        clave = donde.get(id(siguiente)) if siguiente is not None else None
        texto_epi = une(trozos)
        if clave is None:
            avisos.append(f"epígrafe sin salmo detrás: «{texto_epi}»")
            continue
        if clave in out:
            avisos.append(f"{clave[0]} {clave[1]}: segundo epígrafe "
                          f"«{texto_epi}» tras «{out[clave][0]}»")
            continue
        out[clave] = (texto_epi, origenes)
    return out


# Señales que por sí solas prueban que al verso le falta texto impreso
# (precisión comprobada en el facsímil, ver TASKS.md, NACAR-OCR-106).
TRUNCADO = {"guion_final", "fragmento_descartado", "continuacion_desplazada",
            "contenido_desplazado"}
# Pérdida probable: el borde recortado acierta ~80 % en texto bíblico, pero
# lo imitan la inicial grande no leída y las introducciones partidas en
# columnas. Queda para revisar, no marca el verso como truncado.
PROBABLE = {"borde_perdido"}
# El resto («descarte:*», «marca_tras_basura», «renglon_descartado_alineador»)
# es evidencia: un renglón que no llegó al texto, sin saber a qué verso
# pertenecía o si era texto bíblico.


def _minusculas(t):
    letras = [c for c in t if c.isalpha()]
    bajas = sum(c.islower() for c in letras)
    return bajas >= 2 and 2 * bajas >= len(letras)


def separa_perdidas(sucesos):
    """Saca la evidencia de pérdida de la corriente antes del alineador.

    Devuelve la corriente limpia y [(suceso, índice del último verso)]:
    cada señal se ata al verso en curso cuando el parser la vio.
    """
    out, senales, ultimo = [], [], None
    for s in sucesos:
        if s[0] == "perdida":
            senales.append((s, ultimo))
            continue
        if s[0] == "vers":
            ultimo = len(out)
        out.append(s)
    return out, senales


def no_ensamblados(sucesos, vers):
    """Renglones leídos que ensambla() no puso en ningún verso.

    Se mira la identidad del texto, justo tras ensambla(): el alineador
    añade la misma cadena que trae el suceso. Un capítulo que empieza con
    continuaciones antes de su primer verso, o una marca por encima del
    tope, se pierden ahí sin aviso.
    """
    puestos = {id(t) for trozos in vers.values() for t in trozos}
    out, ultimo = [], None
    for i, s in enumerate(sucesos):
        if s[0] == "vers":
            texto_s, origen = s[2], s[5] if len(s) > 5 else None
        elif s[0] == "sigue":
            texto_s, origen = s[1], s[3] if len(s) > 3 else None
        else:
            continue
        if texto_s and id(texto_s) not in puestos:
            out.append((("perdida", "renglon_descartado_alineador", texto_s,
                         None, origen), ultimo))
        if s[0] == "vers":
            ultimo = i
    return out


def registra_perdidas(sucesos, senales, procedencia, textos):
    """{ref: {"truncado", "senales", "renglones"}} con solo lo que se vio.

    Nada se compara con otra traducción ni se completa: cada señal lleva el
    renglón y la caja del facsímil donde está la evidencia.
    """
    clave_de = {id(o): k for k, origenes in procedencia.items()
                for o in origenes}

    def verso(i):
        # El verso en curso: el último con marca que el alineador colocó.
        while i is not None and i >= 0:
            s = sucesos[i]
            if s[0] == "vers" and len(s) > 5 and id(s[5]) in clave_de:
                return clave_de[id(s[5])]
            i -= 1
        return None

    reg = {}

    def anota(clave, tipo, texto_s, origen):
        if clave is None:
            return
        e = reg.setdefault(clave, [])
        e.append({"tipo": tipo, "texto": texto_s, "origen": origen})

    for s, i in senales:
        tipo = s[1]
        # Solo es texto perdido si parece texto: «gos.», «do:», «del rey.»;
        # no el titulillo «ABiOs,» ni la sigla «MH».
        if tipo == "fragmento_descartado" and not _minusculas(s[2]):
            tipo = "descarte:basura"
        anota(verso(i), tipo, s[2], s[4])

    # Marca que solo se ve quitando basura y deja la numeración en pico
    # (1, 7, 2…: la inicial «5» de Jos 5 leída «*7»): lo que va tras ella
    # sigue el verso anterior, que queda cortado, y cae en otro hueco.
    marcas = []
    for i, s in enumerate(sucesos):
        if s[0] in ("libro", "cap"):
            marcas.append(None)
        elif s[0] == "vers" and s[4]:
            marcas.append((i, s))
    # La señal «marca_tras_basura» lleva su propio origen, distinto del del
    # verso: se casa por renglón (página, columna, top).
    renglon = {(o["pagina"], o["columna"], o["top"])
               for o in (s[4] for s, _ in senales
                         if s[1] == "marca_tras_basura" and s[4])}
    for k in range(1, len(marcas) - 1):
        a, b, c = marcas[k - 1], marcas[k], marcas[k + 1]
        if not (a and b and c):
            continue
        ob = b[1][5] if len(b[1]) > 5 else None
        if not (ob and (ob["pagina"], ob["columna"], ob["top"]) in renglon):
            continue
        # Lo que sigue a la marca continúa una frase («amorreos, a
        # occidente…», «la casa de Yave…»); el titulillo «EA ÉXOI» o las
        # referencias «17-25, 46).» de una introducción no.
        if a[1][1] < c[1][1] < b[1][1] and b[1][2][:1].islower():
            anota(verso(a[0]), "continuacion_desplazada", b[1][2], ob)
            anota(verso(b[0]), "contenido_desplazado", b[1][2], ob)

    for ref, t in textos.items():
        if RE_CORTE.search(t):
            k = tuple(ref.replace(":", " ").split())
            clave = (k[0], int(k[1]), int(k[2]))
            anota(clave, "guion_final", t[-30:], None)

    # Renglones de cada verso anotado: el de apertura y sus continuaciones,
    # con su caja, para poder volver al facsímil.
    renglones = {}
    actual = None
    for s in sucesos:
        if s[0] == "vers" and len(s) > 5 and id(s[5]) in clave_de:
            actual = clave_de[id(s[5])]
            origen = s[5]
        elif s[0] in ("vers", "sigue") and actual is not None:
            origen = s[5] if s[0] == "vers" else (s[3] if len(s) > 3 else None)
        else:
            continue
        if actual in reg and origen:
            renglones.setdefault(actual, []).append(origen)

    out = {}
    for clave in sorted(reg, key=lambda k: (ORDEN.index(k[0]), k[1], k[2])):
        senales_v = reg[clave]
        lineas = renglones.get(clave, [])
        out["%s %d:%d" % clave] = {
            "truncado": any(x["tipo"] in TRUNCADO for x in senales_v),
            "perdida_probable": any(x["tipo"] in PROBABLE for x in senales_v),
            "senales": senales_v,
            "renglones": lineas,
            "columnas": len({(o["pagina"], o["columna"]) for o in lineas}),
        }
    return out


CORRESPONDENCIAS = os.path.join(DIR, "correspondencias.json")


def carga_correspondencias(ruta=CORRESPONDENCIAS):
    """Correspondencia verso impreso -> NRSVA documentada con el facsímil.

    Solo para salmos donde el título es un versículo impreso y, aun así,
    Leningrad y NRSVA cuentan lo mismo (Sal 13): la cuenta no basta y sin
    fuente local de correspondencia verso a verso hay que documentarla.
    Cada verso impreso va a uno o más NRSVA (0 = título); se exige que
    cubra todos los impresos y todos los NRSVA, sin repetir ninguno.
    """
    with open(ruta, encoding="utf-8") as f:
        datos = json.load(f)
    out = {}
    for ref, d in datos.items():
        osis, cap = ref.split()
        cap = int(cap)
        impresos = {int(k): v for k, v in d["impresos"].items()}
        cortes = {int(k): v for k, v in d.get("cortes", {}).items()}
        destinos = [n for v in impresos.values() for n in v]
        n_heb = SALMOS_HEBREO[cap - 1] if osis == "Ps" else None
        n_nrsva = POR_OSIS[osis]["versos"][cap - 1]
        if sorted(impresos) != list(range(1, (n_heb or len(impresos)) + 1)):
            raise ValueError(f"{ref}: no cubre todos los versos impresos")
        if len(destinos) != len(set(destinos)) or \
                set(destinos) - {0} != set(range(1, n_nrsva + 1)):
            raise ValueError(f"{ref}: destinos NRSVA incompletos o repetidos")
        for imp, dest in impresos.items():
            if len(dest) > 2 or (len(dest) == 2) != (imp in cortes):
                raise ValueError(f"{ref}:{imp}: reparto sin corte documentado")
        if osis == "Ps" and TITULO_SALMOS.get(cap):
            raise ValueError(f"{ref}: ya lo resuelve TITULO_SALMOS")
        out[(osis, cap)] = (impresos, cortes)
    return out


def _parte_en_hemistiquio(trozos, n):
    """Parte los trozos tras el n-ésimo separador «|». None si no lo hay."""
    visto = 0
    for i, t in enumerate(trozos):
        pos = -1
        while True:
            pos = t.find("|", pos + 1)
            if pos < 0:
                break
            visto += 1
            if visto == n:
                antes = trozos[:i] + [t[:pos]]
                despues = [t[pos + 1:]] + trozos[i + 1:]
                return antes, despues
    return None


def aplica_correspondencias(vers, procedencia, avisos, corr):
    """Lleva los versos impresos de cada capítulo documentado a NRSVA.

    Un verso impreso ausente deja su destino vacío (lo suple el visor); si
    el separador de hemistiquio del corte no está en el OCR, el verso entero
    va al primer destino y queda un aviso: nunca se inventa ni se rellena.
    """
    usados = {}
    nuevo_vers = {}
    for (osis, cap, ver), val in vers.items():
        if (osis, cap) not in corr:
            nuevo_vers[(osis, cap, ver)] = val
            continue
        impresos, cortes = corr[(osis, cap)]
        dest = impresos.get(ver)
        if dest is None:
            raise ValueError(f"{osis} {cap}:{ver}: verso impreso fuera de la "
                             "correspondencia documentada")
        partes = [val]
        if len(dest) == 2:
            partes = _parte_en_hemistiquio(val, cortes[ver])
            if partes is None:
                avisos.append(f"{osis} {cap}:{ver}: sin el separador "
                              f"{cortes[ver]} para repartir en {dest}")
                partes = [val]
        usados[(osis, cap, ver)] = dest[:len(partes)]
        for d, p in zip(dest, partes):
            nuevo_vers.setdefault((osis, cap, d), []).extend(p)

    nueva_proc = {}
    for (osis, cap, ver), val in procedencia.items():
        for d in usados.get((osis, cap, ver), [ver] if (osis, cap) not in corr
                            else []):
            nueva_proc.setdefault((osis, cap, d), []).extend(val)
    return nuevo_vers, nueva_proc


def titulos_de_salmo(vers, procedencia):
    """Pasa los Salmos de la numeración impresa (hebrea) a NRSVA.

    El alineador cuenta los versos de cada salmo como los imprime
    Nácar-Colunga, con el título como versículo 1 (o 1-2). NRSVA no numera
    el título: se guarda como versículo 0 del capítulo -- osis.py lo
    escribe como <title> -- y el resto de versos se corre hacia atrás.
    Contarlos con NRSVA hacía que el título ocupara el 1, que el último
    verso se fundiera con el anterior y que el alineador corriera salmos
    enteros de capítulo.
    """
    def mueve(dic):
        out = {}
        for (osis, cap, ver), val in dic.items():
            d = TITULO_SALMOS.get(cap, 0) if osis == "Ps" else 0
            if d and ver <= d:
                clave = (osis, cap, 0)
            else:
                clave = (osis, cap, ver - d)
            out.setdefault(clave, []).extend(val)
        return out
    vers = mueve(vers)
    # Un título de dos versos se imprime «¹ y ² Al maestro del coro…»: el
    # «y» (y el «2» cuando el OCR se come el «y») son la marca, no el
    # título. Sal 52 y 54 lo dejaban como «y Al maestro», Sal 51 como «2 Al».
    for (osis, cap, ver), trozos in vers.items():
        if osis == "Ps" and ver == 0 and TITULO_SALMOS.get(cap) == 2:
            while trozos:
                limpio = RE_MARCA_TITULO.sub("", trozos[0], count=1)
                if limpio.strip():
                    trozos[0] = limpio
                    break
                trozos.pop(0)
    return vers, mueve(procedencia)


RE_MARCA_TITULO = re.compile(r"^\s*(?:y\s+|y$)?(?:2\s+)?")


def main():
    pages = load()
    print(f"{len(pages)} páginas cargadas", flush=True)
    ev, notas_pag = sucesos_y_notas(pages)
    ev, intros_frags = separa_front_matter(ev)
    ev = corrige_repetidos(ev)
    ev, epis = separa_epigrafes(ev)
    ev, senales = separa_perdidas(ev)
    libros = [dict(POR_OSIS[o], versos=SALMOS_HEBREO) if o == "Ps"
              else POR_OSIS[o] for o in ORDEN]
    vers, avisos, procedencia = ensambla(ev, libros)
    senales += no_ensamblados(ev, vers)
    parte_dolar(vers, procedencia, ev)
    vers, procedencia = aplica_correspondencias(
        vers, procedencia, avisos, carga_correspondencias())
    vers, procedencia = titulos_de_salmo(vers, procedencia)
    epigrafes = asigna_epigrafes(epis, procedencia, avisos)
    # Errores de OCR con regla general (NACAR-OCR-102): el vocabulario sale
    # del propio texto, antes de corregir nada.
    crudo = [une(t) for t in vers.values()]
    vocab, vocab_min = vocabulario(crudo), vocabulario_min(crudo)
    todo = {}
    for k, trozos in vers.items():
        todo["%s %d:%d" % k] = repara_cortes_ocr(
            cierra_exclamaciones(une(repara_guiones(trozos, vocab_min)),
                                 vocab),
            vocab_min)
    aplica_erratas(todo, carga_erratas(), avisos)
    perdidas = registra_perdidas(ev, senales, procedencia, todo)

    out_json = os.path.join(DIR, "texto.json")
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(todo, f, ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "procedencia.json"), "w", encoding="utf-8") as f:
        json.dump({"%s %d:%d" % k: v for k, v in procedencia.items()}, f,
                  ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "perdidas.json"), "w",
              encoding="utf-8") as f:
        json.dump(perdidas, f, ensure_ascii=False, indent=0)
    with open(os.path.join(DIR, "epigrafes.json"), "w",
              encoding="utf-8") as f:
        json.dump({"%s %d" % k: {"texto": t, "procedencia": o}
                   for k, (t, o) in sorted(epigrafes.items(),
                                           key=lambda x: (ORDEN.index(x[0][0]),
                                                          x[0][1]))},
                  f, ensure_ascii=False, indent=0)
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

    # Cuentas en NRSVA y sin los títulos de salmo (versículo 0).
    libros = [POR_OSIS[o] for o in ORDEN]
    hay = sum(1 for k in vers if k[2] > 0)
    esp = sum(sum(L["versos"]) for L in libros)
    print(f"\nTOTAL: {hay}/{esp} versículos "
          f"({100 * hay / esp:.1f}%), {len(avisos)} avisos")
    for L in libros:
        g = sum(1 for k in vers if k[0] == L["osis"] and k[2] > 0)
        e = sum(L["versos"])
        marca = "  <-- REVISAR" if e and g / e < 0.60 else ""
        print(f"    {L['osis']:6} {g:5}/{e:5}  {100 * g / e:5.1f}%{marca}")
    print(f"notas en {len(notas)} capítulos")


if __name__ == "__main__":
    main()
