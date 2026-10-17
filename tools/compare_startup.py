#!/usr/bin/env python3
"""Compare preserved startup binaries on one display with isolated profiles."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--before-debug', type=Path, required=True)
    parser.add_argument('--before-optimized', type=Path, required=True)
    parser.add_argument('--after', type=Path, required=True)
    parser.add_argument('--modules', type=Path, default=Path(os.environ.get(
        'XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'biblia-elim/modules')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--display', choices=('current', 'xvfb'), default='current')
    parser.add_argument('--runs', type=int, default=5)
    args = parser.parse_args()
    for binary in (args.before_debug, args.before_optimized, args.after):
        if not binary.is_file():
            parser.error(f'missing binary: {binary}')
    if args.runs < 1:
        parser.error('runs must be positive')
    args.output.mkdir(parents=True, exist_ok=False)
    cases = [('before-debug', args.before_debug, 'default', 'warm'),
             ('before-optimized', args.before_optimized, 'default', 'warm'),
             ('after-optimized', args.after, 'default', 'warm'),
             ('after-cairo', args.after, 'cairo', 'warm'),
             ('before-empty-cache', args.before_optimized, 'default', 'empty'),
             ('after-empty-cache', args.after, 'default', 'empty')]
    results = {}
    for name, binary, renderer, cache in cases:
        output = args.output / name
        command = [sys.executable, str(Path(__file__).with_name('bench_startup.py')),
                   '--app', str(binary), '--modules', str(args.modules),
                   '--display', args.display, '--runs', str(args.runs),
                   '--renderer', renderer, '--cache-state', cache, '--output', str(output)]
        if name.startswith('after'):
            command.append('--require-reading-window')
        with (args.output / f'{name}.log').open('w') as log:
            status = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT).returncode
        if status:
            failure = output / 'failure.json'
            results[name] = {'error': json.loads(failure.read_text()) if failure.exists()
                             else f'benchmark exited {status}; see {name}.log'}
        else:
            results[name] = json.loads((output / 'summary.json').read_text())
        print(f'{name}: {"FAILED" if status else "measured"}', flush=True)
    comparisons = {}
    for label, before, after in [
            ('optimized-build', 'before-debug', 'before-optimized'),
            ('deferred-neighbours', 'before-optimized', 'after-optimized'),
            ('total', 'before-debug', 'after-optimized'),
            ('cairo', 'after-optimized', 'after-cairo'),
            ('empty-cache', 'before-empty-cache', 'after-empty-cache')]:
        if any('error' in results[case] for case in (before, after)):
            comparisons[label] = {'status': 'unavailable'}
            continue
        comparisons[label] = {}
        for mode in ('sqlite', 'sword'):
            event = 'FIRST_CHAPTER_PAINTED'
            baseline = results[before]['modes'][mode]['median_ms'][event]
            final = results[after]['modes'][mode]['median_ms'][event]
            comparisons[label][mode] = {'before_ms': baseline, 'after_ms': final,
                                        'saved_ms': baseline - final,
                                        'saved_percent': 100 * (baseline - final) / baseline}
    (args.output / 'comparison.json').write_text(json.dumps({
        'cases': results, 'comparisons': comparisons}, indent=2) + '\n')
    return 1 if any('error' in result for result in results.values()) else 0


if __name__ == '__main__':
    sys.exit(main())
