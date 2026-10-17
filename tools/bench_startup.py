#!/usr/bin/env python3
"""Measure real startup draws, without smoke/probe instrumentation or user writes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import signal
import shutil
import statistics
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

EVENTS = ('GTK_INITIALIZED', 'FIRST_CONTENT_READY', 'FIRST_CHAPTER_PAINTED', 'GTK_MAIN_ENTER')
WINDOW_EVENT = 'FIRST_READING_WINDOW_READY'
TRACE = re.compile(r'^\[UI-LOAD\] app (\w+) ([0-9]+(?:[.,][0-9]+)?)ms', re.M)


def milestones(text):
    values = {}
    for event, number in TRACE.findall(text):
        if event in (*EVENTS, WINDOW_EVENT):
            values.setdefault(event, float(number.replace(',', '.')))
    return values


def stop(process, process_group=False):
    # Popen owns this exact PID; never match command lines or kill unrelated apps.
    if process_group:
        # The application owns this group (start_new_session=True), including
        # any automatic module-conversion subprocesses started during a run.
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
    elif process.poll() is None:
        process.terminate()
    if process.poll() is None:
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            if process_group:
                os.killpg(process.pid, signal.SIGKILL)
            else:
                process.kill()
            process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=Path, default=Path('build/src/gtk/biblia-elim'))
    parser.add_argument('--runs', type=int, default=5)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--sword', type=Path, default=Path.home() / '.sword')
    parser.add_argument('--modules', type=Path, default=Path(os.environ.get('XDG_DATA_HOME', str(Path.home() / '.local/share'))) / 'biblia-elim/modules')
    parser.add_argument('--profile', type=Path, default=Path(os.environ.get('XDG_CONFIG_HOME', str(Path.home() / '.config'))) / 'xiphos')
    parser.add_argument('--timeout', type=float, default=30)
    parser.add_argument('--keep-profile', action='store_true', help='retain isolated profile for XTEST inspection')
    parser.add_argument('--display', choices=('xvfb', 'current'), default='xvfb',
                        help='use Xvfb or the current Wayland/X11 session')
    parser.add_argument('--renderer', choices=('default', 'gl', 'cairo', 'vulkan'), default='default')
    parser.add_argument('--cache-state', choices=('warm', 'empty'), default='warm',
                        help='empty clears only the isolated application caches, not the OS disk cache')
    parser.add_argument('--require-reading-window', action='store_true',
                        help='also wait for the deferred chapter window and viewport restore')
    args = parser.parse_args()
    if args.runs < 1 or args.timeout <= 0:
        parser.error('runs and timeout must be positive')
    for path in (args.app, args.sword / 'mods.d', args.modules, args.profile / 'settings.xml'):
        if not path.exists():
            parser.error(f'missing {path}')
    args.output.mkdir(parents=True, exist_ok=False)
    root = Path(tempfile.mkdtemp(prefix='biblia-startup-'))
    server = None
    try:
        environment = {k: v for k, v in os.environ.items() if not k.startswith('BIBLIA_ELIM_')}
        for variable, folder in [('HOME', 'home'), ('XDG_CONFIG_HOME', 'config'), ('XDG_DATA_HOME', 'data'), ('XDG_CACHE_HOME', 'cache'), ('XDG_STATE_HOME', 'state'), ('XDG_RUNTIME_DIR', 'runtime')]:
            path = root / folder
            path.mkdir(mode=0o700)
            environment[variable] = str(path)
        if args.display == 'current':
            if 'XDG_RUNTIME_DIR' in os.environ:
                environment['XDG_RUNTIME_DIR'] = os.environ['XDG_RUNTIME_DIR']
        environment.pop('GSK_RENDERER', None)
        if args.renderer != 'default':
            environment['GSK_RENDERER'] = args.renderer
        (root / 'home/.sword').symlink_to(args.sword.resolve(), target_is_directory=True)
        environment.update(SWORD_PATH=str(args.sword.resolve()), BIBLIA_ELIM_UI_LOAD_DEBUG='1')
        if args.display == 'xvfb':
            environment.update(GDK_BACKEND='x11', GDK_SCALE='1', GDK_DPI_SCALE='1')
            environment.pop('WAYLAND_DISPLAY', None)
        shutil.copytree(args.profile, root / 'config/xiphos')
        modules = root / 'data/biblia-elim/modules'
        shutil.copytree(args.modules, modules)
        # Copies have new inodes: warm-up builds their validation cache normally.
        settings = root / 'config/xiphos/settings.xml'
        tree = ET.parse(settings)
        for section, key, value in [('modules', 'bible', 'SpaPlatense'), ('keys', 'verse', 'Luke 23:36'), ('layout', 'width', '1400'), ('layout', 'height', '1000'), ('layout', 'maximized', '0'), ('misc', 'show_sidebar', '0'), ('misc', 'showcomms', '0'), ('misc', 'showdicts', '0'), ('misc', 'showpreview', '0'), ('misc', 'show_side_preview', '0'), ('misc', 'showparatab', '0'), ('misc', 'splash', '0')]:
            parent = tree.getroot().find(section)
            if parent is None:
                parent = ET.SubElement(tree.getroot(), section)
            matches = parent.findall(key)
            if not matches:
                matches = [ET.SubElement(parent, key)]
            for element in matches:
                element.text = value
        tree.write(settings, encoding='utf-8', xml_declaration=True)
        original_settings = settings.read_bytes()
        tabs = ET.Element('Xiphos_Tabs', Version='4.4.0')
        ET.SubElement(ET.SubElement(tabs, 'tabs'), 'tab',
                      text_mod='SpaPlatense', commentary_mod='SpaPlatenseComentarios',
                      dictlex_mod='EsWiktionary', book_mod='',
                      text_commentary_key='Luke 23:36', dictlex_key='Adonai',
                      book_offset='0', comm_showing='yes', showtexts='yes',
                      showpreview='no', showcomms='no', showdicts='no', showparallel='no')
        tab_file = root / 'config/xiphos/tabs/.last_session_tabs'
        tab_file.parent.mkdir(exist_ok=True)
        ET.ElementTree(tabs).write(tab_file, encoding='utf-8', xml_declaration=True)
        original_tabs = tab_file.read_bytes()
        read_fd, write_fd = os.pipe() if args.display == 'xvfb' else (None, None)
        try:
            if args.display == 'xvfb':
                with (args.output / 'xvfb.log').open('w') as log:
                    server = subprocess.Popen(['Xvfb', '-displayfd', str(write_fd), '-screen', '0', '1400x1000x24', '-nolisten', 'tcp'], pass_fds=(write_fd,), stdout=log, stderr=log)
                os.close(write_fd)
                write_fd = None
                if not select.select([read_fd], [], [], 10)[0]:
                    raise RuntimeError('Xvfb did not become ready; see xvfb.log')
                number = os.read(read_fd, 64).decode().strip()
                if not number.isdigit():
                    raise RuntimeError('Xvfb failed; see xvfb.log')
                environment['DISPLAY'] = ':' + number
        finally:
            if read_fd is not None:
                os.close(read_fd)
            if write_fd is not None:
                os.close(write_fd)
        summary = {'app': str(args.app.resolve()), 'display': args.display,
                   'app_sha256': hashlib.sha256(args.app.read_bytes()).hexdigest(),
                   'settings_sha256': hashlib.sha256(original_settings).hexdigest(),
                   'tabs_sha256': hashlib.sha256(original_tabs).hexdigest(),
                   'display_environment': {key: environment.get(key) for key in (
                       'DISPLAY', 'WAYLAND_DISPLAY', 'GDK_BACKEND', 'GDK_SCALE', 'GDK_DPI_SCALE')},
                   'renderer': args.renderer, 'cache_state': args.cache_state,
                   'runs': args.runs, 'warmups_per_mode': 1, 'profile': str(root), 'modes': {}}
        for mode in ('sqlite', 'sword'):
            samples = []
            for run in range(args.runs + 1):
                if args.cache_state == 'empty':
                    shutil.rmtree(root / 'cache')
                    (root / 'cache').mkdir(mode=0o700)
                settings.write_bytes(original_settings)
                tab_file.write_bytes(original_tabs)
                log_path = args.output / f'{mode}-{run}.log'
                command = [str(args.app.resolve()), '--backend=sword' if mode == 'sword' else f'--backend=sqlite:{modules}']
                with log_path.open('w') as log:
                    process = subprocess.Popen(command, env=environment, stdout=log, stderr=log,
                                               start_new_session=True)
                    try:
                        deadline = time.monotonic() + args.timeout
                        while True:
                            text = log_path.read_text(errors='replace')
                            values = milestones(text)
                            required = (*EVENTS, WINDOW_EVENT) if args.require_reading_window else EVENTS
                            if all(event in values for event in required):
                                # Allow one second of ordinary interaction/event work, detect diagnostics.
                                time.sleep(1)
                                break
                            if process.poll() is not None or time.monotonic() > deadline:
                                raise RuntimeError(f'incomplete startup: {log_path}; events={values}')
                            time.sleep(.05)
                        text = log_path.read_text(errors='replace')
                        values = milestones(text)
                        if 'FIRST_READING_WINDOW_FAILED' in text:
                            raise RuntimeError(f'initial viewport restoration failed: {log_path}')
                        if re.search(r'(Gtk|Gdk|GLib(?:-GObject|-GIO)?)-(CRITICAL|WARNING|ERROR)', text):
                            raise RuntimeError(f'GTK diagnostic: {log_path}')
                        if process.poll() not in (None, 0):
                            raise RuntimeError(f'application exited {process.returncode}: {log_path}')
                    finally:
                        stop(process, process_group=True)
                if run:
                    samples.append(values)
                print(mode, 'warmup' if run == 0 else run, values, flush=True)
            measured_events = [event for event in (*EVENTS, WINDOW_EVENT)
                               if all(event in sample for sample in samples)]
            summary['modes'][mode] = {'samples_ms': samples, 'median_ms': {
                event: statistics.median(s[event] for s in samples) for event in measured_events}}
        (args.output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        print('mode | ' + ' | '.join(EVENTS))
        for mode, result in summary['modes'].items():
            print(mode + ' | ' + ' | '.join(f'{result["median_ms"][e]:.1f}' for e in EVENTS))
    except Exception as error:
        (args.output / 'failure.json').write_text(json.dumps({
            'app': str(args.app.resolve()), 'display': args.display,
            'renderer': args.renderer, 'cache_state': args.cache_state,
            'error': str(error)}, indent=2) + '\n')
        raise
    finally:
        if server:
            stop(server)
        if args.keep_profile:
            print(f'Isolated profile retained: {root}')
        else:
            shutil.rmtree(root)


if __name__ == '__main__':
    main()
