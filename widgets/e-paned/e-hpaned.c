/* EPaned - A slightly more advanced paned widget.
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Christopher James Lahey <clahey@helixcode.com>
 *
 * based on GtkPaned from Gtk+.  Gtk+ Copyright notice follows.
 */

/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "e-hpaned.h"

static void     e_hpaned_class_init     (EHPanedClass   *klass);
static void     e_hpaned_init           (EHPaned        *hpaned);
static void     e_hpaned_size_request   (GtkWidget      *widget,
					 GtkRequisition *requisition);
static void     e_hpaned_size_allocate  (GtkWidget      *widget,
					 GtkAllocation  *allocation);
static void     e_hpaned_draw           (GtkWidget      *widget,
					 GdkRectangle   *area);
static void     e_hpaned_xor_line       (EPaned         *paned);
static gboolean e_hpaned_button_press   (GtkWidget      *widget,
					 GdkEventButton *event);
static gboolean e_hpaned_button_release (GtkWidget      *widget,
					 GdkEventButton *event);
static gboolean e_hpaned_motion         (GtkWidget      *widget,
					 GdkEventMotion *event);
static gboolean e_hpaned_handle_shown   (EPaned *paned);

GtkType
e_hpaned_get_type (void)
{
  static GtkType hpaned_type = 0;

  if (!hpaned_type)
    {
      static const GtkTypeInfo hpaned_info =
      {
	"EHPaned",
	sizeof (EHPaned),
	sizeof (EHPanedClass),
	(GtkClassInitFunc) e_hpaned_class_init,
	(GtkObjectInitFunc) e_hpaned_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
	(GtkClassInitFunc) NULL,
      };

      hpaned_type = gtk_type_unique (E_TYPE_PANED, &hpaned_info);
    }

  return hpaned_type;
}

static void
e_hpaned_class_init (EHPanedClass *klass)
{
  GtkWidgetClass *widget_class;
  EPanedClass    *paned_class;

  widget_class = (GtkWidgetClass *) klass;
  paned_class  = E_PANED_CLASS (klass);

  widget_class->size_request = e_hpaned_size_request;
  widget_class->size_allocate = e_hpaned_size_allocate;
  widget_class->draw = e_hpaned_draw;
  widget_class->button_press_event = e_hpaned_button_press;
  widget_class->button_release_event = e_hpaned_button_release;
  widget_class->motion_notify_event = e_hpaned_motion;

  paned_class->handle_shown = e_hpaned_handle_shown;
}

static void
e_hpaned_init (EHPaned *hpaned)
{
  EPaned *paned;

  g_return_if_fail (hpaned != NULL);
  g_return_if_fail (E_IS_PANED (hpaned));

  paned = E_PANED (hpaned);
  
  paned->cursor_type = GDK_SB_H_DOUBLE_ARROW;
}

GtkWidget *
e_hpaned_new (void)
{
  EHPaned *hpaned;

  hpaned = gtk_type_new (E_TYPE_HPANED);

  return GTK_WIDGET (hpaned);
}

static void
e_hpaned_size_request (GtkWidget      *widget,
			 GtkRequisition *requisition)
{
  EPaned *paned;
  GtkRequisition child_requisition;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_HPANED (widget));
  g_return_if_fail (requisition != NULL);

  paned = E_PANED (widget);
  requisition->width = 0;
  requisition->height = 0;

  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
    {
      gtk_widget_size_request (paned->child1, &child_requisition);

      requisition->height = child_requisition.height;
      requisition->width = child_requisition.width;
    }

  if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    {
      gtk_widget_size_request (paned->child2, &child_requisition);

      requisition->height = MAX(requisition->height, child_requisition.height);
      requisition->width += child_requisition.width;
    }


  requisition->width += GTK_CONTAINER (paned)->border_width * 2;
  requisition->height += GTK_CONTAINER (paned)->border_width * 2;
  if (e_paned_handle_shown(paned))
    requisition->width += paned->handle_size;
}

static void
e_hpaned_size_allocate (GtkWidget     *widget,
			  GtkAllocation *allocation)
{
  EPaned *paned;
  GtkRequisition child1_requisition;
  GtkRequisition child2_requisition;
  GtkAllocation child1_allocation;
  GtkAllocation child2_allocation;
  gint border_width;
  gboolean handle_shown;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_HPANED (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  paned = E_PANED (widget);
  border_width = GTK_CONTAINER (paned)->border_width;
  
  if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
    gtk_widget_get_child_requisition (paned->child1, &child1_requisition);
  else
    child1_requisition.width = 0;

  if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
    gtk_widget_get_child_requisition (paned->child2, &child2_requisition);
  else
    child2_requisition.width = 0;

  e_paned_compute_position (paned,
			      MAX (1, (gint) widget->allocation.width
				   - 2 * border_width),
			      child1_requisition.width,
			      child2_requisition.width);

  /* Move the handle before the children so we don't get extra expose events */

  if (GTK_WIDGET_REALIZED (widget))
    gdk_window_move_resize (widget->window,
			    allocation->x, allocation->y,
			    allocation->width,
			    allocation->height);
  
  handle_shown = e_paned_handle_shown(paned);
  if (handle_shown)
    {
      paned->handle_xpos = paned->child1_real_size + border_width;
      paned->handle_ypos = border_width;
      paned->handle_width = paned->handle_size;
      paned->handle_height = MAX (1, (gint) widget->allocation.height - 2 * border_width);
    
      if (GTK_WIDGET_REALIZED (widget))
	{
	  gdk_window_move_resize (paned->handle,
				  paned->handle_xpos,
				  paned->handle_ypos,
				  paned->handle_size,
				  paned->handle_height);
	  if (paned->handle)
	    gdk_window_show(paned->handle);
	}
    }
  else
    {
      if (paned->handle && GTK_WIDGET_REALIZED (widget))
	gdk_window_hide(paned->handle);
    }

  child1_allocation.height = child2_allocation.height = MAX (1, ((int) allocation->height -
								 border_width * 2));
  child1_allocation.width = MAX (1, paned->child1_real_size);
  child1_allocation.x = border_width;
  child1_allocation.y = child2_allocation.y = border_width;
  
  if (handle_shown)
    child2_allocation.x = (child1_allocation.x + (int) child1_allocation.width +
			   (int) paned->handle_width);
  else
    child2_allocation.x = child1_allocation.x + (int) child1_allocation.width;

  child2_allocation.width = MAX (1, (gint) allocation->width - child2_allocation.x - border_width);

  /* Now allocate the childen, making sure, when resizing not to
   * overlap the windows */
  if (GTK_WIDGET_MAPPED (widget) &&
      paned->child1 && GTK_WIDGET_VISIBLE (paned->child1) &&
      paned->child1->allocation.width < child1_allocation.width)
    {
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
      gtk_widget_size_allocate (paned->child1, &child1_allocation);
    }
  else
    {
      if (paned->child1 && GTK_WIDGET_VISIBLE (paned->child1))
	gtk_widget_size_allocate (paned->child1, &child1_allocation);
      if (paned->child2 && GTK_WIDGET_VISIBLE (paned->child2))
	gtk_widget_size_allocate (paned->child2, &child2_allocation);
    }
}

static void
e_hpaned_draw (GtkWidget    *widget,
		 GdkRectangle *area)
{
  EPaned *paned;
  GdkRectangle handle_area, child_area;
  guint16 border_width;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (E_IS_PANED (widget));

  if (GTK_WIDGET_VISIBLE (widget) && GTK_WIDGET_MAPPED (widget)) 
    {
      paned = E_PANED (widget);
      border_width = GTK_CONTAINER (paned)->border_width;

      gdk_window_clear_area (widget->window,
			     area->x, area->y, area->width,
			     area->height);

      if (e_paned_handle_shown(paned))
	{
	  handle_area.x = paned->handle_xpos;
	  handle_area.y = paned->handle_ypos;
	  handle_area.width = paned->handle_size;
	  handle_area.height = paned->handle_height;
	  
	  if (gdk_rectangle_intersect (&handle_area, area, &child_area))
	    {
	      child_area.x -= paned->handle_xpos;
	      child_area.y -= paned->handle_ypos;
	      
	      gtk_paint_handle (widget->style,
				paned->handle,
				GTK_STATE_NORMAL,
				GTK_SHADOW_NONE,
				&child_area,
				widget,
				"paned",
				0, 0, -1, -1,
				GTK_ORIENTATION_VERTICAL);
	      
	    }
	}
      /* Redraw the children
       */
      if (paned->child1 && gtk_widget_intersect (paned->child1, area, &child_area))
	gtk_widget_draw(paned->child1, &child_area);
      if (paned->child2 && gtk_widget_intersect(paned->child2, area, &child_area))
	gtk_widget_draw(paned->child2, &child_area);
    }
}

static void
e_hpaned_xor_line (EPaned *paned)
{
  GtkWidget *widget;
  GdkGCValues values;
  guint16 xpos;

  widget = GTK_WIDGET(paned);

  if (!paned->xor_gc)
    {
      values.function = GDK_INVERT;
      values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      paned->xor_gc = gdk_gc_new_with_values (widget->window,
					      &values,
					      GDK_GC_FUNCTION | GDK_GC_SUBWINDOW);
    }

  gdk_gc_set_line_attributes (paned->xor_gc, 2, GDK_LINE_SOLID,
			      GDK_CAP_NOT_LAST, GDK_JOIN_BEVEL);

  xpos = paned->child1_real_size
    + GTK_CONTAINER (paned)->border_width + paned->handle_size / 2;

  gdk_draw_line (widget->window, paned->xor_gc,
		 xpos,
		 0,
		 xpos,
		 widget->allocation.height - 1);
}

static gboolean
e_hpaned_button_press (GtkWidget      *widget,
			 GdkEventButton *event)
{
  EPaned *paned;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (E_IS_PANED (widget), FALSE);

  paned = E_PANED (widget);

  if (!paned->in_drag &&
      event->window == paned->handle && event->button == 1)
    {
      paned->old_child1_size = paned->child1_size;
      paned->in_drag = TRUE;
      /* We need a server grab here, not gtk_grab_add(), since
       * we don't want to pass events on to the widget's children */
      gdk_pointer_grab(paned->handle, FALSE,
		       GDK_POINTER_MOTION_HINT_MASK
		       | GDK_BUTTON1_MOTION_MASK
		       | GDK_BUTTON_RELEASE_MASK,
		       NULL, NULL, event->time);
      paned->child1_size = e_paned_quantized_size(paned, paned->child1_size + event->x - paned->handle_size / 2);
      paned->child1_size = CLAMP (paned->child1_size, 0,
				  widget->allocation.width
				  - paned->handle_size
				  - 2 * GTK_CONTAINER (paned)->border_width);
      paned->child1_real_size = paned->child1_size;
      e_hpaned_xor_line (paned);

      return TRUE;
    }

  return FALSE;
}

static gboolean
e_hpaned_button_release (GtkWidget      *widget,
			   GdkEventButton *event)
{
  EPaned *paned;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (E_IS_PANED (widget), FALSE);

  paned = E_PANED (widget);

  if (paned->in_drag && (event->button == 1))
    {
      e_hpaned_xor_line (paned);
      paned->in_drag = FALSE;
      paned->position_set = TRUE;
      gdk_pointer_ungrab (event->time);
      gtk_widget_queue_resize (GTK_WIDGET (paned));

      return TRUE;
    }

  return FALSE;
}

static gboolean
e_hpaned_motion (GtkWidget      *widget,
		   GdkEventMotion *event)
{
  EPaned *paned;
  gint x;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (E_IS_PANED (widget), FALSE);

  paned = E_PANED (widget);

  if (event->is_hint || event->window != widget->window)
    gtk_widget_get_pointer(widget, &x, NULL);
  else
    x = event->x;

  if (paned->in_drag)
    {
      gint size;
      gint new_child1_size;

      size = x - GTK_CONTAINER (paned)->border_width - paned->handle_size / 2;

      new_child1_size = CLAMP (e_paned_quantized_size(paned, size),
			       paned->min_position,
			       paned->max_position);

      if (new_child1_size == paned->child1_size)
	return TRUE;

      e_hpaned_xor_line (paned);
      paned->child1_size = new_child1_size;
      paned->child1_real_size = paned->child1_size;
      e_hpaned_xor_line (paned);
    }

  return TRUE;
}

static gboolean
e_hpaned_handle_shown (EPaned *paned)
{
  return ((paned->child1 && paned->child2) &&
	  (GTK_WIDGET_VISIBLE (paned->child1) && GTK_WIDGET_VISIBLE (paned->child2)));
}
