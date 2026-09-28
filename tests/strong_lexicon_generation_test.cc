/*
 * MORPH-109: the neutral Strong lexicon.
 *
 * Two things need proving, independent of GTK:
 *
 *   1. scripts/generate_strongs_lexicon.py, run against the real bundled
 *      ui/strongs-elim.xml, produces a lexicon_entries SQLite file that
 *      SqliteStrongLexicon reads back correctly for real, known Strong
 *      numbers (lemma/transliteration/definition, not fabricated).
 *   2. main_bind_strong_lexicon() (src/main/strong_lexicon_startup.cc),
 *      the startup glue that calls main_set_strong_lexicon() exactly
 *      once, degrades gracefully -- no crash, nothing bound -- when the
 *      lexicon file it is pointed at does not exist.
 *
 * This binary stubs main_set_strong_lexicon() itself (like
 * sqlite_interlinear_test.cc stubs main_morf_es()) instead of linking
 * the real strong_ui.cc, so it needs no GTK/backend machinery to
 * exercise the startup glue in isolation.
 */

#include <cstdio>
#include <cstdlib>
#include <string>

#include <glib.h>
#include <glib/gstdio.h>

#include "backend/bible_lexicon.h"
#include "backend/sqlite/sqlite_strong_lexicon.h"
#include "backend/strong_id.h"
#include "main/strong_lexicon_startup.h"

namespace {
const BibleLexicon *g_captured = nullptr;
int g_setCalls = 0;
} // namespace

extern "C" void main_set_strong_lexicon(BibleLexicon *lexicon)
{
	g_captured = lexicon;
	++g_setCalls;
}

namespace {

int failures = 0;

void check(bool condition, const char *what)
{
	if (!condition) {
		++failures;
		g_printerr("FAIL: %s\n", what);
	}
}

/* Runs scripts/generate_strongs_lexicon.py against the real
 * ui/strongs-elim.xml, writing to `output`. Returns true on a
 * zero exit status. */
bool runGenerator(const std::string &output)
{
	gchar *python = g_find_program_in_path("python3");
	if (!python) {
		g_printerr("SKIP: no python3 in PATH\n");
		return false;
	}
	const std::string script = std::string(SRCDIR) +
		"/scripts/generate_strongs_lexicon.py";
	const std::string xml = std::string(SRCDIR) + "/ui/strongs-elim.xml";
	gchar *argv[] = {
		python,
		const_cast<gchar *>(script.c_str()),
		const_cast<gchar *>("--xml"),
		const_cast<gchar *>(xml.c_str()),
		const_cast<gchar *>("--output"),
		const_cast<gchar *>(output.c_str()),
		nullptr,
	};
	gint exitStatus = -1;
	gchar *stdErr = nullptr;
	GError *error = nullptr;
	gboolean ok = g_spawn_sync(nullptr, argv, nullptr, G_SPAWN_SEARCH_PATH,
		nullptr, nullptr, nullptr, &stdErr, &exitStatus, &error);
	if (!ok || exitStatus != 0) {
		g_printerr("generator failed: %s\n",
			error ? error->message : (stdErr ? stdErr : "?"));
	}
	if (error) g_error_free(error);
	g_free(stdErr);
	g_free(python);
	return ok && exitStatus == 0;
}

/* Test 1: real generation + real lookups for well-known, stable Strong
 * numbers. Lemma/transliteration are the actual Greek/Hebrew word forms,
 * not the (occasionally re-translated) Spanish prose in `d`/`g`, so they
 * make a durable regression anchor. */
void pruebaGeneracionYBusquedasReales()
{
	const std::string output = std::string(g_get_tmp_dir()) +
		"/xiphos-strongs-lexicon-test.sqlite";
	g_remove(output.c_str());
	if (!runGenerator(output)) {
		g_printerr("SKIP: generator unavailable, skipping lookup checks\n");
		return;
	}

	SqliteStrongLexicon lexicon(output);

	StrongId g26, h430, g2316, missing;
	check(parseStrongId("G26", g26), "parse G26");
	check(parseStrongId("H430", h430), "parse H430");
	check(parseStrongId("G2316", g2316), "parse G2316");
	check(parseStrongId("G99999", missing), "parse G99999");

	LexiconEntry love = lexicon.lookupStrong(g26);
	check(love.valid, "G26 lookup valid");
	check(love.lemma == "ἀγάπη", "G26 lemma is ἀγάπη");
	check(love.transliteration == "agápē", "G26 transliteration is agápē");
	check(!love.definition.empty(), "G26 definition non-empty");

	LexiconEntry elohim = lexicon.lookupStrong(h430);
	check(elohim.valid, "H430 lookup valid");
	check(elohim.lemma == "אֱלֹהִים", "H430 lemma is אֱלֹהִים");
	check(!elohim.transliteration.empty(), "H430 transliteration non-empty");
	check(!elohim.definition.empty(), "H430 definition non-empty");

	LexiconEntry theos = lexicon.lookupStrong(g2316);
	check(theos.valid, "G2316 lookup valid");
	check(theos.lemma == "θεός", "G2316 lemma is θεός");

	/* pronunciation is a documented-optional column and the bundled XML
	 * has no such field: must come back empty, never invented. */
	check(love.pronunciation.empty(), "G26 pronunciation left empty, not fabricated");

	check(!lexicon.lookupStrong(missing).valid, "unknown Strong number stays invalid");

	g_remove(output.c_str());
}

/* Test 2: main_bind_strong_lexicon()'s BIBLIA_ELIM_STRONG_LEXICON
 * override, both ways -- bound when the file exists, gracefully
 * unbound (no crash) when it does not. */
void pruebaEnlaceYRespaldo()
{
	/* a) points at a real, freshly generated lexicon: bound. */
	const std::string real = std::string(g_get_tmp_dir()) +
		"/xiphos-strongs-lexicon-bind-test.sqlite";
	g_remove(real.c_str());
	if (runGenerator(real)) {
		g_setenv("BIBLIA_ELIM_STRONG_LEXICON", real.c_str(), TRUE);
		main_bind_strong_lexicon();
		check(g_captured != nullptr, "bind: lexicon captured when file exists");
		if (g_captured) {
			StrongId g26;
			parseStrongId("G26", g26);
			check(g_captured->contains(g26),
				"bind: bound lexicon resolves a real Strong number");
		}
		g_remove(real.c_str());
	} else {
		g_printerr("SKIP: generator unavailable, skipping bound-lexicon check\n");
	}

	/* b) points at a path that does not exist: no crash, nothing
	 * bound (or a previous binding is cleared). This is the
	 * "graceful fallback" the task asks for. */
	const std::string missing = std::string(g_get_tmp_dir()) +
		"/xiphos-strongs-lexicon-does-not-exist.sqlite";
	g_remove(missing.c_str());
	g_setenv("BIBLIA_ELIM_STRONG_LEXICON", missing.c_str(), TRUE);
	main_bind_strong_lexicon();
	check(g_captured == nullptr,
		"bind: no lexicon bound when the file is absent");

	g_unsetenv("BIBLIA_ELIM_STRONG_LEXICON");
}

} // namespace

int main()
{
	pruebaGeneracionYBusquedasReales();
	pruebaEnlaceYRespaldo();
	g_print("main_set_strong_lexicon called %d time(s); failures=%d\n",
		g_setCalls, failures);
	return failures ? 1 : 0;
}
