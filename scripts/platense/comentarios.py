#!/usr/bin/env python3
"""Extrae las notas OSIS de SpaPlatense a un comentario SWORD separado."""
import html, os, re, subprocess

DIR = os.path.dirname(os.path.abspath(__file__))
DEST = os.path.join(DIR, "platense-comentarios.imp")
ENTRY = re.compile(r"^\$\$\$(.+)$", re.M)
NOTE = re.compile(r"<note\b[^>]*>(.*?)</note>", re.S)

def limpia(s):
    s = re.sub(r"<hi\b[^>]*type=[\"']italic[\"'][^>]*>", "<i>", s)
    s = s.replace("</hi>", "</i>")
    s = re.sub(r"<reference\b[^>]*>", "<b>", s).replace("</reference>", "</b>")
    s = re.sub(r"<(?!/?(?:i|b)\b)[^>]+>", "", s)
    return html.unescape(s).strip()

def main():
    raw = subprocess.check_output(["mod2imp", "SpaPlatense"], text=True,
                                  stderr=subprocess.DEVNULL)
    marks, out, count = list(ENTRY.finditer(raw)), [], 0
    for i, mark in enumerate(marks):
        key = mark.group(1).strip()
        if not re.search(r"\d+:\d+$", key): continue
        end = marks[i + 1].start() if i + 1 < len(marks) else len(raw)
        notes = [limpia(x) for x in NOTE.findall(raw[mark.end():end])]
        notes = [x for x in notes if x]
        if notes:
            out.extend((f"$$${key}", "<br/><br/>".join(notes))); count += 1
    with open(DEST, "w", encoding="utf-8") as stream:
        stream.write("\n".join(out) + "\n")
    print(f"{count} versículos con comentarios: {DEST}")

if __name__ == "__main__": main()
