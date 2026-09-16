/*
 * Routing of a note marker click.
 *
 * The Bible pane renders SWORD's own anchors; for SpaPlatense Matthew
 * 11:30 the marker is
 *
 *   passagestudy.jsp?action=showNote&type=n&value=1
 *                   &module=SpaPlatense&passage=Matthew+11%3A30
 *
 * so the routing input is (type, clicked) and nothing else -- never the
 * link's visible text ("*n"), never the module name.
 *
 * The case that regressed: type 'n' with clicked set had no branch at
 * all, so a clicked editorial marker did nothing.  A user clicking
 * again had the second press delivered by GTK as a double click, which
 * runs the clipboard word lookup and opens the generic dictionary
 * dialog ("Palabra", lexicon search, studies grouped by author).
 */
#include <glib.h>

#include <stdio.h>

#include "main/note_action.h"

static int failures;

#define CHECK(condition)                                                     \
	do {                                                                 \
		if (!(condition)) {                                          \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__,        \
				__LINE__, #condition);                       \
			failures++;                                          \
		}                                                            \
	} while (0)

int
main(void)
{
	/* A. The bug: an editorial note marker that was really clicked
	 * routes to the author commentary, and to nothing else. */
	CHECK(main_note_action_for("n", 1) == NOTE_ACTION_AUTHOR_COMMENTARY);
	CHECK(main_note_action_for("n", 1) != NOTE_ACTION_NONE);
	CHECK(main_note_action_for("n", 1) != NOTE_ACTION_NOTE_PREVIEW);
	CHECK(main_note_action_for("n", 1) != NOTE_ACTION_CROSSREF_LIST);
	CHECK(main_note_action_for("n", 1) != NOTE_ACTION_CROSSREF_PREVIEW);

	/* E. Every other marker keeps the action it already had. */

	/* hovering a note still previews it, it does not open the pane */
	CHECK(main_note_action_for("n", 0) == NOTE_ACTION_NOTE_PREVIEW);
	CHECK(main_note_action_for("n", 0) != NOTE_ACTION_AUTHOR_COMMENTARY);

	/* a cross reference stays a cross reference, clicked or hovered */
	CHECK(main_note_action_for("x", 1) == NOTE_ACTION_CROSSREF_LIST);
	CHECK(main_note_action_for("x", 0) == NOTE_ACTION_CROSSREF_PREVIEW);
	CHECK(main_note_action_for("x", 1) != NOTE_ACTION_AUTHOR_COMMENTARY);
	CHECK(main_note_action_for("x", 0) != NOTE_ACTION_AUTHOR_COMMENTARY);

	/* a marker claiming both keeps the cross-reference precedence the
	 * router has always had -- clicked goes to the reference list, not
	 * to the commentary pane. */
	CHECK(main_note_action_for("nx", 1) == NOTE_ACTION_CROSSREF_LIST);
	CHECK(main_note_action_for("xn", 1) == NOTE_ACTION_CROSSREF_LIST);
	CHECK(main_note_action_for("nx", 0) == NOTE_ACTION_NOTE_PREVIEW);
	CHECK(main_note_action_for("xn", 0) == NOTE_ACTION_NOTE_PREVIEW);

	/* Nothing to route without a marker type. Only showNote URLs reach
	 * this router at all -- Strong's, morphology, dictionary and word
	 * lookups are separate actions dispatched before it, so their
	 * "type" values never arrive here. */
	CHECK(main_note_action_for(NULL, 1) == NOTE_ACTION_NONE);
	CHECK(main_note_action_for(NULL, 0) == NOTE_ACTION_NONE);
	CHECK(main_note_action_for("", 1) == NOTE_ACTION_NONE);
	CHECK(main_note_action_for("", 0) == NOTE_ACTION_NONE);
	CHECK(main_note_action_for("z", 1) == NOTE_ACTION_NONE);
	CHECK(main_note_action_for("z", 0) == NOTE_ACTION_NONE);

	printf("note_action_failures=%d\n", failures);
	return failures ? 1 : 0;
}
