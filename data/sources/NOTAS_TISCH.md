# Notas sobre el módulo SWORD Tisch

El texto de Tischendorf (8.ª ed.) se lee del módulo SWORD `Tisch`, sin modificarlo. Defectos observados al alinearlo con el TAGNT
(`tools/tagnt/alinear.py`); el alineamiento los trata en su propia capa.

## Jn 7:53–8:53 (defecto del módulo oficial de CrossWire)
Verificado el 2026-10-02: el módulo instalado (`Tisch` 2.5.1, SwordVersionDate 2009-01-10, History 2.5.1 de 2022-08-06) es **idéntico byte a byte**
al publicado por CrossWire (`packages/rawzip/Tisch.zip`; SHA-256 de `nt.bzv`/`nt.bzs`/`nt.bzz` iguales). El defecto no es de esta copia.

- Las cabeceras `Jn 8:12` a `Jn 8:52` existen pero están **vacías**; todo el texto de 7:53 (copia de la perícopa) y de 8:12–8:53 está en **una sola
  entrada**, la de `Jn 8:53` (entrada 3371 del índice `nt.bzv`, bloque 3, desplazamiento 634 662).
- Esa entrada mide **106 043 bytes** (987 palabras, de «καὶ ἐπορεύθη ἕκαστος…» a «…τίνα σεαυτὸν ποιεῖς;»), pero el índice zText guarda el tamaño en
  16 bits: 106 043 mod 65 536 = **40 507**. SWORD lee solo esos 40 507 bytes, así que `mod2imp`/`diatheke`/Xiphos entregan 375 palabras, cortadas a mitad
  de una etiqueta, en «Εἶπεν οὖν πάλιν» (Jn 8:21).
- El texto completo **sí está** en `nt.bzz`; solo es inaccesible por el API de SWORD. No se puede corregir desde la app.
- Causa probable en el origen: faltan los marcadores de versículo de `Jn 8:12-52` en el OSIS de origen (todo cayó bajo `Jn 8:53`).
- Lo que ve hoy el usuario: Jn 8:12–8:52 en blanco; Jn 8:53 muestra el bloque truncado (perícopa duplicada + 8:12-21 parcial) y `Jn 8:54–59` normales.
- Tratamiento en nuestro alineamiento: ver abajo; el texto mostrado sigue saliendo del módulo.

### Texto para reportar a CrossWire (inglés)
> **Module Tisch 2.5.1: Jn 8:12–8:52 are empty and Jn 8:53 is truncated (zText 16-bit size overflow)**
> In the official `Tisch.zip` (identical to the installed copy), verse entries John 8:12 … 8:52 are empty. The text of John 7:53–8:53 sits in a single entry
> attached to John 8:53 (`nt.bzv` record 3371, block 3, start 634662). That entry is 106,043 bytes, but the index size field is 16-bit
> (106,043 mod 65,536 = 40,507), so SWORD returns only 40,507 bytes (cut mid-tag, around John 8:21). `mod2imp Tisch` reproduces it.
> The full text is present in `nt.bzz`. The source OSIS probably lacks the verse milestones for John 8:12–8:52. Re-importing with the verse markers restored
> (each verse under 64 KB) should fix it. Reference: TAGNT/NA28 verse boundaries.

### Aviso mínimo propuesto (no implementado)
Mostrar en el panel griego, cuando el módulo es Tisch y el versículo es Jn 8:12–8:52 (lista fija, o cualquier versículo vacío del módulo), una línea discreta:
«Este versículo no está disponible en el módulo Tischendorf (defecto del módulo, ver Notas).» y,
en Jn 8:53, «Este versículo aparece incompleto/duplicado en el módulo Tischendorf».

## Versificación y orden distintos del TAGNT
Tisch y el TAGNT difieren en 45 versículos (p. ej. 1 Jn 2:13↔2:14, 1 Ts 2:6↔2:7, Jn 1:39–40, 2 Co 5:15, 8:13, 11:8, Hch 3:19, 13:32, 24:3)
y en el orden de palabras o versículos (Mt 5:4↔5:5 invertidos en Tischendorf). La pareja guarda la ref TAGNT real; ver `reports/alineamiento_tisch_tagnt.md`.

## Grafía
Tisch usa grafías propias (ἐπίασεν, εἶδαν, ἦλθαν, συνκρίνοντες, ἀλλὰ sin elisión) y asigna Strong a pronombres y verbos
supletivos distinto del TAGNT (ἡμῶν G2249 / G1473; εἶδον G1492 / G3708). Se admiten como `forma_otro_strong`.
