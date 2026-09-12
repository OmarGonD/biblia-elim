#!/usr/bin/env bash
# UI-WIDTH-101: reproducible SpaPlatense vs SpaRV width diagnosis.
# Writes /tmp/biblia-width-debug.log and /tmp/biblia-width-<Module>.html
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/src/gtk/biblia-elim"
SRC_CFG="${XDG_CONFIG_HOME:-${HOME}/.config}/xiphos"
LOG="/tmp/biblia-width-debug.log"
BASE="/tmp/biblia-width-101-run"

if [[ ! -x "$BIN" ]]; then
	echo "missing binary: $BIN" >&2
	exit 1
fi

prepare_config() {
	local mod="$1"
	local verse="$2"
	local dest="$3/xiphos"

	rm -rf "$3"
	mkdir -p "$dest/tabs" "$dest/bookmarks"
	cp -a "${SRC_CFG}/settings.xml" "$dest/"
	cp -a "${SRC_CFG}/fonts.conf" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/modops.conf" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/template.pad" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/studypad.spt" "$dest/" 2>/dev/null || true

	python3 - "$dest/settings.xml" "$mod" "$verse" <<'PY'
import re, sys
path, mod, verse = sys.argv[1:4]
text = open(path, encoding='utf-8').read()
text, n1 = re.subn(r'(<bible>)[^<]*(</bible>)', r'\1%s\2' % mod, text, count=1)
text, n2 = re.subn(r'(<verse>)[^<]*(</verse>)', r'\1%s\2' % verse, text, count=1)
text, n3 = re.subn(r'(<reading_mode>)[^<]*(</reading_mode>)', r'\g<1>0\g<2>', text, count=1)
text, n4 = re.subn(r'(<reading_compare>)[^<]*(</reading_compare>)', r'\g<1>0\g<2>', text, count=1)
if n1 < 1 or n2 < 1:
    raise SystemExit('failed to patch settings.xml bible/verse (%s/%s)' % (n1, n2))
open(path, 'w', encoding='utf-8').write(text)
print('patched bible=%s verse=%s reading_mode=0 (n1=%s n2=%s n3=%s n4=%s)' % (mod, verse, n1, n2, n3, n4))
PY

	cat >"$dest/tabs/.last_session_tabs" <<EOF
<?xml version="1.0"?>
<Xiphos_Tabs Version="">
  <tabs>
    <tab text_mod="${mod}" commentary_mod="SpaPlatenseComentarios" dictlex_mod="EsWiktionary" book_mod="" text_commentary_key="${verse}" dictlex_key="LA UNIÓN HACE LA FUERZA" book_offset="0" comm_showing="no" showtexts="yes" showpreview="no" showcomms="no" showdicts="no" showparallel="no"/>
  </tabs>
</Xiphos_Tabs>
EOF
}

run_one() {
	local label="$1"
	local mod="$2"
	local verse="$3"
	local cfg="${BASE}-${label}"

	prepare_config "$mod" "$verse" "$cfg"
	echo "===== RUN ${label} module=${mod} verse=${verse} =====" | tee -a "$LOG"
	# Isolated config; auto-quit after deferred width probes.
	env -u GNOME_ACCESSIBILITY \
		XDG_CONFIG_HOME="$cfg" \
		BIBLIA_ELIM_WIDTH_DEBUG=1 \
		BIBLIA_ELIM_WIDTH_DEBUG_QUIT=1 \
		timeout --signal=TERM --kill-after=5s 25s \
		"$BIN" >>"$LOG" 2>&1 || true
	echo "===== END RUN ${label} =====" | tee -a "$LOG"
}

: >"$LOG"
echo "UI-WIDTH-101 measure start $(date -Iseconds)" | tee -a "$LOG"

# Fresh-start sequence B pieces and the required pair comparison.
run_one "A-SpaRV" "SpaRV" "Hechos 1:2"
run_one "B-SpaPlatense" "SpaPlatense" "Hechos 1:2"
run_one "C-SpaRV-again" "SpaRV" "Hechos 1:2"
run_one "D-SpaPlatense-again" "SpaPlatense" "Hechos 1:2"

echo "UI-WIDTH-101 measure done $(date -Iseconds)" | tee -a "$LOG"
echo "log: $LOG"
ls -la /tmp/biblia-width-*.html 2>/dev/null || true
