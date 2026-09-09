#!/bin/sh
set -eu
root=${1:-$(pwd)}
bin="$root/build/src/gtk/biblia-elim"
module_dir=${2:-/tmp/rv1909-strong-enabled}
export BIBLIA_ELIM_SQLITE_MODULES="$module_dir"
run_case() {
	name=$1; shift
	log=$(mktemp)
	trap 'rm -f "$log"' EXIT
	set +e
	timeout 5s env NO_AT_BRIDGE=1 dbus-run-session -- xvfb-run -a "$bin" "$@" >"$log" 2>&1
	status=$?
	set -e
	if [ "$status" -ne 124 ] || grep -Eq 'SIGSEGV|Segmentation|Aborted|SQLite.*error' "$log"; then
		echo "$name failed (status $status)" >&2
		cat "$log" >&2
		exit 1
	fi
	echo "$name ok"
}

run_case explicit-sqlite "--backend=sqlite:${module_dir}"
run_case explicit-sword --backend=sword
run_case default-with-env
run_case fallback --backend=sqlite:/tmp/does-not-exist
