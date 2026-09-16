/*
 * wheel_scroll.c - a mouse wheel notch as a short glide, not a jump
 */

#include "main/wheel_scroll.h"

#include <math.h>

gdouble
wheel_scroll_unit(gdouble page_size)
{
	return page_size > 0 ? pow(page_size, 2.0 / 3.0) : 0;
}

static gdouble distance_scale = WHEEL_SCROLL_DISTANCE_SCALE;

void
wheel_scroll_set_distance_scale(gdouble scale)
{
	distance_scale = CLAMP(scale, WHEEL_SCROLL_DISTANCE_SCALE_MIN,
			       WHEEL_SCROLL_DISTANCE_SCALE_MAX);
}

gdouble
wheel_scroll_get_distance_scale(void)
{
	return distance_scale;
}

gdouble
wheel_scroll_notch_distance(gdouble dy, gdouble page_size)
{
	return dy * wheel_scroll_unit(page_size) * distance_scale;
}

gboolean
wheel_scroll_add(WheelScroll *scroll, gdouble current, gdouble delta,
		 gdouble lower, gdouble max, gint64 now)
{
	gdouble base = current, target;

	if (max < lower)
		max = lower;
	if (delta == 0)
		return scroll->active;
	if (scroll->active &&
	    ((delta > 0 && scroll->target > current) ||
	     (delta < 0 && scroll->target < current)))
		base = scroll->target;
	target = CLAMP(base + delta, lower, max);
	if (ABS(target - current) < 0.5) {
		scroll->active = FALSE;
		return FALSE;
	}
	scroll->start_value = current;
	scroll->target = target;
	scroll->start_time = now;
	scroll->active = TRUE;
	return TRUE;
}

gdouble
wheel_scroll_progress(const WheelScroll *scroll, gint64 now)
{
	gdouble t;

	if (!scroll->active)
		return 1.0;
	t = (gdouble)(now - scroll->start_time) / WHEEL_SCROLL_DURATION_US;
	return CLAMP(t, 0.0, 1.0);
}

gdouble
wheel_scroll_value(WheelScroll *scroll, gint64 now, gdouble lower, gdouble max)
{
	gdouble t, eased, target, value;

	if (max < lower)
		max = lower;
	target = CLAMP(scroll->target, lower, max);
	if (!scroll->active)
		return target;
	t = wheel_scroll_progress(scroll, now);
	/* sine ease-out: moving from the first frame, peak speed only pi/2
	 * times the average (cubic ease-out: 3 times) */
	eased = sin(t * G_PI / 2.0);
	value = scroll->start_value + (target - scroll->start_value) * eased;
	if (t >= 1.0) {
		value = target;
		scroll->active = FALSE;
	}
	return CLAMP(value, lower, max);
}

void
wheel_scroll_cancel(WheelScroll *scroll)
{
	scroll->active = FALSE;
}
