# Notas sobre el módulo SWORD Tisch

El texto de Tischendorf (8.ª ed.) se lee del módulo SWORD `Tisch`, sin modificarlo. Defectos observados al alinearlo con el TAGNT
(`tools/tagnt/alinear.py`); el alineamiento los trata en su propia capa.

## Jn 7:53–8:53 (defecto del módulo oficial de CrossWire)
No se generan fichas para Jn 8:12–8:53 (`EXCLUIDOS` en `tools/tagnt/pipeline.py`).

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

### Reporte listo para CrossWire (inglés)
Dónde enviarlo: lista **sword-devel@crosswire.org** (suscripción en https://crosswire.org/mailman/listinfo/sword-devel) y/o el tracker de CrossWire (https://tracker.crosswire.org, proyecto de módulos). El módulo figura en el repositorio como `Tisch` 2.5.1 (SwordVersionDate 2009-01-10, History 2.5.1 del 2022-08-06).

**Asunto:** Tisch 2.5.1: John 8:12-8:52 empty, John 8:53 truncated (zText 16-bit entry size overflow)

**Cuerpo:**
> Hello,
>
> The Tisch module (Tischendorf 8th ed., v2.5.1) in the official repository has a defect around John 7:53–8:53.
>
> **Symptom.** Verses John 8:12 through 8:52 are empty. John 8:53 returns about 375 words that start with the text of John 7:53 (the pericope adulterae) and continue through John 8:21, cut in the middle of a markup tag (`... Εἶπεν οὖν πάλιν <w lemma="strong:G846 ...ὑ`). John 8:22–52 never appear. John 8:54–59 are fine.
> Reproduce: `mod2imp Tisch | awk '/^\$\$\$John 8:53/{f=1;next} /^\$\$\$/{f=0} f' | wc -c` returns 40,507 bytes; or `diatheke -b Tisch -k John 8:22` (empty) and `diatheke -b Tisch -k John 8:53`.
>
> **Cause.** The module is a zText (ZIP, BlockType=BOOK) with `nt.bzs/nt.bzv/nt.bzz`. In `nt.bzv` (10-byte records: uint32 block, uint32 offset, **uint16 size**) the record for John 8:53 is #3371: block 3, offset 634662, size 40507. The next record (John 8:54) starts at offset 740705 in the same decompressed block, so the real entry is 740705 − 634662 = **106,043 bytes** (987 words). 106,043 mod 65,536 = 40,507, i.e. the size was stored modulo 2^16 and SWORD reads only that many bytes. I decompressed block 3 of the official `Tisch.zip` and confirmed the whole text (John 7:53 through 8:53) is present in `nt.bzz`; it is only unreachable through the index.
>
> **Probable origin.** The verse milestones for John 8:12–8:52 are missing in the source OSIS, so all that text was imported under the verse that carries the next milestone (8:53), and the entry exceeded 64 KB (each word carries lemma/morph markup).
>
> **Suggested fix.** Restore the verse milestones for John 8:12–8:52 (and the single `John.7.53` / `John.8.1-11` entries) in the source and rebuild the module so that no verse entry exceeds 65,535 bytes. A check in the build (fail if any entry ≥ 65,536 bytes) would catch this class of problem.
>
> I verified that the module in the repository (`packages/rawzip/Tisch.zip`) is byte-identical to a fresh install, so this is not a local copy issue (SHA-256: nt.bzv 3a16476b…, nt.bzs 5c88eb2e…, nt.bzz 267db594…).
>
> Thanks.

(Comprobaciones reproducibles desde este repositorio: `python3 tools/tagnt/alinear.py` imprime las palabras sin pareja del bloque; el desplazamiento y tamaño se leen con el script de `NOTAS_TISCH.md`.)

### Aviso mínimo (implementado)
Implementado en `src/main/interlineal_aviso.c` y mostrado en el bloque «Griego · Tischendorf» (`main_interlineal_html_original`): cuando el módulo es Tisch y el versículo es Jn 8:12–8:52 (lista fija, o cualquier versículo vacío del módulo), una línea discreta:
«Este versículo no está disponible en el módulo Tischendorf (defecto del módulo, ver Notas).» y,
en Jn 8:53, «Este versículo aparece incompleto/duplicado en el módulo Tischendorf».

## Versificación y orden distintos del TAGNT
Tisch y el TAGNT difieren en 45 versículos (p. ej. 1 Jn 2:13↔2:14, 1 Ts 2:6↔2:7, Jn 1:39–40, 2 Co 5:15, 8:13, 11:8, Hch 3:19, 13:32, 24:3)
y en el orden de palabras o versículos (Mt 5:4↔5:5 invertidos en Tischendorf). La pareja guarda la ref TAGNT real; ver `reports/alineamiento_tisch_tagnt.md`.

## Grafía
Tisch usa grafías propias (ἐπίασεν, εἶδαν, ἦλθαν, συνκρίνοντες, ἀλλὰ sin elisión) y asigna Strong a pronombres y verbos
supletivos distinto del TAGNT (ἡμῶν G2249 / G1473; εἶδον G1492 / G3708). Se admiten como `forma_otro_strong`.
