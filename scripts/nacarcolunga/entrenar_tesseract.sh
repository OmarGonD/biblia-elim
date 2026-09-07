#!/usr/bin/env bash
# Ajuste supervisado: solo usa líneas transcritas y revisadas por persona.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
ground_truth="${1:-$here/ground-truth}"
[[ -d "$ground_truth" ]] || { echo "Falta $ground_truth" >&2; exit 2; }
command -v tesstrain >/dev/null || { echo "Instala tesstrain antes de entrenar" >&2; exit 2; }
# Cada .gt.txt debe tener su imagen homónima y provenir de revision.json.
tesstrain --fonts_dir /usr/share/fonts --lang spa_nacar1944 \
  --linedata_only --noextract_font_properties \
  --langdata_dir "$ground_truth" --tessdata_dir "$here/tessdata" \
  --training_text "$ground_truth/transcripciones.txt" \
  --output_dir "$here/tessdata" --overwrite
