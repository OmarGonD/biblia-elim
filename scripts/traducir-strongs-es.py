#!/usr/bin/env python3
"""Traduce el léxico Strong 1890 (dominio público) al español y añade
glosas de la Reina-Valera 1909 (dominio público, módulo CrossWire SpaRV1909).

Fuentes:
  - James Strong, Exhaustive Concordance (1890), dominio público
  - Open Scriptures Strong's XML (CC-BY-SA / PD)
  - Reina-Valera 1909, dominio público (etiquetado Strong: SpaRV1909 / CrossWire)
"""
from __future__ import annotations

import html
import json
import re
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path("/home/ogonzales/Projects/biblia_elim")
SRC = ROOT / "ui" / "strongs-elim.xml"
GLOSSES = Path("/tmp/rv1909-strongs-es.json")

# Frases primero (más largas). Metalenguaje del Strong 1890.
PHRASES = [
    ("literally or figuratively", "literal o figurativamente"),
    ("figuratively or literally", "figurativa o literalmente"),
    ("literal or figurative", "literal o figurativo"),
    ("of uncertain derivation", "de derivación incierta"),
    ("of uncertain affinity", "de afinidad incierta"),
    ("of uncertain origin", "de origen incierta"),
    ("of Hebrew origin", "de origen hebreo"),
    ("of Hebrew der", "de derivación hebrea"),
    ("of Chaldee origin", "de origen caldeo"),
    ("of Chaldee der", "de derivación caldea"),
    ("of Aramaic origin", "de origen arameo"),
    ("of Latin origin", "de origen latino"),
    ("of Greek origin", "de origen griego"),
    ("of foreign origin", "de origen extranjero"),
    ("of Egyptian origin", "de origen egipcio"),
    ("of Persian origin", "de origen persa"),
    ("of Phoenician origin", "de origen fenicio"),
    ("a primitive root", "raíz primitiva"),
    ("a primitive particle", "partícula primitiva"),
    ("a primitive word", "palabra primitiva"),
    ("a primary particle", "partícula primaria"),
    ("a primary preposition", "preposición primaria"),
    ("a primary numeral", "numeral primario"),
    ("a primary verb", "verbo primario"),
    ("a primary pronoun", "pronombre primario"),
    ("by implication", "por implicación"),
    ("by extension", "por extensión"),
    ("by analogy", "por analogía"),
    ("by Hebraism, very", "por hebraísmo, muy (intensivo)"),
    ("by Hebraism", "por hebraísmo"),
    ("love-feast", "ágape (comida fraterna)"),
    ("in a social or moral sense", "en sentido social o moral"),
    ("social or moral", "social o moral"),
    ("Jewish national name of God", "nombre nacional judío de Dios"),
    ("in the ordinary sense", "en el sentido ordinario"),
    ("occasionally applied", "aplicado a veces"),
    ("by way of deference", "por deferencia"),
    ("by reduplication", "por reduplicación"),
    ("by permutation", "por permutación"),
    ("in various applications", "en varias aplicaciones"),
    ("in a wide application", "en sentido amplio"),
    ("in a wide sense", "en sentido amplio"),
    ("used as a preposition", "usado como preposición"),
    ("used as an adverb", "usado como adverbio"),
    ("used as a particle", "usado como partícula"),
    ("used only as", "usado solo como"),
    ("the same as", "lo mismo que"),
    ("the same as", "lo mismo que"),
    ("through the idea of", "mediante la idea de"),
    ("in the sense of", "en el sentido de"),
    ("in order that", "a fin de que"),
    ("denoting the purpose or the result", "denotando el propósito o el resultado"),
    ("denoting the purpose", "denotando el propósito"),
    ("a place in Palestine", "lugar en Palestina"),
    ("a place in Pal", "lugar en Palestina"),
    ("an Israelite", "un israelita"),
    ("an Israelitess", "una israelita"),
    ("a primitive", "un primitivo"),
    ("compare also", "compárese también"),
    ("compare ", "compárese "),
    ("probably from", "probablemente de"),
    ("perhaps from", "quizá de"),
    ("apparently from", "al parecer de"),
    ("derived from", "derivado de"),
    ("contracted from", "contracción de"),
    ("prolonged from", "prolongación de"),
    ("reduplicated from", "reduplicado de"),
    ("akin to", "afín a"),
    ("corresponding to", "que corresponde a"),
    ("another form for", "otra forma de"),
    ("another form of", "otra forma de"),
    ("the fem. of", "el femenino de"),
    ("the masc. of", "el masculino de"),
    ("the neuter of", "el neutro de"),
    ("the plural of", "el plural de"),
    ("the genitive case of", "el genitivo de"),
    ("the genitive of", "el genitivo de"),
    ("the dative of", "el dativo de"),
    ("the accusative of", "el acusativo de"),
    ("third person", "tercera persona"),
    ("first person", "primera persona"),
    ("second person", "segunda persona"),
    ("a (female)", "una (mujer)"),
    ("an official title of honor", "título oficial de honor"),
    ("an official title", "título oficial"),
    ("including the feminine", "incluido el femenino"),
    ("including the masculine", "incluido el masculino"),
    ("of either gender", "de uno u otro género"),
    ("as a proper name", "como nombre propio"),
    ("as a particle", "como partícula"),
    ("as a preposition", "como preposición"),
    ("as an adverb", "como adverbio"),
    ("as a noun", "como sustantivo"),
    ("as a verb", "como verbo"),
    ("abstractly or concretely", "en abstracto o en concreto"),
    ("concretely or abstractly", "en concreto o en abstracto"),
    ("literally and figuratively", "literal y figurativamente"),
]

WORDS = {
    "figuratively": "figurativamente",
    "figurative": "figurativo",
    "literally": "literalmente",
    "literal": "literal",
    "implication": "implicación",
    "properly": "propiamente",
    "especially": "especialmente",
    "specially": "especialmente",
    "specifically": "específicamente",
    "concretely": "en concreto",
    "abstractly": "en abstracto",
    "collectively": "colectivamente",
    "causatively": "causativamente",
    "reflexively": "reflexivamente",
    "transitively": "transitivamente",
    "intransitively": "intransitivamente",
    "adverbially": "adverbialmente",
    "generally": "en general",
    "usually": "habitualmente",
    "often": "a menudo",
    "perhaps": "quizá",
    "probably": "probablemente",
    "apparently": "al parecer",
    "hence": "de aquí",
    "henceforth": "en adelante",
    "also": "también",
    "including": "incluido",
    "used": "usado",
    "only": "solo",
    "something": "algo",
    "someone": "alguien",
    "somebody": "alguien",
    "anything": "cualquier cosa",
    "nothing": "nada",
    "oneself": "uno mismo",
    "itself": "sí mismo",
    "himself": "él mismo",
    "herself": "ella misma",
    "themselves": "ellos mismos",
    "another": "otro",
    "other": "otro",
    "others": "otros",
    "same": "mismo",
    "such": "tal",
    "any": "cualquier",
    "every": "cada",
    "all": "todo",
    "both": "ambos",
    "each": "cada",
    "either": "uno u otro",
    "neither": "ninguno",
    "not": "no",
    "without": "sin",
    "with": "con",
    "from": "de",
    "into": "hacia",
    "upon": "sobre",
    "over": "sobre",
    "under": "bajo",
    "through": "a través de",
    "between": "entre",
    "among": "entre",
    "against": "contra",
    "toward": "hacia",
    "towards": "hacia",
    "before": "delante de",
    "after": "después de",
    "above": "encima de",
    "below": "debajo de",
    "near": "cerca",
    "off": "fuera",
    "out": "fuera",
    "down": "abajo",
    "away": "lejos",
    "together": "junto",
    "apart": "aparte",
    "again": "de nuevo",
    "even": "aun",
    "very": "muy",
    "more": "más",
    "most": "más",
    "less": "menos",
    "than": "que",
    "then": "entonces",
    "thus": "así",
    "so": "así",
    "magistrate": "magistrado",
    "magistrates": "magistrados",
    "deity": "deidad",
    "deities": "deidades",
    "divinity": "divinidad",
    "godlike": "divino",
    "female": "femenino",
    "male": "masculino",
    "opponent": "adversario",
    "being": "ser",
    "privilege": "privilegio",
    "force": "fuerza",
    "capacity": "capacidad",
    "competency": "competencia",
    "freedom": "libertad",
    "mastery": "señorío",
    "superhuman": "sobrehumano",
    "potential": "potencial",
    "subjectively": "en sentido subjetivo",
    "objectively": "en sentido objetivo",
    "greatness": "grandeza",
    "symbol": "símbolo",
    "idolatry": "idolatría",
    "affection": "afecto",
    "benevolence": "benevolencia",
    "feast": "banquete",
    "social": "social",
    "moral": "moral",
    "jewish": "judío",
    "national": "nacional",
    "jehovah": "Jehová",
    "officer": "oficial",
    "resist": "resistir",
    "daemonic": "demoníaco",
    "demonic": "demoníaco",
    "demon": "demonio",
    "oblique": "oblicuo",
    "cases": "casos",
    "there": "allí",
    "instead": "en lugar",
    "otherwise": "de otro modo",
    "obsolete": "en desuso",
    "cognate": "cognado",
    "latins": "latinos",
    "phoenician": "fenicio",
    "phœnician": "fenicio",
    "ordinary": "ordinario",
    "occasionally": "a veces",
    "applied": "aplicado",
    "deference": "deferencia",
    "sometimes": "a veces",
    "superlative": "superlativo",
    "article": "artículo",
    "specifically": "específicamente",
    "dæmonic": "demoníaco",
    "delegate": "delegado",
    "ambassador": "embajador",
    "commissioner": "comisionado",
    "gospel": "evangelio",
    "supreme": "supremo",
    "controller": "quien tiene el control",
    "master": "maestro",
    "as": "como",
    "like": "como",
    "or": "o",
    "and": "y",
    "but": "pero",
    "yet": "aún",
    "if": "si",
    "when": "cuando",
    "where": "donde",
    "which": "el cual",
    "who": "quien",
    "whom": "quien",
    "whose": "cuyo",
    "what": "qué",
    "that": "que",
    "this": "este",
    "these": "estos",
    "those": "aquellos",
    "of": "de",
    "for": "para",
    "in": "en",
    "on": "en",
    "at": "en",
    "by": "por",
    "its": "su",
    "his": "su",
    "her": "su",
    "their": "su",
    "our": "nuestro",
    "your": "tu",
    "my": "mi",
    "one": "uno",
    "two": "dos",
    "three": "tres",
    "four": "cuatro",
    "five": "cinco",
    "six": "seis",
    "seven": "siete",
    "eight": "ocho",
    "nine": "nueve",
    "ten": "diez",
    "first": "primero",
    "second": "segundo",
    "third": "tercero",
    "fourth": "cuarto",
    "place": "lugar",
    "name": "nombre",
    "names": "nombres",
    "israelite": "israelita",
    "israelites": "israelitas",
    "christian": "cristiano",
    "christians": "cristianos",
    "palestine": "Palestina",
    "pal": "Palestina",
    "inhabitant": "habitante",
    "inhabitants": "habitantes",
    "region": "región",
    "district": "distrito",
    "city": "ciudad",
    "town": "poblado",
    "village": "aldea",
    "mountain": "monte",
    "river": "río",
    "sea": "mar",
    "desert": "desierto",
    "wilderness": "desierto",
    "east": "oriente",
    "west": "occidente",
    "north": "norte",
    "south": "sur",
    "son": "hijo",
    "sons": "hijos",
    "daughter": "hija",
    "daughters": "hijas",
    "father": "padre",
    "mother": "madre",
    "brother": "hermano",
    "sister": "hermana",
    "man": "hombre",
    "men": "hombres",
    "woman": "mujer",
    "women": "mujeres",
    "child": "niño",
    "children": "hijos",
    "people": "pueblo",
    "person": "persona",
    "persons": "personas",
    "king": "rey",
    "kings": "reyes",
    "priest": "sacerdote",
    "priests": "sacerdotes",
    "prophet": "profeta",
    "prophets": "profetas",
    "god": "Dios",
    "gods": "dioses",
    "lord": "Señor",
    "time": "tiempo",
    "times": "veces",
    "day": "día",
    "days": "días",
    "year": "año",
    "years": "años",
    "act": "acto",
    "action": "acción",
    "state": "estado",
    "quality": "cualidad",
    "condition": "condición",
    "thing": "cosa",
    "things": "cosas",
    "word": "palabra",
    "words": "palabras",
    "part": "parte",
    "parts": "partes",
    "kind": "clase",
    "sort": "clase",
    "form": "forma",
    "forms": "formas",
    "way": "modo",
    "manner": "manera",
    "means": "medio",
    "sense": "sentido",
    "idea": "idea",
    "title": "título",
    "office": "oficio",
    "official": "oficial",
    "honor": "honor",
    "honour": "honor",
    "adverb": "adverbio",
    "adjective": "adjetivo",
    "noun": "sustantivo",
    "verb": "verbo",
    "particle": "partícula",
    "preposition": "preposición",
    "pronoun": "pronombre",
    "conjunction": "conjunción",
    "interjection": "interjección",
    "plural": "plural",
    "singular": "singular",
    "masculine": "masculino",
    "feminine": "femenino",
    "neuter": "neutro",
    "intensive": "intensivo",
    "causative": "causativo",
    "passive": "pasivo",
    "active": "activo",
    "middle": "medio",
    "make": "hacer",
    "makes": "hace",
    "made": "hecho",
    "making": "haciendo",
    "being": "siendo",
    "having": "teniendo",
    "given": "dado",
    "called": "llamado",
    "named": "llamado",
    "known": "conocido",
    "used": "usado",
    "found": "encontrado",
    "taken": "tomado",
    "come": "venir",
    "go": "ir",
    "give": "dar",
    "take": "tomar",
    "put": "poner",
    "set": "poner",
    "see": "ver",
    "say": "decir",
    "speak": "hablar",
    "hear": "oír",
    "know": "conocer",
    "love": "amar",
    "live": "vivir",
    "die": "morir",
    "kill": "matar",
    "eat": "comer",
    "drink": "beber",
    "walk": "andar",
    "stand": "estar en pie",
    "sit": "sentarse",
    "rise": "levantarse",
    "fall": "caer",
    "send": "enviar",
    "bring": "traer",
    "bear": "llevar",
    "carry": "llevar",
    "keep": "guardar",
    "hold": "sostener",
    "have": "tener",
    "be": "ser",
    "become": "llegar a ser",
    "cause": "causar",
    "complete": "completar",
    "fully": "por completo",
    "entirely": "enteramente",
    "wholly": "del todo",
    "greatly": "en gran manera",
    "exceedingly": "en gran manera",
    "morally": "moralmente",
    "physically": "físicamente",
    "mentally": "mentalmente",
    "spiritually": "espiritualmente",
    "religiously": "religiosamente",
    "ceremonially": "ceremonialmente",
    "technically": "técnicamente",
    "officially": "oficialmente",
    "poetically": "poéticamente",
    "euphemistically": "eufemísticamente",
    "application": "aplicación",
    "applications": "aplicaciones",
    "extension": "extensión",
    "analogy": "analogía",
    "derivation": "derivación",
    "affinity": "afinidad",
    "origin": "origen",
    "root": "raíz",
    "base": "base",
    "stem": "tema",
    "compound": "compuesto",
    "contraction": "contracción",
    "reduplication": "reduplicación",
    "descendant": "descendiente",
    "descendants": "descendientes",
    "posterity": "posteridad",
    "patriarch": "patriarca",
    "tribe": "tribu",
    "nation": "nación",
    "nations": "naciones",
    "gentile": "gentil",
    "gentiles": "gentiles",
    "jew": "judío",
    "jews": "judíos",
    "hebrew": "hebreo",
    "greek": "griego",
    "chaldee": "caldeo",
    "aramaic": "arameo",
    "latin": "latín",
    "egyptian": "egipcio",
    "syrian": "sirio",
    "moabite": "moabita",
    "edomite": "edomita",
    "philistine": "filisteo",
    "canaanite": "cananeo",
    "babylon": "Babilonia",
    "assyria": "Asiria",
    "egypt": "Egipto",
    "syria": "Siria",
    "persia": "Persia",
    "rome": "Roma",
    "roman": "romano",
    "jerusalem": "Jerusalén",
    "israel": "Israel",
    "judah": "Judá",
    "samaria": "Samaria",
    "galilee": "Galilea",
    "etc": "etc.",
    "viz": "a saber",
    "fem": "femenino",
    "masc": "masculino",
    "neut": "neutro",
    "plur": "plural",
    "sing": "singular",
    "prob": "probablemente",
    "perh": "quizá",
    "appar": "al parecer",
    "lit": "literalmente",
    "fig": "figurativamente",
    "impl": "por implicación",
    "esp": "especialmente",
    "spec": "específicamente",
}

# Do not translate these tokens (Strong numbers, leftover abbreviations we keep)
KEEP = set()


def traducir(en: str) -> str:
    if not en:
        return ""
    s = html.unescape(en)
    s = s.replace("i.e.", " es decir, ")
    s = s.replace("i. e.", " es decir, ")
    s = s.replace("e.g.", " p. ej. ")
    s = s.replace("e. g.", " p. ej. ")
    s = s.replace("&quot;", '"')
    s = re.sub(r"^to ", "", s, flags=re.I)
    # Protect Strong numbers
    nums = {}

    def prot(m):
        k = f"§N{len(nums)}§"
        nums[k] = m.group(0)
        return k

    s = re.sub(r"\b[GH]\d+\b", prot, s)
    low = s
    for a, b in PHRASES:
        low = re.sub(re.escape(a), b, low, flags=re.I)
    # word by word, keep punctuation
    def wsub(m):
        w = m.group(0)
        key = w.lower().strip(".")
        if key in WORDS:
            t = WORDS[key]
            if not t:
                return ""
            if w[:1].isupper() and t[:1].islower():
                t = t[0].upper() + t[1:]
            return t
        return w

    low = re.sub(r"[A-Za-z']+", wsub, low)
    for k, v in nums.items():
        low = low.replace(k, v)
    low = re.sub(r"\b(an|the)\b", "", low, flags=re.I)
    low = re.sub(r"\ba (?!fin\b)([A-Za-zÁÉÍÓÚáéíóúÑñÜü]{3,})", r"\1", low)
    low = re.sub(r"\s+", " ", low)
    low = re.sub(r"\s+([,;:.!?])", r"\1", low)
    low = re.sub(r"\(\s+", "(", low)
    low = re.sub(r"\s+\)", ")", low)
    low = low.strip(" ;,")
    if low:
        low = low[0].upper() + low[1:]
    return low


STOP = {
    "de", "del", "la", "el", "los", "las", "un", "una", "y", "á", "a", "al",
    "oh", "tu", "su", "sus", "vuestro", "vuestra", "mi", "mis", "en", "con",
    "por", "para", "es", "era", "fue", "ha", "han", "se", "le", "les", "lo",
    "me", "te", "nos", "os", "como", "más", "mas", "que", "si", "ya", "no",
    "ni", "o", "u", "e", "yo", "tú", "él", "ella", "ello", "ellos", "ellas",
}


def core(phrase: str) -> str:
    w = phrase.strip()
    w = re.sub(r"^(de|del|á|a|al|el|la|los|las|un|una|oh|y|tu|su|vuestro|vuestra)\s+",
               "", w, flags=re.I)
    return w.strip() or phrase.strip()


def best_gloss(phrases: list[str]) -> str:
    if not phrases:
        return ""
    # Prefer distinctive 2-word particles like "para que"
    ctr = Counter(phrases)
    # Score: freq, prefer shorter content
    scored = []
    for p, n in ctr.items():
        c = core(p)
        words = [x for x in c.split() if x.lower() not in STOP]
        if not words and c.lower() not in {"para que", "a fin", "á fin"}:
            continue
        # "para que" is content
        bonus = 3 if c.lower() in {"para que", "a fin", "á fin", "por eso"} else 0
        scored.append((n + bonus, -len(c), c))
    if not scored:
        return core(ctr.most_common(1)[0][0])
    scored.sort(reverse=True)
    g = scored[0][2]
    # Prefer original capitalization of the winning core
    for p, _ in ctr.most_common():
        if core(p).lower() == g.lower():
            return core(p)
    return g


def main() -> None:
    glosses = {}
    if GLOSSES.exists():
        glosses = json.loads(GLOSSES.read_text(encoding="utf-8"))

    raw = SRC.read_text(encoding="utf-8")
    pat = re.compile(
        r'<s n="([^"]*)" l="([^"]*)" t="([^"]*)" g="([^"]*)" r="([^"]*)" d="([^"]*)"/>'
    )

    def repl(m):
        n, l, t, g, r, d = m.groups()
        letter, digits = n[0], n[1:].lstrip("0") or "0"
        key = f"{letter}{digits}"
        rv = glosses.get(key) or glosses.get(n) or []
        # clean rv phrases
        rv_clean = []
        seen = set()
        for p in rv:
            c = core(p)
            cl = c.lower()
            if not c or cl in seen:
                continue
            seen.add(cl)
            rv_clean.append(c)
        gloss = g.strip()
        if not gloss:
            gloss = best_gloss(rv_clean)
        d_es = traducir(d)
        if gloss:
            g0 = gloss[0].upper() + gloss[1:] if gloss else ""
            if d_es and g0.lower() not in d_es[:40].lower():
                d_es = f"{g0}. {d_es}"
            elif not d_es:
                d_es = g0
        if rv_clean:
            rend = "; ".join(rv_clean[:5])
            if d_es:
                if "Reina-Valera 1909" not in d_es:
                    d_es = f"{d_es.rstrip(' .')}. En Reina-Valera 1909: {rend}."
            else:
                d_es = f"En Reina-Valera 1909: {rend}."
        if not d_es and gloss:
            d_es = gloss
        def esc(x):
            return html.escape(x or "", quote=True)
        return (
            f'<s n="{esc(n)}" l="{esc(html.unescape(l))}" t="{esc(html.unescape(t))}" '
            f'g="{esc(gloss)}" r="{esc(r)}" d="{esc(d_es)}"/>'
        )

    out = pat.sub(repl, raw)
    out = out.replace(
        'src="openscriptures CC-BY-SA; Strong 1890 PD"',
        'src="Strong 1890 PD (español); glosas Reina-Valera 1909 PD (SpaRV1909 CrossWire); Open Scriptures"',
    )
    SRC.write_text(out, encoding="utf-8")
    # sanity
    m = re.search(r'n="G2443"[^>]*g="([^"]*)"[^>]*d="([^"]*)"', out)
    print("G2443 g=", m.group(1) if m else None)
    print("G2443 d=", m.group(2)[:180] if m else None)
    m = re.search(r'n="G2316"[^>]*g="([^"]*)"[^>]*d="([^"]*)"', out)
    print("G2316 g=", m.group(1) if m else None)
    print("G2316 d=", m.group(2)[:180] if m else None)
    m = re.search(r'n="H430"[^>]*g="([^"]*)"[^>]*d="([^"]*)"', out)
    print("H430 g=", m.group(1) if m else None)
    leftover = 0
    for mm in re.finditer(r' d="([^"]*)"', out):
        if re.search(r"\b(the|and|from|with|that|this|literally|figuratively)\b", mm.group(1), re.I):
            leftover += 1
    print("defs still with common English:", leftover)


if __name__ == "__main__":
    main()
