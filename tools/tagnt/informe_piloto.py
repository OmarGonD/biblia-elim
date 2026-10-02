#!/usr/bin/env python3
"""Genera reports/piloto_jn1.md desde data/fichas_v3/John.01.json y la caché del generador manual.
Uso: .venv-fichas/bin/python tools/tagnt/informe_piloto.py"""
import collections, json, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pipeline

RAIZ = pipeline.RAIZ
F = json.load(open(os.path.join(RAIZ, "data", "fichas_v3", "John.01.json"), encoding="utf-8"))
by = {(f["ref"], f["pos_tisch"]): f for f in F}
GR = re.compile(r"[Ͱ-Ͽἀ-῿]")
# Tiempo observado (marcas de archivo y de git de la sesión del 2026-10-02): lectura de palabras 16:43, última corrección 17:05.
MIN_OBSERVADOS = 22


def vref(r):
    o, c, v = r.split(".")
    return "Jn %s:%s" % (c, v)


def verso_txt(ref):
    return " ".join(f["forma"] for f in F if f["ref"] == ref)


def render(ref, pos, comp=False):
    f = by[(ref, pos)]
    L = ["#### %s · pos. %d · %s" % (vref(ref), pos, f["forma"].strip(".,;·"))]
    meta = ["lema %s" % f["lema"], f["strong"], "morf. TAGNT %s" % f.get("morfologia_tagnt", "—"),
            "alineación `%s`" % f["alineacion"], "TAGNT `%s`" % f.get("tagnt")]
    if f.get("caso_regido"):
        meta.append("rige **%s**" % f["caso_regido"])
    if f.get("caso_regido_ambiguo"):
        meta.append("caso regido ambiguo: %s" % f["caso_regido_ambiguo"])
    if f.get("variante"):
        meta.append("variante (ausente en %s)" % ", ".join(f["ausente_en"]))
    if f.get("no_en_na28"):
        meta.append("**no está en NA28**")
    if f.get("lectura_tagnt"):
        meta.append("lectura distinta del TAGNT: %s (%s)" % (f["lectura_tagnt"]["forma"], ", ".join(f["lectura_tagnt"]["ediciones"])))
    L.append("*" + " · ".join(meta) + "*\n")
    L.append("- **Glosa:** %s" % f["glosa_interlineal"])
    if f["rango_semantico"]:
        L.append("- **Rango semántico:** " + "; ".join(f["rango_semantico"]))
    for k, n in (("construccion", "Construcción"), ("sentido_en_contexto", "Sentido en contexto"), ("matiz", "Matiz"),
                 ("variantes_textuales", "Variantes textuales"), ("notas_traduccion", "Notas de traducción")):
        if f[k]:
            L.append("- **%s:** %s" % (n, f[k]))
    if f["otros_usos"]:
        L.append("- **Otros usos:** " + " · ".join(f["otros_usos"]))
    L.append("- **Certeza:** %s" % f["nivel_certeza"])
    if comp and f.get("traducciones_comparadas"):
        L.append("- **Traducciones comparadas del versículo:**")
        for e in f["traducciones_comparadas"]:
            L.append("  - *%s*: %s%s" % (e["version"], e["texto"], " ⚠ " + e["aviso"] if e.get("aviso") else ""))
    return "\n".join(L) + "\n"


def tokens(g, o, rg, ro):
    return g / rg + o / ro


def main():
    datos = pipeline.Datos()
    cache = pipeline.Cache(pipeline.MODELO_MANUAL)
    refs = datos.versiculos("John.1")
    sols = [pipeline.solicitud(datos, r) for r in refs]
    n = len(F)
    basicas = sum(1 for s in sols for w in s["palabras"] if w["nivel"] == "basico")
    var = [f for f in F if f.get("variante") or f.get("no_en_na28")]
    lect = [f for f in F if f.get("lectura_tagnt")]
    amb = [f for f in F if f.get("caso_regido_ambiguo")]
    prep = [f for f in F if f.get("caso_regido")]
    cer = collections.Counter(f["nivel_certeza"] for f in F)
    cal = collections.Counter(f["alineacion"].split("+")[0] for f in F)
    # tamaños reales
    sg = so = og = oo = 0
    for s, r in zip(sols, refs):
        u = json.dumps(s, ensure_ascii=False)
        g = len(GR.findall(u)); sg += g; so += len(u) - g
        a = json.dumps(cache.get(r), ensure_ascii=False, separators=(",", ":"))
        g = len(GR.findall(a)); og += g; oo += len(a) - g
    sysp = pipeline.prompt_texto()
    pg = len(GR.findall(sysp)); po = len(sysp) - pg
    NT_PAL, NT_VERS = 137098, 7895
    rangos = {"bajo": (2.5, 4.0), "medio": (2.0, 3.5), "alto": (1.5, 3.0)}
    tk = {}
    for k, (rg, ro) in rangos.items():
        ti, to, tp = tokens(sg, so, rg, ro), tokens(og, oo, rg, ro), tokens(pg, po, rg, ro)
        tk[k] = dict(ent_pal=ti / n, sal_pal=to / n, prompt=tp,
                     nt_ent=ti / n * NT_PAL, nt_sal=to / n * NT_PAL, nt_prompt=tp * NT_VERS,
                     pil_ent=ti, pil_sal=to)
    m = tk["medio"]
    o = []
    w = o.append
    w("# Piloto Jn 1 — fichas generadas por el generador manual\n")
    w("Generado por `tools/tagnt/informe_piloto.py` a partir de `data/fichas_v3/John.01.json` (modelo `%s`, prompt `%s`). Las 821 fichas las escribió el asistente a mano, versículo por versículo; cada una pasó el esquema JSON y las reglas semánticas de `tools/tagnt/pipeline.py`. Las fichas viejas (`data/fichas_interlineal/`) y los datos de Tischendorf no se han tocado.\n" % (pipeline.MODELO_MANUAL, pipeline.prompt_hash()))
    w("## 1. Cifras\n")
    w("| | |\n|---|---|")
    w("| Versículos | 51 (todo Jn 1, versificación del módulo Tisch) |")
    w("| Fichas | %d (una por posición de Tisch) |" % n)
    w("| Palabras básicas (artículo, καί, δέ, γάρ) | %d (%.1f %%) |" % (basicas, 100 * basicas / n))
    w("| Palabras completas | %d |" % (n - basicas))
    w("| Certeza | alto %d · medio %d · bajo %d |" % (cer["alto"], cer["medio"], cer["bajo"]))
    w("| Alineación Tisch→TAGNT | `forma` %d · `forma_otro_strong` %d · `strong` %d · sin pareja %d |" % (cal["forma"], cal["forma_otro_strong"], cal["strong"], cal.get("sin_pareja", 0)))
    w("| Palabras con variante del TAGNT o fuera de NA28 | %d |" % len(var))
    w("| Lecturas de Tisch distintas del TAGNT (`lectura_tagnt`) | %d |" % len(lect))
    w("| Preposiciones con `caso_regido` | %d resueltas, %d ambiguas |\n" % (len(prep), len(amb)))
    w("Validación: 51/51 versículos pasan esquema y reglas. Durante la escritura el validador rechazó varios intentos míos (`variantes_textuales` mal situado en 3 versículos, más errores de sintaxis de mis propios archivos); se corrigieron antes de guardar y no quedó ningún versículo con error. Después de validar corregí a mano 3 datos de contenido que detecté al releer (una lista de ediciones de 1:26, dos erratas); eso no lo detecta el validador.\n")
    w("## 2. Fichas pedidas\n")
    w("### 2.1 πρὸς (Jn 1:1-2)\n"); w(render("John.1.1", 10)); w(render("John.1.2", 5))
    w("### 2.2 ἦν (Jn 1:1, tres apariciones)\n")
    for p in (3, 9, 15):
        w(render("John.1.1", p))
    w("### 2.3 λόγος (Jn 1:1, tres apariciones)\n")
    for p in (5, 8, 17):
        w(render("John.1.1", p))
    w("### 2.4 θεός (Jn 1:1, con y sin artículo)\n")
    w("Texto de Tisch: *%s*\n" % verso_txt("John.1.1"))
    w(render("John.1.1", 12)); w(render("John.1.1", 14))
    w("Las dos fichas explican las lecturas (identidad, cualitativa, indefinida «un dios») sin elegir ninguna; la segunda va con certeza `medio`.\n")
    w("## 3. Casos pedidos\n")
    w("### 3.1 Jn 1:3-4: γέγονεν y la puntuación\n")
    w("¿Menciona la ficha la diferencia de puntuación (cierra el v. 3 o abre el v. 4)? **Sí.** La de γέγονεν la explica; la de ὃ remite a ella; la de ἐστιν (1:4) añade que Tisch lee ἐστιν donde las ocho ediciones del TAGNT leen ἦν.\n")
    w("Texto: 1:3 *%s* · 1:4 *%s*\n" % (verso_txt("John.1.3"), verso_txt("John.1.4")))
    w(render("John.1.3", 11)); w(render("John.1.3", 12)); w(render("John.1.4", 4))
    w("**Límite:** del TAGNT solo puedo afirmar que cierra el v. 3 tras γέγονεν (lleva punto) y que Tisch no trae signos de puntuación. **No verifiqué con datos la puntuación de NA28**; la ficha no la afirma y queda en `medio`.\n")
    w("### 3.2 Jn 1:18: Tisch lee υἱός y NA28 lee θεός\n")
    w("Texto de Tisch: *%s*\n" % verso_txt("John.1.18"))
    w("Cómo se refleja: (a) υἱός queda alineada con el θεός del TAGNT por Strong alternativo y se marca `lectura_tagnt` con las ediciones que leen θεός (NA28, NA27, SBL, WH, Treg); (b) el artículo ὁ de Tisch se marca `no_en_na28`; (c) ambas fichas llevan `variantes_textuales` y certeza `medio`; (d) la ficha cita las dos versiones de dominio público con su nombre exacto.\n")
    w(render("John.1.18", 5)); w(render("John.1.18", 7, comp=True))
    w("### 3.3 Jn 1:28: Βηθανίᾳ vs. Βηθαβαρᾷ (TR)\n")
    w("Texto de Tisch: *%s*\n" % verso_txt("John.1.28"))
    w("Cuando la Reina-Valera 1909 sigue al TR, la ficha muestra la lectura de Tisch y NA28, dice que TR lee Βηθαβαρᾷ, y cita en `notas_traduccion` la Reina-Valera 1909 («Betábara») y Torres Amat («Bethania») con el nombre exacto de cada versión; `traducciones_comparadas` trae además el versículo completo de ambas.\n")
    w(render("John.1.28", 3, comp=True))
    w("### 3.4 Palabras con variante o fuera de NA28 (%d)\n" % len(var))
    w("| Ref | Pos | Forma | Qué marca el TAGNT |\n|---|---|---|---|")
    for f in sorted(var, key=lambda f: (int(f["ref"].split(".")[2]), f["pos_tisch"])):
        q = ("**no está en NA28**; ausente en " + ", ".join(f["ausente_en"])) if f.get("no_en_na28") else "ausente en " + ", ".join(f["ausente_en"])
        w("| %s | %d | %s | %s |" % (vref(f["ref"]), f["pos_tisch"], f["forma"].strip(".,;·"), q))
    w("")
    w("### 3.5 Lecturas de Tisch que el TAGNT no tiene (%d)\n" % len(lect))
    w("El TAGNT no incluye a Tischendorf, así que estas diferencias no figuran como «variante». El pipeline las detecta al comparar formas alineadas; se descartan las meramente ortográficas (Ἡλείας/Ἠλίας, ῥαββεί/ῥαββί, Λευείτας/Λευίτας, Ἰσραηλείτης/Ἰσραηλίτης).\n")
    w("| Ref | Tisch lee | TAGNT lee | Ediciones que leen la forma del TAGNT |\n|---|---|---|---|")
    for f in lect:
        w("| %s | %s | %s | %s |" % (vref(f["ref"]), f["forma"].strip(".,;·"), f["lectura_tagnt"]["forma"], ", ".join(f["lectura_tagnt"]["ediciones"])))
    w("")
    w("### 3.6 `caso_regido` ambiguo (%d de %d preposiciones)\n" % (len(amb), len(prep) + len(amb)))
    w("En ninguna se adivinó el caso: el campo queda vacío, la ficha explica el motivo y la certeza no es `alto`. **Ambas ambigüedades son falsos positivos de la herramienta** (ὅπου es adverbio, no preposición; en πρὸ τοῦ + infinitivo el régimen es genitivo normal). Las fichas lo dicen.\n")
    for f in amb:
        w(render(f["ref"], f["pos_tisch"]))
    w("## 4. Hallazgos y problemas encontrados\n")
    w("1. **Versificación del módulo Tisch en Jn 1:39-51.** El módulo corta distinto del TAGNT: su «1:39» empieza en «τί ζητεῖτε;» (que en la numeración habitual es 1:38b), y así hasta 1:51. Las fichas llevan la ref del módulo (la que usa la app) y el alineamiento enlaza con la ref TAGNT correcta, pero **la numeración de Jn 1:39-51 en la app no coincide con la de otras Biblias**.")
    w("2. **Errores de etiquetado del módulo Tisch.** ὅ y ὃ (relativos) en 1:42 y 1:43 vienen con Strong G3588 (artículo); el pipeline ya no los trata como palabras básicas cuando el TAGNT discrepa, y la ficha lo dice. τι en 1:47 e ἴδε también discrepan de Strong, sin efecto en el sentido.")
    w("3. **El TAGNT no incluye a Tischendorf.** Solo detectamos 5 lecturas propias de Tisch en Jn 1 porque comparamos con el TAGNT; las que coincidan por accidente con alguna edición no se marcan como variante de Tisch. Es una limitación de la fuente, no del pipeline.")
    w("4. **Torres Amat (OCR).** El texto del módulo trae errores de OCR; detectamos los de símbolos sueltos, pero no erratas de letras («hautizando», «hom.bres»). Las fichas citan solo versículos sin esas marcas en `textos_pd`, y `traducciones_comparadas` marca los dudosos con `aviso`. Conviene decidir si Torres Amat se muestra o se omite.")
    w("5. **Dos falsos positivos de `caso_regido_ambiguo`** (3.6): ὅπου (TAGNT lo etiqueta PREP) y πρὸ τοῦ + infinitivo. Se pueden filtrar en `contexto.py`.")
    w("6. **Jn 8:53 (defecto del módulo, ver `data/sources/NOTAS_TISCH.md`)** no se puede generar con este pipeline tal cual: son 374 palabras (~50 000 tokens de salida) y un límite de 16 000 tokens por llamada. Hay que dividirlo en bloques y decidir qué hacer con Jn 8:12-52, que el módulo no entrega. Es el único versículo del NT por encima de 60 palabras.\n")
    w("## 5. Costo por API y tiempo\n")
    w("**Cómo se midió.** No hay clave de la API en este entorno ni tokenizador local, así que **no puedo dar tokens reales exactos**; los calculé desde los caracteres reales del piloto (JSON de solicitud y de respuesta), con 3 rangos de caracteres por token según la escritura (español/JSON y griego politónico). Para obtener el número exacto basta ejecutar `count_tokens` de la API sobre las solicitudes (`pipeline.py batch-export`). Los precios por token de estos modelos no los conozco con certeza, así que **no invento una tarifa**: doy tokens y la fórmula.\n")
    w("| Tokens (estimados) | bajo | medio | alto |\n|---|---|---|---|")
    w("| Piloto: entrada (solicitudes, 51 llamadas) | %s | %s | %s |" % tuple("{:,.0f}".format(tk[k]["pil_ent"]) for k in ("bajo", "medio", "alto")))
    w("| Piloto: salida (fichas) | %s | %s | %s |" % tuple("{:,.0f}".format(tk[k]["pil_sal"]) for k in ("bajo", "medio", "alto")))
    w("| Prompt de sistema (por llamada, cacheable) | %s | %s | %s |" % tuple("{:,.0f}".format(tk[k]["prompt"]) for k in ("bajo", "medio", "alto")))
    w("| **NT completo**: entrada (%s palabras) | %s | %s | %s |" % ("{:,}".format(NT_PAL), *("{:,.1f} M".format(tk[k]["nt_ent"] / 1e6) for k in ("bajo", "medio", "alto"))))
    w("| **NT completo**: salida | %s | %s | %s |" % tuple("{:,.1f} M".format(tk[k]["nt_sal"] / 1e6) for k in ("bajo", "medio", "alto")))
    w("| **NT completo**: prompt × %s llamadas | %s | %s | %s |\n" % ("{:,}".format(NT_VERS), *("{:,.1f} M".format(tk[k]["nt_prompt"] / 1e6) for k in ("bajo", "medio", "alto"))))
    w("Por palabra (medio): entrada %.0f tokens, salida %.0f tokens.\n" % (m["ent_pal"], m["sal_pal"]))
    w("**Fórmula del costo del NT completo** (en dólares, con tarifas en $ por millón de tokens):\n")
    w("- Sin Batch, con caché de prompt: `entrada × P_ent + salida × P_sal + prompt_cache_lectura × P_ent × 0,1 + (primeras escrituras)`.")
    w("- Con Batch: la Batch API se publicitó históricamente con un **50 % de descuento** sobre ambas tarifas (a verificar con la tarifa vigente); no se combina siempre con caché de prompt, así que conviene medirlo en un lote pequeño.")
    w("- Con las cifras medias: costo ≈ %.1f × P_sal + %.1f × P_ent (en dólares, con P en $ por millón de tokens), más el prompt de sistema. **La salida domina.** Cada $1 por millón de tokens de salida equivale a ≈ $%.0f por todo el NT sin Batch (≈ $%.0f con un 50 %% de descuento).\n" % (m["nt_sal"] / 1e6, m["nt_ent"] / 1e6, m["nt_sal"] / 1e6, m["nt_sal"] / 2e6))
    cards_min = n / MIN_OBSERVADOS
    horas = NT_PAL / cards_min / 60
    w("**Tiempo de seguir con el generador manual.** En esta sesión, de leer las palabras de Jn 1 a la última corrección pasaron ≈ %d minutos para 821 fichas (≈ %.0f fichas/min, incluidas las correcciones). Extrapolado a %s palabras: **≈ %.0f horas de trabajo continuo (≈ %.1f días)**, sin contar los versículos largos ni la revisión de usted. El consumo de contexto observado en esta sesión fue del orden de 175 000 tokens por 821 fichas (≈ 215 por ficha, contando lectura, escritura y correcciones); a ese ritmo el NT completo necesitaría ≈ 29 M tokens de contexto, **más de lo que queda en esta sesión (≈ 14,7 M)**. Conclusión: con el generador manual y esta calidad, completar el NT no cabe en lo que queda; cabría aproximadamente la mitad.\n" % (MIN_OBSERVADOS, cards_min, "{:,}".format(NT_PAL), horas, horas / 24))
    w("## 6. Qué necesito de su revisión\n")
    w("1. ¿Los campos y la profundidad de las fichas (sobre todo `sentido_en_contexto` y `variantes_textuales`) son los que quiere ver en la app?")
    w("2. ¿Se muestran a los usuarios las lecturas propias de Tisch (`lectura_tagnt`)? Hoy el TAGNT es la única referencia para compararlas.")
    w("3. Torres Amat: ¿se muestra con el aviso de OCR, se limpia de forma manual o se omite en `traducciones_comparadas`?")
    w("4. Jn 1:39-51: la numeración de versículos del módulo difiere de la habitual. ¿Se acepta tal cual o se reasigna?")
    w("5. Generador para el resto del NT: manual (no cabe), API síncrona o Batch. Si es API, necesito la clave y la tarifa para fijar el presupuesto.")
    w("6. Jn 8:53: ¿omitimos ese versículo hasta que CrossWire corrija el módulo?")
    open(os.path.join(RAIZ, "reports", "piloto_jn1.md"), "w", encoding="utf-8").write("\n".join(o))
    print("ok", len("\n".join(o)))


if __name__ == "__main__":
    main()
