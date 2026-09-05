#!/usr/bin/env bash
# Vuelve a pasar el OCR sobre los pliegos que dejaron lagunas.
# Baja la página suelta del zip, la decodifica, la pasa por tesseract con
# el modelo español y tira las imágenes: 67 MB de PNG por página no caben
# cuarenta y cuatro veces.
set -uo pipefail
cd "$(dirname "$0")"
ITEM=la-sagrada-biblia-vulgata-tomo-iiv_202111
BASE=https://archive.org/download/${ITEM}
mkdir -p reocr

enc() { python3 -c "import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))" "$1"; }

una() {
	local tomo="$1" pg="$2"
	local salida="reocr/${tomo}_${pg}.hocr"
	[[ -s "${salida}" ]] && return 0
	local nom="LA SAGRADA BIBLIA - Vulgata tomo ${tomo}"
	local num; num=$(printf "%04d" "${pg}")
	local url="${BASE}/$(enc "${nom}_jp2.zip")/$(enc "${nom}_jp2/${nom}_${num}.jp2")"
	local jp2="reocr/${tomo}_${pg}.jp2" png="reocr/${tomo}_${pg}.png"
	curl -sL --max-time 300 -o "${jp2}" "${url}" || { echo "fallo descarga ${tomo}/${pg}"; return 1; }
	[[ -s "${jp2}" ]] || { echo "vacío ${tomo}/${pg}"; return 1; }
	vips copy "${jp2}" "${png}" 2>/dev/null || { echo "fallo decodificar ${tomo}/${pg}"; rm -f "${jp2}"; return 1; }
	OMP_THREAD_LIMIT=2 TESSDATA_PREFIX="${PWD}/tessdata" \
		tesseract "${png}" "reocr/${tomo}_${pg}" -l spa --psm 3 --dpi 600 hocr 2>/dev/null
	rm -f "${jp2}" "${png}"
	[[ -s "${salida}" ]] && echo "hecho ${tomo}/${pg}" || echo "sin hocr ${tomo}/${pg}"
}
export -f una enc
export BASE

python3 -c "
import json
d=json.load(open('rehacer_faltan.json'))
for t,ps in d.items():
    for p in ps: print(t,p)
" | xargs -P 4 -n 2 bash -c 'una "$0" "$1"'

echo "── páginas rehechas: $(ls reocr/*.hocr 2>/dev/null | wc -l)"
