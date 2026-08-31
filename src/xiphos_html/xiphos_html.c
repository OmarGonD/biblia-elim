/*
 * Xiphos Bible Study Tool
 * xiphos_html.c - toolkit-generalized html support
 *
 * Copyright (C) 2010-2026 Xiphos Developer Team
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

#include "xiphos_html.h"
#include "main/sword.h"

XiphosHtml *xiphos_html_new(DIALOG_DATA *dialog, gboolean is_dialog,
			    gint pane)
{
	XiphosHtml *html;
	html = wk_html_create();
	XiphosHtmlPriv *priv = XIPHOS_HTML_GET_PRIVATE(html);

	priv->pane = pane;
	priv->is_dialog = is_dialog;
	priv->dialog = dialog;

	/* Issue #921: enable keyboard caret/selection in the biblical text
	 * so users can navigate and select text (Shift+arrows) with the
	 * keyboard only. Deliberately restricted to TEXT_TYPE (the main
	 * Bible reading pane): enabling WebKit caret browsing on several
	 * WkHtml/WebKitWebView instances at once (e.g. also commentary,
	 * dictionary, sidebar) was observed to break keyboard caret
	 * navigation (Down/Page_Down) in the active pane, even though
	 * that pane keeps receiving every key event normally. Restricting
	 * to the single pane users actually asked to navigate in this way
	 * avoids the interference. */
	if (pane == TEXT_TYPE || pane == COMMENTARY_TYPE)
		wk_html_enable_caret_browsing(WK_HTML(html));

	return html;
}
