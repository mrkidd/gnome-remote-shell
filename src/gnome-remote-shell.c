/* -*- Mode: C; indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8 -*- */

/* gnome-remote-shell - Wrapper for opening remote shells
 * Copyright (C) 2002-2003 by Rodrigo Moya
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmessagedialog.h>
#include <gtk/gtkradiobutton.h>
#include <gtk/gtkspinbutton.h>
#include <glade/glade.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-entry.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomeui/gnome-window-icon.h>
#include <libgnomeui/libgnomeui.h>

#include "gnome-remote-shell.h"

static GtkListStore *list_store;
static GtkTreeModel *model;
static GtkTreeView *list_widget;
static GConfClient *conf_client;
static GtkWidget *main_window;
static GtkWidget *ssh_method;
static GtkWidget *telnet_method;
static GtkWidget *host_entry;
static GtkWidget *user_entry;
static GtkWidget *port_default;
static GtkWidget *port_entry;
static GtkWidget *window_width;
static GtkWidget *window_height;
static GtkWidget *profile_listbox;

static gint     get_ip_version (const gchar *ip_address);
static gboolean check_network_status (const gchar *host, gint port);
static void	create_error (gchar *title, gchar *reason);
static void	activate_shell (void);
static void	set_spin_from_config (GtkWidget *spin, const gchar *key, gint default_value);
static gboolean	validate_host (const gchar *host);

void
window_destroyed_cb (GtkWidget *widget, gpointer user_data)
{
	gtk_main_quit ();
}

void
radio_button_toggled_cb (GtkToggleButton *button, gpointer user_data)
{
	gint port = -1;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method))) {
		gtk_widget_set_sensitive (user_entry, TRUE);
	} else {
		gtk_widget_set_sensitive (user_entry, FALSE);
	}

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (port_default))) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method)))
			port = SSH_PORT;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (telnet_method)))
			port = TELNET_PORT;
		if (port > 0)
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (port_entry), port);
	}
}

void
entry_activate_cb (GtkWidget *widget, gpointer user_data)
{
	activate_shell ();
}

void
port_default_toggled_cb (GtkToggleButton *button, gpointer user_data)
{
	gint port = -1;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button))) {
		gtk_widget_set_sensitive (port_entry, FALSE);

		/*
		 * If the use_default checkbutton is set, then manually update
		 * port_entry to the default port assignment.
		 */
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method)))
			port = SSH_PORT;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (telnet_method)))
			port = TELNET_PORT;
		
		gtk_spin_button_update (GTK_SPIN_BUTTON (port_entry));
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (port_entry), port);
	} else {
		gtk_widget_set_sensitive (port_entry, TRUE);
	}
}

static gint
get_ip_version (const gchar *ip_address)
{
	struct hostent *host;

	g_return_val_if_fail (ip_address != NULL, -1);

	if (strlen (ip_address) > 0) {
		host = gethostbyname2 (ip_address, PF_INET6);
		if (host == NULL) {
			host = gethostbyname2 (ip_address, AF_INET);
			if (host == NULL)
				return -1;
			else
				return IPV4;

			return -1;
		}
		else
			return IPV6;
	}

	return -1;
}

static gboolean
check_network_status (const gchar *host, gint port)
{
	struct sockaddr_in *addr;
	struct sockaddr_in6 *addr6;
	struct hostent *hostname;
	gint sd;
	gchar *msgerror;

	switch (get_ip_version (host))
	{
	case IPV4:
		hostname = gethostbyname2 (host, AF_INET);

		addr = (struct sockaddr_in *) g_malloc (sizeof (struct sockaddr_in));
		
		sd = socket (AF_INET, SOCK_STREAM, 0);
		addr->sin_family = AF_INET;
		addr->sin_port = g_htons (port);
		addr->sin_addr = *(struct in_addr *) hostname->h_addr;
		
		errno = 0;
		if (connect (sd, (struct sockaddr *) addr, sizeof (struct sockaddr_in)) != 0) {
			msgerror = (gchar *) strerror (errno);
			msgerror = g_locale_to_utf8 (msgerror, strlen (msgerror), NULL, NULL, NULL);
			
			create_error (_("There is a connection error"), msgerror);
									
			g_free (msgerror);
			g_free (addr);
			
			return FALSE;
		}
		shutdown (sd, 2);

		g_free (addr);

		break;
	case IPV6:
		hostname = gethostbyname2 (host, PF_INET6);

		addr6 = (struct sockaddr_in6 *) g_malloc (sizeof (struct sockaddr_in6));

		sd = socket (PF_INET6, SOCK_STREAM, 0);
		addr6->sin6_family = PF_INET6;
		addr6->sin6_port = g_htons (port);
		addr6->sin6_addr = *(struct in6_addr *) hostname->h_addr;

		errno = 0;
		if (connect (sd, (struct sockaddr *) addr6, sizeof (struct sockaddr_in6)) != 0) {
			msgerror = (gchar *) strerror (errno);
			msgerror = g_locale_to_utf8 (msgerror, strlen (msgerror), NULL, NULL, NULL);
			
			create_error (_("There is a connection error"), msgerror);

			g_free (msgerror);
			g_free (addr6);

			return FALSE;
		}
		shutdown (sd, 2);

		g_free (addr6);

		break;
	case -1:
		return FALSE;
	}
	
	return TRUE;
}

/* 
 * Given a title and reason for an application error, produce a 
 * HIG compliant error alert 
 */
static void
create_error (gchar *title, gchar *reason)
{
	GtkWidget *error_dialog;
	gchar *title_esc, *reason_esc;
		
	title_esc = g_markup_escape_text (title, -1);
	reason_esc = g_markup_escape_text (reason, -1);
		
	error_dialog = gtk_message_dialog_new (GTK_WINDOW (main_window),
					       GTK_DIALOG_DESTROY_WITH_PARENT,
					       GTK_MESSAGE_ERROR,
					       GTK_BUTTONS_OK,
					       "<span weight='bold' size='larger'>%s</span>.\n\n%s.", title_esc, reason_esc);
							    
	g_free (title_esc);
	g_free (reason_esc);

	gtk_dialog_set_default_response (GTK_DIALOG (error_dialog),
					 GTK_RESPONSE_OK);
	
	gtk_label_set_use_markup (GTK_LABEL (GTK_MESSAGE_DIALOG (error_dialog)->label), TRUE);
					 
	g_signal_connect (error_dialog, "destroy",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	
	g_signal_connect (error_dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);
	
	gtk_window_set_modal (GTK_WINDOW (error_dialog), TRUE);

	gtk_widget_show (error_dialog);

	return;
}

void
on_profile_output_row_activated (GtkTreeView *treeview, GtkTreePath *arg1, GtkTreeViewColumn *arg2, gpointer user_data)
{
	profile_use_clicked (GTK_WIDGET (treeview), user_data);
	entry_activate_cb (GTK_WIDGET (treeview), user_data);
}

/*
 * Use selected profile
 */
void
profile_use_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(list_widget);

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		GValue val0, val1, val2;
		const char *host = NULL, *username = NULL;
		gint portvalue = 0;

		memset(&val0, 0, sizeof(val0));
		memset(&val1, 0, sizeof(val1));
		memset(&val2, 0, sizeof(val2));
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 0, &val0);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 1, &val1);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 2, &val2);

		host = g_value_get_string(&val0);
		username = g_value_get_string(&val1);
		portvalue = g_value_get_int(&val2);

		gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (host_entry))), host);
		if (username) {
			gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (user_entry))), username);
		}

		if (portvalue > 0) {
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (port_entry), portvalue);
		}

		if (portvalue == TELNET_PORT) {
			gtk_widget_set_sensitive (user_entry, FALSE);
			gtk_widget_set_sensitive (port_entry, FALSE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (telnet_method), TRUE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (port_default), TRUE);
		}
		else if (portvalue == SSH_PORT){
			gtk_widget_set_sensitive (user_entry, TRUE);
			gtk_widget_set_sensitive (port_entry, FALSE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ssh_method), TRUE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (port_default), TRUE);
		}
		else {
			gtk_widget_set_sensitive (user_entry, TRUE);
			gtk_widget_set_sensitive (port_entry, TRUE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (ssh_method), TRUE);
			gtk_toggle_button_set_state (GTK_TOGGLE_BUTTON (port_default), FALSE);
		}

		g_value_unset(&val0);
		g_value_unset(&val1);
		g_value_unset(&val2);
	}
	else {
		create_error (_("Could not use profile"), _("No profile is selected"));
		return;
	};

	return;
}

/*
 * Add profiles entry
 */
void
profile_add_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	gint addport;
	const gchar *addhost = NULL, *adduser = NULL;

	/*
	 * Get the port.
	 * Use the default port if the checkbutton is active
	 * (22 for secure shell, 23 for telnet)
	 */
	addport = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (port_entry));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (port_default))) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method))) {
			adduser = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (user_entry))));
			addport = SSH_PORT;
		}
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (telnet_method))) {
			adduser = (char *) "\0";
			addport = TELNET_PORT;
		}
	}

	/* Get the host */
	addhost = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (host_entry))));
	if (!addhost || !strlen (addhost)) {
		create_error (_("Could not add profile"), _("A hostname is required"));
		return;
	}

	gtk_list_store_append (list_store, &iter);
	gtk_list_store_set (list_store, &iter,
				0, addhost,
				1, adduser,
				2, addport,
				-1);

	/* save new profile list in GConf */
	profile_save ();

	return;
}

/*
 * Remove entry from .gnome-remote-shell.profiles
 */
void
profile_remove_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(list_widget);

	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_list_store_remove(list_store, &iter);
	}
	else {
		create_error (_("Could not remove profile"), _("No profile is selected"));
		return;
	};

	/* save new profile list in GConf */
	profile_save ();

	return;
}

/*
 * Save profiles to GConf
 */
void
profile_save (void)
{
	GtkTreeIter iter;
	gboolean valid;
	GSList *profile_list;
	char *buffer;

	profile_list = NULL;

	/* Get the first iter in the list */
	valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {

		/* Walk through the list, reading each row */
		GValue val0, val1, val2;
		const char *host = NULL, *username = NULL;
		gint portvalue = 0;

		memset(&val0, 0, sizeof(val0));
		memset(&val1, 0, sizeof(val1));
		memset(&val2, 0, sizeof(val2));
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 0, &val0);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 1, &val1);
		gtk_tree_model_get_value(GTK_TREE_MODEL(model), &iter, 2, &val2);

		host = g_value_get_string(&val0);
		username = g_value_get_string(&val1);
		if (!username) {
			username = (char *) "\0";
		}

		portvalue = g_value_get_int(&val2);
		buffer = NULL;
		buffer = (char *) malloc (strlen(host) + strlen(username) + 8);

		sprintf (buffer, "%s;%s;%d", host, username, portvalue);

		if (buffer)
			profile_list = g_slist_append (profile_list, buffer);

		g_value_unset(&val0);
		g_value_unset(&val1);
		g_value_unset(&val2);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	/* store list in GConf database */
	gconf_client_set_list (conf_client, "/apps/gnome-remote-shell/Profiles",
				GCONF_VALUE_STRING, profile_list, NULL);

	/* free data */
	g_slist_foreach (profile_list, (GFunc) g_free, NULL);
	g_slist_free (profile_list);

	return;
}

/*
 * Read profiles from GConf at startup
 */
void
profile_read (void)
{
	GtkTreeIter iter1;
	GtkCellRenderer *renderer = NULL;
	GtkTreeViewColumn *column;

	GSList *iter;
	GSList *profile_list;
	char *element;

	char *hostname;
	char *username;
	char *port;

	/* create a new list store */
	list_store = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	/* get widget defined in glade XML file */
	list_widget = (GTK_TREE_VIEW (profile_listbox));

	/* generate column titles */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		(_("Host"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column (list_widget, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		(_("User"), renderer, "text", 1, NULL);
	gtk_tree_view_append_column (list_widget, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes
		(_("Port"), renderer, "text", 2, NULL);
	gtk_tree_view_append_column (list_widget, column);

	/* get list of profile entries via GConf database */
	profile_list = gconf_client_get_list (conf_client, "/apps/gnome-remote-shell/Profiles",
				GCONF_VALUE_STRING, NULL);

	iter = profile_list;

	/* iterate through list and display values to user */
	while (iter != NULL) {
		element = (char *) iter->data;
            
		/* does this line has valid content */
		if (strlen(element) > 4) {

			hostname = strtok(element, ";");
			username = strtok(NULL, ";");

			port = strtok(NULL, ";");
			if (!port) {
				port = username;
				username = (char *) "\0";
			}

			/* add line with profile data to list */
			gtk_list_store_append (list_store, &iter1);

			gtk_list_store_set (list_store, &iter1,
					0, hostname,
					1, username,
					2, atoi(port),
					-1);
		}


		iter = g_slist_next(iter);
	}

	/* free list retrieved from GConf */
	g_slist_foreach(profile_list, (GFunc) g_free, NULL);
	g_slist_free(profile_list);

	/* get model from list store with profile entries */
	model = GTK_TREE_MODEL (list_store);

	gtk_tree_view_set_model (GTK_TREE_VIEW (list_widget), model);

	/* complete successfully */
	return;
}

/*
 * Clear Hostname history
 */
void
on_clear_host_history1_activate (GtkWidget *widget, gpointer user_data)
{
	gnome_entry_clear_history (GNOME_ENTRY (host_entry));

	return;
}

/*
 * Clear User history
 */
void
on_clear_user_history1_activate (GtkWidget *widget, gpointer user_data)
{
	gnome_entry_clear_history (GNOME_ENTRY (user_entry));

	return;
}

/*
 * Display the help file in GNOME's help browser
 */
void
launch_help (GtkWidget *widget, gpointer user_data)
{
	GError *err = NULL;
	
	gnome_help_display ("gnome-remote-shell.xml", NULL, &err); 
		
	if (err) {
		create_error (_("There is an error with displaying help"), err->message);
    	}
}

/*
 * Display GNOME about dialog with credits
 */
void
on_about_activate (GtkWidget *widget, gpointer user_data)
{
	static GtkWidget *about_box = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *filename = NULL;
	const gchar *authors[] = { 
		"Rodrigo Moya <rodrigo@gnome-db.org>",
		"Emil Soleyman-Zomalan <emil@nishra.com>",
		"Carlos Garcia Campos <carlosgc@gnome.org>",
		"Ulrich Neumann <u_neumann@gne.de>",
		NULL
	};
	const gchar *documenters[] = {
                 "", NULL
        };
	const gchar *copyright = "Copyright \xc2\xa9 2003 Rodrigo Moya";

	if (about_box != NULL) {
		gtk_window_present (GTK_WINDOW (about_box));
		return;
	}

	filename = g_build_filename (GNOME_ICONDIR, "gnome-remote-shell.png", NULL);
	if (filename != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
		g_free (filename);
	}
                                                                                
	about_box = gnome_about_new ("GNOME Remote Shell",
				     VERSION,
				     copyright,
				     _
				     ("Graphical user interface for GNOME Remote Shell"),
				     authors,
				     documenters,
				     _("This is an untranslated version of GNOME Remote Shell"),
				     pixbuf);

        if (pixbuf != NULL)
                g_object_unref (pixbuf);

	/* make sure every window manager places the about dialog at the correct position */
	gtk_window_set_transient_for (GTK_WINDOW (about_box),
				      GTK_WINDOW (main_window));

	g_signal_connect (G_OBJECT (about_box), "destroy",
			  G_CALLBACK (gtk_widget_destroyed), &about_box);

	gtk_widget_show (about_box);
}

static void
activate_shell ()
{
	GConfValue *conf_value;
	GError *err = NULL;
	gchar *cmd;
	const gchar *host = NULL, *user = NULL;
	gchar *term_app, *parm_exec, *parm_title;
	gint port;
	gchar *geometry;

	/*
	 * Get the port.
	 * Use the default port if the checkbutton is active
	 * (22 for secure shell, 23 for telnet)
	 */
	port = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (port_entry));

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (port_default))) {
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method)))
			port = SSH_PORT;
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (telnet_method)))
			port = TELNET_PORT;
	}

	/*
	 * Get the host and do a quick lookup.
	 */
	host = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (host_entry))));
	if (!host || !strlen (host)) {
		create_error (_("Failed to create a connection"), _("A hostname is required"));

		return;
	}
					   
	if (validate_host (host) == FALSE)
		return;

	if (check_network_status (host, port) == FALSE)
		return;

	/*
	 * Get the terminal preferences from GConf.
	 */
	conf_value = gconf_client_get (conf_client, "/desktop/gnome/applications/terminal/exec", NULL);
	if (conf_value != NULL) {
		term_app = g_strdup (gconf_value_get_string (conf_value));
		gconf_value_free (conf_value);
	} else {
		term_app = g_strdup ("gnome-terminal");
	}

	conf_value = gconf_client_get (conf_client, "/desktop/gnome/applications/terminal/exec_arg", NULL);
	if (conf_value != NULL) {
		parm_exec = g_strdup (gconf_value_get_string (conf_value));
		gconf_value_free (conf_value);
	} else {
		parm_exec = g_strdup ("-x");
	}

	/*
	 * Do some intelligent guessing from the terminal exec line.
	 */
	if (strlen(term_app) >= 14 && !strncmp(term_app, "gnome-terminal", 14)) {
		geometry = g_strdup_printf (
			"--geometry=%dx%d",
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (window_width)),
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (window_height)));
		parm_title = g_strdup_printf ("-t");
	} else {
		geometry = g_strdup_printf (
			"-geometry %dx%d",
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (window_width)),
			gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (window_height)));
		parm_title = g_strdup_printf ("-T");
	}

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ssh_method))) {
			user = gtk_entry_get_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (user_entry))));
			if (!user || !strlen (user)) {
				create_error (_("Failed to create a connection"), _("A user name is required for SSH"));

				return;
			}

                        if (port > 0) {
				/* 
				 * Gnome-terminal's -x and -e switches are different from one another 
				 * in accepting single or multiple arguments. Moreover, 
				 * gnome-terminal's -e switch (single argument) behaves differently 
				 * from either xterm's or rxvt's -e switch (multiple arguments).
                                 *
                                 * The difference in gnome-terminal's -e and -x switches:
                                 *
                                 *  switch: -e
                                 *   usage: takes a single argument
                                 *  compat: not compatible with rxvt's or xterm's -e switch
                                 * example: gnome-terminal -e "ssh -X user@localhost" 
                                 *
                                 *  switch: -x
                                 *   usage: takes multiple arguments
				 *  compat: compatible with rxvt's and xterm's -e switch 
                                 * example: gnome-terminal -x ssh -X user@localhost
                                 */ 

                                /*  
			 	 * If terminal preferences we receive from Gconf are gnome-terminal 
				 * and -e, then we will give it a single command enclosed in a delimiter. 
                                 */
				if (strlen(term_app) >= 14 && !strncmp(term_app, "gnome-terminal", 14) 
					&& !strcmp(parm_exec, "-e")) {
                                	cmd = g_strdup_printf (
                                        "%s %s %s \"%s@%s - Secure shell\" %s \"ssh -p %d -X %s@%s\"", 
					term_app, geometry, parm_title, user, host, parm_exec, port, user, host);
                                } else {
                                        cmd = g_strdup_printf (
                                        "%s %s %s \"%s@%s - Secure shell\" %s ssh -p %d -X %s@%s",
					term_app, geometry, parm_title, user, host, parm_exec, port, user, host);
                                }                                                                                
                        } else {
                                if (strlen(term_app) >= 14 && !strncmp(term_app, "gnome-terminal", 14) 
                                && !strcmp(parm_exec, "-e")) {
                                        cmd = g_strdup_printf (
                                        "%s %s %s \"%s@%s - Secure shell\" %s \"ssh -X %s@%s\"",
					term_app, geometry, parm_title, user, host, parm_exec, user, host);
                                } else {
                                        cmd = g_strdup_printf (
                                        "%s %s %s \"%s@%s - Secure shell\" %s ssh -X %s@%s",
					term_app, geometry, parm_title, user, host, parm_exec, user, host);
                                }      
						}
		} else {
                        if (port > 0) {
                                if (strlen(term_app) >= 14 && !strncmp(term_app, "gnome-terminal", 14) 
                                && !strcmp(parm_exec, "-e")) {
                                        cmd = g_strdup_printf (
					"%s %s %s \"%s - Remote shell\" %s \"telnet %s %d\"",
					term_app, geometry, parm_title, host, parm_exec, host, port);      
                                } else {
                                        cmd = g_strdup_printf (
					"%s %s %s \"%s - Remote shell\" %s telnet %s %d",
					term_app, geometry, parm_title, host, parm_exec, host, port);              
                                }      
                        } else {
                                if (strlen(term_app) >= 14 && !strncmp(term_app, "gnome-terminal", 14) 
                                && !strcmp(parm_exec, "-e")) {
                                        cmd = g_strdup_printf (
                                        "%s %s %s \"%s - Remote shell\" %s \"telnet %s\"",
                                        term_app, geometry, parm_title, host, parm_exec, host);      
                                } else {
                                        cmd = g_strdup_printf (
                                        "%s %s %s \"%s - Remote shell\" %s telnet %s",
                                        term_app, geometry, parm_title, host, parm_exec, host);
                                }                      
			}
		}

		if (!g_spawn_command_line_async (cmd, &err)) {
			create_error (_("There is a command line error"), err->message);

			g_error_free (err);
		}

		g_free (cmd);
		g_free (geometry); 
	g_free (parm_exec);
	g_free (parm_title);
	g_free (term_app); 

	/* Save the window state */
	gconf_client_set_int (conf_client, "/apps/gnome-remote-shell/TerminalWidth",
			      (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (window_width)),
			      NULL);
	gconf_client_set_int (conf_client, "/apps/gnome-remote-shell/TerminalHeight",
			      (gint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (window_height)),
			      NULL);
	
	if (host != NULL)
		gnome_entry_prepend_history (GNOME_ENTRY (host_entry), TRUE, host);
	if (user != NULL)
		gnome_entry_prepend_history (GNOME_ENTRY (user_entry), TRUE, user);
}

static void
set_spin_from_config (GtkWidget *spin, const gchar *key, gint default_value)
{
	GConfValue *conf_value;

	conf_value = gconf_client_get (conf_client, key, NULL);
	if (conf_value) {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin),
					   (gdouble) gconf_value_get_int (conf_value));
		gconf_value_free (conf_value);
	} else {
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gdouble) default_value);
	}
}

static gboolean
validate_host (const gchar *host)
{
	struct hostent *hostname;

	hostname = gethostbyname2 (host, PF_INET6);
	if (hostname == NULL) {
		hostname = gethostbyname2 (host, AF_INET);
		if (hostname == NULL) {
			create_error (_("Failed to create a connection"), _("The host cannot be found"));

			return FALSE;
		}
	}
	
	return TRUE;
}

int
main (int argc, char **argv)
{
	const gchar *xml_file = DIALOGDIR "gnome-remote-shell.glade";
	GladeXML *xml;
	gchar *icon_path;
	int flag;
	char *host = NULL;
	char *user = NULL;

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	gnome_program_init ("gnome-remote-shell", VERSION, 
			    LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES, NULL);

	icon_path = g_build_filename (GNOME_ICONDIR, "gnome-remote-shell.png", NULL);
	gnome_window_icon_set_default_from_file (icon_path);
	g_free (icon_path);

	conf_client = gconf_client_get_default ();

	/* create the main window */
	xml = glade_xml_new (xml_file, NULL, NULL);

	main_window = glade_xml_get_widget (xml, "gnome_remote_shell");
	ssh_method = glade_xml_get_widget (xml, "ssh_method");
	telnet_method = glade_xml_get_widget (xml, "telnet_method");
	host_entry = glade_xml_get_widget (xml, "host_entry");
	user_entry = glade_xml_get_widget (xml, "user_entry");
	port_entry = glade_xml_get_widget (xml, "port_entry");
	port_default = glade_xml_get_widget (xml, "port_default");
	window_width = glade_xml_get_widget (xml, "window_width");
	window_height = glade_xml_get_widget (xml, "window_height");
	profile_listbox = glade_xml_get_widget (xml, "profile_output");

	/* set settings from gconf */
	set_spin_from_config (window_width, "/apps/gnome-remote-shell/TerminalWidth", 80);
	set_spin_from_config (window_height, "/apps/gnome-remote-shell/TerminalHeight", 25);	

	glade_xml_signal_autoconnect (xml);

	/* check for any command line parameters */
	optind = 1;
	while ((flag = getopt(argc, argv, "h:l:")) != -1) {
		switch (flag) {
			case 'h':
				host = optarg;
				gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (host_entry))), host);
				break;
			case 'l':
				user = optarg;
				gtk_entry_set_text (GTK_ENTRY (gnome_entry_gtk_entry (GNOME_ENTRY (user_entry))), user);
				break;
			default:
				break;
		}
	}

	// bypass the window if there is a hostname and a username at least
	if (user && host) {
		activate_shell ();
		g_object_unref (G_OBJECT (conf_client));
		return 0;
	}

	profile_read ();

	gtk_main ();

	g_object_unref (G_OBJECT (conf_client));

	return 0;
}
