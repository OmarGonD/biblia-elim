# Biblia Elim

Estudio bíblico de [Iglesia Elim](https://github.com/OmarGonD) para Linux,
con interfaz en inglés, español, francés, coreano, chino y portugués.

Es un fork de [Xiphos](https://github.com/crosswire/xiphos) (CrossWire / The SWORD Project): GTK nativo, sin reescribir el lector en WebKit.

## Qué incluye

- Interlineal por versículo (griego Tischendorf / hebreo Open Scriptures OSHB), **Original → Español** y **Español → Original**, con la morfología de cada palabra en español
- Números de Strong’s en español (fuentes de dominio público) y ficha del término, con la morfología de cada palabra dicha en español («verbo · aoristo · activo · indicativo · 3ª persona · singular») y no en clave
- Comparar versiones en panel partido
- Comentario clásico de dominio público (Tesoro del Conocimiento Bíblico, 1830), instalado solo, con sus referencias abriéndose en tu versión
- Biblias instaladas de serie: Reina-Valera, Platense (Straubinger) y Reina Valera Gómez en español, más las fuentes de las que salen — el masorético, la Vulgata Clementina y el Textus Receptus. Y la **Torres Amat** (1823), la primera Biblia católica en castellano de difusión amplia, que no existía en ningún repositorio SWORD: se reconstruyó para esta aplicación desde los escaneos de 1882 y viaja dentro del paquete, **con sus notas** como comentario aparte (véase `scripts/torresamat/`)
- Notas de versículo (ficha inferior) y subrayado tipo Kindle, con un buscador de todo lo que has escrito (**Estudio → Buscar en mis notas**): sin tildes ni mayúsculas como el resto de la aplicación, o con expresión regular para quien la quiera, y de cada resultado al versículo en la versión en que se escribió
- Jesús en la historia: las fuentes de fuera de la Biblia -- Josefo, Tácito, Plinio, Suetonio, el Talmud, la piedra de Pilato, el papiro más antiguo del Nuevo Testamento -- traducidas al castellano, cada una con lo que demuestra, lo que no, y sus pasajes para abrir en tu versión
- Vista púlpito: el bosquejo a pantalla completa y paso a paso, con el texto bíblico grande y su cita, el bosquejo entero a un toque y todo lo demás fuera de la pantalla; con un segundo monitor, la congregación ve solo el versículo y el punto, y una tecla la apaga en negro sin perder el paso. El tiempo que se lleva predicado arriba, con aviso de color si se dijo cuánto iba a durar, y las notas escritas para uno mismo (`Nota:`) a la vista en el atril y nunca en la pared
- Temas (claro, oscuro, claro luna, pergamino, Omarchy)
- Planes de lectura: marcar lo leído, recuperar los días perdidos y ver el progreso por plan, libro y Biblia entera
- Memorización opcional: un versículo por semana con repaso espaciado por cajas
- Tiempo estimado de la lectura de hoy («9–13 min»), contando las palabras de tu versión
- Racha y calendario de constancia: qué días leíste de verdad, en un año de un vistazo
- Versículo del día (450 citas, en tu versión) con un espacio corto para tu reflexión de cada día
- Recordatorio diario a la hora que elijas, que llega aunque la aplicación esté cerrada (temporizador de usuario de systemd)
- Nube de palabras y diccionario offline
- Interfaz en seis idiomas, con elección guardada y opción de seguir el idioma del sistema

## Compilar e instalar

Dependencias típicas de Xiphos (GTK3, Sword, libxml2, CMake). Desde la raíz del repo:

```bash
cmake -S . -B build -DEPUB=OFF -DHELP=OFF
cmake --build build --target biblia-elim -j$(nproc)
./scripts/install-biblia-elim.sh
```

El instalador copia el binario a `~/.local/bin/biblia-elim` y el `.desktop` al menú de aplicaciones.

## Idioma de la interfaz

Abre **Preferencias** (F2), **General**, y elige **Idioma de la interfaz**.
El selector muestra los nombres nativos: **English**, **Español**,
**Français**, **한국어**, **简体中文** y **Português (Brasil)**.
La elección se guarda y se aplica al reiniciar. **Idioma del sistema**
recupera la selección automática; también se conservan los idiomas heredados.

Para probar un idioma en un arranque, sin cambiar la preferencia guardada:

```bash
./build/src/gtk/biblia-elim --language=fr
./build/src/gtk/biblia-elim --language=ko
./build/src/gtk/biblia-elim --language=zh_CN
```

Las traducciones se distribuyen con la aplicación y funcionan sin internet
ni instalar los locales francés/coreano/chino/etc. del sistema. El idioma
de la interfaz se aplica a menús, diálogos, lectura, notas, herramientas y
explicaciones gramaticales. El texto de cada Biblia, las notas personales y
las fuentes de estudio conservan su idioma original.

Los mensajes de la interfaz se extraen con
`python3 tools/i18n_catalogs.py --extract po/xiphos.pot`; las traducciones
añadidas están en `po/elim/` y en los catálogos gettext de `po/`.
Construir `biblia-elim` recompila y prepara esos catálogos para ejecutarlo
directamente desde `build/` o instalarlos con el script habitual.

## Origen

El historial de git incluye el de Xiphos. El remoto `upstream` apunta a CrossWire:

```bash
git remote add upstream https://github.com/crosswire/xiphos.git
```

Licencia: GPL-2.0-or-later, igual que Xiphos. Léxico Strong 1890 y glosas Reina-Valera 1909: dominio público. Las fuentes históricas de «Jesús en la historia» son textos de la antigüedad, de dominio público; sus traducciones al castellano se hicieron para esta aplicación y salen con ella bajo la GPL.

## Construir Xiphos (documentación original)

Véase `INSTALL.md` para el proceso de compilación heredado.

## Módulos SQLite de Biblia

El primer arranque sin módulos incluye dos Biblias en coreano desde
CrossWire: [KorRV — 개역성경, revisión 1952/1961](https://crosswire.org/sword/modules/ModInfo.jsp?modName=KorRV)
y [KorHKJV — Hangul King James Version](https://crosswire.org/sword/modules/ModInfo.jsp?modName=KorHKJV).
En una instalación existente, abre **Instalar Biblias**, selecciona
**CrossWire HTTPS**, pulsa **Actualizar catálogo** y elige el idioma **Coreano**;
marca las dos versiones y pulsa **Instalar**. Se convierten automáticamente
a SQLite, conservando el texto coreano UTF-8 y la numeración de cada edición
(NRSV para KorRV, KJV para KorHKJV). KorRV es de dominio público;
KorHKJV permite distribución gratuita no comercial según su ficha de CrossWire.

El backend predeterminado usa módulos instalados en
`$(g_get_user_data_dir())/biblia-elim/modules`. El diálogo **Módulos SQLite**
permite instalar archivos `.sqlite`, importar directorios USFM y eliminar
módulos administrados; las copias se validan y se instalan atómicamente.
También se puede seleccionar un directorio explícito con
`--backend=sqlite:/ruta` o forzar compatibilidad SWORD con `--backend=sword`.
La ruta antigua `biblia_elim/modules` se migra al iniciar con la ruta predeterminada:
solo se trasladan módulos válidos y nunca se reemplaza un ID ya instalado. Los
archivos inválidos o en conflicto permanecen en la ruta antigua y se muestra un aviso.
El gestor también respeta `--backend=sqlite:/ruta` y `BIBLIA_ELIM_SQLITE_MODULES`.
Si no hay módulos SQLite válidos, el arranque utiliza SWORD como respaldo,
con un aviso no bloqueante en la barra de estado. La barra y **Ayuda > Acerca de**
indican el backend activo.
El ejecutable `biblia-usfm-import` y la interfaz comparten el mismo importer.

**Módulos SQLite > Convertir Biblia SWORD…** convierte Biblias OSIS instaladas
sin `mod2osis`: lee el OSIS con libsword y utiliza el importador neutral y el
escritor SQLite v1. Conserva el ID exacto del módulo, `Lang` y `Versification`
(KJV cuando falta esa clave, como SWORD), sin reemplazar una copia SQLite ya
instalada. La conversión es automática: al instalar Biblias SWORD (instalador
de Biblias o gestor avanzado) y al iniciar la aplicación (primer arranque o
Biblias instaladas por fuera), toda Biblia SWORD convertible sin copia SQLite se
convierte y el lector pasa a SQLite en la misma sesión, sin reiniciar. La
conversión corre en segundo plano, en un proceso hijo por Biblia
(`biblia-elim --convert-sword ID DIR`, porque SWORD no es seguro entre hilos),
con una barra de progreso en la barra de estado; mientras tanto se sigue leyendo
con SWORD. Al salir se espera a que termine la Biblia en curso y las demás se
convierten en el siguiente inicio. Con
`--backend=sword` no se convierte nada. Las entradas SWORD dañadas solo al
final (carácter UTF-8 o etiqueta cortados, elementos sin cerrar, como en SpaRVG
Mt 9:6 o Tisch Jn 8:53) se reparan sin inventar texto y se anotan en el
registro; cualquier otro daño hace fallar la conversión. Una conversión fallida
se anota en `.sword-conversion-failed` dentro del directorio de módulos y no se
reintenta sola (sí cuando mejora el conversor); puede repetirse desde
**Convertir Biblia SWORD…**. La conversión
mantiene el original SWORD y no modifica notas, marcadores ni sesiones.
Las Biblias con otros formatos internos o sin desbloquear siguen en SWORD y
siguen apareciendo en la lista de Biblias también en modo SQLite.
Las copias siguen a su módulo SWORD: si cambia su versión (o mejora el
conversor) se vuelven a convertir y se reemplazan atómicamente, y al
desinstalar la Biblia SWORD se borra su copia. Los módulos importados o
instalados por otra vía nunca se tocan. La conversión conserva las palabras de
Cristo (en rojo según la opción del módulo), las palabras añadidas (cursiva),
el nombre divino (versalitas), las cursivas y negritas de la edición, los
párrafos (¶) y la lectura principal de las variantes textuales, sin cambiar el
esquema SQLite v1.

Las Biblias SQLite se muestran con el mismo panel que las SWORD (herramientas de
versículo, notas, marcadores, resaltados, interlineal, ventana de capítulos).
Las comprobaciones completas de cada módulo (referencias, Strong, morfología) se
recuerdan en `.validation-cache` dentro del directorio de módulos, por archivo;
un módulo reemplazado se vuelve a comprobar.

## Atribución

Datos textuales y gramaticales del TAGNT: STEPBible.org, Tyndale House Cambridge, CC BY 4.0 (https://github.com/STEPBible/STEPBible-Data).

## Fichas de estudio del interlineal (`fichas.sqlite`)

Las fichas (sentido en contexto, construcción, variantes textuales, citas de la Reina-Valera 1909 y Torres Amat) se leen
de una base SQLite **local de solo lectura**; la app no necesita conexión a internet.

- **Fuente versionada:** `data/fichas_v2/` (fichas heredadas), `data/fichas_v3/` (fichas nuevas; si coincide la clave gana v3) y
  `data/citas/citas_nt.json`. Todos en el mismo formato.
- **Generación:** `fichas.sqlite` **no se commitea**. El build la genera con `tools/construir_fichas_sqlite.py`
  (solo biblioteca estándar de Python; el mismo patrón que `strongs-elim.sqlite`) y es reproducible: mismas entradas, mismo archivo.
  A mano: `python3 tools/construir_fichas_sqlite.py --output data/fichas.sqlite`.
- **Instalación:** `make install` copia `fichas.sqlite` a `<prefijo>/share/biblia-elim/fichas.sqlite` (junto a `strongs-elim.sqlite`).
- **Dónde la busca la app** (se abre en la primera consulta, no al arrancar): 1) `$BIBLIA_ELIM_FICHAS` (ruta explícita, exclusiva);
  2) la del árbol de build; 3) `SHARE_DIR/fichas.sqlite`. Si no existe, la ficha básica funciona igual, sin enriquecimiento.
- **Clave de una ficha:** `ref_tisch` (numeración del módulo Tisch) + `posicion` de la palabra en el versículo + `strong`.
  El clic en una palabra del interlineal lleva la posición; sin posición solo se resuelve un Strong único en el versículo.
- Atribución de los datos del texto griego: STEPBible.org, Tyndale House, Cambridge (CC BY 4.0); ver `data/sources/tagnt/SOURCE.md`.
