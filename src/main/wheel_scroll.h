/*
 * wheel_scroll.h - a mouse wheel notch as a short glide, not a jump
 *
 * GTK3 moves a scrolled window by delta_y * page_size^(2/3) per wheel
 * event, all at once: on a 363 px viewport one notch of a Logitech wheel
 * (delta_y 1.5 on Wayland) is 76 px, four verses of a typical chapter in
 * a single frame. This moves half of GTK's distance
 * (WHEEL_SCROLL_DISTANCE_SCALE) and spreads it over a few frames, so the
 * text slides and the reading focus sees every verse go past.
 *
 * Only for wheels. A touchpad already sends a stream of small deltas and
 * must not lag behind the fingers.
 */
#ifndef XIPHOS_WHEEL_SCROLL_H
#define XIPHOS_WHEEL_SCROLL_H

#include <glib.h>

G_BEGIN_DECLS

/* How long one notch takes to settle, with a sine ease-out
 * (wheel_scroll_value()). Measured on Matthew 16 laid out by the real
 * pane at 60 Hz, one 76.3 px notch (363 px viewport):
 *   cubic ease-out 120 ms  27.6 px/frame peak, a frame could cross two
 *                          verse starts (23 -> 24 -> 26, seen in the app)
 *   linear 120 ms          10.6 px/frame, half the notch only at 75 ms
 *   ease-in-out 150 ms     22.7 px/frame, barely moves in its first frame
 *   sine ease-out 140 ms   14.2 px/frame (16.5 at 456 px), moving from the
 *                          first frame, half the notch within ~63 ms, no
 *                          frame crossing two verse starts
 * Then 20 % longer, 168 ms, after trying it with a real wheel (Logitech MX
 * Master): 140 ms still felt too quick from verse to verse. Same curve,
 * ~11.8 px/frame peak at GTK's distance.
 * The figures above are for GTK's full distance per notch; the wheel now
 * moves WHEEL_SCROLL_DISTANCE_SCALE of it. */
#define WHEEL_SCROLL_DURATION_US 168000

/* A mouse wheel notch goes half as far as GTK would move it. At GTK's
 * distance (76 px on a 363 px viewport) a single notch of a Logitech MX
 * Master still carried the reading focus across two or three verses:
 * reading needs more turning of the wheel per verse, not a slower jump.
 * The distance still follows the viewport through GTK's own formula. */
#define WHEEL_SCROLL_DISTANCE_SCALE 0.50

typedef struct {
	gboolean active;
	gdouble start_value;
	gdouble target;
	gint64 start_time; /* microseconds, frame clock / monotonic time */
} WheelScroll;

/* GTK3's own wheel distance per unit of delta for a viewport this tall
 * (gtk_scrolled_window scroll unit): page_size^(2/3). */
gdouble wheel_scroll_unit(gdouble page_size);

/* Distance of one wheel event of delta_y `dy` in this viewport: GTK's
 * (dy * page_size^(2/3)) times WHEEL_SCROLL_DISTANCE_SCALE. */
gdouble wheel_scroll_notch_distance(gdouble dy, gdouble page_size);

/* A wheel step of `delta` px at time `now`, with the view at `current`
 * and scrollable between lower and max. A step the same way as a glide
 * in progress extends its target; a step the other way starts from where
 * the view is now, dropping what was left. The target is kept inside the
 * range. FALSE, and no glide, when the view cannot move that way at all
 * (it is at that edge). */
gboolean wheel_scroll_add(WheelScroll *scroll, gdouble current, gdouble delta,
			  gdouble lower, gdouble max, gint64 now);

/* Where the view should be at `now` (ease-out, never past the target,
 * never outside the range even if it shrank). Clears `active` once the
 * target is reached. */
gdouble wheel_scroll_value(WheelScroll *scroll, gint64 now, gdouble lower,
			   gdouble max);

/* 0..1, how far through the current glide `now` is. */
gdouble wheel_scroll_progress(const WheelScroll *scroll, gint64 now);

void wheel_scroll_cancel(WheelScroll *scroll);

G_END_DECLS

#endif /* XIPHOS_WHEEL_SCROLL_H */
