# Nácar-Colunga: de los escaneos al módulo SWORD (copia local)

La Sagrada Biblia de Eloíno Nácar Fuster y Alberto Colunga, 1.ª edición
BAC 1944, no existe en ningún repositorio SWORD. Estos scripts la
reconstruyen a partir del OCR con coordenadas del facsímil de Internet
Archive y la instalan **solo en `~/.sword`**.

    https://archive.org/details/SagradaBibliaNacarColunga19441Edicin

No viaja con el paquete de la aplicación. Nácar murió en 1971: la obra
sigue con derechos. Es una copia local de uso privado.

## Por qué el EPUB no basta

El EPUB de Archive es el OCR serializado, un párrafo por página. Esta
edición es a **dos columnas** con notas al pie en letra menor. Sin las
cajas de cada palabra el Génesis 1 mezcla el final de la columna
izquierda con el arranque de la derecha, y las notas se cuelan en el
versículo. El `_djvu.xml` trae cada palabra con coordenadas y
confianza: es lo mismo que se usó para Torres Amat.

## Cómo se reconstruye

Del `djvu.xml` sale cada palabra con su caja.

| Fichero | Qué hace |
|---|---|
| `bajar.py` | Trae `djvu.xml` y `scandata.xml` de Archive |
| `load.py` | Lee el XML a una estructura compacta y la cachea |
| `segment.py` | Parte la página por el medianil, agrupa renglones, separa cuerpo de notas por altura de letra |
| `cabeceras.py` | Lee la cabecera corrida (`GÉNESIS, 4`, `SAN LUCAS, 9`) |
| `nombres.py` | Nombres de esta edición a identificadores OSIS |
| `versiculos.py` | Marcas de versículo y el `3 1` que abre capítulo |
| `construir.py` | Alinea contra el canon NRSVA (reusa `alinear.py` de Torres Amat) |
| `completar.py` | Rellena versos a medias con Reina-Valera / Platense, cotejo griego |
| `osis.py` | Genera el OSIS |
| `comentario.py` | Notas por capítulo, como comentario aparte |
| `instalar.sh` | `osis2mod` y copia a `~/.sword` |

La versificación es **NRSVA**, no Vulgata: Nácar-Colunga traduce del
hebreo y del griego (Salmos 150, Samuel/Reyes a la hebrea). El orden
de los libros es el de la BAC 1944 (históricos con deuterocanónicos,
profetas, sapienciales, NT), que no coincide con el de NRSVA.

## Re-OCR (recomendado)

El `djvu.xml` de Archive mezcla las dos columnas. Tesseract en español
sobre cada columna por separado lee el castellano de 1944:

    python3 reocr.py -j 3          # las 1512 páginas, ~90 min
    python3 construir.py
    python3 completar.py
    python3 osis.py && ./instalar.sh

`reocr.py` es reanudable: si se corta, al relanzarlo sigue donde iba.
Páginas sueltas se rehacen (`python3 reocr.py 1469 1470 1471`). El
medianil se mide por tinta en cada página: un 48 % fijo se comía el
final de la columna izquierda en los rectos (1 Juan 3).

Barrido del facsímil de Princeton (Canon 5D, cabecera en rojo):

    python3 reocr.py --princeton -j 4     # 1516 hojas, reanudable
    python3 construir.py && python3 completar.py
    python3 osis.py && ./instalar.sh

El índice del pickle es el número de hoja de Princeton (1474 = p. 1372).
La tinta roja de «I SAN JUAN, 3, 4» se guarda en el pickle y manda
sobre las cajas del cuerpo al identificar el libro.

Para páginas difíciles hay una segunda pasada conservadora que contrasta
PSM 4 con PSM 6 sobre una imagen desinclinada y binarizada; solo sustituye
una palabra si la caja coincide y su confianza mejora:

    python3 reocr.py --princeton --ensemble 1474

## Cotejo y ajuste supervisado

`python3 revision.py` genera `revision.json`, una cola ordenada por riesgo.
Cada entrada lleva referencia, motivo, confianza y, cuando existe, hoja,
columna y caja del facsímil. Es el punto de partida para el cotejo humano.

Los versos se marcan en OSIS como `ocr-facsímil` o
`reconstruido-testigos`; los segundos no deben confundirse con una lectura
certificada del impreso. Tras reunir líneas revisadas manualmente en
`ground-truth/`, `./entrenar_tesseract.sh` deja preparado el ajuste de un
modelo `spa_nacar1944`; no se entrena con OCR sin verificar.

## Reproducir

    cd scripts/nacarcolunga
    ./instalar.sh

Hace falta `osis2mod` e `imp2vs` del paquete `sword` del sistema.
El resultado aparece en Biblia Elim como **NacarColunga**. Las notas,
si se pudieron extraer, como comentario **NacarColungaNotas**.

Estado de esta pasada: **30.218 de 35.221 versículos (85,8 %)**, con
Tesseract español columna a columna. El castellano de 1944 se lee
mucho mejor que el OCR de Archive (Mateo 5:33-36 sale el texto de
Nácar, no un relleno). La cobertura no equivale a fidelidad: quedan
5.003 huecos, erratas de OCR y versos parciales completados de forma
conservadora. No usar como texto de referencia sin cotejar con el
facsímil.

La fusión de numeración usa el DjVu solo para corregir una caja de
Tesseract coincidente. No inserta una segunda cifra encima de una ya
leída: esa duplicación podía convertir «11 Dijo» en «11 1 Dijo», abrir
un capítulo falso y desplazar los versículos restantes. La regresión se
comprueba con `python3 test_load.py`.

El texto no se publica en el repositorio: `fuentes/` y `salida/` están
en `.gitignore`.
