# Biblia Elim para Android

Esta carpeta contiene la primera versión nativa para Android. Lee directamente
los módulos SQLite v1 de Biblia Elim; no convierte ni modifica el texto bíblico.

Incluye Torres Amat para uso sin conexión. Desde **Importar** se pueden añadir
otros archivos `.sqlite` compatibles, por ejemplo módulos exportados desde la
versión de escritorio. Los módulos se guardan en el almacenamiento privado de
la aplicación.

## Biblias de Linux en Android

Cada compilación copia al APK las biblias SQLite instaladas en
`$XDG_DATA_HOME/biblia-elim/modules` (normalmente
`~/.local/share/biblia-elim/modules`). Si no hay módulos instalados allí, usa
los módulos de `../modulos/sqlite` del proyecto. Al abrir la app instalada,
Android valida esas bases y reemplaza atómicamente las copias que hayan
cambiado; las biblias añadidas solo en el teléfono se conservan.

Si Linux usa una carpeta personalizada, compila así:

```bash
./gradlew :app:assembleDebug -PlinuxBibleModules=/ruta/a/modules
```

También reconoce la variable `BIBLIA_ELIM_SQLITE_MODULES`.

## Funciones iniciales

- Elegir Biblia y libro, y avanzar o retroceder capítulos.
- Leer el capítulo completo con texto seleccionable.
- Modo lectura con tipografía ampliada.
- Lectura diaria del módulo activo y seguimiento de completado por fecha.
- Nube de palabras por libro, con acceso a búsqueda al tocar una palabra.
- Diccionario bíblico con definiciones, referencias y estudios del diccionario Elim.
- Buscar palabras o frases y saltar al versículo encontrado.
- Importar módulos SQLite v1 desde el selector de archivos de Android.
- Cargar en cada compilación las biblias instaladas en Linux.

## Compilar

Abre esta carpeta en Android Studio con JDK 17 y Android SDK 35, o compila con
Gradle 8.9 o posterior:

```bash
./gradlew :app:assembleDebug
```

El APK de depuración se genera en
`app/build/outputs/apk/debug/app-debug.apk`. El build obtiene los plugins de
Android Gradle desde Google Maven y Maven Central.

Para instalar la nueva versión desde la carpeta `android`, ejecuta:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Al abrirla, las bases incluidas actualizan las copias del teléfono.

Las lecturas diarias Android proponen un capítulo del módulo activo y guardan
el estado de completado localmente por fecha. El diccionario y sus estudios se
incluyen desde `ui/diccionario-elim.xml`. Otras herramientas de escritorio,
como notas, interlineal y púlpito, todavía requieren portarse.
