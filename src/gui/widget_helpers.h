/*
 * Biblia Elim
 * widget_helpers.h - small GTK 4 widget helpers shared by the interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GUI_WIDGET_HELPERS_H
#define GUI_WIDGET_HELPERS_H

#include <gtk/gtk.h>

/* Inline so that the unit tests which link a single interface file do not
 * need another object for them. */

/* Appends CHILD to BOX the way the interface used to pack it: EXPAND gives
 * it the box's spare room along the box's orientation, FILL FALSE centres
 * it inside that room, PADDING is left on both sides. */
static inline void
gui_box_pack(GtkBox *box, GtkWidget *child, gboolean expand,
	     gboolean fill, guint padding)
{
	gboolean horizontal =
	    gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) ==
	    GTK_ORIENTATION_HORIZONTAL;

	if (expand) {
		if (horizontal)
			gtk_widget_set_hexpand(child, TRUE);
		else
			gtk_widget_set_vexpand(child, TRUE);
		if (!fill) {
			if (horizontal)
				gtk_widget_set_halign(child, GTK_ALIGN_CENTER);
			else
				gtk_widget_set_valign(child, GTK_ALIGN_CENTER);
		}
	}
	if (padding) {
		if (horizontal) {
			gtk_widget_set_margin_start(child,
				gtk_widget_get_margin_start(child) + padding);
			gtk_widget_set_margin_end(child,
				gtk_widget_get_margin_end(child) + padding);
		} else {
			gtk_widget_set_margin_top(child,
				gtk_widget_get_margin_top(child) + padding);
			gtk_widget_set_margin_bottom(child,
				gtk_widget_get_margin_bottom(child) + padding);
		}
	}
	gtk_box_append(box, child);
}

/* The former gtk_box_set_child_packing(): the same expand/fill/padding as
 * gui_box_pack(), for a child already in BOX. */
static inline void
gui_box_set_child_packing(GtkBox *box, GtkWidget *child, gboolean expand,
			  gboolean fill, guint padding)
{
	gboolean horizontal =
	    gtk_orientable_get_orientation(GTK_ORIENTABLE(box)) ==
	    GTK_ORIENTATION_HORIZONTAL;
	GtkAlign align = (expand && !fill) ? GTK_ALIGN_CENTER : GTK_ALIGN_FILL;

	if (horizontal) {
		gtk_widget_set_hexpand(child, expand);
		gtk_widget_set_halign(child, align);
		gtk_widget_set_margin_start(child, padding);
		gtk_widget_set_margin_end(child, padding);
	} else {
		gtk_widget_set_vexpand(child, expand);
		gtk_widget_set_valign(child, align);
		gtk_widget_set_margin_top(child, padding);
		gtk_widget_set_margin_bottom(child, padding);
	}
}

/* The former gtk_box_reorder_child(): moves CHILD to POSITION among BOX's
 * children (0 is the first). */
static inline void
gui_box_reorder_child(GtkBox *box, GtkWidget *child, gint position)
{
	GtkWidget *after = NULL, *w;
	gint i = 0;

	for (w = gtk_widget_get_first_child(GTK_WIDGET(box));
	     w && i < position; w = gtk_widget_get_next_sibling(w)) {
		if (w == child)
			continue;
		after = w;
		i++;
	}
	gtk_box_reorder_child_after(box, child, after);
}

/* Takes WIDGET out of whatever holds it; the parent's reference goes, so
 * an unreferenced widget is finalized. */
static inline void
gui_widget_remove(GtkWidget *widget)
{
	GtkWidget *parent;

	if (!widget)
		return;
	parent = gtk_widget_get_parent(widget);
	if (!parent)
		return;
	if (GTK_IS_POPOVER(widget) && !GTK_IS_POPOVER(parent) &&
	    gtk_popover_get_child(GTK_POPOVER(widget)) != parent)
		/* a popover hangs from the widget it points at */
		gtk_widget_unparent(widget);
	else if (GTK_IS_NOTEBOOK(gtk_widget_get_parent(parent)) &&
	    gtk_notebook_page_num(GTK_NOTEBOOK(gtk_widget_get_parent(parent)),
				  widget) >= 0)
		/* a notebook page's widget sits inside the notebook's stack */
		gtk_notebook_remove_page(
		    GTK_NOTEBOOK(gtk_widget_get_parent(parent)),
		    gtk_notebook_page_num(
			GTK_NOTEBOOK(gtk_widget_get_parent(parent)), widget));
	else if (GTK_IS_BOX(parent))
		gtk_box_remove(GTK_BOX(parent), widget);
	else if (GTK_IS_GRID(parent))
		gtk_grid_remove(GTK_GRID(parent), widget);
	else if (GTK_IS_STACK(parent))
		gtk_stack_remove(GTK_STACK(parent), widget);
	else if (GTK_IS_FIXED(parent))
		gtk_fixed_remove(GTK_FIXED(parent), widget);
	else if (GTK_IS_TEXT_VIEW(parent))
		gtk_text_view_remove(GTK_TEXT_VIEW(parent), widget);
	else if (GTK_IS_OVERLAY(parent) &&
		 gtk_overlay_get_child(GTK_OVERLAY(parent)) != widget)
		gtk_overlay_remove_overlay(GTK_OVERLAY(parent), widget);
	else if (GTK_IS_OVERLAY(parent))
		gtk_overlay_set_child(GTK_OVERLAY(parent), NULL);
	else if (GTK_IS_PANED(parent)) {
		if (gtk_paned_get_start_child(GTK_PANED(parent)) == widget)
			gtk_paned_set_start_child(GTK_PANED(parent), NULL);
		else
			gtk_paned_set_end_child(GTK_PANED(parent), NULL);
	} else if (GTK_IS_WINDOW(parent))
		gtk_window_set_child(GTK_WINDOW(parent), NULL);
	else if (GTK_IS_SCROLLED_WINDOW(parent))
		gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(parent), NULL);
	else if (GTK_IS_VIEWPORT(parent))
		gtk_viewport_set_child(GTK_VIEWPORT(parent), NULL);
	else if (GTK_IS_FRAME(parent))
		gtk_frame_set_child(GTK_FRAME(parent), NULL);
	else if (GTK_IS_BUTTON(parent))
		gtk_button_set_child(GTK_BUTTON(parent), NULL);
	else if (GTK_IS_POPOVER(parent))
		gtk_popover_set_child(GTK_POPOVER(parent), NULL);
	else if (GTK_IS_REVEALER(parent))
		gtk_revealer_set_child(GTK_REVEALER(parent), NULL);
	else if (GTK_IS_EXPANDER(parent))
		gtk_expander_set_child(GTK_EXPANDER(parent), NULL);
	else if (GTK_IS_LIST_BOX_ROW(parent))
		gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(parent), NULL);
	else if (GTK_IS_LIST_BOX(parent))
		gtk_list_box_remove(GTK_LIST_BOX(parent), widget);
	else if (GTK_IS_FLOW_BOX_CHILD(parent))
		gtk_flow_box_child_set_child(GTK_FLOW_BOX_CHILD(parent), NULL);
	else if (GTK_IS_FLOW_BOX(parent))
		gtk_flow_box_remove(GTK_FLOW_BOX(parent), widget);
	else
		gtk_widget_unparent(widget);
}

/* The former gtk_widget_destroy(): a window is destroyed, any other
 * widget is taken out of its parent. */
static inline void
gui_widget_destroy(GtkWidget *widget)
{
	if (!widget)
		return;
	if (GTK_IS_WINDOW(widget))
		gtk_window_destroy(GTK_WINDOW(widget));
	else
		gui_widget_remove(widget);
}

/* GTK 4 GtkCheckButton (and the radio buttons built on it) is no longer a
 * GtkToggleButton: casting one to it fails and reads as «off». These take
 * either kind of button. */
static inline gboolean
gui_toggle_get_active(gpointer button)
{
	if (GTK_IS_CHECK_BUTTON(button))
		return gtk_check_button_get_active(GTK_CHECK_BUTTON(button));
	return gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
}

static inline void
gui_toggle_set_active(gpointer button, gboolean active)
{
	if (GTK_IS_CHECK_BUTTON(button))
		gtk_check_button_set_active(GTK_CHECK_BUTTON(button), active);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
}

/* The former gtk_container_set_border_width() on anything but a window:
 * the same room on the four sides. */
static inline void
gui_widget_set_margins(GtkWidget *widget, gint margin)
{
	gtk_widget_set_margin_start(widget, margin);
	gtk_widget_set_margin_end(widget, margin);
	gtk_widget_set_margin_top(widget, margin);
	gtk_widget_set_margin_bottom(widget, margin);
}

/* The former gtk_popover_new(relative_to): a popover pointing at RELATIVE.
 * Its parent is RELATIVE: remove it with gui_widget_destroy(). */
static inline GtkWidget *
gui_popover_new(GtkWidget *relative)
{
	GtkWidget *popover = gtk_popover_new();

	if (relative)
		gtk_widget_set_parent(popover, relative);
	return popover;
}

/* GTK 4 removed gtk_widget_get_action_group(): inserting a group through
 * this wrapper instead of the plain gtk_widget_insert_action_group() also
 * keeps a reference under a private key, so gui_widget_get_action_group()
 * can still find it afterwards (a fresh popup rebuild, a test). Passing
 * GROUP as NULL clears the prefix, matching the plain call. */
static inline void
gui_widget_insert_action_group(GtkWidget *widget, const gchar *prefix,
			       GActionGroup *group)
{
	gchar *key = g_strdup_printf("elim-actions:%s", prefix);

	gtk_widget_insert_action_group(widget, prefix, group);
	if (group)
		g_object_set_data_full(G_OBJECT(widget), key, g_object_ref(group),
				       g_object_unref);
	else
		g_object_set_data(G_OBJECT(widget), key, NULL);
	g_free(key);
}

/* The group PREFIX was last inserted on WIDGET with, through
 * gui_widget_insert_action_group() above; NULL if none was, or if the
 * plain gtk_widget_insert_action_group() was used instead. */
static inline GActionGroup *
gui_widget_get_action_group(GtkWidget *widget, const gchar *prefix)
{
	gchar *key = g_strdup_printf("elim-actions:%s", prefix);
	GActionGroup *group = G_ACTION_GROUP(g_object_get_data(G_OBJECT(widget), key));

	g_free(key);
	return group;
}

/* Gives WIDGET (and so the menus popped up from it) an action group PREFIX
 * holding the single action NAME, whose "activate" calls ACTIVATE with DATA.
 * A new call replaces the group. */
static inline void
gui_insert_single_action(GtkWidget *widget, const gchar *prefix,
			 const gchar *name, const GVariantType *type,
			 GCallback activate, gpointer data)
{
	GSimpleActionGroup *group = g_simple_action_group_new();
	GSimpleAction *action = g_simple_action_new(name, type);

	g_signal_connect(action, "activate", activate, data);
	g_action_map_add_action(G_ACTION_MAP(group), G_ACTION(action));
	gtk_widget_insert_action_group(widget, prefix, G_ACTION_GROUP(group));
	g_object_unref(action);
	g_object_unref(group);
}

/* GtkFileChooser deals in GFiles now; the interface keeps using paths. */
static inline gchar *
gui_file_chooser_get_filename(GtkFileChooser *chooser)
{
	GFile *file = gtk_file_chooser_get_file(chooser);
	gchar *path = file ? g_file_get_path(file) : NULL;

	g_clear_object(&file);
	return path;
}

static inline gchar *
gui_file_chooser_get_current_folder(GtkFileChooser *chooser)
{
	GFile *file = gtk_file_chooser_get_current_folder(chooser);
	gchar *path = file ? g_file_get_path(file) : NULL;

	g_clear_object(&file);
	return path;
}

static inline gboolean
gui_file_chooser_set_current_folder(GtkFileChooser *chooser,
				    const gchar *path)
{
	GFile *file;
	gboolean done;

	if (!path || !*path)
		return FALSE;
	file = g_file_new_for_path(path);
	done = gtk_file_chooser_set_current_folder(chooser, file, NULL);
	g_object_unref(file);
	return done;
}

static inline gboolean
gui_file_chooser_set_filename(GtkFileChooser *chooser, const gchar *path)
{
	GFile *file;
	gboolean done;

	if (!path || !*path)
		return FALSE;
	file = g_file_new_for_path(path);
	done = gtk_file_chooser_set_file(chooser, file, NULL);
	g_object_unref(file);
	return done;
}

/* The former gtk_button_set_image() on a labelled button: ICON before
 * the button's label. */
static inline void
gui_button_set_icon_and_label(GtkButton *button, const gchar *icon)
{
	gchar *label = g_strdup(gtk_button_get_label(button));
	GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

	gtk_box_append(GTK_BOX(box), gtk_image_new_from_icon_name(icon));
	if (label && *label) {
		GtkWidget *text = gtk_label_new_with_mnemonic(label);
		gtk_box_append(GTK_BOX(box), text);
	}
	gtk_button_set_child(button, box);
	g_free(label);
}

/* The former radio buttons: check buttons in GROUP's group (a new group
 * when GROUP is NULL). */
static inline GtkWidget *
gui_radio_button_new_with_label(GtkWidget *group, const gchar *label)
{
	GtkWidget *button = gtk_check_button_new_with_label(label);

	if (group)
		gtk_check_button_set_group(GTK_CHECK_BUTTON(button),
					   GTK_CHECK_BUTTON(group));
	return button;
}

static inline gboolean
gui_popover_destroy_idle(gpointer popover)
{
	if (gtk_widget_get_parent(GTK_WIDGET(popover)))
		gtk_widget_unparent(GTK_WIDGET(popover));
	return G_SOURCE_REMOVE;
}

static inline void
gui_popover_closed(GtkPopover *popover, gpointer data)
{
	(void)data;
	/* not from inside its own "closed": after the chosen item's action
	 * has run */
	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, gui_popover_destroy_idle,
			g_object_ref(popover), g_object_unref);
}

/* A popover built for one showing: it goes once closed. */
static inline void
gui_popover_destroy_on_close(GtkWidget *popover)
{
	g_signal_connect(popover, "closed", G_CALLBACK(gui_popover_closed), NULL);
}

/* The former gtk_widget_get_toplevel(): the topmost ancestor of WIDGET,
 * WIDGET itself when it has no parent. */
static inline GtkWidget *
gui_widget_get_toplevel(GtkWidget *widget)
{
	GtkWidget *parent;

	while (widget && (parent = gtk_widget_get_parent(widget)))
		widget = parent;
	return widget;
}

/* Removes every child of a box. */
static inline void
gui_box_remove_all(GtkWidget *box)
{
	GtkWidget *child;

	while ((child = gtk_widget_get_first_child(box)))
		gtk_box_remove(GTK_BOX(box), child);
}

/* The direct children of WIDGET, first to last; free with g_list_free(). */
static inline GList *
gui_widget_get_children(GtkWidget *widget)
{
	GList *list = NULL;
	GtkWidget *child;

	for (child = gtk_widget_get_last_child(widget); child;
	     child = gtk_widget_get_prev_sibling(child))
		list = g_list_prepend(list, child);
	return list;
}

/* Pointer and keyboard handlers.
 *
 * GTK 4 delivers input through event controllers instead of the widget's
 * *-event signals. These adaptors hand a handler the fields it used to read
 * from GdkEventButton / GdkEventKey / GdkEventScroll, so the handlers keep
 * their logic; returning TRUE still means "handled, stop here". Coordinates
 * are the widget's own. */

typedef struct {
	GdkEventType type; /* GDK_BUTTON_PRESS or GDK_BUTTON_RELEASE */
	gint n_press;	   /* 2 for the second press of a double click */
	guint button;
	gdouble x, y;
	GdkModifierType state;
	guint32 time;
	GdkEvent *event;
	GtkWidget *target; /* the widget under the pointer: WIDGET or inside it */
} GuiButtonEvent;

typedef gboolean (*GuiButtonFunc)(GtkWidget *widget, GuiButtonEvent *event,
				  gpointer data);

typedef struct {
	GuiButtonFunc press, release;
	gpointer data;
	guint last_button;
	guint32 last_time;
	gdouble last_x, last_y;
} GuiButtonHandlers;

/* Where EVENT happened, in WIDGET's coordinates. */
static inline gboolean
gui_event_widget_position(GdkEvent *event, GtkWidget *widget, gdouble *x,
			  gdouble *y)
{
	GtkNative *native = gtk_widget_get_native(widget);
	graphene_point_t in, out;
	double sx, sy, nx = 0, ny = 0;

	if (!native || !gdk_event_get_position(event, &sx, &sy))
		return FALSE;
	gtk_native_get_surface_transform(native, &nx, &ny);
	graphene_point_init(&in, (float)(sx - nx), (float)(sy - ny));
	if (!gtk_widget_compute_point(GTK_WIDGET(native), widget, &in, &out))
		return FALSE;
	*x = out.x;
	*y = out.y;
	return TRUE;
}

static inline gboolean
gui_button_event(GtkEventControllerLegacy *legacy, GdkEvent *gdk_event,
		 gpointer data)
{
	GuiButtonHandlers *h = (GuiButtonHandlers *)data;
	GtkWidget *widget =
	    gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(legacy));
	GdkEventType type = gdk_event_get_event_type(gdk_event);
	GuiButtonFunc fn;
	GuiButtonEvent event;

	if (type == GDK_BUTTON_PRESS)
		fn = h->press;
	else if (type == GDK_BUTTON_RELEASE)
		fn = h->release;
	else
		return FALSE;
	event.type = type;
	event.button = gdk_button_event_get_button(gdk_event);
	event.state = gdk_event_get_modifier_state(gdk_event);
	event.time = gdk_event_get_time(gdk_event);
	event.event = gdk_event;
	event.n_press = 1;
	if (!gui_event_widget_position(gdk_event, widget, &event.x, &event.y))
		event.x = event.y = 0;
	event.target = gtk_widget_pick(widget, event.x, event.y, GTK_PICK_DEFAULT);
	if (type == GDK_BUTTON_PRESS) {
		/* GDK 4 events carry no click count: count as GTK does */
		gint time = 400, distance = 5;

		g_object_get(gtk_widget_get_settings(widget),
			     "gtk-double-click-time", &time,
			     "gtk-double-click-distance", &distance, NULL);
		if (h->last_button == event.button && h->last_time &&
		    event.time - h->last_time <= (guint32)time &&
		    ABS(event.x - h->last_x) <= distance &&
		    ABS(event.y - h->last_y) <= distance) {
			event.n_press = 2;
			h->last_time = 0; /* a third press starts over */
		} else
			h->last_time = event.time;
		h->last_button = event.button;
		h->last_x = event.x;
		h->last_y = event.y;
	}
	return fn ? fn(widget, &event, h->data) : FALSE;
}

static inline void
gui_handlers_free(gpointer data, GClosure *closure)
{
	(void)closure;
	g_free(data);
}

/* The former "button-press-event" / "button-release-event" handlers, for
 * every mouse button. They see the raw press and release as before, even
 * when a gesture of the widget (a button's, a text view's) takes the
 * click. PHASE: GTK_PHASE_CAPTURE runs them before the widget and its
 * children handle the click, as a handler on a text or tree view used to;
 * GTK_PHASE_BUBBLE only if no child took it, as on a container. */
static inline GtkEventController *
gui_widget_on_button(GtkWidget *widget, GtkPropagationPhase phase,
		     GuiButtonFunc press, GuiButtonFunc release, gpointer data)
{
	GtkEventController *legacy = gtk_event_controller_legacy_new();
	GuiButtonHandlers *h = g_new0(GuiButtonHandlers, 1);

	h->press = press;
	h->release = release;
	h->data = data;
	gtk_event_controller_set_propagation_phase(legacy, phase);
	g_signal_connect_data(legacy, "event", G_CALLBACK(gui_button_event), h,
			      gui_handlers_free, (GConnectFlags)0);
	gtk_widget_add_controller(widget, legacy);
	return legacy;
}

typedef struct {
	GdkEventType type; /* GDK_KEY_PRESS or GDK_KEY_RELEASE */
	guint keyval;
	guint hardware_keycode;
	GdkModifierType state;
	guint32 time;
	GdkEvent *event;
} GuiKeyEvent;

typedef gboolean (*GuiKeyFunc)(GtkWidget *widget, GuiKeyEvent *event,
			       gpointer data);

typedef struct {
	GuiKeyFunc press, release;
	gpointer data;
} GuiKeyHandlers;

static inline void
gui_key_event_fill(GtkEventControllerKey *key, GdkEventType type,
		   guint keyval, guint keycode, GdkModifierType state,
		   GuiKeyEvent *event)
{
	GtkEventController *controller = GTK_EVENT_CONTROLLER(key);

	event->type = type;
	event->keyval = keyval;
	event->hardware_keycode = keycode;
	event->state = state;
	event->time = gtk_event_controller_get_current_event_time(controller);
	event->event = gtk_event_controller_get_current_event(controller);
}

static inline gboolean
gui_key_pressed(GtkEventControllerKey *key, guint keyval, guint keycode,
		GdkModifierType state, gpointer data)
{
	GuiKeyHandlers *h = (GuiKeyHandlers *)data;
	GuiKeyEvent event;

	if (!h->press)
		return FALSE;
	gui_key_event_fill(key, GDK_KEY_PRESS, keyval, keycode, state, &event);
	return h->press(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(key)),
			&event, h->data);
}

static inline void
gui_key_released(GtkEventControllerKey *key, guint keyval, guint keycode,
		 GdkModifierType state, gpointer data)
{
	GuiKeyHandlers *h = (GuiKeyHandlers *)data;
	GuiKeyEvent event;

	if (!h->release)
		return;
	gui_key_event_fill(key, GDK_KEY_RELEASE, keyval, keycode, state, &event);
	h->release(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(key)),
		   &event, h->data);
}

/* The former "key-press-event" / "key-release-event" handlers. They run in
 * the capture phase, before the focus widget sees the key, which is when a
 * handler connected to it or to its window used to run; a connect_after()
 * handler ran only for keys nobody took: that is GTK_PHASE_BUBBLE. */
static inline GtkEventController *
gui_widget_on_key_phase(GtkWidget *widget, GtkPropagationPhase phase,
			GuiKeyFunc press, GuiKeyFunc release, gpointer data)
{
	GtkEventController *key = gtk_event_controller_key_new();
	GuiKeyHandlers *h = g_new0(GuiKeyHandlers, 1);

	h->press = press;
	h->release = release;
	h->data = data;
	gtk_event_controller_set_propagation_phase(key, phase);
	g_signal_connect_data(key, "key-pressed", G_CALLBACK(gui_key_pressed),
			      h, gui_handlers_free, (GConnectFlags)0);
	g_signal_connect(key, "key-released", G_CALLBACK(gui_key_released), h);
	gtk_widget_add_controller(widget, key);
	return key;
}

static inline GtkEventController *
gui_widget_on_key(GtkWidget *widget, GuiKeyFunc press, GuiKeyFunc release,
		  gpointer data)
{
	return gui_widget_on_key_phase(widget, GTK_PHASE_CAPTURE, press,
				       release, data);
}

typedef struct {
	/* a wheel: UP/DOWN/LEFT/RIGHT, the deltas one unit per notch
	 * (fractions on high-resolution wheels); a touchpad: SMOOTH, the
	 * deltas in GTK 3's units */
	GdkScrollDirection direction;
	gdouble delta_x, delta_y;
	GdkModifierType state;
	guint32 time;
	GdkEvent *event;
} GuiScrollEvent;

typedef gboolean (*GuiScrollFunc)(GtkWidget *widget, GuiScrollEvent *event,
				  gpointer data);

typedef struct {
	GuiScrollFunc scroll;
	gpointer data;
} GuiScrollHandlers;

static inline gboolean
gui_scrolled(GtkEventControllerScroll *scroll, gdouble dx, gdouble dy,
	     gpointer data)
{
	GuiScrollHandlers *h = (GuiScrollHandlers *)data;
	GtkEventController *controller = GTK_EVENT_CONTROLLER(scroll);
	GuiScrollEvent event;

	event.delta_x = dx;
	event.delta_y = dy;
	if (gtk_event_controller_scroll_get_unit(scroll) !=
	    GDK_SCROLL_UNIT_WHEEL) {
		/* a touchpad scrolls in pixels; GTK 3 gave tenths of that,
		 * about a wheel notch per 1.0, and the handlers count so */
		event.delta_x = dx / 10.0;
		event.delta_y = dy / 10.0;
		event.direction = GDK_SCROLL_SMOOTH;
	} else {
		/* a wheel click: one step in one direction */
		if (dy < 0)
			event.direction = GDK_SCROLL_UP;
		else if (dy > 0)
			event.direction = GDK_SCROLL_DOWN;
		else if (dx < 0)
			event.direction = GDK_SCROLL_LEFT;
		else
			event.direction = GDK_SCROLL_RIGHT;
	}
	event.state = gtk_event_controller_get_current_event_state(controller);
	event.time = gtk_event_controller_get_current_event_time(controller);
	event.event = gtk_event_controller_get_current_event(controller);
	return h->scroll(gtk_event_controller_get_widget(controller), &event,
			 h->data);
}

/* The former "scroll-event" handlers, before the widget scrolls. */
static inline GtkEventController *
gui_widget_on_scroll(GtkWidget *widget, GuiScrollFunc fn, gpointer data)
{
	GtkEventController *scroll = gtk_event_controller_scroll_new(
	    GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
	GuiScrollHandlers *h = g_new0(GuiScrollHandlers, 1);

	h->scroll = fn;
	h->data = data;
	gtk_event_controller_set_propagation_phase(scroll, GTK_PHASE_CAPTURE);
	g_signal_connect_data(scroll, "scroll", G_CALLBACK(gui_scrolled), h,
			      gui_handlers_free, (GConnectFlags)0);
	gtk_widget_add_controller(widget, scroll);
	return scroll;
}

typedef struct {
	gdouble x, y; /* where the pointer came in; 0, 0 when it leaves */
} GuiCrossingEvent;

typedef gboolean (*GuiCrossingFunc)(GtkWidget *widget, GuiCrossingEvent *event,
				    gpointer data);

typedef struct {
	GuiCrossingFunc enter, leave;
	gpointer data;
} GuiCrossingHandlers;

static inline void
gui_crossing_enter(GtkEventControllerMotion *motion, gdouble x, gdouble y,
		   gpointer data)
{
	GuiCrossingHandlers *h = (GuiCrossingHandlers *)data;
	GuiCrossingEvent event = { x, y };

	if (h->enter)
		h->enter(gtk_event_controller_get_widget(
			     GTK_EVENT_CONTROLLER(motion)),
			 &event, h->data);
}

static inline void
gui_crossing_leave(GtkEventControllerMotion *motion, gpointer data)
{
	GuiCrossingHandlers *h = (GuiCrossingHandlers *)data;
	GuiCrossingEvent event = { 0, 0 };

	if (h->leave)
		h->leave(gtk_event_controller_get_widget(
			     GTK_EVENT_CONTROLLER(motion)),
			 &event, h->data);
}

/* The former "enter-notify-event" / "leave-notify-event" handlers. */
static inline GtkEventController *
gui_widget_on_crossing(GtkWidget *widget, GuiCrossingFunc enter,
		       GuiCrossingFunc leave, gpointer data)
{
	GtkEventController *motion = gtk_event_controller_motion_new();
	GuiCrossingHandlers *h = g_new0(GuiCrossingHandlers, 1);

	h->enter = enter;
	h->leave = leave;
	h->data = data;
	g_signal_connect_data(motion, "enter", G_CALLBACK(gui_crossing_enter),
			      h, gui_handlers_free, (GConnectFlags)0);
	g_signal_connect(motion, "leave", G_CALLBACK(gui_crossing_leave), h);
	gtk_widget_add_controller(widget, motion);
	return motion;
}

/* The former "focus-in-event" / "focus-out-event" handlers: ENTER and
 * LEAVE are called as (GtkEventControllerFocus *, DATA) when the focus
 * enters or leaves WIDGET or anything inside it. */
static inline GtkEventController *
gui_widget_on_focus(GtkWidget *widget, GCallback enter, GCallback leave,
		    gpointer data)
{
	GtkEventController *focus = gtk_event_controller_focus_new();

	if (enter)
		g_signal_connect(focus, "enter", enter, data);
	if (leave)
		g_signal_connect(focus, "leave", leave, data);
	gtk_widget_add_controller(widget, focus);
	return focus;
}

/* The former "size-allocate" handlers, for widgets whose class is not
 * ours: FUNC(widget, &allocation, data) after a frame's layout gave WIDGET
 * a new size. DATA must outlive WIDGET. */
typedef void (*GuiSizeFunc)(GtkWidget *widget, GdkRectangle *allocation,
			    gpointer data);

typedef struct {
	GtkWidget *widget;
	GuiSizeFunc func;
	gpointer data;
	GdkFrameClock *clock;
	gulong layout;
	gint width, height;
} GuiSizeWatch;

static inline void
gui_size_watch_layout(GdkFrameClock *clock, gpointer data)
{
	GuiSizeWatch *w = (GuiSizeWatch *)data;
	GdkRectangle allocation = { 0, 0, gtk_widget_get_width(w->widget),
				    gtk_widget_get_height(w->widget) };

	(void)clock;
	if (allocation.width == w->width && allocation.height == w->height)
		return;
	w->width = allocation.width;
	w->height = allocation.height;
	w->func(w->widget, &allocation, w->data);
}

static inline void
gui_size_watch_realize(GtkWidget *widget, gpointer data)
{
	GuiSizeWatch *w = (GuiSizeWatch *)data;

	if (w->clock)
		return;
	w->clock = (GdkFrameClock *)g_object_ref(gtk_widget_get_frame_clock(widget));
	w->layout = g_signal_connect_after(w->clock, "layout",
					   G_CALLBACK(gui_size_watch_layout), w);
}

static inline void
gui_size_watch_unrealize(GtkWidget *widget, gpointer data)
{
	GuiSizeWatch *w = (GuiSizeWatch *)data;

	(void)widget;
	if (!w->clock)
		return;
	g_signal_handler_disconnect(w->clock, w->layout);
	g_clear_object(&w->clock);
	w->width = w->height = -1;
}

static inline void
gui_size_watch_free(gpointer data)
{
	gui_size_watch_unrealize(NULL, data);
	g_free(data);
}

static inline void
gui_widget_watch_size(GtkWidget *widget, GuiSizeFunc func, gpointer data)
{
	GuiSizeWatch *w = g_new0(GuiSizeWatch, 1);

	w->widget = widget;
	w->func = func;
	w->data = data;
	w->width = w->height = -1;
	g_signal_connect(widget, "realize", G_CALLBACK(gui_size_watch_realize), w);
	g_signal_connect(widget, "unrealize", G_CALLBACK(gui_size_watch_unrealize), w);
	/* freed with the widget; the key keeps several watches apart */
	{
		gchar *key = g_strdup_printf("gui-size-watch-%p", (void *)w);
		g_object_set_data_full(G_OBJECT(widget), key, w, gui_size_watch_free);
		g_free(key);
	}
	if (gtk_widget_get_realized(widget))
		gui_size_watch_realize(widget, w);
}

typedef struct {
	GMainLoop *loop;
	gint response;
} GuiDialogRun;

static inline void
gui_dialog_run_quit(GuiDialogRun *run)
{
	if (g_main_loop_is_running(run->loop))
		g_main_loop_quit(run->loop);
}

static inline void
gui_dialog_run_response(GtkDialog *dialog, gint response, gpointer data)
{
	(void)dialog;
	((GuiDialogRun *)data)->response = response;
	gui_dialog_run_quit((GuiDialogRun *)data);
}

static inline gboolean
gui_dialog_run_close(GtkWindow *window, gpointer data)
{
	(void)window;
	((GuiDialogRun *)data)->response = GTK_RESPONSE_DELETE_EVENT;
	gui_dialog_run_quit((GuiDialogRun *)data);
	return TRUE; /* the caller decides what to do with the dialog */
}

static inline void
gui_dialog_run_gone(GtkWidget *widget, gpointer data)
{
	(void)widget;
	gui_dialog_run_quit((GuiDialogRun *)data);
}

/* The former gtk_dialog_run(): shows DIALOG modal and waits for its
 * response. Closing the window answers GTK_RESPONSE_DELETE_EVENT and leaves
 * the dialog alive, as before; the caller still destroys it. */
static inline gint
gui_dialog_run(GtkDialog *dialog)
{
	GuiDialogRun run = { g_main_loop_new(NULL, FALSE), GTK_RESPONSE_NONE };
	gboolean modal = gtk_window_get_modal(GTK_WINDOW(dialog));
	gulong ids[4];

	g_object_ref(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	ids[0] = g_signal_connect(dialog, "response",
				  G_CALLBACK(gui_dialog_run_response), &run);
	ids[1] = g_signal_connect(dialog, "close-request",
				  G_CALLBACK(gui_dialog_run_close), &run);
	ids[2] = g_signal_connect(dialog, "unmap",
				  G_CALLBACK(gui_dialog_run_gone), &run);
	ids[3] = g_signal_connect(dialog, "destroy",
				  G_CALLBACK(gui_dialog_run_gone), &run);
	gtk_window_present(GTK_WINDOW(dialog));
	g_main_loop_run(run.loop);
	for (guint i = 0; i < G_N_ELEMENTS(ids); i++)
		if (g_signal_handler_is_connected(dialog, ids[i]))
			g_signal_handler_disconnect(dialog, ids[i]);
	gtk_window_set_modal(GTK_WINDOW(dialog), modal);
	g_main_loop_unref(run.loop);
	g_object_unref(dialog);
	return run.response;
}

static inline void
gui_native_dialog_run_response(GtkNativeDialog *dialog, gint response,
			       gpointer data)
{
	(void)dialog;
	((GuiDialogRun *)data)->response = response;
	gui_dialog_run_quit((GuiDialogRun *)data);
}

/* The former gtk_native_dialog_run(): shows DIALOG modal and waits for its
 * response. */
static inline gint
gui_native_dialog_run(GtkNativeDialog *dialog)
{
	GuiDialogRun run = { g_main_loop_new(NULL, FALSE), GTK_RESPONSE_NONE };
	gulong id;

	g_object_ref(dialog);
	gtk_native_dialog_set_modal(dialog, TRUE);
	id = g_signal_connect(dialog, "response",
			      G_CALLBACK(gui_native_dialog_run_response), &run);
	gtk_native_dialog_show(dialog);
	g_main_loop_run(run.loop);
	g_signal_handler_disconnect(dialog, id);
	g_main_loop_unref(run.loop);
	g_object_unref(dialog);
	return run.response;
}

#endif /* GUI_WIDGET_HELPERS_H */
