"""Check shipped catalogs, real gettext startup, and GtkBuilder in six languages."""

import gettext
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from i18n_catalogs import read_po

build = Path(sys.argv[1]).resolve()
probe = Path(sys.argv[2]).resolve()
builder = Path(sys.argv[3]).resolve()
expected = {
    "en_GB": ("Bible study", "Install Bibles"),
    "es": ("Estudio bíblico", "Instalar Biblias"),
    "fr": ("Étude biblique", "Installer des Bibles"),
    "ko_KR": ("성경 연구", "성경 설치"),
    "zh_CN": ("圣经研读", "安装圣经"),
    "pt_BR": ("Estudo bíblico", "Instalar Bíblias"),
}
entries = [e for e in read_po(ROOT / "po/xiphos.pot") if e["msgid"]]
env = dict(os.environ, LC_ALL="C", LANG="C", LANGUAGE="",
           BIBLIA_ELIM_LOCALE_DIR=str(build / "locale"))
checked = 0
for language, labels in expected.items():
    catalog = gettext.translation("xiphos", build / "locale", [language])
    assert catalog.info()["language"] == language
    for entry in entries:
        key = (entry.get("msgctxt", "") + "\x04" if entry.get("msgctxt") else "") + entry["msgid"]
        if "msgid_plural" in entry:
            for n in (0, 1, 2, 7):
                assert (key, catalog.plural(n)) in catalog._catalog, (language, key, n)
                value = catalog.ngettext(key, entry["msgid_plural"], n)
                assert value and "\ufffd" not in value, (language, key)
        else:
            assert key in catalog._catalog, (language, key)
            assert catalog.gettext(key), (language, key)
        checked += 1
    # Check format contracts on the actual source catalogs, not just examples.
    with tempfile.TemporaryDirectory(prefix="elim-msgfmt-") as directory:
        subprocess.run(["msgfmt", "--check", "--check-format", "-o",
                        str(Path(directory) / "checked.mo"), str(ROOT / f"po/{language}.po")], check=True)
    output = subprocess.check_output([str(probe), "--probe", language], env=env, text=True).splitlines()
    assert output == [language, *labels], (language, output)
    output = subprocess.check_output([str(builder), language], env=env, text=True).splitlines()
    assert output == list(labels), (language, "GtkBuilder", output)

# Persisted choice wins over the environment; the CLI wins over that choice.
with tempfile.TemporaryDirectory(prefix="elim-language-precedence-") as directory:
    path = Path(directory) / "settings.xml"
    path.write_text("<Xiphos><locale><special>ko_KR.UTF-8</special></locale>"
                    "<keys><verse>Juan 3:16</verse></keys></Xiphos>")
    before = path.read_bytes()
    output = subprocess.check_output([str(probe), "--probe", "saved", str(path)], env=env, text=True)
    assert output.splitlines() == ["ko_KR", *expected["ko_KR"]]
    output = subprocess.check_output([str(probe), "--probe", "fr", str(path)], env=env, text=True)
    assert output.splitlines() == ["fr", *expected["fr"]]
    assert path.read_bytes() == before, "startup must not rewrite preferences or references"
    path.write_text("<Xiphos><locale><special>None</special></locale></Xiphos>")
    for variable, selection, language in (("LANGUAGE", "ko", "ko_KR"),
                                           ("LANGUAGE", "en", "en_GB"),
                                           ("LANGUAGE", "fr:en", "fr"),
                                           ("LANGUAGE", "", "en_GB")):
        system_env = dict(env, **{variable: selection})
        output = subprocess.check_output([str(probe), "--probe", "saved", str(path)],
                                         env=system_env, text=True).splitlines()
        assert output == [language, *expected[language]], (selection, output)

# Every translatable GtkBuilder label is present in the catalogs actually built.
resource_labels = 0
for path in (ROOT / "ui").glob("*.gtkbuilder"):
    for node in ET.parse(path).iter():
        if node.get("translatable") == "yes" and node.text:
            key = node.text
            for language in expected:
                catalog = gettext.translation("xiphos", build / "locale", [language])
                assert key in catalog._catalog, (path.name, language, key)
            resource_labels += 1
print(f"multilingual_catalog_test: PASS languages=6 entries={checked} "
      f"GtkBuilder_labels={resource_labels} precedence=PASS UTF-8=PASS")
