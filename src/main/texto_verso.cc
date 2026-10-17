/*
 * Biblia Elim
 * texto_verso.cc - leer un pasaje de paso, sin moverle la clave al módulo
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
#include "main/backend_access.h"

#include "gui/debug_glib_null.h"

gchar *
main_texto_de(const char *clave)
{
	if (!clave || !*clave || !settings.MainWindowModule ||
	    !*settings.MainWindowModule)
		return NULL;

	/* Resolve through the module's neutral backend. Both implementations
	 * preserve the reading position when retrieving structured content. */
	const std::string module = settings.MainWindowModule;
	BibleBackend &reader = main_backend_for(module);
	BibleKeyInfo key;
	if (!reader.hasModule(module) || !reader.resolveKey(module, clave, key))
		return NULL;
	BibleVerseContent content = reader.getVerseContent(module, key.reference, true);
	if (!content.valid)
		return NULL;
	gchar *texto = g_strdup(content.plainText.c_str());
	g_strstrip(texto);
	if (!*texto) {
		g_free(texto);
		return NULL;
	}
	return texto;
}
