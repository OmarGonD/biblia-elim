#!/usr/bin/env python3
"""Measure an OSIS import with Linux wait4 resource accounting."""

import argparse
import os
from pathlib import Path
import sqlite3
import time


def validate_database(path: Path, expected_verses: int) -> tuple[bool, int]:
    if not path.exists():
        return False, 0
    with sqlite3.connect(f"file:{path}?mode=ro", uri=True) as database:
        integrity = database.execute("PRAGMA integrity_check").fetchone()[0]
        foreign_keys = database.execute("PRAGMA foreign_key_check").fetchall()
        verses = database.execute("SELECT count(*) FROM verses").fetchone()[0]
    valid = integrity == "ok" and not foreign_keys and verses == expected_verses
    valid = valid and not Path(f"{path}.tmp").exists()
    return valid, verses


def run_once(arguments: argparse.Namespace, run: int) -> dict[str, object]:
    output = arguments.output_dir / f"{arguments.label}-{run}.sqlite"
    log = arguments.output_dir / f"{arguments.label}-{run}.stdout"
    output.unlink(missing_ok=True)
    Path(f"{output}.tmp").unlink(missing_ok=True)
    command = [
        str(arguments.importer), str(arguments.input),
        "--module-id", f"benchmark-{arguments.label}-{run}",
        "--name", f"OSIS benchmark {arguments.label}",
        "--language", "es", "--versification", "custom",
        "--output", str(output),
    ]
    started = time.perf_counter()
    child = os.fork()
    if child == 0:
        descriptor = os.open(log, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
        os.dup2(descriptor, 1)
        os.dup2(descriptor, 2)
        os.close(descriptor)
        os.execv(command[0], command)
    _, status, resources = os.wait4(child, 0)
    wall = time.perf_counter() - started
    exit_code = os.waitstatus_to_exitcode(status)
    valid, actual_verses = validate_database(output, arguments.expected_verses)
    return {
        "label": arguments.label,
        "run": run,
        "input_bytes": arguments.input.stat().st_size,
        "expected_verses": arguments.expected_verses,
        "actual_verses": actual_verses,
        "wall_seconds": wall,
        "user_seconds": resources.ru_utime,
        "system_seconds": resources.ru_stime,
        "peak_rss_kib": resources.ru_maxrss,
        "sqlite_bytes": output.stat().st_size if output.exists() else 0,
        "valid": exit_code == 0 and valid,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--importer", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--expected-verses", type=int, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--runs", type=int, default=3)
    arguments = parser.parse_args()
    arguments.importer = arguments.importer.resolve()
    arguments.input = arguments.input.resolve()
    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    print("label\trun\tinput_bytes\texpected_verses\tactual_verses\twall_seconds\t"
          "user_seconds\tsystem_seconds\tpeak_rss_kib\tsqlite_bytes\tvalid")
    all_valid = True
    for run in range(1, arguments.runs + 1):
        result = run_once(arguments, run)
        print("\t".join(str(result[key]) for key in result))
        all_valid = all_valid and bool(result["valid"])
    raise SystemExit(0 if all_valid else 1)


if __name__ == "__main__":
    main()
