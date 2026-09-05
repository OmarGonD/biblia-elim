#!/usr/bin/env python3
"""Aplica únicamente sustituciones revisadas al atributo d del léxico Strong.

El glosario es deliberadamente conservador: lo que no esté en él no se toca.
--dry-run no escribe el XML y deja un informe de revisión manual.
"""
from __future__ import annotations

import argparse
import html
import json
import re
import shutil
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEXICON = ROOT / "ui" / "strongs-elim.xml"
GLOSSARY = ROOT / "scripts" / "glosario-en-es.json"
MANUAL = ROOT / "scripts" / "revision-manual-strongs.tsv"
ENTRY = re.compile(r'<s n="(?P<n>[^"]*)"[^>]*? d="(?P<d>[^"]*)"/>')
WORD = re.compile(r"[^\W\d_]+(?:'[^\W\d_]+)?", re.UNICODE)
# Falsos positivos del diccionario de contraseñas inglés que son español en
# estas definiciones. No son candidatos de traducción ni de revisión.
SPANISH_WORDS = set("""
a al algo ambos animal aparte asia asiria baal base beth bien bajo caer cananeo
capital caso causar causativo ceremonialmente ciudad clase co colectivamente color
comer completo con concreto contra cosa cosas cristiano cual cuando cuatro dado dar
de del delante descendiente descendientes desierto diez dios divina donde dos e
egipcio egipto el en encima entonces era es especialmente estar este etc femenino
figurativo figurativamente forma fortaleza fue fueron fuera general gran guardar ha
habitante habitantes habitualmente hacer hablar hebreo hecho hijo hijos hombre hombres
ir ira israelita israelitas israelitass jacob jordan la las le lejos literal literalmente
llevar lo lugar luz maestro manera mas masculino me mediante medio menudo mi mismo
moral moralmente mujer muerte muy nacional no nombre neutro ni nueve o objeto oficial
oriente os otra otro otros palabra palabras palestina parecer parte pasivo patriarca
persona personas pie plural poco poner por primero propiamente profeta publico que
religiosa rey romano sacerdote se segundo sentido ser si siendo sin singular sirio sobre
solo son sostener su sus sustantivo tal tener tercero tiempo todo tomar tres tribu tu tus
un una uno usado varias veces venir verdad vivir voz y ya
""".split())


def load_english_words() -> set[str]:
    """Usa el léxico inglés local solo para clasificar, nunca para traducir."""
    result = subprocess.run(
        ("cracklib-unpacker", "/usr/share/cracklib/pw_dict"),
        check=True, text=True, capture_output=True,
    )
    return set(result.stdout.splitlines())


def replace_words(text: str, glossary: dict[str, str]) -> tuple[str, list[str]]:
    """Sustituye palabras completas, preservando mayúscula inicial y acentos."""
    changed: list[str] = []

    def one(match: re.Match[str]) -> str:
        word = match.group(0)
        replacement = glossary.get(word.lower())
        # Entradas técnicas como "mental" son intencionalmente iguales en
        # ambos idiomas; tratarlas como cambio impediría que el proceso fuese
        # idempotente.
        if replacement is None or replacement.casefold() == word.casefold():
            return word
        changed.append(word.lower())
        if word[:1].isupper() and replacement[:1].islower():
            return replacement[:1].upper() + replacement[1:]
        return replacement

    return WORD.sub(one, text), changed


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true", help="escribe solo ui/strongs-elim.xml")
    args = parser.parse_args()
    glossary = json.loads(GLOSSARY.read_text(encoding="utf-8"))
    glossary.pop("_nota", None)
    english_words = load_english_words()
    raw = LEXICON.read_text(encoding="utf-8")
    changed_entries: dict[str, list[str]] = {}
    unmapped: Counter[str] = Counter()
    contexts: defaultdict[str, list[tuple[str, str]]] = defaultdict(list)

    def repl(match: re.Match[str]) -> str:
        definition = html.unescape(match.group("d"))
        # Las citas RV1909 ya están en español y no se analizan ni modifican.
        body, marker, rv = definition.partition(" En Reina-Valera 1909:")
        translated, hits = replace_words(body, glossary)
        if hits:
            changed_entries[match.group("n")] = hits
        # Solo se lista para revisión lo que luce como token inglés ASCII no cubierto.
        for word in WORD.findall(body):
            key = word.lower()
            if (key in english_words and key not in glossary and key not in SPANISH_WORDS
                    and len(key) > 1):
                unmapped[key] += 1
                if len(contexts[key]) < 3:
                    contexts[key].append((match.group("n"), body[:260]))
        escaped = html.escape(translated + marker + rv, quote=True)
        return match.group(0)[:match.start("d") - match.start(0)] + escaped + '"/>'

    output = ENTRY.sub(repl, raw)
    manual_lines = ["token\tapariciones\tejemplos (Strong: definición)\n"]
    for token, count in unmapped.most_common():
        samples = " || ".join(f"{n}: {d}" for n, d in contexts[token])
        manual_lines.append(f"{token}\t{count}\t{samples}\n")
    MANUAL.write_text("".join(manual_lines), encoding="utf-8")
    print(f"entradas que cambiarían: {len(changed_entries)}")
    print(f"sustituciones seguras: {sum(map(len, changed_entries.values()))}")
    print(f"tokens sin mapear para revisión: {len(unmapped)}")
    print(f"informe: {MANUAL}")
    if args.apply:
        backup = LEXICON.with_name("strongs-elim.xml.pre-restos-es.bak")
        if not backup.exists():
            shutil.copy2(LEXICON, backup)
        LEXICON.write_text(output, encoding="utf-8")
        print(f"aplicado: {LEXICON}")
        print(f"respaldo: {backup}")
    else:
        print("modo seco: el XML no se modificó")


if __name__ == "__main__":
    main()
