/* Evolution calendar - Week day picker widget
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <libgnomecanvas/gnome-canvas-text.h>
#include <gal/util/e-util.h>
#include "weekday-picker.h"



#define PADDING 2

/* Private part of the WeekdayPicker structure */
struct _WeekdayPickerPrivate {
	/* Selected days; see weekday_picker_set_days() */
	guint8 day_mask;

	/* Blocked days; these cannot be modified */
	guint8 blocked_day_mask;

	/* Day that defines the start of the week; 0 = Sunday, ..., 6 = Saturday */
	int week_start_day;

	/* Metrics */
	int font_ascent, font_descent;
	int max_letter_width;

	/* Components */
	GnomeCanvasItem *boxes[7];
	GnomeCanvasItem *labels[7];
};



/* Signal IDs */
enum {
	CHANGED,
	LAST_SIGNAL
};

static void weekday_picker_class_init (WeekdayPickerClass *class);
static void weekday_picker_init (WeekdayPicker *wp);
static void weekday_picker_destroy (GtkObject *object);

static void weekday_picker_realize (GtkWidget *widget);
static void weekday_picker_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void weekday_picker_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void weekday_picker_style_set (GtkWidget *widget, GtkStyle *previous_style);

static GnomeCanvasClass *parent_class;

static guint wp_signals[LAST_SIGNAL];



E_MAKE_TYPE (weekday_picker, "WeekdayPicker", WeekdayPicker,
	     weekday_picker_class_init, weekday_picker_init, GNOME_TYPE_CANVAS);

/* Class initialization function for the weekday picker */
static void
weekday_picker_class_init (WeekdayPickerClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	parent_class = g_type_class_peek_parent (class);

	wp_signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_FIRST,
				G_TYPE_FROM_CLASS (object_class),
				GTK_SIGNAL_OFFSET (WeekdayPickerClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	object_class->destroy = weekday_picker_destroy;

	widget_class->realize = weekday_picker_realize;
	widget_class->size_request = weekday_picker_size_request;
	widget_class->size_allocate = weekday_picker_size_allocate;
	widget_class->style_set = weekday_picker_style_set;

	class->changed = NULL;
}

/* Event handler for the day items */
static gint
day_event_cb (GnomeCanvasItem *item, GdkEvent *event, gpointer data)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	int i;
	guint8 day_mask;

	wp = WEEKDAY_PICKER (data);
	priv = wp->priv;

	if (!(event->type == GDK_BUTTON_PRESS && event->button.button == 1))
		return FALSE;

	/* Find which box was clicked */

	for (i = 0; i < 7; i++)
		if (priv->boxes[i] == item || priv->labels[i] == item)
			break;

	g_assert (i != 7);

	/* Turn on that day */

	i += priv->week_start_day;
	if (i >= 7)
		i -= 7;

	if (priv->blocked_day_mask & (0x1 << i))
		return TRUE;

	if (priv->day_mask & (0x1 << i))
		day_mask = priv->day_mask & ~(0x1 << i);
	else
		day_mask = priv->day_mask | (0x1 << i);

	weekday_picker_set_days (wp, day_mask);

	return TRUE;
}


/* Creates the canvas items for the weekday picker.  The items are empty until
 * they are configured elsewhere.
 */
static void
create_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GnomeCanvasGroup *parent;
	int i;

	priv = wp->priv;

	parent = gnome_canvas_root (GNOME_CANVAS (wp));

	for (i = 0; i < 7; i++) {
		priv->boxes[i] = gnome_canvas_item_new (parent,
							GNOME_TYPE_CANVAS_RECT,
							NULL);
		g_signal_connect (priv->boxes[i], "event", G_CALLBACK (day_event_cb), wp);

		priv->labels[i] = gnome_canvas_item_new (parent,
							 GNOME_TYPE_CANVAS_TEXT,
							 NULL);
		g_signal_connect (priv->labels[i], "event", G_CALLBACK (day_event_cb), wp);
	}
}

/* Object initialization function for the weekday picker */
static void
weekday_picker_init (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	GTK_WIDGET_UNSET_FLAGS (wp, GTK_CAN_FOCUS);

	priv = g_new0 (WeekdayPickerPrivate, 1);

	wp->priv = priv;

	create_items (wp);
}

/* Finalize handler for the weekday picker */
static void
weekday_picker_destroy (GtkObject *object)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (object));

	wp = WEEKDAY_PICKER (object);
	priv = wp->priv;

	g_free (priv);
	wp->priv = NULL;

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
colorize_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	GdkColor *outline;
	GdkColor *fill, *sel_fill;
	GdkColor *text_fill, *sel_text_fill;
	int i;

	priv = wp->priv;

	outline = &GTK_WIDGET (wp)->style->fg[GTK_WIDGET_STATE (wp)];

	fill = &GTK_WIDGET (wp)->style->base[GTK_WIDGET_STATE (wp)];
	text_fill = &GTK_WIDGET (wp)->style->fg[GTK_WIDGET_STATE (wp)];

	sel_fill = &GTK_WIDGET (wp)->style->bg[GTK_STATE_SELECTED];
	sel_text_fill = &GTK_WIDGET (wp)->style->fg[GTK_STATE_SELECTED];

	for (i = 0; i < 7; i++) {
		int day;
		GdkColor *f, *t;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		if (priv->day_mask & (0x1 << day)) {
			f = sel_fill;
			t = sel_text_fill;
		} else {
			f = fill;
			t = text_fill;
		}

		gnome_canvas_item_set (priv->boxes[i],
				       "fill_color_gdk", f,
				       "outline_color_gdk", outline,
				       NULL);

		gnome_canvas_item_set (priv->labels[i],
				       "fill_color_gdk", t,
				       NULL);
	}
}

/* Configures the items in the weekday picker by setting their attributes. */
static void
configure_items (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;
	int width, height;
	int box_width;
	const char *str;
	int i;

	priv = wp->priv;

	width = GTK_WIDGET (wp)->allocation.width;
	height = GTK_WIDGET (wp)->allocation.height;

	box_width = (width - 1) / 7;
	str = _("SMTWTFS");

	for (i = 0; i < 7; i++) {
		char *c;
		int day;

		day = i + priv->week_start_day;
		if (day >= 7)
			day -= 7;

		gnome_canvas_item_set (priv->boxes[i],
				       "x1", (double) (i * box_width),
				       "y1", (double) 0,
				       "x2", (double) ((i + 1) * box_width),
				       "y2", (double) (height - 1),
				       "width_pixels", 0,
				       NULL);

		c = g_strndup (str + day, 1);
		gnome_canvas_item_set (priv->labels[i],
				       "text", c,
#if 0
				       "font_gdk", gtk_style_get_font (gtk_widget_get_style (GTK_WIDGET (wp))),
#endif
				       "x", (double) (i * box_width) + box_width / 2.0,
				       "y", (double) (1 + PADDING),
				       "anchor", GTK_ANCHOR_N,
				       NULL);
		g_free (c);
	}

	colorize_items (wp);
}

/* Realize handler for the weekday picker */
static void
weekday_picker_realize (GtkWidget *widget)
{
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	configure_items (wp);
}

/* Size_request handler for the weekday picker */
static void
weekday_picker_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	requisition->width = (priv->max_letter_width + 2 * PADDING + 1) * 7 + 1;
	requisition->height = (priv->font_ascent + priv->font_descent + 2 * PADDING + 2);
}

/* Size_allocate handler for the weekday picker */
static void
weekday_picker_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	WeekdayPicker *wp;

	wp = WEEKDAY_PICKER (widget);

	if (GTK_WIDGET_CLASS (parent_class)->size_allocate)
		(* GTK_WIDGET_CLASS (parent_class)->size_allocate) (widget, allocation);

	gnome_canvas_set_scroll_region (GNOME_CANVAS (wp),
					0, 0, allocation->width, allocation->height);

	configure_items (wp);
}

/* Style_set handler for the weekday picker */
static void
weekday_picker_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	WeekdayPicker *wp;
	WeekdayPickerPrivate *priv;
	GdkFont *font;
	int max_width;
	const char *str;
	int i, len;

	wp = WEEKDAY_PICKER (widget);
	priv = wp->priv;

	font = gtk_style_get_font (gtk_widget_get_style (widget));
	priv->font_ascent = font->ascent;
	priv->font_descent = font->descent;

	max_width = 0;

	str = _("SMTWTFS");
	len = strlen (str);

	for (i = 0; i < len; i++) {
		int w;

		w = gdk_char_measure (font, str[i]);
		if (w > max_width)
			max_width = w;
	}

	priv->max_letter_width = max_width;

	configure_items (wp);

	if (GTK_WIDGET_CLASS (parent_class)->style_set)
		(* GTK_WIDGET_CLASS (parent_class)->style_set) (widget, previous_style);
}



/**
 * weekday_picker_new:
 * @void: 
 * 
 * Creates a new weekday picker widget.
 * 
 * Return value: A newly-created weekday picker.
 **/
GtkWidget *
weekday_picker_new (void)
{
	return g_object_new (TYPE_WEEKDAY_PICKER, NULL);
}

/**
 * weekday_picker_set_days:
 * @wp: A weekday picker.
 * @day_mask: Bitmask with the days to be selected.
 * 
 * Sets the days that are selected in a weekday picker.  In the @day_mask,
 * Sunday is bit 0, Monday is bit 1, etc.
 **/
void
weekday_picker_set_days (WeekdayPicker *wp, guint8 day_mask)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;

	priv->day_mask = day_mask;
	colorize_items (wp);

	gtk_signal_emit (GTK_OBJECT (wp), wp_signals[CHANGED]);
}

/**
 * weekday_picker_get_days:
 * @wp: A weekday picker.
 * 
 * Queries the days that are selected in a weekday picker.
 * 
 * Return value: Bit mask of selected days.  Sunday is bit 0, Monday is bit 1,
 * etc.
 **/
guint8
weekday_picker_get_days (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, 0);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), 0);

	priv = wp->priv;
	return priv->day_mask;
}

/**
 * weekday_picker_set_blocked_days:
 * @wp: A weekday picker.
 * @blocked_day_mask: Bitmask with the days to be blocked.
 * 
 * Sets the days that the weekday picker will prevent from being modified by the
 * user.  The @blocked_day_mask is specified in the same way as in
 * weekday_picker_set_days().
 **/
void
weekday_picker_set_blocked_days (WeekdayPicker *wp, guint8 blocked_day_mask)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));

	priv = wp->priv;
	priv->blocked_day_mask = blocked_day_mask;
}

/**
 * weekday_picker_get_blocked_days:
 * @wp: A weekday picker.
 * 
 * Queries the set of days that the weekday picker prevents from being modified
 * by the user.
 * 
 * Return value: Bit mask of blocked days, with the same format as that returned
 * by weekday_picker_get_days().
 **/
guint
weekday_picker_get_blocked_days (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, 0);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), 0);

	priv = wp->priv;
	return priv->blocked_day_mask;
}

/**
 * weekday_picker_set_week_start_day:
 * @wp: A weekday picker.
 * @week_start_day: Index of the day that defines the start of the week; 0 is
 * Sunday, 1 is Monday, etc.
 * 
 * Sets the day that defines the start of the week for a weekday picker.
 **/
void
weekday_picker_set_week_start_day (WeekdayPicker *wp, int week_start_day)
{
	WeekdayPickerPrivate *priv;

	g_return_if_fail (wp != NULL);
	g_return_if_fail (IS_WEEKDAY_PICKER (wp));
	g_return_if_fail (week_start_day >= 0 && week_start_day < 7);

	priv = wp->priv;
	priv->week_start_day = week_start_day;

	configure_items (wp);
}

/**
 * weekday_picker_get_week_start_day:
 * @wp: A weekday picker.
 * 
 * Queries the day that defines the start of the week in a weekday picker.
 * 
 * Return value: Index of the day that defines the start of the week.  See
 * weekday_picker_set_week_start_day() to see how this is represented.
 **/
int
weekday_picker_get_week_start_day (WeekdayPicker *wp)
{
	WeekdayPickerPrivate *priv;

	g_return_val_if_fail (wp != NULL, -1);
	g_return_val_if_fail (IS_WEEKDAY_PICKER (wp), -1);

	priv = wp->priv;
	return priv->week_start_day;
}
