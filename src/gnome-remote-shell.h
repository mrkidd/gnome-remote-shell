/* -*- Mode: C; indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8 -*- */

/* gnome-remote-shell - Wrapper for opening remote shells
 * Copyright (C) 2002-2003 by Rodrigo Moya & Ulrich Neumann
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

/*
 * Definitions
 */
#define SSH_PORT 22
#define TELNET_PORT 23
#define MAX_SERVER_SIZE 256

enum {
	IPV4,
	IPV6
};

/*
 * Prototypes
 */
void	window_destroyed_cb (GtkWidget *widget, gpointer user_data);
void	radio_button_toggled_cb (GtkToggleButton *button, gpointer user_data);
void	entry_activate_cb (GtkWidget *widget, gpointer user_data);
void	port_default_toggled_cb (GtkToggleButton *button, gpointer user_data);
void	profile_use_clicked (GtkWidget *widget, gpointer user_data);
void	profile_add_clicked (GtkWidget *widget, gpointer user_data);
void	profile_remove_clicked (GtkWidget *widget, gpointer user_data);
void	profile_read (void);
void	profile_save (void);
void	on_clear_host_history1_activate (GtkWidget *widget, gpointer user_data);
void	on_clear_user_history1_activate (GtkWidget *widget, gpointer user_data);
void	launch_help (GtkWidget *widget, gpointer user_data);
void	on_about_activate (GtkWidget *widget, gpointer user_data);

