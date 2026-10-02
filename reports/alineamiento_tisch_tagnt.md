# Alineamiento Tisch → TAGNT (informe previo a la migración)

Generado por `tools/tagnt/alinear.py` → `data/tagnt_alineacion/tisch_a_tagnt.json`
(ref OSIS de Tisch → `[pos_tisch, "ref#pos" TAGNT | null, calidad]`; solo posiciones, sin texto del TAGNT).

## Resultado (NT completo, 137 098 palabras de Tisch)

| Calidad | Palabras | % |
|---|---|---|
| `forma` (misma forma y Strong compatible) | 132 389 | 96,6 |
| `forma_otro_strong` (misma forma, Strong distinto; sobre todo ἐγώ/ἡμεῖς/ὑμεῖς, εἶδον/οἶδα, εἰμί) | 2 542 | 1,9 |
| `strong` (Strong igual, forma distinta: ἀλλά/ἀλλ᾽, grafías, otra lectura) | 1 428 | 1,0 |
| **sin pareja (null)** | **739** | **0,54** |
| (de las emparejadas) la palabra TAGNT no está en NA28 | 384 | 0,28 |

Pasada 2 (versículos desplazados) recuperó 1 097 palabras.

## Efecto sobre las 4 166 fichas (clave = ref + pos. Tisch + Strong)
- 3 728 ficha → una sola palabra de Tisch con ese Strong en el versículo, pareja `forma`: migración segura.
- 63 pareja `strong` y 9 `forma_otro_strong`: migran, revisar la forma.
- 4 con palabra no presente en NA28 (Hch 8:31, Lc 6:1, Lc 24:53, Mc 9:49).
- **345 con varias ocurrencias del mismo Strong en el versículo** (p. ej. 1Co 2:13 G4152): la posición exacta no se deduce del Strong; hay que decidirla con la forma de la ficha. Pendiente: lista de revisión.
- **17 sin pareja** (1Pe 2:7 G4073, Hch 12:1 G2264, Hch 14:8 G102, Gál 2:20 G4957, Heb 11:37 G4249, Stg 3:8 G1150, Jn 6:51 G4561, Jn 7:52 G4396, Jn 8:4 G1598, Jn 8:5 G3034, Lc 21:2 G3016, Mt 5:5 G3996/G3870, Mt 8:10 G4102…): la ficha migra con TAGNT = null y marca de revisión.

## Ejemplos de palabras sin pareja
Lecturas distintas entre Tisch y el TAGNT, no errores del alineamiento:
- 1Co 2:2 `εἰδέναι` (G1492); 1Co 4:17 `αὐτὸ`; 1Co 8:8 `περισσεύομεν`, `μὴ`, `ὑστερούμεθα`.
- 1Co 15:12 `ἐγήγερται`; 1Jn 2:13 (varias palabras, orden distinto del TAGNT: pareja en 2:14).
- 1Pe 2:7 `καὶ λίθος προσκόμματος καὶ πέτρα σκανδάλου` (otra construcción).

## Versificación
- 45 versículos de Tisch tienen palabras emparejadas con otro versículo del TAGNT (1Jn 2:13↔2:14, 1Ts 2:6↔2:7, Jn 1:39-40, 2Co 5:15, 8:13, 11:8, Hch 3:19, 13:32, 24:3…). La pareja guarda la ref TAGNT real.
- **Anomalía del módulo Tisch:** `Jn 8:53` contiene 374 palabras (el texto de Jn 7:53 en adelante, concentrado en un solo versículo) y no existe `Jn 8:12`. 101 palabras de ese bloque quedan sin pareja. No es un fallo del alineamiento; las fichas de Jn 7:53-8:11 hay que revisarlas aparte.

## Qué falta decidir antes de migrar
1. Aprobar el criterio para las 345 fichas con varias ocurrencias (propongo: desempatar por la forma de la ficha y, si sigue ambiguo, marcar para revisión).
2. Qué hacer con el bloque Jn 8:53.
