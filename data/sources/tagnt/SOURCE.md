# TAGNT — Translators Amalgamated Greek NT

- Fuente: https://github.com/STEPBible/STEPBible-Data, carpeta `Translators Amalgamated OT+NT`
- Commit: `0f60797c170f11a1f8dc75c5f7617973e2e66b0d` (2025-09-02). Descargado el 2026-10-02 con `tools/tagnt/fetch_tagnt.sh`.
- Licencia: CC BY 4.0. Datos de STEPBible.org (Tyndale House, Cambridge). Atribución obligatoria en README y en la app (pendiente en la app).
- Los archivos crudos NO se commitean: el encabezado pide no redistribuirlos. Se obtienen con el script.

## Columnas (TSV; fila de palabra = `Ref#NN=Tipo`)
0 Ref+posición+tipo (`Jhn.1.1#12=NKO`; versificación NRSV, alterna entre `[ ]` KJV, `( )` NA, `{ }` otros) ·
1 Griego (translit) · 2 Inglés (BSB) · 3 dStrong=Gramática (`G2424G=N-GSM-P`) · 4 Lema=Glosa ·
5 Ediciones (NA28, NA27, Tyn, SBL, WH, Treg, TR, Byz; a veces NIV, KJV, manuscritos, versiones) ·
6 Variantes de significado · 7 Variantes de ortografía · 8 Español (Marvel Bible Project) · 9 Sub-significado ·
10 Palabra enlazada (`#04»05:G3056`) · 11 sStrong+instancia (`G5207_A`) · 12 Strong alternos · 13 Nota de variante (`^`, `v`).
Tipos: N=NA, K=TR/KJV, O=otros; minúscula = diferencia menor.

## Hallazgos que afectan el plan
- Hay 142 096 filas (incluye variantes); posición `#NN` única por versículo.
- Sí trae traducción al español (col. 8) y ediciones por palabra.
- **No incluye Tischendorf.** Tisch sigue como módulo SWORD aparte.
- La gramática de preposiciones es `PREP` a secas: el caso regido hay que derivarlo (Fase 2).
- Mc 16:9-20: tipo `KO`, sin SBL. Formas griegas en NFC tras el parser (el crudo mezcla composición).
- La columna de ediciones puede traer fuentes que no son ediciones (`otras_fuentes`).

## Columna de ediciones (parser)
`clasificar_fuentes` la separa en tres listas:
- `ediciones`: NA28, NA27, Tyn, SBL, WH, Treg, TR, Byz (texto griego). Solo estas cuentan para `variante` (no están en las 8).
  `TR»1` o `Byz«3` significa que la palabra está en esa edición pero desplazada (`ediciones_desplazadas`).
- `traducciones`: NIV, KJV.
- `manuscritos`: testigos (01, 02, 03, 04, 05, 06, 032, P66, P66*) y versiones antiguas (Coptic, Latin, Syriac).
