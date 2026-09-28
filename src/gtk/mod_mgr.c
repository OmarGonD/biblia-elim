/*
 * Xiphos Bible Study Tool
 * mod_mgr.c
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>

#include "gui/sqlite_module_manager_dialog.h"
#include <gtk/gtk.h>
#include "gui/widget_helpers.h"
#include "gui/dropdown_helpers.h"
#include <gdk/gdkkeysyms.h>

#include "gui/mod_mgr.h"
#include "gui/dialog.h"
#include "gui/sidebar.h"
#include "gui/utilities.h"
#include "gui/widgets.h"
#include "gui/about_modules.h"

#include "main/lists.h"
#include "main/mod_mgr.h"
#include "main/settings.h"
#include "main/sidebar.h"
#include "main/sword.h"
#include "main/xml.h"

#include "main/modulecache.hh"
#include "backend/module_manager.hh"

#include "gui/debug_glib_null.h"

#include <glib/gstdio.h>

/******************************************************************************
 * defines
 */
#define XI_GENERAL_INTRO \
	_("<b>Overview of the Module Manager.</b>\n\nThis is Xiphos' mechanism to get new and updated content.\nIf you have never been here before, please take a moment to look it over.\n\nModules come from different <u>repositories</u>.  <b>Module Sources: Add/Remove</b> will show you what repositories are currently known.\n\n<b>Module Sources: Choose</b> is for deciding from where modules should come, that is, from which repository Xiphos should obtain them, as well as where they should be placed on your system. Set <i>Install Source</i> and <i>Install Destination</i>, then click <i>Refresh</i>.\n\n<b>Modules: Install/Update</b> is for selecting and obtaining modules after choosing source and destination.\n\n<b>Modules: Maintenance</b> is for archive and index creation.\n\nSee section 5 of our manual for Module Manager detail, or ask for help via Live Chat, or (if no one is responsive in chat) send mail to our users' mailing list.\n")

#define XI_FIRST_INSTALL \
	_("<b>Welcome to Xiphos.</b>\n\nThere are no Bibles installed. In order to initialize, Xiphos needs at least one Bible module. To facilitate this, the Module Manager has been opened so that you may install one or more Bibles, either from a local module set (cdrom, flash drive) or over the network from CrossWire Bible Society. Please refer to these step-by-step instructions, and to the general module manager overview that has also been opened.\n\n<u>For local install:</u>\n- In <i>Module Sources: Add/Remove</i>, add a new local folder name where modules can be found.\n  (This is where folders exist named <i>mods.d</i> and <i>modules</i>.)\n- In <i>Module Sources: Choose</i>, click the \"Local\" button, and select your folder from the pulldown.\n\n<u>For network install from CrossWire:</u>\n- In <i>Module Sources: Choose</i>, click the \"Remote\" button and select CrossWire from the pulldown.\n- Click the \"Refresh\" button at the bottom.\n\n<u>In either case:</u>\n- In <i>Modules: Install/Update</i>, select Bibles and other modules of your preference.\n- Click \"Install\".\n- Close the Module Manager when you are done.\n\n<u>Warning</u>: If you live in a persecuted country, use with care.\n\nBoth this step-by-step instruction dialog and the general introduction dialog may be closed at any time.")

#define GTK_RESPONSE_REFRESH 301
#define GTK_RESPONSE_REMOVE 302
#define GTK_RESPONSE_INSTALL 303
#define GTK_RESPONSE_ARCHIVE 304
#define GTK_RESPONSE_FASTMOD 305
#define GTK_RESPONSE_DELFAST 306
#define GTK_RESPONSE_SOURCES 307
#define GTK_RESPONSE_INTRO 308
#define GTK_RESPONSE_OBSOLETE 309
/* see these codes' use in ui/module-manager.gtkbuilder. */

/* activity codes */
enum {
	ALL_MODULES = -1, /* for obsolescence search-&-destroy */
	REMOVE = 0,
	INSTALL = 1,
	ARCHIVE = 2,
	FASTMOD = 3,
	DELFAST = 4,
};

/* dialog & progress bar phrases */
enum {
	PHRASE_INQUIRY = 0,
	PHRASE_PREPARE = 1,
	PHRASE_DOING = 2,
	PHRASE_COMPLETE = 3,
};

/* treeview columns */
enum {
	COLUMN_NAME,
	COLUMN_INSTALLED,
	COLUMN_FIXED,
	COLUMN_INSTALLED_VERSION,
	COLUMN_FASTREADY,
	COLUMN_LOCKED,
	COLUMN_ABOUT,
	COLUMN_DIFFERENT,
	COLUMN_AVAILABLE_VERSION,
	COLUMN_INSTALLSIZE,
	COLUMN_DESC,
	COLUMN_VISIBLE,
	NUM_COLUMNS
};

/* new install source dialog fields */
enum {
	COLUMN_TYPE,
	COLUMN_CAPTION,
	COLUMN_SOURCE,
	COLUMN_DIRECTORY,
	COLUMN_USER,
	COLUMN_PASS,
	COLUMN_UID,
	NUM_REMOTE_COLUMNS
};

static GtkWidget *treeview;
static GtkWidget *treeview1;
static GtkWidget *treeview2;
static GtkWidget *notebook1;
static GtkWidget *button_close;
static GtkWidget *button_cancel;
static GtkWidget *button_refresh;
static GtkWidget *button_install;
static GtkWidget *button_remove;
static GtkWidget *button_add_local;
static GtkWidget *button_remove_local;
static GtkWidget *button_add_remote;
static GtkWidget *button_remove_remote;
static GtkWidget *button_arch;
static GtkWidget *button_idx;
static GtkWidget *button_delidx;
static GtkWidget *button_obsolete;
static GtkWidget *button_load_sources;
static GtkWidget *button_intro;
static GtkWidget *label_home;
static GtkWidget *label_system;
static GtkWidget *progressbar_refresh;
static GtkWidget *progressbar_alt;
static GtkWidget *radiobutton_source;
static GtkWidget *radiobutton2;
static GtkWidget *radiobutton_dest;
static GtkWidget *radiobutton4;
static GtkWidget *combo_entry1;
static GtkWidget *combo_entry2;
static GtkWidget *dialog_modmgr;
static GtkWidget *treeview_local;
static GtkWidget *treeview_remote;

static gboolean local;
static const gchar *source;
static const gchar *destination;
static gboolean have_configs;
static gint current_page;
/* what the icon columns of the module trees show; a blank is no image */
static GdkTexture *INSTALLED;
static GdkTexture *FASTICON;
static GdkTexture *NO_INDEX;
static GdkTexture *LOCKED;
static GdkTexture *REFRESH;
#define BLANK ((GdkTexture *)NULL)
static gchar *current_mod;
static gchar *remote_source;
static gboolean first_time_user = FALSE;
static GMainLoop *initial_run_loop = NULL;
static gboolean working = FALSE;
static gboolean is_running = FALSE;

GtkBuilder *gxml;

static void load_module_tree(GtkWidget *treeview, gboolean install);
static void set_controls_to_last_use(void);
static int load_source_treeviews(void);

/* indexed by REMOVE/INSTALL/ARCHIVE/FASTMOD/DELFAST and PHRASE_* */
char *verbs[5][4] = {
    {N_("Remove these modules?"),
     N_("Preparing to remove"),
     N_("Removing"),
     N_("Remove")},
    {N_("Install these modules?"),
     N_("Preparing to install"),
     N_("Installing"),
     N_("Install")},
    {N_("Archive these modules?"),
     N_("Preparing to archive"),
     N_("Archiving"),
     N_("Archive")},
    {N_("Build fast-search index for these\nmodules (may take minutes/module)?"),
     N_("Preparing to index"),
     N_("Indexing"),
     N_("Index")},
    {N_("Delete fast-search index for these modules?"),
     N_("Preparing to delete index"),
     N_("Deleting index"),
     N_("Deletion")},
};

/******************************************************************************
 * Name
 *   on_modmgr_configure_event
 *
 * Synopsis
 *   #include "gui/main_window.h"
 *
 *   gboolean on_modmgr_configure_event(GtkWidget * widget,
 *				   GdkEventConfigure * event,
 *				   gpointer user_data)
 *
 * Description
 *   remember placement+size of modmgr window.
 *   cloned from on_configure_event
 *
 * Return value
 *   gboolean
 */

static void on_modmgr_configure_event(GObject *window, GParamSpec *pspec,
				      gpointer user_data)
{
	gchar layout[10];
	gint width, height;

	(void)pspec;
	(void)user_data;
	/* GTK 4 keeps the window's size as its default size; the position
	 * belongs to the compositor */
	gtk_window_get_default_size(GTK_WINDOW(window), &width, &height);
	if (width <= 0 || height <= 0)
		return;
	settings.modmgr_width = width;
	settings.modmgr_height = height;

	sprintf(layout, "%d", settings.modmgr_width);
	xml_set_value("Xiphos", "layout", "modmgr_width", layout);

	sprintf(layout, "%d", settings.modmgr_height);
	xml_set_value("Xiphos", "layout", "modmgr_height", layout);
	xml_save_settings_doc(settings.fnconfigure);
}

static gboolean module_tooltip(ElimRow *row, GtkTooltip *tooltip,
			       gpointer user_data)
{
	GdkPixbuf *pixbuf;
	gchar *about;
	gchar *version;
	const gchar *desc;
	GString *str = g_string_new(NULL);
	GString *text = g_string_new(NULL);
	GString *description = g_string_new(NULL);

	(void)user_data;
	if (elim_row_n_children(row)) {
		g_string_free(str, TRUE);
		g_string_free(text, TRUE);
		g_string_free(description, TRUE);
		return FALSE;
	}

	desc = elim_row_get_string(row, COLUMN_DESC);
	if (!*desc) {
		g_string_free(str, TRUE);
		g_string_free(text, TRUE);
		g_string_free(description, TRUE);
		return FALSE;
	}
	about = g_strdup(elim_row_get_string(row, COLUMN_ABOUT));
	version = *elim_row_get_string(row, COLUMN_AVAILABLE_VERSION)
		      ? g_strdup(elim_row_get_string(row, COLUMN_AVAILABLE_VERSION))
		      : NULL;

	g_strdelimit(about, "&", '+');
	g_string_printf(description,
			"%s\n%s %s\n\n",
			desc,
			(version) ? "Sword module version" : "",
			(version) ? version : "");

	about_module_display(str, (*about
				       ? about
				       : _("The module has no About information.")),
			     TRUE);

	text =
	    g_string_append_len(text, description->str, description->len);
	text = g_string_append_len(text, str->str, str->len);
	if (text->len > 1200) {
		text = g_string_truncate(text, 1200);
		text = g_string_append_len(text, " ...", strlen(" ..."));
	}
	pixbuf = pixbuf_finder("sword3.png", 0, NULL);
	if (pixbuf) {
		GdkTexture *icon = gdk_texture_new_for_pixbuf(pixbuf);
		gtk_tooltip_set_icon(tooltip, GDK_PAINTABLE(icon));
		g_object_unref(icon);
	}
	gtk_tooltip_set_text(tooltip, text->str);

	g_free(about);
	g_free(version);
	g_string_free(str, TRUE);
	g_string_free(text, TRUE);
	g_string_free(description, TRUE);
	return TRUE;
}

/******************************************************************************
 * Name
 *   create_pixbufs
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void create_pixbufs(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static GdkTexture *icon_texture(const gchar *name)
{
	GdkPixbuf *pixbuf = theme_icon_pixbuf(name, 16);
	GdkTexture *texture = pixbuf ? gdk_texture_new_for_pixbuf(pixbuf) : NULL;

	if (pixbuf)
		g_object_unref(pixbuf);
	return texture;
}

static void create_pixbufs(void)
{
	INSTALLED = icon_texture("emblem-default");
	FASTICON = icon_texture("edit-find-symbolic");
	NO_INDEX = icon_texture("_Cancel");
	LOCKED = icon_texture("changes-prevent-symbolic");
	REFRESH = icon_texture("view-refresh-symbolic");
}

/******************************************************************************
 * Name
 *   fixed_toggled
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void fixed_toggled (ElimRow *row, gpointer data)
 *
 * Description
 *   the reader clicked the box of a module: it is chosen, or not
 *
 * Return value
 *   void
 */

static void
fixed_toggled(ElimRow *row, gpointer data)
{
	(void)data;
	elim_row_set_int(row, COLUMN_FIXED,
			 !elim_row_get_int(row, COLUMN_FIXED));
}

/******************************************************************************
 * Name
 *   setup_module_tree_view
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void setup_module_tree_view (GtkWidget *view, gboolean remove)
 *
 * Description
 *   VIEW (a GtkColumnView) shows a tree of modules: the name, whether it is
 *   installed, a box to choose it, its versions, whether it has an index or
 *   is locked, and (when installing) whether it needs an update, its size and
 *   its description. The icons explain themselves when hovered.
 *
 * Return value
 *   void
 */

static void setup_module_tree_view(GtkWidget *view, gboolean remove)
{
	GListStore *roots = elim_table_new();
	ElimTextColumn text;
	ElimColumn col;

	elim_tree_setup(view, roots);
	g_object_unref(roots);

	/* -- column for sword module name -- */
	text = elim_text_column(COLUMN_NAME);
	text.expander = TRUE;
	text.fixed_width = 275;
	elim_table_add_column(view, _("Module Name"), &text);

	/* -- installed -- */
	elim_table_add_image_column(view, _("Installed"), COLUMN_INSTALLED, 16,
				    remove ? NULL
					   : _("A checkmark means this module is already installed"));

	/* -- toggle choice -- */
	col = elim_column_toggle(COLUMN_FIXED, fixed_toggled, NULL);
	col.visible_column = COLUMN_VISIBLE;
	col.tooltip = remove ? _("Click the box to work on this module")
			     : _("Click the box to select this module for install/update");
	elim_table_add_cell_column(view, "", &col);

	/* -- installed version -- */
	elim_table_add_text_column(view, _("Installed"), COLUMN_INSTALLED_VERSION, FALSE);

	/* -- fast index ready -- */
	elim_table_add_image_column(view, _("Index"), COLUMN_FASTREADY, 16,
				    _("The index icon means you have built an optimized ('lucene') index for this module for fast searching (see the Maintenance pane for this function)"));

	/* -- locked -- */
	elim_table_add_image_column(view, _("Lock"), COLUMN_LOCKED, 16,
				    _("The lock icon means this module is encrypted, and requires that you purchase an unlock key from the content owner"));

	if (remove)
		return; /* no more fields needed */

	/* -- refresh/update -- */
	elim_table_add_image_column(view, _("Update"), COLUMN_DIFFERENT, 16,
				    _("The refresh icon means the Installed module is older than the newer Available module: You should update the module"));

	/* -- available version -- */
	elim_table_add_text_column(view, _("Available"), COLUMN_AVAILABLE_VERSION, FALSE);

	/* -- install size -- */
	elim_table_add_text_column(view, _("Size"), COLUMN_INSTALLSIZE, FALSE);

	/* -- description -- */
	elim_table_add_text_column(view, _("Description"), COLUMN_DESC, TRUE);
}

static void setup_treeview_install(GtkWidget *install)
{
	setup_module_tree_view(install, FALSE);
	elim_table_set_tooltip_func(install, module_tooltip, NULL);
}

static void setup_treeview_maintenance(GtkWidget *maintenance)
{
	setup_module_tree_view(maintenance, TRUE);
	elim_table_set_tooltip_func(maintenance, module_tooltip, NULL);
}

/******************************************************************************
 * Name
 *   mod_mgr_check_for_file
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   gboolean mod_mgr_check_for_file(const gchar * filename)
 *
 * Description
 *
 *
 * Return value
 *   gboolean
 */

static gboolean mod_mgr_check_for_file(const gchar *filename)
{
	return g_file_test(filename, G_FILE_TEST_EXISTS);
}

/******************************************************************************
 * Name
 *   remove_install_modules
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *    void remove_install_modules(GList * modules, int activity)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

#define ZIP_DIR DOTSWORD "/zip"

static void remove_install_modules(GList *modules, int activity)
{
	GList *tmp;
	gchar *buf;
	const gchar *new_dest = NULL;
	gint result = 1;
	GString *mods;
	gchar *dialog_text = NULL;
	gchar *preserved_cipherkey = NULL;

	if (!modules)
		return;

	mods = g_string_new("");
	tmp = modules;
	while (tmp) {
		buf = (gchar *)tmp->data;
		if (buf[0] == '*')
			++buf;
		if (*(mods->str)) // not the first
			mods = g_string_append(mods, ", ");
		mods = g_string_append(mods, buf);
		tmp = g_list_next(tmp);
	}

	dialog_text =
	    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s",
			    gettext(verbs[activity][PHRASE_INQUIRY]),
			    mods->str);

	if (!gui_yes_no_dialog(dialog_text, NULL)) {
		g_list_free_full(modules, g_free);
		g_free(dialog_text);
		return;
	}
	g_free(dialog_text);

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
				  gettext(verbs[activity]
					       [PHRASE_PREPARE]));
	gtk_widget_show(progressbar_refresh);
	gtk_widget_queue_draw(dialog_modmgr);
	gtk_widget_hide(button_close);
	gtk_widget_hide(button_refresh);
	gtk_widget_hide(button_install);
	gtk_widget_hide(button_remove);
	gtk_widget_hide(button_arch);
	gtk_widget_hide(button_idx);
	gtk_widget_hide(button_delidx);
	gtk_widget_hide(button_obsolete);
	gtk_widget_hide(button_load_sources);
	gtk_widget_show(button_cancel);

	tmp = modules;
	while (tmp) {
		char *module_name, *s;

		buf = (gchar *)tmp->data;

		/* for "Abbreviation (Real)", we must */
		/* get the real name for internal use. */
		if ((s = strchr(buf, '(')) != NULL) {
			module_name = g_strdup(s + 1);
			*(strchr(module_name, ')')) = '\0';
		} else
			module_name = g_strdup(buf); /* no abbreviation */

		current_mod = module_name;
		g_string_printf(mods, "%s: %s",
				gettext(verbs[activity][PHRASE_DOING]),
				buf);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
					  mods->str);
		sync_windows();

		if (activity == ARCHIVE) {
			GString *cmd = g_string_new(NULL);
			gchar *dir;
			gchar *zipfile;
			char *datapath, *conf_file;
			int dlen;

			XI_print(("archive %s in %s\n", buf,
				  (destination
				       ? destination
				       : settings.path_to_mods)));
			dir =
			    g_strdup_printf("%s/%s", settings.homedir,
					    ZIP_DIR);
			if ((g_access(dir, F_OK) == -1) && (g_mkdir(dir, S_IRWXU) != 0)) {
				char msg[300];
				sprintf(msg, _("`mkdir %s' failed:\n%s."),
					dir, strerror(errno));
				g_free(dir);
				gui_generic_warning(msg);
				g_free(module_name);
				return;
			}

			zipfile =
			    g_strdup_printf("%s/%s.zip", dir, module_name);
			datapath =
			    main_get_mod_config_entry(module_name,
						      "DataPath");

			// for modules whose DataPath ends in .../xyz/abc
			// where abc is the file prefix (abc.dat, abc.idx, &c)
			// we have to detect this and eliminate the end.
			if (g_access(datapath, F_OK) == -1)
				*(strrchr(datapath, '/')) = '\0';

			// lastly, if the end char is '/', remove it or
			// it becomes a problem when analyzing path length.
			dlen = strlen(datapath);
			if (datapath[dlen - 1] == '/')
				datapath[dlen - 1] = '\0';

			conf_file = main_get_mod_config_file(module_name, (destination
									       ? destination
									       : settings.path_to_mods));
			g_remove(zipfile);

			xiphos_create_archive(conf_file, datapath, zipfile,
					      destination ? destination : settings.path_to_mods);
			g_string_append(cmd, module_name);
			g_string_append(cmd, _(" archived in: \n"));
			g_string_append(cmd, zipfile);
			gui_generic_warning(cmd->str);
			g_free(conf_file);
			g_free(datapath);
			g_free(zipfile);
			g_free(dir);
			g_string_free(cmd, TRUE);
			result = 0;
		}

		if ((activity == REMOVE) ||				      // just delete it
		    (!first_time_user &&				      // don't trip on "no modules".
		     (activity == INSTALL) && main_is_module(module_name))) { // delete before re-install
			XI_print(("remove %s from %s\n", module_name,
				  (destination
				       ? destination
				       : settings.path_to_mods)));

			/* hang onto the old key, if available. */
			preserved_cipherkey =
			    main_get_mod_config_entry(module_name,
						      "CipherKey");
			if (preserved_cipherkey && (*preserved_cipherkey == '\0')) {
				/* present but empty -> nothingness */
				g_free(preserved_cipherkey);
				preserved_cipherkey = NULL;
			}

			result =
			    mod_mgr_uninstall(destination, module_name);
			if (result == -1) {
				if (destination) {
					new_dest = NULL;
				} else {
					new_dest = gtk_label_get_text(GTK_LABEL(label_home));
				}
				XI_print(("removing %s from %s\n",
					  module_name,
					  (new_dest
					       ? new_dest
					       : settings.path_to_mods)));
				result =
				    mod_mgr_uninstall(new_dest,
						      module_name);
			}
			// annihilate cache of removed module.
			ModuleCacheErase((const char *)module_name);
			/* and the SQLite copy converted from it; a reinstall
			 * converts it again afterwards. */
			if (result != -1)
				gui_forget_converted_sword_bible(module_name);
		}

		if (activity == INSTALL) {
			XI_print(("install %s, source=%s\n", buf, source));
			result = ((local)
				      ? mod_mgr_local_install_module(destination,
								     source,
								     module_name)
				      : mod_mgr_remote_install(destination,
							       source,
							       module_name));

			/* do gtk refresh, to make sure last messages display */
			sync_windows();

			/* try to re-use a saved key. */
			if ((result != -1) && preserved_cipherkey) {
				main_save_module_key(module_name,
						     preserved_cipherkey);
				XI_print(("re-use key %s\n",
					  preserved_cipherkey));
			}

			/* ask to eliminate old/dead/obsolete modules. */
			if (result != -1)
				delete_obsolete(module_name, NULL);
		}

		if (activity == FASTMOD) {
			XI_print(("index %s\n", buf));
			result =
			    ((main_module_mgr_index_mod(module_name))
				 ? 0
				 : 1);
		}

		if (activity == DELFAST) {
			XI_print(("deleting index %s\n", buf));
			result =
			    ((main_module_mgr_delete_index_mod(module_name))
				 ? 0
				 : 1);
		}

		if (preserved_cipherkey) {
			g_free(preserved_cipherkey);
			preserved_cipherkey = NULL;
		}

		g_free(module_name);
		g_free(tmp->data);
		tmp = g_list_next(tmp);
	}
	current_mod = NULL;
	if (!first_time_user) {
		main_update_module_lists();
		main_load_module_tree(sidebar.module_list);
		/* New SWORD Bibles are read from their SQLite copy. */
		if (activity == INSTALL)
			gui_convert_pending_sword_bibles();
	}
	g_list_free(modules);

	if (result) {
		g_string_printf(mods, _("%s failed"),
				gettext(verbs[activity][PHRASE_COMPLETE]));
	} else {
		g_string_printf(mods, "%s", _("Finished"));
	}

	if (!result &&
	    ((activity == REMOVE) ||
	     (activity == FASTMOD) || (activity == DELFAST))) {
		load_module_tree(treeview2, 0);
	}

	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
				  mods->str);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_refresh), 0);
	g_string_free(mods, TRUE);
	gtk_widget_hide(button_cancel);
	gtk_widget_show(button_close);
	switch (current_page) {
	case 3:
		gtk_widget_show(button_refresh);
		gtk_widget_show(button_install);
		gtk_widget_hide(button_arch);
		gtk_widget_hide(button_idx);
		gtk_widget_hide(button_delidx);
		gtk_widget_hide(button_obsolete);
		gtk_widget_hide(button_load_sources);
		break;
	case 4:
		gtk_widget_show(button_remove);
		gtk_widget_show(button_arch);
		gtk_widget_show(button_idx);
		gtk_widget_show(button_delidx);
		gtk_widget_show(button_obsolete);
		gtk_widget_hide(button_load_sources);
		break;
	}
	sync_windows();
}

/******************************************************************************
 * Name
 *   parse_treeview
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *    GList *parse_treeview(GList *list, ElimRow * tree_parent, int activity)
 *
 * Description
 *
 *
 * Return value
 *   GList *
 */

static GList *parse_treeview(GList *list,
			     ElimRow *tree_parent, int activity)
{
	guint i;

	for (i = 0; i < elim_row_n_children(tree_parent); i++) {
		ElimRow *child = elim_row_get_child(tree_parent, i);
		gboolean fixed = elim_row_get_int(child, COLUMN_FIXED);
		const gchar *name = elim_row_get_string(child, COLUMN_NAME);

		if (elim_row_n_children(child)) {
			list = parse_treeview(list, child, activity);
		} else {
			/* handle the abbreviated case, "abbrev (real)". */
			const char *s = strchr(name, '(');

			if (fixed || (activity == ALL_MODULES)) {
				if (s) {
					gchar *n = g_strdup(s + 1);
					gchar *close = strchr(n, ')');

					if (close)
						*close = '\0';
					list = g_list_append(list, n);
				} else {
					list = g_list_append(list, g_strdup(name));
				}
			}
		}
	}

	return list;
}

/******************************************************************************
 * Name
 *   get_list_mods_to_remove_install
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void get_list_mods_to_remove_install(void)
 *
 * Description
 *   return a GList of modules, either all or those checked.
 *
 * Return value
 *   void
 */

static GList *get_list_mods_to_remove_install(int activity)
{
	GList *retval = NULL;
	GListStore *roots =
	    elim_table_get_store(activity == INSTALL ? treeview : treeview2);
	guint i;

	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(roots)); i++) {
		ElimRow *root = elim_table_get(roots, i);

		if (elim_row_n_children(root)) {
			retval = parse_treeview(retval, root, activity);
		} else if (elim_row_get_int(root, COLUMN_FIXED) ||
			   (activity == ALL_MODULES)) {
			retval = g_list_append(retval,
					       g_strdup(elim_row_get_string(root, COLUMN_NAME)));
		}
	}

	return retval;
}

/******************************************************************************
 * Name
 *   add_module_to_language_folder
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void add_module_to_language_folder(ElimRow * folder, GListStore * store,
 *		      MOD_MGR *info, gboolean checkmark)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void
add_module_to_language_folder(ElimRow *folder, GListStore *store,
			      MOD_MGR *info, gboolean checkmark)
{
	guint i;
	GdkTexture *installed;
	GdkTexture *fasticon;
	GdkTexture *locked;
	GdkTexture *refresh;
	const gchar *description = NULL;

	if (!folder)
		return;

	/* Check language */
	const gchar *buf = info->language;
	if (!g_utf8_validate(buf, -1, NULL))
		info->language = _("Unknown");
	if (!g_unichar_isalnum(g_utf8_get_char(buf)) || (info->language == NULL))
		info->language = _("Unknown");

	description = info->description;

	for (i = 0; i < elim_row_n_children(folder); i++) {
		/* Walk through the list, reading each row */
		ElimRow *language_folder = elim_row_get_child(folder, i);

		if (!strcmp(info->language, elim_row_get_string(language_folder, COLUMN_NAME))) {
			ElimRow *row;
			gchar *caption;

			installed =
			    ((info->installed) ? INSTALLED : BLANK);

			locked = ((info->locked) ? LOCKED : BLANK);

			if ((info->installed &&
			     (!info->old_version && info->new_version &&
			      strcmp(info->new_version, " "))) ||
			    ((info->old_version && !info->new_version)) ||
			    ((info->old_version && info->new_version &&
			      strcmp(info->new_version,
				     info->old_version) > 0)))
				refresh = REFRESH;
			else
				refresh = BLANK;

			if (!first_time_user &&
			    info->installed &&
			    main_has_search_framework(info->name)) {
				if (main_optimal_search(info->name))
					fasticon = FASTICON;
				else
					fasticon = NO_INDEX;
			} else
				fasticon = BLANK;

			row = elim_tree_append(store, language_folder, NUM_COLUMNS);

			if (info->abbreviation)
				caption = g_strdup_printf("%s (%s)",
							  info->abbreviation,
							  info->name);
			else
				caption = g_strdup(info->name);

			elim_row_set_string(row, COLUMN_NAME, caption);
			elim_row_set_object(row, COLUMN_INSTALLED,
					    G_OBJECT(checkmark ? installed : BLANK));
			elim_row_set_int(row, COLUMN_FIXED, FALSE);
			elim_row_set_string(row, COLUMN_INSTALLED_VERSION, info->old_version);
			elim_row_set_object(row, COLUMN_FASTREADY, G_OBJECT(fasticon));
			elim_row_set_object(row, COLUMN_LOCKED, G_OBJECT(locked));
			elim_row_set_string(row, COLUMN_ABOUT, info->about);
			elim_row_set_object(row, COLUMN_DIFFERENT, G_OBJECT(refresh));
			elim_row_set_string(row, COLUMN_AVAILABLE_VERSION, info->new_version);
			elim_row_set_string(row, COLUMN_INSTALLSIZE, info->installsize);
			elim_row_set_string(row, COLUMN_DESC, description);
			elim_row_set_int(row, COLUMN_VISIBLE, TRUE);
			g_free(caption);
			return;
		}
	}
}

/******************************************************************************
 * Name
 *   language_add_folders
 *
 * Synopsis
 *   #include "main/sidebar.h"
 *
 *   void language_add_folders(GListStore * roots, ElimRow * folder,
 *			       gchar ** languages)
 *
 * Description
 *   insert a block of languages into a tree model.
 *
 * Return value
 *   void
 */

static void
language_add_folders(GListStore *roots,
		     ElimRow *folder, gchar **languages)
{
	int j;

	if (!folder)
		return;
	for (j = 0; languages[j]; ++j) {
		ElimRow *row = elim_tree_append(roots, folder, NUM_COLUMNS);

		elim_row_set_int(row, COLUMN_VISIBLE, FALSE);
		elim_row_set_string(row, COLUMN_NAME,
				    ((g_utf8_validate(languages[j], -1, NULL))
					 ? languages[j]
					 : _("Unknown")));
	}
}

/******************************************************************************
 * Name
 *   add_language_folder
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void add_language_folder(GListStore * store, ElimRow * folder,
 *			      gchar * language)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void
add_language_folder(GListStore *store,
		    ElimRow *folder, const gchar *language)
{
	guint i;
	ElimRow *row;

	/* Check language */
	const gchar *buf = language;
	if (!g_utf8_validate(buf, -1, NULL))
		language = _("Unknown");
	if (!g_unichar_isalnum(g_utf8_get_char(buf)) || (language == NULL))
		language = _("Unknown");

	for (i = 0; i < elim_row_n_children(folder); i++) {
		/* Walk through the list, reading each row */
		gchar *wanted = g_utf8_casefold(language, -1);
		gchar *have = g_utf8_casefold(
		    elim_row_get_string(elim_row_get_child(folder, i), COLUMN_NAME), -1);
		gboolean same = !g_utf8_collate(wanted, have);

		g_free(wanted);
		g_free(have);
		if (same)
			return;
	}
	row = elim_tree_append(store, folder, NUM_COLUMNS);
	elim_row_set_int(row, COLUMN_VISIBLE, FALSE);
	elim_row_set_string(row, COLUMN_NAME, language);
}

static gboolean
on_modules_list_button_release(GtkWidget *widget,
			       GuiButtonEvent *event, gpointer data)
{
	ElimRow *selected;

	/*
	 * anti-bobble control.
	 * this is kinda silly.  theater of the absurd.
	 * especially in the 1st-time user's mod.mgr, hitting the row text
	 * often calls this routine twice.  why?  we don't know.
	 * effect is rapid open+close, which is visually nothing at all.
	 * this timer use is to avoid reacting to the 2nd call when it
	 * happens too soon.
	 */
	static GTimer *t = NULL;
	static gdouble el = -1.0; /* sentinel */

	if (!t)
		t = g_timer_new();
	else
		el = g_timer_elapsed(t, NULL);
	g_timer_start(t);
	if ((el != -1.0) && (el < 0.1)) {
		XI_message(("button bobble reject"));
		return FALSE;
	}
	/* end of anti-bobble control. */

	selected = elim_table_get_selected(GTK_WIDGET(data));
	if (!selected || !elim_row_n_children(selected))
		return FALSE;

	/* the arrow opens and closes its row by itself */
	if (elim_tree_point_on_expander(GTK_WIDGET(data), event->x, event->y))
		return FALSE;
	if (elim_tree_row_expanded(GTK_WIDGET(data), selected))
		elim_tree_collapse_row(GTK_WIDGET(data), selected);
	else
		elim_tree_expand_row(GTK_WIDGET(data), selected, FALSE);
	return FALSE;
}

static gboolean
on_modules_list_key_press(GtkWidget *widget,
                          GuiKeyEvent *event, gpointer data)
{
    ElimRow *selected = elim_table_get_selected(widget);

    if (!selected || !elim_row_n_children(selected))
        return FALSE;

    if (event->keyval == GDK_KEY_Right)
        elim_tree_expand_row(widget, selected, FALSE);
    else if (event->keyval == GDK_KEY_Left)
        elim_tree_collapse_row(widget, selected);
    else
        return FALSE;

    return TRUE;
}

/* A row of a folder: just a caption. */
static ElimRow *add_folder_row(GListStore *store, const gchar *caption)
{
	ElimRow *row = elim_tree_append(store, NULL, NUM_COLUMNS);

	elim_row_set_string(row, COLUMN_NAME, caption);
	return row;
}

/******************************************************************************
 * Name
 *   load_module_tree
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void load_module_tree(GtkWidget * treeview, gboolean install)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void load_module_tree(GtkWidget *treeview, gboolean install)
{
	GListStore *store;
	ElimRow *repository_name;
	ElimRow *category_type;
	ElimRow *category_avail;

	ElimRow *text = NULL;
	gboolean need_text = 0;
	ElimRow *commentary = NULL;
	gboolean need_commentary = 0;
	ElimRow *dictionary = NULL;
	gboolean need_dictionary = 0;
	ElimRow *devotional = NULL;
	gboolean need_devotional = 0;
	ElimRow *book = NULL;
	gboolean need_book = 0;
	ElimRow *map = NULL;
	gboolean need_map = 0;
	ElimRow *image = NULL;
	gboolean need_image = 0;
	ElimRow *cult = NULL;
	gboolean need_cult = 0;
	ElimRow *glossary = NULL;
	gboolean need_glossary = 0;
	ElimRow *prayerlist = NULL;
	gboolean need_prayerlist = 0;

	ElimRow *separator;
	ElimRow *update = NULL;
	ElimRow *uninstalled = NULL;
	ElimRow *unindexed = NULL;
	gboolean first_unindexed = FALSE;

	GList *tmp = NULL;
	GList *tmp2 = NULL;

	if (install) {
		if (gui_toggle_get_active(GTK_WIDGET(radiobutton_source))) {
			local = TRUE;
			source =
			    elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo_entry1));

			// must find the directory attached to the name.
			// they may (and normally will) be the same,
			// but it's not certain.
			tmp = tmp2 = mod_mgr_list_local_sources();
			while (tmp) {
				MOD_MGR_SOURCE *mms =
				    (MOD_MGR_SOURCE *)tmp->data;

				if (!strcmp(source, mms->caption))
					source = g_strdup(mms->directory);

				g_free((gchar *)mms->type);
				g_free((gchar *)mms->caption);
				g_free((gchar *)mms->source);
				g_free((gchar *)mms->directory);
				g_free((gchar *)mms->user);
				g_free((gchar *)mms->pass);
				g_free((gchar *)mms->uid);
				g_free(mms);
				tmp = g_list_next(tmp);
			}
			g_list_free(tmp2);

			tmp = mod_mgr_list_local_modules(source, FALSE);
			// false -> tell installmgr not to mess with ~/.sword content.
		} else {
			local = FALSE;
			source =
			    elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo_entry2));
			tmp = mod_mgr_remote_list_modules(source);
		}
	} else {
		// we are doing maintenance on all modules.
		tmp =
		    mod_mgr_list_local_modules(settings.path_to_mods,
					       TRUE);
		// true -> we must have all modules available, incl. ~/.sword content.
	}

	// find which folders are needed.
	tmp2 = tmp;
	while (tmp2) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;
		if (info->is_cult)
			need_cult++;
		else if (info->type[0] == 'B')
			need_text++;
		else if (info->type[0] == 'C')
			need_commentary++;
		else if (info->is_maps)
			need_map++;
		else if (info->is_images)
			need_image++;
		else if (info->is_devotional)
			need_devotional++;
		else if (info->is_glossary)
			need_glossary++;
		else if (info->type[0] == 'L')
			need_dictionary++;
		else if (info->is_prayerlist)
			need_prayerlist++;
		else if (info->type[0] == 'G')
			need_book++;
		else {
			XI_warning(("mod `%s' unknown type `%s'",
				    info->name, info->type));
		}
		tmp2 = g_list_next(tmp2);
	}

	/* the tree is made apart, then shown at once */
	store = elim_table_new();

	if (!g_list_length(tmp)) {
		elim_tree_replace(elim_table_get_store(treeview), store);
		g_object_unref(store);
		return;
	}

	if (install) {
		/* note the repository that is active */
		if ((local == FALSE) && (remote_source == NULL)) {
			remote_source =
			    g_strdup(elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo_entry2)));
		}
		gchar *repository_identifier =
		    g_strdup_printf(_("Repository:\n%s"),
				    (local ? source : remote_source));
		repository_name = add_folder_row(store, repository_identifier);
		(void)repository_name;
		g_free(repository_identifier);

		separator = add_folder_row(store, "------------------------");
		(void)separator;

		category_type = add_folder_row(store, _("Categorized by\nModule Type"));
		(void)category_type;
	}

	/* add only those folders actually represented. */

	/*  add Biblical Texts folder */
	if (need_text)
		text = add_folder_row(store, _("Biblical Texts"));

	/*  add Commentaries folder */
	if (need_commentary)
		commentary = add_folder_row(store, _("Commentaries"));

	/*  add Dictionaries folder */
	if (need_dictionary)
		dictionary = add_folder_row(store, _("Dictionaries"));

	/*  add Glossaries folder */
	if (need_glossary)
		glossary = add_folder_row(store, _("Glossaries"));

	/*  add Devotionals folder */
	if (need_devotional)
		devotional = add_folder_row(store, _("Daily Devotionals"));

	/*  add Books folder */
	if (need_book)
		book = add_folder_row(store, _("General Books"));

	/*  add Maps folder */
	if (need_map)
		map = add_folder_row(store, _("Maps"));

	/*  add Images folder */
	if (need_image)
		image = add_folder_row(store, _("Images"));

	/*  add Cult folder */
	if (need_cult)
		cult = add_folder_row(store, _("Cult/Unorthodox"));

	if (install && !first_time_user) {
		separator = add_folder_row(store, "------------------------");
		(void)separator;

		category_avail = add_folder_row(store, _("Categorized by\nAvailability"));
		(void)category_avail;

		/*  add Updates folder */
		update = add_folder_row(store, _("Updates"));

		/*  add Uninstalled folder */
		uninstalled = add_folder_row(store, _("Uninstalled"));
	} else {
		/* add Journal/PrayerList folder */
		if (settings.prayerlist && need_prayerlist)
			prayerlist = add_folder_row(store, _("Prayer List/Journal"));
	}

	language_make_list(tmp, store,
			   text, commentary, map, image,
			   devotional, dictionary, glossary, book, cult,
			   update, uninstalled,
			   language_add_folders, FALSE);

	tmp2 = tmp;
	while (tmp2) {
		MOD_MGR *info = (MOD_MGR *)tmp2->data;

		if (install && !first_time_user) {
			// special lists: updated and uninstalled modules.
			if (!info->installed) {
				add_module_to_language_folder(uninstalled, store,
							      info,
							      install);
			} else if ((!info->old_version && info->new_version && strcmp(info->new_version, " ")) || (info->old_version && !info->new_version) || (info->old_version && info->new_version && strcmp(info->new_version, info->old_version) > 0)) {
				add_module_to_language_folder(update, store, info,
							      install);
			}
		}
		// see comment on similar code in src/main/sidebar.cc.

		if (info->is_cult) {
			add_module_to_language_folder(cult, store, info, install);
		} else if (info->type[0] == 'B') {
			add_module_to_language_folder(text, store, info, install);
		} else if (info->type[0] == 'C') {
			add_module_to_language_folder(commentary, store, info, install);
		} else if (info->is_maps) {
			add_module_to_language_folder(map, store, info, install);
		} else if (info->is_images) {
			add_module_to_language_folder(image, store, info, install);
		} else if (info->is_devotional) {
			add_module_to_language_folder(devotional, store, info, install);
		} else if (info->is_glossary) {
			add_module_to_language_folder(glossary, store, info, install);
		} else if (info->type[0] == 'L') {
			add_module_to_language_folder(dictionary, store, info, install);
		} else if (info->type[0] == 'G') {
			if (first_time_user || !info->is_prayerlist) {
				add_module_to_language_folder(book, store, info, install);
			} else if (settings.prayerlist && need_prayerlist) {
				add_language_folder(store, prayerlist, info->language);
				add_module_to_language_folder(prayerlist, store, info,
							      install);
			}
		} else {
			XI_warning(("mod `%s' unknown type `%s'",
				    info->name, info->type));
		}

		// unindexed modules.
		if (!install && !main_optimal_search(info->name)) {
			if (!first_unindexed) {
				first_unindexed++;
				/* add Unindexed folder */
				unindexed = add_folder_row(store, _("Unindexed Modules"));
			}

			add_language_folder(store, unindexed, info->language);
			add_module_to_language_folder(unindexed, store, info,
						      install);
		}

		g_free(info->name);
		g_free(info->about);
		g_free(info->abbreviation);
		g_free(info->type);
		g_free(info->new_version);
		g_free(info->old_version);
		g_free(info->min_version);
		g_free(info->installsize);
		g_free(info);
		tmp2 = g_list_next(tmp2);
	}
	g_list_free(tmp);

	elim_tree_replace(elim_table_get_store(treeview), store);
	g_object_unref(store);

	/* the click that opens a folder is handled once, however often the tree is loaded */
	if (!g_object_get_data(G_OBJECT(treeview), "elim-modmgr-click")) {
		g_object_set_data(G_OBJECT(treeview), "elim-modmgr-click", GINT_TO_POINTER(1));
		gui_widget_on_button(GTK_WIDGET(treeview), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_modules_list_button_release, treeview);
	}
}

/******************************************************************************
 * Name
 *   remove_install_wrapper
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *    void remove_install_wrapper(int activity)
 *
 * Description
 *   generalization of install/remove/archive/index methodology.
 *
 * Return value
 *   void
 */

static void remove_install_wrapper(int activity)
{
	GList *modules = NULL;

	if (working)
		return;
	working = TRUE;

	modules = get_list_mods_to_remove_install(activity);

	sync_windows();

	remove_install_modules(modules, activity);
	mod_mgr_shut_down();
	mod_mgr_init(destination, FALSE, TRUE);
	load_module_tree(treeview, (activity == INSTALL));

	sync_windows();

	working = FALSE;
}

/******************************************************************************
 * Name
 *   response_refresh
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void response_refresh(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void response_refresh(void)
{
	gint result;
	gchar *buf;

	if (working)
		return;
	working = TRUE;

	if (remote_source == NULL)
		remote_source =
		    g_strdup(elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo_entry2)));
	buf =
	    g_strdup_printf("%s: %s", _("Refreshing from remote source"),
			    remote_source);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
				  buf);
	g_free(buf);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_refresh), 0);
	gtk_widget_show(progressbar_refresh);
	sync_windows();
	result = mod_mgr_refresh_remote_source(remote_source);

	if (result) {
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
					  _("Remote source not found"));
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_refresh), 0);
	} else {
		load_module_tree(treeview, TRUE);
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
					  _("Finished"));
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_refresh), 0);
	}

	working = FALSE;
}

/******************************************************************************
 * Name
 *   check_sync_repos
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void check_sync_repos(void)
 *
 * Description
 *   verifies enough remote sources are known, and gets more if needed.
 *
 * Return value
 *   void
 */

static void check_sync_repos(void)
{
	GList *tmp = mod_mgr_list_remote_sources();
	/*
	 * if too few remote sources, synchronize repos from crosswire.
	 */
	if (g_list_length(tmp) <= 1) {
		char index[10];
		mod_mgr_init_config_extras();
		settings.mod_mgr_remote_source_index =
		    load_source_treeviews();
		g_snprintf(index, 10, "%d",
			   settings.mod_mgr_remote_source_index);
		xml_set_value("Xiphos", "modmgr",
			      "mod_mgr_remote_source_index", index);
		set_controls_to_last_use();
	}
	g_list_free(tmp);
}

/* One row of a table of sources. */
static ElimRow *
source_row(const gchar *type, const gchar *caption, const gchar *source,
	   const gchar *directory, const gchar *user, const gchar *pass,
	   const gchar *uid)
{
	ElimRow *row = elim_row_new(NUM_REMOTE_COLUMNS);

	elim_row_set_string(row, COLUMN_TYPE, type);
	elim_row_set_string(row, COLUMN_CAPTION, caption);
	elim_row_set_string(row, COLUMN_SOURCE, source);
	elim_row_set_string(row, COLUMN_DIRECTORY, directory);
	elim_row_set_string(row, COLUMN_USER, user);
	elim_row_set_string(row, COLUMN_PASS, pass);
	elim_row_set_string(row, COLUMN_UID, uid);
	return row;
}

/* Adds a source to the table SOURCES, a GtkColumnView from setup_sources_view(). */
static void
add_source(GtkWidget *sources, ElimRow *row)
{
	g_list_store_append(elim_table_get_store(sources), row);
	g_object_unref(row);
}

/******************************************************************************
 * Name
 *   setup_sources_view
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void setup_sources_view(GtkWidget *view)
 *
 * Description
 *   VIEW, a GtkColumnView, is a table of install sources: what kind, what it
 *   is called, where it is and how to reach it.
 *
 * Return value
 *   void
 */

static void setup_sources_view(GtkWidget *view)
{
	GListStore *store = elim_table_new();

	elim_table_setup(view, store);
	g_object_unref(store);
	elim_table_add_text_column(view, _("Type"), COLUMN_TYPE, FALSE);
	elim_table_add_text_column(view, _("Caption"), COLUMN_CAPTION, FALSE);
	elim_table_add_text_column(view, _("Source"), COLUMN_SOURCE, FALSE);
	elim_table_add_text_column(view, _("Directory"), COLUMN_DIRECTORY, TRUE);
	elim_table_add_text_column(view, _("User"), COLUMN_USER, FALSE);
	elim_table_add_text_column(view, _("Password"), COLUMN_PASS, FALSE);
	elim_table_add_text_column(view, _("UID"), COLUMN_UID, FALSE);
}

/* The pages of the dialog, in groups; a group has no page of its own. */
static void
add_page_row(GListStore *store, ElimRow *group, const gchar *caption, gint page)
{
	ElimRow *row = elim_tree_append(store, group, 2);

	elim_row_set_string(row, 0, caption);
	elim_row_set_int(row, 1, page);
}

static void setup_pages_tree(GtkWidget *view)
{
	GListStore *store = elim_table_new();
	ElimTextColumn column = elim_text_column(0);
	ElimRow *group;

	column.expand = TRUE;
	elim_tree_setup_list(view, store, &column);
	g_object_unref(store);

	group = elim_tree_append(store, NULL, 2);
	elim_row_set_string(group, 0, _("Module Sources"));
	add_page_row(store, group, _("Add/Remove"), 1);
	add_page_row(store, group, _("Choose"), 2);

	group = elim_tree_append(store, NULL, 2);
	elim_row_set_string(group, 0, _("Modules"));
	add_page_row(store, group, _("Install/Update"), 3);
	add_page_row(store, group, _("Maintenance"), 4);
}

/******************************************************************************
 * Name
 *   load_source_treeviews
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *   int load_source_treeviews(void)
 *
 * Description
 *   inhale InstallMgr.conf content.  return CrossWire's position.
 *
 * Return value
 *   int
 */

static int load_source_treeviews(void)
{
	int crosswire_index = 1 /* guess */, crosswire_tracker =
						 0 /* start */;
	GList *tmp = NULL;
	GList *tmp2 = NULL;
	MOD_MGR_SOURCE *mms;

	/* remote */
	g_list_store_remove_all(elim_table_get_store(treeview_remote));
	elim_dropdown_remove_all(GTK_DROP_DOWN(combo_entry2));
	tmp = tmp2 = mod_mgr_list_remote_sources();
	while (tmp) {
		mms = (MOD_MGR_SOURCE *)tmp->data;
		if (!strcmp(mms->caption, "CrossWire"))
			crosswire_index = crosswire_tracker;
		add_source(treeview_remote,
			   source_row(mms->type, mms->caption, mms->source,
				      mms->directory, mms->user, mms->pass,
				      mms->uid));
		elim_dropdown_append(GTK_DROP_DOWN(combo_entry2), NULL,
				     (gchar *)mms->caption);
		g_free((gchar *)mms->type);
		g_free((gchar *)mms->caption);
		g_free((gchar *)mms->source);
		g_free((gchar *)mms->directory);
		g_free((gchar *)mms->user);
		g_free((gchar *)mms->pass);
		g_free((gchar *)mms->uid);
		g_free(mms);
		tmp = g_list_next(tmp);
		crosswire_tracker++;
	}
	elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry2), 0);
	g_list_free(tmp2);

	/* local */
	g_list_store_remove_all(elim_table_get_store(treeview_local));
	elim_dropdown_remove_all(GTK_DROP_DOWN(combo_entry1));
	tmp = tmp2 = mod_mgr_list_local_sources();
	while (tmp) {
		mms = (MOD_MGR_SOURCE *)tmp->data;
		add_source(treeview_local,
			   source_row(mms->type, mms->caption, " ", // mms->source - eh.
				      mms->directory, "", "", ""));
		elim_dropdown_append(GTK_DROP_DOWN(combo_entry1), NULL,
				     (gchar *)mms->caption);
		g_free((gchar *)mms->type);
		g_free((gchar *)mms->caption);
		g_free((gchar *)mms->source);
		g_free((gchar *)mms->directory);
		g_free((gchar *)mms->user);
		g_free((gchar *)mms->pass);
		g_free((gchar *)mms->uid);
		g_free(mms);
		tmp = g_list_next(tmp);
	}
	elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry1), 0);
	g_list_free(tmp2);

	return crosswire_index;
}

/******************************************************************************
 * Name
 *   clear_and_hide_progress_bar
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void clear_and_hide_progress_bar(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void clear_and_hide_progress_bar(void)
{
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressbar_refresh),
				  "");
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressbar_refresh), 0);
	gtk_widget_hide(progressbar_refresh);
}

/******************************************************************************
 * Name
 *   on_notebook1_switch_page
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_notebook1_switch_page(GtkNotebook * notebook,
 *				     GtkNotebookPage * page,
 *				     guint page_num, gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void
on_notebook1_switch_page(GtkNotebook *notebook,
			 gpointer arg, guint page_num, gpointer user_data)
{

#ifdef CHATTY
	GTimer *total;
	double d;
	total = g_timer_new();
#endif

	current_page = page_num;
	clear_and_hide_progress_bar();

	switch (page_num) {
	case 0:
		break;
	case 1:
		mod_mgr_shut_down();
		mod_mgr_init(destination, FALSE, TRUE);
		break;
	case 2:
		/* we already cleared progress bar */
		break;
	case 3:
		if (!have_configs) {
			gchar *str =
			    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s",
					    _("Please Refresh"),
					    _("Your module list is not up to date!"));
			gui_generic_warning(str);
			g_free(str);
		}
		if (gui_toggle_get_active(GTK_WIDGET(radiobutton_dest))) {
			destination =
			    gtk_label_get_text(GTK_LABEL(label_home));
		} else {
			destination = settings.path_to_mods;
		}
		mod_mgr_shut_down();
		mod_mgr_init(destination, FALSE, TRUE);
		load_module_tree(treeview, TRUE);
		break;
	case 4:
		if (gui_toggle_get_active(GTK_WIDGET(radiobutton_dest))) {
			destination =
			    gtk_label_get_text(GTK_LABEL(label_home));
		} else {
			destination = settings.path_to_mods;
		}
		mod_mgr_shut_down();
		main_update_module_lists();
		mod_mgr_init(destination, FALSE, TRUE);
		load_module_tree(treeview2, FALSE);
		break;
	}

#ifdef CHATTY
	g_timer_stop(total);
	d = g_timer_elapsed(total, NULL);
	g_timer_destroy(total);
	XI_message(("total time is %f", d));
#endif
}

/******************************************************************************
 * Name
 *   on_radiobutton2_toggled
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_radiobutton2_toggled(GtkToggleButton * togglebutton,
 *			     gpointer user_data)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void
on_radiobutton2_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	if (gui_toggle_get_active(togglebutton)) {
		gtk_widget_show(button_refresh);
		if (remote_source)
			g_free(remote_source);
		remote_source =
		    g_strdup(elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combo_entry2)));
		xml_set_value("Xiphos", "modmgr", "mod_mgr_source", "1");

	} else {
		gtk_widget_hide(button_refresh);
		gtk_widget_hide(progressbar_refresh);
		xml_set_value("Xiphos", "modmgr", "mod_mgr_source", "0");
	}
	settings.mod_mgr_source =
	    gui_toggle_get_active(togglebutton);
	xml_save_settings_doc(settings.fnconfigure);
}

void
on_radiobutton4_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	xml_set_value("Xiphos", "modmgr", "mod_mgr_source",
		      (gui_toggle_get_active(togglebutton) ? "1" : "0"));
	settings.mod_mgr_source =
	    gui_toggle_get_active(togglebutton);
	xml_save_settings_doc(settings.fnconfigure);
}

/******************************************************************************
 * Name
 *   save_sources
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void save_sources(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void save_sources(void)
{
	GListStore *remote_store = elim_table_get_store(treeview_remote);
	GListStore *local_store = elim_table_get_store(treeview_local);
	guint i;

	mod_mgr_clear_config();

	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(remote_store)); i++) {
		ElimRow *row = elim_table_get(remote_store, i);
		const gchar *type = elim_row_get_string(row, COLUMN_TYPE);
		const gchar *caption = elim_row_get_string(row, COLUMN_CAPTION);
		const gchar *source = elim_row_get_string(row, COLUMN_SOURCE);
		const gchar *directory = elim_row_get_string(row, COLUMN_DIRECTORY);
		const gchar *user = elim_row_get_string(row, COLUMN_USER);
		const gchar *pass = elim_row_get_string(row, COLUMN_PASS);
		const gchar *uid = elim_row_get_string(row, COLUMN_UID);

		if (strcasecmp(type, "HTTP") == 0)
			mod_mgr_add_source("HTTPSource",
					   type, caption, source,
					   directory, user, pass, uid);
		else if (strcasecmp(type, "HTTPS") == 0)
			mod_mgr_add_source("HTTPSSource",
					   type, caption, source,
					   directory, user, pass, uid);
		else if (strcasecmp(type, "SFTP") == 0)
			mod_mgr_add_source("SFTPSource",
					   type, caption, source,
					   directory, user, pass, uid);
		else
			mod_mgr_add_source("FTPSource",
					   type, caption, source,
					   directory, user, pass, uid);
	}

	for (i = 0; i < g_list_model_get_n_items(G_LIST_MODEL(local_store)); i++) {
		ElimRow *row = elim_table_get(local_store, i);

		mod_mgr_add_source("DIRSource",
				   elim_row_get_string(row, COLUMN_TYPE),
				   elim_row_get_string(row, COLUMN_CAPTION),
				   "[local]",
				   elim_row_get_string(row, COLUMN_DIRECTORY),
				   "", "", "");
	}

	mod_mgr_reread_config();
	load_source_treeviews();
}

/******************************************************************************
 * Name
 *   create_fileselection_local_source
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void create_fileselection_local_source (void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void create_fileselection_local_source(void)
{
	GtkWidget *dialog;

	dialog =
	    gtk_file_chooser_dialog_new("Open File",
					NULL,
					GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					"_Cancel", GTK_RESPONSE_CANCEL,
					"_OK", GTK_RESPONSE_ACCEPT,
					NULL);
	gui_fit_dialog_to_screen(GTK_WINDOW(dialog));

	if (gui_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename =
		    gui_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		XI_message(("%s", filename));
		add_source(treeview_local,
			   source_row("DIR", filename, "[local]", filename, "", "", ""));
		save_sources();
		g_free(filename);
	}
	gui_widget_destroy(dialog);
}

/******************************************************************************
 * Name
 *   on_dialog_destroy
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_dialog_destroy(GtkObject * object, gpointer user_data)
 *
 * Description
 *   when the dialog is destroyed and changes have been made
 *   Sword is shut down and restarted
 *   module lists are cleared and refilled with current modules
 *   the sidebar module tree is reloaded
 *
 * Return value
 *   void
 */

static void on_dialog_destroy(GObject *object, gpointer user_data)
{
	GList *tmp;
	gchar *str;

	if (working)
		return;
	working = TRUE;

	XI_message(("on_destroy"));
	if (remote_source) {
		g_free(remote_source);
		remote_source = NULL;
	}

	mod_mgr_shut_down();
	sync_windows();

	/* little bits of nonsense left behind by the engine. */
	str = g_strdup_printf("%s/dirlist", settings.homedir);
	if (mod_mgr_check_for_file(str))
		unlink(str);
	g_free(str);

	check_sync_repos();

	working = FALSE;
	is_running = FALSE;
	settings.display_prefs = 0;
	xml_set_value("Xiphos", "layout", "prefsopen", "0");

	if (first_time_user) {
		/* no deeper analysis, first time around. */
		if (initial_run_loop && g_main_loop_is_running(initial_run_loop))
			g_main_loop_quit(initial_run_loop);
		return;
	}

	/*
	 * if we uninstalled a current module, substitute a live one
	 */
	if (!main_is_module(settings.MainWindowModule)) {
		if ((tmp = get_list(TEXT_LIST)))
			main_display_bible_after_removal((char *)tmp->data);
		else {
			/* Zero Bibles is just not workable in Xiphos. */
			gui_generic_warning_modal(_("You have uninstalled your last Bible.\n"
						    "Xiphos requires at least one."));
			main_shutdown_list();
			gui_open_mod_mgr_initial_run();
			main_init_lists();
			if (settings.havebible == 0) {
				gui_generic_warning_modal(_("There are still no Bibles installed.\n"
							    "Xiphos cannot continue without one."));
				exit(1);
			}
		}
	}
	if (!main_is_module(settings.CommWindowModule)) {
		if ((tmp = get_list(COMM_LIST)))
			main_display_commentary((char *)tmp->data,
						settings.currentverse);
	}
	if (!main_is_module(settings.DictWindowModule)) {
		if ((tmp = get_list(DICT_LIST)))
			main_display_dictionary((char *)tmp->data,
						settings.dictkey);
	}
	if (!main_is_module(settings.book_mod)) {
		if ((tmp = get_list(GBS_LIST)))
			main_display_book((char *)tmp->data, "/"); /* blank key */
	}

	settings.display_modmgr = 0;
	xml_set_value("Xiphos", "layout", "modmgropen", "0");
}

/******************************************************************************
 * Name
 *   response_close
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void response_close(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

static void response_close(void)
{
	gui_widget_destroy(GTK_WIDGET(dialog_modmgr));
	on_dialog_destroy(NULL, NULL);
}

/*
 * click callbacks, most using a wrapper.
 */
void on_refresh_clicked(GtkButton *button, gpointer user_data)
{
	response_refresh();
}

void on_install_clicked(GtkButton *button, gpointer user_data)
{
	remove_install_wrapper(INSTALL);
}

void on_remove_clicked(GtkButton *button, gpointer user_data)
{
	remove_install_wrapper(REMOVE);
}

void on_archive_clicked(GtkButton *button, gpointer user_data)
{
	remove_install_wrapper(ARCHIVE);
}

void on_index_clicked(GtkButton *button, gpointer user_data)
{
	remove_install_wrapper(FASTMOD);
}

void on_delete_index_clicked(GtkButton *button, gpointer user_data)
{
	remove_install_wrapper(DELFAST);
}

/******************************************************************************
 * Name
 *   delete_obsolete
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void delete_obsolete(char *module)
 *
 * Description
 *   look for "Obsoletes=OldModule" and ask to get rid of it.
 *
 * Return value
 *   void
 */

void delete_obsolete(char *module, int *counter)
{
	char *obsolete =
	    backend_mod_mgr_get_config_entry(module, "Obsoletes");

	if (obsolete && main_is_module(obsolete)) {
		if (counter)
			(*counter)++;

		/* targeting coordinates acquired. */
		char *mod_display_name, *obs_display_name, *question;
		const char *mod_abbrev =
		    main_name_to_abbrev((const char *)module);
		const char *obs_abbrev =
		    main_name_to_abbrev((const char *)obsolete);

		if (mod_abbrev)
			mod_display_name =
			    g_strdup_printf("%s (%s)", mod_abbrev, module);
		else
			mod_display_name = g_strdup(module);

		if (obs_abbrev)
			obs_display_name =
			    g_strdup_printf("%s (%s)", obs_abbrev,
					    obsolete);
		else
			obs_display_name = g_strdup(obsolete);

		question = g_strdup_printf(_("%s obsoletes %s.\n"
					     "Would you like to delete %s?\n"
					     "Beware: This cannot be undone."),
					   mod_display_name,
					   obs_display_name,
					   (obs_abbrev ? obs_abbrev : obsolete));
		if (gui_yes_no_dialog(question, NULL)) {
			/* lay the axe at the root. */
			if (mod_mgr_uninstall(destination, obsolete) == -1) {
				mod_mgr_uninstall((destination
						       ? NULL
						       : gtk_label_get_text(GTK_LABEL(label_home))),
						  obsolete);
			}
		}

		g_free(question);
		g_free(mod_display_name);
		g_free(obs_display_name);

		ModuleCacheErase((const char *)obsolete);
	}
}

/******************************************************************************
 * Name
 *   on_scan_obsolete
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_scan_obsolete(GtkButton * button, gpointer user_data)
 *
 * Description
 *   scan all modules to try to eliminate obsolescences.
 *
 * Return value
 *   void
 */

void on_scan_obsolete(GtkButton *button, gpointer user_data)
{
	GList *modules = NULL;
	int counter = 0;

	if (working)
		return;
	working = TRUE;

	modules = get_list_mods_to_remove_install(ALL_MODULES);

	/*
	 * walk the module list to find
	 * those that obsolete something else.
	 * verify each first, because previous
	 * obsolescences may delete some farther down.
	 */
	GList *tmp = modules;
	while (tmp) {
		if (main_is_module((gchar *)tmp->data))
			delete_obsolete((gchar *)tmp->data, &counter);
		tmp = g_list_next(tmp);
	}
	g_list_free_full(modules, g_free);

	if (counter == 0)
		gui_generic_warning(_("No obsolete modules were found."));
	else {
		mod_mgr_shut_down();
		mod_mgr_init(destination, FALSE, TRUE);
		load_module_tree(treeview2, 0);
	}

	working = FALSE;
	return;
}

void on_cancel_clicked(GtkButton *button, gpointer user_data)
{
	mod_mgr_terminate();
	sync_windows();
}

void on_load_sources_clicked(GtkButton *button, gpointer user_data)
{
	if (mod_mgr_init_config_extras() == 0) {
		/* not quite identical to check_sync_repos(). */
		char index[10];
		gui_generic_warning(_("Standard remote sources have been loaded."));
		settings.mod_mgr_remote_source_index =
		    load_source_treeviews();
		g_snprintf(index, 10, "%d",
			   settings.mod_mgr_remote_source_index);
		xml_set_value("Xiphos", "modmgr",
			      "mod_mgr_remote_source_index", index);
		set_controls_to_last_use();
	} else
		gui_generic_warning(_("Could not load standard sources from CrossWire."));
}

void on_mod_mgr_intro_clicked(GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog;
	dialog = gtk_message_dialog_new_with_markup(NULL, /* no need for a parent window */
						    GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_MESSAGE_INFO,
						    GTK_BUTTONS_OK,
						    (user_data ? XI_FIRST_INSTALL : XI_GENERAL_INTRO));
	g_signal_connect_swapped(dialog, "response",
				 G_CALLBACK(gui_widget_destroy), dialog);
	gtk_widget_show(dialog);
}

/******************************************************************************
 * Name
 *   on_mod_mgr_response
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_mod_mgr_response(GtkDialog * dialog, gint response_id, gpointer user_data)
 *
 * Description
 *   these are local defines at the top of this file.
 *
 * Return value
 *   void
 */

void
on_mod_mgr_response(GtkDialog *dialog,
		    gint response_id, gpointer user_data)
{
	switch (response_id) {
	case GTK_RESPONSE_CANCEL:
		mod_mgr_terminate();
		sync_windows();
		break;
	case GTK_RESPONSE_REFRESH:
		response_refresh();
		break;
	case GTK_RESPONSE_CLOSE:
		response_close();
		break;
	case GTK_RESPONSE_INSTALL:
		remove_install_wrapper(INSTALL);
		break;
	case GTK_RESPONSE_REMOVE:
		remove_install_wrapper(REMOVE);
		break;
	case GTK_RESPONSE_ARCHIVE:
		remove_install_wrapper(ARCHIVE);
		break;
	case GTK_RESPONSE_FASTMOD:
		remove_install_wrapper(FASTMOD);
		break;
	case GTK_RESPONSE_DELFAST:
		remove_install_wrapper(DELFAST);
		break;
	case GTK_RESPONSE_SOURCES:
		on_load_sources_clicked(NULL, NULL);
		break;
	case GTK_RESPONSE_INTRO:
		on_mod_mgr_intro_clicked(NULL, NULL);
		break;
	case GTK_RESPONSE_OBSOLETE:
		on_scan_obsolete(NULL, NULL);
		break;
	}
}

/******************************************************************************
 * Name
 *   on_button_add_local_clicked
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_button_add_local_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   add local source
 *
 * Return value
 *   void
 */

void on_button_add_local_clicked(GtkButton *button, gpointer user_data)
{
	if (!working) {
		working = TRUE;
		create_fileselection_local_source();
		working = FALSE;
	}
}

/******************************************************************************
 * Name
 *   on_button_remove_local_clicked
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_button_remove_local_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   remove local source
 *
 * Return value
 *   void
 */

void on_button_remove_local_clicked(GtkButton *button, gpointer user_data)
{
	ElimRow *selected;
	gchar *str;

	if (working)
		return;
	working = TRUE;

	selected = elim_table_get_selected(treeview_local);
	if (!selected) {
		working = FALSE;
		return;
	}

	str =
	    g_strdup_printf("<span weight=\"bold\">%s</span>\n\n%s|%s|%s|%s",
			    _("Remove the selected source"),
			    elim_row_get_string(selected, COLUMN_CAPTION),
			    elim_row_get_string(selected, COLUMN_TYPE),
			    elim_row_get_string(selected, COLUMN_SOURCE),
			    elim_row_get_string(selected, COLUMN_DIRECTORY));

	if (gui_yes_no_dialog(str,
			      "dialog-warning"
			      )) {

		g_list_store_remove(elim_table_get_store(treeview_local),
				    elim_table_get_selected_position(treeview_local));
		save_sources();
	}
	g_free(str);

	working = FALSE;
}

/******************************************************************************
 * Name
 *   on_button_add_remote_clicked
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_button_add_remote_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   add remote source
 *
 * Return value
 *   void
 */

void on_button_add_remote_clicked(GtkButton *button, gpointer user_data)
{
	gint test;
	GS_DIALOG *dialog;
	GString *str;
	GList *tmp, *tmp2;
	gboolean name_conflict = FALSE;
	MOD_MGR_SOURCE *mms;

	if (working)
		return;
	working = TRUE;

	str = g_string_new(NULL);

	gui_toggle_set_active(GTK_WIDGET(radiobutton2),
				     TRUE);
	gtk_widget_hide(button_refresh);

	g_string_printf(str,
			"<span weight=\"bold\">%s</span>",
			_("Enter a remote source"));
	dialog = gui_new_dialog();
	dialog->stock_icon =
	    "dialog-information";
	dialog->label_top = str->str;
	dialog->label1 = _("Caption:");
	dialog->label2 = _("Type:");
	dialog->label3 = _("Host:");
	dialog->label4 = _("Directory:");
	dialog->label5 = _("User (optional):");
	dialog->label6 = _("Password (optional):");
	dialog->text1 = g_strdup("CrossWire");
	dialog->text2 = g_strdup("FTP");
	dialog->text3 = g_strdup("ftp.crosswire.org");
	dialog->text4 = g_strdup("/pub/sword/raw");
	dialog->cancel = TRUE;
	dialog->ok = TRUE;

	test = gui_gs_dialog(dialog);
	if (test != GS_OK) {
		g_free(dialog->text1);
		g_free(dialog->text2);
		g_free(dialog->text3);
		g_free(dialog->text4);
		g_free(dialog->text5);
		g_free(dialog->text6);
		g_free(dialog);
		g_string_free(str, TRUE);
		goto out;
	}

	for (tmp = tmp2 = mod_mgr_list_remote_sources();
	     tmp; tmp = g_list_next(tmp)) {
		mms = (MOD_MGR_SOURCE *)tmp->data;
		if (!strcmp(mms->caption, dialog->text1)) {
			/* this can happen at most once */
			gui_generic_warning_modal(_("A source by that name already exists."));
			name_conflict = TRUE;
		}
		g_free((gchar *)mms->type);
		g_free((gchar *)mms->caption);
		g_free((gchar *)mms->source);
		g_free((gchar *)mms->directory);
		g_free((gchar *)mms->user);
		g_free((gchar *)mms->pass);
		g_free((gchar *)mms->uid);
		g_free(mms);
	}
	g_list_free(tmp2);

	if (!name_conflict) {
		/* timestamped UID field */
		time_t now = time(NULL);
		struct tm *local = localtime(&now);
		/*
		 * who is the psychotic moron who gave us tm_year as "# yrs
		 * since 1900" and tm_mon as "# months since january, [0-11]"?
		 * flensing, trepanation, and hari-kari all apply.
		 */
		gchar *uid = g_strdup_printf("%d%02d%02d%02d%02d%02d",
					     local->tm_year + 1900,
					     local->tm_mon + 1,
					     local->tm_mday,
					     local->tm_hour, local->tm_min,
					     local->tm_sec);

		add_source(treeview_remote,
			   source_row(dialog->text2, dialog->text1, dialog->text3,
				      dialog->text4, dialog->text5, dialog->text6, uid));
		g_free(uid);
		save_sources();

		/* set the new item's index as active */
		for (test = 0, tmp = tmp2 = mod_mgr_list_remote_sources();
		     tmp; tmp = g_list_next(tmp), ++test) {
			mms = (MOD_MGR_SOURCE *)tmp->data;
			if (!strcmp(mms->caption, dialog->text1)) {
				elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry2), test);
			}
			g_free((gchar *)mms->type);
			g_free((gchar *)mms->caption);
			g_free((gchar *)mms->source);
			g_free((gchar *)mms->directory);
			g_free((gchar *)mms->user);
			g_free((gchar *)mms->pass);
			g_free((gchar *)mms->uid);
			g_free(mms);
		}
		g_list_free(tmp2);
	}

	if (remote_source)
		g_free(remote_source);
	remote_source = g_strdup(dialog->text1);

	g_free(dialog->text1);
	g_free(dialog->text2);
	g_free(dialog->text3);
	g_free(dialog->text4);
	g_free(dialog->text5);
	g_free(dialog->text6);
	g_free(dialog);
	g_string_free(str, TRUE);

	sync_windows();

	mod_mgr_shut_down();
	mod_mgr_init(destination, FALSE, TRUE);

out:
	gtk_widget_show(button_refresh);
	working = FALSE;
}

/******************************************************************************
 * Name
 *   on_button_remove_remote_clicked
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void on_button_remove_remote_clicked(GtkButton * button, gpointer user_data)
 *
 * Description
 *   remove remote source
 *
 * Return value
 *   void
 */

void
on_button_remove_remote_clicked(GtkButton *button, gpointer user_data)
{
	gint test;
	GS_DIALOG *yes_no_dialog;
	ElimRow *selected;
	GString *str;

	if (working)
		return;
	working = TRUE;

	str = g_string_new(NULL);

	selected = elim_table_get_selected(treeview_remote);
	if (!selected) {
		g_string_free(str, TRUE);
		working = FALSE;
		return;
	}

	yes_no_dialog = gui_new_dialog();
	yes_no_dialog->stock_icon =
	    "dialog-warning";
	yes_no_dialog->title = _("Delete a remote source");
	g_string_printf(str,
			"<span weight=\"bold\">%s</span>\n\n%s|%s|%s|%s",
			_("Remove the selected source"),
			elim_row_get_string(selected, COLUMN_CAPTION),
			elim_row_get_string(selected, COLUMN_TYPE),
			elim_row_get_string(selected, COLUMN_SOURCE),
			elim_row_get_string(selected, COLUMN_DIRECTORY));
	yes_no_dialog->label_top = str->str;
	yes_no_dialog->yes = TRUE;
	yes_no_dialog->no = TRUE;

	test = gui_alert_dialog(yes_no_dialog);
	if (test == GS_YES) {
		g_list_store_remove(elim_table_get_store(treeview_remote),
				    elim_table_get_selected_position(treeview_remote));
		save_sources();
		elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry2), 0);
	}
	g_free(yes_no_dialog);
	g_string_free(str, TRUE);

	working = FALSE;
}

/******************************************************************************
 * Name
 *   on_treeview1_button_release_event
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   gboolean on_treeview1_button_release_event(GtkWidget * widget,
 *                           GuiButtonEvent * event, gpointer user_data)
 *
 * Description
 *   button release in main treeview
 *   change notebook page to 'sel' returned from selection
 *   show/hide dialog response buttons as needed
 *
 * Return value
 *   void
 */

gboolean
on_treeview1_button_release_event(GtkWidget *widget,
				  GuiButtonEvent *event,
				  gpointer user_data)
{
	ElimRow *selected;
	gint sel;

	if (working)
		return 0;
	working = TRUE;

	selected = elim_table_get_selected(treeview1);

	if (selected) {
		sel = elim_row_get_int(selected, 1);
		if (!elim_row_n_children(selected)) {
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook1), sel);
			switch (sel) {
			case 1:
				gtk_widget_hide(button_refresh);
				gtk_widget_hide(button_install);
				gtk_widget_hide(button_remove);
				gtk_widget_hide(button_arch);
				gtk_widget_hide(button_idx);
				gtk_widget_hide(button_delidx);
				gtk_widget_hide(button_obsolete);
				if (first_time_user)
					gtk_widget_hide(button_load_sources);
				else
					gtk_widget_show(button_load_sources);
				break;
			case 2:
				if (gui_toggle_get_active(GTK_WIDGET(radiobutton2)))
					gtk_widget_show(button_refresh);
				else
					gtk_widget_hide(button_refresh);
				gtk_widget_hide(button_install);
				gtk_widget_hide(button_remove);
				gtk_widget_hide(button_arch);
				gtk_widget_hide(button_idx);
				gtk_widget_hide(button_delidx);
				gtk_widget_hide(button_obsolete);
				gtk_widget_hide(button_load_sources);
				break;
			case 3:
				if (gui_toggle_get_active(GTK_WIDGET(radiobutton2)))
					gtk_widget_show(button_refresh);
				else
					gtk_widget_hide(button_refresh);
				gtk_widget_show(button_install);
				gtk_widget_hide(button_remove);
				gtk_widget_hide(button_arch);
				gtk_widget_hide(button_idx);
				gtk_widget_hide(button_delidx);
				gtk_widget_hide(button_obsolete);
				gtk_widget_hide(button_load_sources);
				break;
			case 4:
				gtk_widget_show(button_remove);
				gtk_widget_show(button_arch);
				gtk_widget_show(button_idx);
				gtk_widget_show(button_delidx);
				gtk_widget_show(button_obsolete);
				gtk_widget_hide(button_load_sources);
				gtk_widget_hide(button_refresh);
				gtk_widget_hide(button_install);
				break;
			}
		}
	}

	working = FALSE;
	return FALSE;
}

static void setup_treeview_main(GtkWidget *tree_view)
{
	setup_pages_tree(tree_view);
	elim_tree_expand_all(tree_view);

	gui_widget_on_button(GTK_WIDGET(tree_view), GTK_PHASE_CAPTURE, NULL, (GuiButtonFunc)on_treeview1_button_release_event, NULL);
}

static void
setup_treeviews_local_remote(GtkWidget *local, GtkWidget *remote)
{
	setup_sources_view(local);
	setup_sources_view(remote);
}

static void set_combobox(GtkDropDown *combo)
{
	elim_dropdown_prepare(combo);
}

static void setup_dialog_action_area(GtkDialog *dialog)
{
	/* The builder file shows the action area and lays it out at the
	 * end; gtk_dialog_get_action_area() is deprecated (GTK4-PORT-101). */
	(void)dialog;

	/*
	 * response buttons
	 */
	g_signal_connect(button_close, "clicked",
			 G_CALLBACK(response_close), NULL);
	g_signal_connect(button_refresh, "clicked",
			 G_CALLBACK(on_refresh_clicked), NULL);
	g_signal_connect(button_install, "clicked",
			 G_CALLBACK(on_install_clicked), NULL);
	g_signal_connect(button_remove, "clicked",
			 G_CALLBACK(on_remove_clicked), NULL);
	g_signal_connect(button_arch, "clicked",
			 G_CALLBACK(on_archive_clicked), NULL);
	g_signal_connect(button_idx, "clicked",
			 G_CALLBACK(on_index_clicked), NULL);
	g_signal_connect(button_delidx, "clicked",
			 G_CALLBACK(on_delete_index_clicked), NULL);
	g_signal_connect(button_obsolete, "clicked",
			 G_CALLBACK(on_scan_obsolete), NULL);
	g_signal_connect(button_load_sources, "clicked",
			 G_CALLBACK(on_load_sources_clicked), NULL);
	g_signal_connect(button_intro, "clicked",
			 G_CALLBACK(on_mod_mgr_intro_clicked), NULL);
	g_signal_connect(button_cancel, "clicked",
			 G_CALLBACK(on_cancel_clicked), NULL);

	gtk_widget_show(button_close);
	on_mod_mgr_intro_clicked(NULL, NULL);
	sync_windows();
	on_mod_mgr_intro_clicked(NULL, (gpointer)1);
}

static void set_controls_to_last_use(void)
{
	/* local or remote source */
	gui_toggle_set_active(GTK_WIDGET(radiobutton2),
				     settings.mod_mgr_source);
	/* local source */
	elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry1), settings.mod_mgr_local_source_index);
	/* remote source */
	elim_dropdown_set_active(GTK_DROP_DOWN(combo_entry2), settings.mod_mgr_remote_source_index);
	/* destination */
	gui_toggle_set_active(GTK_WIDGET(radiobutton4),
				     settings.mod_mgr_destination);
}

static void setup_ui_labels()
{
	gchar *str;

	str = g_strdup_printf("%s/%s", settings.homedir,
			      DOTSWORD "/InstallMgr/InstallMgr.conf");
	if (!mod_mgr_check_for_file(str)) {
		have_configs = FALSE;
		mod_mgr_init_config();
	}
	g_free(str);

	have_configs = TRUE;
	mod_mgr_init(NULL, TRUE, TRUE);
	str = g_strdup_printf("%s/%s", settings.homedir, DOTSWORD);
	gtk_label_set_text(GTK_LABEL(label_home), str);
	g_free(str);

	gtk_label_set_text(GTK_LABEL(label_system), settings.path_to_mods);
	if (g_access(settings.path_to_mods, W_OK) == -1) {
		XI_print(("%s is write protected\n",
			  (settings.path_to_mods
			       ? settings.path_to_mods
			       : "<null>")));
		gtk_widget_set_sensitive(label_system, FALSE);
		gtk_widget_set_sensitive(radiobutton4, FALSE);
	} else {
		gtk_widget_set_sensitive(label_system, TRUE);
		gtk_widget_set_sensitive(radiobutton4, TRUE);
	}
	create_pixbufs();
	load_source_treeviews();
}

static void
on_comboboxentry_local_changed(GObject *combobox, GParamSpec *pspec,
			       gpointer user_data)
{
	gint index = elim_dropdown_get_active(GTK_DROP_DOWN(combo_entry1));
	settings.mod_mgr_local_source_index = index;
	gchar *index_str = g_strdup_printf("%d", index);
	xml_set_value("Xiphos", "modmgr", "mod_mgr_local_source_index",
		      index_str);
	xml_save_settings_doc(settings.fnconfigure);
	g_free(index_str);
}

static void
on_comboboxentry_remote_changed(GObject *combobox, GParamSpec *pspec,
				gpointer user_data)
{
	gint index = elim_dropdown_get_active(GTK_DROP_DOWN(combo_entry2));
	settings.mod_mgr_remote_source_index = index;
	gchar *index_str = g_strdup_printf("%d", index);
	XI_message(("index = %d index_str = %s", index, index_str));
	xml_set_value("Xiphos", "modmgr", "mod_mgr_remote_source_index",
		      index_str);
	xml_save_settings_doc(settings.fnconfigure);
	g_free(index_str);

	if (remote_source)
		g_free(remote_source);
	remote_source =
	    g_strdup(elim_dropdown_get_active_text_or_empty(GTK_DROP_DOWN(combobox)));
}

static GtkWidget *create_module_manager_dialog(gboolean first_run)
{
	const gchar *ids[] = {"dialog", NULL};
	gxml = elim_gtk_builder_new();
	gtk_builder_add_objects_from_resource(gxml, "/org/xiphos/ui/module-manager.gtkbuilder", ids, NULL);
	g_return_val_if_fail((gxml != NULL), NULL);

	dialog_modmgr = UI_GET_ITEM(gxml, "dialog");
	gtk_window_set_default_size(GTK_WINDOW(dialog_modmgr), settings.modmgr_width, settings.modmgr_height);

	/* response buttons */
	button_close = UI_GET_ITEM(gxml, "button_close");
	button_cancel = UI_GET_ITEM(gxml, "button_cancel");
	button_refresh = UI_GET_ITEM(gxml, "button_refresh");
	button_install = UI_GET_ITEM(gxml, "button_install");
	button_remove = UI_GET_ITEM(gxml, "button_remove");
	button_arch = UI_GET_ITEM(gxml, "button_archive");
	button_idx = UI_GET_ITEM(gxml, "button_index");
	button_delidx = UI_GET_ITEM(gxml, "button_delete_index");
	button_obsolete = UI_GET_ITEM(gxml, "button_scan_obsolete");
	button_load_sources = UI_GET_ITEM(gxml, "button_load_sources");
	button_intro = UI_GET_ITEM(gxml, "button_view_intro");


	g_signal_connect(dialog_modmgr, "destroy",
			 G_CALLBACK(on_dialog_destroy), NULL);
	if (first_run)
		setup_dialog_action_area(GTK_DIALOG(dialog_modmgr));
	else
		g_signal_connect(dialog_modmgr, "response",
				 G_CALLBACK(on_mod_mgr_response), NULL);

	/* progress bars */
	progressbar_refresh = UI_GET_ITEM(gxml, "progressbar1");
	gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progressbar_refresh), TRUE);

	/* treeviews */
	treeview1 = UI_GET_ITEM(gxml, "treeview1");
	setup_treeview_main(treeview1);

	treeview_local = UI_GET_ITEM(gxml, "treeview2");
	treeview_remote = UI_GET_ITEM(gxml, "treeview3");
	setup_treeviews_local_remote(treeview_local, treeview_remote);

	treeview = UI_GET_ITEM(gxml, "treeview4");
	treeview2 = UI_GET_ITEM(gxml, "treeview5");
	setup_treeview_install(treeview);
	setup_treeview_maintenance(treeview2);
	gui_widget_on_key_phase(GTK_WIDGET(treeview), GTK_PHASE_CAPTURE, (GuiKeyFunc)on_modules_list_key_press, NULL, NULL);
	gui_widget_on_key_phase(GTK_WIDGET(treeview2), GTK_PHASE_CAPTURE, (GuiKeyFunc)on_modules_list_key_press, NULL, NULL);

	/* notebook */
	notebook1 = UI_GET_ITEM(gxml, "notebook1");
	g_signal_connect(notebook1, "switch_page",
			 G_CALLBACK(on_notebook1_switch_page), NULL);

	/* labels */
	label_home = UI_GET_ITEM(gxml, "label_home");
	label_system = UI_GET_ITEM(gxml, "label_sword_sys");

	/* sources buttons */
	button_add_local = UI_GET_ITEM(gxml, "button_add_local");
	button_remove_local = UI_GET_ITEM(gxml, "button_remove_local");
	button_add_remote = UI_GET_ITEM(gxml, "button_add_remote");
	button_remove_remote = UI_GET_ITEM(gxml, "button_remove_remote");

	g_signal_connect(button_add_local, "clicked",
			 G_CALLBACK(on_button_add_local_clicked), NULL);
	g_signal_connect(button_remove_local, "clicked",
			 G_CALLBACK(on_button_remove_local_clicked), NULL);
	g_signal_connect(button_add_remote, "clicked",
			 G_CALLBACK(on_button_add_remote_clicked), NULL);
	g_signal_connect(button_remove_remote, "clicked",
			 G_CALLBACK(on_button_remove_remote_clicked),
			 NULL);

	/* combo box entrys */
	combo_entry1 = UI_GET_ITEM(gxml, "comboboxentry1");
	combo_entry2 = UI_GET_ITEM(gxml, "comboboxentry2");
	set_combobox(GTK_DROP_DOWN(combo_entry1));
	set_combobox(GTK_DROP_DOWN(combo_entry2));

	/* radio buttons */
	radiobutton_source = UI_GET_ITEM(gxml, "radiobutton1"); /* local */
	radiobutton2 = UI_GET_ITEM(gxml, "radiobutton2");       /* remote */
	radiobutton_dest = UI_GET_ITEM(gxml, "radiobutton3");   /* homedir */
	radiobutton4 = UI_GET_ITEM(gxml, "radiobutton4");       /* homedir */

	setup_ui_labels();
	set_controls_to_last_use();
	g_signal_connect(radiobutton2, "toggled",
			 G_CALLBACK(on_radiobutton2_toggled), NULL);
	g_signal_connect(radiobutton4, "toggled",
			 G_CALLBACK(on_radiobutton4_toggled), NULL);
	g_signal_connect((gpointer)combo_entry1, "notify::selected",
			 G_CALLBACK(on_comboboxentry_local_changed), NULL);
	g_signal_connect((gpointer)combo_entry2, "notify::selected",
			 G_CALLBACK(on_comboboxentry_remote_changed),
			 NULL);
	if (first_run)
		gui_toggle_set_active(GTK_WIDGET(radiobutton2), TRUE);

	gtk_widget_hide(button_refresh);

	g_signal_connect(dialog_modmgr, "notify::default-width",
			 G_CALLBACK(on_modmgr_configure_event), NULL);
	g_signal_connect(dialog_modmgr, "notify::default-height",
			 G_CALLBACK(on_modmgr_configure_event), NULL);

	settings.display_modmgr = 1;
	xml_set_value("Xiphos", "layout", "modmgropen", "1");

	return dialog_modmgr;
}

/******************************************************************************
 * Name
 *   gui_open_mod_mgr
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void gui_open_mod_mgr(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_open_mod_mgr(void)
{
	if (!is_running) {
		GtkWidget *dlg;
		dlg = create_module_manager_dialog(FALSE);
		set_window_icon(GTK_WINDOW(dlg));
		is_running = TRUE;
	} else
		gtk_window_present(GTK_WINDOW(dialog_modmgr));
}

/******************************************************************************
 * Name
 *   gui_open_mod_mgr_initial_run
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void gui_open_mod_mgr_initial_run(void)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_open_mod_mgr_initial_run(void)
{
	GtkWidget *dlg;
	first_time_user = TRUE;
	dlg = create_module_manager_dialog(TRUE);
	set_window_icon(GTK_WINDOW(dlg));
	/* waits until the dialog is closed */
	initial_run_loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(initial_run_loop);
	g_clear_pointer(&initial_run_loop, g_main_loop_unref);
	first_time_user = FALSE;
	settings.display_modmgr = 0;
	xml_set_value("Xiphos", "layout", "modmgropen", "0");
}

/******************************************************************************
 * Name
 *   gui_update_install_status
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *  void gui_update_install_status(glong total, glong done, const gchar * message)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_mod_mgr_bind_progress(GtkWidget *bar)
{
	progressbar_alt = bar;
}

void gui_mod_mgr_set_current_mod(const gchar *name)
{
	current_mod = (gchar *)name;
}

void gui_update_install_status(glong total, glong done,
			       const gchar *message)
{
	gchar *buf;
	GtkWidget *bar = progressbar_refresh ? progressbar_refresh
					     : progressbar_alt;

	if (!bar)
		return; /* no module manager dialog open (e.g. silent install) */

	if (current_mod)
		buf = g_strdup_printf("%s: %s", current_mod, message);
	else
		buf = g_strdup(message);
	gui_set_progressbar_text(bar, buf);
	g_free(buf);
}

/******************************************************************************
 * Name
 *   gui_update_install_progressbar
 *
 * Synopsis
 *   #include "gui/mod_mgr.h"
 *
 *   void gui_update_install_progressbar(gdouble fraction)
 *
 * Description
 *
 *
 * Return value
 *   void
 */

void gui_update_install_progressbar(gdouble fraction)
{
	GtkWidget *bar = progressbar_refresh ? progressbar_refresh
					     : progressbar_alt;

	if (!bar)
		return; /* no module manager dialog open (e.g. silent install) */

	gui_set_progressbar_fraction(bar, fraction);
}
