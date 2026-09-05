# Torres Amat: de los escaneos al módulo SWORD

La Sagrada Biblia de Félix Torres Amat (1772-1847) es dominio público, pero
no existe en ningún repositorio SWORD ni hay ninguna transcripción digital
publicada: solo escaneos. Estos scripts reconstruyen el texto a partir del
OCR con coordenadas de la edición de cuatro tomos de 1882 conservada en
Internet Archive.

    https://archive.org/details/la-sagrada-biblia-vulgata-tomo-iiv_202111

## Por qué no vale el `_djvu.txt`

Cada imagen escaneada es un **pliego de dos páginas**, y cada página lleva
el texto arriba y un aparato de notas enorme abajo. El OCR serializa las
dos páginas fundiendo renglones de una con los de la otra, así que en el
texto plano el Génesis sale con 90 marcas de capítulo donde hay 50, falta
el 12 % de las marcas de versículo y el orden es `1, 2, 24, 11, 25 … 31,
7, 8 … 23`. No hay limpieza que arregle eso: hace falta la geometría.

## Cómo se reconstruye

Del `djvu.xml` sale cada palabra con su caja y su confianza.

| Fichero | Qué hace |
|---|---|
| `load.py` | Lee el `djvu.xml` a una estructura compacta y la cachea |
| `segment.py` | Parte el pliego por la canal, agrupa palabras en renglones por solapamiento de línea base, recorta el ruido de la lámina vecina y separa cuerpo de notas |
| `cabeceras.py` | Lee la cabecera corrida, que dice en qué libro va la página |
| `capitulos.py` | Reconoce `CAPITULO <romano>`, con los romanos que el OCR estropea |
| `versiculos.py` | Convierte renglones en versículos; reúne la partición de palabra a final de renglón |
| `nombres.py` | Nombres de libro de esta edición a identificadores OSIS |
| `marcas.py` | Reconoce la marca de versículo: basura delante, dígitos leídos como letras, número en mitad del renglón |
| `portadas.py` | Detecta las páginas de título que abren libro |
| `validar.py` | Mide la alineación contra la Vulgata y deja `revisar.txt` |
| `localizar.py` | Dice en qué pliego del escaneo cae cada laguna |
| `rehacer.sh` | Baja esas páginas sueltas del zip y las vuelve a pasar por tesseract con el modelo español |
| `hocr.py` / `paginas.py` | Meten las páginas rehechas por el mismo pipeline que las demás |
| `ojear.py` | Enseña por encima qué hay en cada media hoja de un rango |
| `rescatar.py` | Los capítulos rescatados a mano, uno por uno, contra el facsímil |
| `canon.py` | Versificación Vulgata leída de `canon_vulg.h` de SWORD |
| `alinear.py` | Alinea los capítulos observados contra el canon |
| `construir.py` | Recorre los cuatro tomos y deja `texto.json` |
| `osis.py` | Genera el OSIS |

La pieza que hace que esto funcione es `alinear.py`. Contar capítulos en
secuencia no sirve: en cuanto el OCR se come un encabezado, todo lo que
viene detrás queda corrido y el libro entero se descoloca -- así salían
el Éxodo 20:3 con el texto del 21 y el Deuteronomio 6:4 con el del
Levítico. Segmentar en capítulos candidatos y alinear esa secuencia
contra el canon, permitiendo que un capítulo se haya partido en dos, que
sobre uno espurio o que falte entero, deja cada fallo encerrado en su
sitio.

## Reproducir

    pip install nada   # solo biblioteca estándar
    # bajar los cuatro tomo*.djvu.xml del ítem de arriba
    python3 construir.py     # -> texto.json
    python3 rescatar.py      # añade los capítulos rescatados a mano
    python3 osis.py          # -> torresamat.osis.xml
    osis2mod salida/ torresamat.osis.xml -v Vulg -z z

## Cómo se mide

No hay texto de referencia en español contra el que comparar, pero sí lo
hay en latín: Torres Amat traduce de la Vulgata versículo a versículo, así
que la longitud de cada versículo castellano sigue de cerca la del latino.
`validar.py` correlaciona las dos series por capítulo en los
desplazamientos 0, +1 y -1: si gana el 0, el capítulo está en su sitio.

Cuidado con una trampa: `diatheke` devuelve el latín **con el marcado OSIS
dentro**. Si no se quita, la longitud que se mide es sobre todo marcado y
el validador da por corridos capítulos que están bien. Costó dos rondas de
conclusiones equivocadas.

## Volver a pasar el OCR

`localizar.py` sitúa cada laguna en su pliego y `rehacer.sh` baja esas
páginas sueltas -- archive.org sirve ficheros de dentro del zip, así que
no hay que traerse el gigabyte entero -- y las pasa por tesseract con
`spa.traineddata`. El OCR sale mejor que el de archive.org (`á J ared` ->
`á Jared`, `Malaleé!` -> `Malaleél`, `Y yivió` -> `Y vivió`), pero por sí
solo apenas movió la cifra: de 22 lagunas a 21.

Lo que sí las movió fue lo que se descubrió al mirar una de esas páginas
de cerca. Ver abajo.

## El rescate a mano

Lo que el alineador deja vacío casi nunca falta del OCR: está leído y bien,
pero mal asignado. `localizar.py` dice en qué pliego cae cada laguna,
`ojear.py` enseña qué hay en esa media hoja, y `rescatar.py` guarda, capítulo
por capítulo, de dónde sale y por dónde empieza. Cada entrada se ha
comprobado mirando la página escaneada.

Hicieron falta cuatro afinaciones, todas nacidas de un caso concreto:

- **Varias medias hojas por capítulo.** Colosenses 4 va a caballo del pliego.
- **Números mal leídos**, corregidos ANTES de repartir. Daniel 1:2 sale como
  «9», y si se arregla después ya se ha tragado por monotonía los versículos
  3 a 9.
- **Marcas falsas degradadas a continuación.** Un «17» espurio dentro de
  Ezequiel 1:9 se comía los versículos 10 a 16.
- **Desplazamiento de numeración.** La edición numera algunos salmos
  siguiendo al anterior: el 115 arranca en el 10 y el 147 en el 12.

De doce capítulos rescatados, nueve quedaron completos. El Salmo 1 cae al
pie de la página del prefacio al Salterio, compuesto entero en letra de
nota, así que no hay altura que lo separe: sus seis versículos están
copiados a mano del facsímil en `TRANSCRITOS`.

## Una errata de 1882

El encabezado de Colosenses 4 dice **CAPITULO VI** en el papel. La carta
tiene cuatro capítulos: es errata de la edición. El OCR lo leyó bien; el
libro está mal. Conviene tenerlo presente antes de fiarse de los números
impresos.

## Estado

94,0 % de los versículos (33.659 de 35.817), y 90,9 % de los capítulos
bien alineados según la medida de arriba. Lo que falta sale en blanco.

Sin revisar: el texto conserva erratas del OCR. Las notas de Torres Amat,
que son parte principal de la obra, no se importan. En `revisar.txt` queda
el único capítulo vacío -- el Salmo 132 -- y los 47 sospechosos de llevar
la numeración corrida, que es por donde debe seguir quien revise.

## El Salterio dice SALMO, no CAPITULO

Doce de las veintidós lagunas eran salmos, y no por culpa del OCR: el
texto estaba ahí y se extraía bien. Lo que no se detectaba era la
frontera, porque en el Salterio el encabezado impreso pone `SALMO VIII` y
el patrón solo buscaba `CAPITULO`. Sin encabezado, la única frontera que
quedaba era el reinicio de la numeración, y los salmos empiezan a menudo
en el versículo 2 o 3 porque el título ocupa el 1: se fundían unos con
otros. Añadir esa variante subió los Salmos del 86 % al 92 % y bajó las
lagunas de 21 a 14.

Moraleja para quien siga: antes de echarle la culpa al reconocimiento,
mirar una página que falle y comprobar si el texto está y lo que falla es
la estructura.

## Cosas probadas que NO funcionan

- **Contar capítulos en secuencia.** Un encabezado perdido descoloca el
  libro entero.
- **Aceptar el número de versículo sin punto a principio de renglón**
  ("3 Mas entró despues…"). Recupera versículos pero las listas y
  genealogías empiezan renglón con número y mayúscula: la alineación
  bajaba del 89,7 % al 88,1 %.
- **Llevar el contador de versículo por delante del papel.** Si antes se
  coló un versículo espurio, el contador va alto y el respaldo `ver+1`
  corre el capítulo entero: 3 Reyes 22 acababa con cada versículo un
  puesto más abajo. Hay que hacer caso al número impreso.
- **Volver a pasar el OCR como remedio general.** Cuesta 44 descargas y
  media hora de máquina, mejora el texto de verdad, y sin embargo solo
  cerró 4 lagunas de 22 (y abrió 3 nuevas). El cuello de botella casi
  nunca era el reconocimiento.
- **Limpiar los caracteres que sobran al leer un número romano.** Con
  `re.sub(r"[^IVXLCDM]", "", ...)`, "Salmo de David" dejaba la D de "de"
  y devolvía el capítulo 500. Si queda algo que no es cifra romana, no
  era un encabezado.
- **Corregir los corrimientos automáticamente** con la correlación. Se
  probó: acertaba en unos capítulos (Malaquías 1) y fallaba en otros
  (Salmo 13, y eso que era el de correlación más alta, r=0,97). Si la
  confianza no predice el acierto, no puede renumerar sola. Por eso
  `validar.py` informa y no toca.
