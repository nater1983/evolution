/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-composer-prefs.h"
#include "composer/e-msg-composer.h"

#include <bonobo/bonobo-generic-factory.h>

#include <gal/widgets/e-gui-utils.h>

#include "widgets/misc/e-charset-picker.h"

#include "mail-config.h"

static void mail_composer_prefs_class_init (MailComposerPrefsClass *class);
static void mail_composer_prefs_init       (MailComposerPrefs *dialog);
static void mail_composer_prefs_destroy    (GtkObject *obj);
static void mail_composer_prefs_finalise   (GtkObject *obj);

static void sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs);

static GtkVBoxClass *parent_class = NULL;


GtkType
mail_composer_prefs_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailComposerPrefs",
			sizeof (MailComposerPrefs),
			sizeof (MailComposerPrefsClass),
			(GtkClassInitFunc) mail_composer_prefs_class_init,
			(GtkObjectInitFunc) mail_composer_prefs_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_vbox_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_composer_prefs_class_init (MailComposerPrefsClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	parent_class = gtk_type_class (gtk_vbox_get_type ());
	
	object_class->destroy = mail_composer_prefs_destroy;
	object_class->finalize = mail_composer_prefs_finalise;
	/* override methods */
	
}

static void
mail_composer_prefs_init (MailComposerPrefs *composer_prefs)
{
	;
}

static void
mail_composer_prefs_finalise (GtkObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;
	
	gtk_object_unref (GTK_OBJECT (prefs->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
mail_composer_prefs_destroy (GtkObject *obj)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) obj;
	
	mail_config_signature_unregister_client ((MailConfigSignatureClient) sig_event_client, prefs);
}

static void
colorpicker_set_color (GnomeColorPicker *color, guint32 rgb)
{
	gnome_color_picker_set_i8 (color, (rgb & 0xff0000) >> 16, (rgb & 0xff00) >> 8, rgb & 0xff, 0xff);
}

static guint32
colorpicker_get_color (GnomeColorPicker *color)
{
	guint8 r, g, b, a;
	guint32 rgb = 0;
	
	gnome_color_picker_get_i8 (color, &r, &g, &b, &a);
	
	rgb   = r;
	rgb <<= 8;
	rgb  |= g;
	rgb <<= 8;
	rgb  |= b;
	
	return rgb;
}

static void
attach_style_info (GtkWidget *item, gpointer user_data)
{
	int *style = user_data;
	
	gtk_object_set_data (GTK_OBJECT (item), "style", GINT_TO_POINTER (*style));
	
	(*style)++;
}

static void
toggle_button_toggled (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
menu_changed (GtkWidget *widget, gpointer user_data)
{
	MailComposerPrefs *prefs = (MailComposerPrefs *) user_data;
	
	if (prefs->control)
		evolution_config_control_changed (prefs->control);
}

static void
option_menu_connect (GtkOptionMenu *omenu, gpointer user_data)
{
	GtkWidget *menu, *item;
	GList *items;
	
	menu = gtk_option_menu_get_menu (omenu);
	
	items = GTK_MENU_SHELL (menu)->children;
	while (items) {
		item = items->data;
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    menu_changed, user_data);
		items = items->next;
	}
}

static void
run_script (char *script)
{
	struct stat st;
	
	if (stat (script, &st))
		return;
	
	if (!S_ISREG (st.st_mode) || !(st.st_mode & (S_IXOTH | S_IXGRP | S_IXUSR)))
		return;
	
	mail_config_signature_run_script (script);
}

static void
sig_load_preview (MailComposerPrefs *prefs, MailConfigSignature *sig)
{
	char *str;
	
	if (!sig) {
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), " ", 1);
		return;
	}
	
	str = e_msg_composer_get_sig_file_content (sig->filename, sig->html);
	if (!str)
		str = g_strdup (" ");
	
	/* printf ("HTML: %s\n", str); */
	if (sig->html)
		gtk_html_load_from_string (GTK_HTML (prefs->sig_preview), str, strlen (str));
	else {
		GtkHTMLStream *stream;
		int len;
		
		len = strlen (str);
		stream = gtk_html_begin (GTK_HTML (prefs->sig_preview));
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "<PRE>", 5);
		if (len)
			gtk_html_write (GTK_HTML (prefs->sig_preview), stream, str, len);
		gtk_html_write (GTK_HTML (prefs->sig_preview), stream, "</PRE>", 6);
		gtk_html_end (GTK_HTML (prefs->sig_preview), stream, GTK_HTML_STREAM_OK);
	}
	
	g_free (str);
}

static void
sig_write_and_update_preview (MailComposerPrefs *prefs, MailConfigSignature *sig)
{
	sig_load_preview (prefs, sig);
	mail_config_signature_write (sig);
}

static MailConfigSignature *
sig_current_sig (MailComposerPrefs *prefs)
{
	return gtk_clist_get_row_data (GTK_CLIST (prefs->sig_clist), prefs->sig_row);
}

static void
sig_script_activate (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (sig && sig->script && *sig->script) {
		run_script (sig->script);
		sig_write_and_update_preview (prefs, sig);
	}
}

static void
sig_edit (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (sig->filename && *sig->filename)
		mail_signature_editor (sig);
	else
		e_notice (GTK_WINDOW (prefs), GNOME_MESSAGE_BOX_ERROR,
			  _("Please specify signature filename\nin Andvanced section of signature settings."));
}

MailConfigSignature *
mail_composer_prefs_new_signature (MailComposerPrefs *prefs, gboolean html)
{
	MailConfigSignature *sig;
	char *name[2];
	int row;
	
	sig = mail_config_signature_add (html);
	
	name[0] = sig->name;
	name[1] = sig->random ? _("yes") : _("no");
	row = gtk_clist_append (GTK_CLIST (prefs->sig_clist), name);
	gtk_clist_set_row_data (GTK_CLIST (prefs->sig_clist), row, sig);
	gtk_clist_select_row (GTK_CLIST (prefs->sig_clist), row, 0);
	/*gtk_widget_grab_focus (prefs->sig_name);*/
	
	sig_edit (NULL, prefs);
	
	return sig;
}

static void sig_row_unselect (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs);

static void
sig_delete (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	gtk_clist_remove (prefs->sig_clist, prefs->sig_row);
	mail_config_signature_delete (sig);
	if (prefs->sig_row < prefs->sig_clist->rows)
		gtk_clist_select_row (prefs->sig_clist, prefs->sig_row, 0);
	else if (prefs->sig_row)
		gtk_clist_select_row (prefs->sig_clist, prefs->sig_row - 1, 0);
	else
		sig_row_unselect (prefs->sig_clist, prefs->sig_row, 0, NULL, prefs);
}

static void
sig_add (GtkWidget *widget, MailComposerPrefs *prefs)
{
	mail_composer_prefs_new_signature (prefs, FALSE);
}

static void
sig_row_select (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig;
	
	printf ("sig_row_select\n");
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, TRUE);
	/*gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_name, TRUE);*/
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_random, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_filename, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_script, TRUE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, TRUE);
	
	prefs->sig_switch = TRUE;
	sig = gtk_clist_get_row_data (prefs->sig_clist, row);
	if (sig) {
		/*if (sig->name)
		  e_utf8_gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), sig->name);*/
		gtk_toggle_button_set_active (prefs->sig_random, sig->random);
		gtk_toggle_button_set_active (prefs->sig_html, sig->html);
		if (sig->filename)
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_filename)),
					    sig->filename);
		if (sig->script)
			gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_script)),
					    sig->script);
	}
	prefs->sig_switch = FALSE;
	prefs->sig_row = row;
	
	sig_load_preview (prefs, sig);
}

static void
sig_row_unselect (GtkCList *clist, int row, int col, GdkEvent *event, MailComposerPrefs *prefs)
{
	printf ("sig_row_unselect\n");
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_delete, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_edit, FALSE);
	/*gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_name, FALSE);*/
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_random, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_html, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_filename, FALSE);
	gtk_widget_set_sensitive ((GtkWidget *) prefs->sig_script, FALSE);
	
	prefs->sig_switch = TRUE;
	/*gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), "");*/
	gtk_toggle_button_set_active (prefs->sig_random, FALSE);
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_filename)), "");
	gtk_entry_set_text (GTK_ENTRY (gnome_file_entry_gtk_entry (prefs->sig_script)), "");
	prefs->sig_switch = FALSE;
}

static void
sig_fill_clist (GtkCList *clist)
{
	GList *l;
	char *name[2];
	int row;
	
	gtk_clist_freeze (clist);
	for (l = mail_config_get_signature_list (); l; l = l->next) {
		name[0] = ((MailConfigSignature *) l->data)->name;
		name[1] = ((MailConfigSignature *) l->data)->random ? _("yes") : _("no");
		row = gtk_clist_append (clist, name);
		gtk_clist_set_row_data (clist, row, l->data);
	}
	gtk_clist_thaw (clist);
}

static void
sig_name_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	/*mail_config_signature_set_name (sig, e_utf8_gtk_entry_get_text (GTK_ENTRY (prefs->sig_name)));*/
	gtk_clist_set_text (GTK_CLIST (prefs->sig_clist), prefs->sig_row, 0, sig->name);
	
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_random_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	mail_config_signature_set_random (sig, gtk_toggle_button_get_active (prefs->sig_random));
	
	gtk_clist_set_text (prefs->sig_clist, prefs->sig_row, 1, sig->random ? _("yes") : _("no"));
	
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_advanced_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	GtkWidget *advanced_frame;
	
	advanced_frame = glade_xml_get_widget (prefs->gui, "frameAdvancedOptions");
	gtk_widget_set_sensitive (advanced_frame, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)));
}

static void
sig_html_toggled (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	sig->html = gtk_toggle_button_get_active (prefs->sig_html);
	
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_filename_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	mail_config_signature_set_filename (sig, gnome_file_entry_get_full_path (prefs->sig_filename, FALSE));
	sig_write_and_update_preview (prefs, sig);
}

static void
sig_script_changed (GtkWidget *widget, MailComposerPrefs *prefs)
{
	MailConfigSignature *sig = sig_current_sig (prefs);
	
	if (prefs->sig_switch)
		return;
	
	g_free (sig->script);
	sig->script = g_strdup (gnome_file_entry_get_full_path (prefs->sig_script, FALSE));
	
	sig_write_and_update_preview (prefs, sig);
}

static void
url_requested (GtkHTML *html, const char *url, GtkHTMLStream *handle)
{
	GtkHTMLStreamStatus status;
	int fd;
	
	if (!strncmp (url, "file:", 5))
		url += 5;
	
	fd = open (url, O_RDONLY);
	status = GTK_HTML_STREAM_OK;
	if (fd != -1) {
		ssize_t size;
		void *buf = alloca (1 << 7);
		while ((size = read (fd, buf, 1 << 7))) {
			if (size == -1) {
				status = GTK_HTML_STREAM_ERROR;
				break;
			} else
				gtk_html_write (html, handle, (const gchar *) buf, size);
		}
	} else
		status = GTK_HTML_STREAM_ERROR;
	
	gtk_html_end (html, handle, status);
}

static void
sig_event_client (MailConfigSigEvent event, MailConfigSignature *sig, MailComposerPrefs *prefs)
{
	switch (event) {
	case MAIL_CONFIG_SIG_EVENT_NAME_CHANGED:
		printf ("accounts NAME CHANGED\n");
		gtk_clist_set_text (GTK_CLIST (prefs->sig_clist), sig->id, 0, sig->name);
		if (sig == sig_current_sig (prefs)) {
			prefs->sig_switch = TRUE;
			/*e_utf8_gtk_entry_set_text (GTK_ENTRY (prefs->sig_name), sig->name);*/
			prefs->sig_switch = FALSE;
		}
		break;
	case MAIL_CONFIG_SIG_EVENT_CONTENT_CHANGED:
		printf ("accounts CONTENT CHANGED\n");
		if (sig == sig_current_sig (prefs))
			sig_load_preview (prefs, sig);
		break;
	case MAIL_CONFIG_SIG_EVENT_HTML_CHANGED:
		printf ("accounts HTML CHANGED\n");
		if (sig == sig_current_sig (prefs))
			gtk_toggle_button_set_active (prefs->sig_html, sig->html);
		break;
	default:
		;
	}
}

static void
mail_composer_prefs_construct (MailComposerPrefs *prefs)
{
	GtkWidget *toplevel, *widget, *menu;
	GladeXML *gui;
	int style;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "composer_tab");
	prefs->gui = gui;
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_ref (toplevel);
	gtk_container_remove (GTK_CONTAINER (toplevel->parent), toplevel);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);
	gtk_widget_unref (toplevel);
	
	/* General tab */
	
	/* Default Behavior */
	prefs->send_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkSendHTML"));
	gtk_toggle_button_set_active (prefs->send_html, mail_config_get_send_html ());
	gtk_signal_connect (GTK_OBJECT (prefs->send_html), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_empty_subject = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptEmptySubject"));
	gtk_toggle_button_set_active (prefs->prompt_empty_subject, mail_config_get_prompt_empty_subject ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_empty_subject), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->prompt_bcc_only = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkPromptBccOnly"));
	gtk_toggle_button_set_active (prefs->prompt_bcc_only, mail_config_get_prompt_only_bcc ());
	gtk_signal_connect (GTK_OBJECT (prefs->prompt_bcc_only), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->charset = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuCharset"));
	menu = e_charset_picker_new (mail_config_get_default_charset ());
	gtk_option_menu_set_menu (prefs->charset, GTK_WIDGET (menu));
	option_menu_connect (prefs->charset, prefs);
	
	/* Spell Checking */
	/* FIXME: do stuff with these */
	prefs->spell_check = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkEnableSpellChecking"));
	prefs->colour = GNOME_COLOR_PICKER (glade_xml_get_widget (gui, "colorpickerSpellCheckColor"));
	prefs->language = GTK_COMBO (glade_xml_get_widget (gui, "cmboSpellCheckLanguage"));
	
	/* Forwards and Replies */
	prefs->forward_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuForwardStyle"));
	gtk_option_menu_set_history (prefs->forward_style, mail_config_get_default_forward_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->forward_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->forward_style, prefs);
	
	prefs->reply_style = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuReplyStyle"));
	gtk_option_menu_set_history (prefs->reply_style, mail_config_get_default_reply_style ());
	style = 0;
	gtk_container_foreach (GTK_CONTAINER (gtk_option_menu_get_menu (prefs->reply_style)),
			       attach_style_info, &style);
	option_menu_connect (prefs->reply_style, prefs);
	
	/* Signatures */
	prefs->sig_add = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureAdd"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_add), "clicked",
			    GTK_SIGNAL_FUNC (sig_add), prefs);
	
	prefs->sig_edit = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureEdit"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_edit), "clicked",
			    GTK_SIGNAL_FUNC (sig_edit), prefs);
	
	prefs->sig_delete = GTK_BUTTON (glade_xml_get_widget (gui, "cmdSignatureDelete"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_delete), "clicked",
			    GTK_SIGNAL_FUNC (sig_delete), prefs);
	
	prefs->sig_random = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkRandomSignature"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_random), "toggled",
			    GTK_SIGNAL_FUNC (sig_random_toggled), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_random), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->sig_clist = GTK_CLIST (glade_xml_get_widget (gui, "clistSignatures"));
	sig_fill_clist (prefs->sig_clist);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_clist), "select_row",
			    GTK_SIGNAL_FUNC (sig_row_select), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_clist), "unselect_row",
			    GTK_SIGNAL_FUNC (sig_row_unselect), prefs);
	
	prefs->sig_advanced = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkAdvancedSignature"));
	gtk_toggle_button_set_active (prefs->sig_advanced, FALSE);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_advanced), "toggled",
			    GTK_SIGNAL_FUNC (sig_advanced_toggled), prefs);
	
	widget = glade_xml_get_widget (gui, "frameAdvancedOptions");
	gtk_widget_set_sensitive (widget, FALSE);
	
	prefs->sig_html = GTK_TOGGLE_BUTTON (glade_xml_get_widget (gui, "chkHtmlSignature"));
	gtk_signal_connect (GTK_OBJECT (prefs->sig_html), "toggled",
			    GTK_SIGNAL_FUNC (sig_html_toggled), prefs);
	gtk_signal_connect (GTK_OBJECT (prefs->sig_html), "toggled",
			    toggle_button_toggled, prefs);
	
	prefs->sig_filename = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignatureFilename"));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_filename)),
			    "changed", GTK_SIGNAL_FUNC (sig_filename_changed), prefs);
	
	prefs->sig_script = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignatureScript"));
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_script)),
			    "changed", GTK_SIGNAL_FUNC (sig_script_changed), prefs);
	gtk_signal_connect (GTK_OBJECT (gnome_file_entry_gtk_entry (prefs->sig_script)),
			    "activate", GTK_SIGNAL_FUNC (sig_script_activate), prefs);
	
	/* preview GtkHTML widget */
	widget = glade_xml_get_widget (gui, "scrolled-sig");
	prefs->sig_preview = (GtkHTML *) gtk_html_new ();
	gtk_signal_connect (GTK_OBJECT (prefs->sig_preview), "url_requested", GTK_SIGNAL_FUNC (url_requested), NULL);
	gtk_widget_show (GTK_WIDGET (prefs->sig_preview));
	gtk_container_add (GTK_CONTAINER (widget), GTK_WIDGET (prefs->sig_preview));
	
	if (GTK_CLIST (prefs->sig_clist)->rows)
		gtk_clist_select_row (GTK_CLIST (prefs->sig_clist), 0, 0);
	
	mail_config_signature_register_client ((MailConfigSignatureClient) sig_event_client, prefs);
}


GtkWidget *
mail_composer_prefs_new (void)
{
	MailComposerPrefs *new;
	
	new = (MailComposerPrefs *) gtk_type_new (mail_composer_prefs_get_type ());
	mail_composer_prefs_construct (new);
	
	return (GtkWidget *) new;
}


void
mail_composer_prefs_apply (MailComposerPrefs *prefs)
{
	GtkWidget *menu, *item;
	char *string;
	int val;
	
	/* General tab */
	
	/* Default Behavior */
	mail_config_set_send_html (gtk_toggle_button_get_active (prefs->send_html));
	mail_config_set_prompt_empty_subject (gtk_toggle_button_get_active (prefs->prompt_empty_subject));
	mail_config_set_prompt_only_bcc (gtk_toggle_button_get_active (prefs->prompt_bcc_only));
	
	menu = gtk_option_menu_get_menu (prefs->charset);
	string = e_charset_picker_get_charset (menu);
	if (string) {
		mail_config_set_default_charset (string);
		g_free (string);
	}
	
	/* Spell Checking */
	/* FIXME: implement me */
	
	/* Forwards and Replies */
	menu = gtk_option_menu_get_menu (prefs->forward_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_forward_style (val);
	
	menu = gtk_option_menu_get_menu (prefs->reply_style);
	item = gtk_menu_get_active (GTK_MENU (menu));
	val = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "style"));
	mail_config_set_default_reply_style (val);
	
	/* Keyboard Shortcuts */
	/* FIXME: implement me */
	
	/* Signatures */
	/* FIXME: implement me */
}
