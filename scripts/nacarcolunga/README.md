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
| `versiculos.py` | Marcas de versículo y el `3 1` que abre capítulo; `corrige_repetidos` (⁵ leído 6) y `parte_dolar` (⁵ leído 6 con el ⁶ leído «$», tras ensamblar) |
| `construir.py` | Alinea contra el canon NRSVA (reusa `alinear.py` de Torres Amat) |
| `correspondencias.json` | Verso impreso → NRSVA documentado con el facsímil cuando la cuenta no basta (Sal 13, hoja 967) |
| `limpieza.py` | Errores de OCR con regla general: «!» leído «l» («¡oh Yavel»), guion de fin de renglón leído «.»/«:» |
| `erratas.json` | Lecturas sueltas cotejadas con el facsímil, cada una con su hoja |
| `completar.py` | Hoy no completa nada: deja `texto.json` intacto y `reconstruidos.txt` vacío (ver «Versos incompletos») |
| `osis.py` | Genera el OSIS; los epígrafes de salmo van como `<title canonical="false">` |
| `epigrafes.json` | Epígrafes editoriales de salmo («Canto triunfal.»), con su procedencia; salida de `construir.py` |
| `perdidas.json` | Pérdida estructural por verso vista por el parser (`truncado`, `perdida_probable`, señales con su caja del facsímil); no completa texto |
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

Los versos se marcan en OSIS como `ocr-facsímil`. El tipo
`reconstruido-testigos` marcaba los versos que `completar.py` rellenaba con
otra Biblia; con la política actual ya no se genera (ver «Versos
incompletos»). Tras reunir líneas revisadas manualmente en
`ground-truth/`, `./entrenar_tesseract.sh` deja preparado el ajuste de un
modelo `spa_nacar1944`; no se entrena con OCR sin verificar.

## Reproducir

    cd scripts/nacarcolunga
    ./instalar.sh

Hace falta `osis2mod` e `imp2vs` del paquete `sword` del sistema.
El resultado aparece en Biblia Elim como **NacarColunga**. Las notas,
si se pudieron extraer, como comentario **NacarColungaNotas**.

Estado de esta pasada: **30.316 de 35.221 versículos (~86,1 %)**, con
Tesseract español columna a columna. El castellano de 1944 se lee
mucho mejor que el OCR de Archive (Mateo 5:33-36 sale el texto de
Nácar, no un relleno). La cobertura no equivale a fidelidad: quedan
4.905 huecos, erratas de OCR y versos parciales. No usar como texto de
referencia sin cotejar con el facsímil.

## Versos incompletos

El pipeline no usa otras Biblias para modificar Nácar-Colunga ni rellena
huecos. Un verso que el OCR o el parser dejaron a medias se escribe tal
cual, parcial. Un verso sin texto no se escribe en el módulo; el visor lo
suple al leerlo, desde otra Biblia y con su aviso. `completar.py` sigue en
la cadena solo por compatibilidad: no llama a diatheke, no necesita
`fuentes/testigos.pkl` ni red.

Antes `completar.py` daba por incompleto todo verso con menos palabras que
el 80 % de la Reina-Valera o la Platense y lo cambiaba por el testigo: la
auditoría de NACAR-FALLBACK-101 contó 4.602 versos así, con 21.377 palabras
de Nácar perdidas, entre ellos versos completos en el facsímil (Sal 17:7,
Gn 38:6, Lc 23:21). No hay hoy metadata que distinga un verso truncado de
uno completo; rellenar volverá a ser posible cuando el parser marque de
forma explícita el cuerpo perdido. `python3 test_completar.py` lo comprueba.

La fusión de numeración usa el DjVu solo para corregir una caja de
Tesseract coincidente. No inserta una segunda cifra encima de una ya
leída: esa duplicación podía convertir «11 Dijo» en «11 1 Dijo», abrir
un capítulo falso y desplazar los versículos restantes. La regresión se
comprueba con `python3 test_load.py`.

El texto no se publica en el repositorio: `fuentes/` y `salida/` están
en `.gitignore`.
