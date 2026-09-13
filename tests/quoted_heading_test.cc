/*
 * SpaRVG stores psalm superscriptions as «title» + verse body. That
 * transform is gated by source_quirks (module SpaRVG only). Dialogue
 * that begins with «…» in other modules must stay verse text.
 *
 * Nacar-Colunga Psalm 1 is missing from the reconstructed module.
 * Torres Amat "o // WMestas." is literal OCR in the module; the
 * renderer does not rewrite it.
 */
#include <glib.h>

#include "backend/quoted_heading.h"
#include "backend/source_quirks.h"

static BibleVerseContent
verse(const char *text)
{
	BibleVerseContent content;
	content.renderedText = text;
	content.plainText = text;
	content.valid = true;
	return content;
}

static void
sparvg_psalm_title_is_heading_not_verse()
{
	BibleVerseContent content = verse(
		"«El piadoso será prosperado, el impío perecerá» "
		"Bienaventurado el varón que no anduvo en consejo de malos.");

	applySourceQuirks("SpaRVG", content);

	g_assert_cmpuint(content.headings.size(), ==, 1);
	g_assert_true(content.headings[0].text.find(
			      "El piadoso será prosperado, el impío perecerá") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("Bienaventurado el varón") == 0);
	g_assert_true(content.renderedText.find("El piadoso será prosperado") ==
		      std::string::npos);
}

static void
other_modules_keep_leading_dialogue()
{
	const char *samples[] = {
		"«Levántate», dijo Jesús, y el paralítico se levantó.",
		"«Señor, sálvame», clamó Pedro.",
		"«¿Dónde está el rey de los judíos?», preguntaron.",
		"«Bebe, señor mío», le contestó ella; y bajando el cántaro "
		"lo cogió con sus manos.",
		"«Yo soy el Dios de tus padres, el Dios de Abrahán y de Isaac "
		"y de Jacob». Pero Moisés, sobrecogido de espanto, no osaba "
		"mirar.",
		"«Regresa a tu casa, y declara las grandes cosas que Dios ha "
		"hecho por ti.» Él siguió su camino, proclamando por toda la "
		"ciudad las grandes cosas que Jesús había hecho por él.",
	};
	const char *modules[] = {
		"SpaRV", "SpaRV1909", "NacarColunga", "SpaPlatense",
		"SpaTDP", "TorresAmat", "KJV",
	};

	for (unsigned m = 0; m < sizeof(modules) / sizeof(modules[0]); m++) {
		g_assert_cmpuint(sourceQuirksForModule(modules[m]), ==,
				 (unsigned)SOURCE_QUIRK_NONE);
		for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]);
		     i++) {
			BibleVerseContent content = verse(samples[i]);
			applySourceQuirks(modules[m], content);
			g_assert_true(content.headings.empty());
			g_assert_cmpstr(content.renderedText.c_str(), ==,
					samples[i]);
		}
	}
}

static void
quoted_only_verse_stays_verse_even_for_sparvg()
{
	BibleVerseContent content =
		verse("«El Señor es mi pastor; nada me faltará.»");
	applySourceQuirks("SpaRVG", content);
	g_assert_true(content.headings.empty());
	g_assert_true(content.renderedText.find("El Señor es mi pastor") !=
		      std::string::npos);
}

static void
markup_inside_sparvg_title_is_kept()
{
	BibleVerseContent content = verse(
		"«Al Músico principal: <transChange type=\"added\">Salmo"
		"</transChange> de David» En Jehová he confiado;");
	applySourceQuirks("SpaRVG", content);
	g_assert_cmpuint(content.headings.size(), ==, 1);
	g_assert_true(content.headings[0].text.find("transChange") !=
		      std::string::npos);
	g_assert_true(content.renderedText.find("En Jehová he confiado") == 0);
}

static void
poetry_and_words_of_jesus_are_not_headings()
{
	BibleVerseContent content = verse(
		"<span class=\"line indent0\">«No toquéis a mis ungidos, "
		"no hagáis mal a mis profetas.»</span> Llevó el hambre "
		"sobre aquella tierra.");
	applySourceQuirks("NacarColunga", content);
	g_assert_true(content.headings.empty());
}

static void
multiple_quotes_stay_in_verse()
{
	BibleVerseContent content = verse(
		"«¿Conocéis a Labán hijo de Najor?» «Le conocemos», "
		"contestaron. «¿Y está bien?»");
	applySourceQuirks("NacarColunga", content);
	g_assert_true(content.headings.empty());
	g_assert_true(content.renderedText.find("¿Conocéis a Labán") !=
		      std::string::npos);
}

static void
unlisted_module_is_not_inferred()
{
	g_assert_cmpuint(sourceQuirksForModule("SpaRVG"), ==,
			 (unsigned)SOURCE_QUIRK_LEADING_QUOTED_SUPERSCRIPTION);
	g_assert_cmpuint(sourceQuirksForModule(""), ==,
			 (unsigned)SOURCE_QUIRK_NONE);
	BibleVerseContent content = verse(
		"«El piadoso será prosperado, el impío perecerá» "
		"Bienaventurado el varón.");
	applySourceQuirks("UnknownBible", content);
	g_assert_true(content.headings.empty());
}

static void
empty_module_verse_is_not_invented()
{
	BibleVerseContent content;
	content.valid = false;
	applySourceQuirks("SpaRVG", content);
	g_assert_true(content.headings.empty());
	g_assert_true(content.renderedText.empty());
}

static void
literal_module_ocr_is_not_rewritten()
{
	const char *stored =
		"Hanse coligado los reyes de la tierra; y se han confederado "
		"los príncipes contra el Señor, y contra su Christo, o // "
		"WMestas.";
	BibleVerseContent content = verse(stored);
	applySourceQuirks("TorresAmat", content);
	g_assert_true(content.headings.empty());
	g_assert_cmpstr(content.renderedText.c_str(), ==, stored);
}

static void
rvr1909_does_not_inherit_rvg_title()
{
	BibleVerseContent content = verse(
		"BIENAVENTURADO el varón que no anduvo en consejo de malos.");
	applySourceQuirks("SpaRV1909", content);
	g_assert_true(content.headings.empty());
	g_assert_true(content.renderedText.find("El piadoso") ==
		      std::string::npos);
}

int
main(void)
{
	sparvg_psalm_title_is_heading_not_verse();
	other_modules_keep_leading_dialogue();
	quoted_only_verse_stays_verse_even_for_sparvg();
	markup_inside_sparvg_title_is_kept();
	poetry_and_words_of_jesus_are_not_headings();
	multiple_quotes_stay_in_verse();
	unlisted_module_is_not_inferred();
	empty_module_verse_is_not_invented();
	literal_module_ocr_is_not_rewritten();
	rvr1909_does_not_inherit_rvg_title();
	return 0;
}
