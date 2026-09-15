#include "main/reading_window.h"

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

static void
test_bounds(void)
{
	gint first, last;

	/* Matthew 16 of 28: 15, 16, 17 */
	reading_window_bounds(16, 28, READING_WINDOW_RADIUS, &first, &last);
	CHECK(first == 15 && last == 17);
	/* first and last chapters of a book: clamped, no chapter 0 / 29 */
	reading_window_bounds(1, 28, 1, &first, &last);
	CHECK(first == 1 && last == 2);
	reading_window_bounds(28, 28, 1, &first, &last);
	CHECK(first == 27 && last == 28);
	/* one-chapter book (Jude, Obadiah) */
	reading_window_bounds(1, 1, 1, &first, &last);
	CHECK(first == 1 && last == 1);
	/* whole book */
	reading_window_bounds(119, 150, 0, &first, &last);
	CHECK(first == 1 && last == 150);
	/* the reading-mode window of 5 */
	reading_window_bounds(119, 150, 5, &first, &last);
	CHECK(first == 114 && last == 124);
	/* out-of-range chapter is clamped, never an empty window */
	reading_window_bounds(40, 28, 1, &first, &last);
	CHECK(first == 27 && last == 28);
}

static void
test_recenter(void)
{
	/* [15,17] laid out */
	CHECK(!reading_window_needs_recenter(16, 28, 15, 17));
	/* reading moved into 17: 18 is missing */
	CHECK(reading_window_needs_recenter(17, 28, 15, 17));
	/* ... and back into 15: 14 is missing */
	CHECK(reading_window_needs_recenter(15, 28, 15, 17));
	/* after moving to [16,18] reading 17 is settled */
	CHECK(!reading_window_needs_recenter(17, 28, 16, 18));
	/* book edges: no chapter 0 or 29 to wait for */
	CHECK(!reading_window_needs_recenter(1, 28, 1, 2));
	CHECK(!reading_window_needs_recenter(28, 28, 27, 28));
	CHECK(reading_window_needs_recenter(2, 28, 1, 2));
	CHECK(!reading_window_needs_recenter(1, 1, 1, 1));
	/* whole book laid out: never */
	CHECK(!reading_window_needs_recenter(75, 150, 1, 150));
	/* outside the window altogether */
	CHECK(reading_window_needs_recenter(20, 28, 15, 17));
	CHECK(!reading_window_needs_recenter(0, 28, 15, 17));
}

int
main(void)
{
	test_bounds();
	test_recenter();
	printf("reading_window_failures=%d\n", failures);
	return failures ? 1 : 0;
}
