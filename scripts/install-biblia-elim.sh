#!/usr/bin/env bash
# Instala Biblia Elim en ~/.local para el menú de aplicaciones.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN_SRC="${ROOT}/build/src/gtk/xiphos"
PREFIX="${HOME}/.local"
SHARE="${PREFIX}/share/biblia-elim"
APPDIR="${PREFIX}/share/applications"
ICONDIR="${PREFIX}/share/icons/hicolor"
LOCDIR="${PREFIX}/share/locale"

if [[ ! -x "${BIN_SRC}" ]]; then
	echo "No encuentro el binario: ${BIN_SRC}" >&2
	echo "Compila primero: cmake --build ${ROOT}/build --target xiphos" >&2
	exit 1
fi

install -d "${PREFIX}/bin" "${SHARE}/pixmaps" "${APPDIR}" \
	"${ICONDIR}/scalable/apps" "${ICONDIR}/48x48/apps" "${ICONDIR}/128x128/apps"

install -m 0755 "${BIN_SRC}" "${PREFIX}/bin/biblia-elim"
install -m 0644 "${ROOT}/desktop/biblia-elim.desktop" \
	"${APPDIR}/biblia-elim.desktop"
install -m 0644 "${ROOT}/desktop/biblia-elim.svg" \
	"${ICONDIR}/scalable/apps/biblia-elim.svg"
install -m 0644 "${ROOT}/desktop/biblia-elim.svg" \
	"${SHARE}/pixmaps/biblia-elim.svg"

if command -v rsvg-convert >/dev/null; then
	rsvg-convert -w 48 -h 48 "${ROOT}/desktop/biblia-elim.svg" \
		-o "${ICONDIR}/48x48/apps/biblia-elim.png"
	rsvg-convert -w 128 -h 128 "${ROOT}/desktop/biblia-elim.svg" \
		-o "${ICONDIR}/128x128/apps/biblia-elim.png"
	cp -f "${ICONDIR}/48x48/apps/biblia-elim.png" "${SHARE}/pixmaps/biblia-elim.png"
	cp -f "${ICONDIR}/48x48/apps/biblia-elim.png" "${SHARE}/pixmaps/gs2-48x48.png"
fi

if [[ -d "${ROOT}/pixmaps" ]]; then
	find "${ROOT}/pixmaps" -maxdepth 1 -type f \( -name '*.png' -o -name '*.xpm' -o -name '*.svg' \) \
		-exec install -m 0644 {} "${SHARE}/pixmaps/" \;
fi
if [[ -d "${ROOT}/ui" ]]; then
	install -d "${SHARE}/ui"
	find "${ROOT}/ui" -maxdepth 1 -type f \( -name '*.png' -o -name '*.xml' \) \
		-exec install -m 0644 {} "${SHARE}/ui/" \;
fi

if [[ -d "${ROOT}/build/locale" ]]; then
	find "${ROOT}/build/locale" -name 'xiphos.mo' | while read -r mo; do
		lang="$(echo "${mo}" | sed -n 's|.*/locale/\([^/]*\)/LC_MESSAGES/xiphos.mo|\1|p')"
		[[ -n "${lang}" ]] || continue
		install -d "${LOCDIR}/${lang}/LC_MESSAGES"
		install -m 0644 "${mo}" "${LOCDIR}/${lang}/LC_MESSAGES/xiphos.mo"
	done
fi

# Torres Amat va dentro del paquete, no se descarga.
#
# El resto de las Biblias las trae main_bootstrap_default_modules() de los
# repositorios SWORD en el primer arranque. Esta no está en ninguno: se
# construyó para esta aplicación a partir de los escaneos de la edición de
# 1882 (véase scripts/torresamat/). Como no hay de dónde bajarla, viaja
# con el instalador.
#
# Solo si falta: quien la haya borrado a propósito, o tenga una revisión
# más nueva que la del paquete, no quiere que se la pisemos en cada
# reinstalación.
SWORDDIR="${HOME}/.sword"
if [[ -d "${ROOT}/modulos" ]] && [[ ! -f "${SWORDDIR}/mods.d/torresamat.conf" ]]; then
	install -d "${SWORDDIR}/mods.d" \
		"${SWORDDIR}/modules/texts/ztext/torresamat"
	install -m 0644 "${ROOT}/modulos/mods.d/torresamat.conf" \
		"${SWORDDIR}/mods.d/torresamat.conf"
	install -m 0644 "${ROOT}"/modulos/modules/texts/ztext/torresamat/* \
		"${SWORDDIR}/modules/texts/ztext/torresamat/"
	echo "  Biblia:   Torres Amat instalada en ${SWORDDIR}"
fi

# Y sus notas, que son la mitad de la obra, como comentario aparte.
if [[ -d "${ROOT}/modulos" ]] && [[ ! -f "${SWORDDIR}/mods.d/torresamatnotas.conf" ]]; then
	install -d "${SWORDDIR}/mods.d" \
		"${SWORDDIR}/modules/comments/zcom/torresamatnotas"
	install -m 0644 "${ROOT}/modulos/mods.d/torresamatnotas.conf" \
		"${SWORDDIR}/mods.d/torresamatnotas.conf"
	install -m 0644 "${ROOT}"/modulos/modules/comments/zcom/torresamatnotas/* \
		"${SWORDDIR}/modules/comments/zcom/torresamatnotas/"
	echo "  Notas:    las de Torres Amat, por capítulo"
fi

if command -v update-desktop-database >/dev/null; then
	update-desktop-database "${APPDIR}" >/dev/null 2>&1 || true
fi
if command -v gtk-update-icon-cache >/dev/null; then
	gtk-update-icon-cache -f -t "${ICONDIR}" >/dev/null 2>&1 || true
fi
if command -v xdg-desktop-menu >/dev/null; then
	xdg-desktop-menu forceupdate >/dev/null 2>&1 || true
fi

echo "Biblia Elim instalada."
echo "  comando:  ${PREFIX}/bin/biblia-elim"
echo "  menú:     ${APPDIR}/biblia-elim.desktop"
echo "  icono:    ${ICONDIR}/scalable/apps/biblia-elim.svg"
