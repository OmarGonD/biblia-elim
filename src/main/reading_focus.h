/*
 * reading_focus.h - which verse the reader is on, decided from geometry
 *
 * Pure policy, no GTK: the Bible pane measures where each rendered verse
 * sits in the viewport and asks these functions what to do with it.
 *
 * Two questions, one per kind of movement:
 *
 *   user scroll (wheel, touchpad, scrollbar): the viewport moves freely
 *   and the focused verse follows it -- reading_focus_track();
 *
 *   navigation (Up/Down arrows): the focused verse moves and the viewport
 *   follows it, as little as possible -- reading_zone_scroll_delta().
 */
#ifndef XIPHOS_READING_FOCUS_H
#define XIPHOS_READING_FOCUS_H

#include <glib.h>

G_BEGIN_DECLS

/* Where the eye rests while reading, as a fraction of the viewport
 * height from the top: the upper third. Lower (towards the first visible
 * line) moves the focus on before the previous verse is finished; the
 * centre arrives late. Candidates 0.30-0.40; not yet tuned by eye. */
#define READING_FOCUS_LINE_RATIO 0.33

/* How far past the boundary between the focused verse and the next (or
 * previous) one the reading line has to go before the focus follows, as
 * a fraction of viewport height: a boundary sitting right on the line
 * cannot make the band flicker. Only that one boundary. This is the
 * balanced mode's (the default); the placement of the navigation handoff
 * and the bottom reserve always use it. */
#define READING_FOCUS_HYSTERESIS_RATIO 0.03

/* How readily the focus leaves the verse it is on while scrolling (Ver >
 * Navegación y rueda). Only the boundary distance above changes, the same
 * both ways; the reading line, the geometry and the navigation handoff do
 * not. */
typedef enum {
	READING_FOCUS_IMMEDIATE, /* almost as soon as the next verse is on the line */
	READING_FOCUS_BALANCED,	 /* the default */
	READING_FOCUS_STABLE	 /* a little more scrolling before letting go */
} ReadingFocusMode;

#define READING_FOCUS_HYSTERESIS_IMMEDIATE_RATIO 0.01
#define READING_FOCUS_HYSTERESIS_BALANCED_RATIO READING_FOCUS_HYSTERESIS_RATIO
#define READING_FOCUS_HYSTERESIS_STABLE_RATIO 0.06

/* The boundary distance of `mode`, as a fraction of viewport height. */
gdouble reading_focus_hysteresis_ratio(ReadingFocusMode mode);
/* The mode reading_focus_pick() and reading_focus_track*() use from now on
 * (an out-of-range value selects the balanced mode). */
void reading_focus_set_mode(ReadingFocusMode mode);
ReadingFocusMode reading_focus_get_mode(void);

/* Arrow navigation leaves the viewport alone while the focused verse is
 * inside this band, and otherwise scrolls just enough to bring it back. */
#define READING_ZONE_TOP_RATIO 0.20
#define READING_ZONE_BOTTOM_RATIO 0.75

/* One rendered verse. id is chapter * 1000 + verse, as in the pane's
 * anchors; top/bottom are pixels from the top of the viewport (negative
 * above it). Blocks are passed in document order. */
typedef struct {
	gint id;
	gdouble top;
	gdouble bottom;
} ReadingFocusBlock;

/* Real verse slots only: chapter >= 1 and verse >= 1. Chapter anchors
 * (chapter * 1000, verse 0: headings and intros) and the pane's inert
 * markers (0, previews) are not. */
gboolean reading_focus_id_is_verse(gint id);

/* Snapshot rule, without the line's history: the verse under the reading
 * line (or nearest to it), unless the current verse is still within the
 * hysteresis distance of the line, or the winner lies against `direction`.
 * Kept for callers with a single viewport position; scroll tracking uses
 * reading_focus_track(). */
gint reading_focus_pick(const ReadingFocusBlock *blocks, guint n_blocks,
			gdouble viewport_height, gint current_id,
			gint direction);

typedef struct {
	gint picked;	/* verse to focus now */
	gint candidate;	/* verse under (or nearest to) the reading line, 0 if none */
	gint crossed;	/* verse starts (down) / ends (up) the line went past */
	gint direction;	/* +1 down, -1 up, 0 still: the line's own movement */
} ReadingFocusStep;

/* One scroll update. previous_line_y is where the reading line was at the
 * previous update, in the current viewport's coordinates, so the movement
 * since then is known however many scroll events a frame coalesced; pass
 * the current line position when there is no previous update. blocks
 * must cover the viewport and the stretch between the two line positions.
 *
 * The direction is the line's movement, never a guess from scroll events:
 * layout settling or a range change moving the view a pixel back is just
 * that pixel. The verse under the line is the answer, except:
 *   - the focused verse lies ahead of the line in the direction it moves
 *     (the arrows left it lower in the viewport, say): kept until the line
 *     reaches it;
 *   - the line, moving less than the hysteresis distance since the last
 *     update, is past the boundary with the adjacent verse by less than
 *     that distance: kept (wobbling on a boundary). A line moving further
 *     is not wobbling and takes the adjacent verse as soon as it crosses
 *     it; a candidate further away than the adjacent verse is taken at
 *     once: the line really went past all of them since the last update.
 * push_direction is non-zero when a wheel step was made against the top
 * (-1) or bottom (+1) of the pane, where the viewport cannot move: the
 * focus then moves to the next visible verse that way. */
ReadingFocusStep reading_focus_track(const ReadingFocusBlock *blocks,
				     guint n_blocks, gdouble viewport_height,
				     gint current_id, gdouble previous_line_y,
				     gint push_direction);

/* Explicit navigation -> scroll handoff.
 *
 * A navigation (picker, typed reference, arrows) decides the focused verse
 * and leaves it wherever the reading zone put it -- often not under the
 * reading line, which lies over an earlier or later verse. The first scroll
 * after it must start from the verse the reader asked for, not from the
 * line's geometry: scrolling down never goes back before it, scrolling up
 * never goes past it.
 *
 * So tracking uses a reading line shifted by an offset. The offset starts
 * by putting the line inside the navigated verse (rebase) and shrinks by
 * however far the view scrolls (decay), whichever way: towards the verse,
 * the shifted line stays on it in the document until the real line gets
 * there; away from it, it moves at twice the scroll and meets the real line
 * after going through the verses in between, one by one. A still update
 * changes nothing. */

/* Offset (px) that puts the reading line inside a verse spanning
 * [top, bottom) in viewport coordinates: 0 when the line already is, else
 * a point a little below the verse's top (twice the hysteresis distance,
 * at most half the verse). */
gdouble reading_focus_rebase_line_offset(gdouble top, gdouble bottom,
					 gdouble viewport_height);

/* The offset after the real reading line moved `line_moved` px (the scroll
 * since the last update, either sign): closer to 0 by that much, never
 * past it. */
gdouble reading_focus_line_offset_decay(gdouble offset, gdouble line_moved);

/* reading_focus_track() with the reading line at
 * viewport_height * READING_FOCUS_LINE_RATIO + line_offset;
 * previous_line_y is that shifted line at the previous update. While
 * line_offset is not 0 the line is not wobbling but being carried to the
 * reading line, so the boundary hysteresis does not hold verses back (a
 * short verse would be skipped). With line_offset 0 this is exactly
 * reading_focus_track(). */
ReadingFocusStep reading_focus_track_offset(const ReadingFocusBlock *blocks,
					    guint n_blocks,
					    gdouble viewport_height,
					    gint current_id,
					    gdouble previous_line_y,
					    gint push_direction,
					    gdouble line_offset);

/* Empty space, in whole pixels, to leave below the pane's content so its
 * last verse can be scrolled up past the reading line (by twice the
 * hysteresis distance, so the verse before it lets go). content_bottom is
 * where scrolling ends without any reserve and last_verse_top where the
 * last verse starts, in the same layout coordinates. Integer inputs,
 * rounded once inside: the same geometry always gives the same reserve.
 * 0 when the content after the last verse is already long enough; never
 * more than about three quarters of the viewport. */
gint reading_focus_bottom_reserve(gint viewport_height, gint content_bottom,
				  gint last_verse_top);

/* Pixels to add to the scroll position so a verse spanning [top, bottom)
 * (viewport coordinates) sits inside the reading zone. 0 when it already
 * does. A verse taller than the zone is placed with its top at the top of
 * the zone. */
gdouble reading_zone_scroll_delta(gdouble top, gdouble bottom,
				  gdouble viewport_height);

G_END_DECLS

#endif /* XIPHOS_READING_FOCUS_H */
