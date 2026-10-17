"""Run the Windows entry script with real CMake and a simulated packager.

The fixture starts with Debug caches and a .git directory. Only the foreign
toolchain is omitted by the recorder; the repository's build-type rules run.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class WindowsReleaseBuildTest(unittest.TestCase):
    def run_architectures(self, options, expected):
        with tempfile.TemporaryDirectory(prefix="windows-release-") as tmp:
            root = Path(tmp)
            source = root / "source with spaces"
            (source / "win32").mkdir(parents=True)
            (source / ".git").mkdir()
            script = source / "win32/xc-xiphos-win.sh"
            shutil.copyfile(ROOT / "win32/xc-xiphos-win.sh", script)
            (source / "CMakeLists.txt").write_text(
                'cmake_minimum_required(VERSION 3.16)\n'
                'project(WindowsReleaseRegression NONE)\n'
                f'include("{ROOT / "cmake/XiphosFlags.cmake"}")\n'
                'if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")\n'
                '  message(FATAL_ERROR "Installer would ship a non-Release build")\n'
                'endif()\n')
            work = root / "work"
            work.mkdir()
            for arch in (32, 64):
                folder = work / f"win{arch}"
                folder.mkdir()
                (folder / "CMakeCache.txt").write_text("CMAKE_BUILD_TYPE:STRING=Debug\n")
            commands = root / "commands.jsonl"
            binaries = root / "bin"
            binaries.mkdir()
            cmake = binaries / "cmake"
            cmake.write_text('''#!/usr/bin/env python3
import json, os, subprocess, sys
from pathlib import Path
with open(os.environ['COMMAND_LOG'], 'a') as log:
    log.write(json.dumps({'cwd':str(Path.cwd()), 'args':sys.argv[1:]}) + '\\n')
args = [a for a in sys.argv[1:] if not a.startswith('-DCMAKE_TOOLCHAIN_FILE=')]
sys.exit(subprocess.run([os.environ['REAL_CMAKE'], *args]).returncode)
''')
            make = binaries / "make"
            make.write_text('''#!/usr/bin/env python3
from pathlib import Path
import sys
if 'package' in sys.argv:
    Path('biblia-elim-' + Path.cwd().name + '.exe').touch()
''')
            for executable in (cmake, make):
                executable.chmod(0o755)
            env = dict(os.environ, REAL_CMAKE=shutil.which("cmake"),
                       COMMAND_LOG=str(commands), PATH=str(binaries) + os.pathsep + os.environ['PATH'])
            result = subprocess.run(["bash", str(script), *options], cwd=work,
                                    env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            records = [json.loads(line) for line in commands.read_text().splitlines()]
            self.assertEqual([Path(r['cwd']).name for r in records], [f"win{a}" for a in expected])
            for arch, record in zip(expected, records):
                self.assertIn('-DCMAKE_BUILD_TYPE=Release', record['args'])
                self.assertIn(f'-DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw{arch}.cmake', record['args'])
                self.assertEqual(record['args'][-1], str(source))
                self.assertRegex((work / f'win{arch}/CMakeCache.txt').read_text(),
                                 r'(?m)^CMAKE_BUILD_TYPE:[^=\n]+=Release$')
                self.assertTrue((source / f'biblia-elim-win{arch}.exe').exists())

    def test_default_builds_both_release_installers(self):
        self.run_architectures([], (32, 64))

    def test_win32_release(self):
        self.run_architectures(['-win32'], (32,))

    def test_win64_release(self):
        self.run_architectures(['-win64'], (64,))


if __name__ == '__main__':
    unittest.main()
