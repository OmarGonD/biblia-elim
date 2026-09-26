/* SWORD's locale manager for the reader's language only. */
#ifndef BIBLIA_ELIM_SWORD_LOCALE_H
#define BIBLIA_ELIM_SWORD_LOCALE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SWORD's system LocaleMgr reads every locale it finds (58 locales, 95
 * files on a usual install: 170-380 ms at startup) to serve the one the
 * reader uses. This installs a LocaleMgr holding only the locale that
 * matches `sys_locale` -- chosen by the same rule as before (the whole
 * name, then 5, then 2 characters, over the names in order) and loaded
 * as SWORD loads a locale directory (same encoding filter, files of one
 * name merged) -- and makes it the default. Call it after the
 * StringMgr is set. Returns the chosen name (g_free), or NULL when no
 * locale file matches; nothing is installed then, and the caller falls
 * back to SWORD's full manager.
 */
char *swordInstallReaderLocale(const char *sys_locale);

#ifdef __cplusplus
}
#endif

#endif /* BIBLIA_ELIM_SWORD_LOCALE_H */
