"""Datos TAGNT por palabra para fichas y para el contexto del generador (Fase 3):
morfología, Strong extendido, ediciones, variante y caso regido de las preposiciones."""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tagnt

RE_INF = re.compile(r"^V-[2]?[PIFARL][AMPEDO]N$")
RE_CASO = re.compile(r"^[123]?([NGDAV])([SP])[MFN]?$")
NOMBRE_CASO = {"N": "nominativo", "G": "genitivo", "D": "dativo", "A": "acusativo", "V": "vocativo"}


def caso(p):
    """Caso gramatical de la palabra, o None (verbos finitos, infinitivos, indeclinables)."""
    base = p.gramatica.split(" + ")[0]
    for seg in base.split("-")[1:]:
        m = RE_CASO.match(seg)
        if m:
            return m.group(1)
    return None


# Partículas pospositivas: no interrumpen el régimen de la preposición (ἐν δὲ τῷ…, περὶ γὰρ τοῦ…).
POSPOSITIVAS = {"δέ", "δὲ", "γάρ", "γὰρ", "τε", "οὖν", "μέν", "μὲν", "δ’", "γ’"}
# Léxemas que TAGNT etiqueta PREP pero que se usan como conjunción/adverbio ante verbo u oración.
USO_CONJUNTIVO = {"ἕως", "ὅπου", "ὅτε", "ἄχρι", "ἄχρις", "μέχρι", "μέχρις", "πρίν", "ὡς"}


def caso_regido(pals, i):
    """Caso que rige la preposición pals[i] -> (caso|None, ambiguo: bool, motivo).
    Se toma el primer elemento con caso dentro de las 3 palabras siguientes; es ambiguo si
    se interpone una conjunción/verbo, si no hay ninguno, o si el artículo y el siguiente
    nominal no concuerdan. Nunca se adivina: ambiguo => caso None."""
    if pals[i].gramatica.split(" + ")[0] != "PREP":
        return None, False, "no es preposición"
    sig = pals[i + 1:i + 4]
    for d, q in enumerate(sig):
        c = caso(q)
        tipo = q.gramatica.split(" + ")[0].split("-")[0]
        if c is None and q.griego.lower() in POSPOSITIVAS:
            continue
        if c is None:
            if tipo in ("CONJ", "V", "COND", "PREP", "INJ"):
                if tipo == "V" and pals[i].griego.lower() in USO_CONJUNTIVO:
                    return None, False, "uso conjuntivo ante verbo"
                return None, True, "se interpone %s (%s)" % (tipo, q.griego)
            continue                                   # PRT / ADV: se salta
        if tipo == "T":                                # artículo: debe concordar con el siguiente nominal
            for q2 in pals[i + 2 + d:i + 5 + d]:
                if RE_INF.match(q2.gramatica.split(" + ")[0]):
                    break                              # infinitivo articular (πρὸ τοῦ + inf.): rige el artículo
                c2 = caso(q2)
                if c2 is not None and q2.gramatica.split("-")[0] != "T":
                    if c2 != c:
                        return None, True, "artículo (%s) y %s no concuerdan" % (c, c2)
                    break
        return c, False, ""
    if pals[i].griego.lower() in USO_CONJUNTIVO:
        return None, False, "uso conjuntivo/adverbial"
    return None, True, "sin elemento con caso en las 3 palabras siguientes"


def info(pals, i):
    """Diccionario TAGNT para pals[i] (fichas y prompt)."""
    p = pals[i]
    d = {"tagnt": p.clave, "morfologia_tagnt": p.gramatica, "dstrong": p.dstrong,
         "strong_tagnt": p.strong, "ediciones": p.ediciones, "ausente_en": p.ausente_en(),
         "variante": p.variante, "glosa_tagnt": p.glosa}
    if p.gramatica.split(" + ")[0] == "PREP":
        c, amb, motivo = caso_regido(pals, i)
        d["caso_regido"] = NOMBRE_CASO.get(c)
        if amb:
            d["caso_regido_ambiguo"] = motivo
    return d
