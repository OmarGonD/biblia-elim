/*
 * reading_focus.c - which verse the reader is on, decided from geometry
 */

#include "main/reading_focus.h"

gboolean
reading_focus_id_is_verse(gint id)
{
	return id / 1000 >= 1 && id % 1000 >= 1;
}

static gboolean
usable_block(const ReadingFocusBlock *b)
{
	return reading_focus_id_is_verse(b->id) && b->bottom > b->top;
}

/* Index of the verse under the line, or nearest to it; -1 if none. */
static gint
candidate_index(const ReadingFocusBlock *blocks, guint n_blocks, gdouble line)
{
	gdouble best = G_MAXDOUBLE;
	gint candidate = -1;
	guint i;

	for (i = 0; i < n_blocks; i++) {
		const ReadingFocusBlock *b = &blocks[i];
		gdouble distance;

		if (!usable_block(b))
			continue;
		if (line >= b->top && line < b->bottom)
			distance = 0;
		else if (line < b->top)
			distance = b->top - line;
		else
			distance = line - b->bottom;
		if (distance < best) {
			best = distance;
			candidate = (gint)i;
		}
	}
	return candidate;
}

/* The verse next to index `from` in document order (step +1 / -1). */
static gint
neighbour_index(const ReadingFocusBlock *blocks, guint n_blocks, gint from,
		gint step)
{
	gint i;

	for (i = from + step; i >= 0 && i < (gint)n_blocks; i += step)
		if (usable_block(&blocks[i]))
			return i;
	return -1;
}

gint
reading_focus_pick(const ReadingFocusBlock *blocks, guint n_blocks,
		   gdouble viewport_height, gint current_id, gint direction)
{
	gdouble line, slack;
	gint candidate, current = -1;
	guint i;

	if (!blocks || n_blocks == 0 || viewport_height <= 0)
		return current_id;
	line = viewport_height * READING_FOCUS_LINE_RATIO;
	slack = viewport_height * READING_FOCUS_HYSTERESIS_RATIO;

	for (i = 0; i < n_blocks; i++)
		if (usable_block(&blocks[i]) && blocks[i].id == current_id)
			current = (gint)i;
	candidate = candidate_index(blocks, n_blocks, line);

	if (candidate < 0)
		return current_id;
	if (current < 0 || blocks[current].bottom <= 0 ||
	    blocks[current].top >= viewport_height)
		return blocks[candidate].id;
	if (candidate == current)
		return current_id;
	if ((direction > 0 && candidate < current) ||
	    (direction < 0 && candidate > current))
		return current_id;
	if (blocks[current].bottom >= line - slack &&
	    blocks[current].top <= line + slack)
		return current_id;
	return blocks[candidate].id;
}

ReadingFocusStep
reading_focus_track(const ReadingFocusBlock *blocks, guint n_blocks,
		    gdouble viewport_height, gint current_id,
		    gdouble previous_line_y, gint push_direction)
{
	return reading_focus_track_offset(blocks, n_blocks, viewport_height,
					  current_id, previous_line_y,
					  push_direction, 0.0);
}

gdouble
reading_focus_rebase_line_offset(gdouble top, gdouble bottom,
				 gdouble viewport_height)
{
	gdouble line, slack;

	if (viewport_height <= 0 || bottom <= top)
		return 0.0;
	line = viewport_height * READING_FOCUS_LINE_RATIO;
	if (line >= top && line < bottom)
		return 0.0;
	slack = viewport_height * READING_FOCUS_HYSTERESIS_RATIO;
	return top + MIN(2.0 * slack, (bottom - top) / 2.0) - line;
}

gdouble
reading_focus_line_offset_decay(gdouble offset, gdouble line_moved)
{
	gdouble distance = ABS(line_moved);

	if (offset > 0.0)
		return MAX(0.0, offset - distance);
	if (offset < 0.0)
		return MIN(0.0, offset + distance);
	return 0.0;
}

ReadingFocusStep
reading_focus_track_offset(const ReadingFocusBlock *blocks, guint n_blocks,
			   gdouble viewport_height, gint current_id,
			   gdouble previous_line_y, gint push_direction,
			   gdouble line_offset)
{
	ReadingFocusStep step = { current_id, 0, 0, 0 };
	gdouble line, slack, moved;
	gint candidate, current = -1, neighbour;
	guint i;

	if (!blocks || n_blocks == 0 || viewport_height <= 0)
		return step;
	line = viewport_height * READING_FOCUS_LINE_RATIO + line_offset;
	slack = viewport_height * READING_FOCUS_HYSTERESIS_RATIO;
	moved = line - previous_line_y;
	step.direction = moved > 0.5 ? 1 : (moved < -0.5 ? -1 : 0);
	if (push_direction)
		step.direction = push_direction > 0 ? 1 : -1;

	for (i = 0; i < n_blocks; i++) {
		const ReadingFocusBlock *b = &blocks[i];

		if (!usable_block(b))
			continue;
		if (b->id == current_id)
			current = (gint)i;
		if (moved > 0.5 && b->top > previous_line_y && b->top <= line)
			step.crossed++;
		else if (moved < -0.5 && b->bottom < previous_line_y &&
			 b->bottom >= line)
			step.crossed++;
	}

	candidate = candidate_index(blocks, n_blocks, line);
	if (candidate < 0)
		return step;
	step.candidate = blocks[candidate].id;

	if (current < 0 || blocks[current].bottom <= 0 ||
	    blocks[current].top >= viewport_height) {
		step.picked = step.candidate;
		return step;
	}

	/* Hysteresis is for a line wobbling on a boundary: movements smaller
	 * than the hysteresis distance. A line moving further than that in one
	 * update is not wobbling, and holding the adjacent verse back then
	 * would only let the next update skip it. Nor is a line still handing
	 * off from a navigation (line_offset != 0): it is being carried towards
	 * the real reading line, faster than the scroll, and holding a short
	 * verse back then skips it just the same. */
	if (candidate > current) {
		/* the line is below the focused verse */
		neighbour = neighbour_index(blocks, n_blocks, current, 1);
		if (step.direction < 0)
			; /* moving up towards the focused verse */
		else if (line_offset == 0.0 && candidate == neighbour &&
			 ABS(moved) < slack &&
			 line < blocks[candidate].top + slack)
			; /* wobbling just over that boundary */
		else
			step.picked = step.candidate;
	} else if (candidate < current) {
		/* the line is above the focused verse */
		neighbour = neighbour_index(blocks, n_blocks, current, -1);
		if (step.direction > 0)
			; /* moving down towards the focused verse */
		else if (line_offset == 0.0 && candidate == neighbour &&
			 ABS(moved) < slack &&
			 line > blocks[candidate].bottom - slack)
			;
		else
			step.picked = step.candidate;
	}

	if (push_direction && step.picked == current_id) {
		neighbour = neighbour_index(blocks, n_blocks, current,
					    push_direction > 0 ? 1 : -1);
		if (neighbour >= 0 && blocks[neighbour].bottom > 0 &&
		    blocks[neighbour].top < viewport_height)
			step.picked = blocks[neighbour].id;
	}
	return step;
}

gint
reading_focus_bottom_reserve(gint viewport_height, gint content_bottom,
			     gint last_verse_top)
{
	gint room, tail;

	if (viewport_height <= 0 || content_bottom < last_verse_top)
		return 0;
	/* At the bottom, the last verse starts this far from the viewport
	 * bottom: above the reading line by twice the hysteresis distance,
	 * so the line is inside it and the verse before it has let go.
	 * Rounded here, once. */
	room = (gint)(viewport_height *
			      (1.0 - READING_FOCUS_LINE_RATIO +
			       2.0 * READING_FOCUS_HYSTERESIS_RATIO) +
		      0.5);
	tail = content_bottom - last_verse_top;
	return room > tail ? room - tail : 0;
}

gdouble
reading_zone_scroll_delta(gdouble top, gdouble bottom, gdouble viewport_height)
{
	gdouble zone_top, zone_bottom, delta;

	if (viewport_height <= 0 || bottom < top)
		return 0;
	zone_top = viewport_height * READING_ZONE_TOP_RATIO;
	zone_bottom = viewport_height * READING_ZONE_BOTTOM_RATIO;
	if (top < zone_top)
		return top - zone_top;
	if (bottom <= zone_bottom)
		return 0;
	delta = bottom - zone_bottom;
	if (top - delta < zone_top)
		delta = top - zone_top;
	return delta;
}
