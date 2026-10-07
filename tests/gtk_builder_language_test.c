#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "main/app_language.h"
#include "gui/utilities.h"
#include <glib/gi18n.h>

/* GtkStringList is the model used by the app's dropdowns. Unlike a window,
 * this real GtkBuilder object needs no display server. */
int main(int argc, char **argv)
{
	app_language_initialize(argc == 2 ? argv[1] : "en_GB", NULL);
	g_type_ensure(GTK_TYPE_STRING_LIST);
	GtkBuilder *builder = elim_gtk_builder_new();
	const char *ui = "<interface><object class=\"GtkStringList\" id=\"labels\">"
		"<items><item translatable=\"yes\">Estudio bíblico</item>"
		"<item translatable=\"yes\">Instalar Biblias</item></items>"
		"</object></interface>";
	GError *error = NULL;
	g_assert_true(gtk_builder_add_from_string(builder, ui, -1, &error));
	g_assert_no_error(error);
	GtkStringList *labels = GTK_STRING_LIST(gtk_builder_get_object(builder, "labels"));
	g_assert_nonnull(labels);
	g_assert_cmpstr(gtk_string_list_get_string(labels, 0), ==, _("Estudio bíblico"));
	g_assert_cmpstr(gtk_string_list_get_string(labels, 1), ==, _("Instalar Biblias"));
	g_print("%s\n%s\n", gtk_string_list_get_string(labels, 0),
		gtk_string_list_get_string(labels, 1));
	g_object_unref(builder);
	return 0;
}
