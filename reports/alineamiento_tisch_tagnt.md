# Alineamiento Tisch → TAGNT y migración de fichas (cifras finales)

Generado por `tools/tagnt/alinear.py` y `tools/tagnt/migrar_fichas.py`.
Alineamiento: `data/tagnt_alineacion/tisch_a_tagnt.json` (ref OSIS de Tisch → `[pos_tisch, "ref#pos" TAGNT | null, calidad]`, solo posiciones).
Fichas migradas: `data/fichas_v2/` (las viejas en `data/fichas_interlineal/` no se tocan hasta aprobar el piloto).
Listas de revisión completas: `reports/migracion_fichas.json`.

## Alineamiento (NT completo, 137,098 palabras de Tisch)
| Calidad | Palabras | % |
|---|---|---|
| `forma` (misma forma, Strong compatible) | 132,748 | 96.8 |
| `forma_otro_strong` (ἐγώ/ἡμεῖς/ὑμεῖς, εἶδον/οἶδα, εἰμί) | 2,562 | 1.9 |
| `strong` (Strong igual, forma distinta) | 1,466 | 1.1 |
| **sin pareja (null)** | **322** | **0.23** |
| (incluidas arriba) reordenadas dentro del capítulo, ±3 versículos | 451 | |
| (incluidas arriba) palabra TAGNT ausente en NA28 | 393 | |

Pasadas: (1) por versículo; (2) versículos con <70 % de parejas, contra las palabras TAGNT libres del mismo capítulo, con tope de ±3 versículos
(salvo el bloque anómalo de Jn 8:53, ver `data/sources/NOTAS_TISCH.md`); (3) reordenamientos dentro del capítulo sin exigir orden (Mt 5:4↔5:5, etc.).
De las 322 sin pareja, 101 son la copia duplicada de la perícopa en Jn 8:53 (defecto del módulo).

## Migración de las 4 166 fichas → 4,327 fichas
- Clave nueva: ref OSIS + `pos_tisch` + Strong; más `tagnt` (`ref#pos`), morfología TAGNT, `dstrong`/`strong_tagnt`, `ediciones`, `ausente_en`, `variante`, `caso_regido` (preposiciones).
- `revisar_ocurrencia`: **345 fichas** (199 asignadas a una sola aparición por su forma; 146 copiadas a cada posición coincidente). 161 copias añadidas en total.
- Sin pareja TAGNT: **2** (antes 17): John.8.4|G1598 pos 3 (εκπειραζοντες), John.8.5|G3034 pos 9 (λιθαζειν·).
- Perícopa de la adúltera (variante textual, NA28 entre dobles corchetes): 5 fichas.
- Palabra de Tisch no presente en NA28: 4 fichas (Acts.8.31|G3594 pos 11, Luke.6.1|G1207 pos 5, Luke.24.53|G134 pos 7, Mark.9.49|G233 pos 4).
- Mismas parejas con forma o Strong distinto, a revisar la forma: 80.
- Fichas con palabra no presente en las 8 ediciones (flag `variante`, fuera de la perícopa): 178.
- Preposiciones con `caso_regido` ambiguo (no se adivina; caso nulo): 4 fichas.
- `traducciones_comparadas` vaciado en todas (3 889 fichas tenían citas escritas de memoria). Se rellenará en la Fase 3 desde SpaRV (Reina-Valera 1909) y TorresAmat, con el nombre exacto de la versión.
