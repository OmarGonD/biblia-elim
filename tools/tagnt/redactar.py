"""Ayudas para redactar fichas a mano (generador manual). Lo mecánico lo genera el código a partir de los datos
(análisis morfológico en español, palabras básicas, texto de variantes del TAGNT, significados léxicos ya escritos
para ese Strong); lo que exige criterio lo escribe la persona: glosa, sentido en contexto, matiz, notas.

  .venv-fichas/bin/python tools/tagnt/redactar.py palabras John.2        # lista las palabras de un capítulo
  from redactar import *
  V("John.2.1", {3: ("día", "Tercer día de la secuencia de 1:19-2:1."), 9: ("Caná", "Aldea de Galilea.", {"r": "Caná"})})

Cada entrada: pos: (glosa, sentido[, extra]). `extra` (todo opcional): r=rango "a|b" (obligatorio si el Strong aún no tiene
rango), con=nota que se añade a la construcción, c=construcción completa, mat=matiz, nt=notas de traducción,
var=texto que se añade al de variantes, otros="a|b", cert="alto|medio|bajo". Las palabras básicas (artículo, καί, δέ, γάρ)
salen solas; se puede dar (glosa,) o (glosa, None, extra) para cambiarlas.
"""
import glob, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import warnings; warnings.simplefilter("ignore")
import alinear, pipeline

D = pipeline.Datos()
CACHE = pipeline.Cache(pipeline.MODELO_MANUAL)

TIPO = {"N": "Sustantivo", "A": "Adjetivo", "T": "Artículo", "P": "Pronombre personal", "D": "Pronombre demostrativo",
        "R": "Pronombre relativo", "K": "Pronombre correlativo", "I": "Pronombre interrogativo",
        "X": "Pronombre indefinido", "F": "Pronombre reflexivo", "C": "Pronombre recíproco",
        "Q": "Pronombre correlativo interrogativo", "S": "Adjetivo posesivo", "CONJ": "Conjunción",
        "PREP": "Preposición", "ADV": "Adverbio", "PRT": "Partícula", "COND": "Partícula condicional",
        "INJ": "Interjección", "HEB": "Palabra hebrea transliterada", "ARAM": "Palabra aramea transliterada"}
CASO = {"N": "nominativo", "G": "genitivo", "D": "dativo", "A": "acusativo", "V": "vocativo"}
NUM = {"S": "singular", "P": "plural"}
GEN = {"M": "masculino", "F": "femenino", "N": "neutro"}
TIEMPO = {"P": "presente", "I": "imperfecto", "F": "futuro", "A": "aoristo", "R": "perfecto", "L": "pluscuamperfecto"}
VOZ = {"A": "activa", "M": "media", "P": "pasiva", "E": "media o pasiva", "D": "media (deponente)",
       "O": "pasiva (deponente)", "N": "media o pasiva (deponente)", "Q": "impersonal"}
MODO = {"I": "indicativo", "S": "subjuntivo", "O": "optativo", "M": "imperativo", "N": "infinitivo", "P": "participio"}
SUFIJO = {"C": "comparativo", "S": "superlativo", "N": "negativo", "I": "interrogativo", "ATT": "forma ática"}
BASICAS_GLOSA = {"G3588": "el", "G2532": "y", "G1161": "pero / y", "G1063": "porque"}
BASICAS_RANGO = {"G3588": "el, la, lo (artículo definido)", "G2532": "y|también, incluso|pero (con matiz adversativo)",
                 "G1161": "pero, y|por otra parte", "G1063": "porque, pues|en efecto"}
BASICAS_CON = {"G2532": "Conjunción coordinante.", "G1161": "Conjunción postpositiva.",
               "G1063": "Conjunción postpositiva causal o explicativa."}


def _cng(seg):
    """'NSM' -> 'nominativo singular masculino'; '1GS' -> '1.ª persona, genitivo singular'; '3S' -> '3.ª persona singular'."""
    out, per = [], ""
    if seg and seg[0] in "123":
        per, seg = "%s.ª persona" % seg[0], seg[1:]
    partes = []
    for ch in seg:
        partes.append(CASO.get(ch) if not partes and ch in CASO else NUM.get(ch) or GEN.get(ch) or CASO.get(ch))
    partes = [p for p in partes if p]
    if per and len(partes) and seg[0] in CASO:
        return per + ", " + " ".join(partes)
    return " ".join(([per] if per else []) + partes)


def analisis(morf):
    """Análisis gramatical en español desde el código morfológico (Robinson)."""
    if not morf:
        return ""
    seg = morf.split("-")
    base = seg[0]
    if base == "V":
        resto = [x for x in seg[1:] if x != "ATT"]
        if not resto:
            return "Verbo"
        tvm, cng = resto[0], (resto[1] if len(resto) > 1 else "")
        t2 = ""
        if tvm[0] == "2":
            t2, tvm = " (2.º)", tvm[1:]
        if len(tvm) != 3:
            return morf
        tiempo = TIEMPO.get(tvm[0], tvm[0]) + t2
        voz = "voz " + VOZ.get(tvm[1], tvm[1])
        if tvm[2] == "N":
            return "Infinitivo %s, %s" % (tiempo, voz)
        if tvm[2] == "P":
            return "Participio %s, %s, %s" % (tiempo, voz, _cng(cng))
        return "%s %s, %s, %s" % (tiempo.capitalize(), MODO.get(tvm[2], tvm[2]), voz, _cng(cng))
    tipo = TIPO.get(base)
    if tipo is None:
        return morf
    resto = [x for x in seg[1:] if x]
    if resto and resto[0] == "PRI":
        return tipo + " indeclinable (nombre propio)"
    if resto and resto[0] == "NUI":
        return tipo + " numeral indeclinable"
    cng = _cng(resto[0]) if resto and resto[0] not in SUFIJO and resto[0] not in ("P", "T", "K") else ""
    suf = [SUFIJO[x] for x in resto if x in SUFIJO]
    s = tipo + (": " + cng if cng else "")
    if suf:
        s += " (" + ", ".join(suf) + ")"
    return s


_rangos = None


def rangos():
    """Último rango_semantico conocido por Strong: fichas_v2 < fichas_v3 < caché manual."""
    global _rangos
    if _rangos is None:
        _rangos = {}
        for d in ("fichas_v2", "fichas_v3"):
            for ruta in sorted(glob.glob(os.path.join(alinear.RAIZ, "data", d, "*.json"))):
                for f in json.load(open(ruta, encoding="utf-8")):
                    if f.get("rango_semantico"):
                        _rangos[f["strong"]] = f["rango_semantico"]
        for ruta in sorted(glob.glob(os.path.join(CACHE.dir, "*.json"))):
            for f in json.load(open(ruta, encoding="utf-8"))["fichas"]:
                if f.get("rango_semantico"):
                    _rangos[f["strong"]] = f["rango_semantico"]
    return _rangos


_glosas = None


def glosas():
    """Glosa más frecuente por (Strong, morfología TAGNT): v2 + v3 + caché manual (la última no manda: un desliz
    aislado no se propaga)."""
    global _glosas, _conteo
    if _glosas is None:
        _conteo = {}
        for d in ("fichas_v2", "fichas_v3"):
            for ruta in sorted(glob.glob(os.path.join(alinear.RAIZ, "data", d, "*.json"))):
                for f in json.load(open(ruta, encoding="utf-8")):
                    if f.get("glosa_interlineal") and f.get("morfologia_tagnt"):
                        _anotar(f["strong"], f["morfologia_tagnt"], f["glosa_interlineal"])
        for ruta in sorted(glob.glob(os.path.join(CACHE.dir, "*.json"))):
            d = json.load(open(ruta, encoding="utf-8"))
            sol = pipeline.solicitud(D, d["ref"])
            for f, w in zip(d["fichas"], sol["palabras"]):
                if f.get("glosa_interlineal") and w.get("morfologia_tagnt"):
                    _anotar(w["strong"], w["morfologia_tagnt"], f["glosa_interlineal"])
        _glosas = _Glosas()
    return _glosas


_conteo = {}


def _anotar(strong, morf, glosa):
    c = _conteo.setdefault((strong, morf), {})
    c[glosa] = c.get(glosa, 0) + 1


class _Glosas:
    def get(self, clave, defecto=None):
        c = _conteo.get(clave)
        return max(c, key=lambda g: (c[g], g)) if c else defecto

    def __setitem__(self, clave, glosa):
        _anotar(clave[0], clave[1], glosa)


# Sentido o mayúscula distintos según el contexto (Hijo/hijo, Dios/dios, Verbo/palabra, Espíritu/viento…): siempre explícitos.
SENTIDO_VARIABLE = {"G5207", "G2316", "G3056", "G4151", "G3962", "G2962", "G5547"}


def reutilizable(w):
    """Solo sustantivos, adjetivos y pronombres: verbos, preposiciones, conjunciones y adverbios cambian de sentido
    con el contexto y se redactan siempre."""
    morf = w.get("morfologia_tagnt") or w["morfologia"]
    if w["strong"] in SENTIDO_VARIABLE or (w["strong"] == "G846" and morf.startswith("P-N")):
        return False
    return morf.split("-")[0] in ("N", "A", "P", "T", "D", "R", "K", "S", "F", "X")


def _trim(forma):
    return forma.strip(".,;·¶ ")


def texto_variantes(w):
    """Texto de variantes tomado de los datos del TAGNT (hecho verificable, sin interpretación)."""
    ed = lambda l: ", ".join(l) if l else "ninguna"
    if w.get("lectura_tagnt"):
        lt = w["lectura_tagnt"]
        if lt.get("propia_de_tisch"):
            return ("Lectura de Tischendorf: %s. Ninguna de las ocho ediciones del TAGNT la trae: todas leen %s."
                    % (_trim(w["forma"]), lt["forma"]))
        return ("Lectura de Tischendorf: %s. El TAGNT lee %s en %s." % (_trim(w["forma"]), lt["forma"], ed(lt["ediciones"])))
    partes = []
    if w.get("variante"):
        partes.append("Esta palabra de Tischendorf no está en %s; sí en %s." % (ed(w["ausente_en"]), ed(w.get("ediciones", []))))
    if w.get("no_en_na28") and not (w.get("variante") and "NA28" in w.get("ausente_en", [])):
        partes.append("No figura en NA28.")
    return " ".join(partes) or None


def ficha(w, spec):
    """Ficha completa (dict del esquema) para la palabra `w` de la solicitud."""
    spec = spec if isinstance(spec, tuple) else ((spec,) if spec else ())
    glosa = spec[0] if spec else None
    if len(spec) > 1 and isinstance(spec[1], dict):      # (glosa, {extras}) sin sentido
        spec = (spec[0], None, spec[1])
    sentido = spec[1] if len(spec) > 1 else None
    if not sentido or len(sentido) < 28:
        sentido = None            # relleno («Sujeto.», «Aoristo.»): mejor vacío que sin información
    ex = spec[2] if len(spec) > 2 else {}
    basico = w["nivel"] == "basico"
    strong = w["strong_tagnt"] if False else w["strong"]
    glosa = glosa or (BASICAS_GLOSA.get(strong) if basico else None)
    if not glosa:
        raise ValueError("falta glosa de pos %d %s" % (w["pos"], w["forma"]))
    rango = ex.get("r")
    rango = [x.strip() for x in rango.split("|")] if rango else (BASICAS_RANGO[strong].split("|") if basico
                                                               else rangos().get(strong))
    if ex.get("r"):
        rangos()[strong] = rango            # el mismo versículo puede repetir la palabra
    if not rango:
        raise ValueError("falta rango (r=) para %s pos %d %s" % (strong, w["pos"], w["forma"]))
    an = analisis(w.get("morfologia_tagnt") or w["morfologia"])
    con = ex.get("c")
    if not con:
        if w.get("caso_regido"):
            con = "Preposición con %s." % w["caso_regido"]
        elif w.get("caso_regido_ambiguo"):
            con = "Preposición; caso regido ambiguo (%s)." % w["caso_regido_ambiguo"]
        else:
            con = BASICAS_CON.get(strong) if basico and strong in BASICAS_CON else (an + "." if an else None)
        if ex.get("con"):
            con = (con + " " if con else "") + ex["con"]
    if basico and strong == "G3588" and not ex.get("c"):
        con = (an + ".") + (" " + ex["con"] if ex.get("con") else "")
    var = None
    if not basico or w.get("variante") or w.get("no_en_na28") or w.get("lectura_tagnt"):
        var = texto_variantes(w)
        if var and ex.get("var"):
            var += " " + ex["var"]
    cert = ex.get("cert", "alto")
    if w.get("caso_regido_ambiguo") and cert == "alto":
        cert = "medio"
    otros = [x.strip() for x in ex["otros"].split("|")] if ex.get("otros") else None
    return {"pos_tisch": w["pos"], "strong": w["strong"], "glosa_interlineal": glosa, "rango_semantico": rango,
            "construccion": con, "sentido_en_contexto": None if basico and not sentido else sentido,
            "matiz": ex.get("mat"), "variantes_textuales": var, "notas_traduccion": ex.get("nt"),
            "otros_usos": otros, "nivel_certeza": cert}


def V(ref, specs):
    """Redacta y guarda un versículo. specs: {pos: (glosa, sentido[, extra])}; las básicas son opcionales."""
    sol = pipeline.solicitud(D, ref)
    specs = dict(specs)
    for w in sol["palabras"]:                     # misma palabra y misma forma ya glosada: se reutiliza
        if w["pos"] not in specs and w["nivel"] != "basico":
            g = glosas().get((w["strong"], w.get("morfologia_tagnt"))) if reutilizable(w) else None
            if g:
                specs[w["pos"]] = (g,)
    faltan = [w["pos"] for w in sol["palabras"] if w["pos"] not in specs and w["nivel"] != "basico"]
    sobran = [p for p in specs if p not in {w["pos"] for w in sol["palabras"]}]
    if faltan or sobran:
        print(ref, "FALTAN", faltan, "SOBRAN", sobran)
        return False
    try:
        out = [ficha(w, specs.get(w["pos"])) for w in sol["palabras"]]
    except ValueError as e:
        print(ref, "ERROR:", e)
        return False
    err = pipeline.validar(out, sol)
    if err:
        print(ref, "INVÁLIDO:", err)
        return False
    CACHE.put(ref, out)
    for w, f in zip(sol["palabras"], out):         # lo recién escrito alimenta a los versículos siguientes
        if w.get("morfologia_tagnt") and f["glosa_interlineal"]:
            glosas()[(w["strong"], w["morfologia_tagnt"])] = f["glosa_interlineal"]
        if f["rango_semantico"]:
            rangos()[w["strong"]] = f["rango_semantico"]
    print(ref, "ok", len(out))
    return True


def listar(prefijo):
    """Por versículo: el texto griego y solo las palabras no básicas. Si la forma ya tiene glosa se muestra «=glosa»
    (se reutiliza sola; se puede corregir dando una entrada); si no, se detalla para que se redacte."""
    for r in D.versiculos(prefijo):
        s = pipeline.solicitud(D, r)
        print("##", r, s["texto_griego"])
        for w in s["palabras"]:
            if w["nivel"] == "basico":
                continue
            ex = []
            if w.get("caso_regido"):
                ex.append("rige:" + w["caso_regido"])
            if w.get("caso_regido_ambiguo"):
                ex.append("AMB")
            if w.get("variante"):
                ex.append("VAR-" + ",".join(w["ausente_en"]))
            if w.get("no_en_na28"):
                ex.append("noNA28")
            if w.get("lectura_tagnt"):
                ex.append("LECT:" + w["lectura_tagnt"]["forma"])
            g = glosas().get((w["strong"], w.get("morfologia_tagnt"))) if reutilizable(w) else None
            falta = "" if w["strong"] in rangos() else " [SIN RANGO]"
            if g and not ex and not falta:
                continue                      # se reutiliza sola
            print(w["pos"], _trim(w["forma"]) + ("/" + w["lema"] if falta else ""), "G" + w["strong"][1:] if falta else "",
                  w["morfologia"], ("=" + g + " ") if g else "", " ".join(ex) + falta)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "palabras":
        listar(sys.argv[2])
