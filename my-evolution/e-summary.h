/*
 * e-summary.h: Header file for the ESummary object.
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Iain Holmes  <iain@ximian.com>
 */

#ifndef _E_SUMMARY_H__
#define _E_SUMMARY_H__

#include <gtk/gtkvbox.h>
#include "e-summary-type.h"
#include "e-summary-mail.h"
#include "e-summary-calendar.h"
#include "e-summary-rdf.h"
#include "e-summary-weather.h"
#include "e-summary-tasks.h"

#include <Evolution.h>

#define E_SUMMARY_TYPE (e_summary_get_type ())
#define E_SUMMARY(obj) (GTK_CHECK_CAST ((obj), E_SUMMARY_TYPE, ESummary))
#define E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), E_SUMMARY_TYPE, ESummaryClass))
#define IS_E_SUMMARY(obj) (GTK_CHECK_TYPE ((obj), E_SUMMARY_TYPE))
#define IS_E_SUMMARY_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), E_SUMMARY_TYPE))

typedef struct _ESummaryPrivate ESummaryPrivate;
typedef struct _ESummaryClass ESummaryClass;
typedef struct _ESummaryPrefs ESummaryPrefs;
typedef struct _ESummaryConnection ESummaryConnection;
typedef struct _ESummaryConnectionData ESummaryConnectionData;

typedef void (* ESummaryProtocolListener) (ESummary *summary,
					   const char *uri,
					   void *closure);
typedef int (* ESummaryConnectionCount) (ESummary *summary,
					 void *closure);
typedef GList *(* ESummaryConnectionAdd) (ESummary *summary,
					  void *closure);
typedef void (* ESummaryConnectionSetOnline) (ESummary *summary,
					      gboolean online,
					      void *closure);
typedef void (*ESummaryOnlineCallback) (ESummary *summary,
					void *closure);

struct _ESummaryConnection {
	ESummaryConnectionCount count;
	ESummaryConnectionAdd add;
	ESummaryConnectionSetOnline set_online;
	ESummaryOnlineCallback callback;

	void *closure;
	void *callback_closure;
};

struct _ESummaryConnectionData {
	char *hostname;
	char *type;
};

struct _ESummaryPrefs {

	/* Mail */
	GList *display_folders;
	gboolean show_full_path;

	/* RDF */
	GList *rdf_urls;
	int rdf_refresh_time;
	int limit;

	/* Weather */
	GList *stations;
	ESummaryWeatherUnits units;
	int weather_refresh_time;

	/* Schedule */
	ESummaryCalendarDays days;
	ESummaryCalendarNumTasks show_tasks;
};

struct _ESummary {
	GtkVBox parent;

	ESummaryPrefs *preferences;
	ESummaryPrefs *old_prefs;

	ESummaryMail *mail;
	ESummaryCalendar *calendar;
	ESummaryRDF *rdf;
	ESummaryWeather *weather;
	ESummaryTasks *tasks;

	ESummaryPrivate *priv;

	GNOME_Evolution_ShellView shell_view_interface;

	GtkWidget *prefs_window;
	gboolean online;
};

struct _ESummaryClass {
	GtkVBoxClass parent_class;
};

GtkType e_summary_get_type (void);
GtkWidget *e_summary_new (const GNOME_Evolution_Shell shell);
void e_summary_print (GtkWidget *widget,
		      ESummary *summary);
void e_summary_reload (GtkWidget *widget,
		       ESummary *summary);
void e_summary_draw (ESummary *summary);
void e_summary_change_current_view (ESummary *summary,
				    const char *uri);
void e_summary_set_message (ESummary *summary,
			    const char *message,
			    gboolean busy);
void e_summary_unset_message (ESummary *summary);
void e_summary_add_protocol_listener (ESummary *summary,
				      const char *protocol,
				      ESummaryProtocolListener listener,
				      void *closure);
void e_summary_reconfigure (ESummary *summary);
int e_summary_count_connections (ESummary *summary);
GList *e_summary_add_connections (ESummary *summary);
void e_summary_set_online (ESummary *summary,
			   gboolean online,
			   ESummaryOnlineCallback callback,
			   void *closure);
void e_summary_add_online_connection (ESummary *summary,
				      ESummaryConnection *connection);
#endif
