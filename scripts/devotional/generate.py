#!/usr/bin/env python3
"""Regenerate all read-only SWORD modules using the SWORD RawLD writer."""
import argparse
import calendar
import html
from pathlib import Path
import subprocess
import tempfile
import json
CONTENT = json.loads(Path(__file__).with_name("content.json").read_text(encoding="utf-8"))
LANGUAGES, READINGS = CONTENT["languages"], CONTENT["readings"]

BOOKS = ['Matthew', 'Psalm', 'James', 'Philippians', 'Micah', 'Romans', 'John',
 'Ephesians', 'Colossians', 'Proverbs', 'Luke', '1 Thessalonians', 'Galatians',
 'Hebrews', '2 Corinthians', 'Isaiah', '1 Peter', 'Lamentations']
NAMES = {
 'es': 'Mateo|Salmos|Santiago|Filipenses|Miqueas|Romanos|Juan|Efesios|Colosenses|Proverbios|Lucas|1 Tesalonicenses|Gálatas|Hebreos|2 Corintios|Isaías|1 Pedro|Lamentaciones',
 'fr': 'Matthieu|Psaumes|Jacques|Philippiens|Michée|Romains|Jean|Éphésiens|Colossiens|Proverbes|Luc|1 Thessaloniciens|Galates|Hébreux|2 Corinthiens|Ésaïe|1 Pierre|Lamentations',
 'pt': 'Mateus|Salmos|Tiago|Filipenses|Miqueias|Romanos|João|Efésios|Colossenses|Provérbios|Lucas|1 Tessalonicenses|Gálatas|Hebreus|2 Coríntios|Isaías|1 Pedro|Lamentações',
 'ko_KR': '마태복음|시편|야고보서|빌립보서|미가|로마서|요한복음|에베소서|골로새서|잠언|누가복음|데살로니가전서|갈라디아서|히브리서|고린도후서|이사야|베드로전서|예레미야애가',
 'zh_CN': '马太福音|诗篇|雅各书|腓立比书|弥迦书|罗马书|约翰福音|以弗所书|歌罗西书|箴言|路加福音|帖撒罗尼迦前书|加拉太书|希伯来书|哥林多后书|以赛亚书|彼得前书|耶利米哀歌',
 'zh_TW': '馬太福音|詩篇|雅各書|腓立比書|彌迦書|羅馬書|約翰福音|以弗所書|歌羅西書|箴言|路加福音|帖撒羅尼迦前書|加拉太書|希伯來書|哥林多後書|以賽亞書|彼得前書|耶利米哀歌',
}


def entries(language):
    data = LANGUAGES[language]
    themes = data['themes']
    actions = data['actions']
    assert len(themes) == 12 and len(actions) == 31
    names = NAMES.get('pt' if language == 'pt_BR' else language)
    mapping = dict(zip(BOOKS, names.split('|'))) if names else dict(zip(BOOKS, BOOKS))
    for month in range(1, 13):
        title, reflection, prayer = themes[month - 1].split('|')
        for day in range(1, calendar.monthrange(2024, month)[1] + 1):
            reading = READINGS[day - 1]
            book, reference = reading.rsplit(' ', 1)
            reading = mapping[book] + ' ' + reference
            text = (f'<h3>{html.escape(title)}</h3>'
                    f'<p><b>{data["read"]}:</b> {html.escape(reading)}</p>'
                    f'<p>{html.escape(reflection)}</p>'
                    f'<p><b>{data["practice"]}:</b> {html.escape(actions[day - 1])}</p>'
                    f'<p><b>{data["prayer"]}:</b> {html.escape(prayer)}</p>')
            yield f'{month:02}.{day:02}', text


def generate(writer, destination):
    for language, data in LANGUAGES.items():
        root = destination / language
        configs = root / 'mods.d'
        configs.mkdir(parents=True, exist_ok=True)
        basename = data['id'].lower()
        relative = f'modules/devotionals/rawld/{basename}/{basename}'
        path = root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory() as tmp:
            tsv = Path(tmp) / 'entries.tsv'
            tsv.write_text(''.join(f'{key}\t{text}\n' for key, text in entries(language)), encoding='utf-8')
            subprocess.run([str(writer), str(tsv), str(path)], check=True)
        for suffix in (".dat", ".idx"):
            Path(str(path) + suffix).chmod(0o644)
        (configs / f'{basename}.conf').write_text(
            f'[{data["id"]}]\nDataPath=./{relative}\nModDrv=RawLD\n'
            f'Description={data["name"]}\nLang={data["lang"]}\nEncoding=UTF-8\n'
            'SourceType=ThML\nFeature=DailyDevotion\nCategory=Daily Devotional\n'
            'Version=1.0\nMinimumVersion=1.5.9\n'
            'Copyright=Original Elim meditations; GPL-2.0-or-later\n'
            'DistributionLicense=GPL\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--writer', required=True, type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[2] / 'modulos/devotional')
    args = parser.parse_args()
    generate(args.writer.resolve(), args.output)
