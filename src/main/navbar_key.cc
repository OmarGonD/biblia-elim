/*
 * Xiphos Bible Study Tool
 * navbar_key.cc - normalized navbar key ownership boundary
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

#include <gtk/gtk.h>

#include "backend/bible_backend.h"
#include "main/navbar_versekey.h"

gchar *main_get_valid_key(const char *module_name, const char *key)
{
	BibleKeyInfo info;
	if (!bible_backend->resolveKey(module_name ? module_name : "",
				       key ? key : "", info))
		return NULL;
	return g_strdup(info.key.c_str());
}
