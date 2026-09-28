#include "main/strong_lexicon_startup.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <memory>
#include <string>
#include <vector>

#include <glib.h>

#include "backend/sqlite/sqlite_strong_lexicon.h"
#include "main/strong_ui.h"

namespace {

/* Owns the bound lexicon for the process lifetime: strong_ui.cc's
 * boundLexicon (set via main_set_strong_lexicon()) only stores a raw,
 * non-owning pointer. */
std::unique_ptr<SqliteStrongLexicon> g_boundLexicon;

gchar *firstExistingCandidate(const std::vector<std::string> &candidates)
{
	for (const std::string &candidate : candidates) {
		if (!candidate.empty() &&
		    g_file_test(candidate.c_str(), G_FILE_TEST_IS_REGULAR))
			return g_strdup(candidate.c_str());
	}
	return nullptr;
}

} // namespace

extern "C" void main_bind_strong_lexicon(void)
{
	std::vector<std::string> candidates;

	/* Tests and manual dev runs: an exact path, checked first and
	 * exclusively (an explicit override should not silently fall
	 * through to some other copy on the machine). */
	const char *override_path = g_getenv("BIBLIA_ELIM_STRONG_LEXICON");
	if (override_path && *override_path) {
		candidates.push_back(override_path);
	} else {
#ifdef XIPHOS_BUILD_STRONG_LEXICON
		/* Uninstalled build tree: the exact path
		 * src/main/CMakeLists.txt just generated the file at,
		 * embedded at compile time -- no guessing needed. */
		candidates.push_back(XIPHOS_BUILD_STRONG_LEXICON);
#endif
#ifdef SHARE_DIR
		/* Installed build: alongside the rest of the bundled data
		 * (see cmake/XiphosConfig_h.cmake, install() rule in
		 * src/main/CMakeLists.txt). */
		candidates.push_back(std::string(SHARE_DIR) + G_DIR_SEPARATOR_S +
			"strongs-elim.sqlite");
#endif
	}

	gchar *chosen = firstExistingCandidate(candidates);
	if (!chosen) {
		/* Graceful fallback: no crash, no lexicon bound. The dialog
		 * keeps showing the bare Strong ID, exactly as before this
		 * task. Also clears any previously bound lexicon, so this
		 * function stays safe to call more than once (e.g. a test
		 * that first binds a real lexicon, then re-probes with the
		 * override pointed at a missing file). */
		main_set_strong_lexicon(nullptr);
		g_boundLexicon.reset();
		return;
	}

	g_boundLexicon.reset(new SqliteStrongLexicon(chosen));
	main_set_strong_lexicon(g_boundLexicon.get());
	g_free(chosen);
}
