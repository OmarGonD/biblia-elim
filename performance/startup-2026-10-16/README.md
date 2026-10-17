# STARTUP-PERF-106 — validación final, 2026-10-16

Status: DONE. Acceso a Wayland restablecido, cambios compilados, regresiones
gráficas ejecutadas y comparación real antes/después completada.

## Primer capítulo visible

Medianas en milisegundos, cinco aperturas por backend y variante tras un
calentamiento descartado. Sesión Wayland nativa, GTK 4.24.1, OpenGL por defecto,
misma copia aislada del perfil del lector, SpaPlatense en Lucas 23:36 y entrada
con las 14 Biblias SQLite ya instaladas. No se ejecutaron compilaciones ni
pruebas simultáneas durante esta serie final. Ancho del panel al primer dibujo:
890 px en todas las muestras. `GDK_BACKEND=wayland` fue explícito; Hyprland
confirmó `xwayland=false` durante la verificación.

| Backend | Antes Debug | Antes optimizado | Después optimizado | Reducción total |
|---|---:|---:|---:|---:|
| SQLite | 692,9 | 671,6 | 649,6 | 6,2 % |
| SWORD | 660,1 | 677,0 | 610,5 | 7,5 % |

La compilación optimizada por sí sola tiene un efecto pequeño y variable:
SQLite 692,9 → 671,6 ms,
SWORD 660,1 → 677,0 ms.
No se extrapola el ahorro grande del microbenchmark SQLite a toda la apertura.
Las muestras individuales y su variación se conservan en
`verified-wayland/comparison.json` y en los `summary.json` de cada variante.

El resultado mide el primer capítulo legible. Los vecinos se cargan después:
la ventana completa y la posición restaurada quedan listas a los
1145,4 ms en SQLite y
1065,2 ms en SWORD,
contados desde el inicio. Todas las restauraciones medidas convergen.

## Cairo y cachés vacías

Cairo muestra el primer capítulo en 512,9 ms con SQLite y
505,9 ms con SWORD. Sigue disponible mediante
`GSK_RENDERER=cairo`; OpenGL continúa por defecto. La decisión anterior
GTK4-PERF-101 conserva el criterio de consumo al desplazar texto. En esta tarea
se volvió a medir el arranque de Cairo, no su CPU/FPS al desplazarse, por lo
que esas cifras históricas no se presentan como mediciones actuales.

Cachés XDG vaciadas antes de cada apertura, comparando ambos binarios optimizados:

| Backend | Antes (ms) | Después (ms) |
|---|---:|---:|
| SQLite | 1161,1 | 1153,8 |
| SWORD | 1182,0 | 1107,8 |

Para SQLite la diferencia con cachés vacías es prácticamente nula. No se
vació la caché de disco del sistema: esto no representa un primer inicio tras
reiniciar el equipo. Las cachés reales del usuario permanecieron intactas.

## Cambios finales

- Primera carga del panel principal limitada al capítulo solicitado; después
  del primer dibujo se recupera la ventana normal. Radio cero sigue siendo
  libro completo y los paneles secundarios conservan su comportamiento.
- Ampliación con el recorrido existente de restauración de anclaje, posición,
  foco y reserva de lectura; el recorrido neutral SQLite ya puede usarlo.
  La ampliación espera a que termine el salto inicial al versículo.
- GTK moderno cambia la variante clara/oscura mediante su propiedad de color.
  Se evita volver a cambiar el nombre del tema, que provocaba otra recarga.
  La regresión verifica el esquema, el fondo GTK y que el nombre no cambie.
- Barra de conversiones colocada junto a GtkStatusbar en un contenedor propio;
  se corrige el cast inválido de GtkStatusbar a GtkBox descubierto al medir.
- Eliminada la conexión a una señal `set_focus` inexistente cuyo callback
  estaba vacío; corregido el tamaño de icono GTK3 inválido del gestor de módulos.
- Pruebas conservan sus aserciones: locale español explícito para las que
  buscan títulos españoles, texto paralelo con cursivas de la fixture y
  espera de frames reales antes de comparar anchos. Los checks de superficie
  usan el idle prioritario y los eventos de doble clic de la implementación GTK4.
- Benchmark con perfiles aislados, eventos separados para primer dibujo y
  ventana completa, cachés calientes/vacías, hashes y detección de diagnósticos.
  Cada app tiene un grupo de procesos propio que se termina junto con los
  conversores que hubiera iniciado. `compare_startup.py` usa por defecto las
  Biblias instaladas, igual que el benchmark individual.
- `build` y el comando habitual de README usan RelWithDebInfo. El binario
  actual coincide con `build/performance/startup-2026-10-16/after-optimized`.

## Verificación

- `biblia-elim` RelWithDebInfo: PASS.
- `gtk_lifecycle_smoke`: PASS, 576 comprobaciones, navegación=8, renderers=5,
  panels=40; inicio limitado, ampliación y conservación del versículo incluidos.
- `author_commentary_probe`: PASS.
- `wk_html_surface_test` y `reading_width_contract_test`: PASS con display real
  de Xvfb; no se omitieron sus aserciones gráficas.
- Ventana de lectura, foco, rueda, estado de panel y distribución de lectura:
  PASS, cero fallos. `strong_interaction_test`: PASS, 4/4.
- `sqlite_bible_backend_test`: PASS, 16/16; resolvedores SQLite y SWORD: PASS.
- Harness de benchmark: PASS, 7/7; baseline de arranque: PASS, 35/35.
- CTest de ventana, navbar y preferencias de recursos: PASS, 3/3.
- Prueba adicional con los seis módulos distribuidos y conversiones automáticas:
  PASS, sin los errores GtkStatusbar/GtkBox originales; no se usa para comparar
  velocidad porque la conversión forma parte de esa carga distinta.
- Serie final: las seis variantes y sus 60 muestras medidas son válidas,
  sin diagnósticos GTK/GObject. Perfil/tabs idénticos y restauración convergente
  verificados en `measurement-validation.json`. Logs de pruebas en `tests/`.
- `git diff --check`: PASS.

## Evidencia y reproducción

Hashes de binarios y conjunto de módulos en `manifest.json`. Los binarios
antes Debug/optimizado y después optimizado se conservan bajo `build/performance/`.
`verified-wayland/` es la serie final. `visual-comparison/` documenta el bloqueo
inicial del sandbox, ya resuelto. `visual-session/` conserva el intento con solo
seis módulos que activaba conversiones; `visual-installed/` y `final-wayland/`
son series intermedias, anteriores al ajuste final del tema. No se escogieron
sus mejores tiempos para el resultado final.

Desde la raíz, con un directorio de salida nuevo:

```bash
GDK_BACKEND=wayland python3 tools/compare_startup.py \
  --before-debug build/performance/startup-2026-10-16/before-debug \
  --before-optimized build/performance/startup-2026-10-16/before-optimized \
  --after build/performance/startup-2026-10-16/after-optimized \
  --display current --runs 5 \
  --output performance/startup-2026-10-16/repeat-wayland
ctest --test-dir build -R '^(gtk_lifecycle_smoke|author_commentary_probe)$' --output-on-failure
```

El perfil y los módulos se copian: el benchmark no modifica las preferencias
ni las Biblias instaladas. Requiere una biblioteca SWORD y el perfil local que
se usaron en esta máquina; los logs identifican las condiciones exactas.

## Microbenchmark backend anterior (separado del tiempo visual)

Siete procesos por variante con calentamiento descartado y orden alternado,
las mismas fuentes backend y los seis módulos distribuidos: apertura
9,592 → 9,149 ms y 100 lecturas de capítulo por lotes 168,416 → 67,587 ms.
Datos en `backend-build-comparison.json`. El ejecutable Debug realmente usado
se conserva como `build/performance/startup-2026-10-16/backend-debug`.
