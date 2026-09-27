#!/usr/bin/env bash
# Dependencies: python3, Xvfb; a populated SWORD library and converted modules.
set -euo pipefail
exec python3 "$(dirname "${BASH_SOURCE[0]}")/bench_startup.py" "$@"
