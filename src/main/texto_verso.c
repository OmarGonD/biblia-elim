/*
 * Biblia Elim
 * texto_verso.c - leer un pasaje de paso, sin moverle la clave al módulo
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "main/texto_verso.h"
#include "main/settings.h"
#include "main/sword.h"

#include "gui/debug_glib_null.h"

gchar *
main_texto_de(const char *clave)
{
	char *crudo;
	gchar *texto;

	if (!clave || !*clave || !settings.MainWindowModule ||
	    !*settings.MainWindowModule)
		return NULL;

	crudo = main_get_striptext(settings.MainWindowModule, (char *)clave);

	/* Y el módulo, de vuelta donde el lector lo tenía. */
	if (settings.currentverse && *settings.currentverse) {
		char *vuelta = main_get_striptext(settings.MainWindowModule,
						  settings.currentverse);
		free(vuelta);
	}

	if (!crudo)
		return NULL;
	texto = g_strdup(crudo);
	free(crudo);
	g_strstrip(texto);
	if (!*texto) {
		g_free(texto);
		return NULL;
	}
	return texto;
}
