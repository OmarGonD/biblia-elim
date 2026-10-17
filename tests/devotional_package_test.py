"""Regeneration and a relocated offline installation, including language defaults."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

source, build, writer, probe = map(lambda p: Path(p).resolve(), sys.argv[1:])
spec = importlib.util.spec_from_file_location('devotional_generate', source / 'scripts/devotional/generate.py')
generate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate)
with tempfile.TemporaryDirectory(prefix='elim-devotional-package-') as tmp:
    temp = Path(tmp)
    regenerated = temp / 'generated'
    generate.generate(writer, regenerated)
    original = source / 'modulos/devotional'
    files = [p.relative_to(original) for p in original.rglob('*') if p.is_file()]
    assert sorted(files) == sorted(p.relative_to(regenerated) for p in regenerated.rglob('*') if p.is_file())
    for relative in files:
        assert (original / relative).read_bytes() == (regenerated / relative).read_bytes(), relative
    prefix = temp / 'installed'
    subprocess.run(['cmake', '--install', str(build), '--prefix', str(prefix),
                    '--component', 'Devotional'], check=True, stdout=subprocess.DEVNULL)
    asset = prefix / 'share/biblia-elim/devotional'
    assert asset.is_dir()
    for relative in files:
        assert (asset / relative).read_bytes() == (original / relative).read_bytes()
    binary = prefix / 'bin/devotional-probe'
    binary.parent.mkdir(parents=True)
    shutil.copy2(probe, binary)
    env = os.environ.copy()
    for name in ('BIBLIA_ELIM_BUNDLED_DEVOTIONAL', 'BIBLIA_ELIM_INSTALL_LANGUAGE_FILE'):
        env.pop(name, None)
    # Keep the process offline and independent of real profile preferences.
    env.update(LANG='C', LC_ALL='C', LANGUAGE='en_GB')
    for language in generate.LANGUAGES:
        (prefix / 'share/biblia-elim/installation-language').write_text(language + '\n')
        subprocess.run([str(binary), '--probe-stage', language], env=env, check=True)
    settings = temp / 'settings.xml'
    settings.write_text('<Xiphos><locale><special>es</special></locale></Xiphos>')
    (prefix / 'share/biblia-elim/installation-language').write_text('fr\n')
    subprocess.run([str(binary), '--probe-stage', 'es', str(settings)], env=env, check=True)
    subprocess.run([str(binary), '--probe-stage', 'fr', str(settings), 'fr'], env=env, check=True)
    # Missing or malformed installer defaults fall back to the system language.
    (prefix / 'share/biblia-elim/installation-language').write_text('not-a-language\n')
    subprocess.run([str(binary), '--probe-stage', 'en_GB'], env=env, check=True)
    (prefix / 'share/biblia-elim/installation-language').write_text('fr\n')
    # Explicit system preference wins over the installer's French choice.
    settings.write_text('<Xiphos><locale><special>None</special></locale></Xiphos>')
    subprocess.run([str(binary), '--probe-stage', 'en_GB', str(settings)], env=env, check=True)
    # Exercise the local installer itself without writing a real user profile.
    local = temp / 'local'
    fake = temp / 'fake-tools'
    fake.mkdir()
    for command in ('xdg-desktop-menu', 'update-desktop-database', 'gtk-update-icon-cache'):
        tool = fake / command
        tool.write_text('#!/bin/sh\nexit 0\n')
        tool.chmod(0o755)
    local_env = env | {
        'BIBLIA_ELIM_INSTALL_PREFIX': str(local),
        'BIBLIA_ELIM_SWORD_DIRECTORY': str(temp / 'sword'),
        'XDG_DATA_HOME': str(temp / 'user-data'),
        'PATH': str(fake) + os.pathsep + env['PATH'],
    }
    subprocess.run(['bash', str(source / 'scripts/install-biblia-elim.sh'), '--language=ko_KR'],
                   env=local_env, check=True, stdout=subprocess.DEVNULL)
    assert (local / 'share/biblia-elim/installation-language').read_text().strip() == 'ko_KR'
    for relative in files:
        assert (local / 'share/biblia-elim/devotional' / relative).read_bytes() == (original / relative).read_bytes()
    shutil.copy2(probe, local / 'bin/devotional-probe')
    subprocess.run([str(local / 'bin/devotional-probe'), '--probe-stage', 'ko_KR'], env=env, check=True)
print('devotional_package_test: PASS (reproducible assets; 8 offline install languages; saved/system precedence)')
