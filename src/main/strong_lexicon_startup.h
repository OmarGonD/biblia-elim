#ifndef XIPHOS_STRONG_LEXICON_STARTUP_H
#define XIPHOS_STRONG_LEXICON_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/* MORPH-109: bind the neutral Strong lexicon into strong_ui.cc's
 * word-detail dialog for SQLite-backed modules, exactly once, at
 * application startup (called from main_init_backend(), src/main/sword.cc).
 *
 * The lexicon SQLite file (lexicon_entries table, see
 * src/backend/strong-lexicon-format.md) is generated at build time from
 * the bundled ui/strongs-elim.xml by scripts/generate_strongs_lexicon.py
 * (see src/main/CMakeLists.txt). This function looks for that file --
 * first an override for tests/dev
 * (BIBLIA_ELIM_STRONG_LEXICON env var, an exact path), then the
 * uninstalled build-tree copy, then the installed SHARE_DIR copy -- and
 * calls main_set_strong_lexicon() with it if found.
 *
 * Absence of the file at every candidate location is not an error: the
 * function returns without binding anything, and the dialog keeps
 * today's bare-Strong-ID behavior. Safe to call more than once (e.g.
 * from a test with a different BIBLIA_ELIM_STRONG_LEXICON each time). */
void main_bind_strong_lexicon(void);

#ifdef __cplusplus
}
#endif

#endif /* XIPHOS_STRONG_LEXICON_STARTUP_H */
