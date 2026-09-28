/*
 * Xiphos Bible Study Tool
 * splash.c - Splash related functions
 *
 * Copyright (C) 2000-2026 Xiphos Developer Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "main/sword.h"
#include "main/settings.h"
#include "gui/splash.h"
#include "gui/utilities.h"

/* Biblia Elim shows no splash window: the main window appears as soon as
 * it is built. These keep the startup code's calls meaningful. */

gboolean gui_splash_done(void)
{
	return FALSE;
}

void gui_splash_step(gchar *text, gdouble progress, gint step)
{
	(void)text;
	(void)progress;
	(void)step;
}

void gui_splash_init(void)
{
	settings.showsplash = 0;
}
