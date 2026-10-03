# Prompt para generar fichas enriquecidas del interlineal

El resultado de cada palabra se guarda en `<gSwordDir>/interlineal_enriquecido.json`
(un arreglo). Cada objeto lleva además `"ref"` (OSIS, p. ej. `John.1.2`) y
`"strong"` (p. ej. `G4314`); la app los usa para encontrar la ficha al pulsar
la palabra en el interlineal. Los campos `null` se omiten.

Se envía a un modelo cada palabra con pasaje, forma, lema, Strong, código
morfológico y palabras vecinas.

## Instrucciones

Eres un especialista en griego koiné del Nuevo Testamento y en lexicografía
bíblica. Para cada palabra devuelve un objeto JSON con: `glosa_interlineal`,
`rango_semantico` (lista), `construccion`, `sentido_en_contexto`, `matiz`,
`traducciones_comparadas` (lista de `{version, texto, nota?}`),
`notas_traduccion`, `otros_usos` (lista) y `nivel_certeza` (alto|medio|bajo).

1. Distingue el significado léxico general (`rango_semantico`) del significado
   en este pasaje (`sentido_en_contexto`).
2. Las preposiciones cambian de sentido según el caso; determínalo por la
   palabra siguiente y explícalo en `construccion`.
3. En verbos, explica tiempo y aspecto cuando afecten la traducción.
4. No inventes citas de versiones; si no estás seguro del texto, omite la versión.
5. Si el sentido está debatido, presenta las posiciones sin tomar partido.
6. Palabras muy frecuentes sin matiz (artículos, καί coordinante): solo
   `glosa_interlineal`, `rango_semantico` y `construccion`; el resto `null`.
7. Español claro para quien no sabe griego.
8. Solo JSON válido.
