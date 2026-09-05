#!/usr/bin/env python3
"""Reescribe el atributo d del léxico Strong con la traducción al español.

La traducción no se calcula aquí: se lee de un mapa ya revisado
(número Strong -> definición en español). Este script solo la coloca en el
XML, conservando la coletilla «En Reina-Valera 1909: …» y el encabezado con
la glosa, que es lo que src/main/glosa.c espera encontrar.
"""
from __future__ import annotations

import argparse
import html
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEXICON = ROOT / "ui" / "strongs-elim.xml"
ENTRY = re.compile(
    r'<s n="(?P<n>[^"]*)" l="(?P<l>[^"]*)" t="(?P<t>[^"]*)" '
    r'g="(?P<g>[^"]*)" r="(?P<r>[^"]*)" d="(?P<d>[^"]*)"/>')
TAIL = re.compile(r"\s*En Reina-Valera 1909:.*$", re.S)


def capitalizar(texto: str) -> str:
    """Mayúscula inicial sin tocar paréntesis ni números Strong."""
    for i, c in enumerate(texto):
        if c.isalpha():
            return texto[:i] + c.upper() + texto[i + 1:]
        if c not in "(«¡¿ ":
            break
    return texto


def nueva_definicion(glosa: str, cuerpo_viejo: str, traduccion: str) -> str:
    """Encabezado con la glosa (si ya lo tenía) + la definición traducida."""
    encabezado = ""
    if glosa:
        n = len(glosa)
        if (cuerpo_viejo[:n].lower() == glosa.lower()
                and cuerpo_viejo[n:n + 1] == "."):
            encabezado = capitalizar(glosa) + ". "
    return encabezado + capitalizar(traduccion.strip())


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("traducciones", nargs="+", type=Path,
                    help="uno o más JSON {strong: definición en español}")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    trad: dict[str, str] = {}
    for ruta in args.traducciones:
        trad.update(json.loads(ruta.read_text(encoding="utf-8")))

    texto = LEXICON.read_text(encoding="utf-8")
    puestas = intactas = 0

    def una(m: re.Match[str]) -> str:
        nonlocal puestas, intactas
        n = m.group("n")
        nueva = trad.get(n)
        if nueva is None:
            intactas += 1
            return m.group(0)
        d = html.unescape(m.group("d"))
        cola = TAIL.search(d)
        cuerpo = TAIL.sub("", d).strip()
        cuerpo = nueva_definicion(html.unescape(m.group("g")), cuerpo, nueva)
        if cola:
            cuerpo += cola.group(0).rstrip()
        puestas += 1
        return (f'<s n="{n}" l="{m.group("l")}" t="{m.group("t")}" '
                f'g="{m.group("g")}" r="{m.group("r")}" '
                f'd="{html.escape(cuerpo, quote=True)}"/>')

    salida = ENTRY.sub(una, texto)
    print(f"definiciones traducidas: {puestas}; sin tocar: {intactas}")
    if args.dry_run:
        return
    shutil.copy2(LEXICON, LEXICON.with_suffix(".xml.bak"))
    LEXICON.write_text(salida, encoding="utf-8")
    print(f"escrito {LEXICON}")


if __name__ == "__main__":
    main()
