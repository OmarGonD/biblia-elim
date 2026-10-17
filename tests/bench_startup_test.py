#!/usr/bin/env python3
"""Parsing regressions for the real-startup benchmark (no display needed)."""
import importlib.util
from pathlib import Path
import unittest
from unittest import mock
import json
import tempfile
import os

spec = importlib.util.spec_from_file_location('bench_startup', Path(__file__).parents[1] / 'tools/bench_startup.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)
compare_spec = importlib.util.spec_from_file_location(
    'compare_startup', Path(__file__).parents[1] / 'tools/compare_startup.py')
compare = importlib.util.module_from_spec(compare_spec)
compare_spec.loader.exec_module(compare)


class MilestonesTest(unittest.TestCase):
    def test_locale_and_first_draw(self):
        text = '''[UI-LOAD] app FIRST_CONTENT_READY 800,2ms display_ms=10,0
[UI-LOAD] app GTK_INITIALIZED 171.8ms
[UI-LOAD] bible-main DRAW 900.1ms
[UI-LOAD] app FIRST_CHAPTER_PAINTED 950,5ms width=1398
[UI-LOAD] app GTK_MAIN_ENTER 810.0ms
[UI-LOAD] app FIRST_CHAPTER_PAINTED 990,0ms width=1398
'''
        self.assertEqual(bench.milestones(text), dict(zip(bench.EVENTS, [171.8, 800.2, 950.5, 810.0])))

    def test_ready_is_not_painted(self):
        result = bench.milestones('[UI-LOAD] app FIRST_CONTENT_READY 800ms\n')
        self.assertNotIn('FIRST_CHAPTER_PAINTED', result)
        self.assertLess(len(result), len(bench.EVENTS))

    def test_complete_window_is_measured_separately(self):
        values = bench.milestones('[UI-LOAD] app FIRST_CHAPTER_PAINTED 300ms\n'
                                  '[UI-LOAD] app FIRST_READING_WINDOW_READY 650ms\n')
        self.assertEqual(values['FIRST_CHAPTER_PAINTED'], 300)
        self.assertEqual(values[bench.WINDOW_EVENT], 650)


class IsolatedRunsTest(unittest.TestCase):
    def run_benchmark(self, root, cache_state, renderer):
        app = root / 'app'
        app.write_text('''#!/usr/bin/env python3
import os
from pathlib import Path
p = Path(os.environ['XDG_CACHE_HOME']) / 'probe'
print('cache_present=' + str(p.exists()), flush=True)
p.touch()
print('renderer=' + os.environ.get('GSK_RENDERER', 'default'), flush=True)
print('runtime=' + os.environ.get('XDG_RUNTIME_DIR', ''), flush=True)
for event in ('GTK_INITIALIZED', 'FIRST_CONTENT_READY', 'FIRST_CHAPTER_PAINTED', 'GTK_MAIN_ENTER', 'FIRST_READING_WINDOW_READY'):
    print('[UI-LOAD] app ' + event + ' 123.4ms', flush=True)
''')
        app.chmod(0o755)
        (root / 'sword/mods.d').mkdir(parents=True, exist_ok=True)
        (root / 'modules').mkdir(exist_ok=True)
        (root / 'profile').mkdir(exist_ok=True)
        (root / 'profile/settings.xml').write_text('<Xiphos/>')
        output = root / cache_state
        argv = ['bench', '--app', str(app), '--sword', str(root / 'sword'),
                '--modules', str(root / 'modules'), '--profile', str(root / 'profile'),
                '--runs', '1', '--output', str(output), '--display', 'current',
                '--renderer', renderer, '--cache-state', cache_state, '--require-reading-window']
        with mock.patch('sys.argv', argv), mock.patch.dict(os.environ, {
                'XDG_RUNTIME_DIR': '/example/compositor', 'GSK_RENDERER': 'vulkan'}), \
                mock.patch.object(bench.time, 'sleep'):
            bench.main()
        return output

    def test_current_display_empty_caches_and_explicit_renderer(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = self.run_benchmark(Path(tmp), 'empty', 'cairo')
            for log in output.glob('*.log'):
                text = log.read_text()
                self.assertIn('cache_present=False', text)
                self.assertIn('renderer=cairo', text)
                self.assertIn('runtime=/example/compositor', text)
            summary = json.loads((output / 'summary.json').read_text())
            self.assertEqual(summary['cache_state'], 'empty')
            self.assertEqual(summary['modes']['sqlite']['median_ms']['FIRST_CHAPTER_PAINTED'], 123.4)
            self.assertEqual(summary['modes']['sqlite']['median_ms'][bench.WINDOW_EVENT], 123.4)

    def test_warm_caches_and_application_default_renderer(self):
        with tempfile.TemporaryDirectory() as tmp:
            output = self.run_benchmark(Path(tmp), 'warm', 'default')
            self.assertIn('cache_present=False', (output / 'sqlite-0.log').read_text())
            self.assertIn('cache_present=True', (output / 'sqlite-1.log').read_text())
            self.assertIn('renderer=default', (output / 'sqlite-1.log').read_text())


class ComparisonTest(unittest.TestCase):
    def run_comparison(self, root, failed=False):
        binary = root / 'binary'
        binary.touch()
        output = root / 'results'
        argv = ['compare', '--before-debug', str(binary), '--before-optimized', str(binary),
                '--after', str(binary), '--output', str(output)]
        times = iter((800, 760, 600, 650, 1000, 900))

        def fake_run(command, **kwargs):
            folder = Path(command[command.index('--output') + 1])
            folder.mkdir()
            if not failed:
                value = next(times)
                summary = {'modes': {mode: {'median_ms': {'FIRST_CHAPTER_PAINTED': value}}
                                     for mode in ('sqlite', 'sword')}}
                (folder / 'summary.json').write_text(json.dumps(summary))
            self.assertEqual('--require-reading-window' in command, folder.name.startswith('after'))
            return mock.Mock(returncode=int(failed))

        with mock.patch('sys.argv', argv), mock.patch.object(compare.subprocess, 'run', fake_run):
            status = compare.main()
        return status, json.loads((output / 'comparison.json').read_text())

    def test_gains_and_regressions_are_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            status, result = self.run_comparison(Path(tmp))
            self.assertEqual(status, 0)
            self.assertEqual(result['comparisons']['total']['sqlite']['saved_percent'], 25)
            self.assertEqual(result['comparisons']['cairo']['sqlite']['saved_ms'], -50)

    def test_failed_runs_do_not_fabricate_improvements(self):
        with tempfile.TemporaryDirectory() as tmp:
            status, result = self.run_comparison(Path(tmp), failed=True)
            self.assertEqual(status, 1)
            self.assertEqual(len(result['cases']), 6)
            self.assertEqual(result['comparisons']['total'], {'status': 'unavailable'})


if __name__ == '__main__':
    unittest.main()
