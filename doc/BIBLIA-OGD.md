# Biblia OGD — protocolo y plan de trabajo

## Propósito

Biblia OGD es una revisión de estudio basada en la Reina-Valera 1909 (RV1909).
Su objetivo es expresar en español actual el sentido más probable de los textos
hebreos, arameos y griegos, sin ocultar variantes textuales ni convertir una
interpretación teológica en una traducción literal. «Lo que el autor quiso
decir» se tratará como una hipótesis histórica y lingüística sustentada por
evidencias, no como algo que un algoritmo pueda recuperar con certeza absoluta.

La RV1909 original queda congelada como fuente. Cada cambio de OGD debe poder
compararse con ella y debe llevar una razón editorial, evidencia y estado de
revisión. Ninguna sugerencia automática se incorpora directamente al texto.

## 1. Fuente y licencia

- Fuente: módulo SWORD `SpaRV`, «La Santa Biblia Reina-Valera (1909)».
- Versificación declarada: KJV (`kjv`). Se conserva como referencia de la
  edición derivada; otros sistemas se alinean únicamente mediante mapeos
  comprobados, nunca por igualdad numérica asumida.
- La ficha local de SWORD declara `Public Domain` y enlaza la transcripción de
  Wikisource; CrossWire también lista SpaRV como Public Domain. Se conservarán
  la atribución, la ficha de procedencia, la versión del módulo y la huella
  SHA-256 de la copia fuente.
- La declaración de dominio público del módulo no se extiende automáticamente
  a todos los recursos que se consulten durante la revisión. Los textos,
  aparatos, léxicos y datos de morfología tendrán registros de licencia y
  atribución independientes antes de integrarlos o redistribuirlos.

## 2. Textos de referencia y licencia de datos

Para el Antiguo Testamento se toma el texto masorético como referencia primaria
de traducción. El módulo WLC local se declara dominio público y representa el
texto de Leningrado; OSHB ofrece etiquetado morfológico bajo CC BY 4.0. Se
registrarán por separado las lecturas de la Septuaginta y otros testigos cuando
se disponga de una fuente licenciada y alineada. WLC (versificación `leningrad`)
y OSHB (`mt`) no se tratarán como si compartieran automáticamente la
versificación `kjv` de SpaRV.

Para el Nuevo Testamento se propone usar SBLGNT como texto griego de trabajo,
junto con el texto de Tischendorf instalado y otras lecturas documentadas para
identificar variantes. SBLGNT tiene licencia CC BY 4.0 y condiciones expresas
de atribución; cualquier texto griego o aparato redistribuido debe respetar su
licencia. El módulo local TR declara CC BY-NC-SA 4.0, por lo que no se incluirá
en paquetes distribuibles sin una revisión de compatibilidad y atribuciones.

Los datos de STEP Bible están bajo CC BY 4.0 y son útiles para morfología y
análisis léxico. Antes de reutilizar campos concretos se comprobará su
procedencia; una licencia de repositorio no autoriza a borrar atribuciones ni
a tratar como propio un léxico o comentario de terceros.

## 3. Ficha de decisión por pasaje

Cada propuesta de cambio guardará, como mínimo:

1. Referencia en OGD y referencia equivalente en cada fuente comparada.
2. Texto RV1909 y texto candidato; ambos permanecen visibles.
3. Texto original con lema, morfología, sintaxis y límites de alineación.
4. Contexto literario e histórico, usos paralelos y variantes textuales.
5. Opciones de traducción, razón de preferencia, incertidumbre y fuentes.
6. Estado: propuesta, revisada, aceptada o descartada; responsable y fecha.

La IA puede generar alternativas, buscar inconsistencias terminológicas,
resumir evidencia licenciada y marcar casos dudosos. No decide la lectura base,
no inventa fuentes, no rellena silenciosamente versículos vacíos y no convierte
una posibilidad semántica en el texto final sin revisión humana.

## 4. Piloto: Juan 19:28–30

**Base RV1909, Juan 19:30:** «Consumado es». **Original:** τετέλεσται
(*tetélestai*), forma perfecta de τελέω (*teleō*). Las glosas directas
pertinentes incluyen «ha terminado», «se ha completado» y «se ha cumplido».

La propuesta «la deuda ha sido pagada» no se adopta como traducción: «deuda» no
está expresada en el griego de este versículo. El argumento popular sobre
recibos confunde τετέλεσται con la abreviatura τετελ de recibos de impuestos,
que se desarrolla como τετελώνηται (*pagado como impuesto*). La revisión de los
papiros no confirma que τετέλεσται fuera un sello habitual de «deuda pagada».
Además, Juan 19:28 usa la misma forma antes de hablar del cumplimiento de la
Escritura, creando un vínculo contextual entre completar la obra y cumplirla.

**Decisión provisional:** conservar «Consumado es» en la copia inicial y
anotar que «Está cumplido» o «Todo ha sido completado» son alternativas para
evaluar por claridad y registro, no correcciones demostradas. «La deuda ha sido
pagada» puede aparecer en una nota teológica si el equipo desea explicar esa
doctrina, siempre identificada como interpretación y no como glosa léxica.

Evidencia consultada:

- Gary Manning Jr., profesor de lenguas y literatura del Nuevo Testamento,
  revisa 315 recibos de impuestos y explica la confusión entre las formas
  griegas: <https://www.biola.edu/blogs/good-book-blog/2022/paid-in-full-the-meaning-of-tetelestai-in-jesus-final-words>.
- Las notas de traducción NET/STEP ofrecen «completed», «accomplished»,
  «finished» o «ended» y señalan el juego verbal de Juan 19:28–30:
  <https://www.stepbible.org/?q=version%3DNET2full%7Creference%3DJohn.19.28-John.19.30>.
- Floyd O. Parker, «Is the Subject of τετέλεσται in John 19,30 “It” or “All
  Things”?», *Biblica* 96.2 (2015), pp. 222–244:
  <https://doi.org/10.2143/BIB.96.2.3183419>.

## 5. Control de calidad

- Comparación automática de cada versículo con RV1909: detectar omisiones,
  duplicaciones y cambios no registrados.
- Consistencia de nombres propios, términos teológicos y traducciones de lemas
  recurrentes; las alternativas deliberadas se anotan.
- Alertas para versículos ausentes, texto alterado sin justificación,
  desajustes de versificación y decisiones pendientes.
- Revisión lingüística del español y revisión humana independiente de cambios
  sustantivos; la retrotraducción automática sirve como alarma, nunca como
  prueba de fidelidad.
- Informe por libro con cobertura, variantes abiertas, cambios aceptados y
  atribuciones de datos utilizados.

## 6. Colaboración y distribución

La Biblia OGD se trabaja como proyecto completo independiente. Se conservará
una copia de base inmutable y se distribuirán exportaciones identificadas por
versión, huella, fuentes y avisos. Los cambios concurrentes en un mismo
versículo deben producir un conflicto revisable, no una sobrescritura silenciosa.
La licencia final se decidirá después de inventariar todos los datos y
contribuciones incorporados; tendrá que permitir compartir las adaptaciones y
mantener las atribuciones requeridas por cada fuente.

## Secuencia de ejecución

1. Crear Biblia OGD desde SpaRV y conservar el texto inicial sin cambios.
2. Completar el registro de procedencia/licencia de cada recurso original y
   fijar las ediciones textuales base antes de revisar contenido.
3. Probar en Juan 19:28–30 el registro de evidencia, alternativas y decisión.
4. Ejecutar los controles automáticos y revisar manualmente el piloto.
5. Revisar por libros en lotes pequeños, usando estados de aprobación y
   revisión independiente.
6. Implementar el paquete de colaboración/exportación y validar que incluye
   textos, comentarios, historial, huellas y atribuciones.

La copia OGD se inicializa con el texto de SpaRV sin editar. El piloto anterior
es una decisión documentada, no un cambio aplicado al versículo.

## Estado inicial (2026-10-16)

- Proyecto instalado en el catálogo personal como `BibliaOGD.sqlite`, título
  «Biblia OGD». Es una copia SQLite v1 completa de `SpaRV`; la aplicación la
  reconocerá al actualizar su lista de módulos o en el siguiente arranque.
- La huella SHA-256 del archivo fuente al crearla fue
  `2421dc3a5c676e8fca70809da37c9afa2ee6ec45a65adbcb3a491d6e939db9b5`.
  La misma huella se obtuvo después de la copia: SpaRV no se modificó.
- Comprobación de igualdad: 66 libros y 31.096 filas de versículos; las filas
  de libros y versículos de la copia coinciden exactamente con SpaRV. Las
  anotaciones técnicas presentes también se copiaron (3 filas de palabras y
  3.501 spans). El esquema continúa con `user_version=1`.
- El comentario piloto de Juan 19:30 quedó guardado en Biblia OGD. El texto
  completo del versículo permanece idéntico a SpaRV e incluye «Consumado es».
- El editor conserva un historial de redacción por versículo, permite registrar
  propuestas, fundamento, fuentes/evidencia y estado pendiente, y mantiene un
  glosario por proyecto. Muestra el texto base, Strong cuando la Biblia fuente
  ya incluye alineación y una comparación con módulos SQLite de igual
  versificación.
- La exportación colaborativa con conciliación de cambios concurrentes
  (EDITION-103) y las alertas lingüísticas automáticas de omisión aún requieren
  trabajo. El editor avisa si cambia el texto sin documentar fuentes/evidencia;
  esto no declara fiel una traducción ni sustituye la revisión humana.
