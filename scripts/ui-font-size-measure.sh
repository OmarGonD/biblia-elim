#!/usr/bin/env bash
# Measure SpaPlatense vs SpaRV effective bible body font size.
# Writes /tmp/biblia-font-debug.log and /tmp/<Module>-font-debug.html
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${ROOT}/build/src/gtk/biblia-elim"
SRC_CFG="${XDG_CONFIG_HOME:-${HOME}/.config}/xiphos"
LOG="/tmp/biblia-font-debug.log"
BASE="/tmp/biblia-font-101-run"

if [[ ! -x "$BIN" ]]; then
	echo "missing binary: $BIN" >&2
	exit 1
fi

prepare_config() {
	local mod="$1"
	local verse="$2"
	local zoom_pct="$3"
	local dest="$4/xiphos"

	rm -rf "$4"
	mkdir -p "$dest/tabs" "$dest/bookmarks"
	cp -a "${SRC_CFG}/settings.xml" "$dest/"
	cp -a "${SRC_CFG}/fonts.conf" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/modops.conf" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/template.pad" "$dest/" 2>/dev/null || true
	cp -a "${SRC_CFG}/studypad.spt" "$dest/" 2>/dev/null || true

	python3 - "$dest/settings.xml" "$mod" "$verse" "$zoom_pct" <<'PY'
import re, sys
path, mod, verse, zoom = sys.argv[1:5]
text = open(path, encoding='utf-8').read()
text, n1 = re.subn(r'(<bible>)[^<]*(</bible>)', r'\1%s\2' % mod, text, count=1)
text, n2 = re.subn(r'(<verse>)[^<]*(</verse>)', r'\1%s\2' % verse, text, count=1)
text, n3 = re.subn(r'(<reading_mode>)[^<]*(</reading_mode>)', r'\g<1>0\g<2>', text, count=1)
text, n4 = re.subn(r'(<reading_compare>)[^<]*(</reading_compare>)', r'\g<1>0\g<2>', text, count=1)
# Force bible-main zoom percent in surfacezoom.
def repl_zoom(m):
    parts = []
    for item in m.group(1).split(';'):
        if not item:
            continue
        k, _, v = item.partition('=')
        if k == 'bible-main':
            v = zoom
        parts.append('%s=%s' % (k, v))
    if not any(p.startswith('bible-main=') for p in parts):
        parts.insert(0, 'bible-main=%s' % zoom)
    return '<surfacezoom>%s</surfacezoom>' % ';'.join(parts)
text2, n5 = re.subn(r'<surfacezoom>([^<]*)</surfacezoom>', repl_zoom, text, count=1)
if n5 < 1:
    text2, n5 = re.subn(r'(</fontsize>)',
                        r'<surfacezoom>bible-main=%s</surfacezoom>\1' % zoom,
                        text, count=1)
text = text2
if n1 < 1 or n2 < 1:
    raise SystemExit('failed to patch settings.xml (%s/%s)' % (n1, n2))
open(path, 'w', encoding='utf-8').write(text)
print('patched bible=%s verse=%s zoom=%s (n1=%s n2=%s n5=%s)' % (mod, verse, zoom, n1, n2, n5))
PY

	cat >"$dest/tabs/.last_session_tabs" <<EOF
<?xml version="1.0"?>
<Xiphos_Tabs Version="">
  <tabs>
    <tab text_mod="${mod}" commentary_mod="SpaPlatenseComentarios" dictlex_mod="EsWiktionary" book_mod="" text_commentary_key="${verse}" dictlex_key="LA UNIÓN HACE LA FUERZA" book_offset="0" comm_showing="no" showtexts="yes" showpreview="no" showcomms="no" showdicts="no" showparallel="no"/>
  </tabs>
</Xiphos_Tabs>
EOF
	echo "----- fonts.conf -----" | tee -a "$LOG"
	cat "$dest/fonts.conf" | tee -a "$LOG"
}

run_one() {
	local label="$1"
	local mod="$2"
	local verse="$3"
	local zoom="$4"
	local cfg="${BASE}-${label}"

	prepare_config "$mod" "$verse" "$zoom" "$cfg"
	echo "===== RUN ${label} module=${mod} verse=${verse} zoom=${zoom} =====" | tee -a "$LOG"
	env -u GNOME_ACCESSIBILITY \
		XDG_CONFIG_HOME="$cfg" \
		BIBLIA_ELIM_FONT_DEBUG=1 \
		BIBLIA_ELIM_FONT_DEBUG_QUIT=1 \
		timeout --signal=TERM --kill-after=5s 30s \
		"$BIN" >>"$LOG" 2>&1 || true
	echo "===== END RUN ${label} =====" | tee -a "$LOG"
}

: >"$LOG"
echo "UI font-size measure start $(date -Iseconds)" | tee -a "$LOG"

run_one "A-SpaRV-100" "SpaRV" "Hechos 1:2" "100"
run_one "B-SpaPlatense-100" "SpaPlatense" "Hechos 1:2" "100"
run_one "C-SpaRV-110" "SpaRV" "Hechos 1:2" "110"
run_one "D-SpaPlatense-110" "SpaPlatense" "Hechos 1:2" "110"
run_one "E-SpaRV-again-100" "SpaRV" "Hechos 1:2" "100"
run_one "F-SpaPlatense-again-100" "SpaPlatense" "Hechos 1:2" "100"
run_one "G-SpaPlatense-Ps103" "SpaPlatense" "Salmos 103:1" "100"
run_one "H-SpaRV-Ps103" "SpaRV" "Salmos 103:1" "100"

echo "UI font-size measure done $(date -Iseconds)" | tee -a "$LOG"
echo "log: $LOG"
ls -la /tmp/*-font-debug.html 2>/dev/null || true
rg -n 'FONT DEBUG|FONT EFFECTIVE|effective_pango|font_size_scale|configured_font' "$LOG" | head -120
