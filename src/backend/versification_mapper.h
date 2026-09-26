/* Carries a verse from one versification's numbering to another's.
 *
 * SQLite modules store verses, not versification data: a Vulgate Bible's
 * Psalm 22:1 is KJV's Psalm 23:1 only through a map of the two systems.
 * A backend without such data of its own is handed a mapper
 * (SqliteBibleBackend::setVersificationMapper); without one it maps only
 * between identical versifications, as before. */
#ifndef BIBLIA_ELIM_VERSIFICATION_MAPPER_H
#define BIBLIA_ELIM_VERSIFICATION_MAPPER_H

#include <string>

class VersificationMapper
{
public:
	virtual ~VersificationMapper() = default;
	/* `from`/`to`: SWORD system names ("KJV", "Vulg", "NRSVA"). The verse
	 * is given and returned as OSIS book, chapter, verse. False when the
	 * verse has no counterpart in `to` (or a name is unknown): the verse
	 * is never reread with the other system's numbering. */
	virtual bool map(const std::string &from, const std::string &osisBook,
			 int chapter, int verse, const std::string &to,
			 std::string &osisBookOut, int &chapterOut,
			 int &verseOut) const = 0;
};

#endif /* BIBLIA_ELIM_VERSIFICATION_MAPPER_H */
