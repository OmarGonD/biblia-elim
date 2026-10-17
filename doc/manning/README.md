# Manning ESP y Manning ENG — ediciones de estudio 0.2

Las dos ediciones están instaladas en el catálogo personal de módulos:

| Nombre | ID | Base instalada | Idioma | Versículos | Licencia heredada del módulo |
| --- | --- | --- | --- | ---: | --- |
| Manning ESP | ManningES | RV1909 (`SpaRV`) | español | 31.096 | Public Domain |
| Manning ENG | ManningEN | KJV | inglés | 31.102 | GPL |

Se abren en **Estudio → Mis Biblias de estudio**. Vuelve a abrir la aplicación
si estaba en ejecución. En el editor, el comentario de cada uno de los 81
pasajes documentados incluye la explicación y los enlaces completos. Debajo
aparecen enlaces pulsables; la pestaña **Ayudas editoriales** conserva también
las fuentes, propuesta, confianza y estado de revisión.

Los 81 pasajes y las fuentes de Manning coinciden entre idiomas; la redacción, el
historial y la aprobación son independientes. Cada edición tiene once
versículos revisados, tres propuestas sin aplicar, 66 comentarios de libro y
nueve entradas de glosario. Los demás versículos mantienen su base; no se les
atribuyen aportes inexistentes. Las seis referencias adicionales de KJV se
conservan sin inventar equivalencias con RV1909.

La antigua `BibliaManning.sqlite` se archivó íntegra fuera del catálogo en
`~/.local/share/biblia-elim/edition-backups/BibliaManning-before-bilingual-20261016.sqlite`.
Manning ESP conserva exactamente sus once registros de historial.
RV1909, KJV y Biblia OGD no cambiaron.

[editions.json](editions.json) identifica la familia y ambos planes reproducibles:
[español](editorial-plan-es.json) e [inglés](editorial-plan-en.json).
[verification-bilingual.json](verification-bilingual.json) registra las
comprobaciones instaladas. El plan y la verificación sin sufijo se conservan
como antecedentes de la versión española 0.1.

La inglesa se redactó contra KJV y se contrastaron los once pasajes revisados
con el módulo griego TR. Conserva la licencia GPL del módulo KJV instalado,
incluidos sus datos y procedencia. Todas las decisiones siguen pendientes de
revisión humana; el nombre del proyecto no implica participación de Manning.

## Resúmenes explicados en ambos idiomas (MANNING-108)

Los 81 aportes de cada edición (162 notas reescritas y verificadas) tienen
título, bajada, tres párrafos breves y fuentes al final, en su idioma. Explican
la pregunta o contexto, las pruebas concretas de Manning y el alcance de su
conclusión. Los cuerpos contienen 149–184 palabras en español y 126–169 en
inglés; no sustituyen los argumentos por una aplicación genérica ni requieren
que el lector conozca el artículo enlazado.

Por ejemplo, 1 Samuel 7:12 explica la derrota con el arca, la conducta de los
sacerdotes y el cambio de respuesta del pueblo antes de la victoria. Juan 19:30
explica los 315 recibos y las 17 formas completas que permiten distinguir dos
verbos; Génesis 1:1 localiza la partícula y compara otros usos. Las conexiones
literarias se presentan como propuestas, y las dificultades de Romanos 7,
Mateo 24 y 1 Timoteo 2 no se ocultan.

Los textos bíblicos, propuestas, estados de revisión, confianza, fuentes e
historial de versículos no cambian. `editorial_audit` conserva la justificación
inicial y `comment_review_history` archiva íntegramente las notas anteriores.
`note_title`, `note_lead` y `note_body` explicitan la estructura que el lector
presenta; `comment` y `rationale` contienen la misma nota autónoma.

Se consultaron los pasajes pertinentes de las 93 URL ya citadas (fuentes y
controles). Cambridge se consultó en su ficha, resumen y referencias accesibles,
sin afirmar acceso al texto completo de pago. Los artículos se resumen con
redacción propia; no se distribuyen sus textos completos.

[verification-summary-depth.json](verification-summary-depth.json) registra
los respaldos, controles de integridad, huellas de las fuentes consultadas y
comparación completa de datos antes/después. Los planes ES/EN reproducen las
notas instaladas desde RV1909/KJV.

[verification-comment-format.json](verification-comment-format.json) conserva
la verificación histórica de MANNING-107, anterior a esta ampliación.

## Antecedentes y alcance de la revisión española

Es una revisión española provisional asistida por herramientas automáticas,
basada en RV1909 y en estudios identificados de Gary Manning Jr. El nombre es
el solicitado para este proyecto: Manning no es su autor, traductor, revisor
ni avalista. No se presenta como una traducción completa nueva, una edición
académica aprobada o una fidelidad superior ya demostrada.

## Contenido verificado

| Elemento | Cantidad |
| --- | ---: |
| Libros completos de la base | 66 |
| Versículos | 31.096 |
| Versículos con redacción revisada | 11 |
| Versículos que conservan la base | 31.085 |
| Pasajes con decisión y comentario editorial | 81 |
| Alternativas guardadas sin cambiar el texto | 3 |
| Comentarios de libro con cobertura y procedencia | 66 |
| Entradas del glosario | 9 |

Los once cambios están en Mateo 6:22–23 y 20:15; Lucas 2:7;
Juan 3:16, 3:21, 4:9 y 19:30; Efesios 4:15; Filipenses 3:8;
y 2 Timoteo 2:15. Cada uno conserva el texto anterior en el historial,
la propuesta española, la justificación, las fuentes y la confianza asignada.
Los 81 registros quedan **pendientes de revisión humana**.

Las tres alternativas sin aplicar corresponden a Juan 1:5, Juan 3:3 y
Mateo 26:50. Se pueden comparar y editar en Ayudas editoriales. Las notas de
los otros pasajes incluyen decisiones de conservar una traducción ya adecuada,
variantes textuales y comentarios de contexto; una nota no significa que
fuera necesario cambiar el texto.

## Corpus y alcance real de la consulta

[sources.json](sources.json) inventaría 44 publicaciones del archivo de autor
de Biola y 204 entradas del blog personal Eutychus. Incluye además el artículo
académico sobre Juan 4:9, el estudio de BGU 1208, tres controles NET y cuatro
capítulos localizados en la bibliografía de Biola: 257 registros en total.

La consulta incluye el texto de 29 publicaciones de Biola y texto o extractos
de 79 entradas de Eutychus. El resto se ha cribado bibliográficamente. El campo
`review_level` distingue esas situaciones: **inventariado no significa leído
íntegramente ni incorporado como revisión de traducción**. El inventario
incluye noticias, fotografías, citas, ejercicios literarios, anuncios de audio
y reediciones. Estos materiales no aportan automáticamente una nueva decisión
léxica. Las reediciones y los enlaces a un mismo estudio no cuentan como
testimonios independientes.

No se afirma haber encontrado toda la producción del autor en Internet ni
haber leído capítulos de libros cuyo texto no se obtuvo. El artículo de
*New Testament Studies* se registra mediante su ficha/resumen y la síntesis
pública del propio autor; no se afirma consulta de su texto completo. Las
lagunas del papiro BGU 1208 se respetan y su discusión no decide por sí sola
la traducción de 1 Timoteo 2:12.

Las notas son redacción española original con referencias y enlaces. Los
artículos completos, sus ilustraciones y traducciones extensas no se incluyen
en el módulo. El índice general también se guarda en el comentario de Juan
para conservar la procedencia junto al proyecto.

## Criterios de traducción

- Confrontar cada modificación con el pasaje completo y con el griego
  disponible en los módulos TR y Tisch. Esos dos controles no equivalen a un
  cotejo exhaustivo de manuscritos ni a una nueva edición crítica.
- Distinguir significado léxico, variante manuscrita e interpretación. La
  identificación de una metáfora o una armonización cronológica se explica
  en una nota, sin convertirla en palabras añadidas al texto bíblico.
- Mantener visibles las opciones rivales. En Juan 4:9 se ensaya pedir ayuda;
  en Mateo 6:22–23 se ensaya generosidad/mezquindad. Ambas siguen sujetas a
  discusión. La nota NET de Mateo 6:22, por ejemplo, favorece sano.
- No cambiar globalmente palabras como gracia, amar, yo soy o generación por
  una equivalencia automática. Usos distintos pueden requerir soluciones
  diferentes. Una traducción existente puede ser adecuada.
- Mantener separadas redacción provisional y aprobación. Los niveles Alta,
  Media o Baja registran una evaluación editorial inicial, no una votación de
  especialistas ni una garantía de certeza.

## Cotejos que impiden copiar errores de las fuentes

El post de Eutychus sobre Juan 7:8 intercambia los testigos de no y no todavía.
La [nota textual NET](https://www.stepbible.org/?q=version%3DNET2full%7Creference%3DJohn.7.8)
atribuye no todavía a P66, P75 y B; y no a Sinaiticus, entre otros. Se conserva
el aún no de la base con el desacuerdo registrado. Este error del post no se
repite como evidencia del proyecto.

El artículo de 2016 sobre *The Message* rotula el texto del ojo como Mateo
6:20–21; sus referencias correctas son 6:22–23. El artículo de 2025 sobre
Simón cita Marcos 3:8 en un paralelo que corresponde a 3:18. El ejemplo del
Espíritu guiando a Jesús del artículo de 2026 corresponde a Lucas 4:1, no 4:10.
Las notas usan las referencias corregidas.

En Juan 4:9 el módulo TR conserva la explicación final, mientras Tisch local
la omite. Esta diferencia se registra como cuestión textual; la discusión de
συγχράομαι se aplica a la cláusula conservada en RV1909. No se utiliza para
afirmar que todas las ediciones griegas contienen idéntico texto.

## Reproducción y pruebas

El [plan editorial](editorial-plan.json) fija la huella del archivo fuente y
el texto esperado de cada pasaje. El creador rechaza una fuente distinta, una
referencia inexistente, acciones desconocidas o un destino que ya existe.
Trabaja en un directorio temporal vecino y publica el archivo completo solo
cuando todas las operaciones han terminado correctamente.

```sh
cmake -S . -B build
cmake --build build --target biblia-curated-edition editable_edition_test -j2
./build/src/backend/biblia-curated-edition \
  "$HOME/.local/share/biblia-elim/modules/SpaRV.sqlite" \
  /ruta/nueva/BibliaManning.sqlite doc/manning/editorial-plan.json
python tests/curated_edition_test.py build/src/backend/biblia-curated-edition
```

La ruta de salida debe ser nueva; el creador no actualiza ni reemplaza un
proyecto ya editado. El usuario puede seguir trabajando sobre el archivo
instalado mediante el editor habitual. Para otra instalación se puede usar el
archivo SQLite completo con el gestor de módulos.

[verification.json](verification.json) registra la comprobación de la copia
instalada: integridad SQLite, 66 libros y referencias idénticas a la base,
exactamente once diferencias de texto autorizadas por el plan, historial y
decisiones persistentes, búsqueda actualizada y fuentes originales intactas.

Se verificó además la apertura del módulo con `SqliteBibleBackend`, resolución
de Juan 4:9, lectura de Génesis, título y búsqueda por la frase revisada.

## Ampliación de Génesis 1:1 en Manning ESP

El comentario español explica las dos apariciones de אֵת, su función sintáctica,
su representación en español y el alcance del argumento de Manning contra una
identificación automática con Jesús. Se distingue esta lectura del análisis
editorial y de una exégesis completa de la creación. No cambia el texto bíblico.
Dos controles complementarios (BDB H853 e interlineal de Génesis 1:1, alojados en
Bible Hub) se añadieron solo a la nota española; las fuentes atribuidas a
Manning siguen compartidas con la inglesa. El inventario contiene ahora 259
registros, incluidos estos dos controles. Las verificaciones previas son
instantáneas históricas; [verification-genesis-note.json](verification-genesis-note.json)
registra la actualización española y su respaldo.

La nota visible de Génesis 1:1 se sintetizó después en título, bajada, dos
párrafos y fuente final. El plan ES guarda ese texto en `comment` y `rationale`;
el creador conserva comentarios personalizados literalmente. El vínculo de
Manning es la fuente visible; los controles gramaticales consultados permanecen
en el inventario como antecedentes de la preparación.

La nota de Génesis 1:1 aclara ahora que consultas de lectores sobre un video
motivaron la respuesta de Manning. El lector de comentarios prioriza título,
bajada y cuerpo; elimina la comparación si no cambió el versículo y deja
procedencia/estado de revisión en detalles desplegables. El texto bíblico se
conserva intacto.
