/*
 * Biblia Elim — what a click (or a hover) on a note marker means.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "main/note_action.h"

NoteAction
main_note_action_for(const char *type, int clicked)
{
	if (!type || !*type)
		return NOTE_ACTION_NONE;

	/* Cross references win over notes for a marker that claims to be
	 * both ("nx"): that is the precedence this router has always had,
	 * and a combined marker's useful action is its reference list.
	 */
	if (strchr(type, 'x') && clicked)
		return NOTE_ACTION_CROSSREF_LIST;
	if (strchr(type, 'n') && !clicked)
		return NOTE_ACTION_NOTE_PREVIEW;
	if (strchr(type, 'x') && !clicked)
		return NOTE_ACTION_CROSSREF_PREVIEW;
	/* An editorial note the user actually clicked.  This case used to
	 * fall off the end of the chain and do nothing at all, which is
	 * what made a second click land as a double click and open the
	 * generic dictionary dialog instead. */
	if (strchr(type, 'n') && clicked)
		return NOTE_ACTION_AUTHOR_COMMENTARY;
	return NOTE_ACTION_NONE;
}
