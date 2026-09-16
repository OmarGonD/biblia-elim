#include "main/reading_focus.h"

#include <stdio.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

#define N(a) (sizeof(a) / sizeof((a)[0]))

/* Viewport of 1000 px: reading line at 330, hysteresis 30 px, reading
 * zone 200..750. */
#define H 1000.0

static void
test_ids(void)
{
	CHECK(reading_focus_id_is_verse(3001));
	CHECK(reading_focus_id_is_verse(150006));
	CHECK(!reading_focus_id_is_verse(3000)); /* chapter heading / intro */
	CHECK(!reading_focus_id_is_verse(0));	 /* inert marker, preview */
	CHECK(!reading_focus_id_is_verse(12));	 /* no chapter */
}

/* ---- snapshot rule ------------------------------------------------- */

/* A. The focused verse is still under the reading line: keep it. */
static void
test_keeps_current_in_reading_zone(void)
{
	const ReadingFocusBlock blocks[] = {
		{ 1011, 100, 300 },
		{ 1012, 300, 420 },
		{ 1013, 420, 600 },
	};
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, 1) == 1012);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, -1) == 1012);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, 0) == 1012);
}

/* B. The next verse clearly crosses the line: move to it. */
static void
test_switches_when_next_verse_crosses_line(void)
{
	const ReadingFocusBlock blocks[] = {
		{ 1011, 60, 200 },
		{ 1012, 200, 280 }, /* ended 50 px above the line */
		{ 1013, 280, 500 },
	};
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, 1) == 1013);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 0, 0) == 1013);
}

/* D. A tall verse covering most of the viewport. */
static void
test_tall_verse(void)
{
	const ReadingFocusBlock inside[] = {
		{ 119175, -900, 700 },
		{ 119176, 700, 800 },
	};
	const ReadingFocusBlock past[] = {
		{ 119175, -1400, 200 },
		{ 119176, 200, 1300 },
	};
	CHECK(reading_focus_pick(inside, N(inside), H, 119174, 1) == 119175);
	CHECK(reading_focus_pick(inside, N(inside), H, 119175, 1) == 119175);
	CHECK(reading_focus_pick(past, N(past), H, 119175, 1) == 119176);
}

/* E. A native psalm-title slot is a verse like any other. */
static void
test_title_slot_can_be_focused(void)
{
	const ReadingFocusBlock blocks[] = {
		{ 2013, 0, 120 },
		{ 3001, 280, 400 }, /* "Salmo de David..." */
		{ 3002, 400, 520 },
	};
	CHECK(reading_focus_pick(blocks, N(blocks), H, 2013, 1) == 3001);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 3002, -1) == 3001);
}

/* F. Headings, chapter anchors and previews never take the focus. */
static void
test_headings_ignored(void)
{
	const ReadingFocusBlock heading_under_line[] = {
		{ 2013, 100, 250 },
		{ 3000, 250, 400 }, /* "Salmo 3" heading under the line */
		{ 3001, 400, 480 },
	};
	const ReadingFocusBlock only_markers[] = {
		{ 0, 0, 500 },
		{ 3000, 500, 900 },
	};
	CHECK(reading_focus_pick(heading_under_line, N(heading_under_line), H,
				 2013, 1) == 3001);
	CHECK(reading_focus_pick(only_markers, N(only_markers), H, 2013, 1) ==
	      2013);
	{
		ReadingFocusStep s = reading_focus_track(only_markers,
			N(only_markers), H, 2013, 300, 0);
		CHECK(s.picked == 2013);
		CHECK(s.candidate == 0);
	}
}

static void
test_current_out_of_view(void)
{
	const ReadingFocusBlock blocks[] = {
		{ 1020, 200, 400 },
		{ 1021, 400, 600 },
	};
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1003, 1) == 1020);
	CHECK(reading_focus_track(blocks, N(blocks), H, 1003, 100, 0).picked ==
	      1020);
	CHECK(reading_focus_track(blocks, N(blocks), H, 1040, 900, 0).picked ==
	      1020);
	CHECK(reading_focus_track(NULL, 0, H, 1003, 100, 0).picked == 1003);
}

/* ---- tracking a moving line ---------------------------------------- */

/* A chapter laid out in document coordinates, scrolled by moving a
 * viewport over it. */
typedef struct {
	ReadingFocusBlock *doc;
	gint n;
	gdouble content_bottom;
} Layout;

static Layout
layout_chapter(gint chapter, const gint *heights, gint n, gdouble head,
	       gdouble tail, gdouble scale)
{
	Layout l;
	gdouble y = head * scale;
	gint i;

	l.doc = g_new(ReadingFocusBlock, n);
	l.n = n;
	for (i = 0; i < n; i++) {
		l.doc[i].id = chapter * 1000 + i + 1;
		l.doc[i].top = y;
		l.doc[i].bottom = y + heights[i] * scale;
		y = l.doc[i].bottom + 4 * scale;
	}
	l.content_bottom = l.doc[n - 1].bottom + tail * scale;
	return l;
}

/* What the pane hands reading_focus_track() at scroll position `value`:
 * the verses overlapping the viewport and the stretch the line covered
 * since the previous position. */
static ReadingFocusStep
track_at(const Layout *l, gdouble value, gdouble previous_value, gdouble h,
	 gint current, gint push)
{
	ReadingFocusBlock *vis = g_new(ReadingFocusBlock, l->n);
	gdouble line = h * READING_FOCUS_LINE_RATIO;
	gdouble lo = MIN(value, previous_value + line) ;
	gdouble hi = MAX(value + h, previous_value + line + 1);
	ReadingFocusStep s;
	guint nv = 0;
	gint i;

	for (i = 0; i < l->n; i++) {
		if (l->doc[i].bottom <= lo || l->doc[i].top >= hi)
			continue;
		vis[nv].id = l->doc[i].id;
		vis[nv].top = l->doc[i].top - value;
		vis[nv].bottom = l->doc[i].bottom - value;
		nv++;
	}
	s = reading_focus_track(vis, nv, h, current,
				previous_value + line - value, push);
	g_free(vis);
	return s;
}

/* Matthew 16:20-28 as laid out by the real pane (zoom 100). */
static const gint matthew16_heights[] = { 16, 34, 34, 34, 16, 34, 16, 16,
	34, 16, 34, 34, 34, 34, 16, 34, 52, 34, 52, 34, 52, 34, 34, 34, 34,
	34, 34, 34 };

/* Scroll position that puts the top of verse `v` on the reading line. */
static gdouble
value_with_verse_on_line(const Layout *l, gint index, gdouble h)
{
	return l->doc[index].top - h * READING_FOCUS_LINE_RATIO;
}

/* Slow wheel: the line moves a few pixels per update. Every verse it
 * reaches takes the focus in turn -- 24, 25, 26, 27, 28 -- never a freeze
 * followed by a jump. */
static void
test_slow_scroll_visits_each_verse(void)
{
	const gdouble h = 363; /* viewport of the reported session */
	Layout l = layout_chapter(16, matthew16_heights,
				  N(matthew16_heights), 71, 120, 1.0);
	gint reserve = reading_focus_bottom_reserve((gint)h,
		(gint)l.content_bottom, (gint)l.doc[27].top);
	gdouble max = l.content_bottom + reserve - h;
	gdouble value = value_with_verse_on_line(&l, 23, h) + 2, previous;
	gint focus = 16024, visited[8], nvisited = 0, i;

	for (previous = value;; previous = value) {
		ReadingFocusStep s;
		value = MIN(max, value + 3);
		s = track_at(&l, value, previous, h, focus, 0);
		CHECK(s.crossed <= 1);
		if (s.picked != focus) {
			CHECK(s.picked == focus + 1); /* one verse at a time */
			focus = s.picked;
			if (nvisited < (gint)N(visited))
				visited[nvisited++] = focus;
		}
		if (value >= max)
			break;
	}
	CHECK(nvisited == 4);
	for (i = 0; i < nvisited && i < 4; i++)
		CHECK(visited[i] == 16025 + i);
	CHECK(focus == 16028);

	/* and back up, just as slowly: 27, 26, 25, 24 */
	nvisited = 0;
	{
		gdouble stop = value_with_verse_on_line(&l, 23, h) - 20;
		for (previous = value;; previous = value) {
			ReadingFocusStep s;
			value = MAX(stop, value - 3);
			s = track_at(&l, value, previous, h, focus, 0);
			if (s.picked != focus) {
				CHECK(s.picked == focus - 1);
				focus = s.picked;
				if (nvisited < (gint)N(visited))
					visited[nvisited++] = focus;
			}
			if (value <= stop)
				break;
		}
	}
	CHECK(nvisited >= 4);
	for (i = 0; i < 4 && i < nvisited; i++)
		CHECK(visited[i] == 16027 - i);
	g_free(l.doc);
}

/* The mechanism behind the reported 24 -> 28: at the bottom, each range
 * change moved the view a pixel back, and a direction taken from the last
 * scroll event read that as scrolling up and held the focus. The line's
 * own movement between updates is what counts now. */
static void
test_pixel_back_steps_do_not_hold_the_focus(void)
{
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights,
				  N(matthew16_heights), 71, 120, 1.0);
	gint reserve = reading_focus_bottom_reserve((gint)h,
		(gint)l.content_bottom, (gint)l.doc[27].top);
	gdouble max = l.content_bottom + reserve - h;
	gdouble value = value_with_verse_on_line(&l, 23, h) + 2, previous;
	gint focus = 16024, n;

	for (n = 0, previous = value; value < max; n++, previous = value) {
		ReadingFocusStep s;
		/* +6 then -1, as a scroll interleaved with clamps */
		value = MIN(max, value + ((n % 2) ? -1 : 6));
		s = track_at(&l, value, previous, h, focus, 0);
		if (s.picked != focus) {
			CHECK(s.picked == focus + 1);
			focus = s.picked;
		}
	}
	CHECK(focus == 16028);
	g_free(l.doc);
}

/* Medium steps, smaller than a verse but larger than the hysteresis: the
 * adjacent verse is never held back only to be skipped by the next update
 * (hold on 24 while the line enters 25, then jump to 26). */
static void
test_steps_below_verse_height_never_skip(void)
{
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights,
				  N(matthew16_heights), 71, 120, 1.0);
	gint reserve = reading_focus_bottom_reserve((gint)h,
		(gint)l.content_bottom, (gint)l.doc[27].top);
	gdouble max = l.content_bottom + reserve - h;
	gdouble start = value_with_verse_on_line(&l, 23, h) + 2;
	gdouble step_px;

	/* 34 px verses, 4 px apart: any step under 38 px crosses at most one */
	for (step_px = 12; step_px <= 37; step_px += 5) {
		gdouble value = start, previous;
		gint focus = 16024;
		for (previous = value;; previous = value) {
			ReadingFocusStep s;
			value = MIN(max, value + step_px);
			s = track_at(&l, value, previous, h, focus, 0);
			if (s.picked != focus) {
				if (s.picked != focus + 1)
					fprintf(stderr, "  step %.0f: %d -> %d\n",
						step_px, focus, s.picked);
				CHECK(s.picked == focus + 1);
				focus = s.picked;
			}
			if (value >= max)
				break;
		}
		CHECK(focus == 16028);
	}
	g_free(l.doc);
}

/* Fast gesture: one frame covering four verses may land four verses on. */
static void
test_fast_scroll_may_cross_several(void)
{
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights,
				  N(matthew16_heights), 71, 120, 1.0);
	gdouble from = value_with_verse_on_line(&l, 23, h) + 2;
	gdouble to = value_with_verse_on_line(&l, 27, h) + 5;
	ReadingFocusStep s = track_at(&l, to, from, h, 16024, 0);

	CHECK(s.direction == 1);
	CHECK(s.crossed == 4);
	CHECK(s.candidate == 16028);
	CHECK(s.picked == 16028);
	/* the same distance up */
	s = track_at(&l, from, to, h, 16028, 0);
	CHECK(s.direction == -1);
	CHECK(s.picked == 16024);
	g_free(l.doc);
}

/* Hysteresis acts only on the boundary between the focused verse and its
 * neighbour: wobbling a few pixels across it changes nothing, and it never
 * holds back a verse further away. */
static void
test_hysteresis_only_at_adjacent_boundary(void)
{
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights,
				  N(matthew16_heights), 71, 120, 1.0);
	gdouble boundary = value_with_verse_on_line(&l, 24, h); /* 25 on line */
	gdouble value = boundary - 2, previous = value;
	gint focus = 16024, n;

	for (n = 0; n < 40; n++) {
		ReadingFocusStep s;
		previous = value;
		value = boundary + ((n % 2) ? 4 : -3);
		s = track_at(&l, value, previous, h, focus, 0);
		focus = s.picked;
		CHECK(focus == 16024);
	}
	/* 3 px past the boundary: still 24; past it by the hysteresis: 25 */
	CHECK(track_at(&l, boundary + 3, boundary - 1, h, 16024, 0).picked == 16024);
	CHECK(track_at(&l, boundary + 12, boundary + 3, h, 16024, 0).picked == 16025);
	/* a verse two away is not held by 24's boundary */
	CHECK(track_at(&l, value_with_verse_on_line(&l, 25, h) + 1, boundary + 3,
		       h, 16024, 0).picked == 16026);
	g_free(l.doc);
}

/* A verse the arrows left low in the viewport keeps the focus while the
 * wheel brings it up to the line; the opposite way, the line's verse. */
static void
test_focus_ahead_of_line(void)
{
	const ReadingFocusBlock blocks[] = {
		{ 1010, 250, 400 },
		{ 1011, 400, 550 },
		{ 1012, 550, 700 }, /* focused by the arrows */
	};
	CHECK(reading_focus_track(blocks, N(blocks), H, 1012, 320, 0).picked ==
	      1012);
	CHECK(reading_focus_track(blocks, N(blocks), H, 1012, 340, 0).picked ==
	      1010);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, 1) == 1012);
	CHECK(reading_focus_pick(blocks, N(blocks), H, 1012, -1) == 1010);
}

/* ---- explicit navigation -> scroll handoff ------------------------- */

/* A chapter with the real pane's verse heights (Matthew 16 at zoom 100:
 * one-line verses of 16 px, longer ones of 34-52 px), starting at doc y
 * `start`. */
static Layout
layout_real(gint chapter, gdouble start)
{
	Layout l = layout_chapter(chapter, matthew16_heights,
				  N(matthew16_heights), 0, 400, 1.0);
	gint i;

	for (i = 0; i < l.n; i++) {
		l.doc[i].top += start;
		l.doc[i].bottom += start;
	}
	l.content_bottom += start;
	return l;
}

static gint
layout_index(const Layout *l, gint id)
{
	gint i;
	for (i = 0; i < l->n; i++)
		if (l->doc[i].id == id)
			return i;
	return -1;
}

/* track_at() with the shifted line of the handoff */
static ReadingFocusStep
track_offset_at(const Layout *l, gdouble value, gdouble previous_value,
		gdouble h, gint current, gdouble previous_offset, gdouble offset)
{
	ReadingFocusBlock *vis = g_new(ReadingFocusBlock, l->n);
	gdouble line = h * READING_FOCUS_LINE_RATIO;
	gdouble prev_doc = previous_value + line + previous_offset;
	gdouble lo = MIN(value, prev_doc) - 1;
	gdouble hi = MAX(value + h, prev_doc + 2);
	ReadingFocusStep s;
	guint nv = 0;
	gint i;

	for (i = 0; i < l->n; i++) {
		if (l->doc[i].bottom <= lo || l->doc[i].top >= hi)
			continue;
		vis[nv].id = l->doc[i].id;
		vis[nv].top = l->doc[i].top - value;
		vis[nv].bottom = l->doc[i].bottom - value;
		nv++;
	}
	s = reading_focus_track_offset(vis, nv, h, current, prev_doc - value, 0,
				       offset);
	g_free(vis);
	return s;
}

/* The pane after an explicit navigation to `target`, the view placed so
 * the verse starts at `fraction` of the viewport, then a slow wheel in
 * `direction` with still updates in between (a glide's first frame moves
 * 0.1 px; its last does not move). Checks the handoff contract and
 * returns the verses the focus went through, in order. */
static gint
handoff_run(const Layout *l, gint target, gdouble fraction, gint direction,
	    gdouble distance, gint *visited, gint max_visited)
{
	static const gdouble pattern[] = { 0.1, 0.0, 4.3, 4.2, 7.7, 3.3, 0.0,
					   2.8, 4.0, 1.0, 0.4, 0.0 };
	const gdouble h = 363;
	gint index = layout_index(l, target);
	gdouble value = l->doc[index].top - fraction * h, start = value;
	gdouble offset = reading_focus_rebase_line_offset(
	    l->doc[index].top - value, l->doc[index].bottom - value, h);
	gint focus = target, n = 0, k = 0;

	while (ABS(value - start) < distance) {
		gdouble previous = value, previous_offset = offset;
		ReadingFocusStep s;

		value += direction * pattern[k++ % N(pattern)];
		offset = reading_focus_line_offset_decay(offset, value - previous);
		s = track_offset_at(l, value, previous, h, focus, previous_offset,
				    offset);
		if (direction > 0)
			CHECK(s.picked >= target); /* never back before it */
		else
			CHECK(s.picked <= target); /* never on past it */
		if (s.picked != focus) {
			gint from = layout_index(l, focus);
			gint to = layout_index(l, s.picked);
			/* one verse at a time, the way the wheel goes, while the
			 * handoff shift is in play; once it is gone this is the
			 * ordinary tracker (test_steps_below_verse_height_never_skip
			 * and friends) */
			if (previous_offset != 0.0) {
				if (to != from + direction)
					fprintf(stderr, "  handoff %d at %.0f%%: %d -> %d\n",
						target, fraction * 100, focus, s.picked);
				CHECK(to == from + direction);
			} else {
				CHECK(to * direction > from * direction);
			}
			focus = s.picked;
			if (n < max_visited)
				visited[n++] = focus;
		}
	}
	return n;
}

/* The reported bug, in the old tracker: Matthew 18:12 navigated to and
 * placed at half the viewport, 18:8 under the reading line. A still update
 * (no movement, or a glide's 0.1 px first frame) took 18:8. */
static void
test_still_update_after_navigation_used_to_retreat(void)
{
	const gdouble h = 363;
	Layout l = layout_real(18, 100);
	gint i12 = layout_index(&l, 18012);
	gdouble value = l.doc[i12].top - 0.5 * h;
	ReadingFocusStep s = track_at(&l, value, value, h, 18012, 0);

	CHECK(s.candidate < 18012);
	CHECK(s.picked < 18012); /* what the handoff fixes */
	/* with the handoff offset, the same still update keeps 18:12 */
	{
		gdouble offset = reading_focus_rebase_line_offset(
		    l.doc[i12].top - value, l.doc[i12].bottom - value, h);
		s = track_offset_at(&l, value, value, h, 18012, offset, offset);
		CHECK(offset > 0);
		CHECK(s.candidate == 18012);
		CHECK(s.picked == 18012);
	}
	g_free(l.doc);
}

static void
test_rebase_offset(void)
{
	const gdouble h = 363, line = 363 * READING_FOCUS_LINE_RATIO;

	/* the line already inside the verse: nothing to shift */
	CHECK(reading_focus_rebase_line_offset(line - 5, line + 15, h) == 0);
	/* below the line: the shifted line lands inside the verse */
	{
		gdouble o = reading_focus_rebase_line_offset(180, 200, h);
		CHECK(line + o > 180 && line + o < 200);
	}
	/* above it */
	{
		gdouble o = reading_focus_rebase_line_offset(60, 80, h);
		CHECK(o < 0);
		CHECK(line + o > 60 && line + o < 80);
	}
	CHECK(reading_focus_rebase_line_offset(10, 10, h) == 0);
	CHECK(reading_focus_rebase_line_offset(10, 30, 0) == 0);

	CHECK(reading_focus_line_offset_decay(50, 20) == 30);
	CHECK(reading_focus_line_offset_decay(50, -20) == 30);
	CHECK(reading_focus_line_offset_decay(50, 80) == 0);
	CHECK(reading_focus_line_offset_decay(-50, 20) == -30);
	CHECK(reading_focus_line_offset_decay(-50, -80) == 0);
	CHECK(reading_focus_line_offset_decay(0, 12) == 0);
	CHECK(reading_focus_line_offset_decay(50, 0) == 50);
}

/* Matthew 18:6 -> picker 18:12, the verse left at 25, 50 or 70 % of the
 * viewport: down goes 12, 13, 14..., up goes 12, 11, 10..., never the other
 * way, whatever lies under the reading line. */
static void
test_handoff_after_explicit_navigation(void)
{
	static const gdouble fractions[] = { 0.25, 0.50, 0.70 };
	Layout l = layout_real(18, 100);
	guint f;

	for (f = 0; f < N(fractions); f++) {
		gint visited[16], n;

		/* from 70 % the line first has to get down to the verse */
		n = handoff_run(&l, 18012, fractions[f], 1, 400, visited,
				N(visited));
		CHECK(n >= 3);
		CHECK(n >= 1 && visited[0] == 18013);
		CHECK(n >= 2 && visited[1] == 18014);

		n = handoff_run(&l, 18012, fractions[f], -1, 400, visited,
				N(visited));
		CHECK(n >= 3);
		CHECK(n >= 1 && visited[0] == 18011);
		/* in order after that; which verses in turn is the ordinary
		 * tracker's business (a 16 px verse can go by in one update of
		 * a wide boundary), not the handoff's */
		{
			gint i;
			for (i = 1; i < n; i++)
				CHECK(visited[i] < visited[i - 1]);
		}
	}
	g_free(l.doc);
}

/* The session that showed 12 -> 10: TorresAmat Matthew 18 in a 414 px
 * viewport, 37 px verses except 18:11, a short one-liner (18 px), 18:12
 * left at 66 % of the viewport by the picker, then the wheel up with the
 * glide's frame steps from the log. 11 must not be skipped. */
static void
test_handoff_up_does_not_skip_short_verse(void)
{
	static const gdouble steps[] = { 4.3, 4.2, 4.0, 3.7, 6.1, 3.3, 2.8, 2.3,
					 1.7, 1.0, 0.4, 0.0 };
	const gdouble h = 414;
	Layout l;
	gint i, focus = 18012, visited[8], n = 0, k;
	gdouble y = 100, value, offset;

	l.n = 20;
	l.doc = g_new(ReadingFocusBlock, l.n);
	for (i = 0; i < l.n; i++) {
		gdouble height = (i + 1 == 11) ? 18 : 37;
		l.doc[i].id = 18001 + i;
		l.doc[i].top = y;
		l.doc[i].bottom = y + height;
		y = l.doc[i].bottom + 4;
	}
	l.content_bottom = y + 400;
	value = l.doc[11].top - 0.66 * h;
	offset = reading_focus_rebase_line_offset(l.doc[11].top - value,
						  l.doc[11].bottom - value, h);
	CHECK(offset > 100);
	/* long enough for the stable mode's wider boundary too */
	for (k = 0; k < 8 * (gint)N(steps); k++) {
		gdouble previous = value, previous_offset = offset;
		ReadingFocusStep s;

		value -= steps[k % N(steps)];
		offset = reading_focus_line_offset_decay(offset, value - previous);
		s = track_offset_at(&l, value, previous, h, focus, previous_offset,
				    offset);
		CHECK(s.picked <= 18012);
		if (s.picked != focus) {
			if (s.picked != focus - 1)
				fprintf(stderr, "  short verse: %d -> %d\n", focus,
					s.picked);
			CHECK(s.picked == focus - 1);
			focus = s.picked;
			if (n < (gint)N(visited))
				visited[n++] = focus;
		}
	}
	CHECK(n >= 2);
	CHECK(n >= 1 && visited[0] == 18011);
	CHECK(n >= 2 && visited[1] == 18010);
	g_free(l.doc);
}

/* Chapter picker to Matthew 18:1 with chapter 17 laid out above it: the
 * reading line may still be over 17, but down never goes back into 17, and
 * up leaves 18:1 for 17's last verse first. */
static void
test_handoff_across_chapters(void)
{
	const gdouble h = 363;
	Layout l17 = layout_real(17, 100);
	Layout l18 = layout_real(18, l17.doc[27].bottom + 40);
	Layout l;
	gint visited[16], n, i;
	static const gdouble fractions[] = { 0.40, 0.50, 0.70 };
	guint f;

	(void)h;
	l.n = l17.n + l18.n;
	l.doc = g_new(ReadingFocusBlock, l.n);
	for (i = 0; i < l17.n; i++)
		l.doc[i] = l17.doc[i];
	for (i = 0; i < l18.n; i++)
		l.doc[l17.n + i] = l18.doc[i];
	l.content_bottom = l18.content_bottom;

	for (f = 0; f < N(fractions); f++) {
		n = handoff_run(&l, 18001, fractions[f], 1, 400, visited, N(visited));
		CHECK(n >= 2);
		for (i = 0; i < n; i++)
			CHECK(visited[i] / 1000 == 18);
		CHECK(n >= 1 && visited[0] == 18002);

		n = handoff_run(&l, 18001, fractions[f], -1, 150, visited, N(visited));
		CHECK(n >= 1 && visited[0] == 17028);
	}
	g_free(l.doc);
	g_free(l17.doc);
	g_free(l18.doc);
}

/* ---- reader's focus modes (Ver > Navegación y rueda) --------------- */

/* How far past the boundary with the next verse (direction +1) or the
 * previous one (-1) the reading line has to go, scrolling 1 px per update,
 * before the focus moves to it. */
static gdouble
switch_distance(ReadingFocusMode mode, gint direction)
{
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights, N(matthew16_heights),
				  71, 120, 1.0);
	/* 16:10 (34 px) focused, boundary with 16:11 below / 16:9 above */
	gint from = 9, to = from + direction;
	gdouble boundary = direction > 0
		? value_with_verse_on_line(&l, to, h)
		: l.doc[to].bottom - h * READING_FOCUS_LINE_RATIO;
	gdouble value = direction > 0 ? boundary - 5 : boundary + 5, previous;
	gint focus = l.doc[from].id, k;
	gdouble distance = -1;

	reading_focus_set_mode(mode);
	for (k = 0; k < 200; k++) {
		ReadingFocusStep s;
		previous = value;
		value += direction;
		s = track_at(&l, value, previous, h, focus, 0);
		CHECK(s.picked == focus || s.picked == l.doc[to].id);
		if (s.picked != focus) {
			distance = (value - boundary) * direction;
			break;
		}
	}
	reading_focus_set_mode(READING_FOCUS_BALANCED);
	g_free(l.doc);
	return distance;
}

static void
test_focus_modes(void)
{
	const gdouble h = 363;
	gdouble imm_down, bal_down, stab_down, imm_up, bal_up, stab_up;

	/* the default is the behaviour everything above was written for */
	CHECK(reading_focus_get_mode() == READING_FOCUS_BALANCED);
	CHECK(reading_focus_hysteresis_ratio(READING_FOCUS_BALANCED) ==
	      READING_FOCUS_HYSTERESIS_RATIO);
	CHECK(READING_FOCUS_HYSTERESIS_RATIO == 0.03);
	CHECK(reading_focus_hysteresis_ratio(READING_FOCUS_IMMEDIATE) <
	      reading_focus_hysteresis_ratio(READING_FOCUS_BALANCED));
	CHECK(reading_focus_hysteresis_ratio(READING_FOCUS_BALANCED) <
	      reading_focus_hysteresis_ratio(READING_FOCUS_STABLE));
	CHECK(reading_focus_hysteresis_ratio(READING_FOCUS_IMMEDIATE) > 0);
	reading_focus_set_mode((ReadingFocusMode)42);
	CHECK(reading_focus_get_mode() == READING_FOCUS_BALANCED);

	imm_down = switch_distance(READING_FOCUS_IMMEDIATE, 1);
	bal_down = switch_distance(READING_FOCUS_BALANCED, 1);
	stab_down = switch_distance(READING_FOCUS_STABLE, 1);
	imm_up = switch_distance(READING_FOCUS_IMMEDIATE, -1);
	bal_up = switch_distance(READING_FOCUS_BALANCED, -1);
	stab_up = switch_distance(READING_FOCUS_STABLE, -1);
	printf("switch distance past the boundary (px, 363 px viewport): "
	       "immediate %.0f/%.0f balanced %.0f/%.0f stable %.0f/%.0f "
	       "(down/up)\n", imm_down, imm_up, bal_down, bal_up, stab_down,
	       stab_up);

	/* every mode gets there */
	CHECK(imm_down >= 0 && bal_down >= 0 && stab_down >= 0);
	CHECK(imm_up >= 0 && bal_up >= 0 && stab_up >= 0);
	/* immediate first, stable last */
	CHECK(imm_down <= bal_down && bal_down <= stab_down);
	CHECK(imm_up <= bal_up && bal_up <= stab_up);
	CHECK(imm_down < stab_down);
	/* balanced: the hysteresis distance of before, 3 % of the viewport */
	CHECK(ABS(bal_down - h * 0.03) <= 1.0);
	/* the same effort either way */
	CHECK(ABS(imm_down - imm_up) <= 1.0);
	CHECK(ABS(bal_down - bal_up) <= 1.0);
	CHECK(ABS(stab_down - stab_up) <= 1.0);
	/* stable: roughly the boundary distance of its ratio */
	CHECK(ABS(stab_down - h * READING_FOCUS_HYSTERESIS_STABLE_RATIO) <= 1.0);
}

/* A fast step is not held by any mode: the geometry decides. */
static void
test_focus_modes_do_not_hold_real_movement(void)
{
	static const ReadingFocusMode modes[] = { READING_FOCUS_IMMEDIATE,
		READING_FOCUS_BALANCED, READING_FOCUS_STABLE };
	const gdouble h = 363;
	Layout l = layout_chapter(16, matthew16_heights, N(matthew16_heights),
				  71, 120, 1.0);
	guint m;

	for (m = 0; m < N(modes); m++) {
		gdouble from = value_with_verse_on_line(&l, 23, h) + 2;
		gdouble to = value_with_verse_on_line(&l, 27, h) + 5;

		reading_focus_set_mode(modes[m]);
		CHECK(track_at(&l, to, from, h, 16024, 0).picked == 16028);
		CHECK(track_at(&l, from, to, h, 16028, 0).picked == 16024);
	}
	reading_focus_set_mode(READING_FOCUS_BALANCED);
	g_free(l.doc);
}

/* The navigation handoff comes first, whatever the mode. */
static void
test_handoff_in_every_mode(void)
{
	static const ReadingFocusMode modes[] = { READING_FOCUS_IMMEDIATE,
		READING_FOCUS_BALANCED, READING_FOCUS_STABLE };
	guint m;

	for (m = 0; m < N(modes); m++) {
		reading_focus_set_mode(modes[m]);
		test_handoff_after_explicit_navigation();
		test_handoff_across_chapters();
		test_handoff_up_does_not_skip_short_verse();
	}
	reading_focus_set_mode(READING_FOCUS_BALANCED);
}

/* ---- the edges of the pane ------------------------------------------ */

/* SpaRV Matthew 16 as laid out by the real pane (WkHtml, 834 px wide,
 * 456 px viewport, zoom 100), scrolled to the very bottom without any
 * reserve: 24 on the reading line (150 px), 25-28 visible below. */
static const ReadingFocusBlock matthew16_bottom[] = {
	{ 16020, -2, 14 },   { 16021, 18, 70 },   { 16022, 74, 108 },
	{ 16023, 112, 146 }, { 16024, 150, 184 }, { 16025, 188, 222 },
	{ 16026, 226, 260 }, { 16027, 264, 298 }, { 16028, 302, 336 },
};
static const ReadingFocusBlock matthew16_top[] = {
	{ 16001, 79, 95 }, { 16002, 99, 133 }, { 16003, 137, 171 },
	{ 16004, 175, 209 }, { 16005, 213, 247 },
};

static void
test_edge_pushes(void)
{
	const gdouble h = 456, line = 456 * READING_FOCUS_LINE_RATIO;
	gint focus = 16024, step, previous;

	/* without a push, the line cannot get past 24 */
	CHECK(reading_focus_track(matthew16_bottom, N(matthew16_bottom), h,
				  16024, line, 0).picked == 16024);
	for (step = 0; step < 10; step++) {
		previous = focus;
		focus = reading_focus_track(matthew16_bottom,
					    N(matthew16_bottom), h, focus,
					    line, 1).picked;
		CHECK(focus == previous || focus == previous + 1);
	}
	CHECK(focus == 16028);

	focus = 16003;
	for (step = 0; step < 5; step++) {
		previous = focus;
		focus = reading_focus_track(matthew16_top, N(matthew16_top), h,
					    focus, line, -1).picked;
		CHECK(focus == previous || focus == previous - 1);
	}
	CHECK(focus == 16001);
}

/* Wheel at the edge of what the pane holds: the adjacent book's one-verse
 * previews (anchors "0next" / "0") are not verses, so pushing past the
 * last or first verse laid out stays where it is. Only the arrows cross
 * books. */
static void
test_wheel_stays_in_chapter_at_boundaries(void)
{
	const gdouble h = 456, line = 456 * READING_FOCUS_LINE_RATIO;
	gint i, focus;
	/* bottom of Matthew 16 with the 17:1 preview after 28 */
	const ReadingFocusBlock bottom[] = {
		{ 16026, 226, 260 }, { 16027, 264, 298 }, { 16028, 302, 336 },
		{ 0, 338, 422 }, /* "0hdr" / "0next": not verses */
	};
	/* top of Matthew 16 with the 15:39 preview before 1 */
	const ReadingFocusBlock top[] = {
		{ 0, 10, 60 }, { 16000, 57, 75 },
		{ 16001, 79, 95 }, { 16002, 99, 133 },
	};

	focus = 16028;
	for (i = 0; i < 10; i++) {
		focus = reading_focus_track(bottom, N(bottom), h, focus, line, 1).picked;
		CHECK(focus == 16028);
	}
	CHECK(focus / 1000 == 16);

	focus = 16001;
	for (i = 0; i < 10; i++) {
		focus = reading_focus_track(top, N(top), h, focus, line, -1).picked;
		CHECK(focus == 16001);
	}
	CHECK(focus / 1000 == 16);
}

/* The pane holds several chapters of a book: the reading line goes from
 * one chapter into the next and back, past the chapter heading (anchor
 * 17000, not a verse), one verse at a time. Ids carry the chapter, so 17:1
 * is never confused with 16:1. */
static void
test_tracking_across_chapters(void)
{
	const gdouble h = 363;
	/* Matthew 16:26-28, "Capitulo 17", 17:1-3, in document coordinates */
	ReadingFocusBlock doc[] = {
		{ 16026, 1000, 1034 }, { 16027, 1038, 1072 }, { 16028, 1076, 1110 },
		{ 17000, 1116, 1150 },
		{ 17001, 1154, 1188 }, { 17002, 1192, 1226 }, { 17003, 1230, 1264 },
		{ 16001, 5000, 5016 }, /* far away: not in view, must not match */
	};
	Layout l = { doc, (gint)N(doc), 1300 };
	gdouble line = h * READING_FOCUS_LINE_RATIO;
	gdouble value = 1076 - line + 2, previous = value, stop;
	gint focus = 16028, visited[8], n = 0, i;

	stop = 1192 - line + 20; /* the line well into 17:2 */
	for (;;) {
		ReadingFocusStep s2;
		value = MIN(stop, value + 3);
		s2 = track_at(&l, value, previous, h, focus, 0);
		previous = value;
		if (s2.picked != focus) {
			CHECK(reading_focus_id_is_verse(s2.picked));
			if (n < (gint)N(visited))
				visited[n++] = s2.picked;
			focus = s2.picked;
		}
		if (value >= stop)
			break;
	}
	CHECK(n == 2);
	CHECK(n >= 1 && visited[0] == 17001);
	CHECK(n >= 2 && visited[1] == 17002);

	n = 0;
	stop = 1076 - line + 2;
	for (;;) {
		ReadingFocusStep s2;
		value = MAX(stop, value - 3);
		s2 = track_at(&l, value, previous, h, focus, 0);
		previous = value;
		if (s2.picked != focus) {
			if (n < (gint)N(visited))
				visited[n++] = s2.picked;
			focus = s2.picked;
		}
		if (value <= stop)
			break;
	}
	CHECK(n == 2);
	CHECK(n >= 1 && visited[0] == 17001);
	CHECK(n >= 2 && visited[1] == 16028);
	for (i = 0; i < n; i++)
		CHECK(visited[i] != 16001);
}

/* ---- bottom reading reserve ----------------------------------------- */

static void
test_reserve_is_stable(void)
{
	gint first = reading_focus_bottom_reserve(363, 1107, 953), i, again;
	gint resized, zoomed;

	for (i = 0; i < 1000; i++)
		CHECK(reading_focus_bottom_reserve(363, 1107, 953) == first);

	/* resize: one change, then stable */
	resized = reading_focus_bottom_reserve(456, 1107, 953);
	CHECK(resized != first);
	for (i = 0; i < 1000; i++)
		CHECK(reading_focus_bottom_reserve(456, 1107, 953) == resized);

	/* zoom: the same chapter laid out 30 % larger */
	zoomed = reading_focus_bottom_reserve(456, 1439, 1239);
	CHECK(zoomed != resized);
	for (i = 0; i < 1000; i++)
		CHECK(reading_focus_bottom_reserve(456, 1439, 1239) == zoomed);

	/* applying the reserve does not change its own inputs (content
	 * bottom and last verse are measured without it), so recomputing
	 * after the margin is set gives the same value */
	again = reading_focus_bottom_reserve(456, 1107, 953);
	CHECK(again == resized);

	CHECK(reading_focus_bottom_reserve(456, 2000, 953) == 0);
	CHECK(reading_focus_bottom_reserve(0, 1107, 953) == 0);
	CHECK(reading_focus_bottom_reserve(456, 900, 953) == 0);
	CHECK(reading_focus_bottom_reserve(700, 1107, 953) > resized);
}

/* Slow wheel over whole chapters, with the reserve: the last verse always
 * takes the focus, for many viewports, zooms and chapter shapes. */
static void
test_reserve_reaches_last_verse(void)
{
	static const gint short_psalm[] = { 34, 16 };
	static gint long_psalm[176];
	static const gint tall_last[] = { 34, 34, 34, 900 };
	static const struct {
		const char *name;
		gint chapter;
		const gint *heights;
		gint n;
		gdouble tail;
	} chapters[] = {
		{ "matthew16", 16, matthew16_heights, N(matthew16_heights), 120 },
		{ "short", 117, short_psalm, N(short_psalm), 120 },
		{ "long", 119, long_psalm, N(long_psalm), 120 },
		{ "tall-last", 3, tall_last, N(tall_last), 120 },
		{ "no-tail", 16, matthew16_heights, N(matthew16_heights), 0 },
	};
	static const gint heights[] = { 240, 363, 380, 456, 525, 700, 1000, 1400 };
	static const gdouble zooms[] = { 1.0, 1.3, 2.0 };
	guint c, hi, zi;

	for (c = 0; c < N(long_psalm); c++)
		long_psalm[c] = (c % 3) ? 34 : 52;

	for (c = 0; c < N(chapters); c++) {
		for (hi = 0; hi < N(heights); hi++) {
			for (zi = 0; zi < N(zooms); zi++) {
				gdouble h = heights[hi];
				Layout l = layout_chapter(chapters[c].chapter,
					chapters[c].heights, chapters[c].n, 71,
					chapters[c].tail, zooms[zi]);
				gint last = l.doc[l.n - 1].id;
				gint reserve = reading_focus_bottom_reserve(
					heights[hi], (gint)l.content_bottom,
					(gint)l.doc[l.n - 1].top);
				gdouble max = MAX(0, l.content_bottom + reserve - h);
				gdouble value = 0, previous = 0;
				gint focus = chapters[c].chapter * 1000 + 1;

				for (;;) {
					value = MIN(max, value + 50 * zooms[zi]);
					focus = track_at(&l, value, previous, h,
							 focus, 0).picked;
					previous = value;
					if (value >= max)
						break;
				}
				if (focus != last)
					fprintf(stderr, "  %s h=%d zoom=%.1f -> %d "
						"(reserve %d)\n", chapters[c].name,
						heights[hi], zooms[zi], focus,
						reserve);
				CHECK(focus == last);
				CHECK(reserve >= 0);
				CHECK(reserve < h * 0.75);
				g_free(l.doc);
			}
		}
	}
}

static void
test_reading_zone(void)
{
	CHECK(reading_zone_scroll_delta(300, 400, H) == 0);
	CHECK(reading_zone_scroll_delta(200, 750, H) == 0);
	CHECK(reading_zone_scroll_delta(700, 800, H) == 50);
	CHECK(reading_zone_scroll_delta(1100, 1200, H) == 450);
	CHECK(reading_zone_scroll_delta(150, 250, H) == -50);
	CHECK(reading_zone_scroll_delta(-300, -200, H) == -500);
	CHECK(reading_zone_scroll_delta(600, 1500, H) == 400);
	CHECK(reading_zone_scroll_delta(0, 0, 0) == 0);
}

int
main(void)
{
	test_ids();
	test_keeps_current_in_reading_zone();
	test_switches_when_next_verse_crosses_line();
	test_tall_verse();
	test_title_slot_can_be_focused();
	test_headings_ignored();
	test_current_out_of_view();
	test_slow_scroll_visits_each_verse();
	test_pixel_back_steps_do_not_hold_the_focus();
	test_steps_below_verse_height_never_skip();
	test_fast_scroll_may_cross_several();
	test_hysteresis_only_at_adjacent_boundary();
	test_focus_ahead_of_line();
	test_edge_pushes();
	test_wheel_stays_in_chapter_at_boundaries();
	test_tracking_across_chapters();
	test_still_update_after_navigation_used_to_retreat();
	test_rebase_offset();
	test_handoff_after_explicit_navigation();
	test_handoff_across_chapters();
	test_handoff_up_does_not_skip_short_verse();
	test_reserve_is_stable();
	test_reserve_reaches_last_verse();
	test_reading_zone();
	test_focus_modes();
	test_focus_modes_do_not_hold_real_movement();
	test_handoff_in_every_mode();
	printf("reading_focus_failures=%d\n", failures);
	return failures ? 1 : 0;
}
