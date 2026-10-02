# Notas sobre el módulo SWORD Tisch

El texto de Tischendorf (8.ª ed.) se lee del módulo SWORD `Tisch`, sin modificarlo. Defectos observados al alinearlo con el TAGNT
(`tools/tagnt/alinear.py`); el alineamiento los trata en su propia capa.

## Jn 7:53–8:52 (defecto de versificación del módulo)
- Los versículos `Jn 8:12` a `Jn 8:52` existen como cabeceras pero están **vacíos**.
- `Jn 8:53` contiene un bloque anómalo de 375 palabras: una segunda copia de la perícopa de la adúltera (Jn 7:53–8:11)
  seguida del texto de Jn 8:12–8:21. El texto de Jn 8:22–8:52 no aparece en el módulo. Los versículos reales `Jn 8:54–59`
  son normales. `Jn 7:53` y `Jn 8:1–11` existen también por separado (lectura de Tischendorf).
- Tratamiento: el bloque se alinea al final, contra las palabras del TAGNT que quedan libres, y cada palabra recibe la ref
  TAGNT de su versículo real (la clave de ficha conserva la posición Tisch en `Jn 8:53`). 101 de sus palabras (la copia
  duplicada de la perícopa) no tienen pareja y quedan en `null`.

## Versificación y orden distintos del TAGNT
Tisch y el TAGNT difieren en 45 versículos (p. ej. 1 Jn 2:13↔2:14, 1 Ts 2:6↔2:7, Jn 1:39–40, 2 Co 5:15, 8:13, 11:8, Hch 3:19, 13:32, 24:3)
y en el orden de palabras o versículos (Mt 5:4↔5:5 invertidos en Tischendorf). La pareja guarda la ref TAGNT real; ver `reports/alineamiento_tisch_tagnt.md`.

## Grafía
Tisch usa grafías propias (ἐπίασεν, εἶδαν, ἦλθαν, συνκρίνοντες, ἀλλὰ sin elisión) y asigna Strong a pronombres y verbos
supletivos distinto del TAGNT (ἡμῶν G2249 / G1473; εἶδον G1492 / G3708). Se admiten como `forma_otro_strong`.
