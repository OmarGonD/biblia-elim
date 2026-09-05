# Biblia Elim

Estudio bíblico de [Iglesia Elim](https://github.com/OmarGonD), en español, para Linux.

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
- Menús e interfaz en español

## Compilar e instalar

Dependencias típicas de Xiphos (GTK3, Sword, libxml2, CMake). Desde la raíz del repo:

```bash
cmake -S . -B build -DEPUB=OFF -DHELP=OFF
cmake --build build --target xiphos -j$(nproc)
./scripts/install-biblia-elim.sh
```

El instalador copia el binario a `~/.local/bin/biblia-elim` y el `.desktop` al menú de aplicaciones.

## Origen

El historial de git incluye el de Xiphos. El remoto `upstream` apunta a CrossWire:

```bash
git remote add upstream https://github.com/crosswire/xiphos.git
```

Licencia: GPL-2.0-or-later, igual que Xiphos. Léxico Strong 1890 y glosas Reina-Valera 1909: dominio público. Las fuentes históricas de «Jesús en la historia» son textos de la antigüedad, de dominio público; sus traducciones al castellano se hicieron para esta aplicación y salen con ella bajo la GPL.

## Construir Xiphos (documentación original)

Véase `INSTALL.md` para el proceso de compilación heredado.
