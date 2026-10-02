# Piloto Jn 1 — fichas generadas por el generador manual

Generado por `tools/tagnt/informe_piloto.py` a partir de `data/fichas_v3/John.01.json` (modelo `manual-claude-sonnet-5-5`, prompt `a63f498c6441`). Las 821 fichas las escribió el asistente a mano, versículo por versículo; cada una pasó el esquema JSON y las reglas semánticas de `tools/tagnt/pipeline.py`. Las fichas viejas (`data/fichas_interlineal/`) y los datos de Tischendorf no se han tocado.

## 1. Cifras

| | |
|---|---|
| Versículos | 51 (todo Jn 1, versificación del módulo Tisch) |
| Fichas | 821 (una por posición de Tisch) |
| Palabras básicas (artículo, καί, δέ, γάρ) | 160 (19.5 %) |
| Palabras completas | 661 |
| Certeza | alto 785 · medio 36 · bajo 0 |
| Alineación Tisch→TAGNT | `forma` 787 · `forma_otro_strong` 23 · `strong` 11 · sin pareja 0 |
| Palabras con variante del TAGNT o fuera de NA28 | 18 |
| Lecturas de Tisch distintas del TAGNT (`lectura_tagnt`) | 5 |
| Preposiciones con `caso_regido` | 69 resueltas, 2 ambiguas |

Validación: 51/51 versículos pasan esquema y reglas. Durante la escritura el validador rechazó varios intentos míos (`variantes_textuales` mal situado en 3 versículos, más errores de sintaxis de mis propios archivos); se corrigieron antes de guardar y no quedó ningún versículo con error. Después de validar corregí a mano 3 datos de contenido que detecté al releer (una lista de ediciones de 1:26, dos erratas); eso no lo detecta el validador.

## 2. Fichas pedidas

### 2.1 πρὸς (Jn 1:1-2)

#### Jn 1:1 · pos. 10 · πρὸς
*lema πρός · G4314 · morf. TAGNT PREP · alineación `forma` · TAGNT `John.1.1#10` · rige **acusativo***

- **Glosa:** con / junto a
- **Rango semántico:** hacia (movimiento); junto a, con; en relación con; contra; para
- **Construcción:** Preposición con acusativo (πρὸς τὸν θεόν). Con acusativo y verbo de estado no indica movimiento sino relación o compañía personal.
- **Sentido en contexto:** Expresa una relación personal y distinta: el Verbo está orientado hacia Dios, cara a cara, sin identificarse con él en persona. Con ἐν («en Dios») se habría perdido esa distinción.
- **Matiz:** Con verbo de estado, πρός más acusativo subraya relación activa, no simple cercanía física.
- **Notas de traducción:** El español «con» no recoge la orientación mutua de πρός; «junto a» o «cara a cara con» la sugieren mejor.
- **Otros usos:** 1 Jn 1:2 — la vida eterna estaba πρὸς τὸν πατέρα · Mc 6:3 — las hermanas de Jesús están πρὸς ἡμᾶς (aquí, con nosotros) · Jn 1:2 — repite la afirmación
- **Certeza:** alto

#### Jn 1:2 · pos. 5 · πρὸς
*lema πρός · G4314 · morf. TAGNT PREP · alineación `forma` · TAGNT `John.1.2#05` · rige **acusativo***

- **Glosa:** con / junto a
- **Rango semántico:** hacia (movimiento); junto a, con; en relación con; contra; para
- **Construcción:** Preposición con acusativo (πρὸς τὸν θεόν); con verbo de estado indica relación o compañía personal.
- **Sentido en contexto:** Repite la relación del Verbo con Dios sin añadir nada nuevo: es una conclusión que resume 1:1b.
- **Matiz:** Mismo uso que en 1:1b; aquí la frase queda sin el artículo de ὁ λόγος porque ya se dijo quién es.
- **Otros usos:** Jn 1:1 — misma construcción · 1 Jn 1:2 — la vida estaba πρὸς τὸν πατέρα
- **Certeza:** alto

### 2.2 ἦν (Jn 1:1, tres apariciones)

#### Jn 1:1 · pos. 3 · ἦν
*lema εἰμί · G1510 · morf. TAGNT V-IAI-3S · alineación `forma` · TAGNT `John.1.1#03`*

- **Glosa:** era
- **Rango semántico:** ser, existir; estar; haber
- **Construcción:** Imperfecto indicativo de εἰμί, 3.ª singular. El imperfecto de «ser» expresa estado continuo sin comienzo marcado.
- **Sentido en contexto:** Afirma que el Verbo ya existía en el principio. No dice que fue hecho ni que comenzó; contrasta con ἐγένετο («llegó a ser») en 1:3 y 1:14.
- **Otros usos:** Jn 8:58 — presente εἰμί frente a γενέσθαι de Abrahán · Jn 1:3 — ἐγένετο para lo creado · Jn 1:14 — ἐγένετο para la encarnación
- **Certeza:** alto

#### Jn 1:1 · pos. 9 · ἦν
*lema εἰμί · G1510 · morf. TAGNT V-IAI-3S · alineación `forma` · TAGNT `John.1.1#09`*

- **Glosa:** era
- **Rango semántico:** ser, existir; estar; haber
- **Construcción:** Imperfecto de εἰμί, 3.ª singular, con complemento de relación (πρὸς τὸν θεόν).
- **Sentido en contexto:** Repite que el Verbo existía, ahora en relación con Dios: de «ser» absoluto (v. 1a) a «estar con» (v. 1b).
- **Certeza:** alto

#### Jn 1:1 · pos. 15 · ἦν
*lema εἰμί · G1510 · morf. TAGNT V-IAI-3S · alineación `forma` · TAGNT `John.1.1#15`*

- **Glosa:** era
- **Rango semántico:** ser, existir; estar; haber
- **Construcción:** Imperfecto de εἰμί, 3.ª singular; verbo copulativo entre el predicado θεός y el sujeto ὁ λόγος.
- **Sentido en contexto:** Tercera repetición con valor distinto: existir (1a), estar con (1b), ser (1c). Cada cláusula añade algo que la anterior no decía.
- **Certeza:** alto

### 2.3 λόγος (Jn 1:1, tres apariciones)

#### Jn 1:1 · pos. 5 · λόγος
*lema λόγος · G3056 · morf. TAGNT N-NSM · alineación `forma` · TAGNT `John.1.1#05`*

- **Glosa:** Verbo / Palabra
- **Rango semántico:** palabra, expresión; razón, cuenta; mensaje, enseñanza; Palabra personal de Dios
- **Construcción:** Nominativo singular masculino con artículo; sujeto de ἦν. El artículo lo marca como sujeto frente al predicado θεός sin artículo (v. 1c).
- **Sentido en contexto:** Primera aparición de ὁ λόγος en el Evangelio: aún no se dice quién es (se identificará en 1:14). Reúne ecos de la palabra con que Dios crea (Gn 1), de la Sabiduría personificada y del uso de «palabra» para la autocomunicación de Dios, sin que el texto indique cuál predomina.
- **Matiz:** «Verbo» (del latín verbum) subraya la persona; «Palabra» conserva la idea de comunicación. Ninguno agota el término.
- **Otros usos:** Gn 1:3 (LXX) — Dios crea hablando · Pr 8:22-31 — Sabiduría junto a Dios en el principio · Ap 19:13 — ὁ λόγος τοῦ θεοῦ aplicado a Cristo
- **Certeza:** alto

#### Jn 1:1 · pos. 8 · λόγος
*lema λόγος · G3056 · morf. TAGNT N-NSM · alineación `forma` · TAGNT `John.1.1#08`*

- **Glosa:** Verbo / Palabra
- **Rango semántico:** palabra, expresión; razón, cuenta; mensaje, enseñanza; Palabra personal de Dios
- **Construcción:** Nominativo con artículo, sujeto de ἦν en la segunda cláusula.
- **Sentido en contexto:** La repetición encadena las frases (principio → Verbo → Dios → Verbo) y reafirma el sujeto antes de decir dónde estaba: «con Dios».
- **Certeza:** alto

#### Jn 1:1 · pos. 17 · λόγος
*lema λόγος · G3056 · morf. TAGNT N-NSM · alineación `forma` · TAGNT `John.1.1#17`*

- **Glosa:** Verbo / Palabra
- **Rango semántico:** palabra, expresión; razón, cuenta; mensaje, enseñanza; Palabra personal de Dios
- **Construcción:** Nominativo con artículo, sujeto de ἦν; el artículo lo distingue del predicado sin artículo θεός.
- **Sentido en contexto:** Cierra el círculo del versículo: el sujeto es el Verbo, y de él se ha dicho que existía, que estaba con Dios y que era θεός.
- **Certeza:** alto

### 2.4 θεός (Jn 1:1, con y sin artículo)

Texto de Tisch: *Ἐν ἀρχῇ ἦν ὁ λόγος, καὶ ὁ λόγος ἦν πρὸς τὸν θεόν, καὶ θεὸς ἦν ὁ λόγος.*

#### Jn 1:1 · pos. 12 · θεόν
*lema θεός · G2316 · morf. TAGNT N-ASM-T · alineación `forma` · TAGNT `John.1.1#12`*

- **Glosa:** Dios
- **Rango semántico:** Dios (el único Dios); dios, divinidad; ser divino
- **Construcción:** Acusativo singular masculino con artículo (τὸν θεόν), objeto de πρός. En el NT, ὁ θεός suele designar a Dios como persona definida, normalmente al Padre.
- **Sentido en contexto:** Dios mismo, con quien está el Verbo. El artículo marca aquí identidad personal; en la cláusula siguiente θεός sin artículo se predica del Verbo, y la diferencia entre ambos usos es parte de lo que el pasaje afirma.
- **Matiz:** Contraste con θεὸς ἦν (v. 1c): con artículo, el «Dios» a quien el Verbo se dirige; sin artículo, la categoría o naturaleza que se le atribuye. Esta regla no es absoluta: Juan usa θεός sin artículo para Dios en 1:6, 1:13 y 1:18.
- **Notas de traducción:** El español no puede marcar la diferencia artículo/sin artículo; se traduce «Dios» en ambos casos y el contraste se pierde.
- **Otros usos:** Jn 1:18 — θεὸν οὐδεὶς ἑώρακεν (sin artículo, referido a Dios) · Jn 17:3 — ὁ μόνος ἀληθινὸς θεός · 1 Co 8:6 — εἷς θεὸς ὁ πατήρ
- **Certeza:** alto

#### Jn 1:1 · pos. 14 · θεὸς
*lema θεός · G2316 · morf. TAGNT N-NSM-T · alineación `forma` · TAGNT `John.1.1#14`*

- **Glosa:** Dios / divino
- **Rango semántico:** Dios; dios, deidad; de naturaleza divina
- **Construcción:** Nominativo singular masculino sin artículo; predicado nominal de ἦν, colocado antes del verbo y del sujeto (θεὸς ἦν ὁ λόγος). El predicado antepuesto y sin artículo es enfático y suele ser cualitativo o categorial; por sí solo no decide entre las lecturas posibles.
- **Sentido en contexto:** Atribuye al Verbo la condición de θεός sin identificarlo con ὁ θεός de la cláusula anterior. Se leen tres cosas: (1) identidad o predicado definido: «el Verbo era Dios» (se apoya en que un predicado definido antepuesto suele omitir el artículo, regla de Colwell); (2) predicado cualitativo: el Verbo tenía la naturaleza o condición de Dios, «era divino» en sentido pleno; (3) predicado indefinido: «era un dios», que defienden algunos traductores por la falta de artículo y que otros rechazan por el contexto monoteísta del Evangelio. Cada posición se apoya en la gramática y en la teología de Juan en conjunto (1:18; 20:28), no solo en el artículo.
- **Matiz:** Ningún orden de palabras ni artículo resuelve la cuestión por sí solo. El artículo de ὁ λόγος sí marca cuál es el sujeto y cuál el predicado, de modo que la frase no es reversible («Dios era el Verbo»).
- **Notas de traducción:** El español «Dios» no distingue 1b de 1c. «Divino» y «un dios» son opciones de traducción que ya toman partido por (2) o (3).
- **Otros usos:** Jn 1:6 — παρὰ θεοῦ, θεός sin artículo referido a Dios · Jn 1:18 — θεὸν οὐδεὶς ἑώρακεν, sin artículo y referido a Dios · Jn 20:28 — ὁ θεός μου dicho a Jesús
- **Certeza:** medio

Las dos fichas explican las lecturas (identidad, cualitativa, indefinida «un dios») sin elegir ninguna; la segunda va con certeza `medio`.

## 3. Casos pedidos

### 3.1 Jn 1:3-4: γέγονεν y la puntuación

¿Menciona la ficha la diferencia de puntuación (cierra el v. 3 o abre el v. 4)? **Sí.** La de γέγονεν la explica; la de ὃ remite a ella; la de ἐστιν (1:4) añade que Tisch lee ἐστιν donde las ocho ediciones del TAGNT leen ἦν.

Texto: 1:3 *πάντα δι’ αὐτοῦ ἐγένετο, καὶ χωρὶς αὐτοῦ ἐγένετο οὐδὲ ἕν ὃ γέγονεν* · 1:4 *ἐν αὐτῷ ζωὴ ἐστιν, καὶ ἡ ζωὴ ἦν τὸ φῶς τῶν ἀνθρώπων.*

#### Jn 1:3 · pos. 11 · ὃ
*lema ὅς · G3739 · morf. TAGNT R-NSN · alineación `forma` · TAGNT `John.1.3#11`*

- **Glosa:** lo que
- **Rango semántico:** que, el que, lo que; quien
- **Construcción:** Pronombre relativo neutro singular, nominativo; sujeto de γέγονεν. Su antecedente es οὐδὲ ἕν, o bien introduce una oración nueva (véase γέγονεν).
- **Sentido en contexto:** Introduce la frase final del versículo. Si se une a lo anterior, precisa el alcance («nada de lo que existe»); si se une al v. 4, el relativo abre la oración siguiente.
- **Matiz:** La puntuación de este versículo decide la lectura (véase γέγονεν); el texto antiguo no tenía signos de puntuación.
- **Certeza:** medio

#### Jn 1:3 · pos. 12 · γέγονεν
*lema γίνομαι · G1096 · morf. TAGNT V-2RAI-3S · alineación `forma` · TAGNT `John.1.3#12`*

- **Glosa:** ha sido hecho / existe
- **Rango semántico:** llegar a ser, ser hecho; ocurrir, suceder; existir
- **Construcción:** Perfecto indicativo de γίνομαι, 3.ª singular. El perfecto expresa un estado resultante: lo que ha llegado a ser y sigue existiendo.
- **Sentido en contexto:** Tisch y el TAGNT cierran aquí el v. 3, de modo que ὃ γέγονεν califica a οὐδὲ ἕν: «nada de lo que ha llegado a existir llegó a existir sin él». Existe otra puntuación antigua que pone un punto tras οὐδὲ ἕν y une ὃ γέγονεν con el v. 4: «lo que ha llegado a ser, en él era vida». La diferencia afecta el sentido del v. 4 y se discutió ya en los padres de la Iglesia; los manuscritos más antiguos no tienen puntuación y las ediciones modernas no coinciden entre sí.
- **Matiz:** El perfecto (estado presente de lo creado) frente a ἐγένετο (acto puntual) matiza la lectura: lo hecho sigue subsistiendo en él si se une al v. 4.
- **Notas de traducción:** El módulo Tischendorf pone el límite del versículo aquí y no marca ningún punto tras ἕν. En el griego sin puntuación del texto original ambas lecturas son posibles; aquí no se toma partido.
- **Otros usos:** Col 1:17 — todo subsiste en él · Heb 1:3 — sostiene todas las cosas con su palabra · Jn 1:4 — vida en él
- **Certeza:** medio

#### Jn 1:4 · pos. 4 · ἐστιν
*lema εἰμί · G1510 · morf. TAGNT V-IAI-3S · alineación `strong` · TAGNT `John.1.4#04` · lectura distinta del TAGNT: ἦν (NA28, NA27, Tyn, SBL, WH, Treg, TR, Byz)*

- **Glosa:** es / está
- **Rango semántico:** ser, existir; estar; haber
- **Construcción:** Presente indicativo de εἰμί, 3.ª singular. La lectura de Tisch es ἐστιν; las ocho ediciones del TAGNT leen ἦν.
- **Sentido en contexto:** Con ἐστιν, la afirmación es atemporal: en él hay (ahora) vida. Con ἦν (imperfecto) se mantiene la narración del prólogo: en él estaba la vida, ya entonces.
- **Matiz:** Con ἐστιν el verbo deja de narrar y pasa a afirmar un hecho permanente; con ἦν se alinea con el resto de los imperfectos del prólogo (1:1, 1:2, 1:9).
- **Variantes textuales:** Tisch lee ἐστιν; el TAGNT lee ἦν en las ocho ediciones (NA28, NA27, Tyn, SBL, WH, Treg, TR, Byz). El TAGNT no incluye a Tischendorf, por eso aquí aparece como lectura propia de Tisch.
- **Notas de traducción:** Si se sigue la puntuación de Tisch en 1:3, «lo que ha sido hecho: en él está la vida» tiene sentido propio; con ἦν se lee más fácilmente «en él estaba la vida».
- **Certeza:** medio

**Límite:** del TAGNT solo puedo afirmar que cierra el v. 3 tras γέγονεν (lleva punto) y que Tisch no trae signos de puntuación. **No verifiqué con datos la puntuación de NA28**; la ficha no la afirma y queda en `medio`.

### 3.2 Jn 1:18: Tisch lee υἱός y NA28 lee θεός

Texto de Tisch: *θεὸν οὐδεὶς ἑώρακεν πώποτε· ὁ μονογενὴς υἱὸς ὁ ὢν εἰς τὸν κόλπον τοῦ πατρός, ἐκεῖνος ἐξηγήσατο.*

Cómo se refleja: (a) υἱός queda alineada con el θεός del TAGNT por Strong alternativo y se marca `lectura_tagnt` con las ediciones que leen θεός (NA28, NA27, SBL, WH, Treg); (b) el artículo ὁ de Tisch se marca `no_en_na28`; (c) ambas fichas llevan `variantes_textuales` y certeza `medio`; (d) la ficha cita las dos versiones de dominio público con su nombre exacto.

#### Jn 1:18 · pos. 5 · ὁ
*lema ὁ · G3588 · morf. TAGNT T-NSM · alineación `forma+noNA28` · TAGNT `John.1.18#05` · variante (ausente en NA28, NA27, SBL, WH, Treg) · **no está en NA28***

- **Glosa:** el
- **Rango semántico:** el, la, lo (artículo definido)
- **Construcción:** Artículo nominativo masculino singular antepuesto a μονογενής. Tisch lo trae, igual que Tyn, TR y Byz; NA28, NA27, SBL, WH y Treg no lo traen.
- **Sentido en contexto:** Con el artículo, la frase se lee «el unigénito Hijo»; sin él, «unigénito Dios» o «Dios, el único», según la lectura que acompañe.
- **Matiz:** El artículo está ligado a la lectura υἱός: sin él, μονογενὴς θεός suena como una apelación.
- **Variantes textuales:** Presente en Tyn, TR y Byz; ausente en NA28, NA27, SBL, WH y Treg. La palabra pertenece a la lectura de Tisch (ὁ μονογενὴς υἱός).
- **Notas de traducción:** La presencia o ausencia del artículo no cambia la gramática del resto del versículo; sí interviene en cómo se comprende θεός/υἱός.
- **Certeza:** medio

#### Jn 1:18 · pos. 7 · υἱὸς
*lema υἱός · G5207 · morf. TAGNT N-NSM-T · alineación `strong` · TAGNT `John.1.18#07` · lectura distinta del TAGNT: θεὸς (NA28, NA27, SBL, WH, Treg)*

- **Glosa:** Hijo
- **Rango semántico:** hijo; descendiente; hijo de Dios
- **Construcción:** Nominativo masculino singular. Tisch lee υἱός; NA28, NA27, SBL, WH y Treg leen θεός (μονογενὴς θεός, «Dios unigénito» o «el Único, Dios»); Tyn, TR y Byz leen υἱός como Tisch.
- **Sentido en contexto:** Con υἱός, el versículo cierra con una relación personal: el Hijo único, que está junto al Padre, lo ha dado a conocer; el contraste con «a Dios nadie lo vio» es de naturaleza invisible frente a revelación en el Hijo. Con θεός, la afirmación es que el único Dios unigénito (o: «el único, que es Dios») ha dado a conocer al Padre, y se acerca a la afirmación de 1:1c.
- **Matiz:** Las dos lecturas no son contradictorias en el Evangelio; la primera es la más fácil de entender, la segunda la más difícil, y por eso muchos críticos la prefieren como más antigua y como origen de la otra. Es una cuestión abierta.
- **Variantes textuales:** Lectura de Tisch: υἱός. NA28, NA27, SBL, WH y Treg leen θεός (apoyada por manuscritos alejandrinos antiguos según los aparatos críticos); Tyn, TR y Byz leen υἱός. Aquí la ficha sigue a Tisch y deja constancia de la lectura de NA28.
- **Notas de traducción:** La Santa Biblia Reina-Valera (1909) lee «el unigénito Hijo», siguiendo el Textus Receptus; La Sagrada Biblia (Torres Amat), «El Hijo unigénito». Ninguna de las dos traduce la lectura «Dios unigénito» de NA28.
- **Certeza:** medio
- **Traducciones comparadas del versículo:**
  - *La Santa Biblia Reina-Valera (1909)*: A Dios nadie le vió jamás: el unigénito Hijo, que está en el seno del Padre, él le declaró.
  - *La Sagrada Biblia (Torres Amat)*: A Dios nadie le ha visto jamás: El Hijo unigénito, existente ab eterno en el seno del Padre, él mismo en persona es quien le ha hecho conocer ú los hombres.

### 3.3 Jn 1:28: Βηθανίᾳ vs. Βηθαβαρᾷ (TR)

Texto de Tisch: *ταῦτα ἐν Βηθανίᾳ ἐγένετο πέραν τοῦ Ἰορδάνου, ὅπου ἦν ὁ Ἰωάννης βαπτίζων.*

Cuando la Reina-Valera 1909 sigue al TR, la ficha muestra la lectura de Tisch y NA28, dice que TR lee Βηθαβαρᾷ, y cita en `notas_traduccion` la Reina-Valera 1909 («Betábara») y Torres Amat («Bethania») con el nombre exacto de cada versión; `traducciones_comparadas` trae además el versículo completo de ambas.

#### Jn 1:28 · pos. 3 · Βηθανίᾳ
*lema Βηθανία · G963 · morf. TAGNT N-DSF-L · alineación `forma` · TAGNT `John.1.28#03` · variante (ausente en TR)*

- **Glosa:** Betania
- **Rango semántico:** Betania
- **Construcción:** Nombre propio, dativo femenino singular, regido por ἐν. La lectura de Tisch es Βηθανίᾳ, como en NA28; TR lee Βηθαβαρᾷ («Betábara»).
- **Sentido en contexto:** Lugar «al otro lado del Jordán», distinto de la Betania cercana a Jerusalén (Jn 11:1). La lectura Βηθαβαρᾷ, la del TR, parece una sustitución por conjetura, pues Orígenes no hallaba una Betania junto al Jordán y propuso Betábara.
- **Matiz:** La Betania de Juan 1:28 es un lugar que no se puede ubicar con certeza; se discute su situación.
- **Variantes textuales:** Lectura de Tisch y de NA28, NA27, Tyn, SBL, WH, Treg y Byz: Βηθανίᾳ. TR lee Βηθαβαρᾷ. La Reina-Valera 1909 sigue ahí al TR.
- **Notas de traducción:** La Santa Biblia Reina-Valera (1909) lee «Betábara», siguiendo el Textus Receptus; La Sagrada Biblia (Torres Amat), «Bethania». Ambas versiones traducen distinto porque parten de lecturas griegas distintas.
- **Otros usos:** Jn 11:1 — otra Betania, junto a Jerusalén · Jn 10:40 — Jesús volvió al lugar donde Juan bautizaba antes · Jn 3:23 — Juan bautizaba en Ainón, cerca de Salim
- **Certeza:** medio
- **Traducciones comparadas del versículo:**
  - *La Santa Biblia Reina-Valera (1909)*: Estas cosas acontecieron en Betábara, de la otra parte del Jordán, donde Juan bautizaba.
  - *La Sagrada Biblia (Torres Amat)*: Todo esto sucedió en Bethania, la que está á la otra parte del Jordan, donde Juan estaba hautizando.

### 3.4 Palabras con variante o fuera de NA28 (18)

| Ref | Pos | Forma | Qué marca el TAGNT |
|---|---|---|---|
| Jn 1:15 | 10 | ὃν | ausente en WH |
| Jn 1:15 | 11 | εἶπον | ausente en WH |
| Jn 1:16 | 1 | ὅτι | ausente en TR, Byz |
| Jn 1:18 | 5 | ὁ | **no está en NA28**; ausente en NA28, NA27, SBL, WH, Treg |
| Jn 1:25 | 16 | οὐδὲ | ausente en TR, Byz |
| Jn 1:25 | 18 | οὐδὲ | ausente en TR, Byz |
| Jn 1:27 | 1 | ὁ | ausente en WH |
| Jn 1:27 | 8 | ἐγὼ | ausente en SBL |
| Jn 1:28 | 3 | Βηθανίᾳ | ausente en TR |
| Jn 1:28 | 10 | ὁ | ausente en TR, Byz |
| Jn 1:30 | 3 | ὑπὲρ | ausente en TR, Byz |
| Jn 1:32 | 10 | ὡς | ausente en TR, Byz |
| Jn 1:34 | 9 | υἱὸς | ausente en SBL |
| Jn 1:35 | 5 | ὁ | ausente en WH, Treg |
| Jn 1:40 | 5 | ὄψεσθε | ausente en TR, Byz |
| Jn 1:40 | 7 | οὖν | ausente en TR, Byz |
| Jn 1:43 | 16 | Ἰωάννου | ausente en TR, Byz |
| Jn 1:51 | 9 | ὅτι | ausente en TR, Byz |

### 3.5 Lecturas de Tisch que el TAGNT no tiene (5)

El TAGNT no incluye a Tischendorf, así que estas diferencias no figuran como «variante». El pipeline las detecta al comparar formas alineadas; se descartan las meramente ortográficas (Ἡλείας/Ἠλίας, ῥαββεί/ῥαββί, Λευείτας/Λευίτας, Ἰσραηλείτης/Ἰσραηλίτης).

| Ref | Tisch lee | TAGNT lee | Ediciones que leen la forma del TAGNT |
|---|---|---|---|
| Jn 1:4 | ἐστιν | ἦν | NA28, NA27, Tyn, SBL, WH, Treg, TR, Byz |
| Jn 1:18 | υἱὸς | θεὸς | NA28, NA27, SBL, WH, Treg |
| Jn 1:26 | στήκει | ἕστηκεν | NA28, NA27, Tyn, SBL, TR, Byz |
| Jn 1:39 | ἑρμηνευόμενον | μεθερμηνευόμενον | NA28, NA27, Tyn, SBL, WH, Treg |
| Jn 1:42 | πρῶτος | πρῶτον | NA28, NA27, Tyn, SBL, WH, Treg |

### 3.6 `caso_regido` ambiguo (2 de 71 preposiciones)

En ninguna se adivinó el caso: el campo queda vacío, la ficha explica el motivo y la certeza no es `alto`. **Ambas ambigüedades son falsos positivos de la herramienta** (ὅπου es adverbio, no preposición; en πρὸ τοῦ + infinitivo el régimen es genitivo normal). Las fichas lo dicen.

#### Jn 1:28 · pos. 8 · ὅπου
*lema ὅπου · G3699 · morf. TAGNT PREP · alineación `forma` · TAGNT `John.1.28#08` · caso regido ambiguo: se interpone V (ἦν)*

- **Glosa:** donde
- **Rango semántico:** donde, en donde
- **Construcción:** Adverbio de lugar (ὅπου) que introduce una oración relativa de lugar. El etiquetado automático lo trata como preposición y marca la construcción como ambigua; aquí es adverbio, no rige caso.
- **Sentido en contexto:** Precisa el lugar: donde Juan estaba bautizando.
- **Matiz:** La marca de «caso ambiguo» viene de la herramienta, no del texto: ὅπου no es preposición.
- **Notas de traducción:** El TAGNT clasifica ὅπου como preposición (PREP) por una convención de etiquetado; el sentido es sencillo: «donde».
- **Certeza:** medio

#### Jn 1:49 · pos. 12 · πρὸ
*lema πρό · G4253 · morf. TAGNT PREP · alineación `forma` · TAGNT `John.1.48#13` · caso regido ambiguo: artículo (G) y A no concuerdan*

- **Glosa:** antes de
- **Rango semántico:** antes de; delante de
- **Construcción:** Preposición (πρό) que rige genitivo. Aquí va con el artículo y un infinitivo (πρὸ τοῦ + infinitivo): «antes de que»; el sujeto del infinitivo va en acusativo (σε Φίλιππον φωνῆσαι). La detección automática del caso marca ambigüedad por el artículo genitivo y el acusativo del sujeto del infinitivo, pero la construcción es normal: πρό con genitivo.
- **Sentido en contexto:** Marca el tiempo anterior a la llamada de Felipe: Jesús vio a Natanael antes de que Felipe lo llamara.
- **Matiz:** El genitivo del artículo (τοῦ) forma parte del infinitivo articular; πρό siempre rige genitivo.
- **Notas de traducción:** La marca de ambigüedad proviene de la herramienta, no del texto.
- **Otros usos:** Jn 8:58 — antes que Abrahán fuese · Jn 13:19 — os lo digo desde ahora, antes que suceda · Mt 6:8 — vuestro Padre sabe lo que necesitáis antes que se lo pidáis
- **Certeza:** medio

## 4. Hallazgos y problemas encontrados

1. **Versificación del módulo Tisch en Jn 1:39-51.** El módulo corta distinto del TAGNT: su «1:39» empieza en «τί ζητεῖτε;» (que en la numeración habitual es 1:38b), y así hasta 1:51. Las fichas llevan la ref del módulo (la que usa la app) y el alineamiento enlaza con la ref TAGNT correcta, pero **la numeración de Jn 1:39-51 en la app no coincide con la de otras Biblias**.
2. **Errores de etiquetado del módulo Tisch.** ὅ y ὃ (relativos) en 1:42 y 1:43 vienen con Strong G3588 (artículo); el pipeline ya no los trata como palabras básicas cuando el TAGNT discrepa, y la ficha lo dice. τι en 1:47 e ἴδε también discrepan de Strong, sin efecto en el sentido.
3. **El TAGNT no incluye a Tischendorf.** Solo detectamos 5 lecturas propias de Tisch en Jn 1 porque comparamos con el TAGNT; las que coincidan por accidente con alguna edición no se marcan como variante de Tisch. Es una limitación de la fuente, no del pipeline.
4. **Torres Amat (OCR).** El texto del módulo trae errores de OCR; detectamos los de símbolos sueltos, pero no erratas de letras («hautizando», «hom.bres»). Las fichas citan solo versículos sin esas marcas en `textos_pd`, y `traducciones_comparadas` marca los dudosos con `aviso`. Conviene decidir si Torres Amat se muestra o se omite.
5. **Dos falsos positivos de `caso_regido_ambiguo`** (3.6): ὅπου (TAGNT lo etiqueta PREP) y πρὸ τοῦ + infinitivo. Se pueden filtrar en `contexto.py`.
6. **Jn 8:53 (defecto del módulo, ver `data/sources/NOTAS_TISCH.md`)** no se puede generar con este pipeline tal cual: son 374 palabras (~50 000 tokens de salida) y un límite de 16 000 tokens por llamada. Hay que dividirlo en bloques y decidir qué hacer con Jn 8:12-52, que el módulo no entrega. Es el único versículo del NT por encima de 60 palabras.

## 5. Costo por API y tiempo

**Cómo se midió.** No hay clave de la API en este entorno ni tokenizador local, así que **no puedo dar tokens reales exactos**; los calculé desde los caracteres reales del piloto (JSON de solicitud y de respuesta), con 3 rangos de caracteres por token según la escritura (español/JSON y griego politónico). Para obtener el número exacto basta ejecutar `count_tokens` de la API sobre las solicitudes (`pipeline.py batch-export`). Los precios por token de estos modelos no los conozco con certeza, así que **no invento una tarifa**: doy tokens y la fórmula.

| Tokens (estimados) | bajo | medio | alto |
|---|---|---|---|
| Piloto: entrada (solicitudes, 51 llamadas) | 44,569 | 51,729 | 61,893 |
| Piloto: salida (fichas) | 97,679 | 111,946 | 131,214 |
| Prompt de sistema (por llamada, cacheable) | 1,065 | 1,218 | 1,421 |
| **NT completo**: entrada (137,098 palabras) | 7.4 M | 8.6 M | 10.3 M |
| **NT completo**: salida | 16.3 M | 18.7 M | 21.9 M |
| **NT completo**: prompt × 7,895 llamadas | 8.4 M | 9.6 M | 11.2 M |

Por palabra (medio): entrada 63 tokens, salida 136 tokens.

**Fórmula del costo del NT completo** (en dólares, con tarifas en $ por millón de tokens):

- Sin Batch, con caché de prompt: `entrada × P_ent + salida × P_sal + prompt_cache_lectura × P_ent × 0,1 + (primeras escrituras)`.
- Con Batch: la Batch API se publicitó históricamente con un **50 % de descuento** sobre ambas tarifas (a verificar con la tarifa vigente); no se combina siempre con caché de prompt, así que conviene medirlo en un lote pequeño.
- Con las cifras medias: costo ≈ 18.7 × P_sal + 8.6 × P_ent (en dólares, con P en $ por millón de tokens), más el prompt de sistema. **La salida domina.** Cada $1 por millón de tokens de salida equivale a ≈ $19 por todo el NT sin Batch (≈ $9 con un 50 % de descuento).

**Tiempo de seguir con el generador manual.** En esta sesión, de leer las palabras de Jn 1 a la última corrección pasaron ≈ 22 minutos para 821 fichas (≈ 37 fichas/min, incluidas las correcciones). Extrapolado a 137,098 palabras: **≈ 61 horas de trabajo continuo (≈ 2.6 días)**, sin contar los versículos largos ni la revisión de usted. El consumo de contexto observado en esta sesión fue del orden de 175 000 tokens por 821 fichas (≈ 215 por ficha, contando lectura, escritura y correcciones); a ese ritmo el NT completo necesitaría ≈ 29 M tokens de contexto, **más de lo que queda en esta sesión (≈ 14,7 M)**. Conclusión: con el generador manual y esta calidad, completar el NT no cabe en lo que queda; cabría aproximadamente la mitad.

## 6. Decisiones tomadas tras esta revisión (2026-10-02)

1. Profundidad de las fichas: la revisa el responsable aparte; **el prompt no se cambia** (su hash `a63f498c6441` identifica la caché).
2. Lecturas propias de Tischendorf: **se muestran**; la ficha debe decir que es lectura de Tischendorf y qué leen las demás ediciones (regla del validador, campo `lectura_tisch_propia`).
3. Torres Amat: solo en versículos sin marca de OCR (criterio y porcentaje en `data/sources/tagnt/SOURCE.md`); en los marcados queda solo la Reina-Valera 1909.
4. Versificación: la clave sigue siendo la del módulo Tisch; se guarda `ref_estandar` y las citas se buscan siempre con ella (`reports/verificacion_ref_estandar_jn1.md`). Torres Amat usa la numeración de la Vulgata y se convierte a KJV con el mapeo de SWORD.
5. Resto del NT: Batch API con `claude-sonnet-5-5` y caché del prompt de sistema; antes del NT completo, una corrida de Jn 1 y su comparación (`reports/comparacion_jn1_manual_vs_api.md`).
6. Jn 8:12-53: sin fichas; aviso en la app implementado; reporte para CrossWire en `data/sources/NOTAS_TISCH.md`.