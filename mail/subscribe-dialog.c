/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * subscribe-dialog.c: Subscribe dialog
 *
 * Author:
 *   Chris Toshok (toshok@helixcode.com)
 *
 * (C) 2000 Helix Code, Inc.
 */
#include <config.h>
#include <gnome.h>
#include "subscribe-dialog.h"
#include "e-util/e-html-utils.h"
#include "e-title-bar.h"
#include <gtkhtml/gtkhtml.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-unicode.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-tree.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-table-scrolled.h>
#include <gal/e-table/e-tree-simple.h>
#include <gal/e-paned/e-hpaned.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h> 
#include <bonobo/bonobo-ui-component.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>

#include "mail.h"
#include "mail-tools.h"
#include "camel/camel.h"

#include "art/empty.xpm"
#include "art/mark.xpm"


#define DEFAULT_STORE_TABLE_WIDTH         150
#define DEFAULT_WIDTH                     500
#define DEFAULT_HEIGHT                    300

#define PARENT_TYPE (gtk_object_get_type ())

#define FOLDER_ETABLE_SPEC "<ETableSpecification>                      \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                          \
</ETableSpecification>"

#define STORE_ETABLE_SPEC "<ETableSpecification>                       \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
		<column> 1 </column>     			       \
		<column> 2 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                          \
</ETableSpecification>"

enum {
	FOLDER_COL_SUBSCRIBED,
	FOLDER_COL_NAME,
	FOLDER_COL_LAST
};

enum {
	STORE_COL_NAME,
	STORE_COL_LAST
};

static char *folder_headers [FOLDER_COL_LAST] = {
  "Subscribed",
  "Folder",
};

static char *store_headers [FOLDER_COL_LAST] = {
  "Store"
};

static GtkObjectClass *subscribe_dialog_parent_class;

static void
set_pixmap (BonoboUIComponent *component,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (component, xml_path, pixbuf);

	gdk_pixbuf_unref (pixbuf);

	g_free (path);
}

static void
update_pixmaps (BonoboUIComponent *component)
{
	set_pixmap (component, "/Toolbar/SubscribeFolder", "fetch-mail.png"); /* XXX */
	set_pixmap (component, "/Toolbar/UnsubscribeFolder", "compose-message.png"); /* XXX */
	set_pixmap (component, "/Toolbar/RefreshList", "forward.png"); /* XXX */
}

static GtkWidget*
make_folder_search_widget (GtkSignalFunc start_search_func,
			   gpointer user_data_for_search)
{
	GtkWidget *search_hbox = gtk_hbox_new (FALSE, 0);
	GtkWidget *search_entry = gtk_entry_new ();

	if (start_search_func) {
		gtk_signal_connect (GTK_OBJECT (search_entry), "activate",
				    start_search_func,
				    user_data_for_search);
	}
	
	/* add the search entry to the our search_vbox */
	gtk_box_pack_start (GTK_BOX (search_hbox),
			    gtk_label_new(_("Display folders containing:")),
			    FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (search_hbox), search_entry,
			    FALSE, TRUE, 3);

	return search_hbox;
}



static void
subscribe_close (BonoboUIComponent *uic,
		 void *user_data, const char *path)
{
	SubscribeDialog *sc = (SubscribeDialog*)user_data;

	gtk_widget_destroy (sc->app);
}

static void
subscribe_select_all (BonoboUIComponent *uic,
		      void *user_data, const char *path)
{
}

static void
subscribe_unselect_all (BonoboUIComponent *uic,
			void *user_data, const char *path)
{
}

static void
subscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, model_row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	printf ("subscribe: row %d, node_data %p\n", model_row,
		e_tree_model_node_get_data (sc->folder_model, node));

	if (!camel_store_folder_subscribed (sc->store, info->name)) {
		camel_store_subscribe_folder (sc->store, info->name, NULL);
#if 0
		/* we need to remove it from the shell */
		evolution_storage_removed_folder (sc->storage, info->name);
#endif

		e_tree_model_node_changed (sc->folder_model, node);
	}
}

static void
subscribe_folder (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->folder_etable)->table,
				      subscribe_folder_foreach, sc);
}

static void
unsubscribe_folder_foreach (int model_row, gpointer closure)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (closure);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, model_row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	printf ("unsubscribe: row %d, node_data %p\n", model_row,
		e_tree_model_node_get_data (sc->folder_model, node));

	if (camel_store_folder_subscribed (sc->store, info->name)) {
		camel_store_unsubscribe_folder (sc->store, info->name, NULL);

#if 0
		/* we need to remove it from the shell */
		evolution_storage_removed_folder (sc->storage, info->name);
#endif

		e_tree_model_node_changed (sc->folder_model, node);
	}
}


static void
unsubscribe_folder (GtkWidget *widget, gpointer user_data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (user_data);

	e_table_selected_row_foreach (E_TABLE_SCROLLED(sc->folder_etable)->table,
				      unsubscribe_folder_foreach, sc);
}

static void
subscribe_refresh_list (GtkWidget *widget, gpointer user_data)
{
	printf ("subscribe_refresh_list\n");
}

static void
subscribe_search (GtkWidget *widget, gpointer user_data)
{
	char* search_pattern = e_utf8_gtk_entry_get_text(GTK_ENTRY(widget));

	printf ("subscribe_search (%s)\n", search_pattern);

	g_free (search_pattern);
}


/* HTML Helpers */
static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	if (GTK_LAYOUT (widget)->height > 90)
		requisition->height = 90;
	else
		requisition->height = GTK_LAYOUT (widget)->height;
}

/* Returns a GtkHTML which is already inside a GtkScrolledWindow. If
 * @white is TRUE, the GtkScrolledWindow will be inside a GtkFrame.
 */
static GtkWidget *
html_new (gboolean white)
{
	GtkWidget *html, *scrolled, *frame;
	GtkStyle *style;
	
	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       white ? &style->white :
						       &style->bg[0]);
	}
	gtk_widget_set_sensitive (html, FALSE);
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER,
					GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);
	
	if (white) {
		frame = gtk_frame_new (NULL);
		gtk_frame_set_shadow_type (GTK_FRAME (frame),
					   GTK_SHADOW_ETCHED_IN);
		gtk_container_add (GTK_CONTAINER (frame), scrolled);
		gtk_widget_show_all (frame);
	} else
		gtk_widget_show_all (scrolled);
	
	return html;
}

static void
put_html (GtkHTML *html, char *text)
{
	GtkHTMLStream *handle;
	
	text = e_text_to_html (text, (E_TEXT_TO_HTML_CONVERT_NL |
				      E_TEXT_TO_HTML_CONVERT_SPACES |
				      E_TEXT_TO_HTML_CONVERT_URLS));
	handle = gtk_html_begin (html);
	gtk_html_write (html, handle, "<HTML><BODY>", 12);
	gtk_html_write (html, handle, text, strlen (text));
	gtk_html_write (html, handle, "</BODY></HTML>", 14);
	g_free (text);
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
}



/* etable stuff for the subscribe ui */

static int
folder_etable_col_count (ETableModel *etm, void *data)
{
	return FOLDER_COL_LAST;
}

static void*
folder_etable_duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
folder_etable_free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
folder_etable_init_value (ETableModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
folder_etable_value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
folder_etable_value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}

static GdkPixbuf*
folder_etree_icon_at (ETreeModel *etree, ETreePath *path, void *model_data)
{
	return NULL; /* XXX no icons for now */
}

static void*
folder_etree_value_at (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	SubscribeDialog *dialog = SUBSCRIBE_DIALOG (model_data);
	CamelFolderInfo *info = e_tree_model_node_get_data (etree, path);

	if (col == FOLDER_COL_NAME)
		return info->name;
	else /* FOLDER_COL_SUBSCRIBED */
		return GINT_TO_POINTER(camel_store_folder_subscribed (dialog->store, info->name));
}

static void
folder_etree_set_value_at (ETreeModel *etree, ETreePath *path, int col, const void *val, void *model_data)
{
	/* nothing */
}

static gboolean
folder_etree_is_editable (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	return FALSE;
}



static int
store_etable_col_count (ETableModel *etm, void *data)
{
	return STORE_COL_LAST;
}

static int
store_etable_row_count (ETableModel *etm, void *data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);

	return g_list_length (sc->store_list);
}

static void*
store_etable_value_at (ETableModel *etm, int col, int row, void *data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelStore *store = (CamelStore*)g_list_nth_data (sc->store_list, row);

	return camel_service_get_name (CAMEL_SERVICE (store), FALSE);
}

static void
store_etable_set_value_at (ETableModel *etm, int col, int row, const void *val, void *data)
{
	/* nada */
}

static gboolean
store_etable_is_editable (ETableModel *etm, int col, int row, void *data)
{
	return FALSE;
}

static void*
store_etable_duplicate_value (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup (val);
}

static void
store_etable_free_value (ETableModel *etm, int col, void *val, void *data)
{
	g_free (val);
}

static void*
store_etable_initialize_value (ETableModel *etm, int col, void *data)
{
	return g_strdup ("");
}

static gboolean
store_etable_value_is_empty (ETableModel *etm, int col, const void *val, void *data)
{
	return !(val && *(char *)val);
}

static char*
store_etable_value_to_string (ETableModel *etm, int col, const void *val, void *data)
{
	return g_strdup(val);
}



static void
build_etree_from_folder_info (SubscribeDialog *sc, ETreePath *parent, CamelFolderInfo *info)
{
	CamelFolderInfo *i;

	if (info == NULL)
		return;

	for (i = info; i; i = i->sibling) {
		ETreePath *node = e_tree_model_node_insert (sc->folder_model, parent, -1, i);
		build_etree_from_folder_info (sc, node, i->child);
	}
}


static void
storage_selected_cb (ETable *table,
		     int row,
		     gpointer data)
{
	CamelException *ex = camel_exception_new();
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	CamelStore *store = (CamelStore*)g_list_nth_data (sc->store_list, row);

	/* free up the existing CamelFolderInfo* if there is any */
	if (sc->folder_info)
		camel_store_free_folder_info (sc->store, sc->folder_info);

	sc->store = store;
	sc->folder_info = camel_store_get_folder_info (sc->store, NULL, TRUE, TRUE, FALSE, ex);

	if (camel_exception_is_set (ex)) {
		printf ("camel_store_get_folder_info failed\n");
		camel_exception_free (ex);
		return;
	}

	e_tree_model_node_remove (sc->folder_model, sc->folder_root);
	sc->folder_root = e_tree_model_node_insert (sc->folder_model, NULL,
						    0, NULL);


	build_etree_from_folder_info (sc, sc->folder_root, sc->folder_info);

	camel_exception_free (ex);
}



static void
folder_toggle_cb (ETable *table,
		  int row,
		  gpointer data)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (data);
	ETreePath *node = e_tree_model_node_at_row (sc->folder_model, row);
	CamelFolderInfo *info = e_tree_model_node_get_data (sc->folder_model, node);

	if (camel_store_folder_subscribed (sc->store, info->name)) {
		camel_store_unsubscribe_folder (sc->store, info->name, NULL);

#if 0
		/* we need to remove it from the shell */
		evolution_storage_removed_folder (sc->storage, info->name);
#endif
	}
	else {
		camel_store_subscribe_folder (sc->store, info->name, NULL);
#if 0
		/* we need to remove it from the shell */
		evolution_storage_removed_folder (sc->storage, info->name);
#endif
	}

	e_tree_model_node_changed (sc->folder_model, node);
}



#define EXAMPLE_DESCR "And the beast shall come forth surrounded by a roiling cloud of vengeance.\n" \
"  The house of the unbelievers shall be razed and they shall be scorched to the\n" \
"          earth. Their tags shall blink until the end of days. \n" \
"                 from The Book of Mozilla, 12:10"

static BonoboUIVerb verbs [] = {
	/* File Menu */
	BONOBO_UI_UNSAFE_VERB ("FileCloseWin", subscribe_close),

	/* Edit Menu */
	BONOBO_UI_UNSAFE_VERB ("EditSelectAll", subscribe_select_all),
	BONOBO_UI_UNSAFE_VERB ("EditUnSelectAll", subscribe_unselect_all),
	
	/* Folder Menu / Toolbar */
	BONOBO_UI_UNSAFE_VERB ("SubscribeFolder", subscribe_folder),
	BONOBO_UI_UNSAFE_VERB ("UnsubscribeFolder", unsubscribe_folder),

	/* Toolbar Specific */
	BONOBO_UI_UNSAFE_VERB ("RefreshList", subscribe_refresh_list),

	BONOBO_UI_VERB_END
};

static void
populate_store_foreach (MailConfigService *service, SubscribeDialog *sc)
{
	CamelException *ex = camel_exception_new();
	CamelStore *store = camel_session_get_store (session, service->url, ex);

	if (!store) {
		camel_exception_free (ex);
		return;
	}

	if (camel_store_supports_subscriptions (store)) {
		sc->store_list = g_list_append (sc->store_list, store);
	}
	else {
		camel_object_unref (CAMEL_OBJECT (store));
	}

	camel_exception_free (ex);
}

static void
populate_store_list (SubscribeDialog *sc)
{
	GSList *sources;

	sources = mail_config_get_sources ();
	g_slist_foreach (sources, (GFunc)populate_store_foreach, sc);
	sources = mail_config_get_news ();
	g_slist_foreach (sources, (GFunc)populate_store_foreach, sc);

	e_table_model_changed (sc->store_model);
}

static void
subscribe_dialog_gui_init (SubscribeDialog *sc)
{
	int i;
	ECell *cells[2], *text;
	ETableHeader *e_table_header;
	GdkPixbuf *toggles[2];
	BonoboUIComponent *component;
	BonoboUIContainer *container;
	GtkWidget         *folder_search_widget;
	BonoboControl     *search_control;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	/* Construct the app */
	sc->app = bonobo_win_new ("subscribe-dialog", "Manage Subscriptions");

	/* Build the menu and toolbar */
	container = bonobo_ui_container_new ();
	bonobo_ui_container_set_win (container, BONOBO_WIN (sc->app));

	/* set up the bonobo stuff */
	component = bonobo_ui_component_new_default ();
	bonobo_ui_component_set_container (
		component, bonobo_object_corba_objref (BONOBO_OBJECT (container)));
	
	bonobo_ui_component_add_verb_list_with_data (
		component, verbs, sc);

	bonobo_ui_component_freeze (component, NULL);

	bonobo_ui_util_set_ui (component, EVOLUTION_DATADIR,
			       "evolution-subscribe.xml",
			       "evolution-subscribe");

	update_pixmaps (component);

	bonobo_ui_component_thaw (component, NULL);

	sc->table = gtk_table_new (1, 2, FALSE);

	sc->hpaned = e_hpaned_new ();

	folder_search_widget = make_folder_search_widget (subscribe_search, sc);
	gtk_widget_show_all (folder_search_widget);
	search_control = bonobo_control_new (folder_search_widget);

	bonobo_ui_component_object_set (
		component, "/Toolbar/FolderSearch",
		bonobo_object_corba_objref (BONOBO_OBJECT (search_control)), NULL);
					
	/* set our our contents */
	sc->description = html_new (TRUE);
	put_html (GTK_HTML (sc->description), EXAMPLE_DESCR);

	gtk_table_attach (
		GTK_TABLE (sc->table), sc->description->parent->parent,
		0, 1, 0, 1,
		GTK_FILL | GTK_EXPAND,
		0,
		0, 0);

	/* set up the store etable */
	sc->store_model = e_table_simple_new (store_etable_col_count,
					      store_etable_row_count,
					      store_etable_value_at,
					      store_etable_set_value_at,
					      store_etable_is_editable,
					      store_etable_duplicate_value,
					      store_etable_free_value,
					      store_etable_initialize_value,
					      store_etable_value_is_empty,
					      store_etable_value_to_string,
					      sc);

	e_table_header = e_table_header_new ();

	cells[STORE_COL_NAME] = e_cell_text_new (E_TABLE_MODEL(sc->store_model), NULL, GTK_JUSTIFY_LEFT);

	for (i = 0; i < STORE_COL_LAST; i++) {
		/* Create the column. */
		ETableCol *ecol;

		ecol = e_table_col_new (i, store_headers [i],
					80, 20,
					cells[i],
					g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	sc->store_etable = e_table_scrolled_new (e_table_header, E_TABLE_MODEL(sc->store_model), STORE_ETABLE_SPEC);

	gtk_object_set (GTK_OBJECT (E_TABLE_SCROLLED (sc->store_etable)->table),
			"cursor_mode", E_TABLE_CURSOR_LINE,
			NULL);

	gtk_signal_connect (GTK_OBJECT (E_TABLE_SCROLLED (sc->store_etable)->table),
			    "cursor_change", GTK_SIGNAL_FUNC (storage_selected_cb),
			    sc);

	/* set up the folder etable */
	sc->folder_model = e_tree_simple_new (folder_etable_col_count,
					      folder_etable_duplicate_value,
					      folder_etable_free_value,
					      folder_etable_init_value,
					      folder_etable_value_is_empty,
					      folder_etable_value_to_string,
					      folder_etree_icon_at,
					      folder_etree_value_at,
					      folder_etree_set_value_at,
					      folder_etree_is_editable,
					      sc);

	sc->folder_root = e_tree_model_node_insert (sc->folder_model, NULL,
						    0, NULL);

	e_tree_model_root_node_set_visible (sc->folder_model, FALSE);

	e_table_header = e_table_header_new ();

	toggles[0] = gdk_pixbuf_new_from_xpm_data ((const char **)empty_xpm);
	toggles[1] = gdk_pixbuf_new_from_xpm_data ((const char **)mark_xpm);

	text = e_cell_text_new (E_TABLE_MODEL(sc->folder_model), NULL, GTK_JUSTIFY_LEFT);
	cells[FOLDER_COL_SUBSCRIBED] = e_cell_toggle_new (0, 2, toggles);
	cells[FOLDER_COL_NAME] = e_cell_tree_new (E_TABLE_MODEL(sc->folder_model),
						  NULL, NULL,
						  TRUE, text);

	for (i = 0; i < FOLDER_COL_LAST; i++) {
		/* Create the column. */
		ETableCol *ecol;

		if (i == FOLDER_COL_SUBSCRIBED)
			ecol = e_table_col_new_with_pixbuf (i, toggles[1],
							    0, gdk_pixbuf_get_width (toggles[1]),
							    cells[i], g_str_compare, FALSE);
		else 
			ecol = e_table_col_new (i, folder_headers [i],
						80, 20,
						cells[i],
						g_str_compare, TRUE);
		/* Add it to the header. */
		e_table_header_add_column (e_table_header, ecol, i);
	}

	sc->folder_etable = e_table_scrolled_new (e_table_header, E_TABLE_MODEL(sc->folder_model), FOLDER_ETABLE_SPEC);

	gtk_object_set (GTK_OBJECT (E_TABLE_SCROLLED (sc->folder_etable)->table),
			"cursor_mode", E_TABLE_CURSOR_LINE,
			NULL);
	gtk_object_set (GTK_OBJECT (text),
			"bold_column", FOLDER_COL_SUBSCRIBED,
			NULL);
	gtk_signal_connect (GTK_OBJECT (E_TABLE_SCROLLED (sc->folder_etable)->table),
			    "double_click", GTK_SIGNAL_FUNC (folder_toggle_cb),
			    sc);
	gtk_table_attach (
		GTK_TABLE (sc->table), sc->folder_etable,
		0, 1, 1, 3,
		GTK_FILL | GTK_EXPAND,
		GTK_FILL | GTK_EXPAND,
		0, 0);

	e_paned_add1 (E_PANED (sc->hpaned), sc->store_etable);
	e_paned_add2 (E_PANED (sc->hpaned), sc->table);
	e_paned_set_position (E_PANED (sc->hpaned), DEFAULT_STORE_TABLE_WIDTH);

	bonobo_win_set_contents (BONOBO_WIN (sc->app), sc->hpaned);

	gtk_widget_show (sc->description);
	gtk_widget_show (sc->folder_etable);
	gtk_widget_show (sc->table);
	gtk_widget_show (sc->store_etable);
	gtk_widget_show (sc->hpaned);

	/* FIXME: Session management and stuff?  */
	gtk_window_set_default_size (
		GTK_WINDOW (sc->app),
		DEFAULT_WIDTH, DEFAULT_HEIGHT);

	populate_store_list (sc);
}

static void
subscribe_dialog_destroy (GtkObject *object)
{
	SubscribeDialog *sc;

	sc = SUBSCRIBE_DIALOG (object);

	/* free our folder information */
	e_tree_model_node_remove (sc->folder_model, sc->folder_root);
	gtk_object_unref (GTK_OBJECT (sc->folder_model));
	if (sc->folder_info)
		camel_store_free_folder_info (sc->store, sc->folder_info);

	/* free our store information */
	gtk_object_unref (GTK_OBJECT (sc->store_model));
	g_list_foreach (sc->store_list, (GFunc)gtk_object_unref, NULL);

	subscribe_dialog_parent_class->destroy (object);
}

static void
subscribe_dialog_class_init (GtkObjectClass *object_class)
{
	object_class->destroy = subscribe_dialog_destroy;

	subscribe_dialog_parent_class = gtk_type_class (PARENT_TYPE);
}

static void
subscribe_dialog_init (GtkObject *object)
{
}

static void
subscribe_dialog_construct (GtkObject *object, Evolution_Shell shell)
{
	SubscribeDialog *sc = SUBSCRIBE_DIALOG (object);

	/*
	 * Our instance data
	 */
	sc->shell = shell;
	sc->store = NULL;
	sc->folder_info = NULL;
	sc->store_list = NULL;

	subscribe_dialog_gui_init (sc);
}

GtkWidget *
subscribe_dialog_new (Evolution_Shell shell)
{
	SubscribeDialog *subscribe_dialog;

	subscribe_dialog = gtk_type_new (subscribe_dialog_get_type ());

	subscribe_dialog_construct (GTK_OBJECT (subscribe_dialog), shell);

	return GTK_WIDGET (subscribe_dialog->app);
}

E_MAKE_TYPE (subscribe_dialog, "SubscribeDialog", SubscribeDialog, subscribe_dialog_class_init, subscribe_dialog_init, PARENT_TYPE);

