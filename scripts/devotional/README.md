# Devocional Elim incluido

Colección original de Biblia Elim, distribuida bajo GPL-2.0-or-later, como el
proyecto. No contiene textos copiados de Spurgeon ni citas de traducciones
bíblicas. Las referencias invitan a leer la Biblia elegida por el usuario.

Ocho ediciones: inglés, español, francés, coreano, chino simplificado,
chino tradicional, portugués de Brasil y portugués de Portugal. Cada edición
contiene 366 entradas `MM.DD`, incluido el 29 de febrero. El plan combina doce
reflexiones y oraciones mensuales con 31 lecturas y prácticas diarias; es un
ciclo anual de temas y ejercicios, no 366 ensayos independientes.

`content.json` conserva las traducciones originales editables; `generate.py`
construye ThML UTF-8 y usa el escritor RawLD de SWORD, sin escribir SQL ni
modificar el formato de las Biblias SQLite. Los binarios generados en
`modulos/devotional/` viajan con el paquete para que el arranque nunca necesite
una descarga de devocionales.

## Regenerar y verificar

```sh
cmake --build build --target biblia-devotional-writer bundled_devotional_test
python3 scripts/devotional/generate.py --writer build/src/backend/biblia-devotional-writer
ctest --test-dir build --output-on-failure -R 'bundled_devotional_test|devotional_package_test'
```

El test de paquete vuelve a generar todos los archivos y compara sus bytes,
instala el componente CMake `Devotional` en un prefijo temporal y ejecuta un
lector SWORD reubicado en los ocho idiomas. También ejecuta el instalador local
con destinos temporales. Las pruebas no usan pantalla ni descargan módulos.

## Instalación y selección

```sh
bash scripts/install-biblia-elim.sh --language=es
```

El instalador local guarda el idioma inicial junto a los datos. Para paquetes
CMake se puede configurar `-DBIBLIA_ELIM_INSTALL_LANGUAGE=es`; el valor por
defecto es `system`. Valores admitidos: `system`, `en_GB`, `es`, `fr`, `ko_KR`,
`zh_CN`, `zh_TW`, `pt_BR`, `pt`.

Una preferencia guardada o `--language` tiene prioridad sobre el idioma inicial
del instalador. El perfil nuevo conserva ese idioma en su XML; una elección
explícita de idioma del sistema también se conserva. Cambiar de idioma cambia
la edición Elim predeterminada en el siguiente arranque. Un devocional externo
seleccionado sigue teniendo prioridad. Un módulo ya instalado con el mismo ID
Elim no se sustituye.

Todas las ediciones se instalan en `share/biblia-elim/devotional/<idioma>/`.
Windows usa la ruta relativa al paquete; Linux admite el prefijo del ejecutable,
el directorio configurado y los datos de usuario. Las ejecuciones de desarrollo
usan también los mismos módulos del repositorio. El cargador abre los datos
incluidos en modo de lectura; no los copia sobre módulos del usuario.
