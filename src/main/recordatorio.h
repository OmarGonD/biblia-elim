/*
 * Biblia Elim
 * recordatorio.h - el aviso diario con la aplicación cerrada
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MAIN_RECORDATORIO_H__
#define __MAIN_RECORDATORIO_H__

#include <glib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* El aviso de la lectura tiene dos mitades que se reparten el día:
 *
 *   - Con la aplicación abierta avisa ella misma, mirando el reloj cada
 *     minuto (gui/recordatorio.h).
 *   - Con la aplicación cerrada avisa un temporizador de usuario de
 *     systemd, que a la hora elegida lanza «biblia-elim --recordar»:
 *     ese proceso manda el aviso y se muere. Es lo que hay aquí.
 *
 * Para que no avisen las dos, el proceso de una vez toma un cerrojo que
 * la aplicación mantiene cogido mientras está abierta, y si lo encuentra
 * ocupado se calla y deja hacer a la ventana. */

/* Ruta de settings.xml deducida a mano, sin pasar por settings_init():
 * esa arranca Sword, monta las listas de módulos y puede llegar a abrir
 * diálogos, que es justo lo que un aviso de una vez no quiere. Hay que
 * liberarla con g_free(). */
gchar *main_recordatorio_ruta_settings(void);

/* Coge el cerrojo del recordatorio para lo que le quede de vida al
 * proceso, y dice si lo consiguió. Lo llama la aplicación al arrancar,
 * para que el proceso de una vez sepa que ella está al mando. */
gboolean main_recordatorio_cerrojo(void);

/* El aviso de una vez de «--recordar». Devuelve el código de salida. */
int main_recordatorio_una_vez(void);

/* Deja las unidades de systemd del usuario de acuerdo con la hora que
 * hay guardada: las escribe y enciende el temporizador si el
 * recordatorio está puesto, y las quita si no. Sin systemd (o en
 * Windows) no hace nada y no se queja. */
void main_recordatorio_systemd_sincronizar(void);

#ifdef __cplusplus
}
#endif
#endif
