# Biblia Elim

Estudio bíblico de [Iglesia Elim](https://github.com/OmarGonD), en español, para Linux.

Es un fork de [Xiphos](https://github.com/crosswire/xiphos) (CrossWire / The SWORD Project): GTK nativo, sin reescribir el lector en WebKit.

## Qué incluye

- Interlineal por versículo (griego Tischendorf / hebreo WLC), **Original → Español** y **Español → Original**
- Números de Strong’s en español (fuentes de dominio público) y ficha del término
- Comparar versiones en panel partido
- Notas de versículo (ficha inferior) y subrayado tipo Kindle
- Temas (claro, oscuro, claro luna, pergamino, Omarchy)
- Planes de lectura: marcar lo leído, recuperar los días perdidos y ver el progreso por plan, libro y Biblia entera
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

Licencia: GPL-2.0-or-later, igual que Xiphos. Léxico Strong 1890 y glosas Reina-Valera 1909: dominio público.

## Construir Xiphos (documentación original)

Véase `INSTALL.md` para el proceso de compilación heredado.
