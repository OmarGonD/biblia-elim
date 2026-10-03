# Herramientas TAGNT / fichas del interlineal

| Archivo | Función |
|---|---|
| `fetch_tagnt.sh` | descarga el TAGNT (commit fijo) a `data/sources/tagnt/` (no se commitea) |
| `tagnt.py` | parser del TAGNT |
| `alinear.py` | alineamiento Tisch (SWORD) → TAGNT → `data/tagnt_alineacion/` |
| `contexto.py` | morfología, ediciones, variante y `caso_regido` por palabra |
| `migrar_fichas.py` | fichas viejas (ref+Strong) → clave nueva (ref+pos. Tisch+Strong) en `data/fichas_v2/` |
| `pipeline.py` | generación por versículo: solicitud → generador → validación → caché → `data/fichas_v3/` |

Entorno del pipeline: `python3 -m venv .venv-fichas && .venv-fichas/bin/pip install -r tools/tagnt/requirements.txt`.
Prompt: `prompts/enriquecer_ficha.md`. Esquema: `schemas/ficha_enriquecida.schema.json`.
Tests: `.venv-fichas/bin/python -m unittest discover -s tests/tagnt`.

El generador es intercambiable (`manual` o `api`/Batch): mismo esquema, validación y caché; la caché se separa por
(modelo, hash del prompt), por lo que cambiar de modelo o editar el prompt no mezcla resultados.
`traducciones_comparadas` no la genera el modelo: `ensamblar` la rellena con SpaRV (La Santa Biblia Reina-Valera (1909))
y TorresAmat (La Sagrada Biblia (Torres Amat)), ambas de dominio público, con el nombre exacto del módulo.

## Generar con la API (Batch)
```bash
export ANTHROPIC_API_KEY=...        # solo variable de entorno; el pipeline no la lee ni la escribe en archivos
.venv-fichas/bin/python tools/tagnt/pipeline.py batch-enviar John.1        # lote, claude-sonnet-5-5, prompt cacheado
.venv-fichas/bin/python tools/tagnt/pipeline.py batch-estado <id>
.venv-fichas/bin/python tools/tagnt/pipeline.py batch-traer <id> res.jsonl # valida, guarda en caché, registra tokens en logs/uso_api.jsonl
.venv-fichas/bin/python tools/tagnt/pipeline.py batch-enviar John.1 --reintento   # un reintento con el error del primero
.venv-fichas/bin/python tools/tagnt/comparar_manual_api.py                 # reports/comparacion_jn1_manual_vs_api.md
.venv-fichas/bin/python tools/tagnt/informe_costo.py --precio-ent <USD/MTok> --precio-sal <USD/MTok>
```
Jn 8:12–8:53 está excluido (defecto del módulo Tisch). `swordtext.cc` se compila solo la primera vez (necesita g++ y los headers de SWORD).
