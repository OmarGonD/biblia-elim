#!/bin/sh
set -eu
module_dir=${1:?usage: startup_sqlite_regression.sh MODULE_DIRECTORY}
log_file=$(mktemp)
trap 'rm -f "$log_file"' EXIT
set +e
timeout 8s env NO_AT_BRIDGE=1 dbus-run-session -- xvfb-run -a "$PWD/build/src/gtk/biblia-elim" "--backend=sqlite:${module_dir}" >"$log_file" 2>&1
status=$?
set -e
if [ "$status" -eq 124 ]; then
	printf '%s\n' "SQLite startup survived regression window"
	exit 0
fi
printf '%s\n' "SQLite startup exited unexpectedly (status ${status})" >&2
cat "$log_file" >&2
exit 1
