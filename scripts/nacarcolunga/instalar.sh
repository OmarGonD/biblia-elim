#!/usr/bin/env bash
# Construye el módulo Nácar-Colunga e instálalo solo en ~/.sword.
# No va al paquete de la aplicación: la obra sigue con derechos.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SWORDDIR="${HOME}/.sword"
OUT="${HERE}/salida"

cd "${HERE}"
python3 bajar.py
python3 construir.py
python3 completar.py
python3 osis.py
python3 comentario.py

rm -rf "${OUT}/mod"
mkdir -p "${OUT}/mod/mods.d" \
         "${OUT}/mod/modules/texts/ztext/nacarcolunga" \
         "${OUT}/mod/modules/comments/zcom/nacarcolunganotas"

osis2mod "${OUT}/mod/modules/texts/ztext/nacarcolunga" \
         "${OUT}/nacarcolunga.osis.xml" -v NRSVA -z z

cat > "${OUT}/mod/mods.d/nacarcolunga.conf" <<'EOF'
[NacarColunga]
DataPath=./modules/texts/ztext/nacarcolunga/
ModDrv=zText
CompressType=ZIP
BlockType=BOOK
Encoding=UTF-8
SourceType=OSIS
Versification=NRSVA
Lang=es
Description=Sagrada Biblia (Nácar-Colunga, 1944) — reconstrucción automática
About=Sagrada Biblia. Versión directa de las lenguas originales, hebrea y griega, al castellano, por Eloíno Nácar Fuster y Alberto Colunga, O.P. Primera edición, Biblioteca de Autores Cristianos, Madrid, 1944.\par\par ATENCIÓN: RECONSTRUCCIÓN AUTOMÁTICA ASISTIDA, NO REVISADA. El texto sale de un OCR propio (Tesseract español, columna a columna) sobre el facsímil de Internet Archive. Los versos que el reconocimiento dejó a medias se completan con las otras Biblias en castellano (Reina-Valera, Platense) cotejadas con el griego de Tischendorf: no se sustituye el castellano por griego, se rellena el hueco. Conserva erratas y no debe usarse como texto de referencia sin cotejar. Copia local de uso privado: la obra sigue con derechos (Nácar murió en 1971). No distribuir.\par\par Fuente: https://archive.org/details/SagradaBibliaNacarColunga19441Edicin
DistributionLicense=Copyrighted
TextSource=https://archive.org/details/SagradaBibliaNacarColunga19441Edicin
LCSH=Bible.Spanish
MinimumVersion=1.6.1
Version=0.1.0
EOF

if [[ -s "${OUT}/nacarcolunga-notas.imp" ]]; then
    # imp2vs -z z escribe .bzs (zText). zCom es el mismo formato con .czs.
    tmpcom="$(mktemp -d)"
    (cd "${tmpcom}" && imp2vs "${OUT}/nacarcolunga-notas.imp" -z z >/dev/null)
    for x in ot nt; do
        [[ -f "${tmpcom}/${x}.bzs" ]] && mv "${tmpcom}/${x}.bzs" \
            "${OUT}/mod/modules/comments/zcom/nacarcolunganotas/${x}.czs"
        [[ -f "${tmpcom}/${x}.bzv" ]] && mv "${tmpcom}/${x}.bzv" \
            "${OUT}/mod/modules/comments/zcom/nacarcolunganotas/${x}.czv"
        [[ -f "${tmpcom}/${x}.bzz" ]] && mv "${tmpcom}/${x}.bzz" \
            "${OUT}/mod/modules/comments/zcom/nacarcolunganotas/${x}.czz"
    done
    rm -rf "${tmpcom}"
    cat > "${OUT}/mod/mods.d/nacarcolunganotas.conf" <<'EOF'
[NacarColungaNotas]
DataPath=./modules/comments/zcom/nacarcolunganotas/
ModDrv=zCom
CompressType=ZIP
BlockType=CHAPTER
Encoding=UTF-8
SourceType=GBF
Versification=NRSVA
Lang=es
Description=Notas de Nácar-Colunga (1944)
About=Notas al pie de la Sagrada Biblia Nácar-Colunga, 1.ª ed. 1944, agrupadas por capítulo. Copia local de uso privado; no distribuir.
DistributionLicense=Copyrighted
MinimumVersion=1.6.1
Version=0.1.0
EOF
fi

install -d "${SWORDDIR}/mods.d" \
           "${SWORDDIR}/modules/texts/ztext/nacarcolunga"
install -m 0644 "${OUT}/mod/mods.d/nacarcolunga.conf" \
    "${SWORDDIR}/mods.d/nacarcolunga.conf"
install -m 0644 "${OUT}"/mod/modules/texts/ztext/nacarcolunga/* \
    "${SWORDDIR}/modules/texts/ztext/nacarcolunga/"

if [[ -f "${OUT}/mod/mods.d/nacarcolunganotas.conf" ]]; then
    install -d "${SWORDDIR}/modules/comments/zcom/nacarcolunganotas"
    install -m 0644 "${OUT}/mod/mods.d/nacarcolunganotas.conf" \
        "${SWORDDIR}/mods.d/nacarcolunganotas.conf"
    if ls "${OUT}"/mod/modules/comments/zcom/nacarcolunganotas/* >/dev/null 2>&1; then
        install -m 0644 "${OUT}"/mod/modules/comments/zcom/nacarcolunganotas/* \
            "${SWORDDIR}/modules/comments/zcom/nacarcolunganotas/"
    fi
fi

echo "Instalado en ${SWORDDIR}"
echo "Abre Biblia Elim y elige NacarColunga en el selector."
