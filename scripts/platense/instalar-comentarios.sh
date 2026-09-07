#!/usr/bin/env bash
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
dest="${HOME}/.sword"
work="$(mktemp -d)"
python3 "$here/comentarios.py"
imp2vs "$here/platense-comentarios.imp" -z z -b 3 -v Vulg -l es \
  -o "$work" >/dev/null
mkdir -p "$dest/mods.d" "$dest/modules/comments/zcom/spaplatensecomentarios"
for testament in ot nt; do
  for ext in czs czv czz; do
    src="$work/$testament.$ext"; [[ -f "$src" ]] || continue
    cp "$src" "$dest/modules/comments/zcom/spaplatensecomentarios/$testament.$ext"
  done
done
cp "$here/spaplatensecomentarios.conf" "$dest/mods.d/spaplatensecomentarios.conf"
echo "Instalado SpaPlatenseComentarios"
