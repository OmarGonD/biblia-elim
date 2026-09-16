#include "main/wheel_scroll.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
	do {                                                                      \
		if (!(condition)) {                                                \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,    \
				#condition);                                        \
			failures++;                                               \
		}                                                                 \
	} while (0)

#define FRAME_US 16667 /* 60 Hz */

/* The viewport of the reported session: 363 px, GTK's 76.3 px per 1.5. */
static const gdouble page = 363;
static const gdouble lower = 0, max = 779;

static gdouble
notch(void)
{
	return wheel_scroll_notch_distance(1.5, page);
}

static void
test_distance(void)
{
	/* GTK's unit, as measured on the real GtkScrolledWindow */
	CHECK(fabs(1.5 * wheel_scroll_unit(363) - 76.33) < 0.01);
	CHECK(fabs(1.5 * wheel_scroll_unit(456) - 88.87) < 0.01);
	CHECK(wheel_scroll_unit(0) == 0);

	/* the wheel moves half of it, still derived from the viewport */
	CHECK(WHEEL_SCROLL_DISTANCE_SCALE == 0.50);
	CHECK(fabs(wheel_scroll_notch_distance(1.5, 363) -
		   1.5 * wheel_scroll_unit(363) * 0.50) < 1e-9);
	CHECK(fabs(wheel_scroll_notch_distance(1.5, 363) - 38.165) < 0.01);
	CHECK(fabs(wheel_scroll_notch_distance(1.5, 456) - 44.43) < 0.01);
	CHECK(wheel_scroll_notch_distance(-1.5, 363) ==
	      -wheel_scroll_notch_distance(1.5, 363));

	/* unchanged glide: 168 ms, sine ease-out */
	CHECK(WHEEL_SCROLL_DURATION_US == 168000);
}

/* One notch: the scaled distance, over several frames, never past it. */
static void
test_one_notch_glides(void)
{
	WheelScroll ws = { 0 };
	gdouble d = notch(), v, prev = 600, max_step = 0;
	gint64 now = 1000000;
	gint frames = 0;

	CHECK(wheel_scroll_add(&ws, 600, d, lower, max, now));
	do {
		now += FRAME_US;
		v = wheel_scroll_value(&ws, now, lower, max);
		CHECK(v >= prev);	   /* monotonic */
		CHECK(v <= 600 + d + 1e-9); /* no overshoot */
		/* sine ease-out, exactly */
		if (ws.active)
			CHECK(fabs(v - (600 + d * sin((gdouble)(now - 1000000) /
					       WHEEL_SCROLL_DURATION_US * G_PI / 2))) < 1e-6);
		max_step = MAX(max_step, v - prev);
		prev = v;
		frames++;
	} while (ws.active && frames < 100);
	CHECK(fabs(v - (600 + d)) < 1e-9);
	CHECK(frames >= 10 && frames <= 12);
	CHECK(max_step < d * 0.17);
	/* moving from the very first frame: no ease-in latency */
	{
		WheelScroll first = { 0 };
		CHECK(wheel_scroll_add(&first, 0, d, lower, max, 0));
		CHECK(wheel_scroll_value(&first, FRAME_US, lower, max) > d * 0.1);
	}
	printf("one notch: %.2f px over %d frames, largest frame %.1f px\n", d,
	       frames, max_step);
}

/* Quick notches add up: one glide towards the sum, no competing ones,
 * no input lost. */
static void
test_several_quick_notches(void)
{
	const gdouble d = notch();
	static const gint counts[] = { 2, 5 };
	guint c;

	for (c = 0; c < G_N_ELEMENTS(counts); c++) {
		gint n = counts[c];
		WheelScroll ws = { 0 };
		gdouble v = 100, prev = 100;
		gint64 now = 1000000;
		gint i, frames = 0;

		for (i = 0; i < n; i++) {
			CHECK(wheel_scroll_add(&ws, v, d, lower, max, now));
			now += FRAME_US;
			v = wheel_scroll_value(&ws, now, lower, max);
			CHECK(v >= prev);
			prev = v;
		}
		/* n notches = n * 0.5 native notches */
		CHECK(fabs(ws.target - (100 + n * d)) < 1e-9);
		CHECK(fabs(ws.target - 100 -
			   n * 0.5 * 1.5 * wheel_scroll_unit(page)) < 1e-9);
		while (ws.active && frames++ < 100) {
			now += FRAME_US;
			v = wheel_scroll_value(&ws, now, lower, max);
			CHECK(v >= prev);
			CHECK(v <= 100 + n * d + 1e-9);
			prev = v;
		}
		CHECK(fabs(v - (100 + n * d)) < 1e-9);
	}
}

/* Turning the wheel back mid-glide: from where the view is, what was left
 * of the other way dropped, no rebound. */
static void
test_reversal(void)
{
	const gdouble d = notch();
	WheelScroll ws = { 0 };
	gdouble v = 300, turn, prev;
	gint64 now = 1000000;
	gint frames = 0;

	CHECK(wheel_scroll_add(&ws, v, 2 * d, lower, max, now));
	now += 3 * FRAME_US;
	v = wheel_scroll_value(&ws, now, lower, max);
	turn = v;
	CHECK(turn > 300 && turn < 300 + 2 * d);
	CHECK(wheel_scroll_add(&ws, v, -d, lower, max, now));
	CHECK(fabs(ws.target - (turn - d)) < 1e-9);
	prev = turn;
	while (ws.active && frames++ < 100) {
		now += FRAME_US;
		v = wheel_scroll_value(&ws, now, lower, max);
		CHECK(v <= prev);
		CHECK(v >= turn - d - 1e-9);
		prev = v;
	}
	CHECK(fabs(v - (turn - d)) < 1e-9);
}

/* Bounds: clamped targets, and no glide at all against an edge. */
static void
test_edges(void)
{
	const gdouble d = notch();
	WheelScroll ws = { 0 };
	gdouble v = 0;
	gint64 now = 1000000;
	gint frames = 0;

	CHECK(wheel_scroll_add(&ws, max - 10, d, lower, max, now));
	CHECK(ws.target == max);
	while (ws.active && frames++ < 100) {
		now += FRAME_US;
		v = wheel_scroll_value(&ws, now, lower, max);
		CHECK(v <= max);
	}
	CHECK(v == max);
	/* at the bottom, a further notch down is not a glide: an edge push */
	CHECK(!wheel_scroll_add(&ws, max, d, lower, max, now));
	CHECK(!ws.active);
	/* at the top, same upwards */
	CHECK(!wheel_scroll_add(&ws, 0, -d, lower, max, now));
	/* but back from the bottom works */
	CHECK(wheel_scroll_add(&ws, max, -d, lower, max, now));

	/* the range shrinking during a glide (reserve recomputed) */
	ws.active = FALSE;
	CHECK(wheel_scroll_add(&ws, 730, d, lower, max, now));
	now += FRAME_US;
	v = wheel_scroll_value(&ws, now, lower, 750);
	CHECK(v <= 750);
	now += WHEEL_SCROLL_DURATION_US;
	v = wheel_scroll_value(&ws, now, lower, 750);
	CHECK(v == 750);
	CHECK(!ws.active);
}

/* Ver > Navegación y rueda: the reader's distance per notch. Only the
 * fraction of GTK's distance changes; the formula, the 168 ms and the
 * curve do not. */
static void
test_reader_distance_scale(void)
{
	static const gdouble scales[] = { 0.25, 0.50, 0.75, 1.00 };
	const gdouble native = 1.5 * wheel_scroll_unit(page);
	guint i;

	CHECK(wheel_scroll_get_distance_scale() == WHEEL_SCROLL_DISTANCE_SCALE);
	for (i = 0; i < G_N_ELEMENTS(scales); i++) {
		wheel_scroll_set_distance_scale(scales[i]);
		CHECK(wheel_scroll_get_distance_scale() == scales[i]);
		CHECK(fabs(wheel_scroll_notch_distance(1.5, page) -
			   native * scales[i]) < 1e-9);
		/* still the viewport's own distance, not a fixed number */
		CHECK(fabs(wheel_scroll_notch_distance(1.5, 456) -
			   1.5 * wheel_scroll_unit(456) * scales[i]) < 1e-9);
	}
	wheel_scroll_set_distance_scale(1.00);
	CHECK(fabs(wheel_scroll_notch_distance(1.5, page) - native) < 1e-9);

	/* out of range */
	wheel_scroll_set_distance_scale(0.05);
	CHECK(wheel_scroll_get_distance_scale() == WHEEL_SCROLL_DISTANCE_SCALE_MIN);
	wheel_scroll_set_distance_scale(3.0);
	CHECK(wheel_scroll_get_distance_scale() == WHEEL_SCROLL_DISTANCE_SCALE_MAX);

	/* glide and curve unchanged by the scale */
	CHECK(WHEEL_SCROLL_DURATION_US == 168000);
	{
		WheelScroll ws = { 0 };
		gdouble d;

		wheel_scroll_set_distance_scale(0.35);
		d = wheel_scroll_notch_distance(1.5, page);
		CHECK(wheel_scroll_add(&ws, 100, d, lower, max, 0));
		CHECK(fabs(wheel_scroll_value(&ws, WHEEL_SCROLL_DURATION_US / 2,
					      lower, max) -
			   (100 + d * sin(G_PI / 4))) < 1e-6);
	}

	/* several notches add up at the scale in use */
	{
		WheelScroll ws = { 0 };
		gdouble v = 100;
		gint64 now = 0;
		gint n;

		wheel_scroll_set_distance_scale(0.75);
		for (n = 0; n < 3; n++) {
			CHECK(wheel_scroll_add(&ws, v, wheel_scroll_notch_distance(1.5, page),
					       lower, max, now));
			now += FRAME_US;
			v = wheel_scroll_value(&ws, now, lower, max);
		}
		CHECK(fabs(ws.target - (100 + 3 * native * 0.75)) < 1e-9);
	}

	/* changing the scale during a glide leaves that glide alone: same
	 * target, same path; only the next notch uses the new scale */
	{
		WheelScroll ws = { 0 };
		gdouble d50, before, after, target;

		wheel_scroll_set_distance_scale(0.50);
		d50 = wheel_scroll_notch_distance(1.5, page);
		CHECK(wheel_scroll_add(&ws, 200, d50, lower, max, 0));
		before = wheel_scroll_value(&ws, 3 * FRAME_US, lower, max);
		target = ws.target;
		wheel_scroll_set_distance_scale(0.25);
		CHECK(ws.target == target);
		after = wheel_scroll_value(&ws, 4 * FRAME_US, lower, max);
		CHECK(after >= before && after <= target);
		CHECK(fabs(after - (200 + d50 * sin(4.0 * FRAME_US /
						    WHEEL_SCROLL_DURATION_US *
						    G_PI / 2))) < 1e-6);
		CHECK(fabs(wheel_scroll_notch_distance(1.5, page) - native * 0.25) <
		      1e-9);
	}
	wheel_scroll_set_distance_scale(WHEEL_SCROLL_DISTANCE_SCALE);
}

/* The scale is the mouse wheel's only: the Bible pane asks for a notch
 * distance in its wheel (GDK_SOURCE_MOUSE) branch alone, and touchpads
 * keep GTK's native scrolling. */
static void
test_scale_is_for_mouse_wheels_only(void)
{
	gchar *src = NULL;
	const char *first, *wheel_branch;

	CHECK(g_file_get_contents(SRCDIR "/src/gtk/bibletext.c", &src, NULL, NULL));
	if (!src)
		return;
	/* the call itself, not the comment that names it */
	first = strstr(src, "= wheel_scroll_notch_distance(");
	CHECK(first != NULL);
	CHECK(first && strstr(first + 1, "= wheel_scroll_notch_distance(") == NULL);
	CHECK(strstr(src, "wheel = source_device && source == GDK_SOURCE_MOUSE;") != NULL);
	wheel_branch = strstr(src, "\tif (wheel) {");
	CHECK(wheel_branch != NULL && first != NULL && wheel_branch < first);
	g_free(src);
}

static void
test_cancel(void)
{
	WheelScroll ws = { 0 };
	CHECK(wheel_scroll_add(&ws, 10, notch(), lower, max, 0));
	wheel_scroll_cancel(&ws);
	CHECK(!ws.active);
	CHECK(wheel_scroll_progress(&ws, 5) == 1.0);
}

int
main(void)
{
	test_distance();
	test_one_notch_glides();
	test_several_quick_notches();
	test_reversal();
	test_edges();
	test_cancel();
	test_reader_distance_scale();
	test_scale_is_for_mouse_wheels_only();
	printf("wheel_scroll_failures=%d\n", failures);
	return failures ? 1 : 0;
}
