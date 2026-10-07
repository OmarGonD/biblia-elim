"""
Citas bíblicas de las notas de Torres Amat con el número romano leído mal:
«Levit. ATV, v. 2» es «Levit. XIV, v. 2»; «Malach, HIT, vu. 1» es «Malach.
III, v. 1»; «IL. Reg. IX» es «II. Reg. IX».

El OCR confunde letras de un número romano con otras de forma parecida (la X
con A, la I con T, E, L, 1, la II con H…). Para cada cita se prueban las
sustituciones posibles y solo se corrige cuando queda **un único** número
romano válido de coste mínimo y cabe en los capítulos del libro citado: así
«LX» se queda como está en Isaías (66 capítulos) y pasa a «IX» en Génesis (50).
El «v.» de versículo, que sale como vu., uv., vw, y, w, o, e…, se normaliza
cuando le sigue una cifra.
"""
import itertools
import re

from canon import CANON

CAPS = {L["nombre"]: L["caps"] for L in CANON}

ABREV = {
    "Gen": ["Genesis"], "Exod": ["Exodus"], "Exodi": ["Exodus"], "Levit": ["Leviticus"],
    "Lev": ["Leviticus"], "Num": ["Numbers"], "Deut": ["Deuteronomy"],
    "Deuter": ["Deuteronomy"], "Josue": ["Joshua"], "Jos": ["Joshua"],
    "Judic": ["Judges"], "Reg": ["I Samuel", "II Samuel", "I Kings", "II Kings"],
    "Paral": ["I Chronicles", "II Chronicles"], "Esdr": ["Ezra", "Nehemiah"],
    "Tob": ["Tobit"], "Job": ["Job"], "Ps": ["Psalms"], "Psalm": ["Psalms"],
    "Prov": ["Proverbs"], "Eccl": ["Ecclesiastes"], "Ecel": ["Ecclesiastes"],
    "Cant": ["Song of Solomon"], "Sap": ["Wisdom"], "Eccli": ["Sirach"],
    "Isai": ["Isaiah"], "Jerem": ["Jeremiah"], "Ezech": ["Ezekiel"], "Dan": ["Daniel"],
    "Os": ["Hosea"], "Joel": ["Joel"], "Amos": ["Amos"], "Mich": ["Micah"],
    "Hab": ["Habakkuk"], "Zach": ["Zechariah"], "Malach": ["Malachi"],
    "Mach": ["I Maccabees", "II Maccabees"], "Matth": ["Matthew"], "Marc": ["Mark"],
    "Luc": ["Luke"], "Joann": ["John"], "Joan": ["John"], "Act": ["Acts"],
    "Rom": ["Romans"], "Cor": ["I Corinthians", "II Corinthians"],
    "Gal": ["Galatians"], "Eph": ["Ephesians"], "Phil": ["Philippians"],
    "Col": ["Colossians"], "Thess": ["I Thessalonians", "II Thessalonians"],
    "Tim": ["I Timothy", "II Timothy"], "Tit": ["Titus"], "Hebr": ["Hebrews"],
    "Heb": ["Hebrews"], "Jac": ["James"], "Petr": ["I Peter", "II Peter"],
    "Apoc": ["Revelation of John"], "4ct": ["Acts"], "Aet": ["Acts"], "Lue": ["Luke"],
    "Cap": None,      # None: el libro de la nota
}
# Con numeral delante («II. Reg.», «1. Cor.»): el libro es uno de una familia.
CON_NUMERAL = {"Reg", "Paral", "Mach", "Cor", "Thess", "Tim", "Petr", "Esdr", "Joann", "Joan"}

# Letra leída -> letras romanas posibles. La primera opción es la más probable.
OPCIONES = {
    "A": "X", "Á": "X", "T": "I", "t": "I", "L": "IL", "l": "I", "1": "I", "i": "I",
    "I": "I", "E": "I", "e": "I", "H": "I", "h": "I", "Y": "V", "y": "V", "U": "V",
    "u": "V", "W": "V", "w": "V", "f": "I", "F": "I", "|": "I", "X": "X", "x": "X",
    "V": "V", "v": "V", "C": "C", "c": "C", "Í": "I", "í": "I",
}
RE_ROMANO = re.compile(r"^C?(?:XC|XL|L?X{0,3})(?:IX|IV|V?I{0,3})$")
VALORES = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100}


def valor(r):
    if not r or not RE_ROMANO.match(r):
        return None
    t = 0
    for a, b in zip(r, r[1:] + " "):
        v = VALORES[a]
        t += -v if VALORES.get(b, 0) > v else v
    return t


def repara_romano(token, maximo):
    """El número romano que el OCR quiso decir, o None si no hay uno claro."""
    if valor(token) is not None:
        return None          # ya es un número romano: no se toca
    if len(token) > 8 or not all(c in OPCIONES for c in token):
        return None
    if len(token) == 1 and token not in "IVXLC":
        return None          # una letra suelta: demasiado ambigua
    candidatos = {}
    for combo in itertools.product(*(OPCIONES[c] for c in token)):
        r = "".join(combo)
        v = valor(r)
        if v is None or not 1 <= v <= maximo:
            continue
        coste = sum(1 for a, b in zip(token, r) if a != b)
        candidatos.setdefault(r, coste)
    # «II» leído como «H» u «IL»: una letra puede ser dos
    if len(token) <= 4:
        for combo in itertools.product(*("I" + OPCIONES[c] if c in "Hh" else OPCIONES[c]
                                         for c in token)):
            r = "".join(combo)
            v = valor(r)
            if v is not None and 1 <= v <= maximo:
                candidatos.setdefault(r, 1 + sum(1 for a, b in zip(token, r) if a != b))
    if not candidatos:
        return None
    minimo = min(candidatos.values())
    mejores = [r for r, c in candidatos.items() if c == minimo]
    return mejores[0] if len(mejores) == 1 else None


def maximo_de(abreviatura, libro):
    libros = ABREV.get(abreviatura)
    if libros is None:
        return CAPS.get(libro, 150)
    return max(CAPS[l] for l in libros)


_AB = "|".join(sorted(ABREV, key=len, reverse=True))
RE_CITA = re.compile(
    r"(?P<pre>(?:[IVLTHEY1l]{1,3}\.\s+)?)(?P<ab>%s)(?P<pt>\.?),?\s+"
    r"(?P<rom>[^\s,.;:]{1,8})(?P<sep>[,.]?\s*)"
    r"(?P<vm>(?:vu|uv|vw|yv|v|y|w|o|e|u|0(?=\.))\.?\s*)(?=\d)" % _AB)
RE_CITA_V = re.compile(r"(?<=[A-Z]{1})(?:,v|,\s*v)(?=\.?\s*\d)")


def repara(texto, libro=None):
    """(texto con las citas corregidas, número de citas tocadas)."""
    n = 0

    def cambia(m):
        nonlocal n
        ab, rom = m.group("ab"), m.group("rom")
        nuevo = repara_romano(rom, maximo_de(ab, libro)) or rom
        pre = m.group("pre")
        if pre and ab in CON_NUMERAL:
            p = pre.strip(" .")
            q = repara_romano(p, 4) or p
            pre = f"{q}. "
        ab = {"4ct": "Act", "Aet": "Act", "Lue": "Luc"}.get(ab, ab)
        salida = f"{pre}{ab}{m.group('pt') or '.'} {nuevo}, v. "
        if salida != m.group(0):
            n += 1
        return salida

    texto = RE_CITA.sub(cambia, texto)
    # «V,v. 5»: el romano pegado a la coma
    texto2 = re.sub(r"\b(?P<r>[IVXL]{1,6}),v\.?\s*(?=\d)", r"\g<r>, v. ", texto)
    if texto2 != texto:
        n += 1
    return texto2, n
