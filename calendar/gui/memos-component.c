/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* memos-component.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *          Nathan Owens <pianocomp81@yahoo.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <glib/gi18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-source-selector.h>
#include <shell/e-user-creatable-items-handler.h>
#include <shell/e-component-view.h>
#include "e-cal-model.h"
#include "e-memos.h"
#include "memos-component.h"
#include "memos-control.h"
#include "e-comp-editor-registry.h"
#include "migration.h"
#include "comp-util.h"
#include "calendar-config.h"
#include "e-cal-popup.h"
#include "common/authentication.h"
#include "dialogs/calendar-setup.h"
#include "dialogs/comp-editor.h"
#include "dialogs/copy-source-dialog.h"
#include "dialogs/memo-editor.h"
#include "widgets/misc/e-info-label.h"
#include "e-util/e-error.h"
#include "calendar-component.h"

#define CREATE_MEMO_ID               "memo"
#define CREATE_SHARED_MEMO_ID	     "shared-memo"
#define CREATE_MEMO_LIST_ID          "memo-list"

#define WEB_BASE_URI "webcal://"
#define PERSONAL_RELATIVE_URI "system"

#define PARENT_TYPE bonobo_object_get_type ()

static BonoboObjectClass *parent_class = NULL;

typedef struct _MemosComponentView
{
	ESourceList *source_list;

	GSList *source_selection;

	EMemos *memos;
	ETable *table;
	ETableModel *model;

	GtkWidget *source_selector;

	GList *notifications;

} MemosComponentView;

struct _MemosComponentPrivate {

	ESourceList *source_list;
	GSList *source_selection;

	ECal *create_ecal;

	GList *notifications;
};

/* #define d(x) x */
#define d(x)

/* Utility functions.  */
/* FIXME Some of these are duplicated from calendar-component.c */
static gboolean
is_in_selection (GSList *selection, ESource *source)
{
	GSList *l;

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		if (!strcmp (e_source_peek_uid (selected_source), e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static gboolean
is_in_uids (GSList *uids, ESource *source)
{
	GSList *l;

	for (l = uids; l; l = l->next) {
		const char *uid = l->data;

		if (!strcmp (uid, e_source_peek_uid (source)))
			return TRUE;
	}

	return FALSE;
}

static void
source_selection_changed_cb (ESourceSelector *selector, MemosComponentView *component_view)
{
	GSList *selection, *l, *uids_selected = NULL;

	selection = e_source_selector_get_selection (E_SOURCE_SELECTOR (component_view->source_selector));

	for (l = component_view->source_selection; l; l = l->next) {
		ESource *old_selected_source = l->data;

		if (!is_in_selection (selection, old_selected_source))
			e_memos_remove_memo_source (component_view->memos, old_selected_source);
	}

	for (l = selection; l; l = l->next) {
		ESource *selected_source = l->data;

		e_memos_add_memo_source (component_view->memos, selected_source);
		uids_selected = g_slist_append (uids_selected, (char *)e_source_peek_uid (selected_source));
	}

	e_source_selector_free_selection (component_view->source_selection);
	component_view->source_selection = selection;

	/* Save the selection for next time we start up */
	calendar_config_set_memos_selected (uids_selected);
	g_slist_free (uids_selected);
}

/* Evolution::Component CORBA methods */

static gboolean
update_single_object (ECal *client, icalcomponent *icalcomp, gboolean fail_on_modify)
{
	char *uid;
	icalcomponent *tmp_icalcomp;

	d(g_message("memos-component.c: update_single_object called");)

	uid = (char *) icalcomponent_get_uid (icalcomp);

	if (e_cal_get_object (client, uid, NULL, &tmp_icalcomp, NULL)) {
		if (fail_on_modify)
			return FALSE;
		else
			return e_cal_modify_object (client, icalcomp, CALOBJ_MOD_ALL, NULL);
	}

	return e_cal_create_object (client, icalcomp, &uid, NULL);
}

static gboolean
update_objects (ECal *client, icalcomponent *icalcomp)
{
	icalcomponent *subcomp;
	icalcomponent_kind kind;

	d(g_message("memos-component.c: update_objects called");)

	kind = icalcomponent_isa (icalcomp);
	if (kind == ICAL_VJOURNAL_COMPONENT)
		return update_single_object (client, icalcomp, TRUE);
	else if (kind != ICAL_VCALENDAR_COMPONENT)
		return FALSE;

	subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
	while (subcomp) {
		gboolean success;

		kind = icalcomponent_isa (subcomp);
		if (kind == ICAL_VTIMEZONE_COMPONENT) {
			icaltimezone *zone;

			zone = icaltimezone_new ();
			icaltimezone_set_component (zone, subcomp);

			success = e_cal_add_timezone (client, zone, NULL);
			icaltimezone_free (zone, 1);
			if (!success)
				return success;
		} else if (kind == ICAL_VJOURNAL_COMPONENT) {
			success = update_single_object (client, subcomp, TRUE);
			if (!success)
				return success;
		}

		subcomp = icalcomponent_get_next_component (icalcomp, ICAL_ANY_COMPONENT);
	}

	return TRUE;
}

static void
selector_tree_drag_data_received (GtkWidget *widget,
				  GdkDragContext *context,
				  gint x,
				  gint y,
				  GtkSelectionData *data,
				  guint info,
				  guint time,
				  gpointer user_data)
{
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gpointer source = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean success = FALSE;
	icalcomponent *icalcomp = NULL;
	ECal *client = NULL;
	GSList *components, *p;
	MemosComponent *component = MEMOS_COMPONENT (user_data);

	if (!gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (widget),
						x, y, &path, &pos))
		goto finish;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));

	if (!gtk_tree_model_get_iter (model, &iter, path))
		goto finish;


	gtk_tree_model_get (model, &iter, 0, &source, -1);

	if (E_IS_SOURCE_GROUP (source) || e_source_get_readonly (source) || !data->data)
		goto finish;

	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);

	if (!client || !e_cal_open (client, TRUE, NULL))
		goto  finish;

	components = cal_comp_selection_get_string_list (data);
	for (p = components; p; p = p->next) {
		const char * uid;
		char *old_uid = NULL;
		icalcomponent *tmp_icalcomp = NULL;
		GError *error = NULL;
		char *comp_str; /* do not free this! */

		/* p->data is "source_uid\ncomponent_string" */
		comp_str = strchr (p->data, '\n');
		if (!comp_str)
			continue;

		comp_str [0] = 0;
		comp_str++;
		icalcomp = icalparser_parse_string (comp_str);

		if (!icalcomp)
			continue;

		/* FIXME deal with GDK_ACTION_ASK */
		if (context->action == GDK_ACTION_COPY) {
			old_uid = g_strdup (icalcomponent_get_uid (icalcomp));
			uid = e_cal_component_gen_uid ();
			icalcomponent_set_uid (icalcomp, uid);
		}

		uid = icalcomponent_get_uid (icalcomp);
		if (!old_uid)
			old_uid = g_strdup (uid);

		if (!e_cal_get_object (client, uid, NULL, &tmp_icalcomp, &error)) {
			if ((error != NULL) && (error->code != E_CALENDAR_STATUS_OBJECT_NOT_FOUND))
				g_message ("Failed to search the object in destination task list: %s",error->message);
			else {
				/* this will report success by last item, but we don't care */
				success = update_objects (client, icalcomp);

				if (success && context->action == GDK_ACTION_MOVE) {
					/* remove components rather here, because we know which has been moved */
					ESource *source_source;
					ECal *source_client;

					source_source = e_source_list_peek_source_by_uid (component->priv->source_list, p->data);

					if (source_source && !E_IS_SOURCE_GROUP (source_source) && !e_source_get_readonly (source_source)) {
						source_client = auth_new_cal_from_source (source_source, E_CAL_SOURCE_TYPE_JOURNAL);

						if (source_client) {
							gboolean read_only = TRUE;

							e_cal_is_read_only (source_client, &read_only, NULL);

							if (!read_only && e_cal_open (source_client, TRUE, NULL))
								e_cal_remove_object (source_client, old_uid, NULL);
							else if (!read_only)
								g_message ("Cannot open source client to remove old memo");

							g_object_unref (source_client);
						} else
							g_message ("Cannot create source client to remove old memo");
					}
				}
			}

			g_clear_error (&error);
		} else
			icalcomponent_free (tmp_icalcomp);

		g_free (old_uid);
		icalcomponent_free (icalcomp);
	}
	g_slist_foreach (components, (GFunc)g_free, NULL);
	g_slist_free (components);

 finish:
	if (client)
		g_object_unref (client);
	if (source)
		g_object_unref (source);
	if (path)
		gtk_tree_path_free (path);

	gtk_drag_finish (context, success, success && context->action == GDK_ACTION_MOVE, time);
}

static void
config_create_ecal_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	MemosComponent *component = data;
	MemosComponentPrivate *priv;

	priv = component->priv;

	g_object_unref (priv->create_ecal);
	priv->create_ecal = NULL;

	priv->notifications = g_list_remove (priv->notifications, GUINT_TO_POINTER (id));
}

static ECal *
setup_create_ecal (MemosComponent *component, MemosComponentView *component_view)
{
	MemosComponentPrivate *priv;
	ESource *source = NULL;
	char *uid;
	guint not;

	priv = component->priv;

	if (component_view) {
		ECal *default_ecal;

		default_ecal = e_memos_get_default_client (component_view->memos);
		if (default_ecal)
			return default_ecal;
	}

	if (priv->create_ecal)
		return priv->create_ecal;

	/* Get the current primary calendar, or try to set one if it doesn't already exist */
	uid = calendar_config_get_primary_memos ();
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);

		priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}

	if (!priv->create_ecal) {
		/* Try to create a default if there isn't one */
		source = e_source_list_peek_source_any (priv->source_list);
		if (source)
			priv->create_ecal = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_JOURNAL);
	}

	if (priv->create_ecal) {

		if (!e_cal_open (priv->create_ecal, FALSE, NULL)) {
			GtkWidget *dialog;

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the memo list '%s' for creating events and meetings"),
							   e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);

			return NULL;
		}

	} else {
		GtkWidget *dialog;

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 "%s", _("There is no calendar available for creating memos"));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		return NULL;
	}

	/* Handle the fact it may change on us */
	not = calendar_config_add_notification_primary_memos (config_create_ecal_changed_cb,
							      component);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Save the primary source for use elsewhere */
	calendar_config_set_primary_memos (e_source_peek_uid (source));

	return priv->create_ecal ;
}

/* Ensures the calendar is selected */
static void
object_created_cb (CompEditor *ce, EMemoTable *memo_table)
{
	g_return_if_fail (memo_table != NULL);

	memo_table->user_created_cal = comp_editor_get_client (ce);
	g_signal_emit_by_name (memo_table, "user_created");
	memo_table->user_created_cal = NULL;
}

static gboolean
create_new_memo (MemosComponent *memo_component, gboolean is_assigned, MemosComponentView *component_view)
{
	ECal *ecal;
	ECalComponent *comp;
	CompEditor *editor;
	CompEditorFlags flags = 0;

	ecal = setup_create_ecal (memo_component, component_view);
	if (!ecal)
		return FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	if (is_assigned) {
		flags |= COMP_EDITOR_IS_SHARED;
		flags |= COMP_EDITOR_USER_ORG;
	}

	editor = memo_editor_new (ecal, flags);
	comp = cal_comp_memo_new_with_defaults (ecal);

	if (component_view)
		g_signal_connect (editor, "object_created", G_CALLBACK (object_created_cb), e_memos_get_calendar_table (component_view->memos));

	comp_editor_edit_comp (editor, comp);
	gtk_window_present (GTK_WINDOW (editor));

	e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);

	return TRUE;
}

static MemosComponentView *
create_component_view (MemosComponent *memos_component)
{
	MemosComponentPrivate *priv;
	MemosComponentView *component_view;
	GtkWidget *statusbar_widget;

	priv = memos_component->priv;

	/* Create the calendar component view */
	component_view = g_new0 (MemosComponentView, 1);

	/* Create sidebar selector */
	g_signal_connect (component_view->source_selector, "drag-data-received",
			  G_CALLBACK (selector_tree_drag_data_received), memos_component);

	component_view->memos = (EMemos *) bonobo_control_get_widget (component_view->view_control);
	component_view->table = e_memo_table_get_table (e_memos_get_calendar_table (component_view->memos));
	component_view->model = E_TABLE_MODEL (e_memo_table_get_model (e_memos_get_calendar_table (component_view->memos)));

	/* connect after setting the initial selections, or we'll get unwanted calls
	   to calendar_control_sensitize_calendar_commands */
	g_signal_connect (component_view->source_selector, "selection_changed",
			  G_CALLBACK (source_selection_changed_cb), component_view);
	g_signal_connect (component_view->source_selector, "primary_selection_changed",
			  G_CALLBACK (primary_source_selection_changed_cb), component_view);

	return component_view;
}

static void
destroy_component_view (MemosComponentView *component_view)
{
	GList *l;

	if (component_view->source_list)
		g_object_unref (component_view->source_list);

	if (component_view->source_selection)
		e_source_selector_free_selection (component_view->source_selection);

	for (l = component_view->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (component_view->notifications);

	g_free (component_view);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MemosComponent *memos_component = MEMOS_COMPONENT (object);
	MemosComponentPrivate *priv = memos_component->priv;
	GList *l;

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}
	if (priv->source_selection != NULL) {
		e_source_selector_free_selection (priv->source_selection);
		priv->source_selection = NULL;
	}

	if (priv->create_ecal) {
		g_object_unref (priv->create_ecal);
		priv->create_ecal = NULL;
	}

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));
	g_list_free (priv->notifications);
	priv->notifications = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}
