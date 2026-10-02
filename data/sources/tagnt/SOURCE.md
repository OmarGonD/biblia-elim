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

## Cambios respecto al TAGNT
Registro de correcciones o normalizaciones del CONTENIDO del TAGNT (lema, Strong, morfología, glosa, ediciones). No se anotan cambios de formato ni de Unicode que no alteren el contenido.

| Fecha | Palabra | Campo | TAGNT | Aquí | Motivo |
|---|---|---|---|---|---|
| (ninguno hasta ahora) | | | | | |

Nota: el parser pasa las formas griegas y los lemas a NFC (solo forma de codificación; el contenido no cambia) y quita los ceros del Strong (G0976 → G976) en el campo `strong`; el campo `dstrong` conserva el valor original.

## Glosa en español: no se usa
La columna en español del TAGNT y la traducción literal de OpenGNT proceden de la traducción de E. Barrientos para el proyecto Galeed
(módulo e-Sword 2017, Biblioteca Hispana). OpenGNT está bajo CC BY-SA 4.0, pero no declara la licencia de origen de esa traducción.
Por eso **no se guarda ni se distribuye** (no hay campo `glosa_opengnt`); solo puede consultarse como referencia al escribir las fichas, sin copiarla.

## Versiones de comparación (`traducciones_comparadas`) y numeración
Solo dos versiones de dominio público, con el nombre exacto del módulo: «La Santa Biblia Reina-Valera (1909)» (SpaRV) y «La Sagrada Biblia (Torres Amat)» (TorresAmat).
No se usan SpaRV1909 (con Strong), SpaRVG ni NacarColunga. Nunca RVR1960.

- **Texto:** lo entrega la biblioteca SWORD con su filtro de texto plano (sin notas) mediante `tools/tagnt/swordtext.cc`; no hay limpieza propia.
- **Numeración:** `ref` (clave de la ficha) es la del módulo Tisch; `ref_estandar` es la estándar (KJV). Las citas se buscan **siempre** con `ref_estandar`.
  El módulo Tisch numera distinto en 40 versículos del NT (por ejemplo Jn 1:39–51, que en la numeración estándar son 1:38–50 y el último, 1:50-51).
  SpaRV usa KJV, pero **TorresAmat usa la numeración de la Vulgata**, que difiere de la estándar en 11 capítulos del NT (Mt 17, Mc 4, 8 y 9, Jn 6, Hch 7, 14 y 19, 2 Co 13, 3 Jn 1, Ap 12);
  `swordtext` la convierte a KJV con el mapeo de SWORD (`mod2imp` y `diatheke` no lo hacen). Se comprobaron a mano Jn 1:38–51 (`reports/verificacion_ref_estandar_jn1.md`).
- **Criterio de OCR (Torres Amat):** el módulo trae errores de OCR dentro del texto (símbolos sueltos como `$`, `%`, `/`, `=`, `+`, `£`, y letras aisladas que no son palabras).
  Un versículo con alguno de esos restos se considera marcado y **TorresAmat se omite en él: queda solo la Reina-Valera 1909**.
  Alcance medido (NT, numeración estándar): 481 de 7 721 versículos con texto de Torres Amat quedan marcados (**6,2 %**). La detección no ve erratas de letras (p. ej. «hautizando»), así que algunos versículos no marcados conservan errores menores.
