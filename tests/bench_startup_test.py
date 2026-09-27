#!/usr/bin/env python3
"""Parsing regressions for the real-startup benchmark (no display needed)."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('bench_startup', Path(__file__).parents[1] / 'tools/bench_startup.py')
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


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


if __name__ == '__main__':
    unittest.main()
