/*
 * Psalm titles numbered as their own verse (RENDER-PSALM-TITLES-101).
 *
 * A Vulgate edition shows its own numbering: Torres Amat Ps 3:1 is drawn
 * "1 Salmo de David…" with the title text styled, never without its
 * number and never renumbered. splitPsalmTitle() finds the title from
 * markup alone; psalmTitleVerseHtml() lays out what follows the single
 * native number. The source checks pin how RenderOneChapter, the chapter
 * previews and wk-html use them.
 *
 *   cmake --build build --target psalm_title_render_test && \
 *     ./build/tests/psalm_title_render_test
 */
#include <glib.h>

#include <cstdio>
#include <string>

#include "main/psalm_title.h"

static const char ABSALOM[] =
	"Salmo de David cuando temeroso iba huyendo de su hijo Absalom";

static std::string span(const std::string &text)
{
	return "<span class=\"x-psalm-title\">" + text + "</span>";
}

static std::size_t count(const std::string &s, const std::string &needle)
{
	std::size_t n = 0;
	for (std::size_t at = s.find(needle); at != std::string::npos;
	     at = s.find(needle, at + 1))
		n++;
	return n;
}

/* A: title only. The whole slot is the styled title, nothing else. */
static void test_title_only(void)
{
	const std::string html = span(ABSALOM);
	const PsalmTitleParts p = splitPsalmTitle(html);
	g_assert_true(p.hasTitle);
	g_assert_false(p.hasBody);
	g_assert_cmpstr(p.title.c_str(), ==, html.c_str());
	const std::string out = psalmTitleVerseHtml(p);
	g_assert_cmpstr(out.c_str(), ==, html.c_str());
	g_assert_cmpuint(count(out, ABSALOM), ==, 1);
}

/* B / E: title and body share the slot (Ps 52:1). Title, a break, the
 * body; each once, and no second number is produced here. */
static void test_title_and_body(void)
{
	const std::string title =
		span("Para el fin: Por Maeleth. Salmo de inteligencia de David.");
	const std::string body = "Dijo el insensato en su corazon: No hay Dios.";
	const PsalmTitleParts p = splitPsalmTitle(title + " " + body);
	g_assert_true(p.hasTitle);
	g_assert_true(p.hasBody);
	g_assert_cmpstr(p.title.c_str(), ==, title.c_str());
	g_assert_cmpstr(p.body.c_str(), ==, body.c_str());
	const std::string out = psalmTitleVerseHtml(p);
	const std::string expected = title + "<br/>" + body;
	g_assert_cmpstr(out.c_str(), ==, expected.c_str());
	g_assert_cmpuint(count(out, "Maeleth"), ==, 1);
	g_assert_cmpuint(count(out, "insensato"), ==, 1);
	g_assert_true(out.find("sword:///") == std::string::npos);
}

/* C: ordinary verses come back byte for byte. */
static void test_ordinary_verse_untouched(void)
{
	for (const char *html : {
		     "¡Ah Señor! ¿Cómo es que se han aumentado tanto mis perseguidores?",
		     "<span class=\"line indent0\">Oh Yahvé, ¡cuán numerosos</span><br />",
		     "<h3 class=\"title psalm canonical\">Salmo de David</h3>",
		     "<span class=\"x-psalm-title-other\">no es título</span>",
		     "", }) {
		const PsalmTitleParts p = splitPsalmTitle(html);
		g_assert_false(p.hasTitle);
		g_assert_cmpstr(p.body.c_str(), ==, html);
		const std::string out = psalmTitleVerseHtml(p);
		g_assert_cmpstr(out.c_str(), ==, html);
	}
}

/* D: Ps 50:1 and 50:2 (Vulg) are title only, 50:3 is body. Each slot is
 * laid out on its own, so each keeps its own number. */
static void test_consecutive_title_verses(void)
{
	const PsalmTitleParts v1 = splitPsalmTitle(span("Para el fin: Salmo de David;"));
	const PsalmTitleParts v2 = splitPsalmTitle(span(
		"Cuando despues que pecó con Bethsabée, vino á él el Profeta Nathán"));
	const PsalmTitleParts v3 = splitPsalmTitle(
		"Ten piedad de mí, oh Dios, segun la grandeza de tu misericordia");
	g_assert_true(v1.hasTitle && !v1.hasBody);
	g_assert_true(v2.hasTitle && !v2.hasBody);
	g_assert_false(v3.hasTitle);
	g_assert_true(psalmTitleVerseHtml(v1).find("<br/>") == std::string::npos);
	g_assert_true(psalmTitleVerseHtml(v2).find("<br/>") == std::string::npos);
}

static void test_markup_around_title(void)
{
	/* Phrase highlights nest spans inside the title; the split keeps
	 * them and still finds the title's own closing tag. */
	const std::string hl =
		"<span class=\"x-psalm-title\">Salmo <span data-hl-id=\"7\" "
		"class=\"xiphos-hl\">de David</span></span>2 cuerpo";
	PsalmTitleParts p = splitPsalmTitle(hl);
	g_assert_true(p.hasTitle && p.hasBody);
	g_assert_cmpstr(p.body.c_str(), ==, "2 cuerpo");
	g_assert_true(p.title.find("xiphos-hl") != std::string::npos);

	/* An empty fallback marker before the title is not body. */
	p = splitPsalmTitle("<span data-content-fallback=\"X\"></span>" + span(ABSALOM));
	g_assert_true(p.hasTitle && !p.hasBody);

	/* Extra class tokens still identify the title. */
	p = splitPsalmTitle("<span class=\"a x-psalm-title b\">T</span>");
	g_assert_true(p.hasTitle && !p.hasBody);

	/* Body before the span: not a leading title, left as it is. */
	const std::string later = "cuerpo " + span("T");
	p = splitPsalmTitle(later);
	g_assert_false(p.hasTitle);
	g_assert_cmpstr(p.body.c_str(), ==, later.c_str());

	/* Unclosed: left as it is. */
	p = splitPsalmTitle("<span class=\"x-psalm-title\">T");
	g_assert_false(p.hasTitle);
}

/* Wiring: the renderer really uses the split, it is driven by the class
 * and not by words, and the verse keeps its own number.
 *
 * Adapted to the current master. The historical branch also rewrote both
 * chapter-preview snippets; this port keeps to the one place that draws
 * the verses of the chapter being read, which is where the title-and-body
 * slot (Vulg Ps 52:1) actually runs together today. */
static void test_renderer_wiring(void)
{
	gchar *display = nullptr;
	gchar *html = nullptr;
	g_assert_true(g_file_get_contents(SRCDIR "/src/main/display.cc",
					  &display, nullptr, nullptr));
	g_assert_true(g_file_get_contents(SRCDIR "/src/webkit/wk-html.c",
					  &html, nullptr, nullptr));
	const std::string d(display), w(html);

	/* RenderOneChapter passes the finished verse through the split. */
	g_assert_true(d.find("splitPsalmTitle(rework->str") != std::string::npos);
	g_assert_true(d.find("psalmTitleVerseHtml(title_parts)") != std::string::npos);
	g_assert_true(d.find("#include \"main/psalm_title.h\"") != std::string::npos);

	/* Every verse keeps its anchor and its number: no path hides a
	 * number, renumbers, or moves a title to another slot. */
	g_assert_true(d.find("\"<p class=\\\"verse\\\"><a name=\\\"%d\\\"></a>\"") !=
		      std::string::npos);
	g_assert_true(d.find("title_only") == std::string::npos);
	g_assert_true(d.find("preview_number") == std::string::npos);
	g_assert_true(d.find("after-psalm-title") == std::string::npos);

	/* The presentation is keyed on the class the module emits, and the
	 * style is a quiet superscription, not a second heading renderer. */
	g_assert_true(w.find("class_has(klass, \"x-psalm-title\")") != std::string::npos);
	g_assert_true(w.find("\"psalm-title\"") != std::string::npos);
	g_assert_true(w.find("after-psalm-title") == std::string::npos);

	/* Nothing in either file decides a title from the words, and no
	 * edition is singled out. */
	for (const char *word : { "Salmo de", "Para el fin", "TorresAmat" }) {
		g_assert_true(d.find(word) == std::string::npos);
		g_assert_true(w.find(word) == std::string::npos);
	}
	g_free(display);
	g_free(html);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, nullptr);
	g_test_add_func("/psalm-title/title-only", test_title_only);
	g_test_add_func("/psalm-title/title-and-body", test_title_and_body);
	g_test_add_func("/psalm-title/ordinary-verse", test_ordinary_verse_untouched);
	g_test_add_func("/psalm-title/consecutive-titles", test_consecutive_title_verses);
	g_test_add_func("/psalm-title/markup-around-title", test_markup_around_title);
	g_test_add_func("/psalm-title/renderer-wiring", test_renderer_wiring);
	return g_test_run();
}
