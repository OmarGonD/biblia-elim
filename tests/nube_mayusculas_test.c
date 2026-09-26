/*
 * CLOUD-CASE-101: the word cloud shows proper names as the text writes
 * them, learned from the text instead of a fixed list.
 */
#include <glib.h>
#include <string.h>

#include "main/nube_mayusculas.h"

/* Ruth 1:1-4, 1:22, 2:1, 4:17-22 (Reina-Valera 1909), one verse per
 * string, as the counter reads them. */
static const gchar *const RUT[] = {
    "Y ACONTECIÓ en los días que gobernaban los jueces, que hubo hambre en "
    "la tierra. Y un varón de Beth-lehem de Judá, fué á peregrinar en los "
    "campos de Moab, él y su mujer, y dos hijos suyos.",
    "El nombre de aquel varón era Elimelech, y el de su mujer Noemi; y los "
    "nombres de sus dos hijos eran, Mahalón y Chelión, Ephrateos de "
    "Beth-lehem de Judá. Llegaron pues á los campos de Moab, y asentaron "
    "allí.",
    "Y murió Elimelech, marido de Noemi, y quedó ella con sus dos hijos;",
    "Los cuales tomaron para sí mujeres de Moab, el nombre de la una Orpha, "
    "y el nombre de la otra Ruth; y habitaron allí unos diez años.",
    "Así volvió Noemi y Ruth Moabita su nuera con ella; volvió de los "
    "campos de Moab, y llegaron á Beth-lehem en el principio de la siega "
    "de las cebadas.",
    "Y TENÍA Noemi un pariente de su marido, varón poderoso y de hecho, de "
    "la familia de Elimelech, el cual se llamaba Booz.",
    "Y las vecinas diciendo, A Noemi ha nacido un hijo, le pusieron nombre; "
    "y llamáronle Obed. Este es padre de Isaí, padre de David.",
    "Y estas son las generaciones de Phares: Phares engendró á Hesrón,",
    "Y Hesrón engendró á Ram, y Ram engendró á Aminadab,",
    "Y Aminadab engendró á Naasón, y Naasón engendró á Salmón,",
    "Y Salmón engendró á Booz, y Booz engendró á Obed,",
    "Y Obed engendró á Isaí, é Isaí engendró á David.",
    /* JEHOVÁ in small caps, and «campo» in lowercase everywhere. */
    "Y ella dijo: Que JEHOVÁ os lo pague; y fué al campo, y Booz le dijo.",
    "Campos y más campo; el campo de Booz. Hijos, hijos.",
};

static void
anotar(const gchar *palabra, gboolean abre_frase, gpointer m)
{
	nube_mayusculas_anotar((NubeMayusculas *)m, palabra, abre_frase);
}

static NubeMayusculas *
aprendido(void)
{
	NubeMayusculas *m = nube_mayusculas_nueva();
	for (guint i = 0; i < G_N_ELEMENTS(RUT); ++i)
		nube_recorrer_palabras(RUT[i], anotar, m);
	return m;
}

static void
prueba_nombres_de_rut(void)
{
	NubeMayusculas *m = aprendido();
	const gchar *const nombres[][2] = {
	    {"noemi", "Noemi"},	    {"booz", "Booz"},	  {"moab", "Moab"},
	    {"elimelech", "Elimelech"}, {"mahalón", "Mahalón"}, {"isaí", "Isaí"},
	    {"phares", "Phares"},   {"ruth", "Ruth"},	  {"orpha", "Orpha"},
	    {"obed", "Obed"},	    {"david", "David"},	  {"judá", "Judá"},
	    {"beth", "Beth"},	    {"jehová", "Jehová"},  {"hesrón", "Hesrón"},
	    {"aminadab", "Aminadab"}, {"salmón", "Salmón"}, {"moabita", "Moabita"},
	};
	for (guint i = 0; i < G_N_ELEMENTS(nombres); ++i) {
		gchar *f = nube_mayusculas_forma(m, nombres[i][0]);
		g_assert_cmpstr(f, ==, nombres[i][1]);
		g_free(f);
	}
	nube_mayusculas_libre(m);
}

static void
prueba_palabras_comunes(void)
{
	NubeMayusculas *m = aprendido();
	/* Capitalised only where a sentence or a verse opens: common words. */
	const gchar *const comunes[] = {"campo", "campos", "hijos", "varón",
					"nombre", "tierra", "engendró", "así",
					"aconteció", "tenía", "desconocida"};
	for (guint i = 0; i < G_N_ELEMENTS(comunes); ++i) {
		gchar *f = nube_mayusculas_forma(m, comunes[i]);
		g_assert_cmpstr(f, ==, comunes[i]);
		g_free(f);
	}
	nube_mayusculas_libre(m);
}

/* What the text does most of the time decides: «el Señor» / «el señor». */
static void
prueba_mayoria(void)
{
	NubeMayusculas *m = nube_mayusculas_nueva();
	nube_mayusculas_anotar(m, "Señor", FALSE);
	nube_mayusculas_anotar(m, "Señor", FALSE);
	nube_mayusculas_anotar(m, "señor", FALSE);
	nube_mayusculas_anotar(m, "Casa", TRUE); /* opens a sentence: says nothing */
	nube_mayusculas_anotar(m, "casa", FALSE);
	nube_mayusculas_anotar(m, "Ala", FALSE);
	nube_mayusculas_anotar(m, "ala", FALSE); /* a tie is not a name */
	gchar *f = nube_mayusculas_forma(m, "señor");
	g_assert_cmpstr(f, ==, "Señor");
	g_free(f);
	f = nube_mayusculas_forma(m, "casa");
	g_assert_cmpstr(f, ==, "casa");
	g_free(f);
	f = nube_mayusculas_forma(m, "ala");
	g_assert_cmpstr(f, ==, "ala");
	g_free(f);
	g_assert_null(nube_mayusculas_forma(m, NULL));
	f = nube_mayusculas_forma(NULL, "x");
	g_assert_cmpstr(f, ==, "x");
	g_free(f);
	nube_mayusculas_libre(m);
}

typedef struct {
	GString *s;
} Traza;

static void
trazar(const gchar *p, gboolean abre, gpointer d)
{
	g_string_append_printf(((Traza *)d)->s, "%s%s ", abre ? "^" : "", p);
}

static void
prueba_recorrido(void)
{
	Traza t = {g_string_new(NULL)};
	nube_recorrer_palabras("«Dijo: ¿Quién? —Él, de Moab.» God's l'amour 3 x",
			       trazar, &t);
	g_assert_cmpstr(t.s->str, ==,
			"^Dijo ^Quién ^Él de Moab ^God's l'amour x ");
	g_string_truncate(t.s, 0);
	/* A chapter's small-caps opening; JEHOVÁ further in is a word. */
	nube_recorrer_palabras("Y ACONTECIÓ que JEHOVÁ dijo", trazar, &t);
	g_assert_cmpstr(t.s->str, ==, "^Y ^ACONTECIÓ que JEHOVÁ dijo ");
	g_string_free(t.s, TRUE);
	nube_recorrer_palabras(NULL, trazar, NULL);
	nube_recorrer_palabras("\xff\xfe", trazar, NULL);
}

int
main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/nube/nombres-de-rut", prueba_nombres_de_rut);
	g_test_add_func("/nube/palabras-comunes", prueba_palabras_comunes);
	g_test_add_func("/nube/mayoria", prueba_mayoria);
	g_test_add_func("/nube/recorrido", prueba_recorrido);
	return g_test_run();
}
