#!/usr/bin/env python3
"""Extract the current UI and apply reviewed translations using GNU gettext.

Only the standard library and gettext command-line tools are needed. Bible and
study data are never extracted: this works on source strings and GtkBuilder UI.
"""

import argparse
import ast
import json
import re
import subprocess
import tempfile
import unicodedata
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ("en_GB", "es", "fr", "ko_KR", "zh_CN", "pt_BR")
PLURALS = {
    "en_GB": "nplurals=2; plural=(n != 1);",
    "es": "nplurals=2; plural=(n != 1);",
    "fr": "nplurals=2; plural=(n > 1);",
    "ko_KR": "nplurals=1; plural=0;",
    "zh_CN": "nplurals=1; plural=0;",
    "pt_BR": "nplurals=2; plural=(n > 1);",
}


def read_po(path):
    entries, entry, key = [], {}, None
    for line in Path(path).read_text(encoding="utf-8").splitlines() + [""]:
        if not line:
            if "msgid" in entry:
                entries.append(entry)
            entry, key = {}, None
        elif line.startswith("#, "):
            entry["flags"] = line[3:].split(", ")
        elif line.startswith("#") and not line.startswith("#~"):
            entry.setdefault("comments", []).append(line)
        elif re.match(r"(?:msgid|msgid_plural|msgctxt|msgstr(?:\[\d+\])?) ", line):
            key, literal = line.split(" ", 1)
            entry[key] = ast.literal_eval(literal)
        elif line.startswith('"') and key:
            entry[key] += ast.literal_eval(line)
    return entries


def quote(value):
    return json.dumps(value, ensure_ascii=False)


def extract(output):
    with tempfile.TemporaryDirectory(prefix="elim-i18n-") as tmp:
        tmp = Path(tmp)
        files = sorted(str(p.relative_to(ROOT)) for p in (ROOT / "src").rglob("*")
                       if p.suffix in (".c", ".cc", ".cpp", ".h", ".hh")
                       and p.name not in ("gtk_lifecycle_smoke.c", "author_commentary_probe.c"))
        (tmp / "files").write_text("\n".join(files) + "\n")
        subprocess.run(["xgettext", "--language=C++", "--from-code=UTF-8",
                        "--keyword=_", "--keyword=N_", "--keyword=ngettext:1,2",
                        "--keyword=C_:1c,2", "--add-comments=TRANSLATORS",
                        "--package-name=Biblia Elim", "--package-version=1",
                        "--files-from=" + str(tmp / "files"),
                        "--output=" + str(tmp / "code.pot")], cwd=ROOT, check=True)
        subprocess.run(["xgettext", "--language=Glade", "--from-code=UTF-8",
                        "--package-name=Biblia Elim", "--package-version=1",
                        "--output=" + str(tmp / "ui.pot"),
                        *sorted(str(p.relative_to(ROOT)) for p in (ROOT / "ui").glob("*.gtkbuilder"))],
                       cwd=ROOT, check=True)
        subprocess.run(["msgcat", "--use-first", str(tmp / "code.pot"),
                        str(tmp / "ui.pot"), "-o", str(output)], check=True)


def apply_translations(table):
    # TSV columns: source, English, French, Korean, Chinese, Portuguese,
    # and optionally Spanish when the source string is English.
    # Literal \n is a newline; actual tab/newline delimit records.
    translations = {}
    for number, line in enumerate(Path(table).read_text().splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = [s.replace(r"\n", "\n") for s in line.split("\t")]
        if len(fields) not in (6, 7):
            raise ValueError(f"{table}:{number}: expected 6 or 7 columns, got {len(fields)}")
        source, en, fr, ko, zh, pt = fields[:6]
        es = fields[6] if len(fields) == 7 else source
        translations[source] = dict(zip(LANGUAGES, (en, es, fr, ko, zh, pt)))
    templates = read_po(ROOT / "po/xiphos.pot")
    original = {lang: {e["msgid"]: e for e in read_po(ROOT / f"po/{lang}.po")}
                for lang in (*LANGUAGES, "pt")}
    inverse = {e.get("msgstr"): e["msgid"] for e in original["es"].values()
               if e.get("msgstr") and "fuzzy" not in e.get("flags", ())}
    def normalized(s):
        s = re.sub(r"<[^>]*>", "", s.replace("Biblia Elim", "Xiphos"))
        s = unicodedata.normalize("NFKD", s)
        return " ".join(re.sub(r"[^\w%]+", " ", s.replace("_", "")).lower().split())
    aliases = {}
    for entry in original["es"].values():
        aliases[normalized(entry["msgid"])] = entry["msgid"]
        if entry.get("msgstr") and "fuzzy" not in entry.get("flags", ()):
            aliases[normalized(entry["msgstr"])] = entry["msgid"]
    def compatible(source, value):
        placeholders = r"%(?!%)(?:\d+\$)?[-+#0 ']*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|[hljztL])?[a-zA-Z]"
        return (Counter(re.findall(placeholders, source)) == Counter(re.findall(placeholders, value))
                and source.startswith("\n") == value.startswith("\n")
                and source.endswith("\n") == value.endswith("\n")
                and Counter(re.findall(r"</?\w+[^>]*>", source)) == Counter(re.findall(r"</?\w+[^>]*>", value)))
    def derived(part, lang):
        # Reuse reviewed labels for mnemonic/ellipsis and markup variants.
        leading = part[:len(part) - len(part.lstrip())]
        trailing = part[len(part.rstrip()):]
        plain = part.strip()
        bullet = "*&nbsp;" if plain.startswith("*&nbsp;") else ""
        if bullet:
            plain = plain[len(bullet):]
        break_tag = "</br>" if plain.endswith("</br>") else ""
        if break_tag:
            plain = plain[:-len(break_tag)]
        markup = re.fullmatch(r"(<(?:b|i|small)>)(.*)(</(?:b|i|small)>)", plain, re.S)
        plain = markup[2] if markup else plain
        mnemonic = "_" in plain
        plain = plain.replace("_", "")
        ellipsis = plain.endswith("…")
        if ellipsis:
            plain = plain[:-1]
        values = translations.get(plain)
        if not values:
            return None
        value = values[lang]
        if mnemonic and "_" not in value:
            if lang in ("ko_KR", "zh_CN"):
                key = re.search(r"[A-Za-z]", values["en_GB"])
                value += f"(_{key[0].upper()})" if key else ""
            else:
                value = "_" + value
        if ellipsis:
            value += "…"
        if markup:
            value = markup[1] + value + markup[3]
        return leading + bullet + value + break_tag + trailing
    with tempfile.TemporaryDirectory(prefix="elim-translations-") as tmp:
        for lang in LANGUAGES:
            inherited = original[lang].get("", {})
            fields = dict(line.split(": ", 1) for line in inherited.get("msgstr", "").splitlines()
                          if ": " in line)
            fields.update({"Project-Id-Version": "Biblia Elim 1", "Language": lang,
                           "MIME-Version": "1.0", "Content-Type": "text/plain; charset=UTF-8",
                           "Content-Transfer-Encoding": "8bit", "Plural-Forms": PLURALS[lang]})
            header = "".join(f"{key}: {value}\n" for key, value in fields.items())
            credits = "\n".join(inherited.get("comments", ()))
            blocks = [(credits + "\n" if credits else "") + 'msgid ""\nmsgstr ' + quote(header) + "\n"]
            for entry in templates:
                source = entry["msgid"]
                if not source:
                    continue
                parts = [source]
                if entry.get("msgid_plural"):
                    parts.append(entry["msgid_plural"])
                values = []
                for part in parts:
                    value = translations.get(part, {}).get(lang)
                    if value is None:
                        value = derived(part, lang)
                    if value is None and part in inverse:
                        candidate = original[lang].get(inverse[part], {})
                        if "fuzzy" not in candidate.get("flags", ()):
                            value = candidate.get("msgstr")
                    if not value and normalized(part) in aliases:
                        candidate = original[lang].get(aliases[normalized(part)], {})
                        if "fuzzy" not in candidate.get("flags", ()):
                            value = candidate.get("msgstr")
                            if value:
                                if "Biblia Elim" in part:
                                    value = value.replace("Xiphos", "Biblia Elim")
                                if not compatible(part, value):
                                    value = None
                    if not value and lang == "pt_BR":
                        candidate = original["pt"].get(part, {})
                        if "fuzzy" not in candidate.get("flags", ()):
                            value = candidate.get("msgstr")
                    if not value and not re.search(r"[^%0-9\W_]+", part):
                        value = part
                    values.append(value)
                if not all(values[:1] if lang in ("ko_KR", "zh_CN") else values):
                    continue
                block = []
                if entry.get("msgctxt"):
                    block.append("msgctxt " + quote(entry["msgctxt"]))
                if "c-format" in entry.get("flags", ()):
                    block.insert(0, "#, c-format")
                block.append("msgid " + quote(source))
                if len(parts) == 2:
                    block.append("msgid_plural " + quote(parts[1]))
                    for index in range(1 if lang in ("ko_KR", "zh_CN") else 2):
                        block.append(f"msgstr[{index}] " + quote(values[index]))
                else:
                    block.append("msgstr " + quote(values[0]))
                blocks.append("\n".join(block) + "\n")
            supplemental = Path(tmp) / f"{lang}.po"
            supplemental.write_text("\n".join(blocks), encoding="utf-8")
            merged = Path(tmp) / f"merged-{lang}.po"
            subprocess.run(["msgcat", "--use-first", str(supplemental),
                            str(ROOT / f"po/{lang}.po"), "-o", str(merged)], check=True)
            subprocess.run(["msgfmt", "--check-format", "-o", str(Path(tmp) / "check.mo"),
                            str(merged)], check=True)
            (ROOT / f"po/{lang}.po").write_text(merged.read_text(), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--extract", type=Path)
    parser.add_argument("--apply", type=Path)
    args = parser.parse_args()
    if args.extract:
        extract(args.extract.resolve())
    if args.apply:
        apply_translations(args.apply)


if __name__ == "__main__":
    main()
