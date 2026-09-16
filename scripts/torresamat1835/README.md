# Torres Amat 1832–1835 (`TorresAmat1835`) — fundación

Segunda edición, Madrid, Imprenta de D. Miguel de Burgos, 1832–1835, seis
tomos. Candidata a sustituir algún día al módulo `TorresAmat` que se
distribuye hoy (edición tardía del XIX). **Todavía no la sustituye.**

No hay ficheros fuente en el repositorio. La procedencia, los derechos y
los checksums viven en `data/torresamat1835/source_manifest.json`.

| Fichero | Qué hace |
|---|---|
| `fetch_source.py` | Adquisición reproducible: descarga a caché, verifica SHA-256, se para si la fuente cambia. No toca `~/.sword` |
| `model.py` | Modelo intermedio neutral: `Verse`, `CanonicalTitle`, `ChapterHeading`, `EditorialHeading`, … y `UnclassifiedBlock` |
| `parser.py` | Clasificador de bloques con el fail-safe |
| `osis_out.py` | OSIS determinista; sólo texto bíblico |
| `test_baseline.py` | La batería de la task |

## La regla que gobierna el parser

Reconocer **que** un bloque es una frontera es independiente de saber
**qué número** tiene.

El importador de la edición de 1882 las juntaba: exigía `SALMO` seguido de
un romano válido ≤ 150, y cuando el OCR rompía el numeral (`SALMO LIM`,
`CAPITULO XXVHulI`, `CAPITULO:`) la línea dejaba de ser frontera y caía al
cuerpo del versículo en curso. Así entraron 158 argumentos del editor en el
texto bíblico, entre ellos el de Sal 52:7.

Aquí un numeral ilegible produce un `ChapterHeading` con `number=None` y
`review_reason`: sigue siendo paratexto. Y lo que no se sabe clasificar
sale como `UnclassifiedBlock` a la cola de revisión, nunca pegado al
versículo anterior.

Medido sobre el tomo real (652 páginas): de 339 divisiones reconocidas,
**72 traen el numeral ilegible** y quedan visibles en vez de convertirse
en texto bíblico.

## Inscripciones canónicas

La Vulgata cuenta la inscripción del salmo como versículo. Se marca
`<seg type="x-psalm-title">` **dentro de su versículo nativo** — nunca
`<title type="psalm">` (SWORD lo sacaría como `<h3>`, el versículo se
leería como ausente y el visor lo supliría con otra Biblia) y nunca en un
versículo 0. Es la representación que fijó `e223d8d5`.

Qué versículo es inscripción no se deduce del texto: lo dice el testigo, y
entra por `parser.mark_canonical_title()`.

## Lo que falta

Ver el informe de la task. En resumen: la BNE tiene los seis tomos con
licencia buena (CC-BY) pero devuelve 403 a todo acceso automatizado;
Internet Archive sólo tiene un tomo de esta edición y bajo condiciones de
Google. Sin los seis tomos no hay corpus.
