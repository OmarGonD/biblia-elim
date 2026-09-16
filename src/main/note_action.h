/*
 * Biblia Elim — what a click (or a hover) on a note marker means.
 *
 * The Bible pane renders SWORD's own note anchors, e.g. for SpaPlatense
 * Matthew 11:30:
 *
 *   <a class="noteMarker" href="passagestudy.jsp?action=showNote&type=n
 *      &value=1&module=SpaPlatense&passage=Matthew+11%3A30">*n</a>
 *
 * "type" is the semantic marker: 'n' is an editorial/author footnote,
 * 'x' a cross reference.  That, plus whether the user actually clicked
 * or is merely hovering, is the whole routing decision -- never the
 * link's visible text.
 *
 * It lives apart from url.cc so the decision is one testable function
 * that show_note() itself dispatches on: a second, parallel classifier
 * would be free to drift from the real dispatch.
 */
#ifndef XIPHOS_NOTE_ACTION_H
#define XIPHOS_NOTE_ACTION_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	/* nothing to do: unknown marker type */
	NOTE_ACTION_NONE = 0,
	/* cross reference, clicked: list the targets in the sidebar */
	NOTE_ACTION_CROSSREF_LIST,
	/* cross reference, hovered: show the targets in the previewer */
	NOTE_ACTION_CROSSREF_PREVIEW,
	/* editorial note, hovered: show the note body in the previewer */
	NOTE_ACTION_NOTE_PREVIEW,
	/* editorial note, clicked: open "Comentarios del autor" there */
	NOTE_ACTION_AUTHOR_COMMENTARY
} NoteAction;

/* `type` is the URL's "type" parameter ('n', 'x', or both); `clicked`
 * distinguishes a real click from a hover. */
NoteAction main_note_action_for(const char *type, int clicked);

#ifdef __cplusplus
}
#endif

#endif /* XIPHOS_NOTE_ACTION_H */
